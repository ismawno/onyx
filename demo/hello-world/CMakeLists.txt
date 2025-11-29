cmake_minimum_required(VERSION 3.16)
project(onyx-hello-world)

set(SOURCES main.cpp)

add_executable(hello-world ${SOURCES})

tkit_default_configure(hello-world)

target_link_libraries(hello-world PRIVATE onyx)
