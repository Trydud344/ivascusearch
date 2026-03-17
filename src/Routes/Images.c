#include "Images.h"
#include "../Scraping/ImageScraping.h"
#include "../Utility/Unescape.h"
#include "Config.h"

int images_handler(UrlParams *params) {
  TemplateContext ctx = new_context();
  char *raw_query = "";
  int page = 1;

  if (params) {
    for (int i = 0; i < params->count; i++) {
      if (strcmp(params->params[i].key, "q") == 0) {
        raw_query = params->params[i].value;
      } else if (strcmp(params->params[i].key, "p") == 0) {
        int parsed = atoi(params->params[i].value);
        if (parsed > 1)
          page = parsed;
      }
    }
  }

  char page_str[16], prev_str[16], next_str[16];
  snprintf(page_str, sizeof(page_str), "%d", page);
  snprintf(prev_str, sizeof(prev_str), "%d", page > 1 ? page - 1 : 0);
  snprintf(next_str, sizeof(next_str), "%d", page + 1);

  context_set(&ctx, "query", raw_query);
  context_set(&ctx, "page", page_str);
  context_set(&ctx, "prev_page", prev_str);
  context_set(&ctx, "next_page", next_str);

  char *display_query = url_decode_query(raw_query);
  context_set(&ctx, "query", display_query);

  if (!raw_query || strlen(raw_query) == 0) {
    send_response("<h1>No query provided</h1>");
    if (display_query)
      free(display_query);
    free_context(&ctx);
    return -1;
  }

  ImageResult *results = NULL;
  int result_count = 0;

  if (scrape_images(raw_query, page, &results, &result_count) != 0 ||
      !results) {
    send_response("<h1>Error fetching images</h1>");
    free(display_query);
    free_context(&ctx);
    return -1;
  }

  char ***image_matrix = malloc(sizeof(char **) * result_count);
  int *inner_counts = malloc(sizeof(int) * result_count);

  if (!image_matrix || !inner_counts) {
    if (image_matrix)
      free(image_matrix);
    if (inner_counts)
      free(inner_counts);
    free_image_results(results, result_count);
    free(display_query);
    free_context(&ctx);
    return -1;
  }

  for (int i = 0; i < result_count; i++) {
    image_matrix[i] = malloc(sizeof(char *) * IMAGE_RESULT_FIELDS);
    image_matrix[i][0] = strdup(results[i].thumbnail_url);
    image_matrix[i][1] = strdup(results[i].title);
    image_matrix[i][2] = strdup(results[i].page_url);
    image_matrix[i][3] = strdup(results[i].full_url);
    inner_counts[i] = IMAGE_RESULT_FIELDS;
  }

  context_set_array_of_arrays(&ctx, "images", image_matrix, result_count,
                              inner_counts);

  char *rendered = render_template("images.html", &ctx);
  if (rendered) {
    send_response(rendered);
    free(rendered);
  } else {
    send_response("<h1>Error rendering image results</h1>");
  }

  for (int i = 0; i < result_count; i++) {
    for (int j = 0; j < IMAGE_RESULT_FIELDS; j++)
      free(image_matrix[i][j]);
    free(image_matrix[i]);
  }
  free(image_matrix);
  free(inner_counts);

  free_image_results(results, result_count);
  free(display_query);
  free_context(&ctx);

  return 0;
}
