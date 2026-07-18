cmake_minimum_required(VERSION 3.28)
project(htmw)

set(CMAKE_C_STANDARD           99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS        ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME} src/main.c)

add_subdirectory(libhtmw)

target_link_libraries(${PROJECT_NAME} libhtmw luax)

include_directories(libhtmw/include include libhtmw/luax/include libhtmw/curl/include)

# to enablde debug please run cmake like this
# `cmake -DCMAKE_BUILD_TYPE=Debug ..`