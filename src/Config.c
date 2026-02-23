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
        snprintf(section, sizeof(section), "%.*s", (int)(sizeof(section) - 1), line + 1);
        section[sizeof(section) - 1] = '\0';
      }
      continue;
    }

    char *delimiter = strchr(line, '=');
    if (delimiter) {
      *delimiter = '\0';
      char *key = line;
      char *value = delimiter + 1;

      while (*key == ' ' || *key == '\t') key++;
      while (*value == ' ' || *value == '\t') value++;

      char *key_end = key + strlen(key) - 1;
      while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
        *key_end = '\0';
        key_end--;
      }

      char *value_end = value + strlen(value) - 1;
      while (value_end > value && (*value_end == ' ' || *value_end == '\t')) {
        *value_end = '\0';
        value_end--;
      }

      if (strcmp(section, "server") == 0) {
        if (strcmp(key, "host") == 0) {
          strncpy(config->host, value, sizeof(config->host) - 1);
          config->host[sizeof(config->host) - 1] = '\0';
        } else if (strcmp(key, "port") == 0) {
          config->port = atoi(value);
        }
      }
    }
  }

  fclose(file);
  return 0;
}