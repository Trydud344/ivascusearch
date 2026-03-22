#include "../Utility/Unescape.h"
#include "../Utility/XmlHelper.h"
#include "Config.h"
#include "Scraping.h"
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <stdlib.h>
#include <string.h>

xmlXPathContextPtr create_xpath_context(xmlDocPtr doc) {
  return xmlXPathNewContext(doc);
}

void free_xpath_objects(xmlXPathContextPtr ctx, xmlXPathObjectPtr obj) {
  if (obj)
    xmlXPathFreeObject(obj);
  if (ctx)
    xmlXPathFreeContext(ctx);
}

SearchResult *alloc_results_array(int capacity, int max_results) {
  int count = capacity < max_results ? capacity : max_results;
  return xml_result_alloc(capacity, count);
}

void assign_result(SearchResult *result, char *url, char *title, char *snippet,
                   int unescape) {
  result->url = unescape ? unescape_search_url(url) : strdup(url ? url : "");
  result->title = strdup(title ? title : "No Title");
  result->snippet = strdup(snippet ? snippet : "");
}

void free_xml_node_list(char *title, char *url, char *snippet) {
  if (title)
    xmlFree(title);
  if (url)
    xmlFree(url);
  if (snippet)
    xmlFree(snippet);
}

static int parse_ddg_lite(const char *engine_name, xmlDocPtr doc,
                          SearchResult **out_results, int max_results) {
  (void)engine_name;
  int found_count = 0;

  xmlXPathContextPtr ctx = create_xpath_context(doc);
  if (!ctx)
    return 0;

  xmlXPathObjectPtr obj =
      xml_xpath_eval(ctx, "//tr[not(contains(@class, "
                          "'result-sponsored'))]//a[@class='result-link']");

  if (!obj || !obj->nodesetval || obj->nodesetval->nodeNr == 0) {
    free_xpath_objects(ctx, obj);
    return 0;
  }

  int num_links = obj->nodesetval->nodeNr;
  *out_results = alloc_results_array(num_links, max_results);
  if (!*out_results) {
    free_xpath_objects(ctx, obj);
    return 0;
  }

  for (int i = 0; i < num_links && found_count < max_results; i++) {
    xmlNodePtr link_node = obj->nodesetval->nodeTab[i];
    char *title = xml_node_content(link_node);
    char *url = (char *)xmlGetProp(link_node, (xmlChar *)"href");
    char *snippet_text = NULL;

    xmlNodePtr current = link_node->parent;
    while (current && xmlStrcasecmp(current->name, (const xmlChar *)"tr") != 0)
      current = current->parent;

    if (current && current->next) {
      xmlNodePtr snippet_row = current->next;
      while (snippet_row &&
             xmlStrcasecmp(snippet_row->name, (const xmlChar *)"tr") != 0)
        snippet_row = snippet_row->next;
      if (snippet_row) {
        ctx->node = snippet_row;
        xmlXPathObjectPtr s_obj =
            xml_xpath_eval(ctx, ".//td[@class='result-snippet']");
        if (s_obj && s_obj->nodesetval && s_obj->nodesetval->nodeNr > 0)
          snippet_text = xml_node_content(s_obj->nodesetval->nodeTab[0]);
        if (s_obj)
          xmlXPathFreeObject(s_obj);
        ctx->node = NULL;
      }
    }

    assign_result(&(*out_results)[found_count], url, title, snippet_text, 1);
    free_xml_node_list(title, url, snippet_text);
    found_count++;
  }

  free_xpath_objects(ctx, obj);
  return found_count;
}

static int parse_startpage(const char *engine_name, xmlDocPtr doc,
                           SearchResult **out_results, int max_results) {
  (void)engine_name;
  int found_count = 0;

  xmlXPathContextPtr ctx = create_xpath_context(doc);
  if (!ctx)
    return 0;

  xmlXPathObjectPtr obj =
      xml_xpath_eval(ctx, "//div[contains(@class, 'result')]");

  if (!obj || !obj->nodesetval || obj->nodesetval->nodeNr == 0) {
    free_xpath_objects(ctx, obj);
    return 0;
  }

  int num_results = obj->nodesetval->nodeNr;
  *out_results = alloc_results_array(num_results, max_results);
  if (!*out_results) {
    free_xpath_objects(ctx, obj);
    return 0;
  }

  for (int i = 0; i < num_results && found_count < max_results; i++) {
    xmlNodePtr result_node = obj->nodesetval->nodeTab[i];
    ctx->node = result_node;

    xmlXPathObjectPtr link_obj =
        xml_xpath_eval(ctx, ".//a[contains(@class, 'result-link')]");
    char *url =
        (link_obj && link_obj->nodesetval && link_obj->nodesetval->nodeNr > 0)
            ? (char *)xmlGetProp(link_obj->nodesetval->nodeTab[0],
                                 (xmlChar *)"href")
            : NULL;

    xmlXPathObjectPtr title_obj =
        xml_xpath_eval(ctx, ".//h2[contains(@class, 'wgl-title')]");
    char *title = (title_obj && title_obj->nodesetval &&
                   title_obj->nodesetval->nodeNr > 0)
                      ? xml_node_content(title_obj->nodesetval->nodeTab[0])
                      : NULL;

    xmlXPathObjectPtr snippet_obj =
        xml_xpath_eval(ctx, ".//p[contains(@class, 'description')]");
    char *snippet_text =
        (snippet_obj && snippet_obj->nodesetval &&
         snippet_obj->nodesetval->nodeNr > 0)
            ? xml_node_content(snippet_obj->nodesetval->nodeTab[0])
            : NULL;

    if (url && title) {
      assign_result(&(*out_results)[found_count], url, title, snippet_text, 0);
      found_count++;
    }

    free_xml_node_list(title, url, snippet_text);
    if (link_obj)
      xmlXPathFreeObject(link_obj);
    if (title_obj)
      xmlXPathFreeObject(title_obj);
    if (snippet_obj)
      xmlXPathFreeObject(snippet_obj);
  }

  ctx->node = NULL;
  free_xpath_objects(ctx, obj);
  return found_count;
}

static int parse_yahoo(const char *engine_name, xmlDocPtr doc,
                       SearchResult **out_results, int max_results) {
  (void)engine_name;
  int found_count = 0;

  xmlXPathContextPtr ctx = create_xpath_context(doc);
  if (!ctx)
    return 0;

  xmlXPathObjectPtr obj =
      xml_xpath_eval(ctx, "//div[contains(@class, 'algo-sr')]");

  if (!obj || !obj->nodesetval || obj->nodesetval->nodeNr == 0) {
    free_xpath_objects(ctx, obj);
    return 0;
  }

  int num_results = obj->nodesetval->nodeNr;
  *out_results = alloc_results_array(num_results, max_results);
  if (!*out_results) {
    free_xpath_objects(ctx, obj);
    return 0;
  }

  for (int i = 0; i < num_results && found_count < max_results; i++) {
    xmlNodePtr result_node = obj->nodesetval->nodeTab[i];
    ctx->node = result_node;

    xmlXPathObjectPtr link_obj = xml_xpath_eval(
        ctx, ".//div[contains(@class, 'compTitle')]//a[@target='_blank']");
    char *url =
        (link_obj && link_obj->nodesetval && link_obj->nodesetval->nodeNr > 0)
            ? (char *)xmlGetProp(link_obj->nodesetval->nodeTab[0],
                                 (xmlChar *)"href")
            : NULL;

    xmlXPathObjectPtr title_obj =
        xml_xpath_eval(ctx, ".//h3[contains(@class, 'title')]");
    char *title = (title_obj && title_obj->nodesetval &&
                   title_obj->nodesetval->nodeNr > 0)
                      ? xml_node_content(title_obj->nodesetval->nodeTab[0])
                      : NULL;

    xmlXPathObjectPtr snippet_obj =
        xml_xpath_eval(ctx, ".//div[contains(@class, 'compText')]//p");
    char *snippet_text =
        (snippet_obj && snippet_obj->nodesetval &&
         snippet_obj->nodesetval->nodeNr > 0)
            ? xml_node_content(snippet_obj->nodesetval->nodeTab[0])
            : NULL;

    if (url && title) {
      assign_result(&(*out_results)[found_count], url, title, snippet_text, 1);
      found_count++;
    }

    free_xml_node_list(title, url, snippet_text);
    if (link_obj)
      xmlXPathFreeObject(link_obj);
    if (title_obj)
      xmlXPathFreeObject(title_obj);
    if (snippet_obj)
      xmlXPathFreeObject(snippet_obj);
  }

  ctx->node = NULL;
  free_xpath_objects(ctx, obj);
  return found_count;
}

static int parse_mojeek(const char *engine_name, xmlDocPtr doc,
                        SearchResult **out_results, int max_results) {
  (void)engine_name;
  int found_count = 0;

  xmlXPathContextPtr ctx = create_xpath_context(doc);
  if (!ctx)
    return 0;

  xmlXPathObjectPtr obj =
      xml_xpath_eval(ctx, "//ul[@class='results-standard']/li[starts-with(@class, 'r')]");

  if (!obj || !obj->nodesetval || obj->nodesetval->nodeNr == 0) {
    free_xpath_objects(ctx, obj);
    return 0;
  }

  int num_results = obj->nodesetval->nodeNr;
  *out_results = alloc_results_array(num_results, max_results);
  if (!*out_results) {
    free_xpath_objects(ctx, obj);
    return 0;
  }

  for (int i = 0; i < num_results && found_count < max_results; i++) {
    xmlNodePtr result_node = obj->nodesetval->nodeTab[i];
    ctx->node = result_node;

    xmlXPathObjectPtr link_obj =
        xml_xpath_eval(ctx, ".//a[@class='title']");
    char *url =
        (link_obj && link_obj->nodesetval && link_obj->nodesetval->nodeNr > 0)
            ? (char *)xmlGetProp(link_obj->nodesetval->nodeTab[0],
                                 (xmlChar *)"href")
            : NULL;

    char *title = (link_obj && link_obj->nodesetval &&
                   link_obj->nodesetval->nodeNr > 0)
                      ? xml_node_content(link_obj->nodesetval->nodeTab[0])
                      : NULL;

    xmlXPathObjectPtr snippet_obj = xml_xpath_eval(ctx, ".//p[@class='s']");
    char *snippet_text =
        (snippet_obj && snippet_obj->nodesetval &&
         snippet_obj->nodesetval->nodeNr > 0)
            ? xml_node_content(snippet_obj->nodesetval->nodeTab[0])
            : NULL;

    if (url && title) {
      assign_result(&(*out_results)[found_count], url, title, snippet_text, 0);
      found_count++;
    }

    free_xml_node_list(title, url, snippet_text);
    if (link_obj)
      xmlXPathFreeObject(link_obj);
    if (snippet_obj)
      xmlXPathFreeObject(snippet_obj);
  }

  ctx->node = NULL;
  free_xpath_objects(ctx, obj);
  return found_count;
}

static int parse_yahoo(const char *engine_name, xmlDocPtr doc,
                       SearchResult **out_results, int max_results);
static int parse_mojeek(const char *engine_name, xmlDocPtr doc,
                        SearchResult **out_results, int max_results);

SearchEngine ENGINE_REGISTRY[] = {
    {.id = "ddg",
     .name = "DuckDuckGo Lite",
     .base_url = "https://lite.duckduckgo.com/lite/?q=",
     .host_header = "lite.duckduckgo.com",
     .referer = "https://lite.duckduckgo.com/",
     .page_param = "s",
     .page_multiplier = 30,
     .page_base = 0,
     .parser = parse_ddg_lite,
     .enabled = 1},
    {.id = "startpage",
     .name = "Startpage",
     .base_url = "https://www.startpage.com/sp/search?q=",
     .host_header = "www.startpage.com",
     .referer = "https://www.startpage.com/",
     .page_param = "page",
     .page_multiplier = 1,
     .page_base = 1,
     .parser = parse_startpage,
     .enabled = 1},
    {.id = "yahoo",
     .name = "Yahoo",
     .base_url = "https://search.yahoo.com/search?p=",
     .host_header = "search.yahoo.com",
     .referer = "https://search.yahoo.com/",
     .page_param = "b",
     .page_multiplier = 10,
     .page_base = 1,
     .parser = parse_yahoo,
     .enabled = 1},
    {.id = "mojeek",
     .name = "Mojeek",
     .base_url = "https://www.mojeek.com/search?q=",
     .host_header = "www.mojeek.com",
     .referer = "https://www.mojeek.com/",
     .page_param = "s",
     .page_multiplier = 10,
     .page_base = 1,
     .parser = parse_mojeek,
     .enabled = 1}};

const int ENGINE_COUNT = sizeof(ENGINE_REGISTRY) / sizeof(SearchEngine);

static int engine_id_compare(const char *engine_id, const char *config_id) {
  while (*engine_id && *config_id) {
    char e = *engine_id;
    char c = *config_id;
    if (e >= 'A' && e <= 'Z')
      e = e - 'A' + 'a';
    if (c >= 'A' && c <= 'Z')
      c = c - 'A' + 'a';
    if (e != c)
      return 0;
    engine_id++;
    config_id++;
  }
  return *engine_id == *config_id;
}

void apply_engines_config(const char *engines_str) {
  if (!engines_str || engines_str[0] == '\0') {
    for (int i = 0; i < ENGINE_COUNT; i++) {
      ENGINE_REGISTRY[i].enabled = 1;
    }
    return;
  }

  for (int i = 0; i < ENGINE_COUNT; i++) {
    ENGINE_REGISTRY[i].enabled = 0;
  }

  char *copy = strdup(engines_str);
  if (!copy)
    return;

  char *saveptr;
  char *token = strtok_r(copy, ",", &saveptr);

  while (token) {
    while (*token == ' ' || *token == '\t')
      token++;

    if (strcmp(token, "*") == 0) {
      for (int i = 0; i < ENGINE_COUNT; i++) {
        ENGINE_REGISTRY[i].enabled = 1;
      }
    } else if (token[0] == '-' && token[1] != '\0') {
      char *engine_id = token + 1;
      int found = 0;
      for (int i = 0; i < ENGINE_COUNT; i++) {
        if (engine_id_compare(ENGINE_REGISTRY[i].id, engine_id)) {
          ENGINE_REGISTRY[i].enabled = 0;
          found = 1;
          break;
        }
      }
      if (!found) {
        fprintf(stderr, "[WARN] Unknown engine: %s\n", engine_id);
      }
    } else {
      int found = 0;
      for (int i = 0; i < ENGINE_COUNT; i++) {
        if (engine_id_compare(ENGINE_REGISTRY[i].id, token)) {
          ENGINE_REGISTRY[i].enabled = 1;
          found = 1;
          break;
        }
      }
      if (!found) {
        fprintf(stderr, "[WARN] Unknown engine: %s\n", token);
      }
    }

    token = strtok_r(NULL, ",", &saveptr);
  }

  free(copy);
}
