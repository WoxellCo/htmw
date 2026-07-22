#include "htmw_tag.h"

#if 0
typedef struct htmw_tag_props_list_s {

} htmw_tag_props_list;
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

static inline uint8_t htmw_props_vector_push(ht_template_arg_value *value_vector[], size_t *n, size_t *cap, ht_template_arg_value *prop) {
    if (value_vector == NULL || n == NULL || cap == NULL || prop == NULL)
        return 0;

    if (*value_vector == NULL) {
        *value_vector = (ht_template_arg_value *)malloc(16 * sizeof(ht_template_arg_value));
        if (*value_vector == NULL)
            return 0;
        *cap = 16;
        *n = 0;
    }

    if (*n >= *cap) {
        //if (*cap <= 4)
        //    *cap = 8;
        //printf("CAP: %zu\n", *cap);

        size_t new_cap = (*cap) * 2;//1.5;
        ht_template_arg_value *tmp = (ht_template_arg_value *)realloc(*value_vector, new_cap * sizeof(ht_template_arg_value));
        if (tmp == NULL) {
            return 0;
        }
        *cap = new_cap;
        *value_vector = tmp;
    }

    if (str_isnull(prop->value)) {
        prop->flags |= T_ARGV_NONE;
    }

    //str_debug(prop->key);

    (*value_vector)[(*n)++] = *prop;
    *prop = (ht_template_arg_value){
        .key = nullstr,
        .value = nullstr,
        .flags = 0,
        .syntax_id = nullstr
    };

    return 1;
}

// this is a lightweight version of the tag parser that doesn't store the list of attributes
// 2026-02-19: maybe this will be inlined using a macro to optimize the engine
struct tag_info_s htmw_analyze_tag(const htmw_context *ctx, const string in, const char *required_name, size_t required_name_len, ht_template_arg_value *value_vector[], size_t *value_vector_size, size_t *value_vector_cap) {
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

    ht_template_arg_value prop = (ht_template_arg_value){
        .key = nullstr,
        .value = nullstr,
        .syntax_id = nullstr,
        .flags = 0
    };

    struct tag_info_s o = {
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

    size_t last_key_start_idx = 0, last_value_start_idx = 0;
    //string last_key = nullstr; // last_key
    //string last_value = nullstr; // last_value
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
                ///
                if (prop.key.length == 0) { // if key is still not completely set (<T @key>)
                    prop.key.length = i - last_key_start_idx; //last_key
                    prop.key.c_str = in.c_str + last_key_start_idx;
                }
                //prop.key = last_key;
                htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
                break;
            case htmw_tps_arg_assign:
            case htmw_tps_arg_value:
                if (bounds == '\0') {
                    loop = 0;//puts("b");
                    o.n = i + 1;
                    if (o.args.c_str != NULL && o.args.length == 0) {
                        if (!(o.flags & (1 << 0)))
                            o.args.length = i - args_start_idx;

                        ///
                        prop.value.length = i - last_value_start_idx;//last_value
                        //prop.value = last_value;
                        htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
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
                prop.key.c_str = in.c_str + last_key_start_idx;//last_key
                prop.key.length = i - last_key_start_idx;//last_key
                phase = htmw_tps_arg_key_after;// puts("AAA 3");
                break;
            case htmw_tps_arg_value:
                if (bounds == '\0') {
                    phase = htmw_tps_void;
                    prop.value.length = i - last_value_start_idx;
                    htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
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
                prop.key.c_str = in.c_str + last_key_start_idx;//last_key
                prop.key.length = i - last_key_start_idx;//last_key
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
                prop.value.c_str = in.c_str + i + 1;
                last_value_start_idx = i;
            } else if (bounds == c) {
                if (c != '`' || !value_escape) {
                    bounds = '\0';
                    phase = htmw_tps_void;
                    prop.value.length = i - last_value_start_idx - 1;
                    htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
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
                prop.value.length = ro.result.first.len;
                prop.value.c_str = ro.result.first.s;
                prop.flags |= T_ARGV_VALUE;
                htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
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
                string default_syntax_id = htmw_extract_default_syntax(t, prop.key);//last_key
                htmw_syntax_literal lit = txt_parse_syntax_literal(ctx, in.c_str + i, SIZE_MAX, default_syntax_id.c_str, default_syntax_id.length);
                if (lit.error) {
                    // TODO: throw exception
                    throw_error("there was an error in the syntax notation", E_EXIT);
                }
                //printf("%zu >> ", i);

                i += lit.total_length - 1;

                //printf("%zu, ltl %zu\n", i, lit.total_length);
                
                phase = htmw_tps_void;

                prop.value = lit.content;
                prop.syntax_id = lit.explicit_syntax_id;
                htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
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
            } else if (phase == htmw_tps_void || phase == htmw_tps_typename || phase == htmw_tps_arg_key/* || phase == htmw_tps_arg_assign*/ || phase == htmw_tps_arg_key_after || (phase == htmw_tps_arg_value && bounds == '\0')) {
                // self-closing case
                o.flags |= (1 << 1);
                if (phase == htmw_tps_arg_key || phase == htmw_tps_arg_key_after) {;
                    if (prop.key.length == 0) { // if key is still not completely set (<T @key>)
                        prop.key.length = i - last_key_start_idx; //last_key
                        prop.key.c_str = in.c_str + last_key_start_idx;
                    }
                    htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
                } else if (phase == htmw_tps_arg_value) {
                    prop.value.length = i - last_value_start_idx;
                    htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
                }

                phase = htmw_tps_self_closing;//puts("NAME: 1");
                if (o.name.c_str != NULL && o.name.length == 0) {
                    // if the name is set but the length is not updated `<Element/>`
                    o.name.length = i - 1;
                    if (required_name != NULL && o.name.length != required_name_len || strncmp(o.name.c_str, required_name, required_name_len)) {
                        // if the required name is set but not matching with the tag name
                        o.name = nullstr;
                        return o;
                    }
                } else if (o.args.c_str != NULL && o.args.length == 0 && !(o.flags & (1 << 0))) {
                    // arguments exist, but the length is not updated, and the tag is not closing (closing tags dont have arguments)
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
                case htmw_tps_arg_key_after:
                    if (o.argc > 0 && !str_isnull(prop.key)) {
                        //str_debug(prop.key);
                        htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);
                    }
                    o.argc++;
                    last_key_start_idx = i;
                    prop.key.c_str = in.c_str + i;
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
                case htmw_tps_arg_assign:
                    bounds = '\0';
                    prop.value.c_str = in.c_str + i;
                    last_value_start_idx = i;
                    phase = htmw_tps_arg_value;
                    break;
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

    // PUSH NULL (terminated)
    prop = (ht_template_arg_value){
        .key = nullstr,
        .value = nullstr,
        .syntax_id = nullstr,
        .flags = 0
    };
    htmw_props_vector_push(value_vector, value_vector_size, value_vector_cap, &prop);

    return o;
}



//void htmw_extract_tag_props(ht_template_arg_value *dest, const string *args) {}