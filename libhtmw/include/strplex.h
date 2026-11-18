#ifndef STRPLEX_H
#define STRPLEX_H

#include <stdint.h>
#include <stddef.h>

#define STRPLEX_MAXLEN 0x00FFFFFF
#define strplex_length(e) ((e)->meta >> 8)
#define strplex_nextptr(ptr) ((char *)(((uintptr_t)(ptr) + 3) & ~((uintptr_t)3)))
#define strplex_nextelement(e) ((strplex_element *)strplex_nextptr((e)->value + strplex_length(e)))

typedef struct strplex_s {
    size_t capacity;
    size_t count;
    size_t endn;
    char *data;
} strplex;

typedef struct strplex_element_s {
    uint32_t meta;
    char value[];
} strplex_element;

strplex_element *strplex_setstr(strplex *sp, char *ptr, uint8_t flags, const char *s, uint32_t max_length);
strplex_element *strplex_pushstr(strplex *sp, uint8_t flags, const char *s, uint32_t max_length);

#endif
