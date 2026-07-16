#include "htmw.h"

#include <stdio.h>

#include <lua.h>
#include <curl/curl.h>

int test(int x) {
    printf("hello world (%d)\n", x);
    return x + 1;
}