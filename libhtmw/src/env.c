#include "env.h"
#include "htmw.h"
#include "str.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

env_table env_table_new(const size_t capacity) {
    env_table table = {
        .records = NULL,
        .length = 0,
        .capacity = 0
    };

    table.records = (env_record *)malloc(capacity * sizeof(env_record));
    
    if (table.records == NULL)
        return table;

    table.capacity = capacity;
    return table;
}

void env_record_delete(env_record *const record) {
    str_delete(&record->key);
    str_delete(&record->value);
}

void env_table_delete(env_table *const table) {
    for (size_t i = 0; i < table->length; i++) {
        env_record_delete(&table->records[i]);
    }
    free(table->records);
    table->records = NULL;
    table->capacity = 0;
    table->length = 0;
}

// mk: allocates memory in both vectors (if necessary) and the strings (creates copies if owned strings are passed)
int env_table_push(env_table *const table, const string key, const string value) {
    if (table->records == NULL || table->capacity == 0) {
        *table = env_table_new(4); // mk: default capacity
        if (table->records == NULL)
            return 0;
    } else if (table->length >= table->capacity) {
        if (!env_table_reserve(table, table->capacity * 2)) {
            return 0;
        }
    }

    table->records[table->length] = (env_record){
        .key = str_clone(key),
        .value = value.size == 0 ? str_clone(value) : value
    };
    table->length++;

    return 1;
}

int env_table_reserve(env_table *const table, const size_t capacity) {
    if (table->records == NULL)
        return 0;

    if (capacity > table->capacity) {
        env_record *const block = (env_record *)realloc(table->records, capacity * sizeof(env_record));
        if (block == NULL)
            return 0;
        table->records = block;
        table->capacity = capacity;
    }

    return 1;
}

size_t env_table_find_key(const env_table *const table, const char *const key) {
    return env_table_find_key_having_length(table, key, SIZE_MAX);
}

size_t env_table_find_key_having_length(const env_table *const table, const char *const key, const size_t length) {
    size_t n = 0;
    while (key[n] != '\0' && n < length) {
        n++;
    }

    for (size_t i = 0; i < table->length; i++) {
        const env_record record = table->records[i];
        if (!strncmp(key, record.key.c_str, n)) {
            if (n == record.key.length)
                return i;
        }
    }

    return SIZE_MAX;
}

// mk: this seems not that useful for now
/*struct env_parse_component {
    string data_view;   // mk: if `nullstr` => invalid key or value
    size_t offset;      // mk: how many bytes did it have to shift before reaching the first valid character
};*/

static string env_parse_key(const char *const src) {
    string key_view = nullstr;
    key_view.c_str = src;

    while (src[key_view.length] != '\0' && src[key_view.length] != '=' && src[key_view.length] != ' ' && src[key_view.length] != '\t') {
        if (!(
            (src[key_view.length] >= 'a' && src[key_view.length] <= 'z') ||
            (src[key_view.length] >= 'A' && src[key_view.length] <= 'Z') ||
            (src[key_view.length] >= '0' && src[key_view.length] <= '9') ||
            src[key_view.length] >= '_'
        )) {
            return nullstr;
        }
        key_view.length++;
    }

    return key_view;
}

#define IS_ENV_KEY_CHAR(x) (((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z') || ((x) >= '0' && (x) <= '9') || (x) == '_')

static size_t env_handle_value_string_interpolation(string *const s, const char *const src, const env_table table) {
    const short is_delimited = src[0] == '{';
    size_t n = is_delimited;

    const char *keyptr = is_delimited ? NULL : src;

    if (!is_delimited && !IS_ENV_KEY_CHAR(src[0])) {
        str_append_ch_and_resize(s, '$');
        return 0;
    }

    //size_t phase = 0; // mk: 0 => `${ `, 1 => `${ xyz`, 2 => `${ xyz `
    uint8_t after = 0;
    size_t keylen = 0;

    while (src[n] != '\0') {
        if (IS_ENV_KEY_CHAR(src[n])) {
            if (after) {
                // mk: `${ var  this_shouldnt_be_here }`
                return SIZE_MAX;
            } else if (keyptr == NULL) {
                keyptr = src + n;
            }
            keylen++;
        } else if (src[n] == ' ' || src[n] == '\t') {
            if (!after && is_delimited && keyptr != NULL) {
                after = 1;
            }
        } else if (src[n] == '}') {
            if (is_delimited) {
                if (keyptr == NULL) {
                    // mk: `${}` (empty)
                    return SIZE_MAX;
                }
                n++;
            }
            break;
        } else {
            if (is_delimited) {
                // mk: `${ var! }`
                return SIZE_MAX;
            }
            break;
        }

        n++;
    }

    const size_t idx = env_table_find_key_having_length(&table, keyptr, keylen);

    if (idx == SIZE_MAX) {
        // mk: just do nothing ig.
    } else {
        const string val = table.records[idx].value;
        str_append_and_resize(s, val.c_str, val.length);
    }

    return n;
}

static string env_parse_value(const char *const src, const env_table table) {
    string value = nullstr;
    size_t i = 0;
    char boundaries;

    if (src[i] == '"' || src[i] == '\'') {
        boundaries = src[i];
        i++;
        value.c_str = src + i;
    } else {
        boundaries = '\0';
    }

    while (1) {
        if (src[i] == '\0') {
            if (boundaries != '\0') {
                // mk: todo: handle error: unclosed value string
                throw_error("unclosed value string in ENV", E_EXIT);
            } else {
                break;
            }
        } else if (src[i] == ' ' || src[i] == '\t') {
            if (boundaries == '\0') {
                break;
            }
        } else if (boundaries == '\0' && src[i] == '=') {
            // mk: todo: handle error: double `=` sign
            throw_error("double `=` in ENV", E_EXIT);
        } else if (src[i] == '"' || src[i] == '\'') {
            if (boundaries == src[i]) {
                break;
            }
        } else if (src[i] == '\\' && boundaries == '"') {
            // mk: the string becomes owned from here as well
            if (value.size == 0) {
                value = str_clone(value);
            }

            i++;
            char add;
            switch (src[i]) {
                case 'n':   add = '\n'  ; break;
                case 't':   add = '\t'  ; break;
                default :   add = src[i]; break;
            }
            str_append_ch_and_resize(&value, add);
            i++;
            continue;
        } else if (src[i] == '\\' && boundaries == '\'') {
            if (value.size == 0) {
                value = str_clone(value);
            }
            if (src[i + 1] == '\'') {
                str_append_ch_and_resize(&value, '\'');
                i += 2;
                continue;
            }
        } else if (src[i] == '$' && boundaries != '\'') {
            // mk: handle value interpolation
            // mk: warning from now on the string becomes owned
            if (value.size == 0) {
                value = str_clone(value);
            }

            i++;

            const size_t n = env_handle_value_string_interpolation(&value, src + i, table);

            if (n == SIZE_MAX) {
                // mk: error
                throw_error("invalid string interpolation in ENV", E_EXIT);
            }

            i += n;
        } else {
            if (str_isnull(value)) {
                value.c_str = src + i;
            }
        }

        if (value.size == 0)
            value.length++;
        else
            str_append_ch_and_resize(&value, src[i]);
        i++;
    }

    return value;
}

// mk: returns non-owned keys and incognito-owned values, so the `env_parse_stream` function can pass it to `env_table_push` which creates a clone of the keys & values
static env_record env_parse_record(const char *const src, const env_table table) {
    const string key = env_parse_key(src);
    string value = nullstr;
    size_t i = key.length;

    if (str_isnull(key)) {
        // mk: todo: handle error: invalid character in key
        throw_error("invalid character in key in ENV", E_EXIT);
    }

    size_t eq_pos = SIZE_MAX;

    while (src[i] != '\0') {
        if (src[i] == ' ' || src[i] == '\t') {
            
        } else if (src[i] == '=' && eq_pos == SIZE_MAX) {
            eq_pos = i;
        } else if (eq_pos != SIZE_MAX) {
            value = env_parse_value(src + i, table);
            if (str_isnull(value)) { // mk: this is different from empty value `key=` because an empty value would be pointing to a string but with length = 0
                // mk: todo: handle error: invalid value
                throw_error("invalid value in ENV", E_EXIT);
            }
            break;
        } else {
            // mk: todo: handle error: invalid character or double equal sign
            throw_error("double `=` in ENV", E_EXIT);
        }
        i++;
    }

    return (env_record){
        .key = key,
        .value = value
    };
}

env_table env_parse_stream(FILE *const f) {
    env_table table = {0};
    char line[4096];

    while (fgets(line, 4096, f)) {
        size_t i = 0;
        while (line[i] != '\0') {
            if (line[i] == '#') {
                break;
            } else if (line[i] != ' ' && line[i] != '\t') {
                const env_record record = env_parse_record(line + i, table);
                if (!env_table_push(&table, record.key, record.value)) {
                    // mk: todo: handle error: invalid allocation
                    throw_error("invalid allocation occurred during ENV registration", E_EXIT);
                }
                break;
            }
            i++;
        }
    }

    return table;
}
