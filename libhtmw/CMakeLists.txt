cmake_minimum_required(VERSION 3.28)

project(libhtmw)

set(CMAKE_C_STANDARD           99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS         ON)

add_library(${PROJECT_NAME} STATIC src/htmw.c
    "src/test.c"
    src/str.c src/map.c
    src/netio.c src/strplex.c src/syntaxer.c src/luaw.c
    src/htmw_syntax_notation.c src/htmw_syntax_dir.c src/htmw_tag.c src/htmw_template.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/lua-5.5.0/install/include
)

add_subdirectory(luax)
add_subdirectory(curl)

include_directories(include luax/include curl/include
    lua/win64_vc17/include
)

if (WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_libraries(${PROJECT_NAME} luax libcurl
            ${CMAKE_CURRENT_SOURCE_DIR}/lua/win64_vc17/lua55.lib
        )
    else()
        target_link_libraries(${PROJECT_NAME} luax libcurl
            ${CMAKE_CURRENT_SOURCE_DIR}/lua/win32_vc17/lua55.lib
        )
    endif()
else()
    target_link_libraries(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/lua/lua-5.5.0/install/lib/liblua.a m luax libcurl)
endif()

#target_link_libraries(${PROJECT_NAME} PRIVATE lua)