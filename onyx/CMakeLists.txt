cmake_minimum_required(VERSION 3.16)
project(onyx)

set(SOURCES
    onyx/core/pch.cpp
    onyx/core/core.cpp
    onyx/core/device.cpp
    onyx/core/instance.cpp
    onyx/buffer/buffer.cpp
    onyx/buffer/vertex_buffer.cpp
    onyx/buffer/index_buffer.cpp
    onyx/rendering/swap_chain.cpp
    onyx/rendering/render_context.cpp
    onyx/rendering/render_systems.cpp
    onyx/rendering/renderer.cpp
    onyx/rendering/render_specs.cpp
    onyx/rendering/frame_scheduler.cpp
    onyx/pipeline/graphics_pipeline.cpp
    onyx/pipeline/shader.cpp
    onyx/descriptors/descriptor_pool.cpp
    onyx/descriptors/descriptor_set_layout.cpp
    onyx/descriptors/descriptor_writer.cpp
    onyx/draw/vertex.cpp
    onyx/draw/color.cpp
    onyx/draw/transform.cpp
    onyx/draw/primitives.cpp
    onyx/draw/data.cpp
    onyx/draw/model.cpp
    onyx/app/app.cpp
    onyx/app/mwapp.cpp
    onyx/app/window.cpp
    onyx/app/input.cpp
    onyx/app/layer.cpp
    onyx/app/theme.cpp
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
message(STATUS "ONYX: Vulkan SDK found")

find_program(GLSLC_EXECUTABLE glslc REQUIRED)
message(STATUS "ONYX: Found glslc: ${GLSLC_EXECUTABLE}")

### IMGUI ###


set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)

### IMPLOT ###

if (ONYX_USE_IMPLOT OR ONYX_BUILD_DEMOS)
    FetchContent_Declare(
        implot
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG v0.16
    )
    FetchContent_MakeAvailable(implot)

    set(IMPLOT_SOURCES
        ${implot_SOURCE_DIR}/implot.cpp
        ${implot_SOURCE_DIR}/implot_items.cpp
        ${implot_SOURCE_DIR}/implot_demo.cpp
    )

    add_library(implot STATIC ${IMPLOT_SOURCES})

    target_compile_definitions(implot PUBLIC ONYX_ENABLE_IMPLOT)
    target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
    target_link_libraries(implot PUBLIC imgui)
    target_link_libraries(onyx PUBLIC implot)

endif()

target_include_directories(onyx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Vulkan and GLFW are already linked with imgui
target_link_libraries(onyx PUBLIC toolkit glm imgui VulkanMemoryAllocator PRIVATE tinyobjloader)

target_precompile_headers(onyx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/onyx/core/pch.hpp)
target_compile_definitions(onyx PUBLIC ONYX_ROOT_PATH="${ONYX_ROOT_PATH}" ONYX_GLSL_BINARY="${GLSLC_EXECUTABLE}")

if (MSVC)
    configure_compile_link_options(onyx FLAGS /W4 /WX)
else()
    configure_compile_link_options(onyx FLAGS -Wall -Wextra -Wpedantic -Werror)
endif()

configure_compile_link_options(glm FORBID_SANITIZERS)
configure_compile_link_options(glfw FORBID_SANITIZERS)
configure_compile_link_options(imgui FORBID_SANITIZERS)
configure_compile_link_options(tinyobjloader FORBID_SANITIZERS)

# No need for more: toolkit handles the rest. As it is a library I develop, I will be reusing its macros/configurations