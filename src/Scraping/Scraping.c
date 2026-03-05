#include "Scraping.h"
#include "../Proxy/Proxy.h"
#include "../Utility/Unescape.h"
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                  void *userp) {
  size_t realsize = size * nmemb;
  MemoryBuffer *mem = (MemoryBuffer *)userp;

  if (mem->size + realsize + 1 > mem->capacity) {

  size_t new_cap = mem->capacity == 0 ? 16384 : mem->capacity * 2;
  while (new_cap < mem->size + realsize + 1) new_cap *= 2;

  char *ptr = (char *)realloc(mem->memory, new_cap);
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

static const char *get_random_user_agent() {
  static const char *agents[] = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
    "like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like "
    "Gecko) "
    "Chrome/120.0.0.0` Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 "
    "Firefox/121.0",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
    "(KHTML, like Gecko) Version/17.2 Safari/605.1.15"};
  return agents[rand() % 5];
}

static int parse_ddg_lite(const char *engine_name, xmlDocPtr doc,
              SearchResult **out_results, int max_results) {
  (void)engine_name;
  int found_count = 0;
  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
  if (!xpathCtx) {
  return 0;
  }

  const char *link_xpath = "//tr[not(contains(@class, 'result-sponsored'))]//a[@class='result-link']";
  xmlXPathObjectPtr xpathObj =
    xmlXPathEvalExpression((xmlChar *)link_xpath, xpathCtx);

  if (!xpathObj || !xpathObj->nodesetval || xpathObj->nodesetval->nodeNr == 0) {
  if (xpathObj) xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return 0;
  }

  int num_links = xpathObj->nodesetval->nodeNr;

  int actual_alloc = (num_links < max_results) ? num_links : max_results;
  *out_results = (SearchResult *)calloc(actual_alloc, sizeof(SearchResult));
  if (!*out_results) {
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return 0;
  }

  for (int i = 0; i < num_links && found_count < max_results; i++) {
  xmlNodePtr linkNode = xpathObj->nodesetval->nodeTab[i];
  char *title = (char *)xmlNodeGetContent(linkNode);
  char *url = (char *)xmlGetProp(linkNode, (xmlChar *)"href");
  char *snippet_text = NULL;

  xmlNodePtr current = linkNode->parent;
  while (current && xmlStrcasecmp(current->name, (const xmlChar *)"tr") != 0)
    current = current->parent;

  if (current && current->next) {
    xmlNodePtr snippetRow = current->next;
    while (snippetRow &&
       xmlStrcasecmp(snippetRow->name, (const xmlChar *)"tr") != 0)
    snippetRow = snippetRow->next;
    if (snippetRow) {

    xpathCtx->node = snippetRow;
    xmlXPathObjectPtr sObj = xmlXPathEvalExpression(
      (xmlChar *)".//td[@class='result-snippet']", xpathCtx);
    if (sObj && sObj->nodesetval && sObj->nodesetval->nodeNr > 0) {
      snippet_text = (char *)xmlNodeGetContent(sObj->nodesetval->nodeTab[0]);
    }
    if (sObj) xmlXPathFreeObject(sObj);
    xpathCtx->node = NULL; 

    }
  }

  (*out_results)[found_count].url = unescape_search_url(url);
  (*out_results)[found_count].title = strdup(title ? title : "No Title");
  (*out_results)[found_count].snippet = strdup(snippet_text ? snippet_text : "");

  found_count++;

  if (title) xmlFree(title);
  if (url) xmlFree(url);
  if (snippet_text) xmlFree(snippet_text);
  }

  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return found_count;
}

static int parse_startpage(const char *engine_name, xmlDocPtr doc,
               SearchResult **out_results, int max_results) {
  (void)engine_name;
  int found_count = 0;
  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
  if (!xpathCtx) {
  return 0;
  }

  const char *container_xpath = "//div[contains(@class, 'result')]";
  xmlXPathObjectPtr xpathObj =
    xmlXPathEvalExpression((xmlChar *)container_xpath, xpathCtx);

  if (!xpathObj || !xpathObj->nodesetval || xpathObj->nodesetval->nodeNr == 0) {
  if (xpathObj) xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return 0;
  }

  int num_results = xpathObj->nodesetval->nodeNr;

  int actual_alloc = (num_results < max_results) ? num_results : max_results;
  *out_results = (SearchResult *)calloc(actual_alloc, sizeof(SearchResult));
  if (!*out_results) {
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return 0;
  }

  for (int i = 0; i < num_results && found_count < max_results; i++) {
  xmlNodePtr resultNode = xpathObj->nodesetval->nodeTab[i];
  xpathCtx->node = resultNode;

  xmlXPathObjectPtr linkObj = xmlXPathEvalExpression(
    (xmlChar *)".//a[contains(@class, 'result-link')]", xpathCtx);
  char *url =
    (linkObj && linkObj->nodesetval && linkObj->nodesetval->nodeNr > 0)
      ? (char *)xmlGetProp(linkObj->nodesetval->nodeTab[0],
                 (xmlChar *)"href")
      : NULL;

  xmlXPathObjectPtr titleObj = xmlXPathEvalExpression(
    (xmlChar *)".//h2[contains(@class, 'wgl-title')]", xpathCtx);
  char *title =
    (titleObj && titleObj->nodesetval && titleObj->nodesetval->nodeNr > 0)
      ? (char *)xmlNodeGetContent(titleObj->nodesetval->nodeTab[0])
      : NULL;

  xmlXPathObjectPtr snippetObj = xmlXPathEvalExpression(
    (xmlChar *)".//p[contains(@class, 'description')]", xpathCtx);
  char *snippet_text =
    (snippetObj && snippetObj->nodesetval &&
     snippetObj->nodesetval->nodeNr > 0)
      ? (char *)xmlNodeGetContent(snippetObj->nodesetval->nodeTab[0])
      : NULL;

  if (url && title) {
    (*out_results)[found_count].url = strdup(url);
    (*out_results)[found_count].title = strdup(title);
    (*out_results)[found_count].snippet =
      strdup(snippet_text ? snippet_text : "");
    found_count++;
  }

  if (title) xmlFree(title);
  if (url) xmlFree(url);
  if (snippet_text) xmlFree(snippet_text);
  if (linkObj) xmlXPathFreeObject(linkObj);
  if (titleObj) xmlXPathFreeObject(titleObj);
  if (snippetObj) xmlXPathFreeObject(snippetObj);
  }

  xpathCtx->node = NULL; 

  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return found_count;
}

static int parse_yahoo(const char *engine_name, xmlDocPtr doc,
             SearchResult **out_results, int max_results) {
  (void)engine_name;
  int found_count = 0;
  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
  if (!xpathCtx) {
  return 0;
  }

  const char *container_xpath = "//div[contains(@class, 'algo-sr')]";
  xmlXPathObjectPtr xpathObj =
    xmlXPathEvalExpression((xmlChar *)container_xpath, xpathCtx);

  if (!xpathObj || !xpathObj->nodesetval || xpathObj->nodesetval->nodeNr == 0) {
  if (xpathObj) xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return 0;
  }

  int num_results = xpathObj->nodesetval->nodeNr;

  int actual_alloc = (num_results < max_results) ? num_results : max_results;
  *out_results = (SearchResult *)calloc(actual_alloc, sizeof(SearchResult));
  if (!*out_results) {
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return 0;
  }

  for (int i = 0; i < num_results && found_count < max_results; i++) {
  xmlNodePtr resultNode = xpathObj->nodesetval->nodeTab[i];
  xpathCtx->node = resultNode;

  xmlXPathObjectPtr linkObj = xmlXPathEvalExpression(
    (xmlChar *)".//div[contains(@class, 'compTitle')]//a[@target='_blank']",
    xpathCtx);
  char *url =
    (linkObj && linkObj->nodesetval && linkObj->nodesetval->nodeNr > 0)
      ? (char *)xmlGetProp(linkObj->nodesetval->nodeTab[0],
                 (xmlChar *)"href")
      : NULL;

  xmlXPathObjectPtr titleObj = xmlXPathEvalExpression(
    (xmlChar *)".//h3[contains(@class, 'title')]", xpathCtx);
  char *title =
    (titleObj && titleObj->nodesetval && titleObj->nodesetval->nodeNr > 0)
      ? (char *)xmlNodeGetContent(titleObj->nodesetval->nodeTab[0])
      : NULL;

  xmlXPathObjectPtr snippetObj = xmlXPathEvalExpression(
    (xmlChar *)".//div[contains(@class, 'compText')]//p", xpathCtx);
  char *snippet_text =
    (snippetObj && snippetObj->nodesetval &&
     snippetObj->nodesetval->nodeNr > 0)
      ? (char *)xmlNodeGetContent(snippetObj->nodesetval->nodeTab[0])
      : NULL;

  if (url && title) {
    (*out_results)[found_count].url = unescape_search_url(url);
    (*out_results)[found_count].title = strdup(title);
    (*out_results)[found_count].snippet =
      strdup(snippet_text ? snippet_text : "");
    found_count++;
  }

  if (title) xmlFree(title);
  if (url) xmlFree(url);
  if (snippet_text) xmlFree(snippet_text);
  if (linkObj) xmlXPathFreeObject(linkObj);
  if (titleObj) xmlXPathFreeObject(titleObj);
  if (snippetObj) xmlXPathFreeObject(snippetObj);
  }

  xpathCtx->node = NULL;
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return found_count;
}

const SearchEngine ENGINE_REGISTRY[] = {
  {.name = "DuckDuckGo Lite",
   .base_url = "https://lite.duckduckgo.com/lite/?q=",
   .host_header = "lite.duckduckgo.com",
   .referer = "https://lite.duckduckgo.com/",
   .page_param = "s",
   .page_multiplier = 30,
   .page_base = 0,
   .parser = parse_ddg_lite},
  {.name = "Startpage",
   .base_url = "https://www.startpage.com/sp/search?q=",
   .host_header = "www.startpage.com",
   .referer = "https://www.startpage.com/",
   .page_param = "page",
   .page_multiplier = 1,
   .page_base = 1,   
   .parser = parse_startpage},
  {.name = "Yahoo",
   .base_url = "https://search.yahoo.com/search?p=",
   .host_header = "search.yahoo.com",
   .referer = "https://search.yahoo.com/",
   .page_param = "b",
   .page_multiplier = 10,
   .page_base = 1,   
   .parser = parse_yahoo}};

const int ENGINE_COUNT = sizeof(ENGINE_REGISTRY) / sizeof(SearchEngine);

static void configure_curl_handle(CURL *curl, const char *full_url, 
                   MemoryBuffer *chunk,
                   struct curl_slist *headers) {
  curl_easy_setopt(curl, CURLOPT_URL, full_url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, get_random_user_agent());

  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);

  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

  curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 300L);

  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

  apply_proxy_settings(curl);
}

int scrape_engines_parallel(ScrapeJob *jobs, int num_jobs) {
  int retries = 0;

retry:
  CURLM *multi_handle = curl_multi_init();
  if (!multi_handle) {
  return -1;
  }

  for (int i = 0; i < num_jobs; i++) {
  ScrapeJob *job = &jobs[i];

  if (job->handle) {
    curl_easy_cleanup(job->handle);
    job->handle = NULL;
  }
  if (job->response.memory) {
    free(job->response.memory);
  }

  job->handle = curl_easy_init();
  if (!job->handle) {
    continue;
  }

  job->response.memory = (char *)malloc(16384);
  job->response.size = 0;
  job->response.capacity = 16384;

  char full_url[1024];
  char *encoded_query = curl_easy_escape(job->handle, job->query, 0);
  if (!encoded_query) {
    curl_easy_cleanup(job->handle);
    job->handle = NULL;
    continue;
  }

  int page = (job->page < 1) ? 1 : job->page;
  int page_value = (page - 1) * job->engine->page_multiplier + job->engine->page_base;

  snprintf(full_url, sizeof(full_url), "%s%s&%s=%d",
       job->engine->base_url,
       encoded_query,
       job->engine->page_param,
       page_value);
  curl_free(encoded_query);

  struct curl_slist *headers = NULL;
  char host_buf[256], ref_buf[256];
  snprintf(host_buf, sizeof(host_buf), "Host: %s", job->engine->host_header);
  snprintf(ref_buf, sizeof(ref_buf), "Referer: %s", job->engine->referer);
  headers = curl_slist_append(headers, host_buf);
  headers = curl_slist_append(headers, ref_buf);
  headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
  headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.5");
  headers = curl_slist_append(headers, "DNT: 1");

  configure_curl_handle(job->handle, full_url, &job->response, headers);

  curl_easy_setopt(job->handle, CURLOPT_PRIVATE, headers);

  curl_multi_add_handle(multi_handle, job->handle);
  }

  usleep(100000 + (rand() % 100000)); 

  int still_running = 0;
  curl_multi_perform(multi_handle, &still_running);

  do {
  int numfds = 0;
  CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);

  if (mc != CURLM_OK) {
    break;
  }

  curl_multi_perform(multi_handle, &still_running);
  } while (still_running);

  CURLMsg *msg;
  int msgs_left;
  while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
  if (msg->msg == CURLMSG_DONE) {
    CURL *handle = msg->easy_handle;

    for (int i = 0; i < num_jobs; i++) {
    if (jobs[i].handle && jobs[i].handle == handle) {
      ScrapeJob *job = &jobs[i];

      long response_code;
      curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);

      if (msg->data.result == CURLE_OK && job->response.size > 0) {
      xmlDocPtr doc = htmlReadMemory(
        job->response.memory, job->response.size, NULL, NULL,
        HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);

      if (doc) {
        job->results_count = job->engine->parser(
          job->engine->name, doc, job->out_results, job->max_results);
        xmlFreeDoc(doc);
      }
      } else {
      job->results_count = 0;
      }

      struct curl_slist *headers;
      curl_easy_getinfo(handle, CURLINFO_PRIVATE, &headers);
      if (headers) curl_slist_free_all(headers);

      free(job->response.memory);
      job->response.memory = NULL;
      curl_multi_remove_handle(multi_handle, handle);
      if (handle) curl_easy_cleanup(handle);
      job->handle = NULL;
      break;
    }
    }
  }
  }

  curl_multi_cleanup(multi_handle);

  if (retries < max_proxy_retries && proxy_count > 0) {
  int any_failed = 0;
  for (int i = 0; i < num_jobs; i++) {
    if (jobs[i].results_count == 0 && jobs[i].response.size == 0) {
    any_failed = 1;
    break;
    }
  }
  if (any_failed) {
    retries++;
    goto retry;
  }
  }

  return 0;
}

int scrape_engine(const SearchEngine *engine, const char *query,
          SearchResult **out_results, int max_results) {
  ScrapeJob job = {
    .engine = engine,
    .query = (char *)query,
    .out_results = out_results,
    .max_results = max_results,
    .results_count = 0,
    .page = 1
  };

  scrape_engines_parallel(&job, 1);
  return job.results_count;
}