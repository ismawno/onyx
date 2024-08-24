cmake_minimum_required(VERSION 3.16)
project(onyx-demo)

set(SOURCES
    src/main.cpp
)

add_executable(onyx-demo ${SOURCES})

target_link_libraries(onyx-demo PRIVATE onyx)
target_include_directories(onyx-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)