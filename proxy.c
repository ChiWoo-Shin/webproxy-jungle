#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
void doit(int fd);
void find_host(char *uri, char *hostinfo, char *portinfo, char *remain_uri);
void make_header(char *hostinfo, char *portinfo, int fd, char *version, char *remain_uri);
void response_server(int pto_s_fd, int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *thread(void *vargp); // 동시성 - thread에서는 각각의 쓰레드 루틴마다 입력으로 한 개의 기본 포인터를 가져옴
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd)
{
  struct stat sbuf; // 이건 원래 동시성 할때 sbuf를 사용하면 필요한 부분 하지만 난 사용하지 않았음
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // client에게 받은 정보를 나눠줄 부분
  char hostinfo[MAXLINE], portinfo[MAXLINE], remain_uri[MAXLINE]; // uri를 우리가 필요한대로 쪼개서 저장할 부분
  int pto_s_fd; // 프록시에서 서버로 가는 소켓을 위한 선언부
  rio_t rio, stop_rio; // rio는 client --> proxy // stop_rio : server to proxy rio를 얘기함

  // client --> proxy
  Rio_readinitb(&rio, fd); // connfd를 rio 주소로 연결함 소켓 파일을 읽기 위해서
  Rio_readlineb(&rio, buf, MAXLINE); // line by line으로 읽어줌
  sscanf(buf, "%s %s %s", method, uri, version); // 쪼개서 저장해주고 = tiny와 동일
  printf("%s %s %s\n", method, uri, version);
  strcpy(version,"HTTP/1.0"); // 우리는 HTTP 버전이 뭘로 들어와도 1.0만 쓸거니깐
  printf("%s %s %s\n", method, uri, version); // 버전 바뀐거 확인해주고
  if (strcmp(method, "GET")) // Method는 GET만 쓸거임 그 외에는 error 발생
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  if (strcmp(uri, "/") != 0) // 들어온 uri가 / 가 아닐경우에만 동작함
  { // request 내용을 쪼갬
    find_host(uri, hostinfo, portinfo, remain_uri); // uri에서 host와 portinfo를 찾을거임
  }
  pto_s_fd=Open_clientfd(hostinfo,portinfo); // 프록시에서 서버로 가는 소켓을 열어줌 위에서 정리한 host 정보와 port 정보를 이용해서
  //make request header from proxy to server
  make_header(hostinfo, portinfo, pto_s_fd, version, remain_uri); // proxy에서 server로 가는 소캣에 Header를 만들어서 써줄거임

  // check response
  response_server(pto_s_fd, fd); // server에서 받은 소켓 파일을 connfd에 적어서 보내줄거임
  Close(pto_s_fd); // response 까지해서 connfd에 전부 보내줬으니 소켓을 닫아줌 - 메모리 누수 방지

}

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

void response_server(int pto_s_fd, int fd){
  char buf[MAXLINE], con_len[MAXLINE];
  rio_t stop_rio;
  char *p, *srcp;
  int content_len;

  //header를 읽는 부분
  Rio_readinitb(&stop_rio, pto_s_fd); // 처음 시작부 연결하고
  Rio_readlineb(&stop_rio, buf, MAXLINE); // 라인바이 라인으로 읽어주고
  while(strcmp(buf,"\r\n")){ // header의 끝은 \r\n 이니깐 헤더가 끝날때까지 반복합
    
    if (strncmp(buf,"Content-length:",15) == 0){ // 중간에 body에 대한 정보가 있는 부분 length 다음 숫자는 body의 byte 정보
      p=index(buf,32); // 해당 부분의 index를 찾음 (공백부)
      strcpy(con_len,p+1); // 공백 +1 부터 값을 저장해주고 (이땐 문자열)
      content_len = atoi(con_len); // 그걸 숫자로 바꿔줌
    }
    printf("%s",buf); // response 읽어온거 확인하는 부분
    Rio_writen(fd, buf, strlen(buf)); // 헤더부분을 line by line으로 읽은걸 connfd로 바로 보내줌 --> MAXLINE만큼 보내면 쓰레기 값이 같이가니 크기 잘 맞춰줘야함
    Rio_readlineb(&stop_rio, buf, MAXLINE); // read를 다음 line으로 이동
  }
  Rio_writen(fd, buf, strlen(buf));  //header가 끝난 \r\n 부분을 connfd로 보내주고
  printf("len : %d\n",content_len); // body 에 대한 byte가 제대로 들어왔는지 확인함

  srcp=malloc(content_len); // 임시 포인터에 body 크기에 맞는 공간을 만들어줌
  Rio_readnb(&stop_rio,srcp,content_len); // stop_rio의 현재 주소는 body의 최상단이니 body의 최상단부터 content_len 크기만큼 srcp에 넣어줌
  Rio_writen(fd,srcp,content_len); // 위에서 넣어준 정보를 connfd로 content_len 만큼 보내줌
  free(srcp); // 얘는 더이상 필요 없으니 free -- memory leak 방지
}

void find_host(char *uri, char *hostinfo, char *portinfo, char *remain_uri)
{
  char buf[MAXLINE]; // 임시 공간을 만들어서
  char *p;

  if (strstr(uri, "://"))
  {
    strcpy(buf, uri);
    p = strchr(buf, 58); // http:// < 제거
    *p = '\0';
    strcpy(buf, p + 1);
    p = strchr(buf, 47); // http:// < 제거
    *p = '\0';
    strcpy(buf, p + 1);
    p = strchr(buf, 47); // http:// < 제거
    *p = '\0';
    strcpy(hostinfo, p + 1); // :// 뒤에부터 저장
  }
  else{
    strcpy(hostinfo,uri);
  }

  if (strstr(hostinfo, ":")){
    p = strchr(hostinfo, 58); // www. 부터 카운트
    *p = '\0';
    strcpy(portinfo, p + 1); //  www.host.com 은 buf에 저장되고 그 뒤에가 portinfo에 저장
    if (strchr(portinfo, 47)){
      p = strchr(portinfo, 47);
      strcpy(remain_uri, p);
      *p = '\0';
    }
    else{
      sprintf(remain_uri,"/");
    }
  }
  else{
    sprintf(portinfo,"80");
    if(strchr(hostinfo,47)){
      p = strchr(hostinfo, 47);
      strcpy(remain_uri, p);
      *p = '\0';
    }
    else{
      sprintf(remain_uri,"/");
    }
  }  

  return;
}

//request headers from proxy to server
void make_header(char *hostinfo, char *portinfo, int pto_s_fd, char *version, char *remain_uri)
{
  char buf[MAXLINE]; // 임시 공간을 만들어주고

  //header를 만들어주는 부분
  sprintf(buf, "GET %s %s\n", remain_uri, version);
  sprintf(buf, "%sHost: %s:%s\n", buf, hostinfo, portinfo);
  strcat(buf, user_agent_hdr);
  strcat(buf, "Connection: close\r\n");
  strcat(buf, "Proxy-Connection: close\r\n\r\n");
  // header가 제대로 되어있는지 확인
  printf("%s",buf);
  // buf에 저장된 header를 proxy to server 소캣에 써줌 --> 쓰자마자 바로 response가 와서 pto_s_fd에 response가 적힐거임
  Rio_writen(pto_s_fd, buf, strlen(buf));
}

void *thread(void *vargp){ // 동시성 - thread에서는 각각의 쓰레드 루틴마다 입력으로 한 개의 기본 포인터를 가져옴
  int connfd = *((int *)vargp); // 위에서 받은 식별자를 int로 형변환
  Pthread_detach(pthread_self()); // thread를 분리함 인자는 자기자신 -- 분리된 thread는 종료되자마자 자원은 free함 -->그래서 종료가 따로 없는듯?
  Free(vargp); // 얘는 이미 connfd로 옮겨놨으니 쓸모가 다함 -- free
  doit(connfd); // client- proxy 연결을 통해서 정보를 요청함
  Close(connfd); // doit이 다 끝나면 client-proxy 사이의 connfd를 닫고
  return NULL; // 전부 끝났으니 thread도 NULL 로 return 함 -- thread도 종료됐으니 자원도 free됨
}

int main(int argc, char **argv)
{
  int listenfd, *connfdp; // connfdp가 포인터인 이유는 동시성에서 Thread에 통째로 전달해주기 위해서 
  char hostname[MAXLINE], port[MAXLINE]; // hostname과 port를 사용하기 위한 공간 만들기
  socklen_t clientlen; // socket을 쓸거니깐 크기 선언부
  struct sockaddr_storage clientaddr; //input 받은 clientaddress를 넣을 공간
  pthread_t tid; // peer thread의 ID를 선언

  if (argc != 2) // 정보가 제대로 들어왔는지 인자가 2개 들어온건지
  {
    fprintf(stderr, "usage %s <port> \n", argv[0]);
    exit(0);
  }

  listenfd = Open_listenfd(argv[1]); // 프록시의 listen 소캣부를 열어줌
  while (1)
  {
    clientlen = sizeof(clientaddr); // clientlen 에 적합한 크기 설정
    connfdp=malloc(sizeof(int)); // connfdp에 공간을 만들어줌 왜? accept 된 이후에 정보를 넣어주려고 포인터가 아닐때는 필요없었지만 포인터라서 필요한 부분
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    Pthread_create(&tid, NULL, thread, connfdp); // Peer thread의 생성 - 인자설명 : 1. thread id 넣고, 2. attribute = NULL, 3. 함수를 호출, 4. connection 식별자
    // pthread_create 를 호출할 때 연결 식별자를 전달하는 방법은 식별자를 가리키는 포인터를 같이 보내는 것--> 그래서 connfdp를 포인터로 선언
    // doit(connfd);
    // Close(connfd);
  }

}
