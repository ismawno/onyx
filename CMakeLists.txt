cmake_minimum_required(VERSION 3.16...3.31)
project(onyx-root)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE
    "Debug"
    CACHE STRING "")

# If demos are built, implot will always be used
set(ONYX_BUILD_DEMOS
    OFF
    CACHE BOOL "")
set(ONYX_ENABLE_IMGUI
    OFF
    CACHE BOOL "")
set(ONYX_ENABLE_IMPLOT
    OFF
    CACHE BOOL "")
set(ONYX_ENABLE_NFD
    OFF
    CACHE BOOL "")
set(ONYX_ENABLE_OBJ
    OFF
    CACHE BOOL "")
set(ONYX_ENABLE_SHADER_COMPILATION
    OFF
    CACHE BOOL "")

set(ONYX_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(ONYX_ENABLE_IMPLOT AND NOT ONYX_ENABLE_IMGUI)
  message(ERROR "ONYX - ImGui is required to use ImPlot. Enable ImGui as well")
endif()

message(STATUS "ONYX - Build demos: ${ONYX_BUILD_DEMOS}")
message(STATUS "ONYX - Use ImGui: ${ONYX_ENABLE_IMGUI}")
message(STATUS "ONYX - Use ImPlot: ${ONYX_ENABLE_IMPLOT}")
message(STATUS "ONYX - NFD: ${ONYX_ENABLE_NFD}")
message(STATUS "ONYX - OBJ: ${ONYX_ENABLE_OBJ}")

# Compiler flags, linker flags etcetera, those should all be handled when making
# available the toolkit library through the global CMAKE flag variables
add_subdirectory(onyx)

if(ONYX_BUILD_DEMOS)
  add_subdirectory(demo)
endif()
