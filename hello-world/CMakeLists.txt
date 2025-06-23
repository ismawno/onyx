cmake_minimum_required(VERSION 3.16)
project(hello-world)

set(SOURCES main.cpp)

add_executable(hello-world ${SOURCES})

tkit_default_configure(hello-world)

target_link_libraries(hello-world PRIVATE onyx)

if(UNIX AND APPLE)
  vkit_add_vulkan_executable_macos_rpath(hello-world)
endif()
