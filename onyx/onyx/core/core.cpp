#include "onyx/core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/data/state.hpp"
#include "onyx/core/shaders.hpp"
#include "onyx/object/primitives.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "vkit/pipeline/pipeline_layout.hpp"
#include "vkit/core/core.hpp"
#include "tkit/utils/logging.hpp"

#include "onyx/core/glfw.hpp"
#include "vkit/vulkan/allocator.hpp"
#include "vkit/vulkan/vulkan.hpp"
#ifdef TKIT_OS_LINUX
#    include <fontconfig/fontconfig.h>
#endif

namespace Onyx
{
using namespace Detail;

static TKit::ITaskManager *s_TaskManager;

static VKit::Instance s_Instance{};
static VKit::LogicalDevice s_Device{};

static VKit::DeletionQueue s_DeletionQueue{};

static VKit::DescriptorPool s_DescriptorPool{};
static VKit::DescriptorSetLayout s_InstanceDataStorageLayout{};
static VKit::DescriptorSetLayout s_LightStorageLayout{};

static VKit::PipelineLayout s_DLevelSimpleLayout{};
static VKit::PipelineLayout s_DLevelComplexLayout{};

static VKit::CommandPool s_CommandPool{};
static TransferMode s_TransferMode;

#ifdef TKIT_ENABLE_VULKAN_PROFILING
static TKit::VkProfilingContext s_ProfilingContext;
static VkCommandBuffer s_ProfilingCommandBuffer;
#endif

static VkQueue s_GraphicsQueue = VK_NULL_HANDLE;
static VkQueue s_PresentQueue = VK_NULL_HANDLE;
static VkQueue s_TransferQueue = VK_NULL_HANDLE;

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;

static Initializer *s_Initializer;

static void createDevice(const VkSurfaceKHR p_Surface) noexcept
{
    VKit::PhysicalDevice::Selector selector(&s_Instance);

    selector.SetSurface(p_Surface)
        .PreferType(VKit::PhysicalDevice::Discrete)
        .AddFlags(VKit::PhysicalDevice::Selector::Flag_AnyType |
                  VKit::PhysicalDevice::Selector::Flag_PortabilitySubset |
                  VKit::PhysicalDevice::Selector::Flag_RequireGraphicsQueue |
                  VKit::PhysicalDevice::Selector::Flag_RequirePresentQueue |
                  VKit::PhysicalDevice::Selector::Flag_RequireTransferQueue)
        .RequireExtension("VK_KHR_dynamic_rendering")
        .RequireApiVersion(1, 2, 0)
        .RequestApiVersion(1, 3, 0);

    if (s_Initializer)
        s_Initializer->OnPhysicalDeviceCreation(selector);

    auto physres = selector.Select();
    VKIT_ASSERT_RESULT(physres);

    VKit::PhysicalDevice &phys = physres.GetValue();
    const u32 apiVersion = phys.GetInfo().ApiVersion;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR drendering{};
    drendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    drendering.dynamicRendering = VK_TRUE;
#ifdef VKIT_API_VERSION_1_3
    if (apiVersion >= VKIT_API_VERSION_1_3)
    {
        VKit::PhysicalDevice::Features features{};
        features.Vulkan13.dynamicRendering = VK_TRUE;
        TKIT_ASSERT_RETURNS(phys.EnableFeatures(features), true, "[ONYX] Failed to enable dynamic rendering");
    }
    else
        phys.EnableExtensionBoundFeature(&drendering);
#else
    phys.EnableExtensionBoundFeature(&drendering);
#endif

    const auto devres = VKit::LogicalDevice::Create(s_Instance, phys);
    VKIT_ASSERT_RESULT(devres);
    s_Device = devres.GetValue();

    s_GraphicsQueue = s_Device.GetQueue(VKit::QueueType::Graphics);
    s_PresentQueue = s_Device.GetQueue(VKit::QueueType::Present);
    s_TransferQueue = s_Device.GetQueue(VKit::QueueType::Transfer);
    TKIT_LOG_INFO("[ONYX] Created Vulkan device: {}",
                  s_Device.GetPhysicalDevice().GetInfo().Properties.Core.deviceName);
    TKIT_LOG_WARNING_IF(!(s_Device.GetPhysicalDevice().GetInfo().Flags & VKit::PhysicalDevice::Flag_Optimal),
                        "[ONYX] The device is suitable, but not optimal");

    if (s_GraphicsQueue == s_TransferQueue)
    {
        s_TransferMode = TransferMode::SameQueue;
        TKIT_LOG_INFO("[ONYX] Transfer mode is 'SameQueue'");
    }
    else if (Core::GetGraphicsIndex() == Core::GetTransferIndex())
    {
        s_TransferMode = TransferMode::SameIndex;
        TKIT_LOG_INFO("[ONYX] Transfer mode is 'SameIndex'");
    }
    else
    {
        s_TransferMode = TransferMode::Separate;
        TKIT_LOG_INFO("[ONYX] Transfer mode is 'Separate'");
    }

    s_DeletionQueue.SubmitForDeletion(s_Device);
}

static void createVulkanAllocator() noexcept
{
    VKit::AllocatorSpecs specs{};
    if (s_Initializer)
        s_Initializer->OnAllocatorCreation(specs);

    const auto result = VKit::CreateAllocator(s_Device);
    VKIT_ASSERT_RESULT(result);
    s_VulkanAllocator = result.GetValue();
    TKIT_LOG_INFO("[ONYX] Creating Vulkan allocator");

    s_DeletionQueue.Push([] { VKit::DestroyAllocator(s_VulkanAllocator); });
}

static void createCommandPool() noexcept
{
    TKIT_LOG_INFO("[ONYX] Creating global command pool");
    const auto poolres = VKit::CommandPool::Create(s_Device, s_Device.GetPhysicalDevice().GetInfo().GraphicsIndex,
                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                                                       VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    VKIT_ASSERT_RESULT(poolres);
    s_CommandPool = poolres.GetValue();

    s_DeletionQueue.SubmitForDeletion(s_CommandPool);
}

#ifdef TKIT_ENABLE_VULKAN_PROFILING
static void createProfilingContext() noexcept
{
    TKIT_LOG_INFO("[ONYX] Creating Vulkan profiling context");
    const auto cmdres = s_CommandPool.Allocate();
    VKIT_ASSERT_RESULT(cmdres);
    s_ProfilingCommandBuffer = cmdres.GetValue();

    s_ProfilingContext = TKIT_PROFILE_CREATE_VULKAN_CONTEXT(s_Device.GetPhysicalDevice(), s_Device, s_GraphicsQueue,
                                                            s_ProfilingCommandBuffer);
}
#endif

static void createDescriptorData() noexcept
{
    TKIT_LOG_INFO("[ONYX] Creating global descriptor data");
    const auto poolResult = VKit::DescriptorPool::Builder(s_Device)
                                .SetMaxSets(ONYX_MAX_DESCRIPTOR_SETS)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ONYX_MAX_DESCRIPTORS)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ONYX_MAX_DESCRIPTORS)
                                .Build();

    VKIT_ASSERT_RESULT(poolResult);
    s_DescriptorPool = poolResult.GetValue();

    auto layoutResult = VKit::DescriptorSetLayout::Builder(s_Device)
                            .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_InstanceDataStorageLayout = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(s_Device)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_LightStorageLayout = layoutResult.GetValue();

    s_DeletionQueue.SubmitForDeletion(s_DescriptorPool);
    s_DeletionQueue.SubmitForDeletion(s_InstanceDataStorageLayout);
    s_DeletionQueue.SubmitForDeletion(s_LightStorageLayout);
}

static void createPipelineLayouts() noexcept
{
    TKIT_LOG_INFO("[ONYX] Creating global pipeline layouts");
    auto layoutResult = VKit::PipelineLayout::Builder(s_Device)
                            .AddDescriptorSetLayout(s_InstanceDataStorageLayout)
                            .AddPushConstantRange<PushConstantData<DrawLevel::Simple>>(VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_DLevelSimpleLayout = layoutResult.GetValue();
    s_DeletionQueue.SubmitForDeletion(layoutResult.GetValue());

    layoutResult = VKit::PipelineLayout::Builder(s_Device)
                       .AddDescriptorSetLayout(s_InstanceDataStorageLayout)
                       .AddDescriptorSetLayout(s_LightStorageLayout)
                       .AddPushConstantRange<PushConstantData<DrawLevel::Complex>>(VK_SHADER_STAGE_VERTEX_BIT |
                                                                                   VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_DLevelComplexLayout = layoutResult.GetValue();
    s_DeletionQueue.SubmitForDeletion(layoutResult.GetValue());
}

static void createShaders() noexcept
{
    TKIT_LOG_INFO("[ONYX] Creating global shaders");
    Shaders<D2, DrawMode::Fill>::Initialize();
    Shaders<D2, DrawMode::Stencil>::Initialize();
    Shaders<D3, DrawMode::Fill>::Initialize();
    Shaders<D3, DrawMode::Stencil>::Initialize();
}

void Core::Initialize(TKit::ITaskManager *p_TaskManager, Initializer *p_Initializer) noexcept
{
    TKIT_LOG_INFO("[ONYX] Creating Vulkan instance");
#ifdef TKIT_OS_LINUX
    FcInit();
#endif

    s_TaskManager = p_TaskManager;
    s_Initializer = p_Initializer;

    const auto sysres = VKit::Core::Initialize();
    VKIT_ASSERT_RESULT(sysres);

    TKIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "[ONYX] Failed to initialize GLFW");
    u32 extensionCount;
    const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    const TKit::Span<const char *const> extensionSpan(extensions, extensionCount);

    VKit::Instance::Builder builder{};
    builder.SetApplicationName("Onyx")
        .RequestApiVersion(1, 3, 0)
        .RequireApiVersion(1, 2, 0)
        .RequireExtensions(extensionSpan)
        .SetApplicationVersion(1, 2, 0);
#ifdef TKIT_ENABLE_ASSERTS
    builder.RequireValidationLayers();
#endif
    if (s_Initializer)
        s_Initializer->OnInstanceCreation(builder);

    const auto result = builder.Build();
    VKIT_ASSERT_RESULT(result);

    s_Instance = result.GetValue();
    TKIT_LOG_INFO("[ONYX] Created Vulkan instance. API version: {}.{}.{}",
                  VKIT_API_VERSION_MAJOR(s_Instance.GetInfo().ApiVersion),
                  VKIT_API_VERSION_MINOR(s_Instance.GetInfo().ApiVersion),
                  VKIT_API_VERSION_PATCH(s_Instance.GetInfo().ApiVersion));
    s_DeletionQueue.SubmitForDeletion(s_Instance);
}

void Core::Terminate() noexcept
{
    if (IsDeviceCreated())
        DeviceWaitIdle();

    s_DeletionQueue.Flush();
    glfwTerminate();
}

void Core::CreateDevice(const VkSurfaceKHR p_Surface) noexcept
{
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    TKIT_ASSERT(!s_Device, "[ONYX] Device has already been created");

    createDevice(p_Surface);
    createVulkanAllocator();
    createCommandPool();
    createDescriptorData();
    createPipelineLayouts();
    CreateCombinedPrimitiveBuffers();
    createShaders();
#ifdef TKIT_ENABLE_VULKAN_PROFILING
    createProfilingContext();
#endif
}
TKit::ITaskManager *Core::GetTaskManager() noexcept
{
    return s_TaskManager;
}
void Core::SetTaskManager(TKit::ITaskManager *p_TaskManager) noexcept
{
    s_TaskManager = p_TaskManager;
}

const VKit::Instance &Core::GetInstance() noexcept
{
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance;
}
const VKit::Vulkan::InstanceTable &Core::GetInstanceTable() noexcept
{
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance.GetInfo().Table;
};
const VKit::LogicalDevice &Core::GetDevice() noexcept
{
    return s_Device;
}
const VKit::Vulkan::DeviceTable &Core::GetDeviceTable() noexcept
{
    return s_Device.GetTable();
};
bool Core::IsDeviceCreated() noexcept
{
    return s_Device;
}
void Core::DeviceWaitIdle() noexcept
{
    s_Device.WaitIdle();
}

VKit::CommandPool &Core::GetCommandPool() noexcept
{
    return s_CommandPool;
}
VmaAllocator Core::GetVulkanAllocator() noexcept
{
    return s_VulkanAllocator;
}

VKit::DeletionQueue &Core::GetDeletionQueue() noexcept
{
    return s_DeletionQueue;
}

const VKit::DescriptorPool &Core::GetDescriptorPool() noexcept
{
    return s_DescriptorPool;
}
const VKit::DescriptorSetLayout &Core::GetInstanceDataStorageDescriptorSetLayout() noexcept
{
    return s_InstanceDataStorageLayout;
}
const VKit::DescriptorSetLayout &Core::GetLightStorageDescriptorSetLayout() noexcept
{
    return s_LightStorageLayout;
}

VkPipelineLayout Core::GetGraphicsPipelineLayoutSimple() noexcept
{
    return s_DLevelSimpleLayout;
}
VkPipelineLayout Core::GetGraphicsPipelineLayoutComplex() noexcept
{
    return s_DLevelComplexLayout;
}

VkQueue Core::GetGraphicsQueue() noexcept
{
    return s_GraphicsQueue;
}
VkQueue Core::GetPresentQueue() noexcept
{
    return s_PresentQueue;
}
VkQueue Core::GetTransferQueue() noexcept
{
    return s_TransferQueue;
}

u32 Core::GetGraphicsIndex() noexcept
{
    return s_Device.GetPhysicalDevice().GetInfo().GraphicsIndex;
}
u32 Core::GetPresentIndex() noexcept
{
    return s_Device.GetPhysicalDevice().GetInfo().PresentIndex;
}
u32 Core::GetTransferIndex() noexcept
{
    return s_Device.GetPhysicalDevice().GetInfo().TransferIndex;
}

TransferMode Core::GetTransferMode() noexcept
{
    return s_TransferMode;
}

#ifdef TKIT_ENABLE_VULKAN_PROFILING
TKit::VkProfilingContext Core::GetProfilingContext() noexcept
{
    return s_ProfilingContext;
}
#endif

} // namespace Onyx
