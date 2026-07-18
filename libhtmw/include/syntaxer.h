// HTMW vT.7 Syntax Router

#ifndef SYNTAXER_H
#define SYNTAXER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
//#include <wchar.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "flag.h"

#include "str.h"
#include <luax_regex.h>

typedef struct htmw_context_s htmw_context;

#define syntaxer_list_null ((syntaxer_list){ NULL, NULL, 0 })

#define SYNTXF_EXE_BREAK                (1 << 0)
#define SYNTXF_EXE_ESC_CHAR             (1 << 1)
#define SYNTXF_EXE_ESC_RAW              (1 << 2)
//#define SYNTXF_EXE_ESC_BREAK            (1 << 3)

#define SYNTXF_DEF_N_ML                 (1 << 0)
#define SYNTXF_DEF_N_NO_ESC_RAW         (1 << 1)
#define SYNTXF_DEF_N_ESC_RAW            (1 << 2)

#define SYNTXF_DEF_NB_ESC_IMMUNE        (1 << 0)


#define SYNTXF_STATIC_ROUTING           (1 << 0)
#define SYNTXF_DYNAMIC_ROUTING          (1 << 1)
#define SYNTXF_NEUTRAL_ROUTING          (1 << 2)

#define SYNTXF_NEWLINE                  (1 << 0)

struct syntaxer_s;

typedef struct syntaxer_resolved_route_s {
    union {
        struct syntaxer_router_s *syntax;
        string id;
    } data;
    lua_Integer token_length;
    uint8_t flags;
} syntaxer_resolved_route;

union syntaxer_block_route_u {
    syntaxer_resolved_route block;
    int resolver;                               // reference to function or tuple
};

typedef struct {
    regex rex;
    union syntaxer_block_route_u data;
    uint8_t flags;
} syntaxer_route;

typedef struct syntaxer_router_s {
    char *id;
    size_t routes_count;
    syntaxer_route *routes;
    int executor;                               // this is a reference of a lua function, in this case the executor in the NBE architecture
} syntaxer_router;

typedef struct {
    uint8_t type;
    char *message;
    char *ext_message;
} syntaxer_error;

typedef struct syntaxer_node_s {
    syntaxer_router router;
    struct syntaxer_node_s *next;
} syntaxer_node;

typedef struct syntaxer_list {
    syntaxer_node *head;
    syntaxer_node *tail;
    size_t size;
} syntaxer_list;

enum syntaxer_error_type {
    syntaxer_et_invalid_return,                 // the resolver doesn't return the block tuple
    syntaxer_et_invalid_execution,              // lua state wasn't okay during the execution
    syntaxer_et_invalid_ref,                    // the lua ref wasn't a function or a block tuple
    syntaxer_et_syntax_not_found                // syntax does not exist
};

typedef struct {
    size_t n;                                   // length
    syntaxer_error error;                       // error
} syntaxer_result;

typedef struct {
    /*union {
        lua_Integer dynamic_resolver;
        struct {
            size_t token_length;
        } static_resolver;
    } terminator;*/
    string in_token;
    size_t out_token_length;
    uint8_t flags;
    size_t shift;
} syntaxer_analyze_input_data;

enum syntaxer_executor_signal {
    syntaxer_es_continue = 0,
    syntaxer_es_break = 1,
    syntaxer_es_error = 2
};

syntaxer_resolved_route syntaxer_resolve(lua_State *L, const syntaxer_list *l, int handler, syntaxer_error *err);
void syntaxer_list_add(syntaxer_list *l, const syntaxer_node *node);
syntaxer_node *syntaxer_list_find_by_id(const syntaxer_list *l, const char *id, size_t id_char_limit);

// analyze the string and reutrn the length of the syntax
syntaxer_result syntaxer_analyze(const htmw_context *ctx, const char *s, size_t max_len, const syntaxer_router *root_router, syntaxer_analyze_input_data params);

void syntaxer_test(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif