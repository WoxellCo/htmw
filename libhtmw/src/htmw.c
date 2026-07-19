/**
 * (c) HTMW vT.7 by Woxell.co
 * 
 * [WARNING]
 * The current version is a test version, it's unstable and there are known memory leaks and unhandled errors.
 * The purpose for now is just to implement and test it's functionality without focusing 100% on performance & optimization.
 */
#include "htmw.h"

#define DEBUG_MODE 0

#if DEBUG_MODE
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#endif

#define MAX_LEN_IN  100000000
#define MAX_LEN_OUT 100000000
#define MAX_LEN_LN 10000
//#define MAX_TEMPLATE_LEN 1000000
#define MAX_TEMPLATE_LEN 100000

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <limits.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#endif

#define NORM_INCLUDE_PIPELINE 1

htmw_context *luaw_ctx = NULL;

int check_lua(lua_State* L, int r) {
    if (r != LUA_OK) {
        char *msg = lua_tostring(L, -1);
        printf("%s\n", msg);
        return 0;
    }
    return 1;
}

void throw_error(const char* msg, flag_t flags) {
    printf("[HTMW Error]: %s\n", msg);
    if (flag_includes(flags, E_EXIT)) {
        puts("Compilation interrupted, exiting...");
        exit(1);
    }
}

string read_file(const char* path, int exe_location) {
    FILE* f = fopen(path, "r");
    
    if (f == NULL) {
        size_t msg_len = 33 + strlen(path);
        char *msg = (char *)malloc(msg_len * sizeof(char));
        snprintf(msg, msg_len, "Couldn't open input file(s): \"%s\"", path);
        throw_error(msg, E_EXIT);
    }

    fseek(f, 0L, SEEK_END);
    size_t sz = ftell(f);
    rewind(f);
    //printf("%zu\n", sz);

    string s = str_new(sz + 1);

    charw* buffer = (charw*)malloc(MAX_LEN_LN * sizeof(charw));
#if WCHAR_MODE
    while (fgetws(buffer, MAX_LEN_LN, f)) {
        str_append(&s, buffer, MAX_LEN_LN);
    }
#else
    while (fgets(buffer, MAX_LEN_LN, f))
        str_append(&s, buffer, MAX_LEN_LN);
#endif

    fclose(f);
    return s;
}

/*FILE* fopen_g(const char* filename, const char* mode) {
    char* base_dir = get_executable_dir();
    if (!base_dir) {
        throw_error("Could not determine executable directory\n", E_EXIT);
        return NULL;
    }

    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s%s", base_dir, filename);

    return fopen(full_path, mode);
}*/

enum template_args_phase {
    template_ap_name_before,
    template_ap_syntax_id,
    template_ap_name,
    template_ap_name_after,
    template_ap_value_before,
    template_ap_value,
    template_ap_value_after
    
};

// creates a list of arguments in a given template and a given piece of a string (insde the parenthesis), `n` is the max allocation size
void parse_template_args(ht_template* t, string in, size_t n, const htmw_context *ctx) {
    //printf("_________________n: %u___________________", n);
    string* args_split = (string*)malloc(n * sizeof(string));
    flag_t* args_flag = (flag_t*)calloc(n, sizeof(flag_t));
    string* args_default = (string*)malloc(n * sizeof(string));
    string* args_syntax = (string*)malloc(n * sizeof(string));

#if DEBUG_MODE
    //str_debug(in);
#endif

    size_t count = 0;
    charw c;
    int arg_phase = template_ap_name_before;
    size_t last_arg_begin_idx = 0;
    charw delim = charl('\0');
    size_t last_default_begin_idx = 0;
    for (size_t i = 0; i < in.length; i++) {
        c = in.c_str[i];
        //printf("c:'%lc'\tph:\t%d\n", c, arg_phase);
        switch (arg_phase) {
        case template_ap_name_before:
            args_default[count] = nullstr;
            if (c == charl(' ') || c == charl('\t') || c == charl('\n') || c == charl('\r')) {
                continue;
            } else if ((c >= charl('a') && c <= charl('z')) || (c >= charl('A') && c <= charl('Z')) || (c >= charl('0') && c <= charl('9')) || c == charl('_')) {
                last_arg_begin_idx = i;
                args_split[count].c_str = in.c_str + i;
                arg_phase = template_ap_name;
                if (i == in.length - 1) { // if it's the very last character of the argument
                    arg_phase = template_ap_name_before;
                    args_split[count++].length = i - last_arg_begin_idx + 1;
                }
            } else if (c == '#') {
                arg_phase = template_ap_syntax_id;
                if (args_flag[count] & T_ARG_SYNTAX) {
                    throw_error("expected identifier in template parameters", E_EXIT);
                }
                args_flag[count] |= T_ARG_SYNTAX;
                args_syntax[count].c_str = in.c_str + i + 1;
                args_syntax[count].length = 0;
            } else {
                // throw excpetion
                throw_error("invalid character, expected name or syntax id in template parameters", E_EXIT);
            }
            break;
        case template_ap_name:
            if (c == charl(' ') || c == charl('\t') || c == charl('\n') || c == charl('\r')) {
                arg_phase = template_ap_name_after;
                args_split[count].length = i - last_arg_begin_idx;
            } else if (c == charl('?')) {
                arg_phase = template_ap_name_after;
                args_split[count].length = i - last_arg_begin_idx;
                args_flag[count] |= T_ARG_NULLABLE;
            } else if (c == charl('=')) {
                arg_phase = template_ap_value_before;
                args_split[count].length = i - last_arg_begin_idx;
            } else if (c == charl(',')) {
                arg_phase = template_ap_name_before;
                args_split[count++].length = i - last_arg_begin_idx;
            } else if (i == in.length - 1) {
                arg_phase = template_ap_name_before;
                args_split[count++].length = i - last_arg_begin_idx + 1;
            } else {
                // TODO: throw excpetion
            }
            break;
        case template_ap_syntax_id:
        //puts("SYNTX_ID");
            if (i == in.length - 1) {
                throw_error("expected parameter name in template directive", E_EXIT);
            } else if ((c >= charl('a') && c <= charl('z')) || (c >= charl('A') && c <= charl('Z')) || (c >= charl('0') && c <= charl('9')) || c == charl('_')) {
                args_syntax[count].length++;
            } else if (c == charl(' ') || c == charl('\t') || c == charl('\n') || c == charl('\r')) {
                if (args_syntax[count].length == 0) {
                    throw_error("expected syntax name after `#`", E_EXIT);
                }
                arg_phase = template_ap_name_before;
            }
            break;
        case template_ap_name_after:
            if (c == charl(' ') || c == charl('\t') || c == charl('\n') || c == charl('\r')) {

            } else if (c == charl(',') || i == in.length - 1) {
                count++;
                arg_phase = template_ap_name_before;
            } else if (c == charl('=')) { // default arg
                // for now throw exception, it will be implemented in the future
                arg_phase = template_ap_value_before;
            } else if (c == charl('?')) {
                args_flag[count] |= T_ARG_NULLABLE;
            } else {
                // TODO: throw excpetion
            }
            break;
        case template_ap_value_before:
            //puts("-----3-----");
            // what now :3
            if (c == charl(' ') || c == charl('\t') || c == charl('\n') || c == charl('\r')) {

            } else if (c == charl('"')) {
                args_default[count].c_str = in.c_str + i + 1;
                last_default_begin_idx = i + 1;
                delim = charl('"');
                arg_phase = template_ap_value;
            } else if (c == charl('\'')) {
                args_default[count].c_str = in.c_str + i + 1;
                last_default_begin_idx = i + 1;
                delim = charl('\'');
                arg_phase = template_ap_value;
            } else if (c == charl(',')) {
                // TODO: handle exception
                throw_error("Argument with default values must have a value", E_EXIT);
            } else if (c == '{') {
                luax_transpile_result o = luax_transpile(in.c_str + i + 1, in.length * 40 + 999999, /*&luax_ctx*/ ctx->luax_ctx);
                if (o.error) { // ofc a more accurate error log will be implemented
                    printf("[HTMW LuaX Error]: there is an error on the LuaX syntax (1)\n", o.error);
                    exit(1);
                }
                luax_str_view sv = o.result.first;
                // printf("%zu\n", sv.len);
                // printf("%zu\n", sv.len);
                // printf("%zu\n", sv.len);
                // printf("%zu\n", sv.len);
                // printf("%zu\n", sv.len);
                // printf("%zu\n", sv.len);
                //printf("%.*s\n", sv.len, sv.s);
                args_default[count].c_str = sv.s;
                args_default[count].length = sv.len;
                args_default[count].size = 0;
                args_default[count] = str_cpy(args_default[count]);
                i += o.result.second + 1;
                arg_phase = template_ap_value_after;
                args_flag[count] |= T_ARG_EXPR;
                //str_debug(args_default[count]);
                count++;
            } else {
                args_default[count].c_str = in.c_str + i;
                last_default_begin_idx = i;
                delim = charl('\0');
                arg_phase = template_ap_value;
            }
            break;
        case template_ap_value:
            // case4
            //if (count == 2) puts("3->4");
            if ((c == charl(' ') || c == charl('\t') || c == charl('\n') || c == charl('\r')) && delim == charl('\0')) {
                args_default[count++].length = i - last_default_begin_idx;
                arg_phase = template_ap_value_after;
            } else if (c == charl('"') && delim == charl('"')) {
                args_default[count++].length = i - last_default_begin_idx;
                arg_phase = template_ap_value_after;
            } else if (c == charl('\'') && delim == charl('\'')) {
                args_default[count++].length = i - last_default_begin_idx;
                arg_phase = template_ap_value_after;
            } else if (c == charl(',') && delim == charl('\0')) {
                args_default[count++].length = i - last_default_begin_idx;
                arg_phase = template_ap_name_before;
            } else if (i == in.length - 1 && delim == charl('\0')) {
                args_default[count++].length = i - last_default_begin_idx + 1;
                arg_phase = template_ap_name_before;
            } else {

            }
            break;
        case template_ap_value_after:
            if (c == charl(' ') || c == charl('\t') || c == charl('\n') || c == charl('\r')) {

            } else if (c == charl(',') || i == in.length - 1) {
                arg_phase = template_ap_name_before;
            } else {
                // TODO: throw excpetion
                throw_error("Unexpected character", E_EXIT);
            }
            break;
        }
                //puts(in.c_str + i + 1);
                //printf("argc: %zu, len: %zu\n", n, in.length);
                //str_debug(args_default[count]);
                //puts("=================================");
    }

    vect* args = vect_new(n, sizeof(ht_template_arg));
    VECT_FOR(args, i) {
        VECT_A(ht_template_arg, args)[i] = (ht_template_arg){
            str_cpy(args_split[i]),
            str_isnull(args_default[i]) ? nullstr : str_cpy(args_default[i]), // COPY
            args_flag[i],
            args_flag[i] & T_ARG_SYNTAX ? str_cpy(args_syntax[i]) : nullstr
        };
#if DEBUG_MODE
        ht_template_arg a = VECT_A(ht_template_arg, args)[i];
        printf("\nname: |%ls|\ndefault: |%ls|\nnullable: %d\n-------\n",
            a.name.c_str,
            a.default_value.c_str,
            flag_includes(a.flags, T_ARG_NULLABLE)
        );
#endif
    }

    t->args = args;

    free(args_split);
    free(args_flag);
    free(args_syntax);

    /*for (size_t i = 0; i < n; i++) {
        printf("<<<<< \"%ls\" >>>>>\n", VECT_A(string, args)[i].c_str);
    }*/
}

// converts wide string to string and constructs the function for the template constructor
char* generate_lua_template_constructor(const string name, const string impl, const vect* args, ht_template_core* core, flag_t flags) {
    //setlocale(LC_ALL, "");

    char** args_c_str = (char**)malloc(args->size * sizeof(char*));
    size_t* args_length = (size_t*)malloc(args->size * sizeof(size_t));

    char* name_c_str = (char*)malloc((name.length + 1) * sizeof(char));
    wcstombsw(name_c_str, name.c_str, name.length);
    name_c_str[name.length] = '\0';

    size_t total_args_len = 0;
    VECT_FOR(args, i) {
        ht_template_arg e = VECT_A(ht_template_arg, args)[i];
        args_c_str[i] = (char*)malloc((e.name.length + 1) * sizeof(char));
        wcstombsw(args_c_str[i], e.name.c_str, e.name.length);
        args_c_str[i][e.name.length] = '\0';
        args_length[i] = e.name.length;
        total_args_len += e.name.length;
    }
    
    /*
    * sum: "function " + "__template_construct_" + name + total_args + commas + parenthesis + new line + null terminator |new!| + (("INNER") + ", ") + (("ARGS") + ", ")
    */

    const size_t lua_constructor_signature_len = 9 + 21 + name.length + total_args_len + (args->size > 1 ? 2 * (args->size - 1) : 0) + 2 + 1 + 0 + ((flags & TEMPLATE_DINNER) ? (((flags & TEMPLATE_DARGS) || args->size > 0) ? 7 : 5) : 0) + ((flags & TEMPLATE_DARGS) ? ((args->size > 0) ? 6 : 4) : 0);
    //puts("cc");
    //printf("LASL: %zu\n", lua_constructor_signature_len);
    char* lua_contstructor_signature = (char*)malloc((lua_constructor_signature_len + 1) * sizeof(char));
    strncpy(lua_contstructor_signature, "function __template_construct_", 30);
    strncpy(lua_contstructor_signature + 30, name_c_str, name.length);
    size_t lua_contstructor_signature_idx = 30 + name.length;
    lua_contstructor_signature[lua_contstructor_signature_idx++] = '(';
    if (flags & TEMPLATE_DINNER) {
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'I';
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'N';
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'N';
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'E';
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'R';

        if ((flags & TEMPLATE_DARGS) || args->size > 0) {
            lua_contstructor_signature[lua_contstructor_signature_idx++] = ',';
            lua_contstructor_signature[lua_contstructor_signature_idx++] = ' ';
        }
    }

    if (flags & TEMPLATE_DARGS) {
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'A';
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'R';
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'G';
        lua_contstructor_signature[lua_contstructor_signature_idx++] = 'S';

        if (args->size > 0) {
            lua_contstructor_signature[lua_contstructor_signature_idx++] = ',';
            lua_contstructor_signature[lua_contstructor_signature_idx++] = ' ';
        }
    }
    VECT_FOR(args, i) {
        strncpy(lua_contstructor_signature + lua_contstructor_signature_idx, args_c_str[i], args_length[i]);
        lua_contstructor_signature_idx += args_length[i];
        if (i != args->size - 1) {
            lua_contstructor_signature[lua_contstructor_signature_idx++] = ',';
            lua_contstructor_signature[lua_contstructor_signature_idx++] = ' ';
        }
    }
    lua_contstructor_signature[lua_contstructor_signature_idx++] = ')';
    lua_contstructor_signature[lua_contstructor_signature_idx++] = '\n';
    lua_contstructor_signature[lua_contstructor_signature_idx++] = '\0';

    //printf("________construct____>>>> \"%s\" <<<<", lua_contstructor_signature);

    /*
    * sum: "return {" & "}" + `["` & `"]=` + names aggregation + commas + NO null terminator
    */
    // sum: (length of the block + 1) - (number of variables (remove null terminator)) - ("INNER" + null terminator)
    size_t aggr_core_names_len = core->shifts_count ? core->field_shifts[core->shifts_count - 1] - (core->shifts_count) - 6 : 0;
    //printf("..................>>>>>>>>....>>>> aggr_core_names_len: %zu <<<<\n", aggr_core_names_len);
    const size_t lua_ret_block_len = 1 + 9 + (core->shifts_count * 15) + (2 * aggr_core_names_len) + (core->shifts_count > 1 ? 1 * (core->shifts_count - 1) : 0) + 0;
    //printf("__%zu, %zu, %zu\n", (core->shifts_count * 15), (2 * aggr_core_names_len), (core->shifts_count > 1 ? 1 * (core->shifts_count - 1) : 0));
    //printf("_calc sz: %zu\n", lua_ret_block_len);
    char* lua_ret_block = (char*)malloc((lua_ret_block_len + 1) * sizeof(char));
    size_t lua_ret_block_idx = 9;
    strncpy(lua_ret_block, "\nreturn {", 10);

    //char* var_names_mb = (char*)malloc((core->field_shifts[core->shifts_count - 1] - core->field_shifts[0]) * sizeof(char));
    //wcstombsw(var_names_mb, core->field_names + core->field_shifts[0], core->field_shifts[core->shifts_count] - core->field_shifts[0] - 1);
    /*i < core->field_shifts[core->shifts_count] - core->field_shifts[0] - 1*/

    for (size_t i = 0; i < core->shifts_count; i++) {
        //printf("_idx: %zu\n", lua_ret_block_idx);
        size_t size = core->field_shifts[i] - (i ? core->field_shifts[i - 1] : 6);
        size_t shift = i ? core->field_shifts[i - 1] : 6;
        char* var_name = (char*)malloc(size * sizeof(char));
        wcstombsw(var_name, core->field_names + shift, size);
        var_name[size - 1] = '\0';
        //printf("|(size: %zu, shift: %zu)__%s__", size, shift, var_name);

        lua_ret_block[lua_ret_block_idx] = '[';
        lua_ret_block[lua_ret_block_idx + 1] = '"';
        strncpy(lua_ret_block + lua_ret_block_idx + 2, var_name, size - 1);
        lua_ret_block_idx += size + 1;
        lua_ret_block[lua_ret_block_idx] = '"';
        lua_ret_block[lua_ret_block_idx + 1] = ']';
        lua_ret_block[lua_ret_block_idx + 2] = '=';
        lua_ret_block[lua_ret_block_idx + 3] = 't';
        lua_ret_block[lua_ret_block_idx + 4] = 'o';
        lua_ret_block[lua_ret_block_idx + 5] = 'o';
        lua_ret_block[lua_ret_block_idx + 6] = 'p';
        lua_ret_block[lua_ret_block_idx + 7] = 't';
        lua_ret_block[lua_ret_block_idx + 8] = 's';
        lua_ret_block[lua_ret_block_idx + 9] = 't';
        lua_ret_block[lua_ret_block_idx + 10] = 'r';
        lua_ret_block[lua_ret_block_idx + 11] = '(';
        strncpy(lua_ret_block + lua_ret_block_idx + 12, var_name, size - 1);
        lua_ret_block_idx += size + 9;
        lua_ret_block[lua_ret_block_idx + 2] = ')';
        if (i != core->shifts_count - 1) {
            lua_ret_block[lua_ret_block_idx + 3] = ',';
            lua_ret_block_idx++;
        }
        lua_ret_block_idx += 3;

        free(var_name);
    }
    lua_ret_block[lua_ret_block_idx++] = '}';
    //printf("idx: %zu, len: %zu", lua_ret_block_idx, lua_ret_block_len);
    lua_ret_block[lua_ret_block_idx] = '\0';
    
    //printf("(sz: %zu)____%s____\n", lua_ret_block_idx, lua_ret_block);

    char* impl_c_str = (char*)malloc((impl.length + 1/* + 5*/) * sizeof(char));
    size_t impl_c_str_len = strlen(wcstombsw(impl_c_str, impl.c_str, impl.length));
    //strncpy(impl_c_str + impl.length, "\nend\n", 5);
    impl_c_str[impl.length] = '\0';

    char* out = (char*)malloc((lua_constructor_signature_len + impl.length + 5 + 1 + lua_ret_block_len) * sizeof(char));
    strncpy(out, lua_contstructor_signature, lua_constructor_signature_len);
    out[lua_constructor_signature_len] = '\0';
    //strncat(out, impl_c_str, lua_constructor_signature_len + impl.length + 5);
    strncat(out, impl_c_str, lua_constructor_signature_len + impl.length);
    out[lua_constructor_signature_len + impl.length] = '\0';
    strncat(out, lua_ret_block, lua_constructor_signature_len + impl.length + lua_ret_block_len);
    out[lua_constructor_signature_len + impl.length + lua_ret_block_len] = '\0';
    strncat(out, "\nend\n", lua_constructor_signature_len + impl.length + lua_ret_block_len + 5);
    out[lua_constructor_signature_len + lua_ret_block_len + impl.length + 5] = '\0';

    free(args_length);
    free(name_c_str);
    VECT_FOR(args, i) {
        free(args_c_str[i]);
    }
    free(args_c_str);
    free(lua_contstructor_signature);
    free(impl_c_str);

    //printf("\n\n\n%d\n\n\n%s\n\n\n\n\n\n", impl_c_str_len, impl_c_str);
    //printf("\n\n\n\n\n\n>>%s<<\n\n\n\n\n\n", out);
    return out;
}

/*typedef struct {
    field_location fl;
    string path;
    flag_t flags;
} include_frag;*/

typedef struct {
    size_t idx;
    size_t replaced_length;
    string path;
    string* content;
} include_dir;

#define INCLUDE_ONCE 1
#define INCLUDE_GLOB 2

#if NORM_INCLUDE_PIPELINE == 0
//
void htmw_apply_includes(string* in, u_map* includes, list* global_include_paths, int enable_network) {
    list* dirs = list_new();    // list of include_dir
    size_t difference = 0;
    charw c;
    //printf("in->length: %zu\n", in->length);
    for (size_t idx = 0; idx < in->length; idx++) {
        c = in->c_str[idx];
        //printf("i: %zu\tlen: %zu\tc: '%lc'\n", idx, in->length, c);
        if (c == charl('<')) {
            if (!strncmpw(in->c_str + idx + 1, strl("@include"), 8)) {
#if DEBUG_MODE
                //printf("after include kw: \"%lc\"\n", in->c_str[idx + 1 + 8]);
#endif
                charw c;
                uint8_t phase = 0;
                flag_t flags = 0;
                size_t path_begin_idx = 0;
                string view = nullstr;
                charw delim = charl('\0');
                include_dir* incl = (include_dir*)malloc(sizeof(include_dir));
                for (size_t i = 0; (c = in->c_str[idx + 1 + 8 + i]) != charl('\0'); i++) {
                    switch (c) {
                    case charl(' '):
                    case charl('\n'):
                    case charl('\t'):
                    case charl('\r'):
                        if (!phase) {
                            phase = 1;
                        } else if (phase == 4) {
                            throw_error("Tags cannot contain whitespaces at the end (after the '/')", E_EXIT);
                        }
                        break;
                    case charl('"'):
                    case charl('\''):
                        if (phase == 0 || phase == 1) {
                            view.c_str = in->c_str + idx + 1 + 8 + i + 1;
                            path_begin_idx = i;
                            phase = 2;
                            delim = c;
                        } else if (phase == 2) {
                            if (c == delim) {
                                view.length = i - 1 - path_begin_idx;
                                phase = 3;
                            }
                        } else {
                            throw_error("Invalid character ''' or '\"' in include directive", E_EXIT);
                        }
                        break;
                    case charl('<'):
                        if (phase == 0 || phase == 1) {
                            flags |= INCLUDE_GLOB;
                            view.c_str = in->c_str + idx + 1 + 8 + i + 1;
                            path_begin_idx = i;
                            phase = 2;
                            delim = charl('>');
                        } else {
#if DEBUG_MODE
                            //printf("after include: \"%lc\"\n", in->c_str[idx + 1 + 8 + i + 1]);
#endif
                            throw_error("Invalid character '<' in include directive", E_EXIT);
                        }
                        break;
                    case charl('>'):
#if DEBUG_MODE
                            //printf("phase: \"%u\"\n", phase);
#endif
                        if (phase == 2) {
                            if (delim == charl('>')) {
                                view.length = i - 1 - path_begin_idx;
                                phase = 3;
                            } else {
                                // throw exception?
                            }
                        } else if (phase == 3 || phase == 4) {
                            incl->idx = idx - difference;
                            incl->replaced_length = i + 9 + 1;
                            //printf("lenn: %zu\n", i + 9);
                            difference += incl->replaced_length;
                            goto include_for_break;
                        } else {
                            throw_error("No include path specified", E_EXIT);
                        }
                        break;
                    case charl('/'):
                        if (phase == 3)
                            phase = 4;
                        break;
                    default:
                        if (!phase) {
                            if (!strncmpw(in->c_str + idx + 1 + 8 + i, strl("_once"), 5)) {
                                flags |= INCLUDE_ONCE;
                                phase = 1;
                                i += 4;
                            } else {
                                throw_error("Invalid character in include directive", E_EXIT);
                            }
                        } else if (phase != 2) {
                            throw_error("Invalid character in include directive", E_EXIT);
                        }
                    }
                }
                include_for_break:;
#if DEBUG_MODE
                /*puts("<------------------->");
                printf("%zu\t%zu\t%p\n", view.length, view.size, view.c_str);
                printf("once: %d\n", flag_includes(flags, INCLUDE_ONCE));
                printf("glob: %d\n", flag_includes(flags, INCLUDE_GLOB));
                str_debug(view);*/
#endif
                //include_frag* incl = (include_frag*)malloc(sizeof(include_frag));
                //incl->path = str_cpy(view);
                //string name = str_cpy(view);
                string path;
                string* content = NULL;
                if (flag_includes(flags, INCLUDE_GLOB)) {
                    path = str_new(view.length + 2);
                    str_append_ch(&path, charl('>'));
                    //str_debug(path);
                    //str_debug(view);
                    //printf("-----%ls-----", view.c_str);
                    str_append(&path, view.c_str, view.length);
                } else {
                    path = str_cpy(view);
                }
                
                /////// READ FILE ///////
                if (!map_get(includes, path.c_str, &content)) {
                    //map_set(includes, path.c_str, (void*)content, 0);

                    content = (string*)malloc(sizeof(string));
                    if (flag_includes(flags, INCLUDE_GLOB)) {
                        
                        //char* path_c_str = (char*)malloc((path.length + 1 + 8) * sizeof(char));
                        //sprintf(path_c_str, "include/%ls", path.c_str + 1);
                        ////wcstombsw(path_c_str, path.c_str + 1, path.length);
                        //path_c_str[path.length + 8] = '\0';

                        
                        //if (!strncmp(path_c_str + 8, "https://", 8) || !strncmp(path_c_str + 8, "http://", 7)) {
                        //str_debug(path);
                        if (!strncmpw(path.c_str + 1, strl("https://"), 8) || !strncmpw(path.c_str + 1, strl("http://"), 7)) {
                            // TODO: implement HTTP Include
                            //throw_error("HTTP Include is not implemented yet", E_EXIT);
                            if (!enable_network)
                                throw_error("HTTP Include is disabled, use the `-N` flag to enable it", E_EXIT);
                            char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));//puts("b");
                            #if WCHAR_ENABLED
                            sprintf(path_c_str, "%ls", path.c_str + 1);
                            #else
                            sprintf(path_c_str, "%s", path.c_str + 1);
                            #endif
                            //wcstombsw(path_c_str, path.c_str + 1, );
                            path_c_str[path.length] = '\0';
                            *content = read_file_network(path_c_str);
                            htmw_apply_includes(content, includes, global_include_paths, enable_network);
                            free(path_c_str);
                        } else {
#if DEBUG_MODE
                            //printf("path: %s\n", path_c_str);
#endif

                            //printf("___________%zu\n", (path.length + GLOBAL_INCLUDE_PATH_LEN) * sizeof(char));
                            list_node* ln = global_include_paths->head;
                            char* path_c_str = NULL;
                            while (ln != NULL) {
                                const string* global_include_path = (string*)ln->data;
                                char* path_c_str_buffer = (char*)malloc((path.length + 1 + global_include_path->length) * sizeof(char));
                                #if WCHAR_ENABLED
                                sprintf(path_c_str_buffer, "%s/%ls", global_include_path->c_str, path.c_str + 1);
                                #else
                                sprintf(path_c_str_buffer, "%s/%s",  global_include_path->c_str, path.c_str + 1);
                                #endif
                                path_c_str_buffer[path.length + global_include_path->length] = '\0';
                                #ifdef _WIN32
                                    for (size_t i = 0; i < path.length; i++) {
                                        if (path_c_str[i] == '/') {
                                            path_c_str[i] = '\\';
                                        }
                                    }
                                #endif
                                // OPTIMIZATION NEEDED
                                const FILE* f = fopen(path_c_str_buffer, "r");
                                if (f != NULL) {
                                    path_c_str = path_c_str_buffer;
                                    fclose(f);
                                    break;
                                }
                                free(path_c_str_buffer);
                                ln = ln->next;
                            }
                            if (path_c_str == NULL) {
                                char msg[256];
                                sprintf(msg, "Couldn't open global include file: \"%s\"", path.c_str + 1);
                                throw_error(msg, E_EXIT);
                            }
                            *content = read_file(path_c_str, 1);
                            htmw_apply_includes(content, includes, global_include_paths, enable_network);
                            free(path_c_str);
                        }
                    
                    } else {
                        char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));
                        wcstombsw(path_c_str, path.c_str, path.length);
                        path_c_str[path.length] = '\0';
                        #ifdef _WIN32
                            for (size_t i = 0; i < path.length; i++) {
                                if (path_c_str[i] == '/') {
                                    path_c_str[i] = '\\';
                                }
                            }
                        #endif
                        *content = read_file(path_c_str, 0);
                        htmw_apply_includes(content, includes, global_include_paths, enable_network);
                        free(path_c_str);
                    }
                } else if (flag_includes(flags, INCLUDE_ONCE)) {
                    //puts("############################");
                    content = NULL;
                    //break;
                } else {
                    //printf("content: %p\n", content);
                    //////htmw_apply_includes(content, includes);



                    content = (string*)malloc(sizeof(string));
                    if (flag_includes(flags, INCLUDE_GLOB)) {
                        
                        ///////////////////////


                        if (!strncmpw(path.c_str + 1, strl("https://"), 8) || !strncmpw(path.c_str + 1, strl("http://"), 7)) {
                            // TODO: implement HTTP Include
                            //throw_error("HTTP Include is not implemented yet", E_EXIT);
                            if (!enable_network)
                                throw_error("HTTP Include is disabled, use the `-N` flag to enable it", E_EXIT);
                            char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));//puts("b");
                            #if WCHAR_ENABLED
                            sprintf(path_c_str, "%ls", path.c_str + 1);
                            #else
                            sprintf(path_c_str, "%s", path.c_str + 1);
                            #endif
                            //wcstombsw(path_c_str, path.c_str + 1, );
                            path_c_str[path.length] = '\0';
                            *content = read_file_network(path_c_str);
                            htmw_apply_includes(content, includes, global_include_paths, enable_network);
                            free(path_c_str);
                        } else {
#if DEBUG_MODE
                            //printf("path: %s\n", path_c_str);
#endif

                            //printf("___________%zu\n", (path.length + GLOBAL_INCLUDE_PATH_LEN) * sizeof(char));
                            list_node* ln = global_include_paths->head;
                            char* path_c_str = NULL;
                            while (ln != NULL) {
                                const string* global_include_path = (string*)ln->data;
                                char* path_c_str_buffer = (char*)malloc((path.length + 2 + global_include_path->length) * sizeof(char));
                                #if WCHAR_ENABLED
                                sprintf(path_c_str, "%s/%ls", global_include_path->c_str, path.c_str + 1);
                                #else
                                sprintf(path_c_str_buffer, "%s/%s",  global_include_path->c_str, path.c_str + 1);
                                #endif
                                path_c_str_buffer[path.length + global_include_path->length + 1] = '\0';
                                #ifdef _WIN32
                                    for (size_t i = 0; i < path.length; i++) {
                                        if (path_c_str[i] == '/') {
                                            path_c_str[i] = '\\';
                                        }
                                    }
                                #endif
                                // OPTIMIZATION NEEDED
                                const FILE* f = fopen(path_c_str_buffer, "r");
                                if (f != NULL) {
                                    path_c_str = path_c_str_buffer;
                                    fclose(f);
                                    break;
                                }
                                free(path_c_str_buffer);
                            }
                            if (path_c_str == NULL) {
                                char msg[256];
                                sprintf(msg, "Couldn't open global include path file: \"%s\"", path.c_str);
                                throw_error(msg, E_EXIT);
                            }
                            *content = read_file(path_c_str, 1);
                            htmw_apply_includes(content, includes, global_include_paths, enable_network);
                            free(path_c_str);
                        }
                    
                    } else {
                        char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));
                        wcstombsw(path_c_str, path.c_str, path.length);
                        path_c_str[path.length] = '\0';
                        #ifdef _WIN32
                            for (size_t i = 0; i < path.length; i++) {
                                if (path_c_str[i] == '/') {
                                    path_c_str[i] = '\\';
                                }
                            }
                        #endif
                        *content = read_file(path_c_str, 0);
                        htmw_apply_includes(content, includes, global_include_paths, enable_network);
                        free(path_c_str);
                    }


                        ////////////////////////


                    /*} else {
                        char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));
                        wcstombsw(path_c_str, path.c_str, path.length);
                        path_c_str[path.length] = '\0';
                        *content = read_file(path_c_str, 0);
                        htmw_apply_includes(content, includes);
                        free(path_c_str);
                    }*/


                }
                incl->path = path;
                incl->content = content;
#if DEBUG_MODE
                /*puts("_____________________________________");
                if (content != NULL)
                    str_debug(*content);
                else
                    puts("(null)");*/
#endif
                map_set(includes, path.c_str, (void*)content, 0);
                list_add(dirs, (void*)incl); // TODO: fix idx and length and replace includes with their file content
#if DEBUG_MODE
                //printf("idx: %zu\nlen: %zu\npath: \"%ls\"\n-------\n\n", incl->idx, incl->replaced_length, incl->path.c_str);
#endif
            }
        }
    }
#if DEBUG_MODE
    /*u_map_node* incl = includes->head;
    puts("---[U_MAP]---");
    while (incl != NULL) {
        include_dir* dir = (include_dir*)incl->data;
        if (dir != NULL)
            printf("\"%ls\"\n", dir->path);
        else puts("(null)");
        incl = incl->next;
    }
    puts("---[U_END]---");*/

    list_node* incl = dirs->head;
    puts("---[U_MAP]---");
    while (incl != NULL) {
        include_dir* dir = (include_dir*)incl->data;
        if (dir != NULL) {
            printf("\"%s\"\n", dir->path.c_str);
        } else puts("(null)");
        incl = incl->next;
    }
    puts("---[U_END]---");

    //printf("TOT diff: %zu\n", difference);
#endif
    size_t includes_len_tot = 0;

    for (size_t i = 0; i < dirs->size; i++) {
        include_dir* dir = (include_dir*)list_get(dirs, i);
        //str_debug(*dir->content);
    }

    list_node* ln = dirs->head;
    while (ln != NULL) {
        include_dir* dir = (include_dir*)ln->data;
        if (dir->content != NULL)
            includes_len_tot += dir->content->length;

        ln = ln->next;
    }

    //printf("TOT len: %zu\n", includes_len_tot);

    string n = str_new(in->length + includes_len_tot - difference + 1);

    ln = dirs->head;
    include_dir* dir = NULL;
    size_t i = 0;
    if (ln != NULL) {
        dir = (include_dir*)ln->data;
    } else goto include_rewrite_skip_cond;
    difference = 0;
    for (; i < in->length; i++) {
        //printf("i: %zu\n", i);
        if (/*dir != NULL && */i - difference == dir->idx) {
            string* content = dir->content;
            if (content != NULL) {
                for (size_t j = 0; j < content->length; j++) {
                    str_append_ch(&n, content->c_str[j]);
                    //printf(".");
                    //putwc(content->c_str[i], stdout);
                }
            }
            i += dir->replaced_length - 1;
            difference += dir->replaced_length;
            ln = ln->next;
            if (ln != NULL) {
                dir = (include_dir*)ln->data;
            } else {
                ++i;
                break;
            }
        } else {
            str_append_ch(&n, in->c_str[i]);
            //printf("_");
            //putwc(in->c_str[i], stdout);
        }
    }
    include_rewrite_skip_cond:
    for (; i < in->length; i++) {
        str_append_ch(&n, in->c_str[i]);
    }
    str_delete(in);
    *in = n;
}
#endif

#if NORM_INCLUDE_PIPELINE
// vT.7: Apply include, insert characters and automatically allocate memory if length exceeds size (double the size?)
/*
    in: entire text string to be processed
    content: content of the file to be included
    index: current character index position of the processing text
    difference: total length of the @include or @include_once
*/
void htmw_apply_include(string *in, string *content, size_t index, size_t difference) {
    string n;
    uint8_t flags = 0;

    string tmp;
    const static char nullch = '\0';
    if (content == NULL) {
        flags |= (1 << 2);
        tmp = (string) {
            .c_str = &nullch,
            .length = 0,
            .size = 0
        };
        content = &tmp;
    }

    size_t old_len = in->length;
    size_t new_len = old_len + content->length - difference;
    flags |= new_len > old_len ? (1 << 1) : 0;
    //size_t new_size = new_len + 1 + (flags & (1 << 1) ? content->length - difference : 0);

    //printf("%zu > %zu => %d\n", in->size, (old_len + content->length + 1), in->size > (old_len + content->length + 1));
    if (in->size < (old_len + content->length + 1)) {
        // TODO: keep the old string and free it later (already done), it will be used to fill the new string
        flags |= (1 << 0);
        //printf("newwwwwwwwwww %zu\n", old_len + content->length + 1);//  + difference + index + content->length + 100000
        n = str_new(old_len + content->length + 1); // <= FIXME (correct?)
        strncpy(n.c_str, in->c_str, index);
        n.c_str[index] = '\0';
    } else {
        //printf("olddddd %zu\n", in->size);
        n = *in;
    }

    size_t i;
    //size_t shift = index + content->length; // position where the existing text resumes
    n.length = new_len;

    char *s = n.c_str;// + index;
    /*size_t shift = (flags & (1 << 1))
        ? content->length - difference
        : difference - content->length;*/

    size_t shift;

    if (flags & (1 << 1)) {
        i = new_len;
        size_t j = 0;
        shift = content->length - difference;
        //printf("%zu > %zu\n", i, shift);
        //printf("======> %zu, %zu\n", in->size, index + i + difference - content->length - 1);
        //printf("||%c||\n", (in->c_str + index)[i + difference - content->length - 1]);
        //puts("aaaa");
        //printf("in->c_str: %zu\n", n.length);
        //printf("old_len: %zu\nnew_len: %zu\n", old_len, new_len);
        //printf("%zu\n", old_len - index - 1);
        //printf("old_len: %zu\nnew_len: %zu\n", old_len, new_len);
        //printf("---->>>%c\n", s[i - 1]);
        while (i > index + content->length) {
            //s[i-- - 1] = (in->c_str + index)[i + difference - content->length - 1]; // <-
            //printf("%zu, %zu\n", i - 1, old_len - j - 1);
            //printf("%c-", (in->c_str)[old_len - j]);
            s[i - 1] = in->c_str[old_len - j - 1];
            i--;
            j++;
        }//puts(s + i);
    } else if (new_len != old_len) {
        shift = difference - content->length;
        i = 0;
        //printf("old: %zu\nnew: %zu\nnl - d: %zu\n", old_len, new_len, new_len - (old_len - new_len));
        //printf("idx + i(max) + diff: %zu\n", index + new_len + difference);
        //printf("in + idx: \"%s\"\n", in->c_str + index);
        //printf("new_len - content->length: %zu\n", new_len - content->length);
        //printf("%zu\n", content->length);
        s = n.c_str;
        while (i < old_len - index/*(old_len - new_len)*/) {
            //s[i++ + content->length] = (in->c_str + index)[i + difference];
            s[i + content->length + index] = in->c_str[index + i + difference];
            i++;
        }//printf("i = %zu\n", i);
        //printf("--0-(%s)\n", n.c_str);
    }//puts("a");

    //printf("%zu\n", content->length);
    //printf("|||%s|||\n", content->c_str);
    for (i = 0; i < content->length; i++) {//printf("%c", content->c_str[0]);
        s[i + index] = content->c_str[i];
    }

    s[new_len] = '\0';

                //printf("--1-(%p)\n", n.c_str);
    if (flags & (1 << 0)) {
        str_delete(in);
        //in = malloc(sizeof(string));
    }
    *in = n;
                //printf("--2-(%s)\n", in->c_str);
}
#endif

// PSDV

// parses all the <@...> tags, and returns (in the context param now) a clean string with saved templates + state in the context struct
void htmw_preprocess(lua_State* L, string in, htmw_context* ectx, list *global_paths, int enable_network) {
    if (ectx == NULL) {
        return; // abort?
    }

    string post_in = str_new(MAX_LEN_OUT);
    #if NORM_INCLUDE_PIPELINE
    list* dirs = list_new();    // list of include_dir
    #endif
    #if 0
    u_map* areas;

    syntaxer_list syntax_routers;

    size_t *crv_size;
    luax_context luax_ctx;
    compiled_regex_vector *crv;
    #endif

    if (ectx->flags ^ CTX_INIT) {
        ectx->flags |= CTX_INIT;
        //areas = map_new();
        ectx->areas = *map_new(); // <- FIXME (it shouldn't return a pointer)
        luaw_init(L);
        //syntax_routers = syntaxer_list_null;
        size_t *crv_size = (size_t *)malloc(sizeof(size_t));
        *crv_size = 1;
        #if 0

        crv = (compiled_regex_vector *)malloc(sizeof(compiled_regex_vector));

        *crv = htmw_crv_new(HTMW_INIT_MEMV_SIZE);
        //printf("CRV CORE: %p\n", crv->data);
        //luax_ctx.crv = &crv;
        #endif
        #if 0
        luax_context luax_ctx;
        luax_ctx.crv = crv;

        luax_ctx.counters.rex = crv_size;
        luax_ctx.counters.xml = NULL;
        #endif

        // FIXME: do not heap allocate
        ectx->global_paths = list_new();
        ectx->includes = map_new();
        ectx->templates = map_new();
        //ectx->input = post_in;
        ectx->L = L;
        //flags,
        *ectx->syntax_routers = syntaxer_list_null;//syntax_routers;
        //ectx->crv = htmw_crv_new(HTMW_INIT_MEMV_SIZE);//*crv,//crv,
        *ectx->crv = htmw_crv_new(HTMW_INIT_MEMV_SIZE);
        *ectx->crv_size = 1; //*crv_size,
        *ectx->luax_ctx = (luax_context){
            .crv = ectx->crv,
            .counters = {
                .rex = ectx->crv_size,
                .xml = NULL
            }
        };//luax_ctx;


        //debug_htmw_crv(crv, *crv_size - 1);
    }
    #if 0
    else {
        areas = &ectx->areas;
        syntax_routers = ectx->syntax_routers;

        crv_size = &ectx->crv_size;

        crv = &ectx->crv;
        //*crv = ectx->crv;
        //luax_ctx.crv = &crv;
        luax_ctx.crv = &crv;
        luax_ctx.counters.rex = &ectx->crv_size;
    }
    #endif
    //debug_syntax_directives(&syntax_routers);
    //debug_htmw_crv(ectx->crv, *ectx->crv_size - 1);

    #if 0
    u_map* templates = ectx == NULL ? map_new() : ectx->templates;
    u_map* includes = ectx == NULL ? map_new() : ectx->includes;
    #endif

    //if (ectx == NULL)             commented out in case users want dynamic includes
    #if NORM_INCLUDE_PIPELINE == 0
    htmw_apply_includes(&in, includes, global_paths, enable_network);
    #endif

    int comment_mode = 0;
    int php_mode = 0;
    size_t ntxt_out_count = 0;
    size_t last_idx = 0;
    htmw_mode mode = h_null;
    size_t area_out_count = 0;

    charw c;

    ht_any current;
    string current_template_name = nullstr;
    size_t no_out_count = 0;

    uint8_t flags_status = 0;
    for (size_t idx = 0; (c = in.c_str[idx]) != charl('\0'); idx++) {
        //putwc(c, stdout);
        //printf("----\nmode: %d\nchar: %c\nidx: %zu\n", mode, in.c_str[idx], idx);
        switch (mode) {
        case h_null:
            /*if (in.c_str[idx + 1] == charl('\0')) {
                //last_idx = idx;
                str_append(&post_in, in.c_str + last_idx, idx - last_idx + 1);
            } */if (comment_mode) {
                if (c == charl('-')) {
                    if (!strncmpw(in.c_str + idx, strl("-->"), 3)) {
                        comment_mode = 0;
                        idx += 2;
                    } else continue;
                } else continue;
            }
            if (php_mode) {
                if (c == charl('?')) {
                    if (!strncmpw(in.c_str + idx, strl("?>"), 2)) {
                        php_mode = 0;
                        idx += 1;
                    } else continue;
                } else continue;
            }

            if (c == charl('<')) {
                //puts(in.c_str + idx);
                if (!strncmpw(in.c_str + idx + 1, strl("@template"), 9) && (in.c_str[idx + 10] == ' ' || in.c_str[idx + 10] == '\t' || in.c_str[idx + 10] == '\n' || in.c_str[idx + 10] == '\r') && area_out_count == 0) {
                    //str_append(&post_in, in.c_str + last_idx, idx - last_idx - 1);
                    mode = h_template;
                    ht_template* t = (ht_template*)malloc(sizeof(ht_template));
                    t->flags = 0;
                    t->inner = str_new(MAX_TEMPLATE_LEN);
                    t->impl = nullstr;
                    current.template = t;

                    charw c;
                    string name = str_new(128);
                    size_t i = 0;
                    idx += 10;
                    int sty = 0;
                    for (; (c = in.c_str[idx + i]) != charl('\0'); i++) {
                        if (c >= charl('a') && c <= charl('z') || c >= charl('A') && c <= charl('Z') || c >= charl('0') && c <= charl('9') || c == charl('_')/* || c == charl('-')*/) {
                            if (!sty && !strncmpw(in.c_str + idx + i, strl("ninner"), 6) && (in.c_str[idx + i + 6] == charl(' ') || in.c_str[idx + i + 6] == charl('\n') || in.c_str[idx + i + 6] == charl('\t') || in.c_str[idx + i + 6] == charl('\r'))) {
                                t->flags |= TEMPLATE_NINNER;
                                idx += 6;
                                continue;
                            } else if (!sty && !strncmpw(in.c_str + idx + i, strl("dinner"), 6) && (in.c_str[idx + i + 6] == charl(' ') || in.c_str[idx + i + 6] == charl('\n') || in.c_str[idx + i + 6] == charl('\t') || in.c_str[idx + i + 6] == charl('\r'))) {
                                t->flags |= TEMPLATE_DINNER;
                                idx += 6;
                                continue;
                            }
                            sty = 1;
                            str_append_ch(&name, c);
                        } else if (str_isempty(name)) {
                            continue;
                        } else {
                            break;
                        }
                    }
                    t->name = name;
                    string args = nullstr;// = str_new(2048);
                    string impl = nullstr;
                    int constructing_constructor = 0;
                    size_t constructor_begin_idx_args = 0;
                    size_t constructor_begin_idx_impl = 0;
                    size_t args_count = 0;
                    size_t args_len = 0;
                    size_t impl_idx_begin = 0;
                    int impl_inner_braces = 0;
                    charw in_str_type = '\0';
                    int in_str_backslashing = 0;
                    char lua_comment_mode = 0;
                    size_t temp = i;
                    for (; (c = in.c_str[idx + i]) != charl('\0'); i++) {
                        if (i - temp >= 60)
                            printf("");
                        if (in_str_backslashing) {
                            in_str_backslashing = 0;
                            continue;
                        }
                        switch (c) {
                        case charl('('):
                        {
                            if (constructing_constructor == 0) {
                                args_len = 0;
                                constructing_constructor = 1;
                                constructor_begin_idx_args = idx + i + 1;
                                args.c_str = in.c_str + constructor_begin_idx_args;
                            } else if (constructing_constructor == 1) {
                                args_len++;
                            } else {
                                // TODO: throw exception
                            }
                        } break;
                        case charl(','):
                        {
                            if (constructing_constructor == 1) {
                                args_len++;
                                if (in_str_type == charl('\0'))
                                    args_count++;
                            }
                        } break;
                        case charl(')'):
                        {
                            if (constructing_constructor == 1) {
                                if (in_str_type != charl('\0')) {
                                    args_len++;
                                    break;
                                }
                                constructing_constructor = 2;
                                args.length = args_len;
                                args.size = args_len + 1;
                                //printf(">>>>>>>>>>>>>> args_len: %u >>>>>>>>>>>\n", args_len);
                                parse_template_args(t, args, args_count, ectx);
                                t->args_min_size = args_len + 1 + (args_count * 5);
                            } else {
                                // TODO: throw exception
                            }
                        } break;
                        case charl('{'):
                        {
                            if (constructing_constructor == 2 || constructing_constructor == 0) {
                                constructing_constructor = 3;
                                constructor_begin_idx_impl = idx + i + 1;
                                impl_idx_begin = i;
                                impl.c_str = in.c_str + constructor_begin_idx_impl;
                                in_str_type = charl('\0');

                                luax_transpile_result o = luax_transpile(impl.c_str, 100000, /*&luax_ctx*/ ectx->luax_ctx);
                                if (o.error) { // ofc a more accurate error log will be implemented
                                    printf("[HTMW LuaX Error]: there is an error on the LuaX syntax (1)\n", o.error);
                                    exit(1);
                                }
                                luax_str_view sv = o.result.first;
                                impl.c_str = sv.s;
                                impl.length = sv.len;
                                impl.size = impl.length + 1;
                                i += o.result.second + 1;
                                constructing_constructor = 4;
                                t->impl = impl;//str_cpy(impl);

                                //printf("----%zu\n", i);
                                //str_debug(impl);
                            } else if (constructing_constructor == 3 && in_str_type == charl('\0') && !lua_comment_mode) {
                                impl_inner_braces++;
                            } else if (constructing_constructor == 1) {
                                //
                                //puts(in.c_str + idx + i + 1);
                                luax_transpile_result ro = luax_transpile(in.c_str + idx + i + 1, in.length - 1, NULL);
                                if (ro.error) { // ofc a more accurate error log will be implemented
                                    printf("[HTMW LuaX Error]: there is an error on the LuaX syntax (5)\n", ro.error);
                                    exit(1);
                                }
                                i += ro.result.second;
                                //
                                //args_len++;
                                args_len += ro.result.second + 1;
                            } else {
                                // TODO: throw exception
                            }
                        } break;
                        case charl('}'):
                        {
                            if (constructing_constructor == 3 && in_str_type == charl('\0') && !lua_comment_mode) {
                                if (!impl_inner_braces) {
                                    constructing_constructor = 4;
                                    impl.length = i - impl_idx_begin - 1;
                                    impl.size = impl.length + 1;
                                    t->impl = str_cpy(impl);
                                //printf("----%zu\n", i);
                                } else {
                                    impl_inner_braces--;
                                }
                            } else if (constructing_constructor == 1) {
                                args_len++;
                            } else {
                                // TODO: throw exception
                            }
                        } break;
                        case charl('\\'):
                        {
                            if (constructing_constructor == 3) {
                                if (in_str_type != charl('\0')) {
                                    in_str_backslashing = !in_str_backslashing;
                                }
                            }
                        } break;
                        case charl('"'):
                        case charl('\''):
                        {
                            if (constructing_constructor == 3 && !lua_comment_mode) {
                                if (in_str_type == charl('\0')) {
                                    in_str_type = c;
                                } else if (in_str_type == c && !in_str_backslashing) {
                                    in_str_type = charl('\0');
                                }
                            } else if (constructing_constructor == 1) {
                                // default args
                                if (in_str_type == charl('\0')) {
                                    in_str_type = c;
                                } else if (in_str_type == c/* && !in_str_backslashing*/) {
                                    in_str_type = charl('\0');
                                }
                                args_len++; //
                            } else {
                                // TODO: throw exception
                            }
                        } break;
                        case charl('\n'):
                        case charl('\r'):
                        {
                            if (constructing_constructor == 3) {
                                if (lua_comment_mode == 1) {
                                    lua_comment_mode = 0;
                                } else if (in_str_type == charl('"') || in_str_type == charl('\'')) {
                                    in_str_type = charl('\0');
                                }
                            }
                        }
                        case charl(' '):
                        case charl('\t'):
                        {
                            if (constructing_constructor == 1) {
                                args_len++;
                            }
                            //continue;
                            //goto pre_process_template_for_switch_continue;
                        } break;
                        case charl('>'):
                        {
                            if (constructing_constructor == 0) {
                                parse_template_args(t, args, args_count, ectx);
                                idx += i;
                                goto pre_process_for_switch_continue;
                            } else if (constructing_constructor == 1) {
                                args_len++;
                                // TODO: throw exception
                            } else if (constructing_constructor != 3) {
                                //printf("_____--------___%lc____%u\n\n", c, idx + i);
                                idx += i;
                                goto pre_process_for_switch_continue;
                                //break;
                            };
                        } break;
                        default:
                        {
                            if (constructing_constructor == 1) {
                                if (!args_count) args_count++;
                                args_len++;
                            } else if (constructing_constructor == 3) {
                                if (c == charl('-') && !lua_comment_mode && in_str_type == charl('\0')) {
                                    if (!strncmpw(in.c_str + idx + i, strl("--[["), 4)) {
                                        //puts("lua_comment_mode = 2");
                                        i += 3;
                                        lua_comment_mode = 2;
                                    } else if (!strncmpw(in.c_str + idx + i, strl("--"), 2)) {
                                        //puts("lua_comment_mode = 1");
                                        i++;
                                        lua_comment_mode = 1;
                                    }
                                } else {
                                    if (c == charl(']') && !strncmpw(in.c_str + idx + i, strl("]]"), 2)) {
                                        if (lua_comment_mode == 2) {
                                            i++;
                                            //puts("lua_comment_mode = 0");
                                            lua_comment_mode = 0;
                                        } else if (in_str_type = charl(']') && !in_str_backslashing) {
                                            i++;
                                            in_str_type = charl('\0');
                                        }
                                    } else if (c == charl('[') && !strncmpw(in.c_str + idx + i, strl("[["), 2) && !lua_comment_mode) {
                                        i++;
                                        in_str_type = charl('[');
                                    }
                                }
                            }
                        } break;
                        }
                        //pre_process_template_for_switch_continue:
                    }
                #if NORM_INCLUDE_PIPELINE
                    
                } else if (!strncmpw(in.c_str + idx + 1, strl("@include"), 8) && area_out_count == 0) {
#if DEBUG_MODE
                //printf("after include kw: \"%lc\"\n", in->c_str[idx + 1 + 8]);
#endif
                // CRASHES HERE: not exactly where, but after this the `in` is set to null
                charw c;
                uint8_t phase = 0;
                flag_t flags = 0;
                size_t path_begin_idx = 0;
                string view = nullstr;
                charw delim = charl('\0');
                include_dir* incl = (include_dir*)malloc(sizeof(include_dir));
                size_t difference = 0;
                for (size_t i = 0; (c = in.c_str[idx + 1 + 8 + i]) != charl('\0'); i++) {
                    //printf("iii: %zu => %c\n", i, c);
                    switch (c) {
                    case charl(' '):
                    case charl('\n'):
                    case charl('\t'):
                    case charl('\r'):
                        if (!phase) {
                            phase = 1;
                        } else if (phase == 4) {
                            throw_error("Tags cannot contain whitespaces at the end (after the '/')", E_EXIT);
                        }
                        break;
                    case charl('"'):
                    case charl('\''):
                        if (phase == 0 || phase == 1) {
                            view.c_str = in.c_str + idx + 1 + 8 + i + 1;
                            path_begin_idx = i;
                            phase = 2;
                            delim = c;
                        } else if (phase == 2) {
                            if (c == delim) {
                                view.length = i - 1 - path_begin_idx;
                                phase = 3;
                            }
                        } else {
                            throw_error("Invalid character ''' or '\"' in include directive", E_EXIT);
                        }
                        break;
                    case charl('<'):
                        if (phase == 0 || phase == 1) {
                            flags |= INCLUDE_GLOB;
                            view.c_str = in.c_str + idx + 1 + 8 + i + 1;
                            path_begin_idx = i;
                            phase = 2;
                            delim = charl('>');
                        } else {
#if DEBUG_MODE
                            //printf("after include: \"%lc\"\n", in->c_str[idx + 1 + 8 + i + 1]);
#endif
                            throw_error("Invalid character '<' in include directive", E_EXIT);
                        }
                        break;
                    case charl('>'):
#if DEBUG_MODE
                            //printf("phase: \"%u\"\n", phase);
#endif
                        if (phase == 2) {
                            if (delim == charl('>')) {
                                view.length = i - 1 - path_begin_idx;
                                phase = 3;
                            } else {
                                // throw exception?
                            }
                        } else if (phase == 3 || phase == 4) {
                            incl->idx = idx - difference;
                            incl->replaced_length = i + 9 + 1;
                            //printf("lenn: %zu\n", i + 9);
                            difference += incl->replaced_length;
                            goto include_for_break;
                        } else {
                            throw_error("No include path specified", E_EXIT);
                        }
                        break;
                    case charl('/'):
                        if (phase == 3)
                            phase = 4;
                        break;
                    default:
                        if (!phase) {
                            if (!strncmpw(in.c_str + idx + 1 + 8 + i, strl("_once"), 5)) {
                                flags |= INCLUDE_ONCE;
                                phase = 1;
                                i += 4;
                            } else {
                                throw_error("Invalid character in include directive", E_EXIT);
                            }
                        } else if (phase != 2) {
                            throw_error("Invalid character in include directive", E_EXIT);
                        }
                    }
                }
                include_for_break:;
#if DEBUG_MODE
                /*puts("<------------------->");
                printf("%zu\t%zu\t%p\n", view.length, view.size, view.c_str);
                printf("once: %d\n", flag_includes(flags, INCLUDE_ONCE));
                printf("glob: %d\n", flag_includes(flags, INCLUDE_GLOB));
                str_debug(view);*/
#endif
                //include_frag* incl = (include_frag*)malloc(sizeof(include_frag));
                //incl->path = str_cpy(view);
                //string name = str_cpy(view);
                string path;
                string* content = NULL;
                if (flag_includes(flags, INCLUDE_GLOB)) {
                    path = str_new(view.length + 2);
                    str_append_ch(&path, charl('>'));
                    //str_debug(path);
                    //str_debug(view);
                    //printf("-----%ls-----", view.c_str);
                    str_append(&path, view.c_str, view.length);
                } else {
                    path = str_cpy(view);
                }
                
                /////// READ FILE ///////
                if (!map_get(ectx->includes, path.c_str, (void **)&content)) {
                    //map_set(includes, path.c_str, (void*)content, 0);

                    content = (string*)malloc(sizeof(string));
                    if (flag_includes(flags, INCLUDE_GLOB)) {
                        
                        //char* path_c_str = (char*)malloc((path.length + 1 + 8) * sizeof(char));
                        //sprintf(path_c_str, "include/%ls", path.c_str + 1);
                        ////wcstombsw(path_c_str, path.c_str + 1, path.length);
                        //path_c_str[path.length + 8] = '\0';

                        
                        //if (!strncmp(path_c_str + 8, "https://", 8) || !strncmp(path_c_str + 8, "http://", 7)) {
                        //str_debug(path);
                        if (!strncmpw(path.c_str + 1, strl("https://"), 8) || !strncmpw(path.c_str + 1, strl("http://"), 7)) {
                            // TODO: implement HTTP Include
                            //throw_error("HTTP Include is not implemented yet", E_EXIT);
                            if (!enable_network)
                                throw_error("HTTP Include is disabled, use the `-N` flag to enable it", E_EXIT);
                            char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));//puts("b");
                            #if WCHAR_ENABLED
                            sprintf(path_c_str, "%ls", path.c_str + 1);
                            #else
                            sprintf(path_c_str, "%s", path.c_str + 1);
                            #endif
                            //wcstombsw(path_c_str, path.c_str + 1, );
                            path_c_str[path.length] = '\0';
                            *content = read_file_network(path_c_str);
                            //htmw_preprocess(L, *content, ectx, global_paths, enable_network);
                            free(path_c_str);
                        } else {
#if DEBUG_MODE
                            //printf("path: %s\n", path_c_str);
#endif

                            //printf("___________%zu\n", (path.length + GLOBAL_INCLUDE_PATH_LEN) * sizeof(char));
                            list_node* ln = global_paths->head;
                            char* path_c_str = NULL;
                            while (ln != NULL) {
                                const string* global_include_path = (string*)ln->data;
                                char* path_c_str_buffer = (char*)malloc((path.length + 1 + global_include_path->length) * sizeof(char));
                                #if WCHAR_ENABLED
                                sprintf(path_c_str_buffer, "%s/%ls", global_include_path->c_str, path.c_str + 1);
                                #else
                                sprintf(path_c_str_buffer, "%s/%s",  global_include_path->c_str, path.c_str + 1);
                                #endif
                                path_c_str_buffer[path.length + global_include_path->length] = '\0';
                                #ifdef _WIN32
                                    for (size_t i = 0; i < path.length; i++) {
                                        if (path_c_str[i] == '/') {
                                            path_c_str[i] = '\\';
                                        }
                                    }
                                #endif
                                // OPTIMIZATION NEEDED
                                const FILE* f = fopen(path_c_str_buffer, "r");
                                if (f != NULL) {
                                    path_c_str = path_c_str_buffer;
                                    fclose(f);
                                    break;
                                }
                                free(path_c_str_buffer);
                                ln = ln->next;
                            }
                            if (path_c_str == NULL) {
                                char msg[256];
                                sprintf(msg, "Couldn't open global include file: \"%s\"", path.c_str + 1);
                                throw_error(msg, E_EXIT);
                            }
                            *content = read_file(path_c_str, 1);
                            //htmw_preprocess(L, *content, ectx, global_paths, enable_network);
                            free(path_c_str);
                        }
                    
                    } else {
                        char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));
                        wcstombsw(path_c_str, path.c_str, path.length);
                        path_c_str[path.length] = '\0';
                        #ifdef _WIN32
                            for (size_t i = 0; i < path.length; i++) {
                                if (path_c_str[i] == '/') {
                                    path_c_str[i] = '\\';
                                }
                            }
                        #endif
                        *content = read_file(path_c_str, 0);
                        //htmw_preprocess(L, *content, ectx, global_paths, enable_network);
                        free(path_c_str);
                    }
                } else if (flag_includes(flags, INCLUDE_ONCE)) {
                    //puts("############################");
                    content = NULL;
                    //break;
                } else {
                    //printf("content: %p\n", content);
                    //////htmw_apply_includes(content, includes);



                    content = (string*)malloc(sizeof(string));
                    if (flag_includes(flags, INCLUDE_GLOB)) {
                        
                        ///////////////////////


                        if (!strncmpw(path.c_str + 1, strl("https://"), 8) || !strncmpw(path.c_str + 1, strl("http://"), 7)) {
                            // TODO: implement HTTP Include
                            //throw_error("HTTP Include is not implemented yet", E_EXIT);
                            if (!enable_network)
                                throw_error("HTTP Include is disabled, use the `-N` flag to enable it", E_EXIT);
                            char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));//puts("b");
                            #if WCHAR_ENABLED
                            sprintf(path_c_str, "%ls", path.c_str + 1);
                            #else
                            sprintf(path_c_str, "%s", path.c_str + 1);
                            #endif
                            //wcstombsw(path_c_str, path.c_str + 1, );
                            path_c_str[path.length] = '\0';
                            *content = read_file_network(path_c_str);
                            //htmw_preprocess(L, *content, ectx, global_paths, enable_network);
                            free(path_c_str);
                        } else {
#if DEBUG_MODE
                            //printf("path: %s\n", path_c_str);
#endif

                            //printf("___________%zu\n", (path.length + GLOBAL_INCLUDE_PATH_LEN) * sizeof(char));
                            list_node* ln = global_paths->head;
                            char* path_c_str = NULL;
                            while (ln != NULL) {
                                const string* global_include_path = (string*)ln->data;
                                char* path_c_str_buffer = (char*)malloc((path.length + 2 + global_include_path->length) * sizeof(char));
                                #if WCHAR_ENABLED
                                sprintf(path_c_str, "%s/%ls", global_include_path->c_str, path.c_str + 1);
                                #else
                                sprintf(path_c_str_buffer, "%s/%s",  global_include_path->c_str, path.c_str + 1);
                                #endif
                                path_c_str_buffer[path.length + global_include_path->length + 1] = '\0';
                                #ifdef _WIN32
                                    for (size_t i = 0; i < path.length; i++) {
                                        if (path_c_str[i] == '/') {
                                            path_c_str[i] = '\\';
                                        }
                                    }
                                #endif
                                // OPTIMIZATION NEEDED
                                const FILE* f = fopen(path_c_str_buffer, "r");
                                if (f != NULL) {
                                    path_c_str = path_c_str_buffer;
                                    fclose(f);
                                    break;
                                }
                                free(path_c_str_buffer);
                            }
                            if (path_c_str == NULL) {
                                char msg[256];
                                sprintf(msg, "Couldn't open global include path file: \"%s\"", path.c_str);
                                throw_error(msg, E_EXIT);
                            }
                            *content = read_file(path_c_str, 1);
                            //htmw_preprocess(L, *content, ectx, global_paths, enable_network);
                            free(path_c_str);
                        }
                    
                    } else {
                        char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));
                        wcstombsw(path_c_str, path.c_str, path.length);
                        path_c_str[path.length] = '\0';
                        #ifdef _WIN32
                            for (size_t i = 0; i < path.length; i++) {
                                if (path_c_str[i] == '/') {
                                    path_c_str[i] = '\\';
                                }
                            }
                        #endif
                        *content = read_file(path_c_str, 0);
                        //htmw_preprocess(L, *content, ectx, global_paths, enable_network);
                        free(path_c_str);
                    }


                        ////////////////////////


                    /*} else {
                        char* path_c_str = (char*)malloc((path.length + 1) * sizeof(char));
                        wcstombsw(path_c_str, path.c_str, path.length);
                        path_c_str[path.length] = '\0';
                        *content = read_file(path_c_str, 0);
                        htmw_apply_includes(content, includes);
                        free(path_c_str);
                    }*/


                }
                incl->path = path;
                incl->content = content;
#if DEBUG_MODE
                /*puts("_____________________________________");
                if (content != NULL)
                    str_debug(*content);
                else
                    puts("(null)");*/
#endif
                map_set(ectx->includes, path.c_str, (void*)content, 0);
                list_add(dirs, (void*)incl); // TODO: fix idx and length and replace includes with their file content
#if DEBUG_MODE
                //printf("idx: %zu\nlen: %zu\npath: \"%ls\"\n-------\n\n", incl->idx, incl->replaced_length, incl->path.c_str);
#endif
                //str_debug(*content);

                //printf("----(%p)\n", in.c_str);
                htmw_apply_include(&in, content, idx, difference); // <-
                //printf("----(%p)\n", in.c_str);
                idx--;
                //printf("---%zu---", idx);
                //str_append_ch(&post_in, in.c_str[idx]);
                //printf(">>>>>>>>>>>>>>>>>>>>>>>>>%s\n", in.c_str + idx);
                //ectx = malloc(sizeof(htmw_context));
                //*ectx = htmw_preprocess(L, in, ectx, global_paths, enable_network);
            
                    
                #endif // NORM_INCLUDE_PIPELINE
                } else if (!strncmpw(in.c_str + idx + 1, strl("@no>"), 4)) {
                    //puts("no mode");
                    ///str_append(&post_in, in.c_str + last_idx, idx - last_idx - 1); // NEGATIVE SUM ON ZERO
                    mode = h_no;
                    no_out_count = 0;
                    goto pre_process_for_switch_continue;
                } else if (!strncmpw(in.c_str + idx + 1, strl("@comment>"), 9)) {
                    //str_append(&post_in, in.c_str + last_idx, idx - last_idx - 1);
                    mode = h_comment;
                    goto pre_process_for_switch_continue;
                } else if (!strncmpw(in.c_str + idx + 1, strl("@ntxt>"), 6)) {
                    ntxt_out_count++;
                    idx += 6;
                    goto pre_process_for_switch_continue;
                } else if (!strncmpw(in.c_str + idx + 1, strl("/@ntxt>"), 7)) {
                    if (ntxt_out_count > 0) {
                        ntxt_out_count--;
                    }
                    idx += 7;
                    goto pre_process_for_switch_continue;
                } else if (!strncmpw(in.c_str + idx + 1, strl("@area"), 5) && (in.c_str[idx + 6] == ' ' || in.c_str[idx + 6] == '\t' || in.c_str[idx + 6] == '\n' || in.c_str[idx + 6] == '\r')) {
                    size_t i = 0;
                    charw c;
                    charw bounds = '\0';
                    uint8_t phase = 0;
                    // 0: <@area
                    // 1: <@area "...
                    // 2: <@area "..." 
                    // 3: <@area "...">
                    string area_id = str_new(128);
                    uint8_t self_enclosing = 0;
                    //size_t area_id_begin_idx = 0;
                    for (; (c = in.c_str[idx + i]) != charl('\0'); i++) {
                        switch (c)
                        {
                        case charl('"'):
                        case charl('\''):
                            if (phase == 0) {
                                bounds = c;
                                //area_id.c_str = in.c_str + (idx + 7 + i);
                                //area_id_begin_idx = i;
                                ++phase;
                            } else if (phase == 1) {
                                if (bounds == c) {
                                    //area_id.length = area_id_begin_idx - i - 1;
                                    ++phase;
                                } else {
                                    str_append_ch(&area_id, c);
                                }
                            } else {
                                // TODO: throw exception
                            }
                            break;
                        case charl('>'):
                            if (phase == 2) {
                                idx += i;
                                if (!map_get(&ectx->areas, area_id.c_str, NULL) && !area_out_count) {
                                    map_set(&ectx->areas, area_id.c_str, NULL, 0);
                                    //str_debug(area_id);
                                    current.str = area_id;
                                } else if (!self_enclosing) {
                                    area_out_count++;
                                }
                                str_delete(&area_id);
                                goto pre_process_for_switch_continue;
                            }
                            break;
                        case charl('\t'):
                        case charl(' '):
                            if (phase == 1) {
                                str_append_ch(&area_id, c);
                            }
                            break;
                        case charl('\r'):
                        case charl('\n'):
                            break;
                        case charl('/'):
                            if (phase == 1) {
                                str_append_ch(&area_id, c);
                            } else if (phase == 2) {
                                //str_debug(in);
                                //throw_error("area directive cannot be self enclosed", E_EXIT);
                                if (!self_enclosing)
                                    self_enclosing = 1;
                                else throw_error("self enclosing area directives have only one `/` at the end", E_EXIT);
                            } else {
                                // TODO: throw exception
                            }
                            break;
                        default:
                            if (phase == 1) {
                                str_append_ch(&area_id, c);
                            } else {
                                // TODO: throw exception
                            }
                            break;
                        }
                    }
                    str_delete(&area_id);
                    //mode = h_comment;
                    goto pre_process_for_switch_continue;
                } else if (c == charl('<') && !strncmpw(in.c_str + idx + 1, strl("/@area>"), 7)) {
                    if (area_out_count != 0)
                        area_out_count--;
                    idx += 7;
                } else if (!strncmp(in.c_str + idx + 1, "@syntax", 7) && (in.c_str[idx + 1 + 7] == ' ' || in.c_str[idx + 1 + 7] == '\n' || in.c_str[idx + 1 + 7] == '\r' || in.c_str[idx + 1 + 7] == '\t') && area_out_count == 0) {
                    uint8_t phase = 0;
                    size_t i = 0;
                    char *s = in.c_str + idx + 1 + 7;
                    char c;
                    //char *syntax_id = NULL;
                    string syntax_id = nullstr;
                    size_t syntax_id_begin_idx = 0;
                    uint8_t syntax_flags = 0;
                    //size_t syntax_id_len = 0;
                    // 0 <@syntax |
                    // 1 <@syntax j|
                    // 2 <@syntax js |
                    // 3 <@syntax js{ (call LuaX?)
                    // 4 <@syntax js{...} |
                    // 5 <@syntax js{...}>
                    for (i = 1; (c = s[i]) != charl('\0'); i++) {
                        switch (c)
                        {
                        case ' ':
                        case '\t':
                        case '\n':
                        case '\r':
                            if (phase == 2 || phase == 0 || phase == 4) {
                                // (RESUME)
                            } else if (phase == 1) {
                                syntax_id.length = i - syntax_id_begin_idx - 1;
                                phase = 2;
                            }
                            break;
                        case '{':
                            if (phase == 1) {
                                syntax_id.length = i - syntax_id_begin_idx - 1;
                                phase = 2;
                            }
                            if (phase == 2) {
                                //char temp = s[i - 1];
                                //s[i - 1] = '{';
                                luax_transpile_result o = luax_transpile(in.c_str + idx + 1 + i + 7 + 1, /*in.length*/ 10000, /*&luax_ctx*/ ectx->luax_ctx);
                                if (o.error) { // ofc a more accurate error log will be implemented
                                    printf("[HTMW LuaX Error]: there is an error on the LuaX syntax in syntax directive\n", o.error);
                                    exit(1);
                                }
                                //s[i - 1] = temp;
                                luax_str_view sv = o.result.first;
                                //puts(luax_sv_to_str(sv));
                                //s.c_str = sv.s;
                                //s.length = sv.len - 1;
                                //i += o.result.second + 1;
                                char *lua_buffer = (char *)malloc((sv.len + 2 + 7 + 1) * sizeof(char));
                                //strncpy(lua_buffer, sv.s, sv.len);
                                //lua_buffer[sv.len] = '}';
                                //lua_buffer[sv.len + 1] = '\0';

                                // sv.len + "return " + '{' + '}' + null terminator
                                snprintf(lua_buffer, sv.len + 2 + 7 + 1, "return {%s}", sv.s);
                                //printf("lua_buffer: |%s|\n", lua_buffer);
                                //puts(lua_buffer);

                                //htmw_process_syntax_directive_value(L, &syntax_routers, /* syntax name? */, lua_buffer);
                                char temp_subst = syntax_id.c_str[syntax_id.length + 1];
                                syntax_id.c_str[syntax_id.length + 1] = '\0';
                                htmw_process_syntax_directive_value(L, /*&syntax_routers*/ ectx->syntax_routers, syntax_id.c_str, lua_buffer);
                                //debug_syntax_directives(&syntax_routers);
                                syntax_id.c_str[syntax_id.length + 1] = temp_subst;

                                free(lua_buffer);

                                phase = 4;
                                //printf("o.result.second: %zu\n", o.result.second);
                                //puts(in.c_str + idx + i + 7 + 1 + 1);
                                i += o.result.second + 1;
                                //puts("-------a");
                                //puts(s + i + 1);
                                //puts("-------b");
                            }
                            break;
                        default:
                            //printf(">>>>>>>>>>>>>>>.. %c\n", c);
                            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                                if (phase == 0) {
                                    syntax_id.c_str = s + i;
                                    syntax_id_begin_idx = i;
                                    phase = 1;
                                } else if (phase != 1) {
                                    //printf("=%d\n", phase);
                                    throw_error("unexpected character in syntax directive", E_EXIT);
                                }
                            } else if (c == '>') {
                                //phase = 5;
                                goto htmw_preprocess_syntax_directive_exit_for;
                                break;
                            } else if (c == '/' && phase == 4 && (syntax_flags ^ (1 << 0))) {
                                syntax_flags |= (1 << 0);
                                //continue;
                            } else {
                                char error_msg[50];
                                //puts(in.c_str + idx + i);
                                sprintf(error_msg, "unexpected character '%c' in syntax directive", c);
                                throw_error(error_msg, E_EXIT);
                            }
                            break;
                        }
                    }
                    htmw_preprocess_syntax_directive_exit_for:
                    idx += i + 7 + 1;
                }/* (we're not ready for this yet) else if (!strncmp(in.c_str + idx + 1, "@kw", 3) && (in.c_str[idx + 1 + 3] == ' ' || in.c_str[idx + 1 + 3] == '\n' || in.c_str[idx + 1 + 3] == '\r' || in.c_str[idx + 1 + 3] == '\t') && area_out_count == 0) {
                    char *s = in.c_str + idx + 1 + 3;
                    char c;
                    for (size_t i = 0; (c = s[i]) != '\0'; i++) {
                        if (c == '/') {

                        } else {
                            
                        }
                        //regex_analyze_literal()
                    }
                } */else if (in.c_str[idx + 1] == '@' && area_out_count == 0) {
                    flags_status = 0;
                    for (size_t i = 0; in.c_str[idx + 2 + i] != '\0'; i++) {
                        //printf("%zu: %c\n", i, in.c_str[idx + 2 + i]);
                        switch (in.c_str[idx + 2 + i]) {
                        case charl(' '):
                        case charl('\t'):
                        case charl('\n'):
                        case charl('\r'):
                            break;
                        case charl('-'):
                            if (in.c_str[idx + 3 + i] == '>' && !(flags_status & (1 << 0))) { // <@ -> {}>
                                flags_status |= (1 << 0);
                                i++;
                            } else {
                                flags_status = 0;
                                throw_error("invalid `-` symbol or arrow operator for inline template", E_EXIT);
                            }
                            break;
                        case charl('{'):
                            //puts("blockkk");
                            mode = h_block;
                            idx += i + 1;
                        default:
                            goto pre_process_for_switch_continue;
                        }
                    }
                } else if (ntxt_out_count == 0 && area_out_count == 0) {
                    str_append_ch(&post_in, in.c_str[idx])
                }
                //printf("a");
                //map_set(templates, "")
            } else if (ntxt_out_count == 0 && area_out_count == 0) {
                str_append_ch(&post_in, in.c_str[idx])
            }
        pre_process_for_switch_continue:;
            break;
        case h_template:
            //puts("----------------");
            //printf("__/___%lc____", c);
            if (c == charl('<') && !strncmpw(in.c_str + idx + 1, strl("/@template>"), 11)) {
                //puts("___________________________________________________________________________________");
                mode = h_null;
                //puts(current.template->name.c_str);
                map_set(ectx->templates, current.template->name.c_str, current.template, 0);
#if DEBUG_MODE
                //printf(">>>>>>>>>>>>>>>>>>>>>>> \"%ls\" registered >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", current.template->name.c_str);
                //str_debug(current.template->impl);
#endif

                current.template->core = htmw_process_template_inner(*current.template);
                char* lua_impl = generate_lua_template_constructor(current.template->name, current.template->impl, current.template->args, current.template->core, current.template->flags);
                //printf("___>>>%s<<<___", lua_impl);

                int r = luaL_dostring(L, lua_impl);
                if (check_lua(L, r)) {
                    //luaL_dostring(L, "__template_construct_template('x', 'y', 'z')");
                    //htmw_process_template_inner(*current.template);
                }

                idx += 11;
                last_idx = idx + 1;
            } else {
                str_append_ch(&current.template->inner, c);
            }
            break;
        case h_no:
            if (c == charl('<')) {
                if (!strncmpw(in.c_str + idx + 1, strl("/@no>"), 5)) {
                    //puts("no end");
                    //no_out_count--;
                    //printf("1: %zu\n", no_out_count);
                    if (no_out_count-- == 0) {
                        //puts("no dnd");
                        mode = h_null;
                    }
                    idx += 5;
                    last_idx = idx + 1;
                } else if (!strncmpw(in.c_str + idx + 1, strl("@no>"), 4)) {
                    //puts("no nst");
                    no_out_count++;
                    //printf("2: %zu\n", no_out_count);
                    idx += 4;
                    last_idx = idx + 1;
                }
            }
            break;
        case h_comment:
            if (c == charl('<') && !strncmpw(in.c_str + idx + 1, strl("/@comment>"), 10)) {
                mode = h_null;
                idx += 10;
                last_idx = idx + 1;
            }
            break;
        case h_block: {//puts("block");
            luax_transpile_result o = luax_transpile(in.c_str + idx + 1, /*in.length*/ 10000, /*&luax_ctx*/ ectx->luax_ctx);
            if (o.error) { // ofc a more accurate error log will be implemented
                printf("[HTMW LuaX Error]: there is an error on the LuaX syntax (2)\n", o.error);
                exit(1);
            }
            luax_str_view sv = o.result.first;
            size_t arrow_shift = 0;
            //s.c_str = sv.s;
            //s.length = sv.len - 1;
            idx += o.result.second + 1;
            //printf("len: %zu\n", sv.len);

            size_t lin_len = sv.len;
            size_t tot_len = lin_len + (!(flags_status & (1 << 0)) ? 16 : 0) + 42 + 1;
            char* lin = (char*)malloc(tot_len * sizeof(char));
            /*lin[0] = '(';
            lin[1] = 'f';
            lin[2] = 'u';
            lin[3] = 'n';
            lin[4] = 'c';
            lin[5] = 't';
            lin[6] = 'i';
            lin[7] = 'o';
            lin[8] = 'n';
            lin[9] = '(';
            lin[10] = ')';
            lin[11] = 'r'; // <- ?
            lin[12] = 'e';
            lin[13] = 't';
            lin[14] = 'u';
            lin[15] = 'r';
            lin[16] = 'n';
            lin[17] = '(';
            if (flags_status & (1 << 0)) {
                lin[17 + 1] = 'f';
                lin[17 + 2] = 'u';
                lin[17 + 3] = 'n';
                lin[17 + 4] = 'c';
                lin[17 + 5] = 't';
                lin[17 + 6] = 'i';
                lin[17 + 7] = 'o';
                lin[17 + 8] = 'n';
                lin[17 + 9] = '(';
                lin[17 + 10] = ')';
                arrow_shift = 10;
            } else {
                lin[17 + 1] = 't';
                lin[17 + 2] = 'o';
                lin[17 + 3] = 'o';
                lin[17 + 4] = 'p';
                lin[17 + 5] = 't';
                lin[17 + 6] = 's';
                lin[17 + 7] = 't';
                lin[17 + 8] = 'r';
                lin[17 + 9] = '(';
                arrow_shift = 9;
            }
            strncpy(lin + 18 + arrow_shift, sv.s, lin_len);
            lin[lin_len + 18 + arrow_shift] = ')';
            if (flags_status & (1 << 0)) {
                lin[lin_len + 18 + arrow_shift] = '\n';
                lin[lin_len + 19 + arrow_shift] = 'e';
                lin[lin_len + 20 + arrow_shift] = 'n';
                lin[lin_len + 21 + arrow_shift] = 'd';
                lin[lin_len + 22 + arrow_shift] = ')';
                lin[lin_len + 23 + arrow_shift] = '(';
                lin[lin_len + 24 + arrow_shift] = ')';
                lin[lin_len + 25 + arrow_shift] = '\n';
                arrow_shift += 8;
            } else {
                lin[lin_len + 19 + arrow_shift] = ')';
                arrow_shift++;
            }
            //lin[lin_len + 18 - 1 + arrow_shift] = ')';
            lin[lin_len + 19 + arrow_shift] = 'e';
            lin[lin_len + 20 + arrow_shift] = 'n';
            lin[lin_len + 21 + arrow_shift] = 'd';
            lin[lin_len + 22 + arrow_shift] = ')'; // `)` ?
            lin[lin_len + 23 + arrow_shift] = '('; // `(` ?
            lin[lin_len + 24 + arrow_shift] = ')'; // `)` ?
            lin[lin_len + 25 + arrow_shift] = '\0';
            //lin[lin_len + 26 - 1 + arrow_shift] = '\0';*/
            //puts(lin);
            //lin[lin_len + 19 + arrow_shift] = '\0';
            //int r = luaL_dostring(L, lin);

            char temp = sv.s[sv.len];
            sv.s[sv.len] = '\0';

            if (flags_status & (1 << 0)) { // @ -> {}
                snprintf(lin, tot_len, "%s", sv.s);
            } else {
                snprintf(lin, tot_len, "__TEMP=(function()return(tooptstr(%s))end)()", sv.s);
            }
            //printf("%zu\n", strlen(sv.s));
            //printf("%zu\n", tot_len);
            //printf("---%s---\n", sv.s);
            //printf("===%s===\n", lin);

            int r = luaL_loadstring(L, lin);

            sv.s[sv.len] = temp;

            if (check_lua(L, r)) {
                if (lua_pcall(L, 0, !!(flags_status & (1 << 0)), 0) == LUA_OK) {
                    // ...
                    char* v = NULL;
                    //printf("%d\n", !(flags_status & (1 << 0)));
                    if (!(flags_status & (1 << 0)) && !ntxt_out_count) {
                        lua_getglobal(L, "__TEMP");
                        v = lua_tostring(L, -1);
                        //puts(v);
                    }
                    if (v != NULL) {
                        //puts(v);
                        if (!ntxt_out_count) {
                            for (size_t i = 0; v[i] != '\0'; i++)
                                str_append_ch(&post_in, v[i]);
                        }
                        lua_pop(L, 1);
                    }
                } else {
                    throw_error("Lua pcall is not ok", E_EXIT);
                }


            } else {
                throw_error("Lua status is not ok", E_EXIT);
            }
            free(lin);
            while (in.c_str[idx] != '>' && in.c_str[idx++] != '\0');
            flags_status = 0;
            mode = h_null;
        } break;
        }
    }

    #if 0

    flag_t flags = 0;
    if (ectx == NULL) {
        flags |= CTX_HEAD;
    }
    htmw_context ctx = (htmw_context) {
        *areas,
        global_paths,
        includes,
        templates,
        post_in,
        L,
        flags,
        syntax_routers,
        *crv,//crv,
        *crv_size,
        luax_ctx
    };

    return ctx;
    #endif

    ectx->input = post_in;
}

// generates a template core (bare bones template information) from a given template
ht_template_core* htmw_process_template_inner(ht_template t) {
    const string inner = t.inner;
    string ground = str_new(t.inner.length + 1);
    u_map* variables = map_new();
    list* fields = list_new();

    int dollar_mode = 0;
    size_t last_dollar_idx_begin = 0;
    size_t last_dollar_len = 0;

    size_t difference = 0;

    size_t* len_buffer;
    field_location* idx_buffer;
    size_t var_name_alloc_size_tot = 6;
    //list_add(var_idxs, idx);
    //out.fields_count = 0;
    for (size_t i = 0; i < inner.length + 1; i++) {
        charw c = inner.c_str[i];

        if (dollar_mode) {
            if (c >= charl('a') && c <= charl('z') || c >= charl('A') && c <= charl('Z') || c >= charl('0') && c <= charl('9') || c == charl('_')) {
                last_dollar_len++;
                continue;
            } else if (c == charl('$')) {
                if (!last_dollar_len) { // $$
                    dollar_mode = 0;
                    str_append_ch(&ground, charl('$'));
                    difference++;
                } else { // $x$
                    difference += last_dollar_len + 1;
                    //printf("diff: %zu\n", difference);
                    string view = (string){
                        .c_str = inner.c_str + last_dollar_idx_begin,
                        .length = last_dollar_len,
                        .size = last_dollar_len
                    };
                    string name = str_cpy(view);
                    idx_buffer = (field_location*)malloc(sizeof(field_location));
                    //out.fields_count++;
                    size_t map_find_result = map_get_counted(variables, name.c_str, NULL);
                    if (map_find_result != MAP_NPOS) {
                        //puts("\n\nalready registered\n\n");
                        *idx_buffer = (field_location){ i - difference, map_find_result + 1 };
                    } else {
                        if (strcmpw(name.c_str, strl("INNER")) || (t.flags & TEMPLATE_DINNER) != 0) { // vT.7 (2026-01-22)
                            len_buffer = (size_t*)malloc(sizeof(size_t));
                            *len_buffer = name.length;
                            var_name_alloc_size_tot += *len_buffer + 1;
                            //fputws(name.c_str, stdout);
                            map_set(variables, name.c_str, len_buffer, 0);
                            *idx_buffer = (field_location){ i - difference, variables->size };
                            //printf("|a: (pos: %zu, var: %zu)", idx_buffer->pos, idx_buffer->var);
                        } else {
                            *idx_buffer = (field_location){ i - difference, 0 };
                        }
                    }
                    list_add(fields, (void*)idx_buffer);
                    str_delete(&name);
                    //str_debug(view);
                    last_dollar_idx_begin = i + 1;
                    last_dollar_len = 0;
                }
            } else if (c == charl('#')) {
                dollar_mode = 0;
                difference += 2;
            } else {
                dollar_mode = 0;
                if (last_dollar_len) {
                    difference += last_dollar_len + 1;
                    //printf("diff: %zu\n", difference);
                    str_append_ch(&ground, c);
                    string view = (string){
                        .c_str = inner.c_str + last_dollar_idx_begin,
                        .length = last_dollar_len,
                        .size = last_dollar_len
                    };
                    string name = str_cpy(view);
                    idx_buffer = (field_location*)malloc(sizeof(field_location));
                    //out.fields_count++;
                    size_t map_find_result = map_get_counted(variables, name.c_str, NULL);
                    if (map_find_result != MAP_NPOS) {
                        //puts("\n\nalready registered\n\n");
                        *idx_buffer = (field_location){ i - difference, map_find_result + 1 };
                    } else {
                        if (strcmpw(name.c_str, strl("INNER")) || (t.flags & TEMPLATE_DINNER) != 0) { // vT.7 (2026-01-22)
                            len_buffer = (size_t*)malloc(sizeof(size_t));
                            *len_buffer = name.length;
                            var_name_alloc_size_tot += *len_buffer + 1;
                            //fputws(name.c_str, stdout);
                            map_set(variables, name.c_str, len_buffer, 0);
                            *idx_buffer = (field_location){ i - difference, variables->size };
                            //printf("|b: (pos: %zu, var: %zu)", idx_buffer->pos, idx_buffer->var);
                        } else {
                            *idx_buffer = (field_location){ i - difference, 0 };
                        }
                    }
                    list_add(fields, (void*)idx_buffer);
                    str_delete(&name);
                    //str_debug(view);
                    last_dollar_len = 0;
                } else {
                    difference++;
                    str_append_ch(&ground, c);
                    //str_append_ch(&ground, charl('$'));
                }
            }
        } else if (c == charl('$')) {
            dollar_mode = 1;
            last_dollar_idx_begin = i + 1;
            last_dollar_len = 0;
        } else {
            str_append_ch(&ground, c);
        }
    }

    ht_template_core* out = (ht_template_core*)malloc(sizeof(ht_template_core));
    out->fields_count = fields->size;
    out->shifts_count = variables->size;
    out->field_names = (charw*)malloc(var_name_alloc_size_tot * sizeof(charw));
    out->field_shifts = (size_t*)malloc(variables->size * sizeof(size_t));
    out->fields = (field_location*)malloc(fields->size * sizeof(field_location));
    out->ground = ground;

    strncpyw(out->field_names, strl("INNER"), 5);
    out->field_names[5] = charl('\0');

    size_t shift = 6;
    for (size_t i = 0; i < variables->size; i++) {
        u_map_node* umn = mapn_at(variables->head, i);
        //printf("<--token: %ls->>>", umn->token);
        //printf("<--token: %zu->>>", *(size_t*)umn->data);
        strncpyw(out->field_names + shift, umn->token, *(size_t*)umn->data);
        shift += *(size_t*)umn->data + 1;
        out->field_shifts[i] = shift;
        //printf("<--shift: %zu->>>", shift);
        free(umn->data);
        out->field_names[shift - 1] = charl('\0');
    }
    //out->field_shifts[variables->size] = shift;
    out->field_names[var_name_alloc_size_tot - 1] = charl('\0');

    list_node* ln = fields->head;
    for (size_t i = 0; i < fields->size; i++) {
        if (i > 0) {
            ln = ln->next;
        }
        out->fields[i] = *(field_location*)ln->data;
        free(ln->data);
        //printf("(pos: %zu, var: %zu)", out->fields[i].pos, out->fields[i].var);
    }

    map_delete(variables);
    list_delete(fields);

    /*printf("\n---------------%ls-----------\n\n", out.field_names);
    for (size_t i = 0; i < variables->size; i++) {
        printf("\n---------------%ls-----------\n\n", out.field_names + out.field_shifts[i]);
    }*/
    /*for (size_t i = 0; i < var_name_alloc_size_tot; i++) {
        printf("__%lc__|", out->field_names[i]);
    }
    printf("|fields_count: %zu\n", out->fields_count);
    printf("|shifts_count: %zu\n", out->shifts_count);*/
    //str_debug(ground);
    //printf("&out: %p\n", &out);
    return out;
}

// returns a string in a fixed size allocated memory given the `core`, the `values` array and the `inner` value
string template_fill(ht_template_core* core, const string* values, const string inner) {
    size_t difference = 0;
    for (size_t i = 0; i < core->fields_count; i++) {
        size_t var = core->fields[i].idx;
        if (var) {
            difference += values[core->fields[i].idx - 1].length;
        } else {
            difference += inner.length;
        }
    }
    size_t len = core->ground.length + difference;
    string out = str_new(len + 1);
    size_t list_shift = 0;
    for (size_t i = 0; i < core->ground.length; i++) {
        field_location f = list_shift < core->fields_count ? core->fields[list_shift] : (field_location) { 0, 0 };
        if (list_shift < core->fields_count && i == f.pos) {
            string v = f.idx ? values[f.idx - 1] : inner;
            for (size_t j = 0; j < v.length; j++) {
                str_append_ch(&out, v.c_str[j]);
            }
            list_shift++;
            i--;
        } else {
            str_append_ch(&out, core->ground.c_str[i]);
        }
    }
    //printf("templated -%ls-", out.c_str);
    //printf("expected length: %zu\nlength: %zu\n", len, out.length);
    return out;
}

#define NEW_COMPILE_TXT 1
#define NEW_PROCESS_TEMPLATE NEW_COMPILE_TXT
#if NEW_PROCESS_TEMPLATE
// process the template and returns a string
string process_template(const htmw_context *ctx, const ht_template* t, const strplex *args, size_t args_begin[], const string inner) {
    const ht_template_core *core = t->core;
    lua_State *L = ctx->L;

    char* construct_name = (char*)malloc((t->name.length + 21 + 1) * sizeof(char));
    strncpy(construct_name, "__template_construct_", 21);
    strncpy(construct_name + 21, t->name.c_str, t->name.length);
    construct_name[t->name.length + 21] = '\0';

    lua_getglobal(L, construct_name);
    if (t->flags & TEMPLATE_DINNER) { // vT.7 (2026-01-22)
        lua_pushstring(L, inner.c_str);
    }//printf("FOR \"%s\"\n", t->name.c_str);
    for (size_t i = 0; i < t->args->size; i++) {
        const strplex_element *arg = (strplex_element *)(args->data + args_begin[i]);
        if (arg->meta & T_ARGV_NONE) { // pass nil ('\b' flag set)
            lua_pushnil(L);
        }
        else if (arg->meta & T_ARGV_VALUE) { // pass lua expr ('\r' flag set)
            // TODO: THIS THING SHOULD BE OPTIMIZED USING THE NEW LUAX TRANSPILER
            //puts("--- arg->meta & T_ARGV_VALUE");
            //puts(arg->value);
            size_t lin_len = (size_t)strplex_length(arg);
            char* lin = (char*)malloc((lin_len + 9 + 1) * sizeof(char));
            lin[0] = 'r';
            lin[1] = 'e';
            lin[2] = 't';
            lin[3] = 'u';
            lin[4] = 'r';
            lin[5] = 'n';
            lin[6] = ' ';
            lin[7] = '(';
            strncpy(lin + 8, arg->value, lin_len);
            lin[lin_len + 8] = ')';
            lin[lin_len + 9] = '\0';
            int r = luaL_loadstring(L, lin);
            if (check_lua(L, r)) {
                if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
                    // ...
                } else {
                    throw_error("Lua pcall is not ok", E_EXIT);
                }
            } else {
                throw_error("Lua status is not ok", E_EXIT);
            }
            free(lin);
        } else {  // if arg value is not null
            //puts("--- ELSEEE");
            //puts(arg->value);
            lua_pushstring(L, arg->value);
        }
    }

    if (check_lua(L, lua_pcall(L, t->args->size + !!(t->flags & TEMPLATE_DINNER), 1, 0))) {
        if (lua_istable(L, -1)) {
            string* lvs = (string*)malloc(core->shifts_count * sizeof(string));
            for (size_t i = 0; i < core->shifts_count; i++) {
                const char *k; // #DETLEN (arg name)
                //wcstombsw(k, core->field_names + (i ? core->field_shifts[i - 1] : 6), 4095);
                k = core->field_names + (i ? core->field_shifts[i - 1] : 6);
                //k[core->field_shifts[i] - 1] = '\0';
                //printf(">>>>> %s\n", k);
                lua_pushstring(L, k);
                //lua_pushlstring(L, k, core->field_shifts[i] - 1 - 1);
                lua_gettable(L, -2);
                const char *v = lua_tostring(L, -1);

                #if 0
                lvs[i] = str_new(4096); // #DETLEN
                                        // i forgot what `lvs` stands for maybe "long value string"? (it was a `wchar_t *`)
                                        // or maybe "long valueS" because it has multiple values
                char c;
                for (size_t j = 0; (c = v[j]) != '\0'; j++) {
                    charw wc;
                    #if WCHAR_ENABLED
                    int r = mbtowc(&wc, v + j, MB_CUR_MAX);
                    #else
                    wc = c;
                    #endif
                    //printf("i: %d => %lc\n" , r, wc);
                    str_append_ch(&lvs[i], wc);
                }
                #endif
                //printf("lv >> %ls\n", lvs[i].c_str);
                //printf("v >> %s\n", v);
                lvs[i] = str_from(v);
                //printf("lv >> %s\n", lvs[i].c_str);
                lua_pop(L, 1);
            }
            string filled = template_fill(core, lvs, inner);
            for (size_t i = 0; i < core->shifts_count; i++) {
                str_delete(&lvs[i]);
            }
            free(lvs);
            // TODO: re-process AND FREE MEMORY
            htmw_context inner_ctx = *ctx; // TODO: deep copy??
            htmw_preprocess(ctx->L, filled, &inner_ctx, ctx->global_paths, ctx->flags & CTX_NETWORK);
            txt_template_fill_data tfd = htmw_process_txt(&inner_ctx);
            
            if (tfd.fields_count) {
                string out = htmw_compile_txt(&inner_ctx, &tfd);
                lua_pop(L, 1);
                return out;
            } else {
                lua_pop(L, 1);
                return inner_ctx.input;
            }
            //string out = str_new();
            
            //return filled;
        } else {
            // TODO: handle exception
        }
    }
}
#else
// process the template and returns a string
string process_template(const htmw_context *ctx, const ht_template* t, const charw** args, const string inner) {
    //debug_lua_stack(ctx.L);
    ht_template_core* core = t->core;
    lua_State* L = ctx->L;

    //char* template_name = (char*)malloc((t->name.length + 1) * sizeof(char));
    char* construct_name = (char*)malloc((t->name.length + 21 + 1) * sizeof(char));
    strncpy(construct_name, "__template_construct_", 21);
    wcstombsw(construct_name + 21, t->name.c_str, t->name.length);
    construct_name[t->name.length + 21] = '\0';

    lua_getglobal(L, construct_name);
    //ht_template_arg* = t->args->data
    if (t->flags & TEMPLATE_DINNER) { // vT.7 (2026-01-22)
        /*char* inner_dup = (char*)malloc((inner.length + 1) * sizeof(char));
        strncpy(inner_dup, inner.c_str, inner.length);
        inner_dup[inner.length] = '\0';
        lua_pushstring(L, inner_dup);*/
        lua_pushstring(L, inner.c_str);
        //lua_pushnil(L);
    }
    for (size_t i = 0; i < t->args->size; i++) {
        //printf("> |%s|\n", args[i]);
        if (args[i][0] == charl('\b')) { // pass nil ('\b' flag set)
            lua_pushnil(L);
        } else if (args[i][0] == charl('\r')) { // pass lua expr ('\r' flag set)
            //char* lin = (char*)malloc((wlin.length + 1 + 16 + 1 - 1) * sizeof(char)); // len + null terminator + "return tostring(" + ")" - "\r"
            
            // TODO: THIS THING SHOULD BE OPTIMIZED USING THE NEW LUAX TRANSPILER
            size_t lin_len = strlenw(args[i]);
            char* lin = (char*)malloc((lin_len + 9 + 1) * sizeof(char));//printf("-----------|%ls|---\n", args[i] + 1);
            lin[0] = 'r';
            lin[1] = 'e';
            lin[2] = 't';
            lin[3] = 'u';
            lin[4] = 'r';
            lin[5] = 'n';
            lin[6] = ' ';
            lin[7] = '(';
            wcstombsw(lin + 8, args[i] + 1, lin_len - 1);
            lin[lin_len + 8 - 1] = ')';
            lin[lin_len + 9 - 1] = '\0';
            int r = luaL_loadstring(L, lin);
            if (check_lua(L, r)) {
                if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
                    // ...
                } else {
                    throw_error("Lua pcall is not ok", E_EXIT);
                }
            } else {
                throw_error("Lua status is not ok", E_EXIT);
            }
            free(lin);
        } else {  // if arg value is not null
            ht_template_arg a = VECT_A(ht_template_arg, t->args)[i];

            char arg[4096];
            //printf(">>>> %p\n", args[i]);
            //printf("2) %p -> \"%ls\"\n", args[i], args[i]);
            wcstombsw(arg, args[i], 4095);
            lua_pushstring(L, arg);
        }
    }//debug_lua_stack(L);

    if (check_lua(L, lua_pcall(L, t->args->size + ((t->flags & TEMPLATE_DINNER) ? 1 : 0), 1, 0))) {
        //setlocale(LC_ALL, "");
        if (lua_istable(L, -1)) {
            string* lvs = (string*)malloc(core->shifts_count * sizeof(string));
            for (size_t i = 0; i < core->shifts_count; i++) {
                char k[4096]; // #DETLEN (arg name)
                wcstombsw(k, core->field_names + (i ? core->field_shifts[i - 1] : 6), 4095);
                //char *k = core->field_names + (i ? core->field_shifts[i - 1] : 6);
                k[core->field_shifts[i] - 1] = '\0';
                lua_pushstring(L, k);
                lua_gettable(L, -2);
                char* v = lua_tostring(L, -1);

                lvs[i] = str_new(4096); // #DETLEN
                                        // i forgot what `lvs` stands for maybe "long value string"? (it was a `wchar_t *`)
                char c;
                for (size_t j = 0; (c = v[j]) != '\0'; j++) {
                    charw wc;
                    #if WCHAR_ENABLED
                    int r = mbtowc(&wc, v + j, MB_CUR_MAX);
                    #else
                    wc = c;
                    #endif
                    //printf("i: %d => %lc\n" , r, wc);
                    str_append_ch(&lvs[i], wc);
                }
                lua_pop(L, 1);

                //printf("lv >> %ls\n", lvs[i].c_str);
            }
            string filled = template_fill(core, lvs, inner);
            for (size_t i = 0; i < core->shifts_count; i++) {
                str_delete(&lvs[i]);
            }
            free(lvs);
            // TODO: re-process AND FREE MEMORY
            htmw_context inner_ctx = *ctx; // TODO: deep copy??
            htmw_preprocess(ctx->L, filled, &inner_ctx, ctx->global_paths, ctx->flags & CTX_NETWORK);
            txt_template_fill_data tfd = htmw_process_txt(&inner_ctx);
            //str_debug(inner_ctx.input);
            //printf("___%ls\n", ((ht_template_core*)inner_ctx.templates->head->next->data)->ground.c_str);
            
            if (tfd.fields_count) {
                string out = htmw_compile_txt(&inner_ctx, &tfd);
                lua_pop(L, 1);
                //debug_lua_stack(L);puts("================================================o1");
                return out;
            } else {
                lua_pop(L, 1);
                //debug_lua_stack(L);puts("================================================o2");
                return inner_ctx.input;
            }
            //string out = str_new();
            
            //return filled;
        } else {
            // TODO: handle exception
        }
    }
}
#endif
#if 0
// process the template and returns a string
string process_template(const htmw_context *ctx, const ht_template* t, const charw** args, const string inner) {
    //debug_lua_stack(ctx.L);
    ht_template_core* core = t->core;
    lua_State* L = ctx->L;

    //char* template_name = (char*)malloc((t->name.length + 1) * sizeof(char));
    char* construct_name = (char*)malloc((t->name.length + 21 + 1) * sizeof(char));
    strncpy(construct_name, "__template_construct_", 21);
    wcstombsw(construct_name + 21, t->name.c_str, t->name.length);
    construct_name[t->name.length + 21] = '\0';

    lua_getglobal(L, construct_name);
    //ht_template_arg* = t->args->data
    if (t->flags & TEMPLATE_DINNER) { // vT.7 (2026-01-22)
        /*char* inner_dup = (char*)malloc((inner.length + 1) * sizeof(char));
        strncpy(inner_dup, inner.c_str, inner.length);
        inner_dup[inner.length] = '\0';
        lua_pushstring(L, inner_dup);*/
        lua_pushstring(L, inner.c_str);
        //lua_pushnil(L);
    }
    for (size_t i = 0; i < t->args->size; i++) {
        //printf("> |%s|\n", args[i]);
        if (args[i][0] == charl('\b')) { // pass nil ('\b' flag set)
            lua_pushnil(L);
        }
        else if (args[i][0] == charl('\r') || args[i][1] == charl('\r')) { // pass lua expr ('\r' flag set)
            //char* lin = (char*)malloc((wlin.length + 1 + 16 + 1 - 1) * sizeof(char)); // len + null terminator + "return tostring(" + ")" - "\r"
            uint8_t subtract = args[i][1] == '\r' ? 2 : 1;
            // TODO: THIS THING SHOULD BE OPTIMIZED USING THE NEW LUAX TRANSPILER
            size_t lin_len = strlenw(args[i]);
            char* lin = (char*)malloc((lin_len + 9 + 1) * sizeof(char));//printf("-----------|%ls|---\n", args[i] + 1);
            lin[0] = 'r';
            lin[1] = 'e';
            lin[2] = 't';
            lin[3] = 'u';
            lin[4] = 'r';
            lin[5] = 'n';
            lin[6] = ' ';
            lin[7] = '(';
            wcstombsw(lin + 8, args[i] + subtract, lin_len - subtract);
            lin[lin_len + 8 - subtract] = ')';
            lin[lin_len + 9 - subtract] = '\0';
            int r = luaL_loadstring(L, lin);
            if (check_lua(L, r)) {
                if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
                    // ...
                } else {
                    throw_error("Lua pcall is not ok", E_EXIT);
                }
            } else {
                throw_error("Lua status is not ok", E_EXIT);
            }
            free(lin);
        } else {  // if arg value is not null
            ht_template_arg a = VECT_A(ht_template_arg, t->args)[i];

            char arg[4096];
            //printf(">>>> %p\n", args[i]);
            //printf("2) %p -> \"%ls\"\n", args[i], args[i]);
            wcstombsw(arg, args[i], 4095);
            lua_pushstring(L, arg);
        }
    }//debug_lua_stack(L);

    if (check_lua(L, lua_pcall(L, t->args->size + ((t->flags & TEMPLATE_DINNER) ? 1 : 0), 1, 0))) {
        //setlocale(LC_ALL, "");
        if (lua_istable(L, -1)) {
            string* lvs = (string*)malloc(core->shifts_count * sizeof(string));
            for (size_t i = 0; i < core->shifts_count; i++) {
                char k[4096]; // #DETLEN (arg name)
                wcstombsw(k, core->field_names + (i ? core->field_shifts[i - 1] : 6), 4095);
                //char *k = core->field_names + (i ? core->field_shifts[i - 1] : 6);
                k[core->field_shifts[i] - 1] = '\0';
                lua_pushstring(L, k);
                lua_gettable(L, -2);
                char* v = lua_tostring(L, -1);

                lvs[i] = str_new(4096); // #DETLEN
                                        // i forgot what `lvs` stands for maybe "long value string"? (it was a `wchar_t *`)
                char c;
                for (size_t j = 0; (c = v[j]) != '\0'; j++) {
                    charw wc;
                    #if WCHAR_ENABLED
                    int r = mbtowc(&wc, v + j, MB_CUR_MAX);
                    #else
                    wc = c;
                    #endif
                    //printf("i: %d => %lc\n" , r, wc);
                    str_append_ch(&lvs[i], wc);
                }
                lua_pop(L, 1);

                //printf("lv >> %ls\n", lvs[i].c_str);
            }
            string filled = template_fill(core, lvs, inner);
            for (size_t i = 0; i < core->shifts_count; i++) {
                str_delete(&lvs[i]);
            }
            free(lvs);
            // TODO: re-process AND FREE MEMORY
            htmw_context inner_ctx = *ctx; // TODO: deep copy??
            htmw_preprocess(ctx->L, filled, &inner_ctx, ctx->global_paths, ctx->flags & CTX_NETWORK);
            txt_template_fill_data tfd = htmw_process_txt(&inner_ctx);
            //str_debug(inner_ctx.input);
            //printf("___%ls\n", ((ht_template_core*)inner_ctx.templates->head->next->data)->ground.c_str);
            
            if (tfd.fields_count) {
                string out = htmw_compile_txt(&inner_ctx, &tfd);
                lua_pop(L, 1);
                //debug_lua_stack(L);puts("================================================o1");
                return out;
            } else {
                lua_pop(L, 1);
                //debug_lua_stack(L);puts("================================================o2");
                return inner_ctx.input;
            }
            //string out = str_new();
            
            //return filled;
        } else {
            // TODO: handle exception
        }
    }
}
#endif

typedef enum {
    ps_null,
    ps_tag
} parse_status;

typedef struct {
    list* txts;
    size_t len;
} txt_list_tot_len;

#if 0
typedef struct {
    //ht_template_arg_value
} ;
#endif

string htmw_extract_default_syntax(const ht_template *t, const string key) {
    if (t == NULL || str_isnull(key))
        return nullstr;
    string default_syntax_id = nullstr;
    for (size_t j = 0; j < t->args->size; j++) {
        const ht_template_arg* arg = VECT_A(ht_template_arg, t->args) + j;
        if (arg->name.length == key.length && !strncmp(arg->name.c_str, key.c_str, key.length)) {
            if (arg->flags & T_ARG_SYNTAX) {
                default_syntax_id = arg->syntax_id;
            } else {
                // TODO: error
            }
        }
    }
    return default_syntax_id;
}

// takes a string of args (`in`) and outputs an ordered & filtered list of arg `name` + `value` in a txt_template based on the template (`t`)
int txt_parse_args(ht_template* t, const string in, txt_list_tot_len* out, htmw_context* ctx) {
    list* argsl = list_new();
    
    #if 0
    puts("TXT_PARSE_ARGS");
    printf("T: %p\n", t);
    str_debug(in);
    puts("===");
    #endif

    int mode = 0;
    out->len = 0;
    ht_template_arg_value* arg = NULL;
    charw value_bounds = charl('\0');
    size_t last_ent = 0;
    int eq = 0;
    char value_mode = 0;    // 0: default, 1: ``, 2: {} /// unused????
    unsigned char neutral_mode; // bool: escaping in strlt or commenting in lua
    size_t nest = 0;
    charw lua_str_mode = charl('\0');
    char err_msg[256];
    for (size_t i = 0; i <= in.length; i++) {
        //printf("i: %zu\n", i);
        charw c = i < in.length ? in.c_str[i] : charl('\0');
        if (c >= charl('a') && c <= charl('z') || c >= charl('A') && c <= charl('Z') || c >= charl('0') && c <= charl('9') || c == charl('_')) {
            switch (mode) {
            case 0: // enter name mode
                arg = (ht_template_arg_value*)malloc(sizeof(ht_template_arg_value));
                arg->name = nullstr;
                arg->value = nullstr;
                arg->flags = 0;
                arg->name.c_str = in.c_str + i;
                neutral_mode = 0;
                last_ent = i;
                mode = 1;
                break;
            case 1: // name

                break;
            case 2: // default arg value and new attribute name
                arg->value = nullstr;
                //arg->value = str_cpy(!str_isnull(arg.default_value) ? arg.default_value)
                //arg->value = str_new(2);
                //str_append_ch(&arg->value, charl('\b'));
                list_add(argsl, arg);
                arg = (ht_template_arg_value*)malloc(sizeof(ht_template_arg_value));
                arg->name = nullstr;
                arg->value = nullstr;
                arg->flags = 0;
                arg->name.c_str = in.c_str + i;
                neutral_mode = 0;
                last_ent = i;
                mode = 1;
                break;
            case 3: // enter value mode without quotes
                value_bounds = charl('\0');
                arg->value.c_str = in.c_str + i;
                last_ent = i;
                mode = 4;
                break;
            case 4: // value
                if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    if (!neutral_mode) {
                        str_append_ch(&arg->value, c);
                    } else {
                        switch (c)
                        {
                        case charl('n'):
                            str_append_ch(&arg->value, charl('\n'));
                            break;
                        case charl('t'):
                            str_append_ch(&arg->value, charl('\t'));
                            break;
                        case charl('r'):
                            str_append_ch(&arg->value, charl('\r'));
                            break;
                        case charl('v'):
                            str_append_ch(&arg->value, charl('\v'));
                            break;
                        case charl('f'):
                            str_append_ch(&arg->value, charl('\f'));
                            break;
                        case charl('a'):
                            str_append_ch(&arg->value, charl('\a'));
                            break;
                        default:
                            str_append_ch(&arg->value, c);
                            break;
                        }
                    }
                    neutral_mode = 0;
                }
                break;
            }
        } else switch (c) {
        case charl('['):
            switch (mode) {
            case 0: // !
                // TODO: throw exception
                throw_error("Unexpected character `[` in widget tag, arg name expected", E_EXIT);
                break;
            case 1: // name!
                // TODO: throw exception
                throw_error("Unexpected character `[` in widget tag, in arg name", E_EXIT);
                break;
            case 2: // name !
                // TODO: throw exception
                throw_error("Unexpected character `[ in widget tag, arg name or `=` expected", E_EXIT);
                break;
            case 3: // name=!
                value_bounds = charl('\0');
                arg->value.c_str = in.c_str + i;
                last_ent = i;
                mode = 4;
                break;
            case 4: // name="!"
                if (flag_includes(arg->flags, T_ARGV_VALUE) && !neutral_mode && !strncmpw(in.c_str + i, strl("[["), 2)) {
                    if (lua_str_mode == charl('\0')) {
                        i++;
                        lua_str_mode = charl(']');
                    }
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    str_append_ch(&arg->value, charl('[')); // 2025-12-16
                }
                break;
            }
            break;
        case charl(']'):
            switch (mode) {
            case 0: // !
                // TODO: throw exception
                throw_error("Unexpected character `]` in widget tag, arg name expected", E_EXIT);
                break;
            case 1: // name!
                // TODO: throw exception
                throw_error("Unexpected character `]` in widget tag, in arg name", E_EXIT);
                break;
            case 2: // name !
                // TODO: throw exception
                throw_error("Unexpected character `]` in widget tag, arg name or `=` expected", E_EXIT);
                break;
            case 3: // name=!
                value_bounds = charl('\0');
                arg->value.c_str = in.c_str + i;
                last_ent = i;
                mode = 4;
                break;
            case 4: // name="!"
                if (neutral_mode == 3) {
                    neutral_mode = 0;
                    break;
                } else if (flag_includes(arg->flags, T_ARGV_VALUE) && !strncmpw(in.c_str + i, strl("]]"), 2)) {
                    if (neutral_mode == 2) {
                        i++;
                        neutral_mode = 0;
                    } else if (lua_str_mode == charl(']')) {
                        i++;
                        lua_str_mode = charl('\0');
                    }
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    str_append_ch(&arg->value, charl(']'));
                }
                break;
            }
            break;
        case charl('-'):
            switch (mode) {
            case 0: // !
                // TODO: throw exception
                throw_error("Unexpected character `-` in widget tag, arg name expected", E_EXIT);
                break;
            case 1: // name!
                // TODO: throw exception
                throw_error("Unexpected character `-` in widget tag, in arg name", E_EXIT);
                break;
            case 2: // name !
                // TODO: throw exception
                throw_error("Unexpected character `-` in widget tag, arg name or `=` expected", E_EXIT);
                break;
            case 3: // name=!
                value_bounds = charl('\0');
                arg->value.c_str = in.c_str + i;
                last_ent = i;
                mode = 4;
                break;
            case 4: // name="!"
                if (lua_str_mode != charl('\0')) {
                    break;
                }
                if (flag_includes(arg->flags, T_ARGV_VALUE) && !neutral_mode && lua_str_mode == charl('\0')) {
                    if (!strncmpw(in.c_str + i, strl("--[["), 4)) {
                        neutral_mode = 2;
                        i += 3;
                    } else if (!strncmpw(in.c_str + i, strl("--"), 2)) {
                        neutral_mode = 1;
                        i++;
                    }
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    str_append_ch(&arg->value, charl('-'));
                }
                break;
            }
            break;
        case charl('\\'):
            switch (mode) {
            case 0: // !
                // TODO: throw exception
                throw_error("Unexpected character `\\` in widget tag, arg name expected", E_EXIT);
                break;
            case 1: // name!
                // TODO: throw exception
                throw_error("Unexpected character `\\` in widget tag, in arg name", E_EXIT);
                break;
            case 2: // name !
                // TODO: throw exception
                throw_error("Unexpected character `\\` in widget tag, arg name or `=` expected", E_EXIT);
                break;
            case 3: // name=!
                value_bounds = charl('\0');
                arg->value.c_str = in.c_str + i;
                last_ent = i;
                mode = 4;
                break;
            case 4: // name="!"
                if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    neutral_mode = !neutral_mode;
                    if (!neutral_mode) {
                        str_append_ch(&arg->value, charl('\\'));
                    }
                } else if (flag_includes(arg->flags, T_ARGV_VALUE) && lua_str_mode != charl('\0')) {
                    neutral_mode = (!neutral_mode) * 3;
                }
                break;
            }
            break;
        case charl('}'):
        switch (mode) {
            case 0: // "value"
                // TODO: throw exception
                throw_error("Constructor argument values should have a name", E_EXIT);
                break;
            case 1: // name"
                // TODO: throw exception
                throw_error("Illegal constructor argument value begin", E_EXIT);
                break;
            case 2: // name "value"
                // TODO: throw exception
                break;
            case 3: // enter value with quotes
                value_bounds = charl('\0');
                arg->value.c_str = in.c_str + i;
                last_ent = i;
                mode = 4;
                break;
            case 4: // exit value with quotes?
                //printf("%d, %d, %d\n", neutral_mode, lua_str_mode, nest);
                if (c == value_bounds && !neutral_mode && lua_str_mode == charl('\0')) {
                    if (!nest) {
                        value_mode = 0;
                        arg->value.length = i - last_ent - 1;
                        list_add(argsl, arg);
                        //str_debug(arg->value);
                        mode = 0;
                    } else {
                        nest--;
                    }
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    str_append_ch(&arg->value, charl('}'));
                    neutral_mode = 0;
                }
                break;
            }
            break;
        case charl('{'):
            switch (mode) {
            case 0: // "value"
                // TODO: throw exception
                throw_error("Constructor argument values should have a name", E_EXIT);
                break;
            case 1: // name"
                // TODO: throw exception
                throw_error("Illegal constructor argument value begin", E_EXIT);
                break;
            case 2: // name "value"
                // TODO: throw exception
                break;
            case 3: // enter value with quotes
                value_bounds = charl('}');
                value_mode = 2;
                arg->flags |= T_ARGV_VALUE;
                arg->value.c_str = in.c_str + i + 1;
                last_ent = i;
                mode = 4;

                luax_transpile_result o = luax_transpile(in.c_str + i + 1, /*in.length*/ 10000, ctx->luax_ctx);
                if (o.error) { // ofc a more accurate error log will be implemented
                    printf("[HTMW LuaX Error]: there is an error on the LuaX syntax (3)\n", o.error);
                    exit(1);
                }
                luax_str_view sv = o.result.first;
                arg->flags |= T_ARGV_VALUE;
                arg->value.c_str = sv.s;
                arg->value.length = sv.len;
                //args_buffer.size = 0;
                list_add(argsl, arg);
                i += o.result.second + 1;
                //str_debug(arg->value);

                //last_ent = i;
                mode = 0;
                value_mode = 0;

                break;
            case 4: // exit value with quotes?
                if (flag_includes(arg->flags, T_ARGV_VALUE) && !neutral_mode && lua_str_mode == charl('\0')) {
                    nest++;
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    str_append_ch(&arg->value, charl('{'));
                    neutral_mode = 0;
                }
                break;
            }
            break;
        case charl('`'):
            switch (mode) {
            case 0: // "value"
                // TODO: throw exception
                throw_error("Constructor argument values should have a name", E_EXIT);
                break;
            case 1: // name"
                // TODO: throw exception
                throw_error("Illegal constructor argument value begin", E_EXIT);
                break;
            case 2: // name "value"
                // TODO: throw exception
                break;
            case 3: // enter value with quotes
                value_bounds = charl('`');
                value_mode = 1;
                arg->flags |= T_ARGV_STRLT;
                arg->value = str_new(in.length - i);
                last_ent = i;
                mode = 4;
                break;
            case 4: // exit value with quotes?
                if (c == value_bounds && !neutral_mode) {
                    value_mode = 0;
                    //arg->value.length = i - last_ent - 1;
                    list_add(argsl, arg);
                    mode = 0;
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    str_append_ch(&arg->value, charl('`'));
                    neutral_mode = 0;
                } else if (flag_includes(arg->flags, T_ARGV_VALUE) && neutral_mode == 3) {
                    neutral_mode = 0;
                }
                break;
            }
            break; /// ^^^ NEW ^^^
        case charl('\''):
        case charl('"'):
            switch (mode) {
            case 0: // "value"
                // TODO: throw exception
                throw_error("Constructor argument values should have a name", E_EXIT);
                break;
            case 1: // name"
                // TODO: throw exception
                throw_error("Illegal constructor argument value begin", E_EXIT);
                break;
            case 2: // name "value"
                // TODO: throw exception
                break;
            case 3: // enter value with quotes
                value_bounds = c;
                arg->value.c_str = in.c_str + i + 1;
                last_ent = i;
                mode = 4;
                break;
            case 4: // exit value with quotes?
                if (c == value_bounds) {
                    arg->value.length = i - last_ent - 1;
                    list_add(argsl, arg);
                    mode = 0;
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)) {
                    str_append_ch(&arg->value, c);
                } else if (flag_includes(arg->flags, T_ARGV_VALUE)) {
                    if (lua_str_mode == charl('\0') && !neutral_mode) {
                        lua_str_mode = c;
                    } else if (lua_str_mode == c && neutral_mode != 3) {
                        lua_str_mode = charl('\0');
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                }
                break;
            }
            break;
        case charl(' '):
        case charl('\t'):
        case charl('\n'):
        case charl('\r'):
            switch (mode) {
            case 0:
                break;
            case 1: // exit name mode
                arg->name.length = i - last_ent;
                mode = 2;
                break;
            case 2: // ignore

                break;
            case 3: // ignore
                
                break;
            case 4:
                if (value_bounds == charl('\0')) { // exit value without quotes
                    arg->value.length = i - last_ent;
                    list_add(argsl, arg);
                    mode = 0;
                } else if (flag_includes(arg->flags, T_ARGV_VALUE)) { // exit single line lua comment mode
                    if (neutral_mode == 1 && c == charl('\n')) {
                        neutral_mode = 0;
                    } else if ((lua_str_mode == charl('"') || lua_str_mode == charl('\'')) && c == charl('\n')) {
                        lua_str_mode = charl('\0');
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)/* && !neutral_mode*/) {
                    str_append_ch(&arg->value, c);
                    neutral_mode = 0;
                }
                break;
            }
            break;
        case charl('='):
            switch (mode) {
            case 0: // =
                // TODO: throw exception
                throw_error("Unexpected symbol '=' in widget tag", E_EXIT);
                break;
            case 1: // name=
                arg->name.length = i - last_ent;
                mode = 3;
                break;
            case 2:
                mode = 3;
                break;
            case 3: // name==
                // TODO: throw exception
                throw_error("Unexpected symbol '=' in widget tag", E_EXIT);
                break;
            case 4: // name="=" or // name= =
                //if (c != charl('\0')) {
                if (value_bounds != charl('\0')) {
                    /*arg->value.length = i - last_ent;
                    list_add(argsl, arg);
                    mode = 0;*/
                    if (flag_includes(arg->flags, T_ARGV_VALUE) && neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                    break;
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)/* && !neutral_mode*/) {
                    str_append_ch(&arg->value, charl('='));
                    neutral_mode = 0;
                } else {
                    // TODO: throw exception
                    throw_error("Unexpected symbol '=' in widget tag", E_EXIT);
                }
                break;
            }
            break;
        case charl('\0'):
            switch (mode) {
            case 0:
                break;
            case 1: // exit name mode
                arg->name.length = i - last_ent;
                mode = 2;
                arg->value = nullstr;
                //printf("i: %zu | le: %zu\n", i, last_ent);
                //str_debug(arg->value);
                list_add(argsl, arg);
                break;
            case 2: // default arg value
                arg->value = nullstr;
                list_add(argsl, arg);
                break;
            case 3: // ignore
                break;
            case 4: // exit value without quotes
                if (value_bounds == charl('\0')) {
                    arg->value.length = i - last_ent;
                    list_add(argsl, arg);
                    mode = 0;
                } else if (flag_includes(arg->flags, T_ARGV_STRLT)/* && !neutral_mode*/) {
                    str_append_ch(&arg->value, charl('\0'));
                    neutral_mode = 0;
                }
                break;
            }
            break;
        /*case '>':
            if (mode == 0) {
                goto txt_parse_args_break_for;
            }
            break;*/
        case '#':
            switch (mode)
            {
            case 3: {
                //VECT_A();
                string default_syntax_id = htmw_extract_default_syntax(t, arg->name);
                htmw_syntax_literal lit = txt_parse_syntax_literal(ctx, in.c_str + i, SIZE_MAX, default_syntax_id.c_str, default_syntax_id.length);
                if (lit.error) {
                    // TODO: throw exception
                }

                i += lit.total_length - 1;
                arg->value = lit.content;
                //str_debug(arg->value);
                list_add(argsl, arg);
                mode = 0;
            } break;
            default:
                break;
            }
            break;
        default:
            switch (mode) {
            case 0: // !
                // TODO: throw exception
                //printf("'%lc'\n", c);
                sprintf(err_msg, "Unexpected symbol `%c` in widget tag, arg name expected", c);
                throw_error(err_msg, E_EXIT);
                break;
            case 1: // name!
                // TODO: throw exception
                //if (c != '/') {
                    sprintf(err_msg, "Unexpected symbol `%c` in widget tag, in arg name", c);
                    throw_error(err_msg, E_EXIT);
                //}
                break;
            case 2: // name !
                // TODO: throw exception
                sprintf(err_msg, "Unexpected symbol `%c` in widget tag, arg name or `=` expected", c);
                throw_error(err_msg, E_EXIT);
                break;
            case 3: // name=!
                value_bounds = charl('\0');
                arg->value.c_str = in.c_str + i;
                last_ent = i;
                mode = 4;
                break;
            case 4: // name="!"
                if (flag_includes(arg->flags, T_ARGV_STRLT)/* && !neutral_mode*/) {
                    str_append_ch(&arg->value, c);
                    neutral_mode = 0;
                } else if (flag_includes(arg->flags, T_ARGV_VALUE) && neutral_mode == 3) {
                    neutral_mode = 0;
                }
                break;
            }
            break;
        }
    }
    txt_parse_args_break_for:

#if DEBUG_MODE
    puts("---[ARGSL]---");
    for (size_t i = 0; i < argsl->size; i++) {
        ht_template_arg_value* a = list_get(argsl, i);
        printf("- %ls\t\t%ls\n", str_cpy(a->name).c_str, str_cpy(a->value).c_str);
    }
    puts("---[ END ]---");
#endif

    list* args = list_new(); // sorted & filtered output list
    vect* a = t->args;
    VECT_FOR(a, i) {
        int found = 0;
        list_node* ln = argsl->head;
        for (size_t j = 0; j < argsl->size; j++) {
            ht_template_arg_value* o = (ht_template_arg_value*)ln->data;

            if (!strncmpw(VECT_A(ht_template_arg, a)[i].name.c_str, o->name.c_str, o->name.length)) {
                found = 1;
                if (str_isnull(o->value)) {
                    o->value = VECT_A(ht_template_arg, a)[i].default_value;
                } else if (flag_includes(o->flags, T_ARGV_VALUE)) {
                    //o->value = str_new(2);
                    //str_append_ch(&o->value, charl('\r'));
                    //string wlin = str_cpy(o->value);
                    //string wlin = str_new(o->value.length + 2);
                    //str_append_ch(&wlin, charl('\r')); // signals lua code passed
                    o->flags = T_ARGV_VALUE;
                    //str_append(&wlin, o->value.c_str, o->value.length);
                    //o->value = str_cpy(o->value);
                    //printf("{%ls}\n", o->value.c_str + 1);
                    /*char* lin = (char*)malloc((wlin.length + 1 + 16 + 1 - 1) * sizeof(char)); // len + null terminator + "return tostring(" + ")" - "/r"
                    lin[0] = 'r';
                    lin[1] = 'e';
                    lin[2] = 't';
                    lin[3] = 'u';
                    lin[4] = 'r';
                    lin[5] = 'n';
                    lin[6] = ' ';
                    lin[7] = 't';
                    lin[8] = 'o';
                    lin[9] = 's';
                    lin[10] = 't';
                    lin[11] = 'r';
                    lin[12] = 'i';
                    lin[13] = 'n';
                    lin[14] = 'g';
                    lin[15] = '(';
                    wcstombsw(lin + 16, wlin.c_str, wlin.length);
                    lin[wlin.length + 16] = ')';
                    lin[wlin.length + 17] = '\0';
                    int r = luaL_loadstring(L, lin);
                    if (check_lua(L, r)) {
                        if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
                            const char *lout = lua_tostring(L, -1);
                            size_t lout_len = strlen(lout);
                            string wlout = str_new(lout_len + 1);
                            mbstowcs(wlout.c_str, lout, lout_len);
                            wlout.c_str[lout_len] = charl('\0');
                            wlout.length = lout_len;
                            free(lin);
                            //free(o->data);
                            o->value = wlout;

                            lua_pop(L, 1); // pop the result off the stack
                        } else {
                            throw_error("Lua pcall is not ok", E_EXIT);
                        }
                    } else {
                        throw_error("Lua status is not ok", E_EXIT);
                    }*/
                    
                    //lua_pop(L, 1);
                }
                list_add(args, o);
                list_remove(argsl, j);
                out->len += o->value.length + 1;
                break;
            }

            ln = ln->next;
        }
        if (!found) {
            // TODO: throw exception
            // FIXME: duplicate names are not handled properly, fix this!!!

            //printf("i: %zu\n", i);
            ht_template_arg arg = VECT_A(ht_template_arg, a)[i];
            if (!str_isnull(arg.default_value)) {
                if (flag_includes(arg.flags, T_ARG_NULLABLE)) {
                    ht_template_arg_value* o = (ht_template_arg_value*)malloc(sizeof(ht_template_arg_value));
                    o->name = str_cpy(arg.name); // ??? str_cpy?
                    //o->value = nullstr;
                    o->flags = T_ARGV_NONE;
                    string v = str_new(3);
                    //str_append_ch(&v, charl('\b')); // this character signals nullable
                    if (arg.flags & T_ARG_EXPR) { // 2026-16-04
                        o->flags |= T_ARGV_VALUE;
                        //str_append_ch(&v, charl('\r')); // this means it's an evaluated expression
                    }
                    o->value = v;
                    out->len += o->value.length + 1;
                    //puts(o->value.c_str);
                    //puts(o->value.c_str);
                    //puts(o->value.c_str);
                    //puts(o->value.c_str);
                    //puts(o->value.c_str);
                    //puts(VECT_A(ht_template_arg, a)[i].name.c_str);
                    //puts("1a");
                    list_add(args, o);
                    continue;
                } else {
                    ht_template_arg_value* o = (ht_template_arg_value*)malloc(sizeof(ht_template_arg_value));
                    o->name = str_cpy(arg.name); // ??? str_cpy?
                    o->flags = 0;
                    #if 0
                    o->value = str_cpy(arg.default_value);
                    out->len += o->value.length + 1;
                    #else
                    //string v = str_new(arg.default_value.length + 2);
                    if (arg.flags & T_ARG_EXPR) { // 2026-16-04
                        //v = ;
                        o->flags |= T_ARGV_VALUE;
                        //str_append_ch(&v, charl('\r')); // this means it's an evaluated expression
                    }

                    //o->value = v;
                    //o->value = str_append(&o->value, arg.default_value.c_str, arg.default_value.length);
                    o->value = str_cpy(arg.default_value);
                    out->len += o->value.length + 1;
                    #endif
                    //str_debug(arg.default_value);
                    //str_debug(o->value);
                    //puts(VECT_A(ht_template_arg, a)[i].name.c_str);
                    //puts("1b");
                    list_add(args, o);
                    continue;
                }
            } else if (flag_includes(arg.flags, T_ARG_NULLABLE)) {
                ht_template_arg_value* o = (ht_template_arg_value*)malloc(sizeof(ht_template_arg_value));
                o->name = str_cpy(arg.name); // ??? str_cpy?
                //o->value = nullstr;;
                o->flags = T_ARGV_NONE;
                //o->value = str_cpy(o->value);
                out->len += o->value.length + 1;
                //puts(VECT_A(ht_template_arg, a)[i].name.c_str);
                //puts("2");
                list_add(args, o);
                continue;
            }

            //throw_error("Widget argument(s) missing", E_EXIT);
            char e_msg[1024];
            #if WCHAR_ENABLED
            snprintf(e_msg, 255, "Required widget argument \"%ls\" in tag \"%ls\" is missing", VECT_A(ht_template_arg, a)[i].name.c_str, t->name.c_str);
            #else
            snprintf(e_msg, 255, "Required widget argument \"%s\" in tag \"%s\" is missing", VECT_A(ht_template_arg, a)[i].name.c_str, t->name.c_str);
            #endif
            throw_error(e_msg, E_EXIT);
            ln = argsl->head;
            for (size_t j = 0; j < argsl->size; j++) {
                ht_template_arg_value* o = (ht_template_arg_value*)ln->data;
                //free(o); duplicate keys cause crash? double-free?
            }
            list_delete(argsl);
            ln = args->head;
            for (size_t j = 0; j < args->size; j++) {
                ht_template_arg_value* o = (ht_template_arg_value*)ln->data;
                free(o);
            }
            out = NULL;
            return 0;
        }
    }

    /*for (size_t i = 0; i < args->size; i++) {
        printf("list debug (%zu)\n", i);
        ht_template_arg_value* o = (ht_template_arg_value*)list_get(args, i);
        str_debug(o->name);
        str_debug(o->value);
        puts("-----");
    }*/

    list_node* ln = argsl->head;//puts("eee");
    for (size_t i = 0; i < argsl->size; i++) {
        ht_template_arg_value* o = (ht_template_arg_value*)ln->data;
#if DEBUG_MODE == 1
        printf("[%zu]\t", i);
#endif
        //free(o); // FIXME: this free wasn't commented out, but for some reason it causes "free(): double free detected in tcache 2\nAborted"
    }//puts("eeeeeeeeee");
    out->txts = args;
    //*out = args;
    return 1;
}

enum tag_parse_status_e {
    htmw_tps_beginning = 0,
    htmw_tps_closing = 1,
    htmw_tps_typehash = 2,
    htmw_tps_typename = 3,
    htmw_tps_void = 4,
    htmw_tps_arg_key = 5,
    htmw_tps_arg_key_after = 6,
    htmw_tps_arg_assign = 7,
    htmw_tps_arg_value = 8,
    htmw_tps_self_closing = 9,
};

// 2026-02-19
struct tag_info_lite_s {
    size_t n;       // length of the tag like <tag attrib="val"> would be 18
    string name;    // name of the tag (this string is not in a new allocation)
    uint8_t flags;  // self closing or closing
    size_t argc;    // number of attributes
    string args;    // string view of the starting point of the attributes
};

#if 0
typedef struct htmw_tag_props_list_s {

} htmw_tag_props_list;
#endif

// this is a lightweight version of the tag parser that doesn't store the list of attributes
// 2026-02-19: maybe this will be inlined using a macro to optimize the engine
static inline struct tag_info_lite_s htmw_analyze_tag(const htmw_context *ctx, const string in, const char *required_name, size_t required_name_len) {
    /*
    [phases]
    0: beginning (name or slash?)
    1: slash passed
    1.5: # native tag notation
    2: constructing name
    3: name/arg passed (whitespace) <= returning point
    4: constructing arg name
    5: arg name end
    6: equals sign passed (if there's anything else other than a whitespace return to point 3)
    7: constructing value
    8: self-closing slash
    */
    struct tag_info_lite_s o = {
        .n = 0,
        .name = nullstr,
        .flags = 0,
        .argc = 0,
        .args = nullstr
    };
    if (in.c_str[0] != '<') return o; // null result
    char c;
    uint8_t phase = 0;
    uint8_t loop = 1;
    uint8_t value_escape = 0;
    char bounds = '\0';
    size_t args_start_idx = 0;

    size_t last_key_start_idx = 0;
    string last_key = nullstr;
    //printf("L: %zu\n", in.length);
    size_t i;
    for (i = 1; i < in.length && loop; i++) {
        c = in.c_str[i];
        //putc(c, stdout);
        //printf("c: '%c', i: %zu, phase: %d\n", c, i, phase);
        switch (c)
        {
        case '>'://putc(">", stdout);
            switch (phase)
            {
            case htmw_tps_beginning:
            case htmw_tps_closing:
            //puts("1");
                return o; // null result
            case htmw_tps_typename:
                o.name.length = i - 1 - !!(o.flags & (1 << 0));
                if (required_name != NULL && (o.name.length != required_name_len || strncmp(o.name.c_str, required_name, required_name_len))) {
                    o.name = nullstr;
                    //puts("2");
                    return o;
                }
            case htmw_tps_void:
            case htmw_tps_arg_key_after:
                //loop = 0;
                //o.n = i + 1;
                //if (o.args.c_str != NULL && o.args.length == 0 && (o.flags ^ (1 << 0))) {
                //    o.args.length = i - 1 - args_start_idx;
                //}
                //break;
            case htmw_tps_arg_key:
                loop = 0;//puts("a");
                o.n = i + 1;
                if (o.args.c_str != NULL && o.args.length == 0 && !(o.flags & (1 << 0))) {
                    o.args.length = i - args_start_idx;
                }
                break;
            case htmw_tps_arg_assign:
            case htmw_tps_arg_value:
                if (bounds == '\0') {
                    loop = 0;//puts("b");
                    o.n = i + 1;
                    if (o.args.c_str != NULL && o.args.length == 0 && !(o.flags & (1 << 0))) {
                        o.args.length = i - args_start_idx;
                    }
                }
                break;
            case htmw_tps_self_closing:
                loop = 0;//puts("c");
                o.n = i + 1;
                //
                if (o.args.c_str != NULL && o.args.length == 0 && !(o.flags & (1 << 0))) {
                    o.args.length = i - args_start_idx;
                }//
                break;
            default:
                break;
            }
            break;
        case '\t':
        case '\n':
        case '\r':
        case ' '://puts("[whitespace]");
            switch (phase)
            {
            case htmw_tps_beginning:
            case htmw_tps_closing:
                //puts("3");
                return o; // null result
            case htmw_tps_typename:
                o.name.length = i - 1 - !!(o.flags & (1 << 0));
                if (o.argc == 0) {
                    o.args.c_str = in.c_str + i;
                    args_start_idx = i;
                }
                if (required_name != NULL && (o.name.length != required_name_len || strncmp(o.name.c_str, required_name, required_name_len))) {
                    o.name = nullstr;
                    //puts("4");
                    return o;
                }
            case htmw_tps_arg_key:
                last_key.c_str = in.c_str + last_key_start_idx;
                last_key.length = i - last_key_start_idx;
                phase++;// puts("AAA 3");
                break;
            case htmw_tps_arg_value:
                if (bounds == '\0') {
                    phase = htmw_tps_void;
                }
                break;
            case htmw_tps_arg_key_after:
                if (o.argc == 0) {
                    o.args.c_str = in.c_str + i;
                    args_start_idx = i;
                }
                break;
            default:
                break;
            }
            break;
        case '='://puts("=");
            if (phase == htmw_tps_arg_key) {
                last_key.c_str = in.c_str + last_key_start_idx;
                last_key.length = i - last_key_start_idx;
                phase = htmw_tps_arg_assign;// puts("AAA 1");// <= here you can use to set the thing
            } else if (phase == htmw_tps_arg_key_after) {
                phase = htmw_tps_arg_assign;// puts("AAA 2");
            }
            break;
        case '"':
        case '\'':
        case '`'://puts("\"");
            if (phase == htmw_tps_arg_assign && bounds == '\0') {
                bounds = c;
                phase = htmw_tps_arg_value;
            } else if (bounds == c) {
                if (c != '`' || !value_escape) {
                    bounds = '\0';
                    phase = htmw_tps_void;
                }
            }
            break;
        case '{'://putc("{", stdout);
            if (phase == htmw_tps_arg_assign) {
                // TODO: implement a lightweight analyze only purpose function
                luax_transpile_result ro = luax_transpile(in.c_str + i + 1, in.length - 1, NULL);
                if (ro.error) { // ofc a more accurate error log will be implemented
                    printf("[HTMW LuaX Error]: there is an error on the LuaX syntax (0)\n", ro.error);
                    exit(1);
                }
                i += ro.result.second + 1;
                phase = htmw_tps_void;
            }
            break;
        case '#'://puts("#"); this should handle both the native template and the vT.7 syntax notation
            if (phase == htmw_tps_typehash) {
                o.flags |= (1 << 2);
                phase++;
            } else if (phase == htmw_tps_arg_assign) {
                if (o.argc == 0) { /// TEMPORARY
                    //o.args.c_str = in.c_str + i;
                    args_start_idx = i;
                }
                /// TODO: OPTIMIZE THIS PROCESS and put it in a separate subroutine
                ht_template *t = NULL;
                u_map_node* mnt = (u_map_node*)ctx->templates->head;
                for (size_t j = 0; j < ctx->templates->size; j++) {
                    ht_template* tt = (ht_template*)mnt->data;
                    if (o.name.length == tt->name.length && !strncmpw(o.name.c_str, tt->name.c_str, o.name.length)) {
                        t = tt;
                        break;
                    }
                    mnt = mnt->next;
                }
                string default_syntax_id = htmw_extract_default_syntax(t, last_key);
                htmw_syntax_literal lit = txt_parse_syntax_literal(ctx, in.c_str + i, SIZE_MAX, default_syntax_id.c_str, default_syntax_id.length);
                if (lit.error) {
                    // TODO: throw exception
                    throw_error("there was an error in the syntax notation", E_EXIT);
                }
                //printf("%zu >> ", i);

                i += lit.total_length - 1;

                //printf("%zu, ltl %zu\n", i, lit.total_length);
                
                phase = htmw_tps_void;
                ///
                //phase++;
            }
            break;
            // vT.7s syntax notation
            break;
        case '\\'://puts("\\");
            if (phase == htmw_tps_arg_value && bounds == '`' && !value_escape) {
                value_escape = 2;
            }
            break;
        case '/'://puts("/");
            //printf(">>>>>>>>>> %d\n", phase);
            if (phase == htmw_tps_beginning) { // closing
                o.flags |= (1 << 0);
                phase = htmw_tps_closing;
            } else if (phase == htmw_tps_void || phase == htmw_tps_typename || phase == htmw_tps_arg_key/* || phase == htmw_tps_arg_assign*/ || phase == htmw_tps_arg_key_after || (phase == htmw_tps_arg_value && bounds == '\0')) { // self-closing
                o.flags |= (1 << 1);
                phase = htmw_tps_self_closing;//puts("NAME: 1");
                if (o.name.c_str != NULL && o.name.length == 0) {
                    o.name.length = i - 1;
                    if (required_name != NULL && o.name.length != required_name_len || strncmp(o.name.c_str, required_name, required_name_len)) {
                        o.name = nullstr;
                        //puts("5");
                        return o;
                    }
                } else if (o.args.c_str != NULL && o.args.length == 0 && !(o.flags & (1 << 0))) {
                    o.args.length = i - args_start_idx;
                }
            }//puts("(/) break");
            break;
        default:
            if (c > ' ') {//puts("any"); // any symbol
                switch (phase)
                {
                case htmw_tps_beginning:
                    //puts("====================================");
                    //o.name.c_str = in.c_str + i;
                    //phase = 2;
                    //break;
                case htmw_tps_closing:
                case htmw_tps_typehash:
                    o.name.c_str = in.c_str + i;
                    phase = htmw_tps_typename;
                    //puts("NAME: 2");
                    break;
                case htmw_tps_void:
                    o.argc++;
                    last_key_start_idx = i;
                case htmw_tps_arg_key_after:
                    if (o.argc == 0) {
                        o.args.c_str = in.c_str + i;
                        args_start_idx = i;
                    }
                    //phase++; puts("AAA 4");
                    phase = htmw_tps_arg_key;
                    break;
                //case 8:
                //    throw_error("unexpected symbol in ending self-closing tag", E_EXIT);
                //    break;
                default:
                    break;
                }
            }
            break;
        }
        if (value_escape > 0) {
            value_escape--;
        }
    }

    //puts("---AT---");
    //printf("%.7s\n", in.c_str);
    //printf("[%.*s]\n", required_name_len, required_name);
    //str_debug(o.name);
    //puts("---/AT--");
    //printf("->i: %zu\n", i);

    return o;
}

// 2026-02-19: processes the text and returns where the template result should be placed
txt_template_fill_data htmw_process_txt(const htmw_context *ctx) {
    string in = ctx->input;
    string ground = str_new(ctx->input.length + 1);

    list* fields_list = list_new(); // list >> field_location
    list* argsls = list_new();      // list >> list >> string
    list* inners_list = list_new(); // list >> string

    txt_template_fill_data out = (txt_template_fill_data){
        .args = NULL,
        .arg_shifts = NULL,
        .fields = NULL,
        .fields_count = 0,
        .ground = nullstr,
        .inner_shifts = NULL,
    };

    int comment_mode = 0;
    int php_mode = 0;
    size_t last_idx = 0;

    charw c;

    size_t args_count = 0;
    size_t args_block_size = 0;
    size_t inners_block_size = 0;
    size_t difference = 0;int testt__ = 0;

    //string name = str_new(128);
    for (size_t idx = 0; (c = in.c_str[idx]) != charl('\0'); idx++) {
        //putwc(c, stdout);
        /*if (in.c_str[idx + 1] == charl('\0')) {
            //last_idx = idx;
            //printf("gwa gwa 1\n");
            //str_append(&ground, in.c_str + last_idx, idx - last_idx - 1);
            //printf("gwa gwa 2\n");
            //str_append_ch(&ground, c);
        } *//*if (comment_mode) {
            if (c == charl('-')) {
                if (!strncmpw(in.c_str + idx, strl("-->"), 3)) {
                    comment_mode = 0;
                    idx += 2;
                } else continue;
            } else continue;
        }
        if (php_mode) {
            if (c == charl('?')) {
                if (!strncmpw(in.c_str + idx, strl("?>"), 2)) {
                    php_mode = 0;
                    idx += 1;
                } else continue;
            } else continue;
        }*/

        if (c == charl('<')) {

            #define NEW_TXT_PROCESSING 1
            #if NEW_TXT_PROCESSING
            char c_next = in.c_str[idx + 1];
            if ((c_next < 'a' || c_next > 'z') && (c_next < 'A' || c_next > 'Z') && (c_next < '0' || c_next > '9') && c_next == '_') { // shouldn't be processed
                str_append_ch(&ground, '<');
                continue;
            }

            #if 0
            if (in.c_str[idx + 1] == '#') { // native tags shouldn't be processed
                str_append_ch(&ground, '<');
                continue;
            }
            #endif

            string tag_sv = (string){
                .c_str = in.c_str + idx,
                .length = in.length - idx,
                .size = 0
            };
            struct tag_info_lite_s tag_info = htmw_analyze_tag(ctx, tag_sv, NULL, 0);
            //printf("tag: n: %zu\n", tag_info.n);
            //str_debug(tag_info.args);
            #endif

            charw c;
            size_t i;

            //int naming = 1;
            int ninner = 0;
            int dninner = 0;        // tag is self-enclosing
            int aninner = 0;        // template itself is ninner
            ht_template* t = NULL;
            string name_buffer = nullstr;
            string args_buffer = nullstr;
            string* inner = (string*)malloc(sizeof(string));
            *inner = nullstr;
            int in_args = 0;

            #if NEW_TXT_PROCESSING
            name_buffer = tag_info.name;
            args_buffer = tag_info.args;

            //str_debug(args_buffer);

            if (str_isnull(tag_info.name)) {
                //puts(">>>>>>>>>>>>>>>>>>>>>>>>AAAAA");
                str_append_ch(&ground, '<');
                continue;
            }
            #if 1
            if ((tag_info.flags & (1 << 0))/* || (tag_info.flags & (1 << 1))*/) {
                //puts(">>>>>>>>>>>>>>>>>>>>>>>>BBBBB");
                str_append_ch(&ground, '<');
                continue;
            }
            #endif

            u_map_node* mnt = (u_map_node*)ctx->templates->head;
            for (size_t j = 0; j < ctx->templates->size; j++) {
                ht_template* tt = (ht_template*)mnt->data;
                if (name_buffer.length == tt->name.length && !strncmpw(name_buffer.c_str, tt->name.c_str, name_buffer.length)) {
                    t = tt;
                    in_args = 1;
                    field_location* fl = (field_location*)malloc(sizeof(field_location));
                    fl->pos = idx - difference;
                    fl->idx = j;
                    list_add(fields_list, (void*)fl);
                    if (t != NULL && (t->flags & TEMPLATE_NINNER)) {
                        ninner = 1;
                        aninner = 1;
                        *inner = nullstr;
                    }
                    break;
                } else {
                    mnt = mnt->next;
                }
            }
            if (t == NULL) {
                //puts(">>>>>>>>>>>>>>>>>>>>>>>>CCCCC");
                str_append_ch(&ground, '<');
                continue;
            }
            i = tag_info.n - !!(tag_info.flags & (1 << 1)) - 2;

            
            dninner = !!(tag_info.flags & (1 << 1));
            
            #if 1
            //  v NOT self-enclosing
            if (!(tag_info.flags & (1 << 1)) && !(t->flags & TEMPLATE_NINNER))
                inner->c_str = in.c_str + idx + tag_info.n;
            #endif
            //printf("tag_info.n: %zu\n", tag_info.n);

            #endif

            // TODO: move this to a more appropriate location
            //char *strplex_args_data = (char *)malloc((ctx->input.length + (ctx->input.length / 2)) * sizeof(char));
            //out.args.capacity = ctx->input.length + (ctx->input.length / 2);
            char *strplex_args_data = (char *)malloc((max(tag_info.n, t->args_min_size) + 99999) * sizeof(char));
            out.args.capacity = max(tag_info.n, t->args_min_size) + 99999;
            out.args.count = 0;
            out.args.data = strplex_args_data;
            out.args.endn = 0;

            #if !NEW_TXT_PROCESSING
            for (i = 0; (c = in.c_str[idx + 1 + i]) != charl('\0'); i++) {
                //printf("__[%zu] => %lc\n", idx + i, c);
                if (c >= charl('a') && c <= charl('z') || c >= charl('A') && c <= charl('Z') || c >= charl('0') && c <= charl('9') || c == charl('_')/* || c == charl('-')*/) {
#if DEBUG_MODE
                    /*if (!strncmpw(in.c_str + idx + 1, strl("metas"), 5)) {
                        printf("i: %zu\n", i);
                        puts("metafound!");
                    }*/
#endif
                    continue;
                } else if (c == charl(' ') || c == charl('\n') || c == charl('\t') || c == charl('\r')) {
#if DEBUG_MODE
                    /*putc('.', stdout);
                        printf("i: %zu\n", i);
                    if (!strncmpw(in.c_str + idx + 1, strl("metas"), 5)) {
                        puts("post-metafound!");
                    }*/
#endif
                    if (!i) {
                        break;
                    } else {
                        name_buffer = (string){
                            .c_str = in.c_str + idx + 1,
                            .length = i,
                            .size = 0
                        };
                        u_map_node* mnt = (u_map_node*)ctx->templates->head;
                        for (size_t j = 0; j < ctx->templates->size; j++) {
                            ht_template* tt = (ht_template*)mnt->data;
#if DEBUG_MODE
                            //printf("\"%ls\"\n", name_buffer.c_str);
                            //printf("\"%ls\"\n\n", in.c_str + idx + 1);
                            //printf("nl: %zu\tns: %zu\n", name_buffer.length, name_buffer.size);
                            /*//if (!strncmpw(name_buffer.c_str, strl("metas"), 5)) {
                                string _tmp_name = str_cpy(name_buffer);
                                printf("i: %zu\tlen: %zu\tnb: \"%ls\"\t\ttb: \"%ls\"\n", i, tt->name.length, _tmp_name.c_str, tt->name.c_str);
                            //}*/
#endif
                            if (i == tt->name.length && !strncmpw(name_buffer.c_str, tt->name.c_str, i)) {
                                t = tt;
                                in_args = 1;
                                field_location* fl = (field_location*)malloc(sizeof(field_location));
                                fl->pos = idx - difference;
                                fl->idx = j;
                                list_add(fields_list, (void*)fl);
#if DEBUG_MODE
                                //printf("t1: %p\tpos: %zu\tname: %ls\n", t, fl->pos, t->name.c_str);
                                //printf("t1r_idx: %zu\n", fl->pos);
#endif
                                // check if ninner mode
                                //printf("ninner? %d\n", t->ninner);
                                if (t != NULL && (t->flags & TEMPLATE_NINNER)) {
                                    ninner = 1;
                                    aninner = 1;
                                    *inner = nullstr;
                                }
                                ///////////////////////
                                goto widget_name_to_arg_parse;
                            } else {
                                mnt = mnt->next;
                            }
                        }
                    }
                } else if (c == charl('/')) {
                    //putc(':', stdout);
                    if (in.c_str[idx + 2 + i] != charl('>')) break;
                    ninner = 1;
                    dninner = 1;
                    if (!i) {
                        break;
                    } else {
                        name_buffer = (string){
                            .c_str = in.c_str + idx + 1,
                            .length = i,
                            .size = 0
                        };
                        u_map_node* mnt = (u_map_node*)ctx->templates->head;
                        size_t j;
                        for (j = 0; j < ctx->templates->size; j++) {
                            ht_template* tt = (ht_template*)mnt->data;
                            if (i/* - 1*/ == tt->name.length && !strncmpw(name_buffer.c_str, tt->name.c_str, i/* - 1*/)) {
                                t = tt;
                                field_location* fl = (field_location*)malloc(sizeof(field_location));
                                fl->pos = idx - difference;
                                fl->idx = j;
                                list_add(fields_list, (void*)fl);
#if DEBUG_MODE
                                //printf("t2: %p\tpos: %zu\tname: %ls\n", t, fl->pos, t->name.c_str);
#endif
                                goto widget_name_to_arg_parse;
                                //break;
                            } else {
                                mnt = mnt->next;
                            }
                        }
                        break;
                    }
                } else if (c == charl('>')) {
                    //putc(';', stdout);
                    name_buffer = (string){
                        .c_str = in.c_str + idx + 1,
                        .length = i,
                        .size = 0
                    };
                    u_map_node* mnt = (u_map_node*)ctx->templates->head;
                    size_t j;
#if DEBUG_MODE
                    if (!strncmpw(name_buffer.c_str, strl("test"), 4)) {
                        puts("__test__");
                    }
#endif
                    for (j = 0; j < ctx->templates->size; j++) {
                        ht_template* tt = (ht_template*)mnt->data;
                        if (i/* - 1*/ == tt->name.length && !strncmpw(name_buffer.c_str, tt->name.c_str, i/* - 1*/)) {
                            t = tt;
                            if (t != NULL) {
                                field_location* fl = (field_location*)malloc(sizeof(field_location));
                                fl->pos = idx - difference;
                                fl->idx = j;
                                list_add(fields_list, (void*)fl);
#if DEBUG_MODE
                                //printf("t3: %p\tpos: %zu\tname: %ls\n", t, fl->pos, t->name.c_str);
#endif
                            }
                            break;
                        } else {
                            mnt = mnt->next;
                        }
                    }
                    // check if ninner mode
                    if (t != NULL && (t->flags & TEMPLATE_NINNER)) {
                        ninner = 1;
                        aninner = 1;
                        *inner = nullstr;
                        goto widget_name_to_arg_parse;
                    }
                    ///////////////////////
                    inner->c_str = in.c_str + idx + 3 + i;
                    ;break;
                } else {
                    //putc('@', stdout);
                    //printf("c(%ld, '%lc')", (long int)c, c);
                    break;
                }
            }
        widget_name_to_arg_parse:;
            if (/*!in_args && */t == NULL) {
                // in case tag is not a widget
                str_append_ch(&ground, charl('<'));
                continue;
            }
            args_buffer.c_str = in.c_str + idx + 1 + i;
            size_t b = i;
            charw str_bounds = charl('\0');
            size_t nest = 0;
            charw lua_str_mode = charl('\0');
            unsigned char neutral_mode = 0;
            // TODO: remove this abomination
            for (; (c = in.c_str[idx + 1 + i]) != charl('\0'); i++) {
                switch (c) {
                case charl('['):
                    if (str_bounds == charl('}') && !strncmpw(in.c_str + idx + 1 + i, strl("[["), 2)) {
                        if (lua_str_mode == charl('\0')) {
                            lua_str_mode = charl(']');
                            i++;
                        }
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                    break;
                case charl(']'):
                    if (str_bounds == charl('}') && !strncmpw(in.c_str + idx + 1 + i, strl("]]"), 2)) {
                        if (neutral_mode == 2) {
                            neutral_mode = 0;
                            i++;
                        } else if (lua_str_mode == charl(']') && !neutral_mode) {
                            lua_str_mode = charl('\0');
                            i++;
                        }
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                    break;
                case charl('-'):
                    if (str_bounds == charl('`') && neutral_mode) {
                        neutral_mode = 0;
                    } else if (str_bounds == charl('}') && neutral_mode == 0) {
                        if (!strncmpw(in.c_str + idx + 1 + i, strl("--[["), 4)) {
                            neutral_mode = 2;
                            i += 3;
                        } else if (!strncmpw(in.c_str + idx + 1 + i, strl("--"), 2)) {
                            neutral_mode = 1;
                            i++;
                        }
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                    break;
                case charl('\\'):
                    if (str_bounds == charl('`')) {
                        neutral_mode = !neutral_mode;
                    } else if (lua_str_mode != charl('\0') && neutral_mode == 0 || neutral_mode == 3) {
                        neutral_mode = (!neutral_mode) * 3;
                    }
                    break;
                case charl('}'):
                    if (str_bounds == charl('`') && neutral_mode) {
                        neutral_mode = 0;
                        break;
                    }
                    if (str_bounds != charl('}')) {
                        break;
                    }
                    if (!neutral_mode && lua_str_mode == charl('\0')) {
                        if (nest > 0) {
                            nest--;
                        } else {
                            str_bounds = charl('\0');
                        }
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                    break;
                case charl('{'):
                    if (str_bounds == charl('`') && neutral_mode) {
                        neutral_mode = 0;
                        break;
                    }
                    if (str_bounds == charl('\0')) {
                        str_bounds = charl('}');
                        // why was this commented out?
                        // because the whole loop is ass
                        //puts(in.c_str + idx + i + 1);
                        /*luax_transpile_result o = luax_transpile(in.c_str + idx + i + 2, idx + i, ctx);
                        if (o.error) { // ofc a more accurate error log will be implemented
                            printf("[HTMW LuaX Error]: there is an error on the LuaX syntax (txt)\n", o.error);
                            exit(1);
                        }
                        luax_str_view sv = o.result.first;
                        //args_buffer.c_str = sv.s;
                        //args_buffer.length = sv.len;
                        //args_buffer.size = 0;
                        //puts(sv.s);
                        //printf("len: %zu\n", sv.len);
                        i += o.result.second;
                        //free(o.result.first.s);
                        //puts(in.c_str + idx + i);

                        //neutral_mode = 0;*/
                        break;
                    }
                    if (str_bounds != charl('}')) {
                        break;
                    }
                    if (!neutral_mode && lua_str_mode == charl('\0')) {
                        nest++;
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                    break;
                case charl('`'): // NEW
                case charl('\''):
                case charl('"'):
                    if (str_bounds == charl('`') && neutral_mode) {
                        neutral_mode = 0;
                        break;
                    } else if (str_bounds == charl('}') && !neutral_mode && c != charl('`')) {
                        //lua_str_mode = lua_str_mode == c ? charl('\0') : c;
                        if (lua_str_mode == c) {
                            lua_str_mode = charl('\0');
                        } else if (lua_str_mode == charl('\0')) {
                            lua_str_mode = c;
                        }
                        break;
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                    if (c == str_bounds) {
                        str_bounds = charl('\0');
                    } else if (str_bounds == charl('\0')) {
                        str_bounds = c;
                    }
                    break;
                case charl('/'):
                    if (str_bounds == charl('`') && neutral_mode) {
                        neutral_mode = 0;
                        break;
                    }
                    if (str_bounds == charl('\0')) {
                        dninner = 1;
                        goto widget_arg_parse_to_arg_process;
                    }
                    break;
                case charl('>'):
                    if (str_bounds == charl('`') && neutral_mode) {
                        neutral_mode = 0;
                        break;
                    }
                    if (str_bounds == charl('\0')) {
                        if (!ninner) {
                            inner->c_str = in.c_str + idx + 2 + i;
                        }
                        goto widget_arg_parse_to_arg_process;
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    }
                    break;
                case '\n':
                    if (str_bounds == charl('`') && neutral_mode == 1) {
                        neutral_mode = 0;
                    } else if (str_bounds == charl('}') && neutral_mode == 1) {
                        neutral_mode = 0;
                    } else if (neutral_mode == 3) {
                        neutral_mode = 0;
                    } else if (lua_str_mode == charl('"') || lua_str_mode == charl('\'')) {
                        lua_str_mode = charl('\0');
                    }
                    break;
                default:
                    if (neutral_mode == 3) {
                        neutral_mode = 0;
                    } else if (str_bounds == charl('`') && neutral_mode) {
                        neutral_mode = 0;
                        break;
                    }
                    break;
                }
            }
            widget_arg_parse_to_arg_process:
            args_buffer.length = i - b;
            #endif
            //printf(">>>|%ls\n", )
            //str_debug(args_buffer);
            txt_list_tot_len argsln;
            list* argsl = NULL;
            //printf("=>>%zu\n", args_buffer.length);
            //str_debug(args_buffer);

            if (txt_parse_args(t, args_buffer, &argsln, ctx)) {
                argsl = argsln.txts;
                //printf("---argsl: %p <=  %zu\n", argsl, testt__);
                list_add(argsls, (void*)argsl);
                //printf("last: %p\n", argsls->last->data);
                testt__++;
            } else {
                // exception already handled (TEMPORARY)
            }
            args_count += argsl->size;
            args_block_size += argsln.len;
            //printf("str_isnull: %d\n", str_isnull(*inner));
            //str_debug(*inner);
            //printf("args_count: %zu, args_block_size: %zu\n", args_count, args_block_size);
            #if NEW_TXT_PROCESSING
            size_t b = i;
            #endif
            //static int ccccc;
            //ccccc++;
            //printf("ccccc: %zu, inner.c_str: %p\n", ccccc, inner->c_str);
            if (str_isnull(*inner)) {
                // if widget doesn't have a body or is a ninner e.g. <widget/>
                inners_block_size++;
                list_add(inners_list, inner);
                /*size_t d = 0;
                charw c;
                //charw 
                for (size_t i = 0; i < in.length; i++) {

                }*/
                //printf("%zu + 3 - %d + %d\n", i, aninner, dninner * aninner);
                #if NEW_TXT_PROCESSING
                //size_t diffs[] = {0, 0, 15, 19, 4};
                //size_t idxs[] = {0, 4, 21, 27, 5};
                difference += i + 3 - aninner + (dninner * aninner);
                //difference = diffs[ccccc];
                idx += i + 2 - aninner + (dninner * aninner);
                //idx = idxs[ccccc];
                //difference += i + 3 - aninner + (dninner * aninner);
                //idx += i + 2 - aninner + (dninner * aninner);
                #else
                difference += i + 3 - aninner + (dninner * aninner);
                idx += i + 2 - aninner + (dninner * aninner);
                #endif
            //printf("1: [ccccc: %d, idx: %zu, i: %zu, b: %zu, diff: %zu]\n", ccccc, idx, i, b, difference);
                continue;
            }
            //printf("2: [ccccc: %d, idx: %zu, i: %zu, b: %zu, diff: %zu]\n", ccccc, idx, i, b, difference);
            b = i;
            size_t nt_count = 0; // nested template with the same type count (avoids the first </a> closing in `<a><a></a></a>`)
            for (; (c = in.c_str[idx + 1 + i]) != charl('\0'); i++) {
                if (c == charl('<')) {

                    #define NEW_INNER_PROCESSING 1
                    #if NEW_INNER_PROCESSING
                    // HERE
                    string in_tag = (string){
                        .c_str = in.c_str + idx + 1 + i,
                        .length = in.length - i - idx - 1,
                        .size = 0
                    };
                    struct tag_info_lite_s e = htmw_analyze_tag(ctx, in_tag, t->name.c_str, t->name.length);

                    //str_debug(in_tag);
                    //str_debug(e.name);

                    i += e.n;

                    if (e.name.c_str == NULL) continue;
                    if ((e.flags ^ (1 << 2)) && e.name.length == t->name.length && !strncmpw(e.name.c_str, t->name.c_str, t->name.length)) {
                    //  ^^^^^^^^^^^^^^^^^^^^ => NOT native
                        /*if (t->flags & TEMPLATE_NINNER) {
                            inner->length = i - b - 1;
                            idx += i;
                            difference += i;
                            break;
                        } else */if (e.flags & (1 << 0)) { // closing tag
                            if (nt_count > 0) // same type nesting and NOT ninner (not in the condition cuz unreachable)
                                nt_count--;
                            else {
                                inner->length = i - b - 1 - e.n;
                                idx += i;
                                difference += i + 1;
                                break;
                            }
                        } else if ((e.flags ^ (1 << 1)) && (t->flags ^ TEMPLATE_NINNER)) { // NOT self-closing and NOT ninner
                            nt_count++;
                        }
                        i--;
                    }
                    #else
                    if (in.c_str[idx + i + 2] == charl('/')) {
                        charw after = in.c_str[idx + 3 + i + t->name.length];
                        if (!strncmpw(in.c_str + idx + 3 + i, t->name.c_str, t->name.length)) {
                            if (after == charl('>')) {
                                if (nt_count)
                                    nt_count--;
                                else {
                                    inner->length = i - b - 1;
                                    idx += 3 + i + 0 + t->name.length;
                                    difference += 3 + i + 1 + t->name.length;
                                    break;
                                }
                            } else if (after == charl(' ') || after == charl('\n') || after == charl('\t') || after == charl('\r')) {
                                charw c;
                                for (size_t j = 0; (c = in.c_str[idx + 3 + i + t->name.length + j]) != charl('\0'); j++) {
                                    if (c == charl('>')) {
                                        if (nt_count)
                                            nt_count--;
                                        else {
                                            inner->length = i - b - 1;
                                            idx += i + j + 3 + t->name.length + 0;
                                            difference += i + j + 3 + t->name.length + 1;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    } else if (!strncmpw(in.c_str + idx + 2 + i, t->name.c_str, t->name.length)) {
                        charw after = in.c_str[idx + 2 + i + t->name.length];
                        if (after == charl('/')) {
                            //nt_count++;
                        } else if (after == charl(' ') || after == charl('\n') || after == charl('\t') || after == charl('>') || after == charl('\r')) {
                            //int is_ninner = 0;
                            int is_ninner = (t->flags & TEMPLATE_NINNER);
                            for (size_t j = 0; (c = in.c_str[idx + 1 + i + j]) != charl('>'); j++) {
                                if (c == charl('/')) {
                                    is_ninner = 1;
                                    break;
                                }
                            }
                            if (!is_ninner) {
                                nt_count++;
                            }
                        }
                    }
                    #endif
                }
            }
            //idx += inner.length + args_buffer.length;
            //printf("___%%zu->> '%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc\")<<<'\n", in.c_str[idx], in.c_str[idx + 1], in.c_str[idx + 2], in.c_str[idx + 3], in.c_str[idx + 4], in.c_str[idx + 5], in.c_str[idx + 6], in.c_str[idx + 7], in.c_str[idx + 8], in.c_str[idx + 9], in.c_str[idx + 10], in.c_str[idx + 11], in.c_str[idx + 12], in.c_str[idx + 13], in.c_str[idx + 14], in.c_str[idx + 15], in.c_str[idx + 16], in.c_str[idx + 17], in.c_str[idx + 18], in.c_str[idx + 19]);
            //printf("G___%%zu->> '%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc%lc\")<<<'\n", in.c_str[idx - 19], in.c_str[idx - 18], in.c_str[idx - 17], in.c_str[idx - 16], in.c_str[idx - 15], in.c_str[idx - 14], in.c_str[idx - 13], in.c_str[idx - 12], in.c_str[idx - 11], in.c_str[idx - 10], in.c_str[idx - 9], in.c_str[idx - 8], in.c_str[idx - 7], in.c_str[idx - 6], in.c_str[idx - 5], in.c_str[idx - 4], in.c_str[idx - 3], in.c_str[idx - 2], in.c_str[idx - 1], in.c_str[idx]);
            //printf("G___%%zu----('%lc')----\n", in.c_str[idx]);

#if 0 || DEBUG_MODE
            puts("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE");
            str_debug(*inner);
#endif
            inners_block_size += inner->length + 1;
            list_add(inners_list, inner);
        } else {
            str_append_ch(&ground, c);
        }
    }

    // pack all the info
    out.ground = ground;
    //out._args = (charw*)malloc(args_block_size * sizeof(charw));
    out._inners = (charw*)malloc(inners_block_size * sizeof(charw));
    out.arg_shifts = (size_t*)malloc(args_count * sizeof(size_t));
    out.inner_shifts = (size_t*)malloc(inners_list->size * sizeof(size_t));
    out.fields = (field_location*)malloc(fields_list->size * sizeof(field_location));
    out.fields_count = fields_list->size;
    out.args_count = 0;
    //out.

    //list_node* ln = fields_list->head;
    size_t s_idx = 0;
    size_t s_shifs_idx = 0;
    list_node* ln = argsls->head;size_t ti = 0;
    { // pack args
        while (ln != NULL) {
            list* l = (list*)ln->data;
            out.args_count += l->size;
            {
                list_node* ln = l->head;
                while (ln != NULL) {
                    ht_template_arg_value* o = (ht_template_arg_value*)ln->data;
                    string s = o->value;
                    /*for (size_t i = 0; i < s.length; i++) {
                        out._args[s_idx++] = s.c_str[i];
                    }
                    out._args[s_idx++] = charl('\0');*/

                    const strplex_element *e = strplex_pushstr(&out.args, o->flags, s.c_str != NULL && !(o->flags & T_ARGV_NONE) ? s.c_str : "", s.length + 1);

                    //str_debug(s);
                    //puts("99999999999999999999999999999999");

                    #if NEW_PROCESS_TEMPLATE
                    out.arg_shifts[s_shifs_idx++] = (size_t)((uintptr_t)e - (uintptr_t)out.args.data);
                    //puts("---PROCESS TXT ARG");
                    //puts(e->value);
                    #else
                    out.arg_shifts[s_shifs_idx++] = s_idx;
                    #endif

                    ln = ln->next;
                    if (flag_includes(o->flags, T_ARGV_STRLT) || flag_includes(o->flags, T_ARGV_VALUE)) {
                        str_delete(&o->value); // or `s` they share the same memory
                    }
                    free(o);
                    //printf("1) %p -> \"%ls\"\n", out.args + s_idx - 1, out.args[s_idx - 1]);
                    //str_delete(s); do NOT delete
                }
            }
            list_delete(l);
            ln = ln->next;
        ti++;}
    }
    list_delete(argsls);
    s_idx = 0;
    s_shifs_idx = 0;
    ln = inners_list->head;
    {
        while (ln != NULL) {
            string* s = (string*)ln->data;
            for (size_t i = 0; i < s->length; i++) {
                out._inners[s_idx++] = s->c_str[i];
            }
            out._inners[s_idx++] = charl('\0');
            out.inner_shifts[s_shifs_idx++] = s_idx;
            ln = ln->next;
            free(s);
            //str_delete(s);
        }
    }
    list_delete(inners_list);
    s_idx = 0;
    ln = fields_list->head;
    {
        while (ln != NULL) {
            field_location* fl = (field_location*)ln->data;
            out.fields[s_idx++] = *fl;
            ln = ln->next;
            free(fl);
        }
    }
    list_delete(fields_list);
    // TODO: implement list deletion directly in the loops to avoid unnecessary iterations
    /*for (size_t i = 0; i < fields_list->size; i++) {
        
        ln = ln->next;
    }*/

    return out;
}

#if !NEW_COMPILE_TXT
string htmw_compile_txt(const htmw_context *ctx, txt_template_fill_data* tf) {
    size_t difference = 0;
    string* fills = (string*)malloc(tf->fields_count * sizeof(string));
    ht_template** templates = (ht_template**)malloc(ctx->templates->size * sizeof(ht_template*));
    u_map_node* mnt = ctx->templates->head;
    for (size_t i = 0; i < ctx->templates->size; i++) {
        templates[i] = mnt->data;
        mnt = mnt->next;
    }
    size_t idx = 0;
    for (size_t i = 0; i < tf->fields_count; i++) {
        field_location fl = tf->fields[i];
        ht_template* t = templates[fl.idx];
        size_t template_args_count = t->args->size;
        //string* args = (string*)malloc(template_args_count * sizeof(string)); // using string struct
        charw** args = (charw**)malloc(template_args_count * sizeof(charw*)); // using string struct
        for (size_t j = 0; j < template_args_count; j++) {
            //printf("->%ls\n", tf->args + (idx ? tf->arg_shifts[idx - 1] : 0));
            size_t arg_shift = idx ? tf->arg_shifts[idx - 1] : 0;
            /*args[j] = (string){  // using string struct
                .c_str = tf->args + arg_shift,
                .length = tf->arg_shifts[idx] - arg_shift - 1,
                .size = 0
            };*/
            args[j] = tf->_args + arg_shift;
            //printf("len: %zu\n", args[j].length);
            //printf(">>>>>\n%ls\n", args[j]);
            idx++;
        }
        size_t inner_shift = i ? tf->inner_shifts[i - 1] : 0;
        string inner = (string){
            .c_str = tf->_inners + inner_shift,
            .length = tf->inner_shifts[i] - inner_shift - 1,
            0
        };
        //str_debug(inner);
        fills[i] = process_template(ctx, t, args, inner);
        difference += fills[i].length;
    }
    size_t len = tf->ground.length + difference;
    string out = str_new(len + 1);
    size_t list_shift = 0;
    //printf("----------%s\n", tf->inners);
    for (size_t i = 0; i < tf->ground.length + 1; i++) {
        if (list_shift < tf->fields_count) {
            field_location f = tf->fields[list_shift];
            if (i == f.pos) {
                //printf(">%zu => %zu {%s}\n", list_shift, fills[list_shift].length, fills[list_shift].c_str);
                //str_debug(fills[list_shift]);
                for (size_t j = 0; j < fills[list_shift].length; j++) {
                    charw c = fills[list_shift].c_str[j];
                    if (c != charl('\0'))
                        str_append_ch(&out, c);
                }
                list_shift++;
                i--;
            } else {
                charw c = tf->ground.c_str[i];
                if (c != charl('\0'))
                    str_append_ch(&out, c);
            }
        } else {
            charw c = tf->ground.c_str[i];
            if (c != charl('\0'))
                str_append_ch(&out, c);
        }
    }
    //str_debug(out);
    return out;
}
#else
string htmw_compile_txt(const htmw_context *ctx, const txt_template_fill_data *tf) {
    size_t difference = 0;
    string* fills = (string*)malloc(tf->fields_count * sizeof(string));
    ht_template** templates = (ht_template**)malloc(ctx->templates->size * sizeof(ht_template*));
    u_map_node* mnt = ctx->templates->head;
    for (size_t i = 0; i < ctx->templates->size; i++) {
        templates[i] = mnt->data;
        mnt = mnt->next;
    }

    size_t idx = 0;
    //strplex_element *e;
    for (size_t i = 0; i < tf->fields_count; i++) {
        field_location fl = tf->fields[i];
        ht_template* t = templates[fl.idx];
        size_t template_args_count = t->args->size;
        charw** args = (charw**)malloc(template_args_count * sizeof(charw*)); // using string struct
        //puts("---CT_ARGS---");
        size_t arg_shift = tf->arg_shifts[idx];
        strplex sp = {
            .capacity = tf->args.capacity - arg_shift,
            .count = t->args->size,
            .endn = tf->args.endn,
            .data = tf->args.data + arg_shift
        };
        #if 0
        for (size_t j = 0; j < template_args_count; j++) {
            size_t arg_shift = idx ? tf->arg_shifts[idx - 1] : 0;
            args[j] = tf->args.data + arg_shift + sizeof(strplex_element);
            printf("ARG(%zu, %zu): \"%s\"\n", i, j, args[j]);
            idx++;
        }
        #endif
        size_t inner_shift = i ? tf->inner_shifts[i - 1] : 0;
        string inner = (string){
            .c_str = tf->_inners + inner_shift,
            .length = tf->inner_shifts[i] - inner_shift - 1,
            0
        };
        //strplex_print(&sp);
        //printf("args.size: %zu\n", t->args->size);
        fills[i] = process_template(ctx, t, &tf->args, tf->arg_shifts + idx, inner);
        difference += fills[i].length;
        idx += template_args_count;
    }
    size_t len = tf->ground.length + difference;
    string out = str_new(len + 1);
    size_t list_shift = 0;
    for (size_t i = 0; i < tf->ground.length + 1; i++) {
        if (list_shift < tf->fields_count) {
            field_location f = tf->fields[list_shift];
            if (i == f.pos) {
                for (size_t j = 0; j < fills[list_shift].length; j++) {
                    charw c = fills[list_shift].c_str[j];
                    if (c != charl('\0'))
                        str_append_ch(&out, c);
                }
                list_shift++;
                i--;
            } else {
                charw c = tf->ground.c_str[i];
                if (c != charl('\0'))
                    str_append_ch(&out, c);
            }
        } else {
            charw c = tf->ground.c_str[i];
            if (c != charl('\0'))
                str_append_ch(&out, c);
        }
    }
    return out;
}
#endif

string htmw_postprocess(string s) {
    string out = str_new(s.length + 1);
    for (size_t i = 0; i < s.length; i++) {
        if (s.c_str[i] == charl('<') && s.c_str[i + 1] == charl('#')) {
            str_append_ch(&out, charl('<'));
            i++;
        } else if (s.c_str[i] == charl('<') && s.c_str[i + 1] == charl('/') && s.c_str[i + 2] == charl('#')) {
            str_append_ch(&out, charl('<'));
            str_append_ch(&out, charl('/'));
            i += 2;
        } else {
            str_append_ch(&out, s.c_str[i]);
        }
    }
    return out;
}

// parses the include path from the command line argument (`-I` exluded)
/*void arg_add_include_path(const char *s) {
    /*char value_bounds = '\0';
    size_t idx = 0;
    if (s[0] == '"' || s[0] == '\'')
        value_bounds = s[idx++];*
    // apparently stringed values are automatically handled by the system even if it starts with a character

    list_add

    printf("global include path val: |%s|\n", s);
}*/

void htmw_remove_unsafe(lua_State* L) {
    lua_pushnil(L);
    lua_setglobal(L, "os");
    lua_pushnil(L);
    lua_setglobal(L, "io");
    lua_pushnil(L);
    lua_setglobal(L, "require");
    lua_pushnil(L);
    lua_setglobal(L, "dofile");
    lua_pushnil(L);
    lua_setglobal(L, "loadfile");
    lua_pushnil(L);
    lua_setglobal(L, "package");
}

int _main(void) {
    PCRE2_SPTR pattern = (PCRE2_SPTR)"^[a-z]+@[a-z]+\\.com$";
    PCRE2_SPTR subject = (PCRE2_SPTR)"test@example.com";

    int errorcode;
    PCRE2_SIZE erroroffset;

    pcre2_code *re = pcre2_compile(
        pattern,
        PCRE2_ZERO_TERMINATED,
        0,
        &errorcode,
        &erroroffset,
        NULL
    );

    if (!re) {
        printf("Regex compilation failed at offset %d\n", (int)erroroffset);
        return 1;
    }

    pcre2_match_data *match_data =
        pcre2_match_data_create_from_pattern(re, NULL);

    int rc = pcre2_match(
        re,
        subject,
        strlen((char *)subject),
        0,
        0,
        match_data,
        NULL
    );

    if (rc >= 0)
        printf("Match!\n");
    else
        printf("No match\n");

    pcre2_match_data_free(match_data);
    pcre2_code_free(re);

    return 0;
}