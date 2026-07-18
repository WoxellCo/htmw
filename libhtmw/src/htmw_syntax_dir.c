#include "htmw_syntax_dir.h"

// 1. lua state
// 2. total routers list in the current context
// 3. name of the processing syntax
// 4. lua code buffer string
void htmw_process_syntax_directive_value(lua_State *L, syntaxer_list *routers, const char *id, const char *buffer) {
    //puts("------------------------------");debug_lua_stack(L);
    //puts(buffer);
    if (!check_lua(L, luaL_dostring(L, buffer)))
        throw_error("Lua expression is not OK in syntax directive", E_EXIT);
    /*if (!check_lua(L, lua_pcall(L, 0, 1, 0))/* != LUA_OK/)
        throw_error("Lua call is not OK in syntax directive", E_EXIT);*/
    if (!lua_istable(L, -1))
        throw_error("expected Lua table in syntax directive", E_EXIT);
    // check the @syntax lua table and retrieve the values
    // RESUME

    lua_getfield(L, -1, "routes"); // 1: routes table
    if (!lua_istable(L, -1)) { // 1: routes table
        // exception
        throw_error("Invalid type for `routes`, array expected", E_EXIT);
    }
    size_t table_len = (size_t)lua_rawlen(L, -1);
    // TODO: allocate array memory
    syntaxer_node *node = (syntaxer_node *)malloc(sizeof(syntaxer_node));
    *node = (syntaxer_node) {
        .router = {
            .id = strdup(id), // buffer??
            .routes = (syntaxer_route *)malloc(table_len * sizeof(syntaxer_route)),
            .routes_count = table_len,
            .executor = 0
        },
        .next = NULL
    };

    for (size_t table_element_idx = 1; table_element_idx <= table_len; table_element_idx++) {
        lua_rawgeti(L, -1, (lua_Integer)table_element_idx); // 2: routes table element

        int refs[2] = {0, 0}; // ?
        
        if (!lua_istable(L, -1)) { // 2: routes table element
            // exception: expected a table (for the tuple(regex, tuple(string, int) | function => tuple(string, int))), found smth else
            throw_error("expected a table (for the tuple(Regex, tuple(string, int) | function => tuple(string, int)))", E_EXIT);
        }
        size_t route_resolve_tuple_len = (size_t)lua_rawlen(L, -1);
        switch (route_resolve_tuple_len) {
        case 0:
            // exception: the tuple isn't long enough
            throw_error("Expected Lua tuple (Regex, nil | table | function) in syntax directive", E_EXIT);
            break;
        case 1: // default the 2nd ref to zero
            goto htmw_psdv_skip_route_2nd_element;
        case 2:
            break;
        default:
            // warning: tuple too long (useless)
            printf("[HTMW Warning]: Syntax Routing tuple have too many elements, additional elements will be ignored\n");
            break;
        }

        syntaxer_resolved_route resolved_route;
        //node->router.routes[0].data.block.data.id;

        // 2nd element: can be nil | table | function
        lua_rawgeti(L, -1, 2); // 3: routes table element[2] {/regex/, {"string", 2}}
        if (lua_istable(L, -1)) { // 4: TUPLE LITERAL {"string", 2}
            lua_rawgeti(L, -1, 1); // 5: id | resolver
            if (lua_isstring(L, -1)) { // "string" (5)

                /// commented out because the compiler should register all the syntax directives first
                /// so we will temporarily store a string

                //lua_tostring(L, -1)
                //syntaxer_node *node = syntaxer_list_find_by_id(routers, lua_tostring(L, -1));
                //if (node != NULL) {
                //    resolved_route.syntax = &node->router;
                //} else {
                //    // exception: syntax not defined
                //    throw_error("Invalid type for the 1st element in Syntax directive in inner tuple, the specified Syntax identifier does not exist", E_EXIT);
                //}

                char *resolved_syntax_id = lua_tostring(L, -1);
                resolved_route.data.id = str_from(resolved_syntax_id);
                resolved_route.flags = SYNTXF_STATIC_ROUTING;
                //str_debug(resolved_route.data.id);
                lua_pop(L, 1); // 5 >> 4

                //lua_rawgeti(L, -1, 2);
                lua_geti(L, -1, 2); // 5b: number
                if (lua_isinteger(L, -1)) { // 5b: number
                    resolved_route.token_length = lua_tointeger(L, -1);
                } else if (lua_isnil(L, -1)) {
                    resolved_route.token_length = -1;
                } else {
                    // throw exception  
                    throw_error("Invalid type for the 2nd element in Syntax directive in inner tuple, integer or nil expected", E_EXIT);
                }
                node->router.routes[table_element_idx - 1].data.block = resolved_route;
                node->router.routes[table_element_idx - 1].flags = SYNTXF_STATIC_ROUTING;
                lua_pop(L, 1); // 5b >> 4
            } else if (lua_isnil(L, -1)) { // first element of the tuple literal is nil (routing with no blocks (neutral))
                //resolved_route.data.id = NULL;
                resolved_route.data.id = nullstr;
                resolved_route.flags = SYNTXF_NEUTRAL_ROUTING | SYNTXF_STATIC_ROUTING;
                /*lua_rawgeti(L, -1, 2); //?
                if (lua_isinteger(L, -1)) {
                    resolved_route.token_length = lua_tointeger(L, -1);
                } else if (lua_isnil(L, -1)) {
                    resolved_route.token_length = -1;
                } else {
                    // throw exception
                    throw_error("Invalid type for the 2nd element in Syntax directive in inner tuple, integer or nil expected", E_EXIT);
                }*///wait i did the same thing here?? ^^^^^^^

                lua_rawgeti(L, -2, 2);
                //debug_lua_stack(L);
                if (lua_isinteger(L, -1)) { // 5b: number
                    resolved_route.token_length = lua_tointeger(L, -1);
                    //printf("resolved_route.token_length: %lld\n", resolved_route.token_length);
                } else if (lua_isnil(L, -1)) {
                    resolved_route.token_length = -1;
                } else {
                    // throw exception  
                    throw_error("Invalid type for the 2nd element in Syntax directive in inner tuple, integer or nil expected", E_EXIT);
                }
                node->router.routes[table_element_idx - 1].data.block = resolved_route;

                node->router.routes[table_element_idx - 1].flags = SYNTXF_NEUTRAL_ROUTING | SYNTXF_STATIC_ROUTING;
                lua_pop(L, 2); // 5 >> 4
                // chaged 1 to 2 when added the intger
            } else {
                // type exception
                throw_error("Invalid type for the 1st element in Syntax directive in inner tuple, string or nil expected", E_EXIT);
            }
            /////lua_pop(L, 1); // inner tuple, 4 >> 3
            lua_pop(L, 1);
        } else if (lua_isfunction(L, -1)) { // RESOLVER (4)
            resolved_route.flags = SYNTXF_DYNAMIC_ROUTING;
            //refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);
            node->router.routes[table_element_idx - 1].flags = SYNTXF_DYNAMIC_ROUTING;
            node->router.routes[table_element_idx - 1].data.resolver = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            // exception
            throw_error("Tuple or Resolver expected in Syntax directive", E_EXIT);
        }
        //lua_pop(L, 1); // 3 >> 2 -> routes table

        htmw_psdv_skip_route_2nd_element:

        //debug_lua_stack(L);

        // 1st element: regex (a table)
        lua_rawgeti(L, -1, 1); // 3b: regex
        if (!lua_istable(L, -1)) {
            // TODO: throw exception
            throw_error("expected a Regex in Syntax directive", E_EXIT);
        }
        //lua_getfield(L, -1, "__name");
        /////lua_getfield(L, -1, "__name");
        /////if (!lua_isstring(L, -1)) {
        /////    // TODO: throw exception
        /////    throw_error("expected a Regex object in Syntax directive", E_EXIT);
        /////}
        /////const char *name = lua_tostring(L, -1);
        /////if (strcmp(name, "Regex")) {
        /////    // exception
        /////    throw_error("expected a Regex in Syntax directive", E_EXIT);
        /////}
        /////lua_pop(L, 2); // field: __name + 1st element

        node->router.routes[table_element_idx - 1].rex = (regex) {
            .flags = 0,
            .g = 0,
            .length = 0,
            .pattern = 0,
            .re = NULL,
            .ref_id = 0
        };

        {
        // assuming it's the right type for now
        lua_getfield(L, -1, "refId");
        size_t ref_id;
        if (lua_isinteger(L, -1) && (ref_id = lua_tointeger(L, -1)) > 0) {
            node->router.routes[table_element_idx - 1].rex.ref_id = ref_id;
        }
        lua_pop(L, 1); // refId
        }

        lua_pop(L, 1); // regex 3b >> 2

        lua_pop(L, 1); // outer tuple, 2 >> 1
        //node->router.routes[table_element_idx - 1].data.block = resolved_route;
        //printf("table_element_idx: %zu\n", table_element_idx - 1);
        //debug_resolved_route(&resolved_route, 0);
        //str_debug(node->router.routes[table_element_idx - 1].data.block.data.id);
    }
    lua_pop(L, 1); // routes, 1 >> 0

    //lua_pushstring(L, "neutral"); REMOVED! it's merged in routes (the nil tuple)
    
    lua_getfield(L, -1, "executor"); // 1b
    if (!lua_isfunction(L, -1)) {
        throw_error("expected Executor in Syntax directive", E_EXIT);
    }
    
    node->router.executor = luaL_ref(L, LUA_REGISTRYINDEX);
    //printf("EXECUTOR: %d\n", node->router.executor);
    //syntaxer_list_add(routers, id); // ??????/
    lua_pop(L, 1); // whole table literal, 1 >> 0

    //lua_pop(L, 1);//puts("====");

    syntaxer_list_add(routers, node);
    //debug_lua_stack(L);puts("==========================================");
}

// once all the syntaxes are defined, the prefetcher will fetch all the blocks that have a string and not a resolved
// to make the runtime much more efficient
// TODO: CHECK IF IT ACTUALLY HAS THE STATIC RESOLVER
void htmw_prefetch_syntax_drectives(syntaxer_list *routers) {
    syntaxer_node *syntaxer_n = routers->head;
    while (syntaxer_n != NULL) {
        syntaxer_router *router = &syntaxer_n->router;
        //printf("router->routes_count: %zu", router->routes_count);
        for (size_t i = 0; i < router->routes_count; i++) {
            syntaxer_route *route = &router->routes[i];
            syntaxer_resolved_route *resolved_route = &route->data.block;
            
            //if (route->data.block.flags & SYNTXF_STATIC_ROUTING) {
            //printf("ROUTE: %s[%zu] => NEUTRAL: %d\n", router->id, i, !(route->flags & SYNTXF_NEUTRAL_ROUTING));
            if ((route->flags & SYNTXF_STATIC_ROUTING) && !(route->flags & SYNTXF_NEUTRAL_ROUTING)) {
                syntaxer_node *node = syntaxer_list_find_by_id(routers, resolved_route->data.id.c_str, SIZE_MAX);
                if (node != NULL) {//puts(node->router.id);
                    resolved_route->data.syntax = &node->router;
                } else {
                    // exception: syntax not defined
                    throw_error("Invalid type for the 1st element in Syntax directive in inner tuple, the specified Syntax identifier does not exist", E_EXIT);
                }
            }
        }
        syntaxer_n = syntaxer_n->next;
    }
}