#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <curl/curl.h>
#include <libxml/parser.h>
#include <stddef.h>
#include <time.h>

typedef struct {
  char *memory;
  size_t size;
  size_t capacity;
} HttpResponse;

HttpResponse http_get(const char *url, const char *user_agent);
void http_response_free(HttpResponse *resp);

typedef xmlDocPtr (*XmlParserFn)(const char *data, size_t size,
                                 const char *url);

typedef struct {
  char *memory;
  size_t size;
  void *parsed_result;
  int success;
} CachedHttpResponse;

CachedHttpResponse cached_http_get(const char *url, const char *user_agent,
                                   const char *cache_key, time_t cache_ttl,
                                   XmlParserFn parser);

#endif
