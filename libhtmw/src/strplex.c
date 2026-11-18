#include "strplex.h"

strplex_element *strplex_setstr(strplex *sp, char *ptr, uint8_t flags, const char *s, uint32_t max_length) {
    size_t offset = ptr - sp->data;
    size_t available = sp->capacity - offset;
    size_t limit = available - sizeof(strplex_element);
    
    if (available <= sizeof(strplex_element))
        return NULL;

    if (max_length < limit)     limit =  max_length;
    if (STRPLEX_MAXLEN < limit) limit =  STRPLEX_MAXLEN;

    uint32_t i;
    for (i = 0; s[i] != '\0' && i < limit - 1; i++) {
        ((strplex_element *)ptr)->value[i] = s[i];
    }
    ((strplex_element *)ptr)->value[i] = '\0';

    sp->count++;
    sp->endn = (ptr - sp->data) + i + sizeof(strplex_element) + 1;

    ((strplex_element *)ptr)->meta = ((i & STRPLEX_MAXLEN) << 8) | flags;

    return (strplex_element *)ptr;
}

strplex_element *strplex_pushstr(strplex *sp, uint8_t flags, const char *s, uint32_t max_length) {
    char *ptr = strplex_nextptr(sp->data + sp->endn);
    return strplex_setstr(sp, ptr, flags, s, max_length);
}