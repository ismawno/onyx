cmake_minimum_required(VERSION 3.16)
project(onyx-sw-demo)

set(SOURCES swdemo/layer.cpp swdemo/main.cpp)

add_executable(onyx-sw-demo ${SOURCES})
if(MSVC)
  tkit_configure_compile_link_options(onyx-sw-demo FLAGS /W4 /WX)
else()
  tkit_configure_compile_link_options(onyx-sw-demo FLAGS -Wall -Wextra
                                      -Wpedantic -Werror)
endif()

target_link_libraries(onyx-sw-demo PRIVATE onyx-demo-utils)
target_include_directories(onyx-sw-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

vkit_add_vulkan_executable_macos_rpath(onyx-sw-demo)
