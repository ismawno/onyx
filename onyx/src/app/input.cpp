#include "core/pch.hpp"
#include "onyx/app/input.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
void Input::PollEvents()
{
    glfwPollEvents();
}
} // namespace ONYX