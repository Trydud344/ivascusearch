#include "Wikipedia.h"
#include "../Proxy/Proxy.h"
#include "../Scraping/Scraping.h"
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct WikiMemoryStruct {
  char *memory;
  size_t size;
};

static void shorten_summary(char **extract_ptr, int max_chars) {
  if (!extract_ptr || !*extract_ptr) return;

  char *text = *extract_ptr;
  int len = strlen(text);

  if (len <= max_chars) return;

  int end_pos = max_chars;
  for (int i = max_chars; i > (max_chars / 2); i--) {
    if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
      end_pos = i + 1;
      break;
    }
  }

  char *new_text = (char *)malloc(end_pos + 4);

  if (new_text) {
    strncpy(new_text, text, end_pos);
    new_text[end_pos] = '\0';
    strcat(new_text, "...");
    free(*extract_ptr);
    *extract_ptr = new_text;
  }
}

static size_t WikiWriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                      void *userp) {
  size_t realsize = size * nmemb;
  struct WikiMemoryStruct *mem = (struct WikiMemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (ptr == NULL) {
    fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static void extract_wiki_info(xmlNode *node, InfoBox *info) {
  xmlNode *cur_node = NULL;

  for (cur_node = node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (strcmp((const char *)cur_node->name, "page") == 0) {
        xmlChar *title = xmlGetProp(cur_node, (const xmlChar *)"title");
        if (title) {
          info->title = strdup((const char *)title);

          const char *base_article_url = "https://en.wikipedia.org/wiki/";
          char *formatted_title = strdup((const char *)title);
          for (int i = 0; formatted_title[i]; i++) {
            if (formatted_title[i] == ' ') formatted_title[i] = '_';
          }

          info->url =
              malloc(strlen(base_article_url) + strlen(formatted_title) + 1);
          if (info->url) {
            strcpy(info->url, base_article_url);
            strcat(info->url, formatted_title);
          }
          free(formatted_title);
          xmlFree(title);
        }
      }

      if (strcmp((const char *)cur_node->name, "thumbnail") == 0) {
        xmlChar *source = xmlGetProp(cur_node, (const xmlChar *)"source");
        if (source) {
          info->thumbnail_url = strdup((const char *)source);
          xmlFree(source);
        }
      }

      if (strcmp((const char *)cur_node->name, "extract") == 0) {
        xmlChar *content = xmlNodeGetContent(cur_node);
        if (content) {
          info->extract = strdup((const char *)content);

          shorten_summary(&(info->extract), 300);
          xmlFree(content);
        }
      }
    }
    extract_wiki_info(cur_node->children, info);
  }
}

InfoBox fetch_wiki_data(char *api_url) {
  CURL *curl_handle;
  CURLcode res;
  struct WikiMemoryStruct chunk;
  InfoBox info = {NULL, NULL, NULL, NULL};

  chunk.memory = malloc(1);
  chunk.size = 0;

  curl_handle = curl_easy_init();

  if (curl_handle) {
    curl_easy_setopt(curl_handle, CURLOPT_URL, api_url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION,
                     WikiWriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    apply_proxy_settings(curl_handle);

    res = curl_easy_perform(curl_handle);

    if (res == CURLE_OK) {
      xmlDocPtr doc =
          xmlReadMemory(chunk.memory, chunk.size, "noname.xml", NULL, 0);
      if (doc != NULL) {
        xmlNode *root_element = xmlDocGetRootElement(doc);
        extract_wiki_info(root_element, &info);
        xmlFreeDoc(doc);
      }
    }

    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
  }

  return info;
}

char *construct_wiki_url(const char *search_term) {
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;

  char *escaped_term = curl_easy_escape(curl, search_term, 0);
  const char *base =
      "https://en.wikipedia.org/w/"
      "api.php?action=query&prop=extracts|pageimages&exintro&"
      "explaintext&pithumbsize=400&format=xml&origin=*&titles=";

  char *full_url = malloc(strlen(base) + strlen(escaped_term) + 1);
  if (full_url) {
    strcpy(full_url, base);
    strcat(full_url, escaped_term);
  }

  curl_free(escaped_term);
  curl_easy_cleanup(curl);
  return full_url;
}
