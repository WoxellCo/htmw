#include <stdio.h>
#include <wchar.h>
#include "syntaxer.h"

charw *read_file(const char* path) {
    charw *s = calloc(1000, sizeof(charw));
    FILE* f = fopen(path, "r");

    if (f == NULL) {
        puts("fopen(): err");
        exit(1);
    }

    charw* buffer = (charw*)malloc(1000 * sizeof(charw));
    while (fgetws(buffer, 1000, f)) {
        strncatw(s, buffer, 1000);
    }

    fclose(f);
    return s;
}

charw *str(const charw *s) {
    size_t len = strlenw(s);
    charw *o = (charw *)calloc(len + 1, sizeof(charw));
    strncpyw(o, s, len);
}

int main() {
    syntaxer_neutralizer neutralizers[3] = {
        (syntaxer_neutralizer){
            .begin = str(L"\""),
            .end = str(L"\""),
            .esc_char = str(L"\\"),
            .esc_raw = str(L"\\"),
            .flags = 0
        },
        (syntaxer_neutralizer){
            .begin = str(L"\'"),
            .end = str(L"\'"),
            .esc_char = str(L"\\"),
            .esc_raw = str(L"\\"),
            .flags = 0
        },
        (syntaxer_neutralizer){
            .begin = str(L"`"),
            .end = str(L"`"),
            .esc_char = str(L"\\"),
            .esc_raw = str(L"\\"),
            .flags = SYNTXF_DEF_N_ML
        }
    };

    //charw *s = str(L"\"}'\\\"\"{{}} '}' `\n}` .\"\n}\" ;}123456");
    charw *s = read_file("syntx_test.js");

    syntaxer_sort(neutralizers, 3);

    size_t n = syntaxer_select_until(neutralizers, 3, s, L"{", L"}", 50);
    printf("n: %zu\n", n);
    s[n] = L'\0';
    printf("|%ls|\n", s);
}