#ifndef NETIO_H
#define NETIO_H
#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <curl/curl.h>

#include <str.h>

string read_file_network(const char* url);

#ifdef __cplusplus
}
#endif
#endif