cmake_minimum_required(VERSION 3.16)
project(onyx-single-window)

set(SOURCES app/layer.cpp app/main.cpp)

add_executable(onyx-single-window ${SOURCES})

tkit_default_configure(onyx-single-window)

target_link_libraries(onyx-single-window PRIVATE onyx-demo)
target_include_directories(onyx-single-window
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
