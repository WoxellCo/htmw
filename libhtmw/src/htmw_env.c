#include "htmw_env.h"
#include "env.h"
#include "lua.h"
#include <stddef.h>

static void htmw_get_env_table(lua_State *const L, const char *const name) {
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
