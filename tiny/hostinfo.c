
#include "csapp.h"

int main(int argc, char **argv){
  struct addrinfo *p, *listp, hints;
  char buf[MAXLINE];
  int rc, flags;

  if(argc !=2){
    fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
    exit(0);
  }

  /* Get a list of addrinfo records*/
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET; /* IPv4 only */
  hints.ai_socktype = SOCK_STREAM; /* Connections only */
  if ((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0){ // 이때 getaddrinfo 를 통하여 listp에 hints의 정보와 argv[1] 에서 가져온 정보를 합쳐서 listp로 내보냄
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc)); // 현재 listp에는 hints의 정보 + argv[1] ( == addrinfo struct의 제일 top부분)
    exit(1);
  }

  /* Walk the list and display each IP address */
  flags = NI_NUMERICHOST; /* Display address string instead of domain name 숫자 주소를 return 함*/
  for (p = listp; p; p=p->ai_next){ // listp 의 정보를 p에 넣음
    Getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, NULL, 0, flags); // top부터 struct를 탐색하면서 get name info에 값을 넣어줌 이때 분리되어있던 ai_addr과 addlen을 합쳐 ip주소로 변환 후 buf에 넣어줌
    printf("%s\n", buf); // 해당 buf를 출력
  }

  /* Clean up */
  Freeaddrinfo(listp);

  exit(0);
}