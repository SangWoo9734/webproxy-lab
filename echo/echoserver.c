#include "csapp.h"


void echo(int connfd)
{
  size_t n;
  char buf [MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, connfd);
  while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    printf("server received %d byes\n", (int)n);
    Rio_writen(connfd, buf, n);
  }
}

int main(int argc, char **argv)
{
  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 소켓 구조체
  char client_hostname[MAXLINE], client_port[MAXLINE];

  if (argc != 2) {
    fprintf(stderr, "useage: %s <port>\n", argv[0]);
    exit(0);
  }

  // 듣기 식별자 오픈
  listenfd = Open_listenfd(argv[1]);
  

  // 무한루트 start
  while (1) {
    clientlen = sizeof(struct sockaddr_storage);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    
    printf("Connected to (%s %s)\n", client_hostname, client_port);
    echo(connfd);
    Close(connfd);
  }

  exit(0);
}