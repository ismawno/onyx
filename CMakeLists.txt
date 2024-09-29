cmake_minimum_required(VERSION 3.16)
project(onyx-root)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist" CACHE STRING "Available build types" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
endif()

set(TOOLKIT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "Root path to be used" FORCE) # Override the root path
set(TOOLKIT_BUILD_TESTS OFF CACHE BOOL "Build toolkit tests" FORCE)
set(TOOLKIT_BUILD_PERFORMANCE OFF CACHE BOOL "Build toolkit performance tests" FORCE)

if (CMAKE_BUILD_TYPE STREQUAL "Dist")
    option(ONYX_BUILD_TESTS "Build the tests" OFF)
    option(ONYX_BUILD_DEMO "Build the demo" OFF)
else()
    option(ONYX_BUILD_TESTS "Build the tests" ON)
    option(ONYX_BUILD_DEMO "Build the demo" ON)
endif()

option(ONYX_USE_IMPLOT "Use ImPlot for plotting" OFF)
option(ONYX_USE_IMGUIZMO "Use ImGuizmo for gizmos" OFF)

# Compiler flags, linker flags etcetera, those should all be handled when making available the toolkit library through the global CMAKE flag variables
add_subdirectory(onyx)

# To be implemented
# if (ONYX_BUILD_TESTS)
#     add_subdirectory(tests)
# endif()

if (ONYX_BUILD_DEMO)
    add_subdirectory(demo)
endif()