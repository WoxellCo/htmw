#ifndef HTMW_TEMPLATE_H
#define HTMW_TEMPLATE_H

#include "flag.h"
#include "str.h"
#include "nx/vector.h"

#define T_ARG_NULLABLE      (1 << 0)
#define T_ARG_DOUBLEASS     (1 << 1)
#define T_ARG_SYNTAX        (1 << 2)
#define T_ARG_EXPR          (1 << 3)

typedef struct {
    string name;
    string default_value;
    flag_t flags;                               // nullable or double ass
    string syntax_id;                           // it's a string it will be dynamically linked to increase flexibility on template definitions
} ht_template_arg;

#define T_ARGV_STRLT        (1 << 0)
#define T_ARGV_VALUE        (1 << 1)            // VALUE and EXPR ARE THE SAME THING! TODO: fix redundancy
#define T_ARGV_NONE         (1 << 2)

typedef struct {
    string key;
    string value;
    string syntax_id;
    flag_t flags;
} ht_template_arg_value;

// can ht_template_arg and ht_template_arg_value be the same?

typedef struct {
    size_t pos;
    size_t idx;
} field_location;

typedef struct {
    size_t shifts_count;                        // total number of shifts (field "tpyes" - 1, $INNER is excluded)
    size_t fields_count;                        // total number of fields on the gorund
    charw* field_names;                       // 1 block of array of wide characters, where each field name is listed
    size_t* field_shifts;                       // contains indices of each field name string except the first one
    field_location* fields;                     // contains the info where each field is located on the ground (pos) and which placeholder (idx)
    string ground;                              // inner with invisible placeholders
} ht_template_core;

typedef struct {
    string name;
    vect* args;
    string impl;
    string inner;
    ht_template_core* core;
    size_t args_min_size;
    flag_t flags;                               // ninner, dinner, darg
} ht_template;

#define TEMPLATE_NINNER     (1 << 0)            // no inner
#define TEMPLATE_DINNER     (1 << 1)            // dynamic inner
#define TEMPLATE_DARGS      (1 << 2)            // dynamic arguments

#endif