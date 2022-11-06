
#include "csapp.h"

void echo(int connfd);

void echo(int connfd){
  size_t n;
  char buf[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, connfd); // open한 식별자마다 한 번 호출, 식별자 connfd를 주소 rio에 위치한 읽기 버퍼와 연결
  while((n=Rio_readlineb(&rio, buf, MAXLINE)) != 0){ // rio에서 얻은 값을 buf에 복사함
    printf(" server received %d bytes\n", (int)n);
    Rio_writen(connfd, buf, n); // buf의 값을 connfd에 복사함
  }
}


int main(int argc, char **argv){
  /* 선언부 */
  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; /* Enough space for any address */
  char client_hostname[MAXLINE], client_port[MAXLINE];

  if(argc !=2){
    fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
    exit(0);
  }

  listenfd = Open_listenfd(argv[1]); // open_listenfd 함수를 사용해서 server 소켓을 listenfd로 만들어서 대기함
  while (1){ // EOF가 들어오기 전까지 계속 돌음
    clientlen = sizeof(struct sockaddr_storage); // clientlen 은 sockaddr_storage 구조체의 크기만큼
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // listenfd가 clinentfd를 만나면 accept가 되고 새로운 식별자를 connfd에 return 함
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0); // clientaddr와 clientlen을 이용해 client_hostname, client_port를 만들어냄
    printf("Connected to (%s %s)\n", client_hostname, client_port); // 그들을 출력
    echo(connfd); // echo 함수에 connfd 를 넣어서 echo 함수 돌림 --> 여기서 buf에 connfd에 복사함
    Close(connfd); // 그리고 닫음
  }
  exit(0);
}