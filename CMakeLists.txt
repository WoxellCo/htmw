cmake_minimum_required(VERSION 3.28)
project(htmw)

set(CMAKE_C_STANDARD           99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS        OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME} src/main.c)

target_link_libraries(${PROJECT_NAME} libhtmw)
add_subdirectory(libhtmw)


include_directories( libhtmw/include include)