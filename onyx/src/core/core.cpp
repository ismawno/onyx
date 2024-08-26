#include "core/pch.hpp"
#include "onyx/core/core.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
void Initialize() noexcept
{
    KIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "Failed to initialize GLFW");
}

void Terminate() noexcept
{
    glfwTerminate();
}
} // namespace ONYX