#ifndef HTMW_SYNTAX_DIR_H
#define HTMW_SYNTAX_DIR_H

typedef struct syntaxer_list syntaxer_list;

#include "luaw.h"
#include "luax.h"
#include <luax_regex.h>
#include "syntaxer.h"
#include "htmw.h"

// typedef struct htmw_syntax_prefetch_list_s {
//     char *;
//     htmw_syntax_prefetch_list;
// } htmw_syntax_prefetch_list;

void htmw_process_syntax_directive_value(lua_State *L, syntaxer_list *routers, const char *name, const char *buffer);
void htmw_prefetch_syntax_drectives(syntaxer_list *routers);

#endif