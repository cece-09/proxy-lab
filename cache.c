#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* method type */
typedef enum { GET, HEAD } method_t;

/* for cache list */
typedef struct _cache_t {
  uint32_t ip;
  uint16_t port;
  method_t method;
  char path[MAXLINE];
  ssize_t reslen;
  char *response;
  struct _cache_t *prev;
  struct _cache_t *next;
} cache_t;

/* global variable for cache list */
static cache_t *head, *tail;
static ssize_t *cache_left;

/* cache functions */
cache_t *read_cache(u_int32_t, u_int16_t, method_t, char *);
void write_cache(u_int32_t, u_int16_t, method_t, char *, char *, ssize_t *);
void pop_cache(cache_t *);
void push_cache(cache_t *);

/** Write cache and push into top
 * @param ip              numeric ip
 * @param port            numeric port
 * @param method          enum method
 * @param path            path string from uri
 * @param cache_data      ptr of response
 * @param cache_len       total length of response
 * @return                void
 */
void write_cache(u_int32_t ip, u_int16_t port, method_t method, char *path,
                 char *cache_data, ssize_t *res_len) {
  cache_t *meta; /* cache block */
  cache_t *lru;  /* least recently used */
  ssize_t length = *res_len;
  free(res_len);

  printf("* Write cache.. %d\n", *cache_left);
  /* if cache memory is full */
  lru = tail->prev;
  while (length >= (*cache_left) && lru != head) {
    printf("\tCache is full. cleaning..%d %s\n", lru->ip, lru->path);

    /* delete the least recently used and check */
    pop_cache(lru);
    free(lru->response);
    (*cache_left) += lru->reslen;
    lru = lru->prev;

    if (lru == head) {
      printf("\tCache overflow\n");
      return;
    }
    free(lru->next);
  }

  /* make metadata */
  meta = (cache_t *)malloc(sizeof(cache_t));
  meta->ip = ip;
  meta->port = port;
  meta->method = method;
  strcpy(meta->path, path);

  /* save data */
  char *real_size = (char *)malloc(length);
  /* reallocate with real size (not MAX_OBJ_SIZE) */
  memcpy(real_size, cache_data, length);
  free(cache_data);
  meta->reslen = length;
  meta->response = real_size;
  (*cache_left) -= length;
  printf("* Write cache.. fin %d %d\n", length, *cache_left);

  /* push */
  push_cache(meta);
}

/** Read cached response from list
 * @param ip              search cache based on numeric ip,
 * @param port            numeric port,
 * @param method          request method,
 * @param path            and path
 * @return                cache_t* ptr if found, else NULL
 */
cache_t *read_cache(u_int32_t ip, u_int16_t port, method_t method, char *path) {
  cache_t *curr;
  int tmp;

  curr = head;
  while (curr != tail) {
    printf("* Search Cache: %d %d %s\n", curr->ip, curr->port, curr->path);
    /* if same request */
    if (curr->ip == ip && curr->port == port && curr->method == method &&
        !strcmp(curr->path, path)) {
      printf("* Cache found %s\n", curr->path);
      pop_cache(curr); /* delete from list */
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

/** Remove cache block from list
 * @param curr            cache block to remove
 * @return                void
 */
void pop_cache(cache_t *curr) {
  curr->prev->next = curr->next;
  curr->next->prev = curr->prev;
}

/** Push cache block into list
 * @param curr            cache block to push top
 * @return                void
 */
void push_cache(cache_t *curr) {
  curr->next = head->next;
  head->next->prev = curr;
  curr->prev = head;
  head->next = curr;
}