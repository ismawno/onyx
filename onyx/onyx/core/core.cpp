#define VMA_IMPLEMENTATION
#include "onyx/core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"
#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/rendering/swap_chain.hpp"
#include "onyx/rendering/render_specs.hpp"
#include "kit/core/logging.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
static KIT::StackAllocator *s_StackAllocator;
static KIT::ITaskManager *s_Manager;

static KIT::Ref<Instance> s_Instance;
static KIT::Ref<Device> s_Device;

static KIT::Storage<DescriptorPool> s_DescriptorPool;
static KIT::Storage<DescriptorSetLayout> s_TransformStorageLayout;
static KIT::Storage<DescriptorSetLayout> s_LightStorageLayout;

static VkPipelineLayout s_GraphicsPipelineLayout2D = VK_NULL_HANDLE;
static VkPipelineLayout s_GraphicsPipelineLayout3D = VK_NULL_HANDLE;

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
    poolSpecs.MaxSets = ONYX_MAX_DESCRIPTOR_SETS;
    poolSpecs.PoolFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = ONYX_MAX_STORAGE_BUFFER_DESCRIPTORS;
    poolSpecs.PoolSizes = std::span<const VkDescriptorPoolSize>(&poolSize, 1);

    s_DescriptorPool.Create(poolSpecs);

    VkDescriptorSetLayoutBinding transformBinding{};
    transformBinding.binding = 0;
    transformBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    transformBinding.descriptorCount = 1;
    transformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    const std::span<const VkDescriptorSetLayoutBinding> bindings(&transformBinding, 1);
    s_TransformStorageLayout.Create(bindings);

    std::array<VkDescriptorSetLayoutBinding, 2> lightBindings{};
    for (u32 i = 0; i < 2; i++)
    {
        lightBindings[i].binding = i;
        lightBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightBindings[i].descriptorCount = 1;
        lightBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    s_LightStorageLayout.Create(lightBindings);
}

static void createPipelineLayouts() noexcept
{
    VkPipelineLayoutCreateInfo layoutInfo{};

    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    const VkDescriptorSetLayout layout = s_TransformStorageLayout->GetLayout();
    layoutInfo.pSetLayouts = &layout;
    layoutInfo.setLayoutCount = 1;

    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    KIT_ASSERT_RETURNS(vkCreatePipelineLayout(s_Device->GetDevice(), &layoutInfo, nullptr, &s_GraphicsPipelineLayout2D),
                       VK_SUCCESS, "Failed to create pipeline layout");

    const std::array<VkDescriptorSetLayout, 2> setLayouts = {s_TransformStorageLayout->GetLayout(),
                                                             s_LightStorageLayout->GetLayout()};
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.setLayoutCount = static_cast<u32>(setLayouts.size());

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantData3D);

    layoutInfo.pPushConstantRanges = &pushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;

    KIT_ASSERT_RETURNS(vkCreatePipelineLayout(s_Device->GetDevice(), &layoutInfo, nullptr, &s_GraphicsPipelineLayout3D),
                       VK_SUCCESS, "Failed to create pipeline layout");
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
    // Release all refcounted objects. After this call, none are guaranteed to be valid
    s_TransformStorageLayout.Destroy();
    s_LightStorageLayout.Destroy();

    vkDestroyPipelineLayout(s_Device->GetDevice(), s_GraphicsPipelineLayout2D, nullptr);
    vkDestroyPipelineLayout(s_Device->GetDevice(), s_GraphicsPipelineLayout3D, nullptr);

    s_DescriptorPool.Destroy();
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

VmaAllocator Core::GetVulkanAllocator() noexcept
{
    return s_VulkanAllocator;
}

const DescriptorPool *Core::GetDescriptorPool() noexcept
{
    return s_DescriptorPool.Get();
}
const DescriptorSetLayout *Core::GetTransformStorageDescriptorSetLayout() noexcept
{
    return s_TransformStorageLayout.Get();
}
const DescriptorSetLayout *Core::GetLightStorageDescriptorSetLayout() noexcept
{
    return s_LightStorageLayout.Get();
}

template <Dimension D> VkPipelineLayout Core::GetGraphicsPipelineLayout() noexcept
{
    if constexpr (D == D2)
        return s_GraphicsPipelineLayout2D;
    else
        return s_GraphicsPipelineLayout3D;
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
        createPipelineLayouts();
        CreateCombinedPrimitiveBuffers();
    }
    KIT_ASSERT(s_Device->IsSuitable(p_Surface), "The current device is not suitable for the given surface");
    return s_Device;
}

template VkPipelineLayout Core::GetGraphicsPipelineLayout<D2>() noexcept;
template VkPipelineLayout Core::GetGraphicsPipelineLayout<D3>() noexcept;

} // namespace ONYX
