
#include "csapp.h"

int main(int argc, char **argv){
  /* 선언부 */
  int clientfd;
  char *host, *port, buf[MAXLINE];
  rio_t rio;

  if(argc !=3){
    fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
    exit(0);
  }

  host = argv[1];
  port = argv[2];

  clientfd = Open_clientfd(host, port); // open_clientfd 함수를 이용해서 소켓을 client fd로 만듦
  Rio_readinitb(&rio, clientfd); // clientfd에 rio의 주소를 연결
  
  while (Fgets(buf, MAXLINE, stdin) != NULL){ // 표준 입력 스트림을 사용해서 buf 에서 MAXLINE 크기 만큼 읽어오고
    Rio_writen(clientfd, buf, strlen(buf)); // 연결된 주소 clientfd 에 buf를 strlen(buf) 만큼 복사해 넣고
    Rio_readlineb(&rio, buf, MAXLINE); // rio에 있는 값을 buf에 복사해넣음 MAXLINE 만큼
    Fputs(buf, stdout); // 표준 출력 스트림에 buf를 넣음
  }
  Close(clientfd); // 메모리 누수를 막기 위해서 clientfd를 닫음
  exit(0); // 함수 밖으로 나감
}