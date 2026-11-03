/*
    (c) Woxell Co.
*/
#ifndef HTMW_ENV_H
#define HTMW_ENV_H

#include <lua.h>
#include "env.h"

void htmw_init_env(lua_State *const L, const char *const name);
void htmw_load_env(lua_State *const L, const env_table *const table, const char *const name);

#endif
