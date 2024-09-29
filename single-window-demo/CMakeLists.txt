cmake_minimum_required(VERSION 3.16)
project(onyx-sw-demo)

set(SOURCES
    src/layer.cpp
    src/app.cpp
    src/main.cpp
)

add_executable(onyx-sw-demo ${SOURCES})
if (MSVC)
    configure_compile_link_options(onyx-sw-demo "/W4 /WX")
else()
    configure_compile_link_options(onyx-sw-demo "-Wall -Wextra -Wpedantic -Werror")
endif()

target_link_libraries(onyx-sw-demo PRIVATE onyx)
target_include_directories(onyx-sw-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)