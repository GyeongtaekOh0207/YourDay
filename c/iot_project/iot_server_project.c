#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 100
#define MAX_CLNT 10
#define ID_SIZE 20
#define ARR_CNT 5
#define LOG_BUF_SIZE 256
#define IP_SIZE 20

#define DEBUG

typedef struct {
  int fd;     
  char *from;
  char *to;
  char *msg;
  int len; 
} MSG_INFO;

typedef struct {
  int index;
  int fd;
  char ip[IP_SIZE];
  char id[ID_SIZE];
} CLIENT_INFO;

void *clnt_connection(void *arg);
void send_msg(MSG_INFO *msg_info, CLIENT_INFO *first_client_info);
void error_handling(const char *msg);
void log_message(const char *msgstr);
void get_local_time(char *buf);

int clnt_cnt = 0;
pthread_mutex_t mutx;

int main(int argc, char *argv[]) {
  int serv_sock, clnt_sock;
  struct sockaddr_in serv_adr, clnt_adr;
  socklen_t clnt_adr_sz;
  int sock_option = 1;
  pthread_t t_id[MAX_CLNT] = {0};
  int str_len = 0;
  int i;
  char client_id[ID_SIZE + 3];
  char *pToken;
  char *pArray[ARR_CNT] = {0};
  char msg[BUF_SIZE];

  CLIENT_INFO client_info[MAX_CLNT] = {
      {0, -1, "", "SERVER_SQL"},  {0, -1, "", "SERVER_STM32"},
      {0, -1, "", "SERVER_LIN"},  {0, -1, "", "SERVER_AND]"},
      {0, -1, "", "OUTSIDE_ARD"}, {0, -1, "", "INSIDE_ARD"},
      {0, -1, "", "CLOSET_ARD"}};

  if (argc != 2) {
    printf("사용법: %s <포트>\n", argv[0]);
    exit(1);
  }

  printf("IoT 서버 시작 (포트: %s)...\n", argv[1]);

  if (pthread_mutex_init(&mutx, NULL)) error_handling("뮤텍스 초기화 오류");

  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (serv_sock == -1) error_handling("소켓 생성 오류");

  if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void *)&sock_option,
                 sizeof(sock_option)) == -1)
    error_handling("소켓 옵션 설정 오류");

  memset(&serv_adr, 0, sizeof(serv_adr));
  serv_adr.sin_family = AF_INET;
  serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_adr.sin_port = htons(atoi(argv[1]));

  if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    error_handling("바인드 오류");

  if (listen(serv_sock, 5) == -1) error_handling("리슨 오류");

  while (1) {
    clnt_adr_sz = sizeof(clnt_adr);
    clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

    if (clnt_cnt >= MAX_CLNT) {
      printf("최대 연결 수 초과 (최대: %d)\n", MAX_CLNT);
      shutdown(clnt_sock, SHUT_WR);
      continue;
    } else if (clnt_sock < 0) {
      perror("accept()");
      continue;
    }

    str_len = read(clnt_sock, client_id, sizeof(client_id) - 1);
    if (str_len <= 0) {
      shutdown(clnt_sock, SHUT_WR);
      continue;
    }

    client_id[str_len] = '\0';

    /* ID 정보 파싱 - "[ID]" 형식 기대 */
    // if (client_id[0] != '[' || client_id[str_len - 1] != ']') {
    //   sprintf(msg, "잘못된 ID 형식입니다. %s [ID] 형식이어야 합니다.\n",
    //           client_id);
    //   // sprintf(msg, "수신 ID: %s\n", client_id);
    //   write(clnt_sock, msg, strlen(msg));
    //   log_message(msg);
    //   shutdown(clnt_sock, SHUT_WR);
    //   continue;
    // }

    client_id[str_len - 1] = '\0';
    char *extracted_id = client_id + 1;
    sprintf(msg, "수신 ID: %s\n", extracted_id);

    for (i = 0; i < MAX_CLNT; i++) {
      if (strcmp(client_info[i].id, extracted_id) == 0) {
        if (client_info[i].fd != -1) {
          sprintf(msg, "[%s] 이미 로그인되어 있습니다!\n", extracted_id);
          write(clnt_sock, msg, strlen(msg));
          log_message(msg);
          shutdown(clnt_sock, SHUT_WR);
#if 1
          client_info[i].fd = -1;
#endif
          break;
        }

        strcpy(client_info[i].ip, inet_ntoa(clnt_adr.sin_addr));

        pthread_mutex_lock(&mutx);
        client_info[i].index = i;
        client_info[i].fd = clnt_sock;
        clnt_cnt++;
        pthread_mutex_unlock(&mutx);

        sprintf(msg, "[%s] 새로운 연결 성공! (IP:%s, FD:%d, 접속자:%d)\n",
                extracted_id, inet_ntoa(clnt_adr.sin_addr), clnt_sock,
                clnt_cnt);
        log_message(msg);
        write(clnt_sock, msg, strlen(msg));

        pthread_create(&t_id[i], NULL, clnt_connection,
                       (void *)&client_info[i]);
        pthread_detach(t_id[i]);
        break;
      }
    }

    if (i == MAX_CLNT) {
      sprintf(msg, "%s 등록되지 않은 ID입니다!\n", extracted_id);
      write(clnt_sock, msg, strlen(msg));
      log_message(msg);
      shutdown(clnt_sock, SHUT_WR);
    }
  }

  pthread_mutex_destroy(&mutx);
  close(serv_sock);
  return 0;
}

void *clnt_connection(void *arg) {
  CLIENT_INFO *client_info = (CLIENT_INFO *)arg;
  int str_len = 0;
  int index = client_info->index;
  char msg[BUF_SIZE];
  char to_msg[MAX_CLNT * ID_SIZE + 1];
  int i = 0;
  char *pToken;
  char *pArray[ARR_CNT] = {0};
  char strBuff[LOG_BUF_SIZE] = {0};
  MSG_INFO msg_info;
  CLIENT_INFO *first_client_info;

  first_client_info = (CLIENT_INFO *)((void *)client_info -
                                      (void *)(sizeof(CLIENT_INFO) * index));

  while (1) {
    memset(msg, 0, sizeof(msg));
    str_len = read(client_info->fd, msg, sizeof(msg) - 1);

    if (str_len <= 0) break;

    msg[str_len] = '\0';
    pToken = strtok(msg, "[:]");
    i = 0;
    while (pToken != NULL && i < ARR_CNT) {
      pArray[i++] = pToken;
      pToken = strtok(NULL, "[:]");
    }

    msg_info.fd = client_info->fd;
    msg_info.from = client_info->id;
    msg_info.to = pArray[0];
    sprintf(to_msg, "[%s]%s", msg_info.from, pArray[1]);
    msg_info.msg = to_msg;
    msg_info.len = strlen(to_msg);

    sprintf(strBuff, "메시지: [%s -> %s] %s", msg_info.from, msg_info.to,
            pArray[1]);
    log_message(strBuff);

    send_msg(&msg_info, first_client_info);
  }

  close(client_info->fd);

  sprintf(strBuff, "연결 종료 ID:%s (IP:%s, FD:%d, 접속자:%d)\n",
          client_info->id, client_info->ip, client_info->fd, clnt_cnt - 1);
  log_message(strBuff);

  pthread_mutex_lock(&mutx);
  clnt_cnt--;
  client_info->fd = -1;
  pthread_mutex_unlock(&mutx);

  return NULL;
}

void send_msg(MSG_INFO *msg_info, CLIENT_INFO *first_client_info) {
  int i = 0;

  if (strcmp(msg_info->to, "ALLMSG") == 0) {
    for (i = 0; i < MAX_CLNT; i++) {
      if ((first_client_info + i)->fd != -1) {
        write((first_client_info + i)->fd, msg_info->msg, msg_info->len);
      }
    }
  }
  else if (strcmp(msg_info->to, "IDLIST") == 0) {
    char *idlist = (char *)malloc(ID_SIZE * MAX_CLNT);
    if (idlist == NULL) {
      log_message("메모리 할당 오류 (IDLIST)");
      return;
    }

    if (strlen(msg_info->msg) > 0) {
      msg_info->msg[strlen(msg_info->msg) - 1] = '\0';
      strcpy(idlist, msg_info->msg);
    } else {
      strcpy(idlist, "[시스템]");
    }

    for (i = 0; i < MAX_CLNT; i++) {
      if ((first_client_info + i)->fd != -1) {
        strcat(idlist, " ");
        strcat(idlist, (first_client_info + i)->id);
      }
    }
    strcat(idlist, "\n");

    write(msg_info->fd, idlist, strlen(idlist));
    free(idlist);
  }
  else if (strcmp(msg_info->to, "GETTIME") == 0) {
    sleep(1);
    get_local_time(msg_info->msg);
    write(msg_info->fd, msg_info->msg, strlen(msg_info->msg));
  }
  else {
    for (i = 0; i < MAX_CLNT; i++) {
      if ((first_client_info + i)->fd != -1) {
        if (strcmp(msg_info->to, (first_client_info + i)->id) == 0) {
          write((first_client_info + i)->fd, msg_info->msg, msg_info->len);
          break;
        }
      }
    }
  }
}

void error_handling(const char *msg) {
  fputs(msg, stderr);
  fputc('\n', stderr);
  exit(1);
}

void log_message(const char *msgstr) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char timestamp[20];

  sprintf(timestamp, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
  fputs(timestamp, stdout);
  fputs(msgstr, stdout);
  fputc('\n', stdout);
}

void get_local_time(char *buf) {
  struct tm *t;
  time_t tt;
  char wday[7][4] = {"일", "월", "화", "수", "목", "금", "토"};

  tt = time(NULL);
  if (errno == EFAULT) perror("time()");

  t = localtime(&tt);
  sprintf(buf, "[GETTIME]%02d.%02d.%02d %02d:%02d:%02d %s요일\n",
          t->tm_year + 1900 - 2000, t->tm_mon + 1, t->tm_mday, t->tm_hour,
          t->tm_min, t->tm_sec, wday[t->tm_wday]);
}
