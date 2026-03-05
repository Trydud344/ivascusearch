#include "Images.h"
#include "../Utility/Unescape.h"
#include "../Proxy/Proxy.h"
#include "../Scraping/Scraping.h"

#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct MemoryBlock {
  char *response;
  size_t size;
};

static size_t ImageWriteCallback(void *data, size_t size, size_t nmemb,
                 void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryBlock *mem = (struct MemoryBlock *)userp;
  char *ptr = (char *)realloc(mem->response, mem->size + realsize + 1);
  if (ptr == NULL) {
  return 0;
  }
  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;
  return realsize;
}

static char *fetch_images_html(const char *url) {
  CURL *curl_handle;
  struct MemoryBlock chunk = {.response = malloc(1), .size = 0};
  if (!chunk.response) {
  return NULL;
  }

  curl_handle = curl_easy_init();
  if (!curl_handle) {
  free(chunk.response);
  return NULL;
  }

  curl_easy_setopt(curl_handle, CURLOPT_URL, url);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, ImageWriteCallback);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
  curl_easy_setopt(
    curl_handle, CURLOPT_USERAGENT,
    "Mozilla/5.0 (Windows NT 6.1; WOW64; Trident/7.0; rv:11.0) like Gecko");
  curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
  apply_proxy_settings(curl_handle);

  CURLcode res = curl_easy_perform(curl_handle);
  if (res != CURLE_OK) {
  free(chunk.response);
  curl_easy_cleanup(curl_handle);
  return NULL;
  }

  curl_easy_cleanup(curl_handle);
  return chunk.response;
}

int images_handler(UrlParams *params) {
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
  context_set(&ctx, "page",    page_str);
  context_set(&ctx, "prev_page", prev_str);
  context_set(&ctx, "next_page", next_str);

  char *display_query = url_decode_query(raw_query);
  context_set(&ctx, "query", display_query);

  if (!raw_query || strlen(raw_query) == 0) {
  send_response("<h1>No query provided</h1>");
  if (display_query) free(display_query);
  free_context(&ctx);
  return -1;
  }

  CURL *tmp = curl_easy_init();
  if (!tmp) {
  send_response("<h1>Error initializing curl</h1>");
  if (display_query) free(display_query);
  free_context(&ctx);
  return -1;
  }
  char *encoded_query = curl_easy_escape(tmp, raw_query, 0);
  curl_easy_cleanup(tmp);

  if (!encoded_query) {
  send_response("<h1>Error encoding query</h1>");
  if (display_query) free(display_query);
  free_context(&ctx);
  return -1;
  }

  char url[1024];
  int first = (page - 1) * 32 + 1;
  snprintf(url, sizeof(url),
       "https://www.bing.com/images/search?q=%s&first=%d", encoded_query, first);

  char *html = fetch_images_html(url);
  if (!html) {
  send_response("<h1>Error fetching images</h1>");
  free(encoded_query);
  free(display_query);
  free_context(&ctx);
  return -1;
  }

  htmlDocPtr doc = htmlReadMemory(html, (int)strlen(html), NULL, NULL,
                  HTML_PARSE_RECOVER | HTML_PARSE_NOERROR);
  if (!doc) {
  free(html);
  free(encoded_query);
  free(display_query);
  free_context(&ctx);
  return -1;
  }

  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);

  if (!xpathCtx) {
  xmlFreeDoc(doc);
  free(html);
  free(encoded_query);
  free(display_query);
  free_context(&ctx);
  return -1;
  }

  xmlXPathObjectPtr xpathObj =
    xmlXPathEvalExpression((const xmlChar *)"//div[@class='item']", xpathCtx);

  int image_count = 0;
  char ***image_matrix = NULL;
  int *inner_counts = NULL;

  if (xpathObj && xpathObj->nodesetval) {
  int nodes = xpathObj->nodesetval->nodeNr;

  int max_images = (nodes < 32) ? nodes : 32;
  image_matrix = malloc(sizeof(char **) * max_images);
  inner_counts = malloc(sizeof(int) * max_images);

  for (int i = 0; i < nodes; i++) {
    if (image_count >= 32) break;

    xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
    xmlNodePtr img_node = NULL;
    xmlNodePtr tit_node = NULL;
    xmlNodePtr des_node = NULL;
    xmlNodePtr thumb_link = NULL;

    for (xmlNodePtr child = node->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE) continue;

    if (xmlStrcmp(child->name, (const xmlChar *)"a") == 0) {
      xmlChar *class = xmlGetProp(child, (const xmlChar *)"class");
      if (class) {
      if (xmlStrstr(class, (const xmlChar *)"thumb") != NULL) {
        thumb_link = child;
        for (xmlNodePtr thumb_child = child->children; thumb_child; thumb_child = thumb_child->next) {
        if (xmlStrcmp(thumb_child->name, (const xmlChar *)"div") == 0) {
          xmlChar *div_class = xmlGetProp(thumb_child, (const xmlChar *)"class");
          if (div_class && xmlStrcmp(div_class, (const xmlChar *)"cico") == 0) {
          for (xmlNodePtr cico_child = thumb_child->children; cico_child; cico_child = cico_child->next) {
            if (xmlStrcmp(cico_child->name, (const xmlChar *)"img") == 0) {
            img_node = cico_child;
            break;
            }
          }
          }
          if (div_class) xmlFree(div_class);
        }
        }
      } else if (xmlStrstr(class, (const xmlChar *)"tit") != NULL) {
        tit_node = child;
      }
      xmlFree(class);
      }
    } else if (xmlStrcmp(child->name, (const xmlChar *)"div") == 0) {
      xmlChar *class = xmlGetProp(child, (const xmlChar *)"class");
      if (class && xmlStrcmp(class, (const xmlChar *)"meta") == 0) {
      for (xmlNodePtr meta_child = child->children; meta_child; meta_child = meta_child->next) {
        if (xmlStrcmp(meta_child->name, (const xmlChar *)"div") == 0) {
        xmlChar *div_class = xmlGetProp(meta_child, (const xmlChar *)"class");
        if (div_class) {
          if (xmlStrcmp(div_class, (const xmlChar *)"des") == 0) {
          des_node = meta_child;
          }
          xmlFree(div_class);
        }
        } else if (xmlStrcmp(meta_child->name, (const xmlChar *)"a") == 0) {
        xmlChar *a_class = xmlGetProp(meta_child, (const xmlChar *)"class");
        if (a_class && xmlStrstr(a_class, (const xmlChar *)"tit") != NULL) {
          tit_node = meta_child;
        }
        if (a_class) xmlFree(a_class);
        }
      }
      }
      if (class) xmlFree(class);
    }
    }

    xmlChar *iurl = img_node ? xmlGetProp(img_node, (const xmlChar *)"src") : NULL;
    xmlChar *full_url = thumb_link ? xmlGetProp(thumb_link, (const xmlChar *)"href") : NULL;
    xmlChar *title = des_node ? xmlNodeGetContent(des_node) : (tit_node ? xmlNodeGetContent(tit_node) : NULL);
    xmlChar *rurl = tit_node ? xmlGetProp(tit_node, (const xmlChar *)"href") : NULL;

    if (iurl && strlen((char *)iurl) > 0) {
    char *proxy_url = NULL;
    CURL *esc_curl = curl_easy_init();
    if (esc_curl) {
      char *encoded = curl_easy_escape(esc_curl, (char *)iurl, 0);
      if (encoded) {
      size_t proxy_len = strlen("/proxy?url=") + strlen(encoded) + 1;
      proxy_url = malloc(proxy_len);
      if (proxy_url) {
        snprintf(proxy_url, proxy_len, "/proxy?url=%s", encoded);
      }
      curl_free(encoded);
      }
      curl_easy_cleanup(esc_curl);
    }

    image_matrix[image_count] = malloc(sizeof(char *) * 4);
    image_matrix[image_count][0] = proxy_url ? strdup(proxy_url) : strdup((char *)iurl);
    image_matrix[image_count][1] = strdup(title ? (char *)title : "Image");
    image_matrix[image_count][2] = strdup(rurl ? (char *)rurl : "#");
    image_matrix[image_count][3] = strdup(full_url ? (char *)full_url : "#");
    inner_counts[image_count] = 4;
    image_count++;
    }

    if (iurl) xmlFree(iurl);
    if (title) xmlFree(title);
    if (rurl) xmlFree(rurl);
    if (full_url) xmlFree(full_url);
  }
  }

  context_set_array_of_arrays(&ctx, "images", image_matrix, image_count,
                inner_counts);

  char *rendered = render_template("images.html", &ctx);
  if (rendered) {
  send_response(rendered);
  free(rendered);
  } else {
  send_response("<h1>Error rendering image results</h1>");
  }

  if (image_matrix) {
  for (int i = 0; i < image_count; i++) {
    for (int j = 0; j < 4; j++) {
    free(image_matrix[i][j]);
    }
    free(image_matrix[i]);
  }
  free(image_matrix);
  }
  if (inner_counts) {
  free(inner_counts);
  }

  if (xpathObj) xmlXPathFreeObject(xpathObj);
  if (xpathCtx) xmlXPathFreeContext(xpathCtx);
  if (doc) xmlFreeDoc(doc);
  free(html);
  curl_free(encoded_query);
  free(display_query);
  free_context(&ctx);

  return 0;
}