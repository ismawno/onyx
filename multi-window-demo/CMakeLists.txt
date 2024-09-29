cmake_minimum_required(VERSION 3.16)
project(onyx-mw-demo)

set(SOURCES
    src/layer.cpp
    src/app.cpp
    src/main.cpp
)

add_executable(onyx-mw-demo ${SOURCES})
if (MSVC)
    configure_compile_link_options(onyx-mw-demo "/W4 /WX")
else()
    configure_compile_link_options(onyx-mw-demo "-Wall -Wextra -Wpedantic -Werror")
endif()

target_link_libraries(onyx-mw-demo PRIVATE onyx)
target_include_directories(onyx-mw-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)