#include "core/pch.hpp"
#include "onyx/core/core.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

ONYX_NAMESPACE_BEGIN

void Initialize() KIT_NOEXCEPT
{
    KIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "Failed to initialize GLFW");
}

void Terminate() KIT_NOEXCEPT
{
    glfwTerminate();
}

ONYX_NAMESPACE_END