#include "AiOverview.h"
#include "../Scraping/Scraping.h"
#include "../Utility/Unescape.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *escape_json(const char *input) {
  if (!input)
    return strdup("");
  int len = strlen(input);
  char *output = malloc(len * 2 + 1);
  if (!output)
    return NULL;

  int out_idx = 0;
  for (int i = 0; i < len; i++) {
    unsigned char c = input[i];
    if (c == '"') {
      output[out_idx++] = '\\';
      output[out_idx++] = '"';
    } else if (c == '\\') {
      output[out_idx++] = '\\';
      output[out_idx++] = '\\';
    } else if (c == '\n') {
      output[out_idx++] = '\\';
      output[out_idx++] = 'n';
    } else if (c == '\r') {
      output[out_idx++] = '\\';
      output[out_idx++] = 'r';
    } else if (c == '\t') {
      output[out_idx++] = '\\';
      output[out_idx++] = 't';
    } else if (c < 0x20) {
      /* skip other control chars */
    } else {
      output[out_idx++] = c;
    }
  }
  output[out_idx] = '\0';
  return output;
}

static char *unescape_json_string(const char *json_str) {
  int len = strlen(json_str);
  char *output = malloc(len + 1);
  if (!output)
    return NULL;

  int out_idx = 0;
  for (int i = 0; i < len; i++) {
    if (json_str[i] == '\\' && i + 1 < len) {
      i++;
      if (json_str[i] == 'n')
        output[out_idx++] = '\n';
      else if (json_str[i] == 't')
        output[out_idx++] = '\t';
      else if (json_str[i] == 'r')
        output[out_idx++] = '\r';
      else if (json_str[i] == '"')
        output[out_idx++] = '"';
      else if (json_str[i] == '\\')
        output[out_idx++] = '\\';
      else
        output[out_idx++] = json_str[i]; // Unrecognized escape? Just copy
    } else {
      output[out_idx++] = json_str[i];
    }
  }
  output[out_idx] = '\0';
  return output;
}

int handle_ai_overview(UrlParams *params) {
  char *raw_query = "";
  if (params) {
    for (int i = 0; i < params->count; i++) {
      if (strcmp(params->params[i].key, "q") == 0) {
        raw_query = params->params[i].value;
        break;
      }
    }
  }

  if (!raw_query || strlen(raw_query) == 0) {
    send_response("No query provided.");
    return -1;
  }

  const char *base_url = getenv("OPENAI_BASE_URL");
  const char *api_key = getenv("OPENAI_API_KEY");
  const char *model = getenv("OPENAI_MODEL");

  if (!base_url || !api_key) {
    send_response("AI Overview is not configured.");
    return 0;
  }
  
  if (!model) {
    model = "openrouter/free";
  }

  // 1. Fetch top results (using cache)
  int enabled_engine_count = 0;
  for (int i = 0; i < ENGINE_COUNT; i++) {
    if (ENGINE_REGISTRY[i].enabled) {
      enabled_engine_count++;
    }
  }

  ScrapeJob jobs[ENGINE_COUNT];
  SearchResult *all_results[ENGINE_COUNT];

  int engine_idx = 0;
  for (int i = 0; i < ENGINE_COUNT; i++) {
    if (ENGINE_REGISTRY[i].enabled) {
      all_results[engine_idx] = NULL;
      jobs[engine_idx].engine = &ENGINE_REGISTRY[i];
      jobs[engine_idx].query = raw_query;
      jobs[engine_idx].out_results = &all_results[engine_idx];
      jobs[engine_idx].max_results = 5; // We only need top few results
      jobs[engine_idx].results_count = 0;
      jobs[engine_idx].page = 1;
      jobs[engine_idx].handle = NULL;
      jobs[engine_idx].response.memory = NULL;
      jobs[engine_idx].response.size = 0;
      jobs[engine_idx].response.capacity = 0;
      jobs[engine_idx].http_status = 0;
      jobs[engine_idx].status = SCRAPE_STATUS_PENDING;
      engine_idx++;
    }
  }

  if (enabled_engine_count > 0) {
    scrape_engines_parallel(jobs, enabled_engine_count);
  }

  // 2. Build context string
  char context_buffer[8192] = {0};
  int context_len = 0;
  int results_added = 0;

  for (int i = 0; i < enabled_engine_count; i++) {
    for (int j = 0; j < jobs[i].results_count; j++) {
      if (results_added >= 5) break;
      
      char snippet_entry[1024];
      snprintf(snippet_entry, sizeof(snippet_entry), "- %s: %s\n",
               all_results[i][j].title ? all_results[i][j].title : "Untitled",
               all_results[i][j].snippet ? all_results[i][j].snippet : "");
               
      if (context_len + strlen(snippet_entry) < sizeof(context_buffer) - 1) {
        strcat(context_buffer, snippet_entry);
        context_len += strlen(snippet_entry);
        results_added++;
      }
    }
    if (results_added >= 5) break;
  }

  for (int i = 0; i < enabled_engine_count; i++) {
    if (all_results[i]) {
      for (int k = 0; k < jobs[i].results_count; k++) {
        free(all_results[i][k].url);
        free(all_results[i][k].title);
        free(all_results[i][k].snippet);
      }
      free(all_results[i]);
    }
  }

  // 3. Prepare AI Prompt
  char prompt_buffer[10240];
  snprintf(prompt_buffer, sizeof(prompt_buffer),
           "You are an AI assistant. The user searched for: %s\n\n"
           "Here are the top web search results:\n%s\n\n"
           "Please provide a helpful, concise summary and direct overview based strictly on these results. "
           "Write in Markdown. Be concise.",
           raw_query, context_buffer);

  char *escaped_prompt = escape_json(prompt_buffer);

  char *json_body = malloc(strlen(escaped_prompt) + 512);
  if (!json_body) {
    free(escaped_prompt);
    send_response("Internal Server Error.");
    return -1;
  }

  snprintf(json_body, strlen(escaped_prompt) + 512,
           "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
           model, escaped_prompt);
  free(escaped_prompt);

  // 4. Send API Request
  CURL *curl = curl_easy_init();
  if (!curl) {
    free(json_body);
    send_response("Failed to initialize cURL.");
    return -1;
  }

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  
  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
  headers = curl_slist_append(headers, auth_header);

  MemoryBuffer response = {malloc(1), 0, 1};
  response.memory[0] = '\0';

  curl_easy_setopt(curl, CURLOPT_URL, base_url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

  CURLcode res = curl_easy_perform(curl);
  
  curl_slist_free_all(headers);
  free(json_body);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    send_response("Failed to contact AI provider.");
    free(response.memory);
    return -1;
  }

  // 5. Very basic JSON "content" extraction
  char *output = "Failed to parse AI response.";
  
  char *content_start = strstr(response.memory, "\"content\"");
  if (content_start) {
    char *colon = strchr(content_start, ':');
    if (colon) {
      char *quote = strchr(colon, '"');
      if (quote) {
        quote++; // move past the opening quote
        char *end_quote = quote;
        int escaped = 0;
        
        while (*end_quote) {
          if (*end_quote == '\\' && !escaped) {
             escaped = 1;
          } else if (*end_quote == '"' && !escaped) {
             break;
          } else {
             escaped = 0;
          }
          end_quote++;
        }
        
        if (*end_quote == '"') {
           *end_quote = '\0'; // truncate
           char *unescaped = unescape_json_string(quote);
           if (unescaped) {
               send_response(unescaped);
               free(unescaped);
               free(response.memory);
               return 0; // Success!
           }
        }
      }
    }
  }

  send_response(output);
  free(response.memory);
  return 0;
}
