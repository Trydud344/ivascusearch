#include <beaker.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <stdio.h>
#include <stdlib.h>

#include "Config.h"
#include "Routes/Home.h"
#include "Routes/Images.h"
#include "Routes/Search.h"

int handle_opensearch(UrlParams *params) {
    (void)params;
    serve_static_file_with_mime("opensearch.xml", "application/opensearchdescription+xml");
    return 0;
}

int main() {
  LIBXML_TEST_VERSION
  xmlInitParser();

  curl_global_init(CURL_GLOBAL_DEFAULT);

  Config config = {.host = "0.0.0.0", .port = 5000};
  
  if (load_config("config.ini", &config) != 0) {
    fprintf(stderr, "Warning: Could not load config file, using defaults\n");
  }

  set_handler("/", home_handler);
  set_handler("/opensearch.xml", handle_opensearch);
  set_handler("/search", results_handler);
  set_handler("/images", images_handler);

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
  return EXIT_SUCCESS;
}