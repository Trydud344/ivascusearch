#ifndef SCRAPING_H
#define SCRAPING_H

#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

typedef struct {
  char *url;
  char *title;
  char *snippet;
} SearchResult;

typedef int (*ParserFunc)(const char *engine_name, xmlDocPtr doc,
                          SearchResult **out_results, int max_results);

typedef struct {
  const char *name;
  const char *base_url;
  const char *host_header;
  const char *referer;

  const char *page_param;
  int page_multiplier;
  int page_base;
  ParserFunc parser;
} SearchEngine;

typedef struct {
  char *memory;
  size_t size;
  size_t capacity;
} MemoryBuffer;

typedef struct {
  const SearchEngine *engine;
  char *query;
  SearchResult **out_results;
  int max_results;
  int page;
  CURL *handle;
  MemoryBuffer response;
  int results_count;
} ScrapeJob;

extern const SearchEngine ENGINE_REGISTRY[];
extern const int ENGINE_COUNT;

size_t write_memory_callback(void *contents, size_t size, size_t nmemb,
                             void *userp);
const char *get_random_user_agent(void);
void configure_curl_handle(CURL *curl, const char *full_url,
                           MemoryBuffer *chunk, struct curl_slist *headers);
char *build_search_url(const char *base_url, const char *page_param,
                       int page_multiplier, int page_base,
                       const char *encoded_query, int page);
struct curl_slist *build_request_headers(const char *host_header,
                                         const char *referer);
void http_delay(void);

xmlXPathContextPtr create_xpath_context(xmlDocPtr doc);
void free_xpath_objects(xmlXPathContextPtr ctx, xmlXPathObjectPtr obj);
SearchResult *alloc_results_array(int capacity, int max_results);
void assign_result(SearchResult *result, char *url, char *title, char *snippet,
                   int unescape);
void free_xml_node_list(char *title, char *url, char *snippet);

int scrape_engine(const SearchEngine *engine, const char *query,
                  SearchResult **out_results, int max_results);

int scrape_engines_parallel(ScrapeJob *jobs, int num_jobs);

#endif
