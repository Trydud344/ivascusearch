#ifndef PROXY_H
#define PROXY_H

#include <curl/curl.h>

typedef enum { PROXY_HTTP, PROXY_SOCKS4, PROXY_SOCKS5 } ProxyType;

typedef struct {
  ProxyType type;
  char host[256];
  int port;
  char username[64];
  char password[64];
  int failures;
} Proxy;

extern Proxy *proxy_list;
extern int proxy_count;
extern int max_proxy_retries;
extern int randomize_username;
extern int randomize_password;
extern char proxy_url[512];

int load_proxy_list(const char *filename);
void free_proxy_list(void);
Proxy *get_random_proxy(void);
void record_proxy_failure(Proxy *proxy);
void apply_proxy_settings(CURL *curl);
void set_proxy_config(const char *proxy_str, int rand_user, int rand_pass);

#endif
