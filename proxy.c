#include <stdio.h>
#include "csapp.h"
#include "malloc.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
void doit(int fd);
void find_host(char *uri, char *hostinfo, char *portinfo, char *remain_uri);
void make_header(char *hostinfo, char *portinfo, int fd, char *version, char *remain_uri);
int response_server(int pto_s_fd, int fd, int size_buf, char *cache_buf);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *thread(void *vargp); // 동시성 - thread에서는 각각의 쓰레드 루틴마다 입력으로 한 개의 기본 포인터를 가져옴

/* function for cache*/
void cache_init();
int cache_find(char *uri);
void cache_uri(char *uri, char *cache_buf, int size_buf);
void cache_LRU(int min_idx);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

typedef struct
{
  char cache_object[MAX_OBJECT_SIZE]; // 캐시 객체 저장
  char cache_url[MAXLINE];            // 캐시 URL 저장
  int LRU;                            // 캐시 넘버링
  int readCNT;                        // 캐시에 read로 접근하는 수
  int Empty_check;                    // 캐시가 비어있는지 확인 -- 0 이면 empty이고 1이면 full임
  int cache_size;

  sem_t writing_protect; // 세마포어를 이용한 writing가 중복되지 않게 방어
  sem_t rdcnt_protect;   // 세마포어를 이용하여 현재 reader의 수를 조작할때 한번에 한 스레드만 조작가능하게 함
} node_t;                // 각각의 cache를 저장할 공간

typedef struct
{
  node_t cachelist[10]; // 전체 캐시리스트는 총 10개까지 쓸 수 있다고 가정하였으니깐 10개
  
} cache_tree;           // 전체 캐시 리스트를 관리할 공간

cache_tree cache;

void doit(int fd)
{
  struct stat sbuf;                                                   // 이건 원래 동시성 할때 sbuf를 사용하면 필요한 부분 하지만 난 사용하지 않았음
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // client에게 받은 정보를 나눠줄 부분
  char hostinfo[MAXLINE], portinfo[MAXLINE], remain_uri[MAXLINE];     // uri를 우리가 필요한대로 쪼개서 저장할 부분
  int pto_s_fd;                                                       // 프록시에서 서버로 가는 소켓을 위한 선언부
  rio_t rio, stop_rio;                                                // rio는 client --> proxy // stop_rio : server to proxy rio를 얘기함
  // 캐시를 위한 선언부
  char cache_buf[MAX_OBJECT_SIZE] = {NULL};
  int size_buf = 0;
  int *content_len;

  // client --> proxy
  Rio_readinitb(&rio, fd);                       // connfd를 rio 주소로 연결함 소켓 파일을 읽기 위해서
  Rio_readlineb(&rio, buf, MAXLINE);             // line by line으로 읽어줌
  sscanf(buf, "%s %s %s", method, uri, version); // 쪼개서 저장해주고 = tiny와 동일
  strcpy(version, "HTTP/1.0");                   // 우리는 HTTP 버전이 뭘로 들어와도 1.0만 쓸거니깐
  printf("%s %s %s\n", method, uri, version);    // 버전 바뀐거 확인해주고

  if (strcasecmp(method, "GET")) // Method는 GET만 쓸거임 그 외에는 error 발생
  {
    clienterror(fd, method, "501", "Not implemented", "Proxy does not implement this method");
    return;
  }
  int cache_index;
  if ((cache_index = cache_find(uri)) != -1)
  {
    pre_read(cache_index); // 캐시 읽어올때 locking걸고 -- 우선순위는 read > write

    Rio_writen(fd, cache.cachelist[cache_index].cache_object, cache.cachelist[cache_index].cache_size + 100); // 읽은걸 바로 connfd로 보내줌

    after_read(cache_index); // 캐시 다 읽었으니 unlocking으로 바꿔줌 -- 하지만 pre_read가 많다면 (즉 동시에 읽고 있는 사람들이 많다면) 마지막 reader가 빠져나갈때까지는 locking상태임

    return;
  }
  
  if (strcmp(uri, "/") != 0)                        // 들어온 uri가 / 가 아닐경우에만 동작함
  {                                                 // request 내용을 쪼갬
    find_host(uri, hostinfo, portinfo, remain_uri); // uri에서 host와 portinfo를 찾을거임
  }
  pto_s_fd = Open_clientfd(hostinfo, portinfo); // 프록시에서 서버로 가는 소켓을 열어줌 위에서 정리한 host 정보와 port 정보를 이용해서
  // make request header from proxy to server
  make_header(hostinfo, portinfo, pto_s_fd, version, remain_uri); // proxy에서 server로 가는 소캣에 Header를 만들어서 써줄거임

  // check response
  size_buf = response_server(pto_s_fd, fd, size_buf, cache_buf); // server에서 받은 소켓 파일을 connfd에 적어서 보내줄거임
    
  Close(pto_s_fd); // response 까지해서 connfd에 전부 보내줬으니 소켓을 닫아줌 - 메모리 누수 방지
  
  // 만일 캐시가 없었다면 위에까지 진행될꺼임
  // 그러면 우리는 서버에서 받은 정보를 캐시에 저장해줘야함
  if (size_buf < MAX_OBJECT_SIZE)
    cache_uri(uri, cache_buf, size_buf);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXLINE];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
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

int response_server(int pto_s_fd, int fd, int size_buf, char *cache_buf)
{
  char buf[MAXLINE];
  rio_t stop_rio;
  size_t n; // Rio readlineb 함수로부터 크기를 돌려받기 위한 공간
  char *p, *srcp;

  Rio_readinitb(&stop_rio, pto_s_fd);
  while ((n = Rio_readlineb(&stop_rio, buf, MAXLINE)) != 0)
  {
    size_buf += n; // 크기를 모아서 size_buf를 만들고
    if (size_buf < MAX_OBJECT_SIZE) // MAX보다 작으면
      strcat(cache_buf, buf); // buf를 cache에 저장
    Rio_writen(fd, buf, n); // 그리고 connfd로 보내주고
  }

  return size_buf; // 캐시에 저장하기 위한 크기 return
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
  else
  {
    strcpy(hostinfo, uri);
  }

  if (strstr(hostinfo, ":"))
  {
    p = strchr(hostinfo, 58); // www. 부터 카운트
    *p = '\0';
    strcpy(portinfo, p + 1); //  www.host.com 은 buf에 저장되고 그 뒤에가 portinfo에 저장
    if (strchr(portinfo, 47))
    {
      p = strchr(portinfo, 47);
      strcpy(remain_uri, p);
      *p = '\0';
    }
    else
    {
      sprintf(remain_uri, "/");
    }
  }
  else
  {
    sprintf(portinfo, "80");
    if (strchr(hostinfo, 47))
    {
      p = strchr(hostinfo, 47);
      strcpy(remain_uri, p);
      *p = '\0';
    }
    else
    {
      sprintf(remain_uri, "/");
    }
  }

  return;
}

// request headers from proxy to server
void make_header(char *hostinfo, char *portinfo, int pto_s_fd, char *version, char *remain_uri)
{
  char buf[MAXLINE]; // 임시 공간을 만들어주고

  // header를 만들어주는 부분
  sprintf(buf, "GET %s %s\n", remain_uri, version);
  sprintf(buf, "%sHost: %s:%s\n", buf, hostinfo, portinfo);
  strcat(buf, user_agent_hdr);
  strcat(buf, "Connection: close\r\n");
  strcat(buf, "Proxy-Connection: close\r\n\r\n");
  // header가 제대로 되어있는지 확인
  printf("%s", buf);
  // buf에 저장된 header를 proxy to server 소캣에 써줌 --> 쓰자마자 바로 response가 와서 pto_s_fd에 response가 적힐거임
  Rio_writen(pto_s_fd, buf, strlen(buf));
}

void *thread(void *vargp)
{                                 // 동시성 - thread에서는 각각의 쓰레드 루틴마다 입력으로 한 개의 기본 포인터를 가져옴
  int connfd = *((int *)vargp);   // 위에서 받은 식별자를 int로 형변환
  Pthread_detach(pthread_self()); // thread를 분리함 인자는 자기자신 -- 분리된 thread는 종료되자마자 자원은 free함 -->그래서 종료가 따로 없는듯?
  Free(vargp);                    // 얘는 이미 connfd로 옮겨놨으니 쓸모가 다함 -- free
  doit(connfd);                   // client- proxy 연결을 통해서 정보를 요청함
  Close(connfd);                  // doit이 다 끝나면 client-proxy 사이의 connfd를 닫고
  return NULL;                    // 전부 끝났으니 thread도 NULL 로 return 함 -- thread도 종료됐으니 자원도 free됨
}

int main(int argc, char **argv)
{
  int listenfd, *connfdp;                // connfdp가 포인터인 이유는 동시성에서 Thread에 통째로 전달해주기 위해서
  char hostname[MAXLINE], port[MAXLINE]; // hostname과 port를 사용하기 위한 공간 만들기
  socklen_t clientlen;                   // socket을 쓸거니깐 크기 선언부
  struct sockaddr_storage clientaddr;    // input 받은 clientaddress를 넣을 공간
  pthread_t tid;                         // peer thread의 ID를 선언

  if (argc != 2) // 정보가 제대로 들어왔는지 인자가 2개 들어온건지
  {
    fprintf(stderr, "usage %s <port> \n", argv[0]);
    exit(0);
  }

  cache_init();

  listenfd = Open_listenfd(argv[1]); // 프록시의 listen 소캣부를 열어줌
  while (1)
  {
    clientlen = sizeof(clientaddr); // clientlen 에 적합한 크기 설정
    connfdp = malloc(sizeof(int));  // connfdp에 공간을 만들어줌 왜? accept 된 이후에 정보를 넣어주려고 포인터가 아닐때는 필요없었지만 포인터라서 필요한 부분
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    Pthread_create(&tid, NULL, thread, connfdp); // Peer thread의 생성 - 인자설명 : 1. thread id 넣고, 2. attribute = NULL, 3. 함수를 호출, 4. connection 식별자
    // pthread_create 를 호출할 때 연결 식별자를 전달하는 방법은 식별자를 가리키는 포인터를 같이 보내는 것--> 그래서 connfdp를 포인터로 선언
    // doit(connfd);
    // Close(connfd);
  }
  return 0;
}

void cache_init()
{ // 캐시 리스트안에 있는 캐시 10개를 전부 초기화해줌
  
  for (int i = 0; i < 10; i++)
  {
    cache.cachelist[i].LRU = 0;
    cache.cachelist[i].readCNT = 0;
    cache.cachelist[i].Empty_check = 0;
    cache.cachelist[i].cache_size = 0;
    Sem_init(&cache.cachelist[i].writing_protect, 0, 1);
    // 인자 설명. 1번인자 : 세마포어 구족체, 2번 인자(pshared) : 0이면 프로세스의 스레드간의 공유, 1이면 프로세스 간의 공유, 3번인자 : 초기화될 때 세마포어가 갖는 값
    Sem_init(&cache.cachelist[i].rdcnt_protect, 0, 1);
  }
}

// cache에서 찾기위한 함수
int cache_find(char *url)
{
  int i;
  for (i = 0; i < 10; i++)
  {
    
    pre_read(i);     // 캐시 읽을때 locking 걸고
    
    if ((cache.cachelist[i].Empty_check == 1) && (strcmp(url, cache.cachelist[i].cache_url)) == 0) break; // 캐시 읽고
      
    after_read(i); // 다읽었으면 locking 풀고
    
  }
  if (i >= 10)    return -1;
  return i;
}

void pre_read(int i)
{  
  P(&cache.cachelist[i].rdcnt_protect); // 세마포어 P함수를 사용하여 reader의 수 제어를 보호하려고함 (얘 사용하면 -1 하고 종료)
  cache.cachelist[i].readCNT=cache.cachelist[i].readCNT+1;   // cache에 접근한 수를 올려가면서 하나 올림  
  if (cache.cachelist[i].readCNT == 1)
    P(&cache.cachelist[i].writing_protect);
       // 만약 얘가 첫번째 reader이면
     // writing을 보호해야하니깐 lock을 걸어줌 (뮤텍스 0은 unlocking이고, 1은 locking임)

  V(&cache.cachelist[i].rdcnt_protect); // 정확히 1개의 쓰레드를 실행하고 rdcnt_protect 1을 올림
}

void after_read(int i)
{
  P(&cache.cachelist[i].rdcnt_protect);
  cache.cachelist[i].readCNT = cache.cachelist[i].readCNT-1;
  if (cache.cachelist[i].readCNT == 0) // 마지막 reader이면 이제 locking을 unlocking으로 변환
    V(&cache.cachelist[i].writing_protect);

  V(&cache.cachelist[i].rdcnt_protect);
}

void cache_LRU(int min_idx) // LRU 방식을 사용하기 위해서 캐시를 추가할때마다 LRU를 -1씩 함 --> 가장 오래된거 찾기 가능
{
  int num;
  for (num = 0; num < min_idx; num++)
  {
    P(&cache.cachelist[num].writing_protect);
    if (cache.cachelist[num].Empty_check == 1 && num != min_idx)
      cache.cachelist[num].LRU--;
    V(&cache.cachelist[num].writing_protect);
  }
  num++;
  for (num; num < 10; num++)
  {
    P(&cache.cachelist[num].writing_protect);
    if (cache.cachelist[num].Empty_check == 1)
      cache.cachelist[num].LRU--;
    V(&cache.cachelist[num].writing_protect);
  }
}

void cache_uri(char *uri, char *cache_buf, int size_buf)
{
  int min = 9999;  // LRU와 값을 비교하기위한 최대 값 설정
  int min_idx = 0; // LRU가 제일 작은 값 혹은 비어있는 공간을 찾으면 넣을 value
  int num = 0;     // LRU 감소를 위한 index

  for (int i = 0; i < 10; i++)
  {
    pre_read(i); // locking
    if (cache.cachelist[i].Empty_check == 0)
    { // 빈공간이 있으면 그 i를 idx로 저장
      min_idx = i;
      after_read(i); // unlocking
      break;
    }
    else if (cache.cachelist[i].LRU < min)
    { // 위에 안걸리면 LRU가 최소인 애 min보다 작은 애를 찾아서 갈아끼워야지
      min_idx = i;
    }
    after_read(i); // 위 두가지에 다 안걸리는 경우도 있음 그리고 unlocking 하는곳
  }

  P(&cache.cachelist[min_idx].writing_protect);
  
  // 캐시 정보를 저장하는 구간
  strcpy(cache.cachelist[min_idx].cache_object, cache_buf);
  strcpy(cache.cachelist[min_idx].cache_url, uri);
  cache.cachelist[min_idx].Empty_check = 1;
  cache.cachelist[min_idx].LRU = 9999;
  cache.cachelist[min_idx].cache_size = size_buf;
  cache_LRU(min_idx); // LRU 적용을 위해서 호출

  V(&cache.cachelist[min_idx].writing_protect);
}