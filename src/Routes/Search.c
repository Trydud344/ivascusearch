#include "Search.h"
#include "../Infobox/Calculator.h"
#include "../Infobox/CurrencyConversion.h"
#include "../Infobox/Dictionary.h"
#include "../Infobox/UnitConversion.h"
#include "../Infobox/Wikipedia.h"
#include "../Scraping/Scraping.h"
#include "../Utility/Display.h"
#include "../Utility/Unescape.h"
#include "Config.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  const char *query;
  InfoBox result;
  int success;
} InfoBoxThreadData;

typedef struct {
  int (*check_fn)(const char *query);
  InfoBox (*fetch_fn)(char *query);
  char *(*url_construct_fn)(const char *query);
} InfoBoxHandler;

static InfoBox fetch_wiki_wrapper(char *query) {
  char *url = construct_wiki_url(query);
  if (!url)
    return (InfoBox){NULL};
  InfoBox result = fetch_wiki_data(url);
  free(url);
  return result;
}

static int always_true(const char *query) {
  (void)query;
  return 1;
}

static InfoBox fetch_dict_wrapper(char *query) {
  return fetch_dictionary_data(query);
}
static InfoBox fetch_calc_wrapper(char *query) {
  return fetch_calc_data(query);
}
static InfoBox fetch_unit_wrapper(char *query) {
  return fetch_unit_conv_data(query);
}
static InfoBox fetch_currency_wrapper(char *query) {
  return fetch_currency_data(query);
}

static int is_calculator_query(const char *query) {
  if (!query)
    return 0;

  int has_digit = 0;
  int has_math_operator = 0;

  for (const char *p = query; *p; p++) {
    if (isdigit(*p) || *p == '.') {
      has_digit = 1;
    }
    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '^') {
      has_math_operator = 1;
    }
  }

  if (!has_digit || !has_math_operator)
    return 0;

  int len = strlen(query);
  for (int i = 0; i < len; i++) {
    char c = query[i];
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
      int has_num_before = 0;
      int has_num_after = 0;

      for (int j = i - 1; j >= 0; j--) {
        if (isdigit(query[j]) || query[j] == '.') {
          has_num_before = 1;
          break;
        }
        if (query[j] != ' ')
          break;
      }

      for (int j = i + 1; j < len; j++) {
        if (isdigit(query[j]) || query[j] == '.') {
          has_num_after = 1;
          break;
        }
        if (query[j] != ' ')
          break;
      }

      if (has_num_before || has_num_after) {
        return 1;
      }
    }
  }

  return 0;
}

static InfoBoxHandler handlers[] = {
    {is_dictionary_query, fetch_dict_wrapper, NULL},
    {is_calculator_query, fetch_calc_wrapper, NULL},
    {is_unit_conv_query, fetch_unit_wrapper, NULL},
    {is_currency_query, fetch_currency_wrapper, NULL},
    {always_true, fetch_wiki_wrapper, construct_wiki_url},
};
enum { HANDLER_COUNT = sizeof(handlers) / sizeof(handlers[0]) };

static void *infobox_thread_func(void *arg) {
  InfoBoxThreadData *data = (InfoBoxThreadData *)arg;
  int handler_idx = data->success;
  if (handler_idx < 0 || handler_idx >= HANDLER_COUNT)
    return NULL;

  InfoBoxHandler *h = &handlers[handler_idx];
  if (h->check_fn && !h->check_fn(data->query)) {
    data->success = 0;
    return NULL;
  }

  data->result = h->fetch_fn((char *)data->query);
  data->success = (data->result.title != NULL && data->result.extract != NULL &&
                   strlen(data->result.extract) > 10);
  return NULL;
}

static int add_infobox_to_collection(InfoBox *infobox, char ****collection,
                                     int **inner_counts, int current_count) {
  *collection =
      (char ***)realloc(*collection, sizeof(char **) * (current_count + 1));
  *inner_counts =
      (int *)realloc(*inner_counts, sizeof(int) * (current_count + 1));

  (*collection)[current_count] =
      (char **)malloc(sizeof(char *) * INFOBOX_FIELD_COUNT);
  (*collection)[current_count][0] =
      infobox->title ? strdup(infobox->title) : NULL;
  (*collection)[current_count][1] =
      infobox->thumbnail_url ? strdup(infobox->thumbnail_url) : NULL;
  (*collection)[current_count][2] =
      infobox->extract ? strdup(infobox->extract) : NULL;
  (*collection)[current_count][3] = infobox->url ? strdup(infobox->url) : NULL;
  (*inner_counts)[current_count] = INFOBOX_FIELD_COUNT;

  return current_count + 1;
}

static int add_warning_to_collection(const char *engine_name,
                                     const char *warning_message,
                                     char ****collection, int **inner_counts,
                                     int current_count) {
  char ***new_collection =
      (char ***)malloc(sizeof(char **) * (current_count + 1));
  int *new_inner_counts =
      (int *)malloc(sizeof(int) * (current_count + 1));

  if (!new_collection || !new_inner_counts) {
    free(new_collection);
    free(new_inner_counts);
    return current_count;
  }

  if (*collection && current_count > 0) {
    memcpy(new_collection, *collection, sizeof(char **) * current_count);
  }
  if (*inner_counts && current_count > 0) {
    memcpy(new_inner_counts, *inner_counts, sizeof(int) * current_count);
  }

  free(*collection);
  free(*inner_counts);

  *collection = new_collection;
  *inner_counts = new_inner_counts;

  (*collection)[current_count] = (char **)malloc(sizeof(char *) * 2);
  if (!(*collection)[current_count])
    return current_count;

  (*collection)[current_count][0] = strdup(engine_name ? engine_name : "");
  (*collection)[current_count][1] =
      strdup(warning_message ? warning_message : "");

  if (!(*collection)[current_count][0] || !(*collection)[current_count][1]) {
    free((*collection)[current_count][0]);
    free((*collection)[current_count][1]);
    free((*collection)[current_count]);
    return current_count;
  }

  (*inner_counts)[current_count] = 2;
  return current_count + 1;
}

static const char *warning_message_for_job(const ScrapeJob *job) {
  switch (job->status) {
  case SCRAPE_STATUS_FETCH_ERROR:
    return "request failed before OmniSearch could read search results.";
  case SCRAPE_STATUS_PARSE_MISMATCH:
    return "returned search results in a format OmniSearch could not parse.";
  case SCRAPE_STATUS_BLOCKED:
    return "returned a captcha or another blocking page instead of search "
           "results.";
  default:
    return NULL;
  }
}

int results_handler(UrlParams *params) {
  TemplateContext ctx = new_context();
  char *raw_query = "";
  int page = 1;
  int btnI = 0;

  if (params) {
    for (int i = 0; i < params->count; i++) {
      if (strcmp(params->params[i].key, "q") == 0) {
        raw_query = params->params[i].value;
      } else if (strcmp(params->params[i].key, "p") == 0) {
        int parsed = atoi(params->params[i].value);
        if (parsed > 1)
          page = parsed;
      } else if (strcmp(params->params[i].key, "btnI") == 0) {
        btnI = atoi(params->params[i].value);
      }
    }
  }

  context_set(&ctx, "query", raw_query);

  char page_str[16], prev_str[16], next_str[16], two_prev_str[16],
      two_next_str[16];
  snprintf(page_str, sizeof(page_str), "%d", page);
  snprintf(prev_str, sizeof(prev_str), "%d", page > 1 ? page - 1 : 0);
  snprintf(next_str, sizeof(next_str), "%d", page + 1);
  snprintf(two_prev_str, sizeof(two_prev_str), "%d", page > 2 ? page - 2 : 0);
  snprintf(two_next_str, sizeof(two_next_str), "%d", page + 2);
  context_set(&ctx, "page", page_str);
  context_set(&ctx, "prev_page", prev_str);
  context_set(&ctx, "next_page", next_str);
  context_set(&ctx, "two_prev_page", two_prev_str);
  context_set(&ctx, "two_next_page", two_next_str);

  if (!raw_query || strlen(raw_query) == 0) {
    send_response("<h1>No query provided</h1>");
    free_context(&ctx);
    return -1;
  }

  pthread_t infobox_threads[HANDLER_COUNT];
  InfoBoxThreadData infobox_data[HANDLER_COUNT];

  for (int i = 0; i < HANDLER_COUNT; i++) {
    infobox_data[i].query = raw_query;
    infobox_data[i].success = i;
    infobox_data[i].result = (InfoBox){NULL};
  }

  if (page == 1) {
    for (int i = 0; i < HANDLER_COUNT; i++) {
      pthread_create(&infobox_threads[i], NULL, infobox_thread_func,
                     &infobox_data[i]);
    }
  }

  ScrapeJob jobs[ENGINE_COUNT];
  SearchResult *all_results[ENGINE_COUNT];

  for (int i = 0; i < ENGINE_COUNT; i++) {
    all_results[i] = NULL;
    jobs[i].engine = &ENGINE_REGISTRY[i];
    jobs[i].query = raw_query;
    jobs[i].out_results = &all_results[i];
    jobs[i].max_results = MAX_RESULTS_PER_ENGINE;
    jobs[i].results_count = 0;
    jobs[i].page = page;
    jobs[i].handle = NULL;
    jobs[i].response.memory = NULL;
    jobs[i].response.size = 0;
    jobs[i].response.capacity = 0;
    jobs[i].http_status = 0;
    jobs[i].status = SCRAPE_STATUS_PENDING;
  }

  scrape_engines_parallel(jobs, ENGINE_COUNT);

  if (page == 1) {
    for (int i = 0; i < HANDLER_COUNT; i++) {
      pthread_join(infobox_threads[i], NULL);
    }
  }

  if (btnI) {
    for (int i = 0; i < ENGINE_COUNT; i++) {
      if (jobs[i].results_count > 0 && all_results[i][0].url) {
        char *redirect_url = strdup(all_results[i][0].url);
        for (int j = 0; j < ENGINE_COUNT; j++) {
          for (int k = 0; k < jobs[j].results_count; k++) {
            free(all_results[j][k].url);
            free(all_results[j][k].title);
            free(all_results[j][k].snippet);
          }
          free(all_results[j]);
        }
        if (page == 1) {
          for (int j = 0; j < HANDLER_COUNT; j++) {
            if (infobox_data[j].success) {
              free_infobox(&infobox_data[j].result);
            }
          }
        }
        free_context(&ctx);
        if (redirect_url) {
          send_redirect(redirect_url);
          free(redirect_url);
        }
        return 0;
      }
    }
    for (int i = 0; i < ENGINE_COUNT; i++) {
      free(all_results[i]);
    }
    if (page == 1) {
      for (int i = 0; i < HANDLER_COUNT; i++) {
        if (infobox_data[i].success) {
          free_infobox(&infobox_data[i].result);
        }
      }
    }
    free_context(&ctx);
    send_response("<h1>No results found</h1>");
    return 0;
  }
  
  char ***infobox_matrix = NULL;
  int *infobox_inner_counts = NULL;
  int infobox_count = 0;

  if (page == 1) {
    for (int i = 0; i < HANDLER_COUNT; i++) {
      if (infobox_data[i].success) {
        infobox_count =
            add_infobox_to_collection(&infobox_data[i].result, &infobox_matrix,
                                      &infobox_inner_counts, infobox_count);
      }
    }
  }

  if (infobox_count > 0) {
    context_set_array_of_arrays(&ctx, "infoboxes", infobox_matrix,
                                infobox_count, infobox_inner_counts);
    for (int i = 0; i < infobox_count; i++) {
      for (int j = 0; j < INFOBOX_FIELD_COUNT; j++)
        free(infobox_matrix[i][j]);
      free(infobox_matrix[i]);
    }
    free(infobox_matrix);
    free(infobox_inner_counts);
  }

  int warning_count = 0;
  for (int i = 0; i < ENGINE_COUNT; i++) {
    if (warning_message_for_job(&jobs[i]))
      warning_count++;
  }

  if (warning_count > 0) {
    char ***warning_matrix = NULL;
    int *warning_inner_counts = NULL;
    int warning_index = 0;

    for (int i = 0; i < ENGINE_COUNT; i++) {
      const char *warning_message = warning_message_for_job(&jobs[i]);
      if (!warning_message)
        continue;

      warning_index = add_warning_to_collection(
          jobs[i].engine->name, warning_message, &warning_matrix,
          &warning_inner_counts, warning_index);
    }

    if (warning_index > 0) {
      context_set_array_of_arrays(&ctx, "engine_warnings", warning_matrix,
                                  warning_index, warning_inner_counts);
    }

    if (warning_matrix) {
      for (int i = 0; i < warning_index; i++) {
        free(warning_matrix[i][0]);
        free(warning_matrix[i][1]);
        free(warning_matrix[i]);
      }
      free(warning_matrix);
    }
    if (warning_inner_counts)
      free(warning_inner_counts);
  }

  int total_results = 0;
  for (int i = 0; i < ENGINE_COUNT; i++) {
    total_results += jobs[i].results_count;
  }

  if (total_results > 0) {
    char ***results_matrix = (char ***)malloc(sizeof(char **) * total_results);
    int *results_inner_counts = (int *)malloc(sizeof(int) * total_results);
    char **seen_urls = (char **)malloc(sizeof(char *) * total_results);
    if (!results_matrix || !results_inner_counts || !seen_urls) {
      if (results_matrix)
        free(results_matrix);
      if (results_inner_counts)
        free(results_inner_counts);
      if (seen_urls)
        free(seen_urls);
      char *html = render_template("results.html", &ctx);
      if (html) {
        send_response(html);
        free(html);
      }
      for (int i = 0; i < ENGINE_COUNT; i++)
        free(all_results[i]);
      if (page == 1) {
        for (int i = 0; i < HANDLER_COUNT; i++) {
          if (infobox_data[i].success) {
            free_infobox(&infobox_data[i].result);
          }
        }
      }
      free_context(&ctx);
      return 0;
    }
    int unique_count = 0;

    for (int i = 0; i < ENGINE_COUNT; i++) {
      for (int j = 0; j < jobs[i].results_count; j++) {
        char *display_url = all_results[i][j].url;

        int is_duplicate = 0;
        for (int k = 0; k < unique_count; k++) {
          if (strcmp(seen_urls[k], display_url) == 0) {
            is_duplicate = 1;
            break;
          }
        }

        if (is_duplicate) {
          free(all_results[i][j].url);
          free(all_results[i][j].title);
          free(all_results[i][j].snippet);
          continue;
        }

        seen_urls[unique_count] = strdup(display_url);
        if (!seen_urls[unique_count]) {
          free(all_results[i][j].url);
          free(all_results[i][j].title);
          free(all_results[i][j].snippet);
          continue;
        }
        results_matrix[unique_count] =
            (char **)malloc(sizeof(char *) * INFOBOX_FIELD_COUNT);
        if (!results_matrix[unique_count]) {
          free(seen_urls[unique_count]);
          free(all_results[i][j].url);
          free(all_results[i][j].title);
          free(all_results[i][j].snippet);
          continue;
        }
        char *pretty_url = pretty_display_url(display_url);

        results_matrix[unique_count][0] = strdup(display_url);
        results_matrix[unique_count][1] = strdup(pretty_url);
        results_matrix[unique_count][2] = all_results[i][j].title
                                              ? strdup(all_results[i][j].title)
                                              : strdup("Untitled");
        results_matrix[unique_count][3] =
            all_results[i][j].snippet ? strdup(all_results[i][j].snippet)
                                      : strdup("");

        results_inner_counts[unique_count] = INFOBOX_FIELD_COUNT;

        free(pretty_url);
        free(all_results[i][j].url);
        free(all_results[i][j].title);
        free(all_results[i][j].snippet);

        unique_count++;
      }
      free(all_results[i]);
    }

    context_set_array_of_arrays(&ctx, "results", results_matrix, unique_count,
                                results_inner_counts);

    char *html = render_template("results.html", &ctx);
    if (html) {
      send_response(html);
      free(html);
    }

    for (int i = 0; i < unique_count; i++) {
      for (int j = 0; j < INFOBOX_FIELD_COUNT; j++)
        free(results_matrix[i][j]);
      free(results_matrix[i]);
      free(seen_urls[i]);
    }
    free(seen_urls);
    free(results_matrix);
    free(results_inner_counts);
  } else {
    char *html = render_template("results.html", &ctx);
    if (html) {
      send_response(html);
      free(html);
    }

    for (int i = 0; i < ENGINE_COUNT; i++) {
      free(all_results[i]);
    }
  }

  if (page == 1) {
    for (int i = 0; i < HANDLER_COUNT; i++) {
      if (infobox_data[i].success) {
        free_infobox(&infobox_data[i].result);
      }
    }
  }
  free_context(&ctx);

  return 0;
}
