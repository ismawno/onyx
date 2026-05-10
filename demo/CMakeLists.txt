cmake_minimum_required(VERSION 3.16)
project(onyx-demo)

add_executable(onyx-hello-world hello_world.cpp)

tkit_default_configure(onyx-hello-world)

target_link_libraries(onyx-hello-world PRIVATE onyx)

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
