/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#define HTTP_VERSION "HTTP/1.1"

typedef enum { GET, HEAD } method_t;

void doit(int fd);
void read_requesthdrs(rio_t *);
int parse_uri(char *, char *, char *);
void get_filetype(char *, char *);
void serve_static(int, char *, int, method_t, char *);
void serve_dynamic(int, char *, char *, method_t, char *);
void clienterror(int, char *, char *, char *, char *);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd) {
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  int is_static, is_get;
  rio_t rp;
  method_t mcode;

  // 요청 라인을 읽고 분석
  Rio_readinitb(&rp, fd);
  Rio_readlineb(&rp, buf, MAXLINE);
  // buf에 저장된 내용을 포맷에 맞게 읽어온다
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("%s %s %s\n", method, uri, version);
  if (!strcasecmp(method, "GET")) {
    mcode = GET;
  } else if (!strcasecmp(method, "HEAD")) {
    mcode = HEAD;
  } else {
    clienterror(fd, method, "501", "Not implemented",
                "Tiny does not implement this method.");
    return;
  }
  read_requesthdrs(&rp);

  /* parse uri */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "403", "Forbidden",
                "Tiny couldn't find the file.");
    return;
  }

  if (is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file.");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, mcode, version);

  } else {
    // 동적파일 처리
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the program.");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, mcode, version);
  }
}

/* static file */
void serve_static(int fd, char *filename, int filesize, method_t method,
                  char *version) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "%s 200 OK\r\n", version);
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("response headers: \n");
  printf("%s", buf);

  if (method == GET) {
    /* Send response body to client */
    char *tmpbuf = (char *)malloc(filesize);
    srcfd = Open(filename, O_RDONLY, 0);
    Rio_readn(srcfd, tmpbuf, filesize);
    Rio_writen(fd, tmpbuf, filesize);
    Close(srcfd);
    free(tmpbuf);
  }
}

/* Handle Dynamic Content */
void serve_dynamic(int fd, char *filename, char *cgiargs, method_t method,
                   char *version) {
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Response Header */
  sprintf(buf, "%s 200 OK\r\n", version);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) {
    setenv("QUERY_STRING", cgiargs, 1); /* set env vars */
    if (method == HEAD)
      setenv("REQUEST_METHOD", "HEAD", 1); /* set method head */
    else
      setenv("REQUEST_METHOD", "GET", 1); /* set method get */

    Dup2(fd, STDOUT_FILENO);              /* ??? */
    Execve(filename, emptylist, environ); /* run cgi program */
  }
  Wait(NULL);
}

/* read request headers and ignore */
void read_requesthdrs(rio_t *rp) {
  char tmp[MAXLINE];

  while (strcmp(tmp, "\r\n")) { /* if sth is read */
    Rio_readlineb(rp, tmp, MAXLINE);
    printf("%s", tmp);
  }
  return;
}

/* get file type for Content-Type header */
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpg");
  } else if (strstr(filename, ".jpeg")) {
    strcpy(filetype, "image/jpeg");
  } else if (strstr(filename, ".ico")) {
    strcpy(filetype, "image/x-icon");
  } else if (strstr(filename, ".mp4")) {
    strcpy(filetype, "video/mp4");
  } else {
    strcpy(filetype, "text/plain");
  }
  return;
}

/* Send error message to client */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg) {
  char buf[MAXLINE], body[MAXBUF]; /* ? difference ? */

  /* Build the HTTP Response Body */
  sprintf(body, "<html><title>Tiny Error</title><html>");
  sprintf(body, "%s<body><div>%s %s %s</div>", body, cause, errnum, shortmsg);
  sprintf(body, "%s<div>%s</div></body>", body, longmsg);

  /* Build the HTTP Response Header */
  sprintf(buf, "%s %s %s\r\n", HTTP_VERSION, errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-Type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-Length: %ld\r\n\r\n", strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
  return;
}

/* parse uri and return if file is static */
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { /* Static content */
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/') strcat(filename, "home.html");
    return 1;
  }

  else { /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    } else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);

    return 0;
  }
}