#ifndef STR_H
#define STR_H

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define str_isempty(s) (!((s).length))
#define nullstr ((string) { 0, 0, NULL })
#define str_isnull(s) (((s).c_str == NULL) && ((s).length == 0) && ((s).size == 0))
//#define str_append_ch(x, c) {if ((x)->length < (x)->size - 1) { (x)->c_str[(x)->length] = (c); (x)->length++; (x)->c_str[(x)->length] = L'\0'; }}

#define WCHAR_ENABLED 0

#if WCHAR_ENABLED
typedef wchar_t charw;
#define strcmpw(x, y) wcscmp((x), (y))
#define strl(s) L##s
#define charl(c) L##c
#define strncmpw(x, y, z) wcsncmp((x), (y), (z))
#define strncpyw(x, y, z) wcsncpy((x), (y), (z))
#define strlenw(x) wcslen(x)
#define wcstombsw(x, y, z) wcstombs((x), (y), (z))
#define strdupw(x) wcsdup(x)
#define strncatw(x, y, z) wcsncat((x), (y), (z))
#else
typedef char charw;
#define strcmpw(x, y) strcmp((x), (y))
#define strl(s) s
#define charl(c) c
#define strncmpw(x, y, z) strncmp((x), (y), (z))
#define strncpyw(x, y, z) strncpy((x), (y), (z))
#define strlenw(x) strlen(x)
#define wcstombsw(x, y, z) strncpy((x), (y), (z))
#define strdupw(x) strdup(x)
#define strncatw(x, y, z) strncat((x), (y), (z))
#endif

typedef struct {
    size_t length;
    size_t size;
    charw* c_str;
} string;

string str_new(size_t max_size);
string str_from(const charw *c_str);
void str_delete(string* x);
void str_debug(string s);
string str_append(string* x, const charw* s, size_t n);

#define str_append_ch(x, c) {if ((x)->length < (x)->size - 1) { (x)->c_str[(x)->length] = (c); (x)->length++; (x)->c_str[(x)->length] = charl('\0'); }}

string _str_append_ch(string* x, charw c);
string str_cpy(const string x);


#ifdef __cplusplus
}
#endif
#endif