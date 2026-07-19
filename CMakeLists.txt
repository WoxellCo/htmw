cmake_minimum_required(VERSION 3.28)
project(htmw)

set(CMAKE_C_STANDARD           99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS        ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME} src/main.c)

add_subdirectory(libhtmw)

target_link_libraries(${PROJECT_NAME} libhtmw luax)


if (WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        include_directories(libhtmw/include include libhtmw/luax/include libhtmw/curl/include
            libhtmw/lua/win64_vc17/include
        )
    else()
        include_directories(libhtmw/include include libhtmw/luax/include libhtmw/curl/include
            libhtmw/lua/win32_vc17/include
        )
    endif()
else()
    include_directories(libhtmw/include include libhtmw/luax/include libhtmw/curl/include)
endif()

# to enablde debug please run cmake like this
# `cmake -DCMAKE_BUILD_TYPE=Debug ..`

# to build on windows use the following commands (uses msvc)
# cmake: `cmake -DCURL_USE_LIBPSL=OFF ..`
# build: `cmake --build .`
# note: it would be nice if there's a way to not remove `libspl`, apparently the library is js missing or idk