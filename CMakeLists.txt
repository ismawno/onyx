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

# TODO(Isma): Add some strs to variables
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
set(ONYX_ENABLE_OBJ_LOAD
    OFF
    CACHE BOOL "")
set(ONYX_ENABLE_IMAGE_LOAD
    OFF
    CACHE BOOL "")
set(ONYX_ENABLE_GLTF_LOAD
    OFF
    CACHE BOOL "")
set(ONYX_ENABLE_FONT_LOAD
    OFF
    CACHE BOOL "")
# right now for these two, former doesnt make sense without latter. but its left
# as is bc the shader api will be exposed in the future
set(ONYX_ENABLE_SHADER_API
    OFF
    CACHE BOOL "")
set(ONYX_COMPILE_SHADERS_ON_EXEC
    OFF
    CACHE BOOL "")
#
set(ONYX_INCLUDE_DEFAULT_FONT
    OFF
    CACHE BOOL "")

if(ONYX_ENABLE_IMPLOT AND NOT ONYX_ENABLE_IMGUI)
  message(ERROR "ONYX - ImGui is required to use ImPlot. Enable ImGui as well")
endif()
if(ONYX_INCLUDE_DEFAULT_FONT AND NOT ONYX_ENABLE_FONT_LOAD)
  message(
    ERROR
    "ONYX - Font loading is required to use the default font. Enable font loading as well"
  )
endif()
if(ONYX_COMPILE_SHADERS_ON_EXEC AND NOT ONYX_ENABLE_SHADER_API)
  message(
    ERROR
    "ONYX - Shader API is required to load shaders on execution. Enable the shader api as well"
  )
endif()

message(STATUS "ONYX - BUILD DEMOS: ${ONYX_BUILD_DEMOS}")
message(STATUS "ONYX - USE IMGUI: ${ONYX_ENABLE_IMGUI}")
message(STATUS "ONYX - USE IMPLOT: ${ONYX_ENABLE_IMPLOT}")
message(STATUS "ONYX - NFD: ${ONYX_ENABLE_NFD}")
message(STATUS "ONYX - OBJ LOAD: ${ONYX_ENABLE_OBJ_LOAD}")
message(STATUS "ONYX - IMAGE LOAD: ${ONYX_ENABLE_IMAGE_LOAD}")
message(STATUS "ONYX - FONT LOAD: ${ONYX_ENABLE_FONT_LOAD}")
message(STATUS "ONYX - DEFAULT FONT: ${ONYX_INCLUDE_DEFAULT_FONT}")
message(STATUS "ONYX - GLTF LOAD: ${ONYX_ENABLE_GLTF_LOAD}")
message(STATUS "ONYX - COMPILE SHADERS: ${ONYX_COMPILE_SHADERS_ON_EXEC}")
message(STATUS "ONYX - SHADER API: ${ONYX_ENABLE_SHADER_API}")

# Compiler flags, linker flags etcetera, those should all be handled when making
# available the toolkit library through the global CMAKE flag variables
add_subdirectory(onyx)

if(ONYX_BUILD_DEMOS)
  add_subdirectory(demo)
endif()
