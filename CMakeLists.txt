cmake_minimum_required(VERSION 3.16...3.31)
project(onyx-root)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(define_option varname default unused_opt_type)
  if(NOT DEFINED ${varname})
    set(${varname}
        ${default}
        PARENT_SCOPE)
  endif()
endfunction()

define_option(CMAKE_BUILD_TYPE "Debug" "Onyx - Build")

# If demos are built, implot will always be used
define_option(ONYX_BUILD_DEMOS OFF "Onyx - Build")
define_option(ONYX_ENABLE_IMGUI OFF "Onyx - Feature")
define_option(ONYX_ENABLE_IMPLOT OFF "Onyx - Feature")

set(ONYX_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(ONYX_ENABLE_IMPLOT AND NOT ONYX_ENABLE_IMGUI)
  message(ERROR "ONYX: ImGui is required to use ImPlot. Enable ImGui as well")
endif()

message(STATUS "ONYX: Build demos: ${ONYX_BUILD_DEMOS}")
message(STATUS "ONYX: Use ImGui: ${ONYX_ENABLE_IMGUI}")
message(STATUS "ONYX: Use ImPlot: ${ONYX_ENABLE_IMPLOT}")

# Compiler flags, linker flags etcetera, those should all be handled when making
# available the toolkit library through the global CMAKE flag variables
add_subdirectory(onyx)

if(ONYX_BUILD_DEMOS)
  add_subdirectory(demo-utils)
  add_subdirectory(multi-window-demo)
  add_subdirectory(single-window-demo)
  add_subdirectory(hello-world)
  add_subdirectory(performance)
endif()
