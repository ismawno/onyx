cmake_minimum_required(VERSION 3.16)
project(onyx-mw-demo)

set(SOURCES
    mwdemo/layer.cpp
    mwdemo/main.cpp
)

add_executable(onyx-mw-demo ${SOURCES})
if (MSVC)
    tkit_configure_compile_link_options(onyx-mw-demo FLAGS /W4 /WX)
else()
    tkit_configure_compile_link_options(onyx-mw-demo FLAGS -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(onyx-mw-demo PRIVATE onyx-demo-utils)
target_include_directories(onyx-mw-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (APPLE)
    vkit_add_vulkan_executable_macos_rpath(onyx-mw-demo)
endif()