#ifndef HTMW_H
#define HTMW_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <locale.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>

#include <lua.h>
#include "lualib.h"
#include "lauxlib.h"

#include <luax.h>
#include "map.h"
#include "nx/vector.h"
#include "nx/list.h"
#include "nx/stack.h"
#include "flag.h"
#include "strplex.h"

#include "str.h"
// #include "jsw.h"
#include "netio.h"
#include <string.h>

#include "syntaxer.h"
#include "htmw_template.h"
#include "htmw_tag.h"

// #include "htmw_memory.h"

#if 0
#ifdef _MSC_VER && !__INTEL_COMPILER
#pragma comment(lib, "../lua/lua55.lib")
#endif
#endif

#if defined(_WIN32)
    //#define GLOBAL_INCLUDE_PATH_LEN 17
    #define GLOBAL_INCLUDE_PATH "C:\\htmw\\0.7\\include"
#elif defined(__unix__)
    //#define GLOBAL_INCLUDE_PATH_LEN 31
    #define GLOBAL_INCLUDE_PATH "/usr/local/lib/htmw/0.7"
#else
    #error "Unsupported platform"
#endif

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

typedef union {
    ht_template* template;
    string str;
} ht_any;

#define CTX_INIT    (1 << 0)
#define CTX_HEAD    (1 << 1)
#define CTX_NETWORK (1 << 2)
#define CTX_UNSAFE  (1 << 3)

#if 0
/// TEMPORARILY HERE ///
typedef struct syntaxer_router_s {
    char *id;
    size_t routes_count;
    syntaxer_route *routes;
    int executor;                               // this is a reference of a lua function, in this case the executor in the NBE architecture
} syntaxer_router;

typedef struct syntaxer_node_s {
    syntaxer_router router;
    struct syntaxer_node_s *next;
} syntaxer_node;

typedef struct syntaxer_list {
    syntaxer_node *head;
    syntaxer_node *tail;
    size_t size;
} syntaxer_list;
/// TEMPORARILY HERE ///
#endif

typedef struct htmw_context_s {
    u_map areas;                                // list of NULL (claimed areas)
    list* global_paths;                         // list of strings (include paths)
    u_map* includes;                            // map of strings (path, content) global includes are prefixed with `>`
    u_map* templates;                           // (name, ht_template)
    string input;
    lua_State* L;
    flag_t flags;
    syntaxer_list *syntax_routers;
    compiled_regex_vector *crv;
    size_t *crv_size;
    luax_context *luax_ctx;
} htmw_context;

//#define nullctx ((htmw_context){ u_map_null, NULL, NULL, NULL, nullstr, NULL, 0, syntaxer_list_null, nullcrv, 0, luax_nullctx })
#define nullctx ((htmw_context){ u_map_null, NULL, NULL, NULL, nullstr, NULL, 0, NULL, NULL, NULL, NULL })

typedef struct {
    size_t begin;
    size_t length;
} str_frag;

/*typedef struct {
    uint8_t flags;
    char value[];
};*/

typedef struct {
    size_t fields_count;                        // number of fields on the ground
    size_t* inner_shifts;
    size_t args_count;                          // 
    size_t* arg_shifts;                         // contains the indices of the arguments, starting from the 2nd argument, the last value is the address of the byte overflow (after the null terminator)
    charw* _args;                              // contains strings of value passed in order for the constructor
    strplex args;
    charw* _inners;                            // contains strings of the inner for each widget
    strplex inners;
    field_location* fields;                     // contains the info where each field is located on the ground (pos) and which placeholder (idx)
    string ground;                              // content with invisible placeholders
    ht_template_arg_value *args_vect;
    size_t args_vect_size;
    size_t args_vect_cap;
} txt_template_fill_data;

int check_lua(lua_State* L, int r);

ht_template_core* htmw_process_template_inner(ht_template t);
void htmw_preprocess(lua_State* L, string in, htmw_context* ectx, list *global_paths, int enable_network);
txt_template_fill_data htmw_process_txt(const htmw_context *ctx);
string htmw_compile_txt(const htmw_context *ctx, const txt_template_fill_data *tf, size_t *args_vect_cursor);
string htmw_postprocess(string s);

void throw_error(const char* msg, flag_t flags);

typedef enum {
    h_null,
    h_include,
    h_define,
    h_comment,
    h_template,
    h_block,
    h_no,
    h_area_no
} htmw_mode;

typedef struct {
    char* origin_name;
    char* output_name;      // NULL if not recursive
    char* format;           // NULL if auto
    int replica_mode;       // boolean: if true a deep copy of all the files in the folder will be performed
    int enable_network;     // boolean: if true allows HTTP includes
    int unsafe_mode;        // boolean: if enable_network is true but unsafe_mode is false, unsafe lua libraries will be removed
} config_t;

void htmw_remove_unsafe(lua_State* L);
string read_file(const char* path, int exe_location);

#include "luaw.h"
#include "luax.h"
#include <luax_regex.h>

#include "htmw_syntax_notation.h"
#include "htmw_syntax_dir.h"

//#include "test.h"

#define E_EXIT 1

#endif