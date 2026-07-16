cmake_minimum_required(VERSION 3.28)

project(libhtmw)

set(CMAKE_C_STANDARD           99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS        OFF)

add_library(${PROJECT_NAME} STATIC src/htmw.c)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/lua-5.5.0/install/include
)

add_subdirectory(luax)
add_subdirectory(curl)

include_directories(include luax/include curl/include)

target_link_libraries(${PROJECT_NAME} luax)
target_link_libraries(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/lua/lua-5.5.0/install/lib/liblua.a m)