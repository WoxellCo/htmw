#include "htmw.h"
#include <stddef.h>
#include <string.h>
#include "syntaxer.h"

void syntaxer_list_add(syntaxer_list *l, const syntaxer_node *node) {
    if (l->head == NULL) {
        l->head = l->tail = node;
    } else {
        l->tail->next = node;
        l->tail = node;
    }
    l->size++;
}

syntaxer_node *syntaxer_list_find_by_id(const syntaxer_list *l, const char *id, size_t id_char_limit) {
    if (id == NULL)
        return NULL;
    const syntaxer_node *node = l->head;
    //printf("HEAD LIST CHECK: |%p|\n", node);
    while (node != NULL) {
        //printf("SYNTAX LIST CHECK: \"%s\"\n", node->router.id);
        if (!strncmp(node->router.id, id, id_char_limit)) {
            //printf("FOUND NODE: \"%s\"\n", node->router.id);
            return node;
        }
        node = node->next;
    }
    return NULL;
}

/*struct syntaxer_resolve_handle_return_value_t {

};

static lua_Number syntaxer_resolve_*/

static inline uint8_t syntaxer_resolve_expand_tuple(lua_State *L, char **v, syntaxer_resolved_route *rr, syntaxer_error *err) {
    lua_geti(L, -1, 2);             // get the second element of the tuple (returning value)
    if (lua_isnumber(L, -1)) {
        rr->token_length = lua_tointeger(L, -1);
        lua_pop(L, 1);
    } else {
        err->type = syntaxer_et_invalid_return;
        lua_pop(L, 1);
        return 0;
    }
    lua_geti(L, -1, 1);             // get the first element of the tuple (returning value)
    size_t v_len = strlen(lua_tostring(L, -1));
    *v = (char *)calloc(v_len + 1, sizeof(char));
    
    if (lua_isstring(L, -1)) {
        *v = strcpy(*v, lua_tostring(L, -1));
        lua_pop(L, 1);
    } else {
        err->type = syntaxer_et_invalid_return;
        lua_pop(L, 1);
        return 0;
    }
    return 1;
}

syntaxer_resolved_route syntaxer_resolve(lua_State *L, const syntaxer_list *l, int handler, syntaxer_error *err) {
    syntaxer_resolved_route rr = (syntaxer_resolved_route){
        .data.syntax = NULL,
        .token_length = 0
    };
    const char *v = NULL;
    lua_rawgeti(L, LUA_REGISTRYINDEX, handler); // push function on the stack
    if (lua_isfunction(L, -1)) {                // check if it is indeed a function
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            err->type = syntaxer_et_invalid_execution;
            return rr;
        }
    } 
    if (!lua_istable(L, -1)) {                  // check if the returning value is a table
        err->type = syntaxer_et_invalid_return;
        lua_pop(L, 1);
        return rr;
    }

    if (!syntaxer_resolve_expand_tuple(L, &v, &rr, err)) {
        err->type = syntaxer_et_invalid_return;
        lua_pop(L, 1);
        return rr;
    }

    lua_pop(L, 1);

    rr.data.syntax = syntaxer_list_find_by_id(l, v, SIZE_MAX);
    //printf("-----rr.data.syntax: %s\n", rr.data.syntax);

    if (rr.data.syntax == NULL)
        err->type = syntaxer_et_syntax_not_found;
    //else free(v);

    return rr;
}

//////////////////////////


syntaxer_result syntaxer_analyze(const htmw_context *ctx, const char *s, size_t max_len, const syntaxer_router *root_router, syntaxer_analyze_input_data params) {
    //syntaxer_node *node = syntaxer_list_find_by_id(routers, root_router_id);
    //FILE *f = fopen("htmw_log.txt", "a");
    //puts("ddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    if (root_router == NULL)
        return (syntaxer_result){.n = 0, .error = syntaxer_et_syntax_not_found};

    //syntaxer_router root_router = node->router;

    syntaxer_list *routers = ctx->syntax_routers;
    lua_State *L = ctx->L;

    size_t shift = params.shift;
    char *error_message = NULL, *error_ext_message = NULL;
    uint8_t error_type = 0;

    //debug_htmw_crv(ctx->crv, *ctx->crv_size);

    while (s[shift] != '\0') {
        syntaxer_analyze_while_begin:
        lua_rawgeti(L, LUA_REGISTRYINDEX, root_router->executor); // LUAP(1): executor
        syntaxer_route *route;
        //debug_lua_stack(L);
        //printf("ROUTER: \"%s\", SHIFT: %zu, s + shift: \"%.*s\"\n", root_router->id, shift, 10, s + shift);
        //printf("ROUTER: \"%s\", SHIFT: %zu, s + shift: \"%s\"\n", root_router->id, shift, str_endl_to_sp(s + shift, 10));
        for (size_t i = 0; i < root_router->routes_count; i++) {
            route = &root_router->routes[i];
            
            // regex time
            pcre2_code *re = ctx->crv->data[route->rex.ref_id - 1].rex;
            #if 0
            {
                PCRE2_SIZE size;
                pcre2_pattern_info(re, PCRE2_INFO_SIZE, &size);
                printf(">> Compiled size: %zu bytes || addr: %p || ref_id: %zu\n", size, re, route->rex.ref_id);
            }
            #endif

            pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);

            int rc = pcre2_match(
                re,
                s,
                max_len,
                shift,
                PCRE2_ANCHORED | PCRE2_NOTEMPTY | PCRE2_NOTEMPTY_ATSTART,
                match_data,
                NULL
            );

            //printf("RC: %d, REF_ID: %zu, RE: %p, SHIFT: %zu, MAX_LEN: %zu, s + shift: \"%.*s\"\n", rc, route->rex.ref_id, re, shift, max_len, 10, s + shift);
            //fprintf(f, "RC: %d, REF_ID: %zu, RE: %p, SHIFT: %zu, s + shift: \"%.*s\"\n", rc, route->rex.ref_id, re, shift, 10, s + shift);
            
            // check if the string matches with the regex
            if (rc >= 0 && rc != PCRE2_ERROR_NOMATCH) {
                //printf("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEe\n");
                PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
            
                int start = ovector[0];
                int end   = ovector[1];
                size_t match_length = (size_t)end - (size_t)start;
                //printf("ovector: 0 => %d, 1 => %d; shift: %zu\n", ovector[0], ovector[1], shift);

                syntaxer_result r;
                long long out_token_length = 0;
                syntaxer_router *next_router = NULL;
                uint8_t flags = route->flags;

                if (flags & SYNTXF_STATIC_ROUTING) {
                    //puts("STATIC");
                    out_token_length = route->data.block.token_length;
                    if (!(flags & SYNTXF_NEUTRAL_ROUTING))
                        next_router = route->data.block.data.syntax;
                } else if (flags & SYNTXF_DYNAMIC_ROUTING) {
                    //puts("DYNAMIC");
                    // TODO: prepare & push table and call function
                    //lua_rawgeti(L, LUA_REGISTRYINDEX, route->data.resolver);
                    lua_rawgeti(L, LUA_REGISTRYINDEX, route->data.resolver);

                    //////////////

                    uint32_t name_count;
                    uint32_t entry_size;
                    PCRE2_SPTR name_table;
                    
                    pcre2_pattern_info(re, PCRE2_INFO_NAMECOUNT, &name_count);
                    pcre2_pattern_info(re, PCRE2_INFO_NAMEENTRYSIZE, &entry_size);
                    pcre2_pattern_info(re, PCRE2_INFO_NAMETABLE, &name_table);
                    
                    // prepare table
                    
                    lua_newtable(L);
                    lua_pushlstring(L, s + shift, end - start);
                    lua_setfield(L, -2, "token");
                    
                    
                    lua_newtable(L);
                    
                    int anon_captures = 0;
                    
                    for (int j = 1; j < rc; j++) {
                    
                        PCRE2_SIZE start = ovector[2*j];
                        PCRE2_SIZE end   = ovector[2*j+1];
                    
                        if (start == PCRE2_UNSET)
                            continue;
                    
                        const char *group_name = NULL;
                    
                        for (uint32_t k = 0; k < name_count; k++) {
                        
                            PCRE2_SPTR entry = name_table + k * entry_size;
                            int n = (entry[0] << 8) | entry[1];
                        
                            if (n == j) {
                                group_name = (const char*)(entry + 2);
                                break;
                            }
                        }
                    
                        lua_pushlstring(L, s + shift + start, end - start);
                    
                        if (group_name)
                            lua_setfield(L, -2, group_name);
                        else
                            lua_seti(L, -2, ++anon_captures);
                    }
                
                    lua_setfield(L, -2, "captures");
                
                    ///////////////
                    //puts("ppppppppppppppppppppppppppppppppppppp");
                    //debug_lua_stack(L);
                   
                    if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
                        if (!lua_istable(L, -1)) {
                            // TODO: add messages
                            return (syntaxer_result){
                                .n = shift + r.n,
                                .error = {
                                    .type = syntaxer_et_invalid_return,
                                    .message = NULL,
                                    .ext_message = NULL
                                }
                            };
                        }

                        lua_rawgeti(L, -1, 1);

                        if (lua_isstring(L, -1)) {
                            next_router = syntaxer_list_find_by_id(ctx->syntax_routers, lua_tostring(L, -1), SIZE_MAX);
                        } else if (lua_isnil(L, -1)) {
                            flags |= SYNTXF_NEUTRAL_ROUTING;
                        } else {
                            lua_pop(L, 1);
                            // TODO: add messages
                            return (syntaxer_result){
                                .n = shift + r.n,
                                .error = {
                                    .type = syntaxer_et_invalid_return,
                                    .message = NULL,
                                    .ext_message = NULL
                                }
                            };
                        }

                        lua_rawgeti(L, -2, 2);

                        if (!lua_isinteger(L, -1)) {
                            lua_pop(L, 2);
                            // TODO: add messages
                            return (syntaxer_result){
                                .n = shift + r.n,
                                .error = {
                                    .type = syntaxer_et_invalid_return,
                                    .message = NULL,
                                    .ext_message = NULL
                                }
                            };
                        }

                        out_token_length = (long long)lua_tointeger(L, -1);

                        //lua_pop(L, 2);
                        lua_pop(L, 3);
                    }/* else if (flags & SYNTXF_NEUTRAL_ROUTING) {
                        //puts("NEUTRAL");
                        out_token_length = (long long)route->data.block.token_length;
                        //printf("(long long)route->data.block.token_length: %lld\n", (long long)route->data.block.token_length);
                    } */else {
                        //puts("ERR");
                        error_message = (char *)malloc((75 + strlen(root_router->id) + 1) * sizeof(char));
                        error_ext_message = strdup(lua_tostring(L, -1));
                        lua_pop(L, 1); // TODO: debug the stack after the call and check if there's no mem leak
                        if (error_message == NULL) {
                            fprintf(stderr, "[Syntaxer] Error while allocating memory for error message\n");
                            exit(1);
                        }
                        sprintf(error_message, "error while executing the resolver for route at index %zu, router \"%s\"", i + 1, root_router->id);
                        return (syntaxer_result){
                            .n = shift + r.n,
                            .error = {
                                .type = syntaxer_et_invalid_execution,
                                .message = error_message,
                                .ext_message = error_ext_message
                            }
                        };
                    }
                }
                if (out_token_length <= 0) {
                    out_token_length = (long long)match_length;
                }
                if (!(flags & SYNTXF_NEUTRAL_ROUTING)) {
                    r = syntaxer_analyze(
                        ctx,
                        s,
                        max_len,// - shift - match_length,
                        next_router,
                        (syntaxer_analyze_input_data){
                            .flags = s[shift + match_length - 1] != '\n' && s[shift + match_length - 1] != '\r' ? 0 : SYNTXF_NEWLINE,
                            .out_token_length = (size_t)out_token_length,
                            .in_token = {
                                .c_str = s + shift,
                                .length = match_length,
                                .size = 0
                            },
                            .shift = shift + match_length
                        }
                    );
                    //printf("R (NO NEUTRO): %zu\n", r.n);
                } else {
                    //printf("OUT_TOKEN_LENGTH: %zu\n", (size_t)out_token_length);
                    r = (syntaxer_result){
                        .n = shift + (size_t)out_token_length,
                        .error = {
                            .type = 0,
                            .message = NULL,
                            .ext_message = NULL
                        }
                    };
                    //printf("R (NEUTRO): %zu\n", r.n);
                }

                if (r.error.type) {
                    lua_pop(L, 1);
                    return (syntaxer_result){
                        .n = shift + r.n,
                        .error = r.error
                    };
                }
                //shift += match_length + r.n;
                shift = r.n - 1;
                //debug_lua_stack(L);
                pcre2_match_data_free(match_data);
                //if (flags & SYNTXF_NEUTRAL_ROUTING) {
                    shift++;
                    goto syntaxer_analyze_while_begin;
                //}
                break;
            }
            pcre2_match_data_free(match_data);
            //debug_lua_stack(L);
        }



        // TODO: prepare table and call the executor

        lua_newtable(L);
        lua_pushlstring(L, s + shift, params.out_token_length);
        lua_setfield(L, -2, "token");

        //debug_lua_stack(L);
        if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
            
            /*if (!lua_istable(L, -1)) {
                // TODO: add messages
                return (syntaxer_result){
                    .n = shift,
                    .error = {
                        .type = syntaxer_et_invalid_return,
                        .message = NULL,
                        .ext_message = NULL
                    }
                };
            }*///lua_getfield(L, -1, "code");
            if (!lua_isinteger(L, -1)) {
                // TODO: add messages
                return (syntaxer_result){
                    .n = shift,
                    .error = {
                        .type = syntaxer_et_invalid_return,
                        .message = NULL,
                        .ext_message = NULL
                    }
                };
            }

            // TODO: break the loop and return the result

            int8_t sig = (int8_t)lua_tointeger(L, -1);
            int8_t break_free = 0;

            switch (sig) {
            case syntaxer_es_continue:
                break;
            case syntaxer_es_break:
                //printf("shift: %zu, params.otl: %zu\n", shift, params.out_token_length);
                shift += params.out_token_length;
                //printf("shift: %zu\n", shift);
            case syntaxer_es_error:
                break_free = 1;
                break;
            default:
                break;
            }

            //lua_pop(L, 2);
            lua_pop(L, 1);
            
            if (break_free) break;

        } else {
            error_message = (char *)malloc((75 + strlen(root_router->id) + 1) * sizeof(char));
            error_ext_message = strdup(lua_tostring(L, -1));
            lua_pop(L, 2);
            if (error_message == NULL) {
                fprintf(stderr, "[Syntaxer] Error while allocating memory for error message\n");
                exit(1);
            }
            sprintf(error_message, "error while executing the executor for router \"%s\"", root_router->id);
            return (syntaxer_result){
                .n = shift,
                .error = {
                    .type = syntaxer_et_invalid_execution,
                    .message = error_message,
                    .ext_message = error_ext_message
                }
            };
        }
        //puts("+++++++++++++++++++++");
        //usleep(500000);
        shift++;
    }

    //error_message = (char *)malloc((75 + strlen(root_router->id) + 1) * sizeof(char));
    //error_ext_message = strdup(lua_tostring(L, -1));

    //lua_pop(L, 1);

    //sprintf(error_message, "error while executing the executor for router \"%s\"", root_router->id);
    return  (syntaxer_result){
        .n = shift,
        .error = {
            .type = error_ext_message, // FIXME!
            .message = error_message,
            .ext_message = error_ext_message
        }
    };
}


//////////////












void syntaxer_test(lua_State *L) {
    lua_settop(L, 0);
    luaL_dostring(L, "return function() return {\"id_7\", 75} end");

    luaL_checktype(L, -1, LUA_TFUNCTION);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    syntaxer_error err = {0};

    syntaxer_list l = syntaxer_list_null;
    for (size_t i = 0; i < 10; i++) {
        char *id = (char *)malloc(20 * sizeof(char));
        sprintf(id, "id_%zu", i + 1);
        syntaxer_node *node = (syntaxer_node *)malloc(sizeof(syntaxer_node));
        *node = (syntaxer_node){
            .router = {
                .id = id,
                .routes = NULL,
                .routes_count = 0,
                .executor = (int)i
            },
            .next = NULL
        };
        syntaxer_list_add(&l, node);
    }

    syntaxer_resolved_route rr = syntaxer_resolve(L, &l, ref, &err);

    if (err.type) {
        //printf("ERR: %d\n", err.type);
        if (err.type == 1) {
            const char *msg = lua_tostring(L, -1);
            //printf("%s\n", msg);
        }
        return;
    }
    //printf("id: \"%s\", exe: %d, tok len: %d\n", rr.data.syntax->id, rr.data.syntax->executor, rr.token_length);
}