cmake_minimum_required(VERSION 3.16)
project(onyx)

set(SOURCES
    onyx/core/pch.cpp
    onyx/core/core.cpp
    onyx/core/shaders.cpp
    onyx/rendering/camera.cpp
    onyx/rendering/render_context.cpp
    onyx/rendering/render_systems.cpp
    onyx/rendering/renderer.cpp
    onyx/rendering/frame_scheduler.cpp
    onyx/rendering/post_processing.cpp
    onyx/data/state.cpp
    onyx/data/buffers.cpp
    onyx/property/vertex.cpp
    onyx/property/color.cpp
    onyx/property/transform.cpp
    onyx/object/primitives.cpp
    onyx/object/mesh.cpp
    onyx/app/app.cpp
    onyx/app/window.cpp
    onyx/app/input.cpp
    onyx/app/user_layer.cpp
    onyx/app/theme.cpp)

add_library(onyx STATIC ${SOURCES})
target_compile_definitions(onyx PUBLIC ONYX_VERSION=\"v0.2.2\")

if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FONTCONFIG REQUIRED fontconfig)

  target_include_directories(onyx PRIVATE ${FONTCONFIG_INCLUDE_DIRS})
  target_link_libraries(onyx PRIVATE ${FONTCONFIG_LIBRARIES})
endif()

include(FetchContent)
FetchContent_Declare(
  vulkit
  GIT_REPOSITORY https://github.com/ismawno/vulkit.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(vulkit)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(glm)

set(GLFW_BUILD_DOCS
    OFF
    CACHE BOOL "" FORCE)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.1-docking
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
  tobjl
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG v2.0.0rc13
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(tobjl)

tkit_register_for_yaml_serialization(
  onyx
  SOURCES
  onyx/property/transform.hpp
  onyx/property/vertex.hpp
  onyx/data/state.hpp
  onyx/data/options.hpp
  onyx/rendering/camera.hpp
  onyx/object/primitives.hpp)

tkit_register_for_reflection(
  onyx
  SOURCES
  onyx/property/transform.hpp
  onyx/property/vertex.hpp
  onyx/data/state.hpp
  onyx/data/options.hpp
  onyx/rendering/camera.hpp)

# IMGUI ###

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    onyx/vendor/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)

find_package(Vulkan REQUIRED)
add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR}
                                        ${Vulkan_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC glfw)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

# IMPLOT ###

if(ONYX_USE_IMPLOT OR ONYX_BUILD_DEMOS)
  FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.16
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

  FetchContent_MakeAvailable(implot)

  set(IMPLOT_SOURCES
      ${implot_SOURCE_DIR}/implot.cpp ${implot_SOURCE_DIR}/implot_items.cpp
      ${implot_SOURCE_DIR}/implot_demo.cpp)

  add_library(implot STATIC ${IMPLOT_SOURCES})

  target_compile_definitions(implot PUBLIC ONYX_ENABLE_IMPLOT)
  target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
  target_link_libraries(implot PUBLIC imgui)
  target_link_libraries(onyx PUBLIC implot)

endif()

target_include_directories(onyx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Vulkan and GLFW are already linked with imgui
target_link_libraries(
  onyx
  PUBLIC toolkit vulkit glm imgui
  PRIVATE tinyobjloader)

target_precompile_headers(onyx PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}/onyx/core/pch.hpp)
target_compile_definitions(onyx PUBLIC ONYX_ROOT_PATH="${ONYX_ROOT_PATH}")

tkit_default_configure(onyx)

tkit_configure_compile_link_options(glm FORBID_SANITIZERS)
tkit_configure_compile_link_options(glfw FORBID_SANITIZERS)
tkit_configure_compile_link_options(imgui FORBID_SANITIZERS)
tkit_configure_compile_link_options(tinyobjloader FORBID_SANITIZERS)

# No need for more: toolkit handles the rest. As it is a library I develop, I
# will be reusing its macros/configurations
