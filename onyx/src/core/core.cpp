#include "core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/draw/model.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
static KIT::StackAllocator *s_Allocator;
static KIT::TaskManager *s_Manager;

static KIT::Ref<ONYX::Instance> s_Instance;
static KIT::Ref<ONYX::Device> s_Device;

void Core::Initialize(KIT::StackAllocator *p_Allocator, KIT::TaskManager *p_Manager) noexcept
{
    KIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "Failed to initialize GLFW");

    s_Instance = KIT::Ref<ONYX::Instance>::Create();
    s_Allocator = p_Allocator;
    s_Manager = p_Manager;
}

void Core::Terminate() noexcept
{
    if (s_Device)
        Model::DestroyPrimitiveModels();
    glfwTerminate();
    if (s_Device)
        s_Device->WaitIdle();
    // Release both the instance and the device. After this call, these two are no longer guaranteed to be valid
    s_Device = nullptr;
    s_Instance = nullptr;
}

const KIT::Ref<ONYX::Instance> &Core::GetInstance() noexcept
{
    KIT_ASSERT(s_Instance, "Vulkan instance is not initialize! Forgot to call ONYX::Core::Initialize?");
    return s_Instance;
}
const KIT::Ref<ONYX::Device> &Core::GetDevice() noexcept
{
    return s_Device;
}

ONYX_DIMENSION_TEMPLATE const char *Core::GetVertexShaderPath() noexcept
{
    if constexpr (N == 3)
        return KIT_ROOT_PATH "/onyx/shaders/bin/shader3D.vert.spv";
    else
        return KIT_ROOT_PATH "/onyx/shaders/bin/shader2D.vert.spv";
}
ONYX_DIMENSION_TEMPLATE const char *Core::GetFragmentShaderPath() noexcept
{
    if constexpr (N == 3)
        return KIT_ROOT_PATH "/onyx/shaders/bin/shader3D.frag.spv";
    else
        return KIT_ROOT_PATH "/onyx/shaders/bin/shader2D.frag.spv";
}

KIT::StackAllocator *Core::GetStackAllocator() noexcept
{
    return s_Allocator;
}
KIT::TaskManager *Core::GetTaskManager() noexcept
{
    return s_Manager;
}

const KIT::Ref<ONYX::Device> &Core::tryCreateDevice(VkSurfaceKHR p_Surface) noexcept
{
    if (!s_Device)
    {
        s_Device = KIT::Ref<ONYX::Device>::Create(p_Surface);
        Model::CreatePrimitiveModels();
    }
    KIT_ASSERT(s_Device->IsSuitable(p_Surface), "The current device is not suitable for the given surface");
    return s_Device;
}

template const char *Core::GetVertexShaderPath<2>() noexcept;
template const char *Core::GetVertexShaderPath<3>() noexcept;

template const char *Core::GetFragmentShaderPath<2>() noexcept;
template const char *Core::GetFragmentShaderPath<3>() noexcept;

} // namespace ONYX
