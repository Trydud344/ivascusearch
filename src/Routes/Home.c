#include "Home.h"
#include <stdlib.h>

int home_handler(UrlParams *params) {
  (void)params;
  TemplateContext ctx = new_context();
  char *rendered_html = render_template("home.html", &ctx);
  send_response(rendered_html);

  free(rendered_html);
  free_context(&ctx);

  return 0;
}
