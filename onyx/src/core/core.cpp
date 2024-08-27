#include "core/pch.hpp"
#include "onyx/core/core.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
static KIT::StackAllocator *s_Allocator;
static KIT::TaskManager *s_Manager;

static KIT::Ref<Instance> s_Instance;
static KIT::Ref<Device> s_Device;

void Core::Initialize(KIT::StackAllocator *p_Allocator, KIT::TaskManager *p_Manager) noexcept
{
    KIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "Failed to initialize GLFW");

    s_Instance = new Instance;
    s_Allocator = p_Allocator;
    s_Manager = p_Manager;
}

void Core::Terminate() noexcept
{
    glfwTerminate();

    // Release both the instance and the device. After this call, these two are no longer guaranteed to be valid
    s_Device = nullptr;
    s_Instance = nullptr;
}

const KIT::Ref<Instance> &Core::GetInstance() noexcept
{
    KIT_ASSERT(s_Instance, "Vulkan instance is not initialize! Forgot to call ONYX::Core::Initialize?");
    return s_Instance;
}
const KIT::Ref<Device> &Core::GetDevice() noexcept
{
    return s_Device;
}

const KIT::Ref<Device> &Core::getOrCreateDevice(VkSurfaceKHR p_Surface) noexcept
{
    if (!s_Device)
        s_Device = new Device(p_Surface);
    KIT_ASSERT(s_Device->IsSuitable(p_Surface), "The current device is not suitable for the given surface");
    return s_Device;
}

KIT::StackAllocator *Core::StackAllocator() noexcept
{
    return s_Allocator;
}
KIT::TaskManager *Core::TaskManager() noexcept
{
    return s_Manager;
}

void Allocator::Pop(const void *p_Ptr) noexcept
{
    s_Allocator->Deallocate(p_Ptr);
}

} // namespace ONYX
