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
    luaL_dostring(L, "Regex = {}\n\
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