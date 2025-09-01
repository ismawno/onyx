cmake_minimum_required(VERSION 3.16)
project(onyx-sw-demo)

set(SOURCES swdemo/layer.cpp swdemo/main.cpp)

add_executable(onyx-sw-demo ${SOURCES})

if(MSVC)
  tkit_configure_compile_link_options(onyx-sw-demo FLAGS /W4 /WX)
elseif(UNIX AND NOT APPLE)
  tkit_configure_compile_link_options(
    onyx-sw-demo
    FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-changes-meaning)
else()
  tkit_configure_compile_link_options(onyx-sw-demo FLAGS -Wall -Wextra
                                      -Wpedantic -Werror)
endif()

if(APPLE)
  vkit_add_vulkan_executable_macos_rpath(onyx-sw-demo)
endif()

target_link_libraries(onyx-sw-demo PRIVATE onyx-demo-utils)
target_include_directories(onyx-sw-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
