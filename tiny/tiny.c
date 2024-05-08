/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *version, char* method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *version, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg, char *version);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 클라이언트 소켓 주소 ( IP, PORT )

  /* Check command line args */
  if (argc != 2) { // 인자를 2개 받도록 제어
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 표준 에러로 출력
    exit(1); // 프로세스를 (오류) 비정상적으로 종료
  }

  listenfd = Open_listenfd(argv[1]); // 듣기 소켓 생성
  while (1) {
    clientlen = sizeof(clientaddr); 
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept // 연결 구별자 생성, Accept시 요청에 대한 해더 정보가 포함된다.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0); // 구초제에 대응되는 호스트 및 서비스 이름 문자열로 변환
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio; // 읽기 버퍼 선언

  /* Read request line and headers */
  Rio_readinitb(&rio, fd); // 읽기 버퍼 초기화
  Rio_readlineb(&rio, buf, MAXLINE); // 요청 라인 읽기
  printf("Requeset headers: \n");
  printf("%s", buf); // 버퍼에 있는 헤더 정보 출력
  sscanf(buf, "%s %s %s", method, uri, version); // sscanf: 형식에 맞춰 데이터를 읽고 각 변수에 할당.

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) { // GET Method에 대해서만 처리(HEAD는 숙제문제)

    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method", version);
    return;
  }

  read_requesthdrs(&rio); // 헤더 정보 출력 -> Tiny에서는 header정보를 사용하지 않기 때문에 출력만...

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); // URI 구문 분석
  if (stat(filename, &sbuf) < 0) { // 디렉토리에 있는 파일 메타 데이터를 버퍼에 저장, 실패한 경우(-1) 에러 코멘트 출력
    clienterror(fd, filename, "404", "Not Found", "Tiny couldn't find this file", version);
    return;
  }

  if (is_static)  /* Serve static content */ 
  {  if ( !(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode) ) { // S_ISREF: 일반 파일인지 체크하는 내장 매크로, S_IRUSR :  소유자 읽기 권한 확인 매크로
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file", version);
      return;
    }

    serve_static(fd, filename, sbuf.st_size, version, method); // 정적 컨텐츠 전달
  }
  else { /* Serve dynamic content */
    if ( !(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode) ) { // S_IXUSR : 사용자 실행 권한 여부 확인 매크로
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program", version);
      return;
    }
    serve_dynamic(fd, filename, cgiargs, version, method); // 동적 컨텐츠 전달
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, char *version) { // 오류에 대한 정보를 클라이언트에게 보고
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>"); // sprintf : 첫번째 인자가 가리키는 문자열 배열에 두번 째 인자에 담긴 값을 쓴다.
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);
  
  /* Print the HTTP response */
  sprintf(buf, "%s %s %s\r\n", version, errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) // 요청 헤더 정보를 읽음.
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }

  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  if ( !strstr(uri, "cgi-bin")) { /* Static content */
    strcpy(cgiargs, ""); // cgi 인자 초기화
    strcpy(filename, "."); // 상대 경로로 설정
    strcat(filename, uri); // 상대 경로의 상세 path 제공
    if (uri[strlen(uri) - 1] == '/') // 파일 path가 '/'로 끝날 경우 기본 파일인 'home.html'로 응답.
      strcat(filename, "home.html");

    return 1;
  }
  else { /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr) { // cgi 인자가 있는 경우 인자를 추출
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    
    strcpy(filename, "."); // 상대경로로 파일 이름을 변경
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char *version, char* method)
{
  int srcfd;

  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype); // 파일 확장자 확인
  sprintf(buf, "%s 200 OK\r\n", version);
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  if (strcmp(method, "HEAD") == 0)
    return;

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // 정적 파일 열기

  /* 숙제 11.9 */
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // malloc + rio_readn -> 가상 메모리를 만들어서 fd에 있는 컨텐츠로 채워놓음
  // srcp = (char*)malloc(filesize);
  // Rio_readn(srcfd, srcp, filesize); // fd의 현재 파일 위치에서 메모리 위치 버퍼(srcp)로 최대 n 바이트 전송

  Close(srcfd); // 해당 파일 닫기
  Rio_writen(fd, srcp, filesize); // 버퍼(srcp)에서 식별자 fd에게 filesize만큼 데이터를 적는다. 쓴다.
  Munmap(srcp, filesize);  // 메모리 해제
  // free(srcp);
}

void get_filetype(char *filename, char *filetype) // 파일 이름내 파일 확장자를 통해 MIME 타입을 결정
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");

  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");

  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");

  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpg");

  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpeg");

  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *version, char *method)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "%s 200 OK\r\n", version);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */ // 자식 프로세스를 fork()
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // cgi가 사용할 인자를 환경 변수 설정을 통해 전달
    /* Add METHOD ENV for HEAD Method */
    setenv("REQUEST_METHOD", method, 1);
    Dup2(fd, STDOUT_FILENO); // 표준 입출력을 클라이언트로 할당(?) -> CGI 결과가 서버를 거치지지 않고 바로 클라이언트로 향함
    Execve(filename, emptylist, environ); // CGI 실행
  }

  Wait(NULL);
}