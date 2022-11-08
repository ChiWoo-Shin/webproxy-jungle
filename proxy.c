#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
void doit(int fd);
void find_host(char *uri, char *hostinfo, char *portinfo, char *remain_uri);
void make_header(char *hostinfo, char *portinfo, int fd, char *version, char *remain_uri);
void response_server(int pto_s_fd, int fd);
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd)
{
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  char hostinfo[MAXLINE], portinfo[MAXLINE], remain_uri[MAXLINE];
  int pto_s_fd;
  rio_t rio, stop_rio;

  // client --> proxy
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("%s %s %s\n", method, uri, version);
  strcpy(version,"HTTP/1.0");
  printf("%s %s %s\n", method, uri, version);
  if (strcmp(method, "GET"))
  {
    // clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    printf("문제가 있음");
    return;
  }
  if (strcmp(uri, "/") != 0)
  { // request 내용을 쪼갬
    find_host(uri, hostinfo, portinfo, remain_uri); // host와 portinfo를 찾을거임
  }
  pto_s_fd=Open_clientfd(hostinfo,portinfo);
  //make request header from proxy to server
  make_header(hostinfo, portinfo, pto_s_fd, version, remain_uri);

  // check response
  response_server(pto_s_fd, fd); 

}

void response_server(int pto_s_fd, int fd){
  char buf[MAXLINE], con_len[MAXLINE];
  rio_t stop_rio;
  char *p, *srcp;
  int content_len;

  
  Rio_readinitb(&stop_rio, pto_s_fd);
  Rio_readlineb(&stop_rio, buf, MAXLINE);
  while(strcmp(buf,"\r\n")){
    
    if (strncmp(buf,"Content-length:",15) == 0){
      p=index(buf,32);
      strcpy(con_len,p+1);
      content_len = atoi(con_len);
    }
    printf("%s",buf);
    Rio_writen(fd, buf, strlen(buf));
    Rio_readlineb(&stop_rio, buf, MAXLINE);
  }
  Rio_writen(fd, buf, strlen(buf));  
  printf("len : %d\n",content_len);

  srcp=malloc(content_len);
  Rio_readnb(&stop_rio,srcp,content_len);
  Rio_writen(fd,srcp,content_len);
  free(srcp);
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
    p = strchr(portinfo, 47);
    strcpy(remain_uri, p);
    *p = '\0';
  }
  else{
    strcpy(portinfo,"80");
    p = strchr(hostinfo, 47);
    strcpy(remain_uri, p);
    *p = '\0';

  }  

  return;
}

//request headers from proxy to server
void make_header(char *hostinfo, char *portinfo, int pto_s_fd, char *version, char *remain_uri)
{
  char buf[MAXLINE];

  sprintf(buf, "GET %s %s\n", remain_uri, version);
  sprintf(buf, "%sHost: %s:%s\n", buf, hostinfo,portinfo);
  strcat(buf, user_agent_hdr);
  strcat(buf, "Connection: close\r\n");
  strcat(buf, "Proxy-Connection: close\r\n\r\n");

  printf("%s",buf);

  Rio_writen(pto_s_fd, buf, strlen(buf));
}

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2)
  {
    fprintf(stderr, "usage %s <port> \n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    doit(connfd);
    Close(connfd);
  }

  // printf("%s", user_agent_hdr);
  // return 0;
}
