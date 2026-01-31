cmake_minimum_required(VERSION 3.16)
project(onyx-demo)

if(NOT ONYX_ENABLE_IMGUI)
  message(
    WARNING
      "ONYX - Demos rely heavily on ImGui. They will work without it, but the experience will be very limited. Enable it by setting ONYX_ENABLE_IMGUI to ON"
  )
endif()

add_subdirectory(sandbox)
add_subdirectory(hello-world)
# add_subdirectory(performance)
