#include "ImageScraping.h"
#include "../Utility/HttpClient.h"
#include "Config.h"
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *build_proxy_url(const char *image_url) {
  if (!image_url)
    return NULL;

  char *proxy_url = NULL;
  CURL *curl = curl_easy_init();
  if (curl) {
    char *encoded = curl_easy_escape(curl, (char *)image_url, 0);
    if (encoded) {
      size_t len = strlen("/proxy?url=") + strlen(encoded) + 1;
      proxy_url = malloc(len);
      if (proxy_url)
        snprintf(proxy_url, len, "/proxy?url=%s", encoded);
      curl_free(encoded);
    }
    curl_easy_cleanup(curl);
  }

  return proxy_url;
}

static int parse_image_node(xmlNodePtr node, ImageResult *result) {
  xmlNodePtr img_node = NULL;
  xmlNodePtr tit_node = NULL;
  xmlNodePtr des_node = NULL;
  xmlNodePtr thumb_link = NULL;

  for (xmlNodePtr child = node->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE)
      continue;

    if (xmlStrcmp(child->name, (const xmlChar *)"a") == 0) {
      xmlChar *class = xmlGetProp(child, (const xmlChar *)"class");
      if (class) {
        if (xmlStrstr(class, (const xmlChar *)"thumb") != NULL) {
          thumb_link = child;
          for (xmlNodePtr thumb_child = child->children; thumb_child;
               thumb_child = thumb_child->next) {
            if (xmlStrcmp(thumb_child->name, (const xmlChar *)"div") == 0) {
              xmlChar *div_class =
                  xmlGetProp(thumb_child, (const xmlChar *)"class");
              if (div_class &&
                  xmlStrcmp(div_class, (const xmlChar *)"cico") == 0) {
                for (xmlNodePtr cico_child = thumb_child->children; cico_child;
                     cico_child = cico_child->next) {
                  if (xmlStrcmp(cico_child->name, (const xmlChar *)"img") ==
                      0) {
                    img_node = cico_child;
                    break;
                  }
                }
              }
              if (div_class)
                xmlFree(div_class);
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
        for (xmlNodePtr meta_child = child->children; meta_child;
             meta_child = meta_child->next) {
          if (xmlStrcmp(meta_child->name, (const xmlChar *)"div") == 0) {
            xmlChar *div_class =
                xmlGetProp(meta_child, (const xmlChar *)"class");
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
            if (a_class)
              xmlFree(a_class);
          }
        }
      }
      if (class)
        xmlFree(class);
    }
  }

  xmlChar *iurl =
      img_node ? xmlGetProp(img_node, (const xmlChar *)"src") : NULL;
  xmlChar *full_url =
      thumb_link ? xmlGetProp(thumb_link, (const xmlChar *)"href") : NULL;
  xmlChar *title = des_node ? xmlNodeGetContent(des_node)
                            : (tit_node ? xmlNodeGetContent(tit_node) : NULL);
  xmlChar *rurl =
      tit_node ? xmlGetProp(tit_node, (const xmlChar *)"href") : NULL;

  if (!iurl || strlen((char *)iurl) == 0) {
    if (iurl)
      xmlFree(iurl);
    if (title)
      xmlFree(title);
    if (rurl)
      xmlFree(rurl);
    if (full_url)
      xmlFree(full_url);
    return 0;
  }

  char *proxy_url = build_proxy_url((char *)iurl);
  result->thumbnail_url = proxy_url ? strdup(proxy_url) : strdup((char *)iurl);
  free(proxy_url);
  result->title = strdup(title ? (char *)title : "Image");
  result->page_url = strdup(rurl ? (char *)rurl : "#");
  result->full_url = strdup(full_url ? (char *)full_url : "#");

  if (iurl)
    xmlFree(iurl);
  if (title)
    xmlFree(title);
  if (rurl)
    xmlFree(rurl);
  if (full_url)
    xmlFree(full_url);

  return 1;
}

int scrape_images(const char *query, int page, ImageResult **out_results,
                  int *out_count) {
  *out_results = NULL;
  *out_count = 0;

  if (!query || strlen(query) == 0)
    return -1;

  CURL *tmp = curl_easy_init();
  if (!tmp)
    return -1;

  char *encoded_query = curl_easy_escape(tmp, query, 0);
  curl_easy_cleanup(tmp);

  if (!encoded_query)
    return -1;

  char url[BUFFER_SIZE_LARGE];
  int first = (page - 1) * IMAGE_RESULTS_PER_PAGE + 1;
  snprintf(url, sizeof(url), "%s?q=%s&first=%d", BING_IMAGE_URL, encoded_query,
           first);
  free(encoded_query);

  HttpResponse resp = http_get(
      url,
      "Mozilla/5.0 (Windows NT 6.1; WOW64; Trident/7.0; rv:11.0) like Gecko");
  if (!resp.memory) {
    return -1;
  }

  htmlDocPtr doc = htmlReadMemory(resp.memory, resp.size, NULL, NULL,
                                  HTML_PARSE_RECOVER | HTML_PARSE_NOERROR);
  if (!doc) {
    http_response_free(&resp);
    return -1;
  }

  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
  if (!xpathCtx) {
    xmlFreeDoc(doc);
    http_response_free(&resp);
    return -1;
  }

  xmlXPathObjectPtr xpathObj =
      xmlXPathEvalExpression((const xmlChar *)"//div[@class='item']", xpathCtx);

  if (!xpathObj || !xpathObj->nodesetval) {
    if (xpathObj)
      xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    http_response_free(&resp);
    return 0;
  }

  int nodes = xpathObj->nodesetval->nodeNr;
  int max_images =
      (nodes < IMAGE_RESULTS_PER_PAGE) ? nodes : IMAGE_RESULTS_PER_PAGE;

  ImageResult *results = malloc(sizeof(ImageResult) * max_images);
  if (!results) {
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    http_response_free(&resp);
    return -1;
  }

  int count = 0;
  for (int i = 0; i < nodes && count < IMAGE_RESULTS_PER_PAGE; i++) {
    xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
    if (parse_image_node(node, &results[count])) {
      count++;
    }
  }

  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  xmlFreeDoc(doc);
  http_response_free(&resp);

  *out_results = results;
  *out_count = count;
  return 0;
}

void free_image_results(ImageResult *results, int count) {
  if (!results)
    return;

  for (int i = 0; i < count; i++) {
    free(results[i].thumbnail_url);
    free(results[i].title);
    free(results[i].page_url);
    free(results[i].full_url);
  }
  free(results);
}
