#ifndef HTMW_TAGS_H
#define HTMW_TAGS_H

#include <stdint.h>
#include "htmw.h"
#include "htmw_template.h"
#include "str.h"

enum tag_parse_status_e {
    htmw_tps_beginning = 0,         // beginning (name or slash?)
    htmw_tps_closing = 1,           // slash passed (not self-closing)
    htmw_tps_typehash = 2,          // # passed before name (native tag notation)
    htmw_tps_typename = 3,          // constructing name
    htmw_tps_void = 4,              // name/arg passed (whitespace) <= returning point
    htmw_tps_arg_key = 5,           // constructing arg name
    htmw_tps_arg_key_after = 6,     // arg name end (STILL BEFORE THE EQUAL SIGN)
    htmw_tps_arg_assign = 7,        // equals sign passed (if there's anything else other than a whitespace return to point 3)
    htmw_tps_arg_value = 8,         // constructing value
    htmw_tps_self_closing = 9,      // self-closing slash
};

// 2026-02-19
struct tag_info_s {
    size_t n;       // length of the tag like <tag attrib="val"> would be 18
    string name;    // name of the tag (this string is not in a new allocation)
    uint8_t flags;  // self closing or closing
    size_t argc;    // number of attributes
    string args;    // string view of the starting point of the attributes
};

string htmw_extract_default_syntax(const ht_template *t, const string key);
struct tag_info_s htmw_analyze_tag(const htmw_context *ctx, const string in, const char *required_name, size_t required_name_len, ht_template_arg_value **value_vector, size_t *value_vector_size, size_t *value_vector_cap);
//struct tag_info_s htmw_analyze_tag(const htmw_context *ctx, const string in, const char *required_name, size_t required_name_len);

#if 0
// this function reads the attributes part of the tag only (returned by `htmw_analyze_tag`) and
// then stores string views on a pre-allocated vector (size should be argc * sizeof(ht_template_arg_value))
void htmw_extract_tag_props(ht_template_arg_value *dest, const string *args);
#endif

#endif