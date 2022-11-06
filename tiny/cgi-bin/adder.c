/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

//두개의 정수를 더하는 CGI program
/*
웹 서버에서 동적인 페이지를 보여 주기 위해 임의의 프로그램을 실행할 수 있도록 하는 기술 중 하나.
간혹 동적인 페이지는 다 CGI라고 생각하는 사람들이 있는데 CGI 말고도 이런 역할을 하는 기술은 여럿 있다.
 단지 CGI가 맨 처음 나온 기술일 뿐.
*/
int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) !=NULL){
    p = strchr(buf, '&');
    *p = '\0';
    /*
    문자열을 표현할때는 문자열의 끝을 의미하는 문자인 '\0' 이 삽입됩니다.
    이 문자를 가리켜 널(null) 문자라하며 아스키코드값 0에 해당.
    symbol(name)은 NUL
    */
   strcpy(arg1, buf);
   strcpy(arg2, p+1);
   n1 = atoi(arg1);
   n2 = atoi(arg2);
  }

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content); // \r : 캐리지 리턴(줄의 첫 부분으로 이동), \n : 개행
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP responese*/
  printf("Connetion : close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s",content);
  fflush(stdout);
  
  exit(0);
}
/* $end adder */
