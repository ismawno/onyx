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
#ifdef ONYX_FONTCONFIG
#    include <fontconfig/fontconfig.h>
#endif

namespace Onyx
{
using namespace Detail;

static TKit::ITaskManager *s_TaskManager;
static TKit::Storage<TKit::TaskManager> s_DefaultManager;

static VKit::Instance s_Instance{};
static VKit::LogicalDevice s_Device{};

static VKit::DeletionQueue s_DeletionQueue{};

static VKit::DescriptorPool s_DescriptorPool{};
static VKit::DescriptorSetLayout s_InstanceDataStorageLayout{};
static VKit::DescriptorSetLayout s_LightStorageLayout{};

static VKit::PipelineLayout s_DLevelSimpleLayout{};
static VKit::PipelineLayout s_DLevelComplexLayout{};

static VKit::CommandPool s_GraphicsPool{};
static VKit::CommandPool s_TransferPool{};
static TransferMode s_TransferMode;

#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
static TKit::VkProfilingContext s_GraphicsContext;
// static TKit::VkProfilingContext s_TransferContext;
static VkCommandBuffer s_ProfilingGraphicsCmd;
// static VkCommandBuffer s_ProfilingTransferCmd;
#endif

static VkQueue s_GraphicsQueue = VK_NULL_HANDLE;
static VkQueue s_PresentQueue = VK_NULL_HANDLE;
static VkQueue s_TransferQueue = VK_NULL_HANDLE;

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;

static Initializer *s_Initializer;

static void createDevice(const VkSurfaceKHR p_Surface)
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

static void createVulkanAllocator()
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

static void createCommandPool()
{
    TKIT_LOG_INFO("[ONYX] Creating global command pools");
    auto poolres = VKit::CommandPool::Create(s_Device, Core::GetGraphicsIndex(),
                                             VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                                                 VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    VKIT_ASSERT_RESULT(poolres);
    s_GraphicsPool = poolres.GetValue();
    s_DeletionQueue.SubmitForDeletion(s_GraphicsPool);
    if (s_TransferMode == TransferMode::Separate)
    {
        poolres = VKit::CommandPool::Create(s_Device, Core::GetTransferIndex(),
                                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                                                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        VKIT_ASSERT_RESULT(poolres);
        s_TransferPool = poolres.GetValue();
        s_DeletionQueue.SubmitForDeletion(s_TransferPool);
    }
    else
        s_TransferPool = s_GraphicsPool;
}

#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
static void createProfilingContext()
{
    TKIT_LOG_INFO("[ONYX] Creating Vulkan profiling context");
    auto cmdres = s_GraphicsPool.Allocate();
    VKIT_ASSERT_RESULT(cmdres);
    s_ProfilingGraphicsCmd = cmdres.GetValue();

    s_GraphicsContext = TKIT_PROFILE_CREATE_VULKAN_CONTEXT(
        s_Instance, s_Device.GetPhysicalDevice(), s_Device, s_GraphicsQueue, s_ProfilingGraphicsCmd,
        VKit::Vulkan::vkGetInstanceProcAddr, s_Instance.GetInfo().Table.vkGetDeviceProcAddr);

    s_DeletionQueue.Push([] { TKIT_PROFILE_DESTROY_VULKAN_CONTEXT(s_GraphicsContext); });

    // if (s_TransferMode == TransferMode::Separate)
    // {
    //     cmdres = s_TransferPool.Allocate();
    //     VKIT_ASSERT_RESULT(cmdres);
    //     s_ProfilingTransferCmd = cmdres.GetValue();
    //
    //     s_TransferContext = TKIT_PROFILE_CREATE_VULKAN_CONTEXT(
    //         s_Instance, s_Device.GetPhysicalDevice(), s_Device, s_TransferQueue, s_ProfilingTransferCmd,
    //         VKit::Vulkan::vkGetInstanceProcAddr, s_Instance.GetInfo().Table.vkGetDeviceProcAddr);
    //
    //     s_DeletionQueue.Push([] { TKIT_PROFILE_DESTROY_VULKAN_CONTEXT(s_TransferContext); });
    // }
    // else
    //     s_TransferContext = s_GraphicsContext;
}
#endif

static void createDescriptorData()
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

static void createPipelineLayouts()
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

static void createShaders()
{
    TKIT_LOG_INFO("[ONYX] Creating global shaders");
    Shaders<D2, DrawMode::Fill>::Initialize();
    Shaders<D2, DrawMode::Stencil>::Initialize();
    Shaders<D3, DrawMode::Fill>::Initialize();
    Shaders<D3, DrawMode::Stencil>::Initialize();
}

void Core::Initialize(const Specs &p_Specs)
{
    TKIT_LOG_INFO("[ONYX] Creating Vulkan instance");
#ifdef ONYX_FONTCONFIG
    FcInit();
#endif

    if (p_Specs.TaskManager)
        s_TaskManager = p_Specs.TaskManager;
    else
    {
        s_DefaultManager.Construct();
        s_TaskManager = s_DefaultManager.Get();
        s_DeletionQueue.Push([] { s_DefaultManager.Destruct(); });
    }
    s_Initializer = p_Specs.Initializer;

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
    builder.RequestValidationLayers();
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
#ifdef TKIT_ENABLE_INFO_LOGS
    if (s_Instance.GetInfo().Flags & VKit::Instance::Flag_HasValidationLayers)
    {
        TKIT_LOG_INFO("[ONYX] Validation layers enabled");
    }
    else
    {
        TKIT_LOG_INFO("[ONYX] Validation layers disabled");
    }
#endif
    s_DeletionQueue.SubmitForDeletion(s_Instance);
}

void Core::Terminate()
{
    if (IsDeviceCreated())
        DeviceWaitIdle();

    s_DeletionQueue.Flush();
    glfwTerminate();
}

void Core::CreateDevice(const VkSurfaceKHR p_Surface)
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
#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
    createProfilingContext();
#endif
}
TKit::ITaskManager *Core::GetTaskManager()
{
    return s_TaskManager;
}
void Core::SetTaskManager(TKit::ITaskManager *p_TaskManager)
{
    s_TaskManager = p_TaskManager;
}

const VKit::Instance &Core::GetInstance()
{
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance;
}
const VKit::Vulkan::InstanceTable &Core::GetInstanceTable()
{
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance.GetInfo().Table;
};
const VKit::LogicalDevice &Core::GetDevice()
{
    return s_Device;
}
const VKit::Vulkan::DeviceTable &Core::GetDeviceTable()
{
    return s_Device.GetTable();
};
bool Core::IsDeviceCreated()
{
    return s_Device;
}
void Core::DeviceWaitIdle()
{
    s_Device.WaitIdle();
}

VKit::CommandPool &Core::GetGraphicsPool()
{
    return s_GraphicsPool;
}
VKit::CommandPool &Core::GetTransferPool()
{
    return s_TransferPool;
}
VmaAllocator Core::GetVulkanAllocator()
{
    return s_VulkanAllocator;
}

VKit::DeletionQueue &Core::GetDeletionQueue()
{
    return s_DeletionQueue;
}

const VKit::DescriptorPool &Core::GetDescriptorPool()
{
    return s_DescriptorPool;
}
const VKit::DescriptorSetLayout &Core::GetInstanceDataStorageDescriptorSetLayout()
{
    return s_InstanceDataStorageLayout;
}
const VKit::DescriptorSetLayout &Core::GetLightStorageDescriptorSetLayout()
{
    return s_LightStorageLayout;
}

VkPipelineLayout Core::GetGraphicsPipelineLayoutSimple()
{
    return s_DLevelSimpleLayout;
}
VkPipelineLayout Core::GetGraphicsPipelineLayoutComplex()
{
    return s_DLevelComplexLayout;
}

VkQueue Core::GetGraphicsQueue()
{
    return s_GraphicsQueue;
}
VkQueue Core::GetPresentQueue()
{
    return s_PresentQueue;
}
VkQueue Core::GetTransferQueue()
{
    return s_TransferQueue;
}

u32 Core::GetGraphicsIndex()
{
    return s_Device.GetPhysicalDevice().GetInfo().GraphicsIndex;
}
u32 Core::GetPresentIndex()
{
    return s_Device.GetPhysicalDevice().GetInfo().PresentIndex;
}
u32 Core::GetTransferIndex()
{
    return s_Device.GetPhysicalDevice().GetInfo().TransferIndex;
}

TransferMode Core::GetTransferMode()
{
    return s_TransferMode;
}

#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
TKit::VkProfilingContext Core::GetGraphicsContext()
{
    return s_GraphicsContext;
}
// TKit::VkProfilingContext Core::GetTransferContext()
// {
//     return s_TransferContext;
// }
#endif

} // namespace Onyx
