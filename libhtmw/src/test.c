#include "test.h"

void debug_syntax_directives(const syntaxer_list *syntax_routers) {
    printf("routers count: %zu\n", syntax_routers->size);
    syntaxer_node *node = syntax_routers->head;

    for (size_t i = 0; i < syntax_routers->size; i++) {
        syntaxer_router *router = &node->router;
        printf("[%s] => (%d)\n", router->id, router->executor);
        for (size_t i = 0; i < router->routes_count; i++) {
            syntaxer_route *route = &router->routes[i];
            if (route->flags & SYNTXF_STATIC_ROUTING) {
                printf("route -> {\"%s\", %zu} re.ref_id: %zu\n", route->data.block.data.syntax->id, route->data.block.token_length, route->rex.ref_id);
            } else if (route->flags & SYNTXF_DYNAMIC_ROUTING) {
                printf("route (%d) re.ref_id: %zu\n", route->data.resolver, route->rex.ref_id);
            }
        }
        node = node->next;
    }
}

#define TEST_LUAX_STR "{\n\
        local x = /he/\n\
        local y = /xyz/g\n\
    }}}"

void luax_test(htmw_context *ctx) {
    //char *s = (char *)malloc(sizeof(TEST_LUAX_STR));
    //strcpy(s, TEST_LUAX_STR);
    char s[] = TEST_LUAX_STR;
    luax_transpile_result o = luax_transpile(s, 1000000, ctx->luax_ctx);
    if (o.error) { // ofc a more accurate error log will be implemented
        printf("[HTMW LuaX Error]: there is an error on the LuaX syntax\n", o.error);
        exit(1);
    }
    luax_str_view sv = o.result.first;
    printf("===========\n%s\n", sv.s);
}

void debug_regex(const regex *r) {
    printf("regex: {\n\tflags: %d,\n\tref_id: %zu,\n\t\"%s\"\n}\n", r->flags, r->ref_id, r->pattern);
}

void debug_resolved_route(const syntaxer_resolved_route *r, int fin) {
    if (!fin) {
        str_debug(r->data.id);
        printf("syntaxer_resolved_route {\n\tflags: %u,\n\ttoken_length: %lld,\n\tdata.id: \"%.*s\"\n}\n",
            (unsigned)r->flags,
            (lua_Integer)r->token_length,
            (int)r->data.id.length,
            r->data.id.c_str
        );
    } else {
        printf("syntaxer_resolved_route {\n\tflags: %u,\n\ttoken_length: %lld,\n\tdata.syntax.id: \"%s\"\n}\n",
            (unsigned)r->flags,
            (lua_Integer)r->token_length,
            r->data.syntax->id
        );
    }
}

void debug_htmw_crv(const compiled_regex_vector *v, size_t n) {
    #if 1
    printf("===[CRV DEBUG (n: %zu)]===\n", n);
    printf("=vaddr: this(%p)", v);
    puts("aaa");
    printf(", data(%p)=\n", v->data);
    for (size_t i = 0; i < n; i++) {
        printf("---v[%zu]v---\nflags: %d\nrex: %p\n", i, v->data->flags, v->data->rex);
    }
    #endif
}

int pcre2t_test() {
    PCRE2_SPTR pattern = (PCRE2_SPTR)"(?<name>\\w*)=(\\d*)";
    PCRE2_SPTR subject = (PCRE2_SPTR)"foo=";

    int errornumber;
    PCRE2_SIZE erroroffset;

    pcre2_code *re = pcre2_compile(
        pattern,
        PCRE2_ZERO_TERMINATED,
        0,
        &errornumber,
        &erroroffset,
        NULL
    );

    if (!re) {
        printf("compile error\n");
        return 1;
    }

    pcre2_match_data *md =
        pcre2_match_data_create_from_pattern(re, NULL);

    int rc = pcre2_match(
        re,
        subject,
        strlen((char*)subject),
        0,
        0,
        md,
        NULL
    );

    if (rc < 0) {
        printf("no match\n");
        return 0;
    }

    PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);

    /* retrieve named-group table info */
    uint32_t namecount;
    uint32_t entrysize;
    PCRE2_SPTR nametable;

    pcre2_pattern_info(re, PCRE2_INFO_NAMECOUNT, &namecount);
    pcre2_pattern_info(re, PCRE2_INFO_NAMEENTRYSIZE, &entrysize);
    pcre2_pattern_info(re, PCRE2_INFO_NAMETABLE, &nametable);

    printf("Matched groups:\n");

    for (int i = 1; i < rc; i++) {

        PCRE2_SIZE start = ov[2*i];
        PCRE2_SIZE end   = ov[2*i+1];

        if (start == PCRE2_UNSET)
            continue;

        const char *group_name = NULL;

        /* search name table to see if this group has a name */
        for (uint32_t j = 0; j < namecount; j++) {

            PCRE2_SPTR entry = nametable + j * entrysize;
            int n = (entry[0] << 8) | entry[1];

            if (n == i) {
                group_name = (const char*)(entry + 2);
                break;
            }
        }

        if (group_name)
            printf("Group %d (%s) [%zu,%zu): %.*s\n",
                   i, group_name, start, end,
                   (int)(end-start), subject + start);
        else
            printf("Group %d [%zu,%zu): %.*s\n",
                   i, start, end,
                   (int)(end-start), subject + start);
    }

    pcre2_match_data_free(md);
    pcre2_code_free(re);
}

char *str_endl_to_sp(const char *in, size_t n) {
    static char out[2000];

    size_t i = 0;
    char c;
    while ((c = in[i]) != '\0' && i < n && i < 1998) {
        if (c != '\n' && c != '\r')
            out[i] = c;
        else
            out[i] = ' ';
        i++;
    }
    out[i] = '\0';
    return out;
}

void debug_template_arg(const ht_template_arg *arg) {
    printf("arg(\"%s\")\n", arg->name.c_str);
    printf("\tdefault_value: \"%s\"\n", arg->default_value.c_str);
    printf("\tT_ARG_SYNTAX: %d\n", !!(arg->flags & T_ARG_SYNTAX));
    printf("\tT_ARG_EXPR: %d\n", !!(arg->flags & T_ARG_EXPR));
    printf("\tT_ARG_NULLABLE: %d\n", !!(arg->flags & T_ARG_NULLABLE));
    printf("\tT_ARG_DOUBLEASS: %d\n", !!(arg->flags & T_ARG_DOUBLEASS));
}

void debug_template(const ht_template *t) {
    printf("name: \"%s\"\n", t->name.c_str);
    printf("args_min_size: %zu\n", t->args_min_size);
    printf("flags: %u\n", t->flags);
    VECT_FOR(t->args, i) {
        const ht_template_arg arg = VECT_A(ht_template_arg, t->args)[i];
        debug_template_arg(&arg);
    }
}

void debug_templates(const htmw_context *ctx) {
    puts("=====[TEMPLATE DEBUG BEGIN]=====");
    u_map_node *n = ctx->templates->head;
    while (n != NULL) {
        const ht_template *t = (ht_template *)n->data;
        printf("===[%s]===\n", n->token);
        debug_template(t);
        n = n->next;
    }
    puts("=====[ TEMPLATE DEBUG END ]=====");
}

void strplex_print(const strplex *sp) {
    strplex_element *e = (strplex_element *)sp->data;
    puts("=====[STRPLEX DEBUG BEGIN]=====");
    printf("strplex.count: %zu\n", sp->count);
    for (size_t i = 0; i < sp->count; i++) {
        printf("\tlength: %u, s: \"%s\"\n", strplex_length(e), e->value);
        e = strplex_nextelement(e);
    }
    puts("=====[ STRPLEX DEBUG END ]=====");
}






static int table_size(lua_State *L, int abs) {
    int count = 0;
    lua_pushnil(L);
    while (lua_next(L, abs) != 0) {
        count++;
        lua_pop(L, 1);
    }
    return count;
}

static void debug_lua_value(lua_State *L, int idx) {
    int abs = lua_absindex(L, idx);

    if (lua_isnil(L, abs)) {
        printf("type=nil value=nil");

    } else if (lua_isboolean(L, abs)) {
        printf("type=boolean value=%s", lua_toboolean(L, abs) ? "true" : "false");

    } else if (lua_isnumber(L, abs)) {
        printf("type=number value=%g", lua_tonumber(L, abs));

    } else if (lua_isstring(L, abs)) {
        printf("type=string value=\"%s\"", lua_tostring(L, abs));

    } else if (lua_isfunction(L, abs)) {
        if (lua_iscfunction(L, abs)) {
            printf("type=function value=(C function) ptr=%p", lua_tocfunction(L, abs));
        } else {
            printf("type=function value=(Lua function) ptr=%p", lua_topointer(L, abs));
        }

    } else if (lua_istable(L, abs)) {
        printf("type=table value=ptr=%p entries=%d",
               lua_topointer(L, abs),
               table_size(L, abs));

    } else if (lua_isuserdata(L, abs)) {
        if (lua_islightuserdata(L, abs)) {
            printf("type=lightuserdata value=ptr=%p", lua_touserdata(L, abs));
        } else {
            printf("type=userdata value=ptr=%p", lua_touserdata(L, abs));
        }

    } else if (lua_isthread(L, abs)) {
        printf("type=thread value=ptr=%p", lua_tothread(L, abs));

    } else {
        printf("type=unknown value=ptr=%p", lua_topointer(L, abs));
    }

    if (lua_getmetatable(L, abs)) {
        printf(" metatable=yes");
        lua_pop(L, 1);
    } else {
        printf(" metatable=no");
    }
}

void debug_lua_stack(lua_State *L) {
    int top = lua_gettop(L);

    printf("\n===== LUA STACK DUMP =====\n");
    printf("stack size: %d\n\n", top);

    for (int i = 1; i <= top; i++) {
        printf("[%d | rel %d] ", i, i - top - 1);
        debug_lua_value(L, i);
        printf("\n");
    }

    printf("\n==========================\n");
}