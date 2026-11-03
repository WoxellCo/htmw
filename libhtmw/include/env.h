#ifndef ENV_H
#define ENV_H

#include "str.h"
#include <stddef.h>

typedef struct {
    string key;
    string value;
} env_record;

typedef struct {
    env_record *records;
    size_t length;
    size_t capacity;
} env_table;

env_table env_table_new(const size_t capacity);
void env_table_delete(env_table *const table);
int env_table_push(env_table *const table, const string key, const string value);
int env_table_reserve(env_table *const table, const size_t capacity);
size_t env_table_find_key(const env_table *const table, const char *const key);
size_t env_table_find_key_having_length(const env_table *const table, const char *const key, const size_t length);

void env_record_delete(env_record *const record);

env_table env_parse_stream(FILE *const f);

#endif
