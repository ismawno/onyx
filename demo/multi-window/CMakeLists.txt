cmake_minimum_required(VERSION 3.16)
project(onyx-multi-window)

set(SOURCES app/layer.cpp app/main.cpp)

add_executable(onyx-multi-window ${SOURCES})

tkit_default_configure(onyx-multi-window)

target_link_libraries(onyx-multi-window PRIVATE onyx-demo)
target_include_directories(onyx-multi-window
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(UNIX AND APPLE)
  vkit_add_vulkan_executable_macos_rpath(onyx-multi-window)
endif()
