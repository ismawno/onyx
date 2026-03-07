cmake_minimum_required(VERSION 3.16)
project(onyx-sandbox)

set(SOURCES sandbox/sandbox.cpp sandbox/main.cpp sandbox/argparse.cpp)

add_executable(onyx-sandbox ${SOURCES})

tkit_default_configure(onyx-sandbox)
tkit_register_for_yaml_serialization(onyx-sandbox SOURCES sandbox/sandbox.hpp)

FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/ismawno/argparse.git
  GIT_TAG v3.2.0
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(argparse)

target_link_libraries(
  onyx-sandbox
  PUBLIC onyx
  PRIVATE argparse)
target_include_directories(onyx-sandbox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
