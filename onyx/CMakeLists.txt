cmake_minimum_required(VERSION 3.16)
project(onyx)

set(SOURCES
    onyx/core/pch.cpp
    onyx/core/core.cpp
    onyx/rendering/camera.cpp
    onyx/rendering/render_context.cpp
    onyx/rendering/render_systems.cpp
    onyx/rendering/renderer.cpp
    onyx/rendering/frame_scheduler.cpp
    onyx/rendering/post_processing.cpp
    onyx/resource/sync.cpp
    onyx/resource/buffer.cpp
    onyx/resource/assets.cpp
    onyx/resource/pipelines.cpp
    onyx/property/vertex.cpp
    onyx/property/color.cpp
    onyx/property/transform.cpp
    onyx/app/app.cpp
    onyx/app/window.cpp
    onyx/app/input.cpp)

if(ONYX_ENABLE_IMGUI)
  list(APPEND SOURCES onyx/app/user_layer.cpp onyx/imgui/theme.cpp
       onyx/imgui/backend.cpp)
endif()
if(ONYX_ENABLE_NFD)
  list(APPEND SOURCES onyx/core/dialog.cpp)
endif()

add_library(onyx STATIC ${SOURCES})
target_compile_definitions(onyx PUBLIC ONYX_VERSION=\"v0.6.0\")

if(ONYX_ENABLE_VALIDATION_LAYERS)
  target_compile_definitions(onyx PUBLIC ONYX_ENABLE_VALIDATION_LAYERS)
endif()

include(FetchContent)
FetchContent_Declare(
  vulkit
  GIT_REPOSITORY https://github.com/ismawno/vulkit.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(vulkit)

if(UNIX AND NOT APPLE)
  find_package(Fontconfig QUIET)
  if(Fontconfig_FOUND)
    message(STATUS "ONYX - Fontconfig found")
    target_link_libraries(onyx PRIVATE Fontconfig::Fontconfig)
    target_compile_definitions(onyx PUBLIC ONYX_FONTCONFIG)
  else()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(FONTCONFIG QUIET fontconfig)
      if(FONTCONFIG_FOUND)
        message(STATUS "ONYX - Fontconfig found with pkg-config")
        target_include_directories(onyx PRIVATE ${FONTCONFIG_INCLUDE_DIRS})
        target_link_libraries(onyx PRIVATE ${FONTCONFIG_LIBRARIES})
        target_compile_definitions(onyx PUBLIC ONYX_FONTCONFIG)
      else()
        message(STATUS "ONYX - Fontconfig not found")
      endif()
    endif()
  endif()
endif()

find_package(glfw3 QUIET)

if(NOT TARGET glfw)
  message(STATUS "ONYX - GLFW not found. Fetching source...")
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

  set(GLFW_BUILD_DOCS
      OFF
      CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES
      OFF
      CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS
      OFF
      CACHE BOOL "" FORCE)
  set(GLFW_INSTALL
      OFF
      CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(glfw)
  tkit_configure_compile_link_options(glfw NO_EXCEPTIONS NO_RTTI)

else()
  message(STATUS "ONYX - GLFW found")
endif()
target_link_libraries(onyx PRIVATE glfw)

if(ONYX_ENABLE_OBJ)
  FetchContent_Declare(
    tobjl
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

  FetchContent_MakeAvailable(tobjl)

  target_link_libraries(onyx PRIVATE tinyobjloader)
  target_compile_definitions(onyx PUBLIC ONYX_ENABLE_OBJ)
  tkit_configure_compile_link_options(tinyobjloader NO_EXCEPTIONS NO_RTTI)
endif()

tkit_register_for_yaml_serialization(
  onyx
  SOURCES
  onyx/property/transform.hpp
  onyx/property/vertex.hpp
  onyx/resource/state.hpp
  onyx/resource/options.hpp
  onyx/rendering/camera.hpp)

tkit_register_for_reflection(
  onyx
  SOURCES
  onyx/property/transform.hpp
  onyx/property/vertex.hpp
  onyx/resource/state.hpp
  onyx/resource/options.hpp
  onyx/app/input.hpp
  onyx/rendering/camera.hpp)

# IMGUI ###

if(ONYX_ENABLE_IMGUI)
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 396b33d0d011290f11915b6a2a2bf7737fb42dd7
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

  FetchContent_MakeAvailable(imgui)

  set(IMGUI_SOURCES
      ${imgui_SOURCE_DIR}/imgui.cpp
      ${imgui_SOURCE_DIR}/imgui_demo.cpp
      ${imgui_SOURCE_DIR}/imgui_draw.cpp
      ${imgui_SOURCE_DIR}/imgui_tables.cpp
      ${imgui_SOURCE_DIR}/imgui_widgets.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)

  add_library(imgui STATIC ${IMGUI_SOURCES})

  find_package(Vulkan QUIET)
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
  if(VULKAN_FOUND)
    target_include_directories(imgui PUBLIC ${Vulkan_INCLUDE_DIRS})
  else()
    target_link_libraries(imgui PUBLIC Vulkan::Headers)
  endif()
  target_link_libraries(imgui PRIVATE glfw)
  target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES
                                           IMGUI_IMPL_GLFW_DISABLE_X11)
  target_compile_definitions(onyx PUBLIC ONYX_ENABLE_IMGUI)
  target_link_libraries(onyx PUBLIC imgui)

  tkit_configure_compile_link_options(imgui NO_EXCEPTIONS NO_RTTI)
endif()

# IMPLOT ###

if(ONYX_ENABLE_IMPLOT)
  FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.17
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

  FetchContent_MakeAvailable(implot)

  set(IMPLOT_SOURCES
      ${implot_SOURCE_DIR}/implot.cpp ${implot_SOURCE_DIR}/implot_items.cpp
      ${implot_SOURCE_DIR}/implot_demo.cpp)

  add_library(implot STATIC ${IMPLOT_SOURCES})

  target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
  target_link_libraries(implot PUBLIC imgui)
  target_compile_definitions(onyx PUBLIC ONYX_ENABLE_IMPLOT)
  target_link_libraries(onyx PUBLIC implot)

  tkit_configure_compile_link_options(implot NO_EXCEPTIONS NO_RTTI)
endif()

if(ONYX_ENABLE_NFD)
  FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/mlabbe/nativefiledialog.git
    GIT_TAG release_116
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

  FetchContent_MakeAvailable(nfd)

  set(NFD_SOURCES ${nfd_SOURCE_DIR}/src/nfd_common.c)
  if(WIN32)
    list(APPEND NFD_SOURCES ${nfd_SOURCE_DIR}/src/nfd_win.cpp)
  elseif(APPLE)
    list(APPEND NFD_SOURCES ${nfd_SOURCE_DIR}/src/nfd_cocoa.m)
  elseif(UNIX)
    find_package(GTK)
    if(GTK_FOUND)
      message(STATUS "ONYX - Using GTK as NFD backend")
      list(APPEND NFD_SOURCES ${nfd_SOURCE_DIR}/src/nfd_gtk.c)

    else()
      message(STATUS "ONYX - Using Zenity as NFD backend")
      list(APPEND NFD_SOURCES ${nfd_SOURCE_DIR}/src/nfd_zenity.c)

    endif()

  else()
    message(FATAL_ERROR "ONYX - No available platform to use NFD")
  endif()

  add_library(nfd STATIC ${NFD_SOURCES})

  target_include_directories(nfd PUBLIC ${nfd_SOURCE_DIR}/src/include)
  target_compile_definitions(onyx PUBLIC ONYX_ENABLE_NFD)
  target_link_libraries(onyx PRIVATE nfd)
endif()

target_link_libraries(onyx PUBLIC toolkit vulkit)
target_include_directories(onyx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(onyx PRIVATE
                          ${CMAKE_CURRENT_SOURCE_DIR}/onyx/core/pch.hpp)
target_compile_definitions(onyx PUBLIC ONYX_ROOT_PATH="${ONYX_ROOT_PATH}")

tkit_default_configure(onyx)

# No need for more: toolkit handles the rest. As it is a library I develop, I
# will be reusing its macros/configurations
