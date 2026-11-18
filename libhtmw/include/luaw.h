/*
    (c) Woxell Co.
    Lua endpoints to interact with the W api
*/
#ifndef LUAW_H
#define LUAW_H

#include <lua.h>

#include "htmw.h"

extern htmw_context *luaw_ctx;

int luaw_htmw_process(lua_State *L);
int luaw_htmw_postprocess(lua_State *L);

void luaw_init(lua_State *L);

#endif