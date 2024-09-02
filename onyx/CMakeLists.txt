cmake_minimum_required(VERSION 3.16)
project(onyx)

set(SOURCES
    src/core/pch.cpp
    src/core/core.cpp
    src/core/device.cpp
    src/core/instance.cpp
    src/rendering/pipeline.cpp
    src/rendering/swap_chain.cpp
    src/rendering/renderer.cpp
    src/drawing/vertex.cpp
    src/drawing/color.cpp
    src/app/app.cpp
    src/app/window.cpp
    src/app/input.cpp
)

add_library(onyx STATIC ${SOURCES})

include(FetchContent)
FetchContent_Declare(
    toolkit
    GIT_REPOSITORY https://github.com/ismawno/toolkit.git
    GIT_TAG main
)
FetchContent_MakeAvailable(toolkit)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# It is set to required for now even though there is a fallback mechanism with FetchContent (the latter seems to provide very few extensions and I am not sure if it will work well for now)
# TODO: Check if the fallback mechanism works well
find_package(Vulkan REQUIRED)

if (NOT Vulkan_FOUND)
    message(STATUS "ONYX: A global Vulkan installation was not found. Fetching the Vulkan SDK...")
    set(ONYX_LOCAL_VULKAN_SDK ON CACHE BOOL "Use the local Vulkan SDK" FORCE)

    FetchContent_Declare(
        VulkanHeaders
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG        sdk-1.3.250.1  # Use the appropriate version tag
    )
    FetchContent_MakeAvailable(VulkanHeaders)

    # Fetch Vulkan-Loader
    FetchContent_Declare(
        VulkanLoader
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Loader.git
        GIT_TAG        sdk-1.3.250.1  # Use the appropriate version tag
    )
    FetchContent_MakeAvailable(VulkanLoader)
else()
    message(STATUS "ONYX: Vulkan SDK found")
endif()

find_program(GLSLC_EXECUTABLE glslc REQUIRED)
message(STATUS "ONYX: Found glslc: ${GLSLC_EXECUTABLE}")

function(compile_shader shader)
    set(SHADER "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}")
    set(SPIRV "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bin/${shader}.spv")
    
    add_custom_command(
        TARGET onyx
        PRE_BUILD
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV}
        COMMENT "Compiling ${shader} to SPIR-V"
        VERBATIM
    )
endfunction()

set(SHADERS
    "shader2D.frag"
    "shader2D.vert"
    "shader3D.frag"
    "shader3D.vert"
)

foreach(SHADER ${SHADERS})
    compile_shader(${SHADER})
endforeach()

# TODO: Links against vulkan and glfw should be private

if (ONYX_LOCAL_VULKAN_SDK)
    target_include_directories(onyx PUBLIC ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(onyx PUBLIC ${Vulkan_LIBRARIES} Vulkan::Headers Vulkan::Vulkan)
endif()

target_include_directories(onyx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(onyx PUBLIC toolkit glm glfw Vulkan::Vulkan)

target_precompile_headers(onyx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/pch.hpp)

if (MSVC)
    configure_compile_link_options(onyx "/W4 /WX")
else()
    configure_compile_link_options(onyx "-Wall -Wextra -Wpedantic -Werror")
endif()

# No need for more: toolkit handles the rest. As it is a library I develop, I will be reusing its macros/configurations