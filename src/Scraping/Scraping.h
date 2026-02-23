#ifndef SCRAPING_H
#define SCRAPING_H

#include <libxml/HTMLparser.h>
#include <curl/curl.h>

#define LOG_INFO(msg, ...) fprintf(stderr, "[INFO] " msg "\n", ##__VA_ARGS__)
#define LOG_WARN(msg, ...) fprintf(stderr, "[WARN] " msg "\n", ##__VA_ARGS__)
#define LOG_DEBUG(msg, ...) fprintf(stderr, "[DEBUG] " msg "\n", ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) fprintf(stderr, "[ERROR] " msg "\n", ##__VA_ARGS__)

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
  int         page_multiplier;
  int         page_base;
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

int scrape_engine(const SearchEngine *engine, const char *query,
                  SearchResult **out_results, int max_results);

int scrape_engines_parallel(ScrapeJob *jobs, int num_jobs);

#endif