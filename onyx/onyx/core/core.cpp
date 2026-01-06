#include "onyx/core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/core/glfw.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/execution/queues.hpp"

#include "vkit/core/core.hpp"
#include "vkit/memory/allocator.hpp"
#include "vkit/vulkan/vulkan.hpp"

#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/profiling/macros.hpp"
#ifdef ONYX_FONTCONFIG
#    include <fontconfig/fontconfig.h>
#endif

using namespace Onyx::Detail;
namespace Onyx::Core
{
static TKit::ITaskManager *s_TaskManager;
static TKit::Storage<TKit::TaskManager> s_DefaultTaskManager;

static VKit::Instance s_Instance{};
static VKit::LogicalDevice s_Device{};

static VKit::DeletionQueue s_DeletionQueue{};

static TKit::FixedArray<u32, VKit::Queue_Count> s_QueueRequests;

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;
static InitCallbacks s_Callbacks{};

static Specs s_Specs;

static void createDevice(const VkSurfaceKHR p_Surface)
{
    VKit::PhysicalDevice::Selector selector(&s_Instance);

    selector.SetSurface(p_Surface)
        .PreferType(VKit::Device_Discrete)
        .AddFlags(VKit::DeviceSelectorFlag_AnyType | VKit::DeviceSelectorFlag_PortabilitySubset |
                  VKit::DeviceSelectorFlag_RequireGraphicsQueue | VKit::DeviceSelectorFlag_RequirePresentQueue |
                  VKit::DeviceSelectorFlag_RequireTransferQueue)
        .RequireExtension("VK_KHR_dynamic_rendering")
        .RequireApiVersion(1, 2, 0)
        .RequestApiVersion(1, 3, 0);

    if (s_Callbacks.OnPhysicalDeviceCreation)
        s_Callbacks.OnPhysicalDeviceCreation(selector);

    auto physres = selector.Select();
    VKIT_CHECK_RESULT(physres);

    VKit::PhysicalDevice &phys = physres.GetValue();
    const u32 apiVersion = phys.GetInfo().ApiVersion;

    VkPhysicalDeviceShaderDrawParameterFeatures drawParams{};
    drawParams.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;

    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR tsem{};
    tsem.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
    tsem.timelineSemaphore = VK_TRUE;

#ifdef VKIT_API_VERSION_1_2
    if (apiVersion >= VKIT_API_VERSION_1_2)
    {
        VKit::DeviceFeatures features{};
        features.Vulkan11.shaderDrawParameters = VK_TRUE;
        features.Vulkan12.timelineSemaphore = VK_TRUE;
        TKIT_CHECK_RETURNS(phys.EnableFeatures(features), true,
                           "[ONYX] Failed to enable timeline semaphores and shader draw parameters");
    }
    else
    {
        phys.EnableExtensionBoundFeature(&drawParams);
        phys.EnableExtensionBoundFeature(&tsem);
    }
#else
    phys.EnableExtensionBoundFeature(&drawParams);
    phys.EnableExtensionBoundFeature(&tsem);
#endif

    VkPhysicalDeviceDynamicRenderingFeaturesKHR drendering{};
    drendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    drendering.dynamicRendering = VK_TRUE;
#ifdef VKIT_API_VERSION_1_3
    if (apiVersion >= VKIT_API_VERSION_1_3)
    {
        VKit::DeviceFeatures features{};
        features.Vulkan13.dynamicRendering = VK_TRUE;
        TKIT_CHECK_RETURNS(phys.EnableFeatures(features), true, "[ONYX] Failed to enable dynamic rendering");
    }
    else
        phys.EnableExtensionBoundFeature(&drendering);
#else
    phys.EnableExtensionBoundFeature(&drendering);
#endif

    VKit::LogicalDevice::Builder builder{&s_Instance, &phys};
    builder.RequireQueue(VKit::Queue_Graphics)
        .RequestQueue(VKit::Queue_Present, s_QueueRequests[VKit::Queue_Present])
        .RequestQueue(VKit::Queue_Transfer, s_QueueRequests[VKit::Queue_Transfer])
        .RequestQueue(VKit::Queue_Compute, s_QueueRequests[VKit::Queue_Compute]);

    if (s_QueueRequests[VKit::Queue_Graphics] > 1)
        builder.RequestQueue(VKit::Queue_Graphics, s_QueueRequests[VKit::Queue_Graphics] - 1);

    if (s_Callbacks.OnLogicalDeviceCreation)
        s_Callbacks.OnLogicalDeviceCreation(builder);

    const auto devres = builder.Build();

    VKIT_CHECK_RESULT(devres);
    s_Device = devres.GetValue();

    TKIT_LOG_INFO("[ONYX] Created Vulkan device: {}",
                  s_Device.GetInfo().PhysicalDevice.GetInfo().Properties.Core.deviceName);
    TKIT_LOG_WARNING_IF(!(s_Device.GetInfo().PhysicalDevice.GetInfo().Flags & VKit::DeviceFlag_Optimal),
                        "[ONYX] The device is suitable, but not optimal");
    TKIT_LOG_INFO_IF(s_Device.GetInfo().PhysicalDevice.GetInfo().Flags & VKit::DeviceFlag_Optimal,
                     "[ONYX] The device is optimal");

    s_DeletionQueue.SubmitForDeletion(s_Device);
}

static void createVulkanAllocator()
{
    VKit::AllocatorSpecs specs{};
    if (s_Callbacks.OnAllocatorCreation)
        s_Callbacks.OnAllocatorCreation(specs);

    const auto result = VKit::CreateAllocator(s_Device);
    VKIT_CHECK_RESULT(result);
    s_VulkanAllocator = result.GetValue();
    TKIT_LOG_INFO("[ONYX] Creating Vulkan allocator");

    s_DeletionQueue.Push([] { VKit::DestroyAllocator(s_VulkanAllocator); });
}

void Initialize(const Specs &p_Specs)
{
    TKIT_LOG_INFO("[ONYX] Creating Vulkan instance");
    s_Specs = p_Specs;
#ifdef ONYX_FONTCONFIG
    FcInit();
#endif

#ifdef TKIT_ENABLE_ASSERTS
    for (u32 i = 0; i < 4; ++i)
    {
        TKIT_ASSERT(i == 1 || p_Specs.QueueRequests[i] > 0,
                    "[ONYX] The queue request count for all queues must be at least 1 except for compute queues, "
                    "which are not directly used by this framework");
    }
#endif

    s_QueueRequests = p_Specs.QueueRequests;

    if (p_Specs.TaskManager)
        s_TaskManager = p_Specs.TaskManager;
    else
    {
        s_DefaultTaskManager.Construct();
        s_TaskManager = s_DefaultTaskManager.Get();
        s_DeletionQueue.Push([] { s_DefaultTaskManager.Destruct(); });
    }
    s_Callbacks = p_Specs.Callbacks;

    const auto sysres = VKit::Core::Initialize();
    VKIT_CHECK_RESULT(sysres);

    glfwInitHint(GLFW_PLATFORM, p_Specs.Platform);
    TKIT_CHECK_RETURNS(glfwInit(), GLFW_TRUE, "[ONYX] Failed to initialize GLFW");
    TKIT_LOG_WARNING_IF(!glfwVulkanSupported(), "[ONYX] Vulkan is not supported, according to GLFW");

    u32 extensionCount;
    const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    const TKit::Span<const char *const> extensionSpan(extensions, extensionCount);

    VKit::Instance::Builder builder{};
    builder.SetApplicationName("Onyx")
        .RequestApiVersion(1, 3, 0)
        .RequireApiVersion(1, 2, 0)
        .RequireExtensions(extensionSpan)
        .SetApplicationVersion(1, 2, 0);
#ifdef ONYX_ENABLE_VALIDATION_LAYERS
    builder.RequestValidationLayers();
#endif
    if (s_Callbacks.OnInstanceCreation)
        s_Callbacks.OnInstanceCreation(builder);

    const auto result = builder.Build();
    VKIT_CHECK_RESULT(result);

    s_Instance = result.GetValue();
    TKIT_LOG_INFO("[ONYX] Created Vulkan instance. API version: {}.{}.{}",
                  VKIT_API_VERSION_MAJOR(s_Instance.GetInfo().ApiVersion),
                  VKIT_API_VERSION_MINOR(s_Instance.GetInfo().ApiVersion),
                  VKIT_API_VERSION_PATCH(s_Instance.GetInfo().ApiVersion));

#if defined(TKIT_ENABLE_INFO_LOGS) || defined(TKIT_ENABLE_ERROR_LOGS)
    const bool vlayers = s_Instance.GetInfo().Flags & VKit::InstanceFlag_HasValidationLayers;
#endif

    TKIT_LOG_INFO_IF(vlayers, "[ONYX] Validation layers enabled");
#ifdef ONYX_ENABLE_VALIDATION_LAYERS
    TKIT_LOG_ERROR_IF(!vlayers, "[ONYX] Validation layers were requested, but could not be enabled");
#else
    TKIT_LOG_INFO_IF(!vlayers, "[ONYX] Validation layers disabled");
#endif
    s_DeletionQueue.SubmitForDeletion(s_Instance);
    TKIT_PROFILE_PLOT_CONFIG("Draw calls", TKit::ProfilingPlotFormat::Number, false, true, 0);
}

void Terminate()
{
    if (IsDeviceCreated())
        DeviceWaitIdle();

    s_DeletionQueue.Flush();
    glfwTerminate();
}

void CreateDevice(const VkSurfaceKHR p_Surface)
{
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize()?");
    TKIT_ASSERT(!s_Device, "[ONYX] Device has already been created");

    createDevice(p_Surface);
    createVulkanAllocator();
    Queues::Initialize();
    Assets::Initialize();
    Shaders::Initialize(s_Specs.Shaders);
    Pipelines::Initialize();
    s_DeletionQueue.Push([] {
        Pipelines::Terminate();
        Assets::Terminate();
        Queues::Terminate();
    });
}
TKit::ITaskManager *GetTaskManager()
{
    return s_TaskManager;
}
void SetTaskManager(TKit::ITaskManager *p_TaskManager)
{
    s_TaskManager = p_TaskManager;
}

const VKit::Instance &GetInstance()
{
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance;
}
const VKit::Vulkan::InstanceTable &GetInstanceTable()
{
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance.GetInfo().Table;
};
const VKit::LogicalDevice &GetDevice()
{
    return s_Device;
}
const VKit::Vulkan::DeviceTable &GetDeviceTable()
{
    return s_Device.GetInfo().Table;
};
bool IsDeviceCreated()
{
    return s_Device;
}
void DeviceWaitIdle()
{
    VKIT_CHECK_EXPRESSION(s_Device.WaitIdle());
}

VmaAllocator GetVulkanAllocator()
{
    return s_VulkanAllocator;
}

VKit::DeletionQueue &GetDeletionQueue()
{
    return s_DeletionQueue;
}

} // namespace Onyx::Core
