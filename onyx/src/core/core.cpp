#define VMA_IMPLEMENTATION
#include "core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/draw/model.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
static KIT::StackAllocator *s_StackAllocator;
static KIT::ITaskManager *s_Manager;

static KIT::Ref<ONYX::Instance> s_Instance;
static KIT::Ref<ONYX::Device> s_Device;

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;

static void createVulkanAllocator() noexcept
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = s_Device->GetPhysicalDevice();
    allocatorInfo.device = s_Device->GetDevice();
    allocatorInfo.instance = s_Instance->GetInstance();
#ifdef KIT_OS_APPLE
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
#else
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
#endif
    allocatorInfo.flags = 0;
    allocatorInfo.pVulkanFunctions = nullptr;
    KIT_ASSERT_RETURNS(vmaCreateAllocator(&allocatorInfo, &s_VulkanAllocator), VK_SUCCESS,
                       "Failed to create vulkan allocator");
}

void Core::Initialize(KIT::StackAllocator *p_Allocator, KIT::ITaskManager *p_Manager) noexcept
{
    KIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "Failed to initialize GLFW");

    s_Instance = KIT::Ref<ONYX::Instance>::Create();
    s_StackAllocator = p_Allocator;
    s_Manager = p_Manager;
}

void Core::Terminate() noexcept
{
    if (s_Device)
        Model::DestroyPrimitiveModels();
    glfwTerminate();
    if (s_Device)
        s_Device->WaitIdle();
    vmaDestroyAllocator(s_VulkanAllocator);
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

VmaAllocator Core::GetVulkanAllocator() noexcept
{
    return s_VulkanAllocator;
}

KIT::StackAllocator *Core::GetStackAllocator() noexcept
{
    return s_StackAllocator;
}
KIT::ITaskManager *Core::GetTaskManager() noexcept
{
    return s_Manager;
}

const KIT::Ref<ONYX::Device> &Core::tryCreateDevice(VkSurfaceKHR p_Surface) noexcept
{
    if (!s_Device)
    {
        s_Device = KIT::Ref<ONYX::Device>::Create(p_Surface);
        createVulkanAllocator();
        Model::CreatePrimitiveModels();
    }
    KIT_ASSERT(s_Device->IsSuitable(p_Surface), "The current device is not suitable for the given surface");
    return s_Device;
}

} // namespace ONYX
