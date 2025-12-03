cmake_minimum_required(VERSION 3.16)
project(onyx-sandbox)

set(SOURCES sbox/shapes.cpp sbox/argparse.cpp sbox/layer.cpp sbox/main.cpp)

add_executable(onyx-sandbox ${SOURCES})

tkit_default_configure(onyx-sandbox)

FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/ismawno/argparse.git
  GIT_TAG v3.2.x
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(argparse)

target_link_libraries(onyx-sandbox PUBLIC onyx)
target_include_directories(onyx-sandbox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                               ${argparse_SOURCE_DIR}/include)
