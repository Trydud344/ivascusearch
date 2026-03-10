#include "Calculator.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char logic_log[4096];

typedef struct {
  const char *buffer;
  int pos;
} Parser;

static double parse_expression(Parser *p);

static void skip_ws(Parser *p) {
  while (p->buffer[p->pos] == ' ')
    p->pos++;
}

static double parse_factor(Parser *p) {
  skip_ws(p);
  if (p->buffer[p->pos] == '-') {
    p->pos++;
    return -parse_factor(p);
  }
  if (p->buffer[p->pos] == '(') {
    p->pos++;
    double res = parse_expression(p);
    if (p->buffer[p->pos] == ')')
      p->pos++;
    return res;
  }
  char *endptr;
  double val = strtod(&p->buffer[p->pos], &endptr);
  p->pos = (int)(endptr - p->buffer);
  return val;
}

static double parse_term(Parser *p) {
  double left = parse_factor(p);
  while (1) {
    skip_ws(p);
    char op = p->buffer[p->pos];
    if (op == '*' || op == '/') {
      p->pos++;
      double right = parse_factor(p);
      double old = left;
      left = (op == '*') ? left * right : left / right;

      char step[256];

      snprintf(step, sizeof(step), "<div>%g %c %g = <b>%g</b></div>", old, op,
               right, left);
      strncat(logic_log, step, sizeof(logic_log) - strlen(logic_log) - 1);
    } else
      break;
  }
  return left;
}

static double parse_expression(Parser *p) {
  double left = parse_term(p);
  while (1) {
    skip_ws(p);
    char op = p->buffer[p->pos];
    if (op == '+' || op == '-') {
      p->pos++;
      double right = parse_term(p);
      double old = left;
      left = (op == '+') ? left + right : left - right;

      char step[256];

      snprintf(step, sizeof(step), "<div>%g %c %g = <b>%g</b></div>", old, op,
               right, left);
      strncat(logic_log, step, sizeof(logic_log) - strlen(logic_log) - 1);
    } else
      break;
  }
  return left;
}

double evaluate(const char *expr) {
  logic_log[0] = '\0';
  if (!expr || strlen(expr) == 0)
    return 0.0;
  Parser p = {expr, 0};
  return parse_expression(&p);
}

InfoBox fetch_calc_data(char *math_input) {
  InfoBox info = {NULL, NULL, NULL, NULL};
  if (!math_input)
    return info;

  double result = evaluate(math_input);

  char html_output[5120];
  snprintf(html_output, sizeof(html_output),
           "<div class='calc-container' style='line-height: 1.6;'>"
           "%s"
           "<div style='margin-top: 8px; border-top: 1px solid #eee; "
           "padding-top: 8px; font-size: 1.2em;'>"
           "<b>%g</b>"
           "</div>"
           "</div>",
           strlen(logic_log) > 0 ? logic_log : "<div>Constant value</div>",
           result);

  info.title = strdup("Calculation");
  info.extract = strdup(html_output);
  info.thumbnail_url = strdup("/static/calculation.svg");
  info.url = strdup("#");

  return info;
}
