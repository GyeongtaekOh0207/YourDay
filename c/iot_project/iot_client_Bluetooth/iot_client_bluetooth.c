#include <arpa/inet.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 5
#define DB_UPDATE_INTERVAL 10

void *send_msg(void *arg);
void *recv_msg(void *arg);
void *db_update_thread(void *arg);
void error_handling(char *msg);
char *fetch_data_from_db();

char name[NAME_SIZE] = "[Default]";

typedef struct {
  int sockfd;
  int btfd;
  char sendid[NAME_SIZE];
} DEV_FD;

int main(int argc, char *argv[]) {
  DEV_FD dev_fd;
  struct sockaddr_in serv_addr;
  pthread_t snd_thread, rcv_thread, db_thread;
  void *thread_return;
  int ret;
  struct sockaddr_rc addr = {0};

  char dest[18] = "98:DA:60:08:1F:6B";
  char msg[BUF_SIZE];

  if (argc != 4) {
    printf("Usage : %s <IP> <port> <name>\n", argv[0]);
    exit(1);
  }

  sprintf(name, "%s", argv[3]);

  dev_fd.sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if (dev_fd.sockfd == -1) error_handling("socket() error");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  serv_addr.sin_port = htons(atoi(argv[2]));

  if (connect(dev_fd.sockfd, (struct sockaddr *)&serv_addr,
              sizeof(serv_addr)) == -1)
    error_handling("connect() error");

  sprintf(msg, "[%s]", name);
  write(dev_fd.sockfd, msg, strlen(msg));

  dev_fd.btfd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
  if (dev_fd.btfd == -1) {
    perror("socket()");
    exit(1);
  }

  addr.rc_family = AF_BLUETOOTH;
  addr.rc_channel = (uint8_t)1;
  str2ba(dest, &addr.rc_bdaddr);

  ret = connect(dev_fd.btfd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret == -1) {
    perror("connect()");
    exit(1);
  }

  pthread_create(&rcv_thread, NULL, recv_msg, (void *)&dev_fd);
  pthread_create(&snd_thread, NULL, send_msg, (void *)&dev_fd);

  pthread_create(&db_thread, NULL, db_update_thread, (void *)&dev_fd);

  pthread_join(snd_thread, &thread_return);

  close(dev_fd.sockfd);
  return 0;
}

void *db_update_thread(void *arg) {
	DEV_FD *dev_fd = (DEV_FD *)arg;
  
	while (1) {
	  char *db_data = fetch_data_from_db();
	  if (db_data) {
		printf("DB Update - Sending to LCD:\n%s", db_data);
		
		write(dev_fd->btfd, db_data, strlen(db_data));
		
		free(db_data);
	  } else {
		char error_msg[] = "[LCD]DB Error@0\n";
		write(dev_fd->btfd, error_msg, strlen(error_msg));
	  }
	  
	  sleep(DB_UPDATE_INTERVAL);
	}
	
	return NULL;
  }

char *fetch_data_from_db() {
  MYSQL *conn;
  MYSQL_RES *res;
  MYSQL_ROW row;
  char *result = (char *)malloc(BUF_SIZE);
  if (!result) return NULL;

  conn = mysql_init(NULL);
  if (!conn) {
    perror("mysql_init() failed");
    free(result);
    return NULL;
  }

  if (!mysql_real_connect(conn, "localhost", "iot", "pwiot", "sensor_data",
                          3306, NULL, 0)) {
    perror("mysql_real_connect() failed");
    mysql_close(conn);
    free(result);
    return NULL;
  }

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char time_str[20];
  strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

  if (mysql_query(conn,
                  "SELECT temperature, humidity, Illuminance FROM dht_data "
                  "ORDER BY id DESC LIMIT 1")) {
    perror("mysql_query() failed");
    mysql_close(conn);
    free(result);
    return NULL;
  }

  res = mysql_store_result(conn);
  if (!res) {
    perror("mysql_store_result() failed");
    mysql_close(conn);
    free(result);
    return NULL;
  }

  row = mysql_fetch_row(res);
  if (row) {
    char *buffer = (char *)malloc(BUF_SIZE * 3);
    if (!buffer) {
      mysql_free_result(res);
      mysql_close(conn);
      free(result);
      return NULL;
    }

    buffer[0] = '\0';

    char time_msg[100];
    snprintf(time_msg, sizeof(time_msg),
             "[INSIDE_ARD]TIME_SENSOR@%s@%s@%s@%s\n", time_str, row[0], row[1],
             row[2]);
    strcat(buffer, time_msg);

    free(result);
    result = buffer;
  } else {
    snprintf(result, BUF_SIZE, "[INSIDE_ARD]No Data\n");
  }

  mysql_free_result(res);
  mysql_close(conn);
  return result;
}

void *recv_msg(void *arg) {
  DEV_FD *dev_fd = (DEV_FD *)arg;
  char name_msg[NAME_SIZE + BUF_SIZE + 1];
  int str_len;

  while (1) {
    memset(name_msg, 0x0, sizeof(name_msg));
    str_len = read(dev_fd->sockfd, name_msg, NAME_SIZE + BUF_SIZE);
    if (str_len <= 0) {
      dev_fd->sockfd = -1;
      return NULL;
    }
    name_msg[str_len] = 0;
    fputs("SRV:", stdout);
    fputs(name_msg, stdout);

    write(dev_fd->btfd, name_msg, strlen(name_msg));
  }
}

void *send_msg(void *arg) {
  DEV_FD *dev_fd = (DEV_FD *)arg;
  int ret;
  fd_set initset, newset;
  struct timeval tv;
  char msg[BUF_SIZE];
  int total = 0;

  FD_ZERO(&initset);
  FD_SET(dev_fd->sockfd, &initset);
  FD_SET(dev_fd->btfd, &initset);

  while (1) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    newset = initset;
    ret = select(dev_fd->btfd + 1, &newset, NULL, NULL, &tv);
    if (FD_ISSET(dev_fd->btfd, &newset)) {
      ret = read(dev_fd->btfd, msg + total, BUF_SIZE - total);
      if (ret > 0) {
        total += ret;
      } else if (ret == 0) {
        dev_fd->sockfd = -1;
        return NULL;
      }

      if (msg[total - 1] == '\n') {
        msg[total] = 0;
        total = 0;
      } else
        continue;

      fputs("ARD:", stdout);
      fputs(msg, stdout);
      if (write(dev_fd->sockfd, msg, strlen(msg)) <= 0) {
        dev_fd->sockfd = -1;
        return NULL;
      }
    }
  }
}

void error_handling(char *msg) {
  fputs(msg, stderr);
  fputc('\n', stderr);
  exit(1);
}