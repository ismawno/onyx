cmake_minimum_required(VERSION 3.16)
project(onyx-tests)

include(FetchContent)
find_package(Threads REQUIRED)

# the same Catch2 Toolkit and vulkit test with, built the same way: without exceptions or RTTI, like Onyx. a failed
# REQUIRE then ends the process, which suits onyx-tests: tests run in worker processes the runner can lose
# (framework/worker.cpp)
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
  message(STATUS "ONYX - Catch2 not found. Fetching source...")
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
  FetchContent_MakeAvailable(Catch2)
  tkit_configure_compile_link_options(Catch2 NO_EXCEPTIONS NO_RTTI)
endif()

# suites register their tests at static initialization, so they are compiled straight into the executable: a static
# library would let the linker drop them
file(GLOB ONYX_TEST_SUITES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/suites/*.cpp)
file(GLOB ONYX_TEST_FRAMEWORK CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/framework/*.cpp)

add_executable(onyx-tests ${ONYX_TEST_FRAMEWORK} ${ONYX_TEST_SUITES})
tkit_default_configure(onyx-tests)

target_include_directories(onyx-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/framework)
target_link_libraries(onyx-tests PRIVATE onyx Catch2::Catch2 Threads::Threads)
if(MSVC)
  # getenv, fopen and friends: the runner reads CI variables and pipes from its workers
  target_compile_definitions(onyx-tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# `ctest` runs the whole suite through the runner, which already keeps tests in worker processes of their own
add_test(NAME onyx-tests COMMAND onyx-tests)
