#include "HttpClient.h"
#include "../Cache/Cache.h"
#include "../Proxy/Proxy.h"
#include "Config.h"
#include <stdlib.h>
#include <string.h>

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  HttpResponse *mem = (HttpResponse *)userp;

  if (mem->size + realsize + 1 > mem->capacity) {
    size_t new_cap =
        mem->capacity == 0 ? INITIAL_BUFFER_SIZE : mem->capacity * 2;
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

  resp.memory = malloc(INITIAL_BUFFER_SIZE);
  if (!resp.memory) {
    return resp;
  }
  resp.capacity = INITIAL_BUFFER_SIZE;

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

CachedHttpResponse cached_http_get(const char *url, const char *user_agent,
                                   const char *cache_key, time_t cache_ttl,
                                   XmlParserFn parser) {
  CachedHttpResponse result = {
      .memory = NULL, .size = 0, .parsed_result = NULL, .success = 0};

  if (!url || !parser) {
    return result;
  }

  if (cache_key && cache_ttl > 0) {
    char *cached_data = NULL;
    size_t cached_size = 0;
    if (cache_get(cache_key, cache_ttl, &cached_data, &cached_size) == 0 &&
        cached_data && cached_size > 0) {
      xmlDocPtr doc = parser(cached_data, cached_size, url);
      if (doc) {
        result.parsed_result = doc;
        result.success = 1;
      }
      free(cached_data);
      return result;
    }
    free(cached_data);
  }

  HttpResponse resp = http_get(url, user_agent);
  if (resp.memory && resp.size > 0) {
    if (cache_key && cache_ttl > 0) {
      cache_set(cache_key, resp.memory, resp.size);
    }

    xmlDocPtr doc = parser(resp.memory, resp.size, url);
    if (doc) {
      result.parsed_result = doc;
      result.success = 1;
    }
  }

  result.memory = resp.memory;
  result.size = resp.size;
  return result;
}
