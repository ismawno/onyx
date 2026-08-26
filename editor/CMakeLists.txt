cmake_minimum_required(VERSION 3.16)
project(onyx-editor)

set(SOURCES editor/main.cpp editor/editor.cpp editor/components.cpp
            editor/scene.cpp)

add_executable(onyx-editor ${SOURCES})
tkit_default_configure(onyx-editor)
target_link_libraries(onyx-editor PRIVATE onyx)
