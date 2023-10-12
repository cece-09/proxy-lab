#include <stdio.h>

#include "cache.c"
#include "csapp.h"

sem_t sem;

/* function declaration */
void doit(int);
void *thread(void *);
void handle_client_headers(rio_t *, int, char *, char *);
void get_numeric_addr(char *, char *, u_int32_t *, u_int16_t *);
char *strcasestr(const char *, const char *);
int parse_uri(char *, char *, char *, char *);
int handle_response(rio_t *, int, char *, ssize_t *);

/* global variable for user-agent header */
static const char *user_agent_hdr =
    "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3";

/*==============================*/
int main(int argc, char **argv) {
  int listenfd, *connfdp;
  struct sockaddr_storage client;
  char host[MAXLINE], port[MAXLINE];
  socklen_t clientlen = sizeof(client);
  pthread_t tid;

  if (argc != 2) { /* port is argv[1] */
    printf("usage ./proxy <port>\n");
    exit(1);
  }

  /* semaphor */
  Sem_init(&sem, NULL, 1);

  /* init cache list */
  head = (cache_t *)malloc(sizeof(cache_t));
  tail = (cache_t *)malloc(sizeof(cache_t));
  head->next = tail;
  tail->prev = head;

  cache_left = (ssize_t *)malloc(sizeof(ssize_t));
  *cache_left = MAX_CACHE_SIZE;

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    connfdp = (int *)malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&client, &clientlen);
    Pthread_create(&tid, NULL, thread, connfdp);
  }
  return 0;
}

/** Handle a thread
 * @param connfdp         ptr of connfd
 */
void *thread(void *connfdp) {
  int connfd = *((int *)connfdp);
  Pthread_detach(pthread_self());
  free(connfdp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

/** Routine of a single connection
 * @param connfd         connfd - to client
 * @return               void
 */
void doit(int connfd) {
  rio_t rio;        /* for read & write */
  int proxyfd;      /* connect to final server */
  char buf[MAXBUF]; /* tmp buf */

  char method[MAXLINE], uri[MAXLINE], version[MAXLINE]; /* request */
  char host[MAXLINE], serv[MAXLINE], path[MAXLINE];     /* host, port, path */

  u_int32_t ip;         /* numeric ip address */
  u_int16_t port;       /* numeric port number */
  method_t method_code; /* enum type method code */

  /*
   * Host & Serv  CAN  be string name
   * Ip   & Port  MUST be numeric for cache
   *
   * example)
   * host    : www.google.com
   * serv    : http
   * ip      : 111.222.333.444   -> 32bit uint32_t
   * port    : 80                -> 16bit uint16_t
   */

  /* get request */
  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXBUF);
  sscanf(buf, "%s %s %s", method, uri, version);

  /* use HTTP/1.0 */
  strcpy(version, "HTTP/1.0");

  /* get hostname, service, path from uri */
  if (parse_uri(uri, host, serv, path) > 0) {
    printf("* Not a valid request %s %s %s\n", host, serv, path);
    return;
  };

  /* log :) */
  printf("\n* Target: %s %s\n", host, serv);
  printf("* Request: %s %s %s\n", method, path, version);

  /* get numeric ip & port, method_code */
  get_numeric_addr(host, serv, &ip, &port);
  if (!strcasecmp(method, "GET"))
    method_code = GET;
  else
    method_code = HEAD;

  /* check if response is cached */
  cache_t *meta = NULL;

  P(&sem);
  meta = read_cache(ip, port, method_code, path);
  V(&sem);
  /* if cached data exist */
  if (meta != NULL) {
    printf("* Use Cached Data: %d (%d byte)\n", meta->ip, meta->reslen);
    printf("%s", meta->response);

    Rio_writen(connfd, meta->response, meta->reslen);
    P(&sem);
    push_cache(meta); /* cache is read, push into top list */
    V(&sem);
    return;
  }

  /* open new socket for proxy - server */
  proxyfd = Open_clientfd(host, serv);
  printf("* No cached data. Visit server %d\n", proxyfd);

  /* send request to server */
  sprintf(buf, "%s %s %s\r\n", method, path, version);
  Rio_writen(proxyfd, buf, strlen(buf));

  /* send all headers left to server */
  handle_client_headers(&rio, proxyfd, host, serv);

  /* read response from server and send to client */
  char *cache_data;
  ssize_t *cache_len;
  cache_data = (char *)malloc(MAX_OBJECT_SIZE);
  cache_len = (ssize_t *)malloc(sizeof(ssize_t));

  Rio_readinitb(&rio, proxyfd);
  int rc = handle_response(&rio, connfd, cache_data, cache_len);
  if (!rc) { /* cache the response */
    P(&sem);
    write_cache(ip, port, method_code, path, cache_data, cache_len);
    V(&sem);
  }
}

/** Read response from server and send it to client
 * @param rp              rio - to server
 * @param fd              fd  - to client
 * @param cache_data      ptr of response
 * @param cache_len       total size of response
 * @return                0 if cacheable response size, else 1
 */
int handle_response(rio_t *rp, int fd, char *cache_data, ssize_t *cache_len) {
  char *p;              /* for spliting */
  char *cursor;         /* cache_res write cursor */
  char buf[MAXBUF];     /* memory buf for read & write */
  int is_cacheable_res; /* whether res_len exceed cache limit */
  ssize_t content_len;  /* length of body and full response */

  is_cacheable_res = 0;
  cursor = cache_data;
  *cache_len = 0;

  /* read response header and send to client */
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXBUF);

    /* get content length for readnb */
    if (strcasestr(buf, "content-length")) {
      p = index(buf, ' ');
      sscanf(p + 1, "%ld", &content_len);
    }

    Rio_writen(fd, buf, strlen(buf));
    *cache_len += strlen(buf);

    /* if response length is cacheable size */
    if (*cache_len < MAX_OBJECT_SIZE) {
      memcpy(cursor, buf, strlen(buf));
      cursor += strlen(buf);
    } else {
      is_cacheable_res = 1;
    }
  }

  /* Read response body and send to client */
  char body[content_len];
  Rio_readnb(rp, body, content_len);
  Rio_writen(fd, body, content_len);
  *cache_len += content_len;

  /* if response length is cacheable size */
  if (*cache_len < MAX_OBJECT_SIZE) {
    memcpy(cursor, body, content_len);
    cursor += content_len;
  } else {
    is_cacheable_res = 1;
  }
  return is_cacheable_res;
}

/** Read headers from client and send to server
 * @param rp              ptr of rio - to client
 * @param fd              connect fd - to server
 * @param host            server host
 * @param port            server port
 * @return                void
 */
void handle_client_headers(rio_t *rp, int fd, char *host, char *port) {
  char buf[MAXBUF];

  /* custom headers from proxy */
  sprintf(buf, "Host: %s:%s\r\n", host, port);
  sprintf(buf, "%sUser-Agent: %s\r\n", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n", buf);
  Rio_writen(fd, buf, strlen(buf));

  /* original headers from client */
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXBUF);
    /* ignore some headers */
    if ((strcasestr(buf, "user-agent")) || (strcasestr(buf, "connection")) ||
        (strcasestr(buf, "proxy-connection")) || (strcasestr(buf, "host"))) {
      continue;
    }
    Rio_writen(fd, buf, strlen(buf));
  }
}

/** Get host, port, path from uri
 * @param buf             uri line
 * @param host            host buffer
 * @param port            port buffer
 * @param path            path buffer
 * @return                0 if success, else 1
 */
int parse_uri(char *buf, char *host, char *port, char *path) {
  char tmp[MAXLINE];
  char *p, *q;

  /* ignore request if not started with http:// */
  if (sscanf(buf, "http://%s", tmp) == 0) {
    return 1;
  };

  p = index(tmp, ':');
  if (p) { /* port exist */
    *p = '\0';
    strcpy(host, tmp);
    q = index(p + 1, '/');
    *q = '\0';
    strcpy(port, p + 1);

  } else { /* default port 80  */
    q = index(tmp, '/');
    *q = '\0';
    strcpy(host, tmp);
    strcpy(port, "80");
  }
  strcpy(path, "/");
  if (q) strcat(path, q + 1);
  return 0;
}

/** Find substring ignoring case
 * @param haystack        in haystack,
 * @param needle          find needle
 * @return                ptr of substring
 */
char *strcasestr(const char *haystack, const char *needle) {
  int size = strlen(needle);
  while (*haystack) {
    if (strncasecmp(haystack, needle, size) == 0) {
      return (char *)haystack;
    }
    haystack++;
  }
  return NULL;
}

/** Get numeric address
 * @param host            host string
 * @param serv            serv string
 * @param ip              numeric ip (32bit)
 * @param port            numeric port (16bit)
 * @return                void
 */
void get_numeric_addr(char *host, char *serv, u_int32_t *ip, u_int16_t *port) {
  int flags;
  char dotted[MAXLINE];
  struct addrinfo *listp, *p, hints;

  /* get numeric ip address */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  Getaddrinfo(host, NULL, &hints, &listp);
  flags = NI_NUMERICHOST | NI_NUMERICSERV; /* only dotted string */
  for (p = listp; p; p = p->ai_next) {
    Getnameinfo(p->ai_addr, p->ai_addrlen, dotted, MAXLINE, NULL, 0, flags);
    if (strlen(dotted)) break;
  }

  /* convert dotted to 4-byte ip */
  *ip = ntohl(inet_addr(dotted));

  /* convert port to 2-byte short */
  *port = atoi(serv);

  Freeaddrinfo(listp);
}
