#define VMA_IMPLEMENTATION
#include "core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"
#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/rendering/swap_chain.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
static KIT::StackAllocator *s_StackAllocator;
static KIT::ITaskManager *s_Manager;

static KIT::Ref<Instance> s_Instance;
static KIT::Ref<Device> s_Device;

static KIT::Ref<DescriptorPool> s_DescriptorPool;
static KIT::Ref<DescriptorSetLayout> s_DescriptorSetLayout;

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

static void createDescriptorData() noexcept
{
    DescriptorPool::Specs poolSpecs{};
    poolSpecs.MaxSets = 8;

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 8;
    poolSpecs.PoolSizes = std::span<const VkDescriptorPoolSize>(&poolSize, 1);

    s_DescriptorPool = KIT::Ref<DescriptorPool>::Create(poolSpecs);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    const std::span<const VkDescriptorSetLayoutBinding> bindings(&binding, 1);
    s_DescriptorSetLayout = KIT::Ref<DescriptorSetLayout>::Create(bindings);
}

void Core::Initialize(KIT::StackAllocator *p_Allocator, KIT::ITaskManager *p_Manager) noexcept
{
    KIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "Failed to initialize GLFW");

    s_Instance = KIT::Ref<Instance>::Create();
    s_StackAllocator = p_Allocator;
    s_Manager = p_Manager;
}

void Core::Terminate() noexcept
{
    if (s_Device)
        DestroyCombinedPrimitiveBuffers();
    glfwTerminate();
    if (s_Device)
        s_Device->WaitIdle();
    vmaDestroyAllocator(s_VulkanAllocator);
    // Release both the instance and the device. After this call, these two are no longer guaranteed to be valid
    s_Device = nullptr;
    s_Instance = nullptr;
    s_DescriptorPool = nullptr;
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

VmaAllocator Core::GetVulkanAllocator() noexcept
{
    return s_VulkanAllocator;
}

const KIT::Ref<DescriptorPool> &Core::GetDescriptorPool() noexcept
{
    return s_DescriptorPool;
}
const KIT::Ref<DescriptorSetLayout> &Core::GetStorageBufferDescriptorSetLayout() noexcept
{
    return s_DescriptorSetLayout;
}

KIT::StackAllocator *Core::GetStackAllocator() noexcept
{
    return s_StackAllocator;
}
KIT::ITaskManager *Core::GetTaskManager() noexcept
{
    return s_Manager;
}

const KIT::Ref<Device> &Core::tryCreateDevice(VkSurfaceKHR p_Surface) noexcept
{
    if (!s_Device)
    {
        s_Device = KIT::Ref<Device>::Create(p_Surface);
        createVulkanAllocator();
        createDescriptorData();
        CreateCombinedPrimitiveBuffers();
    }
    KIT_ASSERT(s_Device->IsSuitable(p_Surface), "The current device is not suitable for the given surface");
    return s_Device;
}

} // namespace ONYX
