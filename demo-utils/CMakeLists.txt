cmake_minimum_required(VERSION 3.16)
project(onyx-demo-utils)

set(SOURCES utils/shapes.cpp utils/window_data.cpp)

add_library(onyx-demo-utils STATIC ${SOURCES})
if(MSVC)
  tkit_configure_compile_link_options(onyx-demo-utils FLAGS /W4 /WX)
else()
  tkit_configure_compile_link_options(onyx-demo-utils FLAGS -Wall -Wextra
                                      -Wpedantic -Werror)
endif()

target_link_libraries(onyx-demo-utils PUBLIC onyx)
target_include_directories(onyx-demo-utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
