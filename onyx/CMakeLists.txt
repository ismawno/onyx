cmake_minimum_required(VERSION 3.16)
project(onyx)

set(SOURCES
    src/core/pch.cpp
    src/core/core.cpp
    src/core/device.cpp
    src/core/instance.cpp
    src/buffer/buffer.cpp
    src/buffer/vertex_buffer.cpp
    src/buffer/index_buffer.cpp
    src/rendering/pipeline.cpp
    src/rendering/swap_chain.cpp
    src/rendering/render_context.cpp
    src/rendering/render_systems.cpp
    src/rendering/renderer.cpp
    src/rendering/frame_scheduler.cpp
    src/descriptors/descriptor_pool.cpp
    src/descriptors/descriptor_set_layout.cpp
    src/descriptors/descriptor_writer.cpp
    src/draw/vertex.cpp
    src/draw/color.cpp
    src/draw/transform.cpp
    src/draw/primitives.cpp
    src/draw/data.cpp
    src/draw/model.cpp
    src/app/app.cpp
    src/app/mwapp.cpp
    src/app/window.cpp
    src/app/input.cpp
    src/app/layer.cpp
    src/app/theme.cpp
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

FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.1.0
)
FetchContent_MakeAvailable(vma)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.1-docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    tobjl
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
)
FetchContent_MakeAvailable(tobjl)

# It is set to required for now even though there is a fallback mechanism with FetchContent (the latter seems to provide very few extensions and I am not sure if it will work well for now)
# TODO: Check if the fallback mechanism works well
find_package(Vulkan REQUIRED)

if (NOT Vulkan_FOUND)
    message(STATUS "ONYX: A global Vulkan installation was not found. Fetching the Vulkan SDK...")
    message(STATUS "ONYX: Manually fetching the Vulkan SDK may not provide the necessary extensions. A global installation is recommended")
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
    get_filename_component(SHADER_NAME ${shader} NAME)
    set(SPIRV "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bin/${SHADER_NAME}.spv")
    
    make_directory(${CMAKE_CURRENT_SOURCE_DIR}/shaders/bin)

    add_custom_command(
        TARGET onyx
        PRE_BUILD
        COMMAND ${GLSLC_EXECUTABLE} ${shader} -o ${SPIRV}
        COMMENT "Compiling ${shader} to SPIR-V"
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bin
        VERBATIM
    )
endfunction()

file(GLOB SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")

foreach(SHADER ${SHADERS})
    compile_shader(${SHADER})
endforeach()

if (ONYX_LOCAL_VULKAN_SDK)
    target_include_directories(onyx PUBLIC ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(onyx PUBLIC ${Vulkan_LIBRARIES} Vulkan::Headers Vulkan::Vulkan)
endif()

### IMGUI ###

set(IMGUI_DIR ${CMAKE_BINARY_DIR}/_deps/imgui-src)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC ${IMGUI_DIR})
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)

### IMPLOT ###

if (ONYX_USE_IMPLOT OR ONYX_BUILD_DEMO)
    FetchContent_Declare(
        implot
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG v0.16
    )
    FetchContent_MakeAvailable(implot)

    set(IMPLOT_DIR ${CMAKE_BINARY_DIR}/_deps/implot-src)

    set(IMPLOT_SOURCES
        ${IMPLOT_DIR}/implot.cpp
        ${IMPLOT_DIR}/implot_items.cpp
        ${IMPLOT_DIR}/implot_demo.cpp
    )

    add_library(implot STATIC ${IMPLOT_SOURCES})

    target_compile_definitions(implot PUBLIC ONYX_ENABLE_IMPLOT)
    target_include_directories(implot PUBLIC ${IMPLOT_DIR})
    target_link_libraries(implot PUBLIC imgui)
    target_link_libraries(onyx PUBLIC implot)

endif()

target_include_directories(onyx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Vulkan and GLFW are already linked with imgui
target_link_libraries(onyx PUBLIC toolkit glm imgui VulkanMemoryAllocator PRIVATE tinyobjloader)

target_precompile_headers(onyx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/pch.hpp)
target_compile_definitions(onyx PUBLIC KIT_ROOT_LOG_PATH="${ONYX_ROOT_PATH}" ONYX_ROOT_PATH="${ONYX_ROOT_PATH}")

if (MSVC)
    configure_compile_link_options(onyx "/W4 /WX")
else()
    configure_compile_link_options(onyx "-Wall -Wextra -Wpedantic -Werror")
endif()

# No need for more: toolkit handles the rest. As it is a library I develop, I will be reusing its macros/configurations