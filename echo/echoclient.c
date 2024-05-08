#include "csapp.h"

int main(int argc, char **argv) {
  int clientfd;
  char *host, *port, buf[MAXLINE];
  rio_t rio;

  if (argc != 3) {
    fprintf(stderr, "useage: %s <host> <port>\n", argv[0]);
    exit(0);
  }

  host = argv[1]; // 도메인 or IP
  port = argv[2]; // port number

  // 클라이언트 - 서버 연결 확립
  clientfd = Open_clientfd(host, port);

  Rio_readinitb(&rio, clientfd);
  
  // 입력 버퍼에 아무것도 없을 때까지 반복해서 입력
  while (Fgets(buf, MAXLINE, stdin) != NULL) {
    Rio_writen(clientfd, buf, strlen(buf)); // 서버에 입력 버퍼의 값으로 입력
    Rio_readlineb(&rio, buf, MAXLINE); // 서버로부터 데이터 읽기
    Fputs(buf, stdout); // 표준 출력으로 데이터 출력, EOF 만나면 종료
  }

  Close(clientfd); // 열었던 식별자를 명시적으로 닫음.
  exit(0);
}