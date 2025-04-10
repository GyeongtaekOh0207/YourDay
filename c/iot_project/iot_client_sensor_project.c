#include <arpa/inet.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 5

void *send_msg(void *arg);
void *recv_msg(void *arg);
void error_handling(char *msg);

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];
int sock_global;

int main(int argc, char *argv[]) {
  int sock;
  struct sockaddr_in serv_addr;
  pthread_t snd_thread, rcv_thread, data_thread;
  void *thread_return;

  if (argc != 4) {
    printf("Usage : %s <IP> <port> <name>\\n", argv[0]);
    exit(1);
  }

  sprintf(name, "%s", argv[3]);

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock == -1) error_handling("socket() error");

  sock_global = sock;

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  serv_addr.sin_port = htons(atoi(argv[2]));

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("connect() error");

  sprintf(msg, "[%s]", name);
  write(sock, msg, strlen(msg));

  pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
  pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);

  pthread_join(snd_thread, &thread_return);
  pthread_join(rcv_thread, &thread_return);
  pthread_join(data_thread, &thread_return);

  close(sock);
  return 0;
}

void *send_msg(void *arg) {
  int *sock = (int *)arg;
  int str_len;
  int ret;
  fd_set initset, newset;
  struct timeval tv;
  char name_msg[NAME_SIZE + BUF_SIZE + 2];

  FD_ZERO(&initset);
  FD_SET(STDIN_FILENO, &initset);

  fputs("Input a message! [ID]msg (Default ID:ALLMSG)\\n", stdout);
  while (1) {
    memset(msg, 0, sizeof(msg));
    name_msg[0] = '\0';
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    newset = initset;
    ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
    if (FD_ISSET(STDIN_FILENO, &newset)) {
      fgets(msg, BUF_SIZE, stdin);
      if (!strncmp(msg, "quit\\n", 5)) {
        *sock = -1;
        return NULL;
      } else if (msg[0] != '[') {
        strcat(name_msg, "[ALLMSG]");
        strcat(name_msg, msg);
      } else
        strcpy(name_msg, msg);
      if (write(*sock, name_msg, strlen(name_msg)) <= 0) {
        *sock = -1;
        return NULL;
      }
    }
    if (ret == 0) {
      if (*sock == -1) return NULL;
    }
  }
}

void *recv_msg(void *arg) {
  MYSQL *conn;
  char sql_cmd[200] = {0};
  char *host = "localhost";
  char *user = "iot";
  char *pass = "pwiot";
  char *dbname = "sensor_data";

  int *sock = (int *)arg;
  int i;
  char *pToken;
  char *pArray[ARR_CNT] = {0};

  char name_msg[NAME_SIZE + BUF_SIZE + 1];
  int str_len;

  double temp;
  double humi;
  double illu;
  conn = mysql_init(NULL);

  if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0))) {
    fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
    exit(1);
  }

  while (1) {
    memset(name_msg, 0x0, sizeof(name_msg));
    str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
    if (str_len <= 0) {
      *sock = -1;
      return NULL;
    }
    name_msg[str_len] = 0;
    fputs(name_msg, stdout);

    pToken = strtok(name_msg, "[:@]");
    i = 0;
    while (pToken != NULL) {
      pArray[i] = pToken;
      if (++i >= ARR_CNT) break;
      pToken = strtok(NULL, "[:@]");
    }

    if (!strcmp(pArray[1], "SENSOR") && (i == 5)) {
      temp = atof(pArray[2]);
      humi = atof(pArray[3]);
      illu = atof(pArray[4]);

      sprintf(sql_cmd,
              "INSERT INTO dht_data (temperature, humidity, Illuminance, "
              "timestamp) "
              "VALUES (%lf, %lf, %lf, NOW())",
              illu, humi, temp);
      if (mysql_query(conn, sql_cmd)) {
        fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn),
                mysql_errno(conn));
      } else {
        printf("Inserted %lu rows\n", (unsigned long)mysql_affected_rows(conn));
      }
    }
  }
  mysql_close(conn);
}

void error_handling(char *msg) {
  fputs(msg, stderr);
  fputc('\n', stderr);
  exit(1);
}