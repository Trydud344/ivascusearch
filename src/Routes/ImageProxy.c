#include "ImageProxy.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IMAGE_SIZE (10 * 1024 * 1024)

typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} MemoryBuffer;

static int is_allowed_domain(const char *url) {
  const char *protocol = strstr(url, "://");
  if (!protocol) {
    protocol = url;
  } else {
    protocol += 3;
  }

  const char *path = strchr(protocol, '/');
  size_t host_len = path ? (size_t)(path - protocol) : strlen(protocol);

  char host[256] = {0};
  if (host_len >= sizeof(host)) {
    host_len = sizeof(host) - 1;
  }
  strncpy(host, protocol, host_len);

  const char *allowed_domains[] = {
    "mm.bing.net",
    "th.bing.com",
    NULL
  };

  for (int i = 0; allowed_domains[i] != NULL; i++) {
    size_t domain_len = strlen(allowed_domains[i]);
    size_t host_str_len = strlen(host);

    if (host_str_len >= domain_len) {
      const char *suffix = host + host_str_len - domain_len;
      if (strcmp(suffix, allowed_domains[i]) == 0) {
        return 1;
      }
    }
  }

  return 0;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  MemoryBuffer *buf = (MemoryBuffer *)userp;

  if (buf->size + realsize > MAX_IMAGE_SIZE) {
    return 0;
  }

  if (buf->size + realsize > buf->capacity) {
    size_t new_capacity = buf->capacity * 2;
    if (new_capacity < buf->size + realsize) {
      new_capacity = buf->size + realsize;
    }
    char *new_data = realloc(buf->data, new_capacity);
    if (!new_data) return 0;
    buf->data = new_data;
    buf->capacity = new_capacity;
  }

  memcpy(buf->data + buf->size, contents, realsize);
  buf->size += realsize;
  return realsize;
}

int image_proxy_handler(UrlParams *params) {
  const char *url = NULL;
  for (int i = 0; i < params->count; i++) {
    if (strcmp(params->params[i].key, "url") == 0) {
      url = params->params[i].value;
      break;
    }
  }

  if (!url || strlen(url) == 0) {
    send_response("Missing 'url' parameter");
    return 0;
  }

  if (!is_allowed_domain(url)) {
    send_response("Domain not allowed");
    return 0;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    send_response("Failed to initialize curl");
    return 0;
  }

  MemoryBuffer buf = {
    .data = malloc(8192),
    .size = 0,
    .capacity = 8192
  };

  if (!buf.data) {
    curl_easy_cleanup(curl);
    send_response("Memory allocation failed");
    return 0;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

  CURLcode res = curl_easy_perform(curl);

  long response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  char *content_type_ptr = NULL;
  curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type_ptr);

  char content_type[64] = {0};
  if (content_type_ptr) {
    strncpy(content_type, content_type_ptr, sizeof(content_type) - 1);
  }

  curl_easy_cleanup(curl);

  if (res != CURLE_OK || response_code != 200) {
    free(buf.data);
    send_response("Failed to fetch image");
    return 0;
  }

  const char *mime_type = strlen(content_type) > 0 ? content_type : "image/jpeg";
  serve_data(buf.data, buf.size, mime_type);

  free(buf.data);
  return 0;
}
