
#include "csapp.h"

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