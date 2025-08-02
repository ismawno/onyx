cmake_minimum_required(VERSION 3.16)
project(onyx-performance)

set(SOURCES perf/main.cpp perf/lattice.cpp perf/layer.cpp perf/argparse.cpp)

add_executable(onyx-performance ${SOURCES})

tkit_default_configure(onyx-performance)
tkit_register_for_yaml_serialization(onyx-performance SOURCES perf/lattice.hpp)

FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.2
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(argparse)

target_link_libraries(onyx-performance PRIVATE onyx)
target_include_directories(
  onyx-performance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                           ${argparse_SOURCE_DIR}/include)

if(UNIX AND APPLE)
  vkit_add_vulkan_executable_macos_rpath(onyx-performance)
endif()
