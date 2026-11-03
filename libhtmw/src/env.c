#include "env.h"
#include "str.h"

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
        .value = str_clone(value)
    };

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

static string env_parse_value(const char *const src, const env_table table) {
    string value_view = nullstr;
    size_t i = 0;
    char boundaries;

    if (src[i] == '"' || src[i] == '\'') {
        boundaries = src[i];
        i++;
    } else {
        boundaries = '\0';
    }

    value_view.c_str = src + i;

    while (1) {
        if (src[i] == '\0') {
            if (boundaries != '\0') {
                // mk: todo: handle error: unclosed value string
            } else {
                break;
            }
        } else if (src[i] == ' ' || src[i] == '\t') {
            if (boundaries == '\0') {
                break;
            }
        } else if (src[i] == '"' || src[i] == '\'') {
            if (boundaries == src[i]) {
                break;
            }
        } else if (src[i] == '\\') {
            // mk: the string becomes owned from here as well
        } else if (src[i] == '$') {
            // mk: handle value interpolation
            // mk: warning from now on the string becomes owned
        }

        value_view.length++;
    }

    return value_view;
}

// mk: returns non-owned values, so the `env_parse_stream` function can pass it to `env_table_push` which creates a clone of the keys & values
static env_record env_parse_record(const char *const src, const env_table table) {
    const string key = env_parse_key(src);
    string value = nullstr;
    size_t i = key.length;

    if (str_isnull(key)) {
        // mk: todo: handle error: invalid character in key
    }

    size_t eq_pos = SIZE_MAX;

    while (src[i] != '\0') {
        if (src[i] == ' ' || src[i] == '\t') {
            continue;
        } else if (src[i] == '=' && eq_pos == SIZE_MAX) {
            eq_pos = i;
        } else if (eq_pos != SIZE_MAX) {
            value = env_parse_value(src + i, table);
            if (str_isnull(value)) { // mk: this is different from empty value `key=` because an empty value would be pointing to a string but with length = 0
                // mk: todo: handle error: invalid value
            }
        } else {
            // mk: todo: handle error: invalid character or double equal sign
        }
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
        if (line[0] == '#') {
            continue;
        }

        size_t i = 0;
        while (line[i] != '\0') {
            if (line[i] != ' ') {
                const env_record record = env_parse_record(line + i, table);
                if (!env_table_push(&table, record.key, record.value)) {
                    // mk: todo: handle error: invalid allocation
                }
            }
            i++;
        }
    }

    return table;
}
