#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <curl/curl.h>
#include <stddef.h>

typedef struct {
  char *memory;
  size_t size;
  size_t capacity;
} HttpResponse;

HttpResponse http_get(const char *url, const char *user_agent);
void http_response_free(HttpResponse *resp);

#endif
