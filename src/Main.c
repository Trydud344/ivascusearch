#include <beaker.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "Cache/Cache.h"
#include "Config.h"
#include "Infobox/Wikipedia.h"
#include "Proxy/Proxy.h"
#include "Routes/Home.h"
#include "Routes/ImageProxy.h"
#include "Routes/Images.h"
#include "Routes/Search.h"
#include "Scraping/Scraping.h"

int handle_opensearch(UrlParams *params) {
  (void)params;
  serve_static_file_with_mime("opensearch.xml",
                              "application/opensearchdescription+xml");
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

  Config cfg = {.host = DEFAULT_HOST,
                .port = DEFAULT_PORT,
                .proxy = "",
                .proxy_list_file = "",
                .max_proxy_retries = DEFAULT_MAX_PROXY_RETRIES,
                .randomize_username = 0,
                .randomize_password = 0,
                .cache_dir = DEFAULT_CACHE_DIR,
                .cache_ttl_search = DEFAULT_CACHE_TTL_SEARCH,
                .cache_ttl_infobox = DEFAULT_CACHE_TTL_INFOBOX};

  if (load_config("config.ini", &cfg) != 0) {
    fprintf(stderr, "[WARN] Could not load config file, using defaults\n");
  }

  if (cache_init(cfg.cache_dir) != 0) {
    fprintf(stderr,
            "[WARN] Failed to initialize cache, continuing without caching\n");
  } else {
    fprintf(stderr, "[INFO] Cache initialized at %s\n", cfg.cache_dir);
    cache_cleanup(cfg.cache_ttl_search);
  }

  set_cache_ttl_search(cfg.cache_ttl_search);
  set_cache_ttl_infobox(cfg.cache_ttl_infobox);

  if (cfg.proxy_list_file[0] != '\0') {
    if (load_proxy_list(cfg.proxy_list_file) < 0) {
      fprintf(stderr,
              "[WARN] Failed to load proxy list, continuing without proxies\n");
    }
  }

  max_proxy_retries = cfg.max_proxy_retries;
  set_proxy_config(cfg.proxy, cfg.randomize_username, cfg.randomize_password);

  if (proxy_url[0] != '\0') {
    fprintf(stderr, "[INFO] Using proxy: %s\n", proxy_url);
  } else if (proxy_count > 0) {
    fprintf(stderr, "[INFO] Using %d proxies from %s\n", proxy_count,
            cfg.proxy_list_file);
  }

  set_handler("/", home_handler);
  set_handler("/opensearch.xml", handle_opensearch);
  set_handler("/search", results_handler);
  set_handler("/images", images_handler);
  set_handler("/proxy", image_proxy_handler);

  fprintf(stderr, "[INFO] Starting Omnisearch on %s:%d\n", cfg.host, cfg.port);

  int result = beaker_run(cfg.host, cfg.port);

  if (result != 0) {
    fprintf(stderr, "[ERROR] Beaker server failed to start.\n");
    curl_global_cleanup();
    xmlCleanupParser();
    return EXIT_FAILURE;
  }

  curl_global_cleanup();
  xmlCleanupParser();
  free_proxy_list();
  cache_shutdown();
  return EXIT_SUCCESS;
}
