#include "Scraping.h"
#include "../Cache/Cache.h"
#include "../Proxy/Proxy.h"
#include "Config.h"
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int response_contains(const char *response, const char *needle) {
  return response && needle && strstr(response, needle) != NULL;
}

static int is_startpage_job(const ScrapeJob *job) {
  return job && job->engine && strcmp(job->engine->name, "Startpage") == 0;
}

static int response_is_startpage_captcha(const ScrapeJob *job,
                                         const char *response) {
  if (!is_startpage_job(job))
    return 0;

  return response_contains(response, "<title>Startpage Captcha</title>") ||
         response_contains(response, "Startpage Captcha") ||
         response_contains(response, "/static-pages-assets/page-data/captcha/");
}

static int response_looks_like_results_page(const ScrapeJob *job,
                                            const char *response) {
  if (!job || !job->engine || !response)
    return 0;

  if (strcmp(job->engine->name, "DuckDuckGo Lite") == 0) {
    return response_contains(response, "result-link") ||
           response_contains(response, "result-snippet");
  }

  if (strcmp(job->engine->name, "Startpage") == 0) {
    return response_contains(response, "<title>Startpage Search Results</title>") ||
           response_contains(response, "class=\"w-gl") ||
           response_contains(response, "data-testid=\"gl-title-link\"");
  }

  if (strcmp(job->engine->name, "Yahoo") == 0) {
    return response_contains(response, "algo-sr") ||
           response_contains(response, "compTitle") ||
           response_contains(response, "compText");
  }

  if (strcmp(job->engine->name, "Mojeek") == 0) {
    return response_contains(response, "class=\"results-standard\"") ||
           response_contains(response, "Mojeek Search");
  }

  return 0;
}

static void classify_job_response(ScrapeJob *job, const char *response,
                                  size_t response_size) {
  job->results_count = 0;

  if (!response || response_size == 0) {
    job->status = SCRAPE_STATUS_FETCH_ERROR;
    return;
  }

  if (response_is_startpage_captcha(job, response)) {
    job->status = SCRAPE_STATUS_BLOCKED;
    return;
  }

  xmlDocPtr doc = htmlReadMemory(response, response_size, NULL, NULL,
                                 HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                     HTML_PARSE_NOWARNING);

  if (!doc) {
    job->status = SCRAPE_STATUS_FETCH_ERROR;
    return;
  }

  job->results_count =
      job->engine->parser(job->engine->name, doc, job->out_results,
                          job->max_results);
  xmlFreeDoc(doc);

  if (job->results_count > 0) {
    job->status = SCRAPE_STATUS_OK;
    return;
  }

  if (job->http_status >= 400) {
    job->status = SCRAPE_STATUS_FETCH_ERROR;
    return;
  }

  if (response_looks_like_results_page(job, response)) {
    job->status = SCRAPE_STATUS_PARSE_MISMATCH;
    return;
  }

  job->status = SCRAPE_STATUS_EMPTY;
}

int check_cache_for_job(ScrapeJob *job) {
  if (get_cache_ttl_search() <= 0)
    return 0;

  char *key = cache_compute_key(job->query, job->page, job->engine->name);
  if (!key)
    return 0;

  char *cached_data = NULL;
  size_t cached_size = 0;

  if (cache_get(key, (time_t)get_cache_ttl_search(), &cached_data,
                &cached_size) == 0 &&
      cached_data && cached_size > 0) {
    classify_job_response(job, cached_data, cached_size);

    if (job->status == SCRAPE_STATUS_BLOCKED) {
      free(cached_data);
      free(key);
      return 0;
    }

    free(cached_data);
    free(key);

    if (job->results_count == 0)
      return 0;

    return 1;
  }

  free(key);
  return 0;
}

void parse_and_cache_response(ScrapeJob *job) {
  if (job->response.size == 0) {
    job->results_count = 0;
    job->status = SCRAPE_STATUS_FETCH_ERROR;
    return;
  }

  classify_job_response(job, job->response.memory, job->response.size);

  if (job->status == SCRAPE_STATUS_OK || job->status == SCRAPE_STATUS_EMPTY) {
    char *key = cache_compute_key(job->query, job->page, job->engine->name);
    if (key && get_cache_ttl_search() > 0)
      cache_set(key, job->response.memory, job->response.size);
    free(key);
  }
}

void cleanup_job_handle(ScrapeJob *job, CURL *handle) {
  struct curl_slist *headers = NULL;
  curl_easy_getinfo(handle, CURLINFO_PRIVATE, &headers);
  if (headers)
    curl_slist_free_all(headers);

  free(job->response.memory);
  job->response.memory = NULL;
}

void process_response(ScrapeJob *job, CURL *handle, CURLMsg *msg) {
  curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &job->http_status);

  if (msg->data.result == CURLE_OK)
    parse_and_cache_response(job);
  else {
    job->results_count = 0;
    job->status = SCRAPE_STATUS_FETCH_ERROR;
  }

  cleanup_job_handle(job, handle);
}

int setup_job(ScrapeJob *job, CURLM *multi_handle) {
  if (job->handle)
    curl_easy_cleanup(job->handle);
  if (job->response.memory)
    free(job->response.memory);

  job->results_count = 0;
  job->http_status = 0;
  job->status = SCRAPE_STATUS_PENDING;

  if (check_cache_for_job(job)) {
    job->results_count = job->results_count > 0 ? job->results_count : 0;
    return 0;
  }

  char *encoded_query = curl_easy_escape(NULL, job->query, 0);
  if (!encoded_query) {
    job->status = SCRAPE_STATUS_FETCH_ERROR;
    return -1;
  }

  char *full_url =
      build_search_url(job->engine->base_url, job->engine->page_param,
                       job->engine->page_multiplier, job->engine->page_base,
                       encoded_query, job->page);
  free(encoded_query);

  if (!full_url) {
    job->status = SCRAPE_STATUS_FETCH_ERROR;
    return -1;
  }

  job->handle = curl_easy_init();
  if (!job->handle) {
    free(full_url);
    job->status = SCRAPE_STATUS_FETCH_ERROR;
    return -1;
  }

  job->response.memory = (char *)malloc(INITIAL_BUFFER_SIZE);
  job->response.size = 0;
  job->response.capacity = INITIAL_BUFFER_SIZE;

  struct curl_slist *headers =
      build_request_headers(job->engine->host_header, job->engine->referer);

  configure_curl_handle(job->handle, full_url, &job->response, headers);
  curl_easy_setopt(job->handle, CURLOPT_PRIVATE, headers);

  free(full_url);
  curl_multi_add_handle(multi_handle, job->handle);
  return 0;
}

int handle_responses(CURLM *multi_handle, ScrapeJob *jobs, int num_jobs) {
  CURLMsg *msg;
  int msgs_left;

  while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
    if (msg->msg != CURLMSG_DONE)
      continue;

    CURL *handle = msg->easy_handle;

    for (int i = 0; i < num_jobs; i++) {
      if (jobs[i].handle && jobs[i].handle == handle) {
        process_response(&jobs[i], handle, msg);
        curl_multi_remove_handle(multi_handle, handle);
        curl_easy_cleanup(handle);
        jobs[i].handle = NULL;
        break;
      }
    }
  }

  return 0;
}

int should_retry(ScrapeJob *jobs, int num_jobs) {
  if (proxy_count <= 0)
    return 0;

  for (int i = 0; i < num_jobs; i++) {
    if (jobs[i].status == SCRAPE_STATUS_FETCH_ERROR ||
        jobs[i].status == SCRAPE_STATUS_BLOCKED)
      return 1;
  }
  return 0;
}

int scrape_engines_parallel(ScrapeJob *jobs, int num_jobs) {
  int retries = 0;

retry:
  ;
  CURLM *multi_handle = curl_multi_init();
  if (!multi_handle)
    return -1;

  for (int i = 0; i < num_jobs; i++) {
    if (setup_job(&jobs[i], multi_handle) != 0 && jobs[i].handle) {
      curl_multi_remove_handle(multi_handle, jobs[i].handle);
      curl_easy_cleanup(jobs[i].handle);
      jobs[i].handle = NULL;
    }
  }

  http_delay();

  int still_running = 0;
  curl_multi_perform(multi_handle, &still_running);

  do {
    int numfds = 0;
    CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
    if (mc != CURLM_OK)
      break;
    curl_multi_perform(multi_handle, &still_running);
  } while (still_running);

  handle_responses(multi_handle, jobs, num_jobs);
  curl_multi_cleanup(multi_handle);

  if (retries < max_proxy_retries && should_retry(jobs, num_jobs)) {
    retries++;
    goto retry;
  }

  return 0;
}

int scrape_engine(const SearchEngine *engine, const char *query,
                  SearchResult **out_results, int max_results) {
  ScrapeJob job = {.engine = engine,
                   .query = (char *)query,
                   .out_results = out_results,
                   .max_results = max_results,
                   .results_count = 0,
                   .page = 1,
                   .http_status = 0,
                   .status = SCRAPE_STATUS_PENDING};

  scrape_engines_parallel(&job, 1);
  return job.results_count;
}
