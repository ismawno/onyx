cmake_minimum_required(VERSION 3.16)
project(hello-world)

set(SOURCES
    main.cpp
)

add_executable(hello-world ${SOURCES})
if (MSVC)
    configure_compile_link_options(hello-world FLAGS /W4 /WX)
else()
    configure_compile_link_options(hello-world FLAGS -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(hello-world PRIVATE onyx)