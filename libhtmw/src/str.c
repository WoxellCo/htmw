#include "str.h"

string str_new(size_t max_size) {
    //printf("str_new::max_size = %zu\n", max_size);
    charw* data = (charw*)malloc(max_size * sizeof(charw));
    if (data == NULL) {
        fprintf(stderr, "Memory allocation failed in str_new\n");
        exit(EXIT_FAILURE);
    }
    data[0] = L'\0';
    return (string) {
        .length = 0,
        .size = max_size,
        .c_str = data
    };
}

string str_from(const charw *c_str) {
    size_t len = strlenw(c_str);
    string s = str_new(len + 1);
    strncpyw(s.c_str, c_str, len);
    s.c_str[len] = charl('\0');
    s.length = len;
    return s;
}

void str_delete(string* x) {
    if (x->size)
        free(x->c_str);
    *x = nullstr;
}

void str_debug(string s) {
    char c = L'\0';
    puts("---[string debug begin]---");
    printf("(length: %zu, size: %zu, c_str: %p)\"", s.length, s.size, s.c_str);
    /*for (size_t i = 0; (c = s.c_str[i]) != L'\0'; i++) {
        putwc(c, stdout);
    }*/
    for (size_t i = 0; i < s.length; i++) {
        c = s.c_str[i];
        putwc(c, stdout);
    }
    putc('"', stdout);
    puts("---[string debug end]---");
}

string str_append(string* x, const charw* s, size_t n) {
    if (x->c_str == NULL) return nullstr;
    if (s == NULL) return *x;
    //size_t add_size = wcslen(s);
    //add_size = add_size < n ? add_size : n;
    //wcsncpy(x->c_str + x->length, s, add_size);
    size_t i;
    for (i = 0; i < n && s[i] != L'\0' && x->length + i < x->size; i++) {
        x->c_str[x->length + i] = s[i];
    }
    x->length += i;
    x->c_str[x->length] = L'\0';
    return *x;
}

//#define str_append_ch(x, c) {if ((x)->length < (x)->size - 1) { (x)->c_str[(x)->length] = (c); (x)->length++; (x)->c_str[(x)->length] = L'\0'; }}

string _str_append_ch(string* x, charw c) {
    if (x->length < x->size - 1) {
        x->c_str[x->length] = c;
        x->length++;
        x->c_str[x->length] = L'\0';
    }
    return *x;
}

string str_cpy(const string x) {
    string o = (string){
        .length = x.length,
        .size = x.length + 1,
        .c_str = (charw*)malloc((x.length + 1) * sizeof(charw))
    };
#if DEBUG_MODE
    //printf("\n%u\n", x.length);
    //str_debug(x);
#endif
    strncpyw(o.c_str, x.c_str, x.length);
    o.c_str[x.length] = L'\0';
    return o;
}