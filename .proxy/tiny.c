/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */

// fd = file descriptor : Unix OS에서 네트워크 소켓과 같은 파일이나 기타 입력/출력 리소스에 액세스하는 데 사용되는 추상표현이다. 즉, 시스템으로 부터 할당받은 파일이나 소켓을 대표하는 정수다
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // HTTP는 제한이 없지만 MAXLINE에 대한 제한은 없지만 서버 이용을 위해서 길이의 제한을 이용해줌 
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  
  /*Read request line and headers*/
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE); // rio의 정보를 buf에 저장
  printf("Request headers:\n"); 
  sscanf(buf, "%s %s %s",method, uri, version); // 이를 쪼개서 method, uri,version에 저장함
  printf("%s %s %s\n",method, uri, version);
  if (strcmp(method, "GET") && strcmp(method, "HEAD")){ // Tiny는 GET method만 지원 - 다른 method를 요청하면 에러메시지를 보내고, main 루틴으로 돌아옴
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); // 위 if문에 걸리지 않으면 값을 읽어들이고, 다른 요청 헤더들을 무시함

  /* Parse URI from GET request*/
  is_static = parse_uri(uri, filename, cgiargs); // URI 파일 이름과 비어있을 수도 있는 CGI 인자 스트링을 분석, 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그를 설정 
  // parse_uri를 통해서 인자기 있는지 확인하고 있으면 있는대로 없으면 없는대로 uri를 재정리해줌 --> 정적이었으면 1을 return 했을거고 동적이면 0을 return 함


  /*
  stat 함수 설명
  -각 함수들의 호출 성공시 0을 반환하며 두번째 인자인 stat 구조체에 파일 정보들로 채워진다.
  실패 혹은 에러시 -1을 리턴하고 에러시에 errno 변수에 에러 상태가 set된다.
  */
  if (stat(filename, &sbuf)<0){ // 만일 file이 디스크상에 있지 않으면, 에러메시지를 즉시 클라이언트에게 보내고 return 함
    clienterror(fd, filename, "404", "Not found", "Tiny couldnt' find this file");
    return;
  }

  if (is_static){ /* Serve static content -- request가 정적 컨텐츠를 위한 것이라면*/ 
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){ // 해당 파일이 보통 파일이라는 것과 읽기 권한을 가지고 있는지를 검증
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); // 위 if문에 걸리지 않으면 정적 컨텐츠를 클라이언트에게 제공
  }
  else{ /* Serve dynamic content -- request 가 동적 컨텐츠를 위한 것이라면*/
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){ // 해당 파일이 보통 파일이라는 것과 실행 가능한지를 검증
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // 동적 컨텐츠를 제공
  }
}

// 실제 서버에서 볼수 있는 많은 에러 처리 기능들은 빠져있음
// 하지만 일부 명백한 오류에 대해서는 체크하고 있음 --> 체크 후 클라이언트에게 보고 + 브라우저 사용자에게 에러를 설명하는 응답 본체에 HTML 파일도 함께 보냄
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXLINE];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n",body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp){ // Tiny는 요청 헤더 내의 어떤 정보도 사용하지 않음 -- 해당 함수를 사용해서 이들을 읽고 무시함
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE); // rp의 값을 buf에 복사해 넣음 line by line으로 움직임
  printf("%s", buf);
  while(strcmp(buf, "\r\n")){  // 요청 헤더를 종료하는 빈 텍스트 줄이 6번 줄에서 체크하고 있는 carriage return과 line feed 쌍으로 구성되어 있음
    Rio_readlineb(rp, buf, MAXLINE); // buf 가 \r\n 과 동일하면 멈출거고 그게 아니면 지속적으로 line을 읽으면서 복사해나감
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs){ // Tiny는 정적 컨텐츠를 위한 홈 디렉토리가 자신의 현재 디렉토리이고, 실행 파일의 홈 디렉토리는 /cgi-bin 이라고 가정함
                                                         // 스트링 cgi-bin 을 포함하는 모든 URI는 동적 컨텐츠(왜냐면 cgi-bin안에는 동적 콘텐츠 파일이 들어있으니깐)를 요청하는 것을 나타낸다고 가정 - 기본 파일 이름은 ./home.html
  char *ptr;
  if(!strstr(uri, "cgi-bin")){ // static content - 만일 요청이 정적 컨텐츠라면 - strstr 함수 strstr(str1, str2) : str1 안에 str2가 있는지 확인 있으면 True 없으면 False
    strcpy(cgiargs, ""); // CGI 인자 스트링을 지우고 (cgiargs에 ""을 복사한다 - 즉 없애버림)
    strcpy(filename, "."); // filename에 .을 복사함
    strcat(filename, uri); // 그리고 위에서 바뀐 . 뒤에 uri를 붙여넣음 --> 이를통해서 접미어 부분이 ./index.html 같은 모습으로 바뀜
    if (uri[strlen(uri)-1]=='/') // 만일 문자가 '/'로 끝난다면
      strcat(filename, "home.html"); // 기본 파일 이름을 추가함
    return 1;
  }
  else{ // Dynamic content - 이 request가 동적 컨텐츠를 위한 것이라면
    ptr = index(uri, '?'); // 모든 CGI 인자들을 추출 start -- uri에 있는 ? 의 위치부터 뒤에까지 쭉 넣어줌
    if (ptr) { // 그 위치가 TRUE이면 즉 ? 가 존재하면
      strcpy(cgiargs, ptr+1); // cgiargs를 "?"를 뺀 그 다음부터의 값을 넣어줌
      *ptr = '\0'; // 그리고 해당 ptr은 NULL로 변경함
    }
    else
      strcpy(cgiargs, ""); // "?"가 없으면 == 즉 인자가 더 이상 없음 --> cgiargs를 지움
    strcpy(filename, "."); // --> file name을 .으로 바꾸고
    strcat(filename, uri); // --> 그 뒤에 uri를 붙여서 형태를 만들어줌
    return 0;
  }
}
//정적 컨텐츠에서 사용하는 적합한 file type을 찾으러간다
void serve_static (int fd, char *filename, int filesize){ // Tiny는 5개의 서로 다른 정적 컨텐츠 타입을 지원 HTML, 무형식 Text file, GIF, PNG, JPEG로 인코딩된 영상
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype); // 파일 이름의 접미어 부분을 검사해서 파일 타입을 결정하고 HTTP 응답을 보냄
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // response line과 response header를 client에게 보냄 start
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); // 위에서부터 누적해서 buf + 해당 text를 누적함
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // \r\n 이 반복되어서 빈 줄이 하나 더 생김 --> 헤더를 종료함
  Rio_writen(fd, buf, strlen(buf)); // response line과 response header를 client에게 보냄 end -- 위에서 받은 buf를 fd에 복사해넣음 buf 크기만큼
  printf("Response headers: \n");
  printf("%s", buf); // 마지막에 buf에 누적된 것들을 다시 확인

  /* Send response body to client -- 요청한 파일의 내용을 연결 식별자 fd로 복사해서 응답 본체를 보냄 */
  srcfd = Open(filename, O_RDONLY, 0); // filename을 open하고 식별자를 얻음
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 리눅스 mmap 함수는 요청한 파일을 가상메모리 영역으로 맵핑
  // Close(srcfd); // 파일을 메모리로 맵핑 후에 더 이상 해당 식별자는 필요 없으며, 이 파일을 닫음 --> 하지 않으면 치명적인 메모리 누수가 발생할 수 있음
  // Rio_writen(fd, srcp, filesize); // 실제로 파일을 클라이언트에게 전송 -- 주소 srcp에서 시작하는 filesize 바이트를 클라이언트의 연결 식별자로 복사
  // Munmap(srcp, filesize); // 매핑된 가상메모리 주소를 반환 -- 메모리 누수를 피하기 위해서도 필수임

  // mmap대신에 malloc으로 전달
  srcp = (char *)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp,filesize);
  free(srcp);
}

/*
get_ filetype - Derive file type from filename
*/
void get_filetype(char *filename, char *filetype){ // 적합한 file type을 찾는다
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if(strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if(strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

//클라이언트에 성공을 알려주는 응답 라인을 보내는 것으로 시작함 -- CGI 프로개름은 응답의 나머지 부분을 보내야 함
// 해당 CGI 는 기대하는 것만큼 견고하지 않음 -- CGI 프로그램이 에러를 만날 수 있다는 가능성을 염두에 두지 않았기 때문
void serve_dynamic(int fd, char *filename, char *cgiargs){ // Tiny는 child process를 fork 하고, CGI 프로개름을 자식의 context에서 실행하며, 모든 종류의 동적 콘텐츠를 제공
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* return first part of HTTP response*/
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0){ /* Child -- 응답의 첫번째 부분을 보낸 후 새로운 자식 프로세스를 fork 함*/
    /* Real server would set all CGI vars here*/
    setenv("QUERY_STRING", cgiargs, 1); // QUERY_STRING 환경변수를 요청하면 URI의 CGI인자들을 초기화
    //setenv 함수 : 환경변수의 새 변수를 정의하거나 호출 -- 환경 변수 이름, 환경변수 값, 이미 같은 이름의 값이 있다면 값을 변경할지 여부확인 (0은 고정, 1은 변경)
    Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client -- 자식은 자식의 표준 출력을 연결 파일 식별자로 재지정*/
    /*
    dup2는 newfd 파일 descripter가 이미 open된 파일이면 close하고 oldfd를 newfd로 복사를 합니다
    parameter
    oldfd : 복사하려는 원본 file descripter
    newfd : 복사되는 target file descripter 
            (만약 newfd가 열려진 file descripter이면, 먼저 close후에 복사함)
    */
    Execve(filename, emptylist, environ); /* Run CGI program -- CGI 프로그램을 로드하고 실행*/
  }
  Wait(NULL); /* Parent waits for and reaps child -- 부모는 자식이 종료되어 정리되는 것을 기다리기 위해 wait 함수에서 블록됨*/
}

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) { // 두개 라는건 file name과 port 해서 2개 즉 처음에 port 입력 안되면 에러다
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 0은 file name
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept - 반복적으로 연결 요청을 접수 -- 연결이 완료된 이후에 connfd로 return
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit - transaction을 실행
    Close(connfd);  // line:netp:tiny:close - 자신 쪽의 연결 끝을 닫음
  }
}
