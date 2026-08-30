cmake_minimum_required(VERSION 3.16)
project(onyx-engine)

set(ENGINE_SOURCES engine/engine.cpp)
set(EDITOR_SOURCES engine/main.cpp engine/editor.cpp)

add_library(onyx-engine STATIC ${ENGINE_SOURCES})
add_executable(onyx-editor ${EDITOR_SOURCES})

tkit_default_configure(onyx-engine)
tkit_default_configure(onyx-editor)

target_link_libraries(onyx-engine PUBLIC onyx)
target_link_libraries(onyx-editor PRIVATE onyx-engine)
