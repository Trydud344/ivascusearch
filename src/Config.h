#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
  char host[256];
  int port;
} Config;

int load_config(const char *filename, Config *config);

#endif