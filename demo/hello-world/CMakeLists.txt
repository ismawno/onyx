cmake_minimum_required(VERSION 3.16)
project(onyx-hello-world)

set(SOURCES main.cpp)

add_executable(onyx-hello-world ${SOURCES})

tkit_default_configure(onyx-hello-world)

target_link_libraries(onyx-hello-world PRIVATE onyx)
