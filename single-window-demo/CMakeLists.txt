cmake_minimum_required(VERSION 3.16)
project(onyx-sw-demo)

set(SOURCES swdemo/layer.cpp swdemo/main.cpp)

add_executable(onyx-sw-demo ${SOURCES})

tkit_default_configure(onyx-sw-demo)

target_link_libraries(onyx-sw-demo PRIVATE onyx-demo-utils)
target_include_directories(onyx-sw-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(UNIX AND APPLE)
  vkit_add_vulkan_executable_macos_rpath(onyx-sw-demo)
endif()
