#include "Config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int load_config(const char *filename, Config *config) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    return -1;
  }

  char line[512];
  char section[64] = "";

  while (fgets(line, sizeof(line), file)) {

    line[strcspn(line, "\r\n")] = 0;

    if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
      continue;
    }

    if (line[0] == '[') {
      char *end = strchr(line, ']');
      if (end) {
        *end = '\0';
        snprintf(section, sizeof(section), "%.*s", (int)(sizeof(section) - 1),
                 line + 1);
        section[sizeof(section) - 1] = '\0';
      }
      continue;
    }

    char *delimiter = strchr(line, '=');
    if (delimiter) {
      *delimiter = '\0';
      char *key = line;
      char *value = delimiter + 1;

      while (*key == ' ' || *key == '\t')
        key++;
      while (*value == ' ' || *value == '\t')
        value++;

      char *key_end = key + strlen(key) - 1;
      while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
        *key_end = '\0';
        key_end--;
      }

      char *value_end = value + strlen(value) - 1;
      while (value_end > value && (*value_end == ' ' || *value_end == '\t' ||
                                   *value_end == '"' || *value_end == '\'')) {
        *value_end = '\0';
        value_end--;
      }

      while (*value == '"' || *value == '\'')
        value++;

      if (strcmp(section, "server") == 0) {
        if (strcmp(key, "host") == 0) {
          strncpy(config->host, value, sizeof(config->host) - 1);
          config->host[sizeof(config->host) - 1] = '\0';
        } else if (strcmp(key, "port") == 0) {
          config->port = atoi(value);
        }
      } else if (strcmp(section, "proxy") == 0) {
        if (strcmp(key, "proxy") == 0) {
          strncpy(config->proxy, value, sizeof(config->proxy) - 1);
          config->proxy[sizeof(config->proxy) - 1] = '\0';
        } else if (strcmp(key, "list_file") == 0) {
          strncpy(config->proxy_list_file, value,
                  sizeof(config->proxy_list_file) - 1);
          config->proxy_list_file[sizeof(config->proxy_list_file) - 1] = '\0';
        } else if (strcmp(key, "max_retries") == 0) {
          config->max_proxy_retries = atoi(value);
        } else if (strcmp(key, "randomize_username") == 0) {
          config->randomize_username = atoi(value);
        } else if (strcmp(key, "randomize_password") == 0) {
          config->randomize_password = atoi(value);
        }
      }
    }
  }

  fclose(file);
  return 0;
}
