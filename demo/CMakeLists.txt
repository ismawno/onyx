cmake_minimum_required(VERSION 3.16)
project(onyx-demo)

add_executable(onyx-hello-world hello_world.cpp)
add_executable(onyx-layout layout.cpp)
add_executable(onyx-lights lights.cpp)
add_executable(onyx-transparency transparency.cpp)

tkit_default_configure(onyx-hello-world)
tkit_default_configure(onyx-layout)
tkit_default_configure(onyx-lights)
tkit_default_configure(onyx-transparency)

target_link_libraries(onyx-hello-world PRIVATE onyx)
target_link_libraries(onyx-layout PRIVATE onyx)
target_link_libraries(onyx-lights PRIVATE onyx)
target_link_libraries(onyx-transparency PRIVATE onyx)

if(ONYX_INCLUDE_DEFAULT_FONT)
  add_executable(onyx-text text.cpp)

  tkit_default_configure(onyx-text)
  target_link_libraries(onyx-text PRIVATE onyx)
endif()
if(ONYX_ENABLE_IMGUI)
  add_executable(onyx-imgui imgui.cpp)

  tkit_default_configure(onyx-imgui)
  target_link_libraries(onyx-imgui PRIVATE onyx)
endif()
