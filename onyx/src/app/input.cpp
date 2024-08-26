#include "core/pch.hpp"
#include "onyx/app/input.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

ONYX_NAMESPACE_BEGIN

ONYX_DIMENSION_TEMPLATE void Input<N>::PollEvents()
{
    glfwPollEvents();
}

template struct Input<2>;
template struct Input<3>;

ONYX_NAMESPACE_END