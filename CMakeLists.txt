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

set(ONYX_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_BUILD_TYPE STREQUAL "Dist")
    option(ONYX_BUILD_DEMOS "Build the demos" OFF)
else()
    option(ONYX_BUILD_DEMOS "Build the demos" ON)
endif()

message(STATUS "ONYX: Build demos: ${ONYX_BUILD_DEMOS}")

# If demos are built, implot will always be used
option(ONYX_USE_IMPLOT "Use ImPlot for plotting" OFF)

# Compiler flags, linker flags etcetera, those should all be handled when making available the toolkit library through the global CMAKE flag variables
add_subdirectory(onyx)

if (ONYX_BUILD_DEMOS)
    add_subdirectory(demo-utils)
    add_subdirectory(multi-window-demo)
    add_subdirectory(single-window-demo)
    add_subdirectory(hello-world)
endif()