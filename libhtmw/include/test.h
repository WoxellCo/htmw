#ifndef TEST_H
#define TEST_H

#include "htmw.h"

void debug_syntax_directives(const syntaxer_list *syntax_routers);
void luax_test();
void debug_htmw_crv(const compiled_regex_vector *v, size_t n);
int pcre2t_test();
void debug_lua_stack(lua_State *L);
char *str_endl_to_sp(const char *in, size_t n);
void debug_template_arg(const ht_template_arg *arg);
void debug_template(const ht_template *t);
void debug_templates(const htmw_context *ctx);
void strplex_print(const strplex *sp);

#endif