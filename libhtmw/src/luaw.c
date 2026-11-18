#include "luaw.h"
#include "htmw.h"
#include "lauxlib.h"
#include "lua.h"
#include "str.h"
#include <stddef.h>

int luaw_htmw_process(lua_State *L) {
    if (!lua_isstring(L, 1))
        return 0;

    string s = str_from(lua_tostring(L, 1));
    htmw_context inner_ctx = *luaw_ctx; // TODO: deep copy? (again)
    htmw_preprocess(L, s, &inner_ctx, luaw_ctx->global_paths, luaw_ctx->flags & CTX_NETWORK);
    txt_template_fill_data tfd = htmw_process_txt(&inner_ctx);
    if (tfd.fields_count) {
        size_t inner_args_vect_cursor = 0;
        string out = htmw_compile_txt(&inner_ctx, &tfd, &inner_args_vect_cursor);
        //string p_out = htmw_postprocess(out);
        lua_pushstring(L, out.c_str);
        str_delete(&out);
        str_delete(&s);
        return 1;
    } else {
        //string p_out = htmw_postprocess(inner_ctx.input);
        lua_pushstring(L, inner_ctx.input.c_str);
        str_delete(&s);
        return 1;
    }
}

int luaw_htmw_postprocess(lua_State *L) {
    if (!lua_isstring(L, 1))
        return 0;

    string s = str_from(lua_tostring(L, 1));
    string out = htmw_postprocess(s);

    str_delete(&s);
    lua_pushstring(L, out.c_str);
    str_delete(&out);
    return 1;
}

int luaw_htmw_error(lua_State *L) {
    const char *msg;
    if (lua_isstring(L, 1))
        msg = lua_tostring(L, 1);

    throw_error(msg, E_EXIT);
    return 0;
}

#if 0
int luaw_htmw_sanxml(lua_State *L) {
    if (!lua_isstring(L, 1))
        return 0;

    
    return 1;
}
#endif

int luaw_regex_test(lua_State *const L) {
    if (!lua_istable(L, 1))
        return 0;

    size_t txt_len = 0;
    const char *const txt = luaL_checklstring(L, 2, &txt_len);

    pcre2_code *re = NULL;
    int should_free_re = 0;

    lua_getfield(L, 1, "refId");

    if (lua_isinteger(L, -1)) {
        lua_Integer idx = lua_tointeger(L, -1);
        lua_pop(L, 1);

        if (idx <= 0)
            return luaL_error(L, "invalid regex refId");

        lua_getglobal(L, "___CRV");

        if (lua_type(L, -1) != LUA_TUSERDATA)
            return luaL_error(L, "___CRV is malformed");

        compiled_regex_vector **blcrv =
            (compiled_regex_vector **)lua_touserdata(L, -1);

        if (!blcrv || !*blcrv)
            return luaL_error(L, "___CRV is malformed");

        compiled_regex_vector *lcrv = *blcrv;

        lua_pop(L, 1);

        /*if ((size_t)idx > lcrv->size)
            return luaL_error(L, "regex refId out of range");*/

        re = lcrv->data[idx - 1].rex;

        if (!re)
            return luaL_error(L, "invalid compiled regex");

    } else {
        lua_pop(L, 1);

        lua_getfield(L, 1, "pattern");

        size_t pattern_len = 0;
        const char *pattern =
            luaL_checklstring(L, -1, &pattern_len);

        int error_code;
        PCRE2_SIZE error_offset;

        re = pcre2_compile(
            (PCRE2_SPTR)pattern,
            pattern_len,
            0,
            &error_code,
            &error_offset,
            NULL
        );

        lua_pop(L, 1);

        if (!re) {
            PCRE2_UCHAR error_message[256];

            pcre2_get_error_message(
                error_code,
                error_message,
                sizeof(error_message)
            );

            return luaL_error(
                L,
                "regex compilation failed at offset %zu: %s",
                (size_t)error_offset,
                (char *)error_message
            );
        }

        should_free_re = 1;
    }

    pcre2_match_data *match_data =
        pcre2_match_data_create_from_pattern(re, NULL);

    if (!match_data) {
        if (should_free_re)
            pcre2_code_free(re);

        return luaL_error(L, "failed to allocate regex match data");
    }

    int rc = pcre2_match(
        re,
        (PCRE2_SPTR)txt,
        txt_len,
        0,
        PCRE2_NOTEMPTY | PCRE2_NOTEMPTY_ATSTART,
        match_data,
        NULL
    );

    int result = rc >= 0;

    pcre2_match_data_free(match_data);

    if (should_free_re)
        pcre2_code_free(re);

    lua_pushboolean(L, result);

    return 1;
}

void luaw_init(lua_State *L) {
    luaL_Reg htmw[] = {
        {"process", luaw_htmw_process},
        {"postprocess", luaw_htmw_postprocess},
        {"error", luaw_htmw_error},
        {NULL, NULL}
    };

    lua_newtable(L);
    luaL_setfuncs(L, htmw, 0);

    lua_setglobal(L, "htmw");

    luaL_Reg regex[] = {
        {"test", luaw_regex_test},
        {NULL, NULL},
    };

    lua_newtable(L);
    luaL_setfuncs(L, regex, 0);
    lua_setglobal(L, "Regex");

    lua_newtable(L);
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "pop");
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "continue");
    lua_setglobal(L, "syntax");


    luaL_dostring(L, "function tooptstr(x)if x ~= nil then return tostring(x) else return \"\" end end");
    luaL_dostring(L, "LuaX = {}\n\
\n\
function LuaX.stringify(o)\n\
    local out = ''\n\
    local props = ''\n\
\n\
    for k, v in pairs(o.props) do\n\
        props = props .. string.format([[ %s=\"%s\"]], k, v)\n\
    end\n\
\n\
    local inner_count = 0\n\
    for i, _ in pairs(o.inner) do\n\
        inner_count = i\n\
    end\n\
\n\
    if o.type == nil or o.type == '' then\n\
        local inner = ''\n\
\n\
        for _, i in pairs(o.inner) do\n\
            inner = inner .. tostring(i)\n\
        end\n\
        \n\
        out = out .. inner\n\
    elseif inner_count > 0 then\n\
        local inner = ''\n\
\n\
        for _, i in pairs(o.inner) do\n\
            inner = inner .. tostring(i)\n\
        end\n\
\n\
        out = out .. string.format([[<%s%s>%s</%s>]], o.type, props, inner, o.type)\n\
    else\n\
        out = out .. string.format([[<%s%s/>]], o.type, props)\n\
    end\n\
\n\
    return out\n\
end\n\
\n\
function LuaX.createElement(type, props, inner)\n\
    local t\n\
    for _, e in pairs(type) do\n\
        if e ~= nil then\n\
            t = e\n\
            break\n\
        end\n\
    end\n\
\n\
    local o = {\n\
        [\"type\"] = t,\n\
        [\"props\"] = props,\n\
        [\"inner\"] = inner\n\
    }\n\
\n\
    setmetatable(o, {\n\
        __tostring = function()\n\
            return LuaX.stringify(o)\n\
        end\n\
    })\n\
\n\
    return o\n\
end");
    luaL_dostring(L, "--Regex = {}\n\
Regex.__index = Regex\n\
function Regex.new(pattern, flags, global, refId)\n\
    local x = {\n\
        pattern = pattern,\n\
        flags = flags,\n\
        global = global,\n\
        refId = refId,\n\
        __name = \"Regex\"\n\
    }\n\
    setmetatable(x, Regex)\n\
    return x\n\
end");
    luaL_dostring(L, "function htmw.sanxml(content)\n\
    if content == nil then\n\
        return ""\n\
    end\n\
\n\
    content = tostring(content)\n\
\n\
    content = content:gsub(\"&\", \"&amp;\")\n\
                     :gsub(\"<\", \"&lt;\")\n\
                     :gsub(\">\", \"&gt;\")\n\
                     :gsub('\"', \"&quot;\")\n\
                     :gsub(\"'\", \"&#39;\")\n\
\n\
    return content\n\
end");
}