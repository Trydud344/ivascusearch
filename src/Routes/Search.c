#include "Search.h"
#include "../Infobox/Wikipedia.h"
#include "../Infobox/Calculator.h"
#include "../Infobox/Dictionary.h"
#include "../Infobox/UnitConversion.h"
#include "../Scraping/Scraping.h"
#include "../Utility/Display.h"
#include "../Utility/Unescape.h"
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

static void *wiki_thread_func(void *arg) {
  InfoBoxThreadData *data = (InfoBoxThreadData *)arg;
  char *dynamic_url = construct_wiki_url(data->query);
  if (dynamic_url) {
    data->result = fetch_wiki_data(dynamic_url);
    data->success =
        (data->result.title != NULL && data->result.extract != NULL &&
         strlen(data->result.extract) > 10);
    free(dynamic_url);
  } else {
    data->success = 0;
  }
  return NULL;
}

static int is_calculator_query(const char *query) {
  if (!query) return 0;

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

  if (!has_digit || !has_math_operator) return 0;

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
        if (query[j] != ' ') break;
      }

      for (int j = i + 1; j < len; j++) {
        if (isdigit(query[j]) || query[j] == '.') {
          has_num_after = 1;
          break;
        }
        if (query[j] != ' ') break;
      }

      if (has_num_before || has_num_after) {
        return 1;
      }
    }
  }

  return 0;
}

static void *calc_thread_func(void *arg) {
  InfoBoxThreadData *data = (InfoBoxThreadData *)arg;

  if (is_calculator_query(data->query)) {
    data->result = fetch_calc_data((char *)data->query);
    data->success =
        (data->result.title != NULL && data->result.extract != NULL);
  } else {
    data->success = 0;
  }

  return NULL;
}

static void *dict_thread_func(void *arg) {
  InfoBoxThreadData *data = (InfoBoxThreadData *)arg;

  if (is_dictionary_query(data->query)) {
    data->result = fetch_dictionary_data(data->query);
    data->success =
        (data->result.title != NULL && data->result.extract != NULL);
  } else {
    data->success = 0;
  }

  return NULL;
}

static void *unit_thread_func(void *arg) {
  InfoBoxThreadData *data = (InfoBoxThreadData *)arg;

  if (is_unit_conv_query(data->query)) {
    data->result = fetch_unit_conv_data(data->query);
    data->success =
        (data->result.title != NULL && data->result.extract != NULL);
  } else {
    data->success = 0;
  }

  return NULL;
}

static int add_infobox_to_collection(InfoBox *infobox, char ****collection,
                                     int **inner_counts, int current_count) {
  *collection =
      (char ***)realloc(*collection, sizeof(char **) * (current_count + 1));
  *inner_counts =
      (int *)realloc(*inner_counts, sizeof(int) * (current_count + 1));

  (*collection)[current_count] = (char **)malloc(sizeof(char *) * 4);
  (*collection)[current_count][0] = infobox->title ? strdup(infobox->title) : NULL;
  (*collection)[current_count][1] = infobox->thumbnail_url ? strdup(infobox->thumbnail_url) : NULL;
  (*collection)[current_count][2] = infobox->extract ? strdup(infobox->extract) : NULL;
  (*collection)[current_count][3] = infobox->url ? strdup(infobox->url) : NULL;
  (*inner_counts)[current_count] = 4;

  return current_count + 1;
}

int results_handler(UrlParams *params) {
  TemplateContext ctx = new_context();
  char *raw_query = "";
  int page = 1;

  if (params) {
    for (int i = 0; i < params->count; i++) {
      if (strcmp(params->params[i].key, "q") == 0) {
        raw_query = params->params[i].value;
      } else if (strcmp(params->params[i].key, "p") == 0) {
        int parsed = atoi(params->params[i].value);
        if (parsed > 1) page = parsed;
      }
    }
  }

  context_set(&ctx, "query", raw_query);

  char page_str[16], prev_str[16], next_str[16];
  snprintf(page_str, sizeof(page_str), "%d", page);
  snprintf(prev_str, sizeof(prev_str), "%d", page > 1 ? page - 1 : 0);
  snprintf(next_str, sizeof(next_str), "%d", page + 1);
  context_set(&ctx, "page",      page_str);
  context_set(&ctx, "prev_page", prev_str);
  context_set(&ctx, "next_page", next_str);

  if (!raw_query || strlen(raw_query) == 0) {
    send_response("<h1>No query provided</h1>");
    free_context(&ctx);
    return -1;
  }

  pthread_t wiki_tid, calc_tid, dict_tid, unit_tid;
  InfoBoxThreadData wiki_data = {.query = raw_query, .success = 0};
  InfoBoxThreadData calc_data = {.query = raw_query, .success = 0};
  InfoBoxThreadData dict_data = {.query = raw_query, .success = 0};
  InfoBoxThreadData unit_data = {.query = raw_query, .success = 0};

  if (page == 1) {
    pthread_create(&wiki_tid, NULL, wiki_thread_func, &wiki_data);
    pthread_create(&calc_tid, NULL, calc_thread_func, &calc_data);
    pthread_create(&dict_tid, NULL, dict_thread_func, &dict_data);
    pthread_create(&unit_tid, NULL, unit_thread_func, &unit_data);
  }

  ScrapeJob jobs[ENGINE_COUNT];
  SearchResult *all_results[ENGINE_COUNT];

  for (int i = 0; i < ENGINE_COUNT; i++) {
    all_results[i] = NULL;
    jobs[i].engine = &ENGINE_REGISTRY[i];
    jobs[i].query = raw_query;
    jobs[i].out_results = &all_results[i];
    jobs[i].max_results = 10;
    jobs[i].results_count = 0;
    jobs[i].page = page;
    jobs[i].handle = NULL;
    jobs[i].response.memory = NULL;
    jobs[i].response.size = 0;
    jobs[i].response.capacity = 0;
  }

  scrape_engines_parallel(jobs, ENGINE_COUNT);

  if (page == 1) {
    pthread_join(wiki_tid, NULL);
    pthread_join(calc_tid, NULL);
    pthread_join(dict_tid, NULL);
    pthread_join(unit_tid, NULL);
  }

  char ***infobox_matrix = NULL;
  int *infobox_inner_counts = NULL;
  int infobox_count = 0;

  if (page == 1) {
    if (dict_data.success) {
      infobox_count = add_infobox_to_collection(&dict_data.result, &infobox_matrix,
                                                &infobox_inner_counts, infobox_count);
    }

    if (calc_data.success) {
      infobox_count = add_infobox_to_collection(&calc_data.result, &infobox_matrix,
                                                &infobox_inner_counts, infobox_count);
    }

    if (unit_data.success) {
      infobox_count = add_infobox_to_collection(&unit_data.result, &infobox_matrix,
                                                &infobox_inner_counts, infobox_count);
    }

    if (wiki_data.success) {
      infobox_count = add_infobox_to_collection(&wiki_data.result, &infobox_matrix,
                                                &infobox_inner_counts, infobox_count);
    }
  }

  if (infobox_count > 0) {
    context_set_array_of_arrays(&ctx, "infoboxes", infobox_matrix,
                                infobox_count, infobox_inner_counts);
    for (int i = 0; i < infobox_count; i++) {
      for (int j = 0; j < 4; j++) free(infobox_matrix[i][j]);
      free(infobox_matrix[i]);
    }
    free(infobox_matrix);
    free(infobox_inner_counts);
  }

  int total_results = 0;
  for (int i = 0; i < ENGINE_COUNT; i++) {
    total_results += jobs[i].results_count;
  }

  if (total_results > 0) {
    char ***results_matrix = (char ***)malloc(sizeof(char **) * total_results);
    int *results_inner_counts = (int *)malloc(sizeof(int) * total_results);
    char **seen_urls = (char **)malloc(sizeof(char *) * total_results);
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
        results_matrix[unique_count] = (char **)malloc(sizeof(char *) * 4);
        char *pretty_url = pretty_display_url(display_url);

        results_matrix[unique_count][0] = strdup(display_url);
        results_matrix[unique_count][1] = strdup(pretty_url);
        results_matrix[unique_count][2] = all_results[i][j].title ? strdup(all_results[i][j].title) : strdup("Untitled");
        results_matrix[unique_count][3] = all_results[i][j].snippet ? strdup(all_results[i][j].snippet) : strdup("");

        results_inner_counts[unique_count] = 4;

        free(pretty_url);
        free(all_results[i][j].url);
        free(all_results[i][j].title);
        free(all_results[i][j].snippet);

        unique_count++;
      }
      free(all_results[i]);
    }

    context_set_array_of_arrays(&ctx, "results", results_matrix, unique_count, results_inner_counts);

    char *html = render_template("results.html", &ctx);
    if (html) {
      send_response(html);
      free(html);
    }

    for (int i = 0; i < unique_count; i++) {
      for (int j = 0; j < 4; j++) free(results_matrix[i][j]);
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
  }

  if (page == 1) {
    if (wiki_data.success) free_infobox(&wiki_data.result);
    if (calc_data.success) free_infobox(&calc_data.result);
    if (dict_data.success) free_infobox(&dict_data.result);
    if (unit_data.success) free_infobox(&unit_data.result);
  }
  free_context(&ctx);

  return 0;
}