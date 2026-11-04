#include "htmw_env.h"
#include "env.h"
#include "lua.h"
#include <stddef.h>

void htmw_get_env_table(lua_State *const L, const char *const name) {
    if (lua_getglobal(L, name) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, name);
    }
}

void htmw_init_env(lua_State *const L, const char *const name) {
    htmw_get_env_table(L, name);
    lua_pop(L, 1);
}

void htmw_load_env(lua_State *const L, const env_table *const table, const char *const name) {
    htmw_get_env_table(L, name);

    for (size_t i = 0; i < table->length; i++) {
        const env_record record = table->records[i];
        lua_pushstring(L, record.value.c_str);
        lua_setfield(L, -2, record.key.c_str);
    }

    lua_pop(L, 1);
}



static size_t get_strlen(const char *const s, const size_t max_n) {
    size_t n = 0;
    while (s[n] != '\0' && n < max_n) {
        n++;
    }
    return n;
}

void htmw_args_set(
    lua_State *const L,
    const char *const name,
    #ifndef _MSC_VER
    const htmw_arg args[static const 1]
    #else
    const htmw_arg *const args
    #endif
) {
    htmw_get_env_table(L, name);
    size_t n = 0;
    while (!htmw_is_nullarg(args[n])) {
        const htmw_arg *const arg = args + n;
        lua_pushlstring(L, arg->value, get_strlen(arg->value, arg->max_value_length));
        lua_pushlstring(L, arg->key, get_strlen(arg->key, arg->max_key_length));
        lua_settable(L, -3);
        n++;
    }
    lua_pop(L, 1);
}

