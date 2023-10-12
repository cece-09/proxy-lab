/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  if ((buf = getenv("QUERY_STRING")) != NULL) {
    sscanf(buf, "n1=%d&n2=%d", &n1, &n2);
  }

  method = getenv("REQUEST_METHOD");
  if (!strcmp(method, "GET")) {
    /* Make the response body */

    sprintf(content, "<body><p>Welcome to add.com: ");
    sprintf(content, "%sThe Internet addition portal.\r\n</p>", content);
    sprintf(content, "%s<form action='/cgi-bin/adder' method='get'>\r\n",
            content);
    sprintf(content, "%s<input type='text' name='n1' value='%d'/>\r\n", content,
            n1);
    sprintf(content, "%s<input type='text' name='n2' value='%d'/>\r\n", content,
            n2);
    sprintf(content, "%s<input type='submit' value='submit'/></form>", content);

    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2,
            (n1 + n2));
    sprintf(content, "%s<p><a href='/'>home</a></p></body>", content);
  }

  /* HTTP response */
  printf("Connection: close\r\n");
  printf("Content-Length: %d\r\n", (int)strlen(content));
  printf("Content-Type: text/html\r\n\r\n");
  if (!strcmp(method, "GET")) {
    printf("%s", content);
  }
  fflush(stdout);

  exit(0);
}
/* $end adder */
