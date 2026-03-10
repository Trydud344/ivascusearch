#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
  char host[256];
  int port;
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
