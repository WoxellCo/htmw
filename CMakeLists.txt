cmake_minimum_required(VERSION "3.19.2")

add_subdirectory("dependencies/curl-8.15.0")

project("htmw")
set(CMAKE_C_STANDARD 99)
SET(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_STANDARD_REQUIRED ON)




add_library(lua STATIC IMPORTED)
set_target_properties(lua PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lua/src/liblua.a
)

target_include_directories(lua INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lua/src
)

add_dependencies(lua lua_external)




set(CURL_DISABLE_LIBCURL_PSL ON)

set(CURL_DISABLE_LIBCURL_PSL ON CACHE BOOL "Disable libpsl dependency for curl")
set(CURL_DISABLE_LIBSSH ON CACHE BOOL "Disable libssh dependency for curl")
set(CURL_DISABLE_LIBSSH2 ON CACHE BOOL "Disable libssh2 dependency for curl")
set(CURL_DISABLE_OPENSSL ON CACHE BOOL "Disable OpenSSL dependency for curl") # Optional, but good for portability if you don't need it.

add_executable("htmw" "htmw.c" "map.c" "nxlist.c" "nxvector.c" "nxstack.c" "str.c" "syntaxer.c" "netio.c")
target_include_directories("htmw" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")

target_link_libraries(htmw PRIVATE libcurl lua m)