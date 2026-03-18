#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_HOST "0.0.0.0"
#define DEFAULT_PORT 5000
#define DEFAULT_CACHE_DIR "/tmp/omnisearch_cache"
#define DEFAULT_CACHE_TTL_SEARCH 3600
#define DEFAULT_CACHE_TTL_INFOBOX 86400
#define DEFAULT_MAX_PROXY_RETRIES 3

#define BUFFER_SIZE_SMALL 256
#define BUFFER_SIZE_MEDIUM 512
#define BUFFER_SIZE_LARGE 1024
#define BUFFER_SIZE_XLARGE 2048

#define INITIAL_BUFFER_SIZE 16384

#define WIKI_SUMMARY_MAX_CHARS 300

#define MD5_HASH_LEN 32
#define HEX_CHARS "0123456789abcdef"

#define INFOBOX_FIELD_COUNT 4
#define MAX_RESULTS_PER_ENGINE 10

#define CURL_TIMEOUT_SECS 15L
#define CURL_DNS_TIMEOUT_SECS 300L

#define BING_IMAGE_URL "https://www.bing.com/images/search"
#define IMAGE_RESULTS_PER_PAGE 32
#define IMAGE_RESULT_FIELDS 4

typedef struct {
  char host[256];
  int port;
  char domain[256];
  char proxy[256];
  char proxy_list_file[256];
  int max_proxy_retries;
  int randomize_username;
  int randomize_password;
  char cache_dir[512];
  int cache_ttl_search;
  int cache_ttl_infobox;
} Config;

int load_config(const char *filename, Config *config);

#endif
