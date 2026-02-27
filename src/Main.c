#include <beaker.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "Config.h"
#include "Proxy/Proxy.h"
#include "Scraping/Scraping.h"
#include "Routes/Home.h"
#include "Routes/Images.h"
#include "Routes/ImageProxy.h"
#include "Routes/Search.h"

int handle_opensearch(UrlParams *params) {
    (void)params;
    serve_static_file_with_mime("opensearch.xml", "application/opensearchdescription+xml");
    return 0;
}

int main() {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);

  LIBXML_TEST_VERSION
  xmlInitParser();

  curl_global_init(CURL_GLOBAL_DEFAULT);

  Config config = {
    .host = "0.0.0.0",
    .port = 5000,
    .proxy = "",
    .proxy_list_file = "",
    .max_proxy_retries = 3,
    .randomize_username = 0,
    .randomize_password = 0
  };
  
  if (load_config("config.ini", &config) != 0) {
    fprintf(stderr, "Warning: Could not load config file, using defaults\n");
  }

  if (config.proxy_list_file[0] != '\0') {
    if (load_proxy_list(config.proxy_list_file) < 0) {
      fprintf(stderr, "Warning: Failed to load proxy list, continuing without proxies\n");
    }
  }

  max_proxy_retries = config.max_proxy_retries;
  set_proxy_config(config.proxy, config.randomize_username, config.randomize_password);

  if (proxy_url[0] != '\0') {
    fprintf(stderr, "Using proxy: %s\n", proxy_url);
  } else if (proxy_count > 0) {
    fprintf(stderr, "Using %d proxies from %s\n", proxy_count, config.proxy_list_file);
  }

  set_handler("/", home_handler);
  set_handler("/opensearch.xml", handle_opensearch);
  set_handler("/search", results_handler);
  set_handler("/images", images_handler);
  set_handler("/proxy", image_proxy_handler);

  fprintf(stderr, "Starting Omnisearch on %s:%d\n", config.host, config.port);

  int result = beaker_run(config.host, config.port);

  if (result != 0) {
    fprintf(stderr, "Error: Beaker server failed to start.\n");
    curl_global_cleanup();
    xmlCleanupParser();
    return EXIT_FAILURE;
  }

  curl_global_cleanup();
  xmlCleanupParser();
  free_proxy_list();
  return EXIT_SUCCESS;
}