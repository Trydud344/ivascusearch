#include "HttpClient.h"
#include "../Proxy/Proxy.h"
#include <stdlib.h>
#include <string.h>

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  HttpResponse *mem = (HttpResponse *)userp;

  if (mem->size + realsize + 1 > mem->capacity) {
    size_t new_cap = mem->capacity == 0 ? 16384 : mem->capacity * 2;
    while (new_cap < mem->size + realsize + 1)
      new_cap *= 2;

    char *ptr = realloc(mem->memory, new_cap);
    if (!ptr) {
      return 0;
    }
    mem->memory = ptr;
    mem->capacity = new_cap;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

HttpResponse http_get(const char *url, const char *user_agent) {
  HttpResponse resp = {.memory = NULL, .size = 0, .capacity = 0};

  if (!url) {
    return resp;
  }

  resp.memory = malloc(16384);
  if (!resp.memory) {
    return resp;
  }
  resp.capacity = 16384;

  CURL *curl = curl_easy_init();
  if (!curl) {
    free(resp.memory);
    resp.memory = NULL;
    return resp;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,
                   user_agent ? user_agent : "libcurl-agent/1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  apply_proxy_settings(curl);

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    free(resp.memory);
    resp.memory = NULL;
    resp.size = 0;
    resp.capacity = 0;
  }

  return resp;
}

void http_response_free(HttpResponse *resp) {
  if (!resp) {
    return;
  }
  free(resp->memory);
  resp->memory = NULL;
  resp->size = 0;
  resp->capacity = 0;
}
