/*
    (c) Woxell Co.
*/
#ifndef HTMW_ENV_H
#define HTMW_ENV_H

#include <lua.h>
#include "env.h"

void htmw_init_env(lua_State *const L, const char *const name);
void htmw_load_env(lua_State *const L, const env_table *const table, const char *const name);
void htmw_get_env_table(lua_State *const L, const char *const name);

typedef struct {
    const char *const key;
    const char *const value;
    const size_t max_key_length;
    const size_t max_value_length;
} htmw_arg;

#define htmw_nullarg ((htmw_arg){ NULL, NULL, 0, 0 })
#define htmw_args(...) ((htmw_arg[]){__VA_ARGS__, htmw_nullarg})
#define htmw_is_nullarg(arg) ((arg).key == NULL && (arg).value == NULL)

void htmw_args_set(
    lua_State *const L,
    const char *const name,
    #ifndef _MSC_VER
    const htmw_arg args[static const 1]
    #else
    const htmw_arg *const args
    #endif
);

#endif
