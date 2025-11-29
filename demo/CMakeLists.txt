cmake_minimum_required(VERSION 3.16)
project(onyx-demo)

set(SOURCES utils/shapes.cpp utils/window_data.cpp utils/argparse.cpp)

add_library(onyx-demo STATIC ${SOURCES})

tkit_default_configure(onyx-demo)

FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/ismawno/argparse.git
  GIT_TAG v3.2.x
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(argparse)

target_link_libraries(onyx-demo PUBLIC onyx)
target_include_directories(onyx-demo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                            ${argparse_SOURCE_DIR}/include)

add_subdirectory(multi-window)
add_subdirectory(single-window)
add_subdirectory(hello-world)
add_subdirectory(performance)
