#include "onyx/core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/core/glfw.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/asset/assets.hpp"

#include "vkit/memory/allocator.hpp"

#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/utils/debug.hpp"
#ifdef ONYX_FONTCONFIG
#    include <fontconfig/fontconfig.h>
#endif

using namespace Onyx::Detail;
namespace Onyx::Core
{
static u8 s_PushedAlloc = 0;
static TKit::FixedArray<VKit::Allocation, TKit::MaxThreads> s_Allocation{};

static TKit::ITaskManager *s_TaskManager;
static TKit::Storage<TKit::ThreadPool> s_DefaultTaskManager;

static VKit::Instance s_Instance{};
static VKit::PhysicalDevice s_Physical{};
static VKit::LogicalDevice s_Device{};

static VKit::DeletionQueue s_DeletionQueue{};

static TKit::FixedArray<u32, VKit::Queue_Count> s_QueueRequests;

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;
static InitCallbacks s_Callbacks{};

#define PUSH_DELETER(code) s_DeletionQueue.Push([] { code; })
#define SUBMIT_DELETION(object) s_DeletionQueue.SubmitForDeletion(object)

ONYX_NO_DISCARD static Result<> createDevice(const TKit::FixedArray<u32, VKit::Queue_Count> &queueRequests)
{
    for (u32 i = 0; i < VKit::Queue_Count; ++i)
        if (i != VKit::Queue_Compute && queueRequests[i] == 0)
            return Result<>::Error(
                Error_BadInput,
                "[ONYX][CORE] The queue request count for all queues must be at least 1 except for compute queues, "
                "which are not directly used by this framework");

    s_QueueRequests = queueRequests;
    TKIT_LOG_INFO("[ONYX][CORE] Initializing Vulkit");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *dummy = glfwCreateWindow(120, 120, "Eduardo", nullptr, nullptr);
    if (!dummy)
        return Result<>::Error(
            Error_RejectedWindow,
            "[ONYX][CORE] Failed to create a dummy window so choose a device based on surface capabilities");

    VkSurfaceKHR surface;
    const VkResult sresult = glfwCreateWindowSurface(s_Instance, dummy, nullptr, &surface);
    if (sresult != VK_SUCCESS)
    {
        glfwDestroyWindow(dummy);
        return Result<>::Error(sresult);
    }
    VKit::PhysicalDevice::Selector selector(&s_Instance);

    selector.SetSurface(surface)
        .PreferType(VKit::Device_Discrete)
        .AddFlags(VKit::DeviceSelectorFlag_AnyType | VKit::DeviceSelectorFlag_PortabilitySubset |
                  VKit::DeviceSelectorFlag_RequireGraphicsQueue | VKit::DeviceSelectorFlag_RequirePresentQueue |
                  VKit::DeviceSelectorFlag_RequireTransferQueue)
        .RequireExtension("VK_KHR_dynamic_rendering")
        .RequireExtension("VK_KHR_timeline_semaphore")
        .RequireExtension("VK_KHR_synchronization2")
        .RequireExtension("VK_KHR_copy_commands2")
        .RequireApiVersion(1, 2, 0)
        .RequestApiVersion(1, 3, 0);

    if (s_Callbacks.OnPhysicalDeviceCreation)
        s_Callbacks.OnPhysicalDeviceCreation(selector);

    auto physres = selector.Select();

    s_Instance.GetInfo().Table->DestroySurfaceKHR(s_Instance, surface, nullptr);
    glfwDestroyWindow(dummy);

    TKIT_RETURN_ON_ERROR(physres);

    s_Physical = physres.GetValue();
    const u32 apiVersion = s_Physical.GetInfo().ApiVersion;

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
        if (!s_Physical.EnableFeatures(features))
            return Result<>::Error(Error_MissingFeature,
                                   "[ONYX][CORE] Failed to enable timeline semaphores and shader draw parameters");
    }
    else
    {
        s_Physical.EnableExtensionBoundFeature(&drawParams);
        s_Physical.EnableExtensionBoundFeature(&tsem);
    }
#else
    s_Physical.EnableExtensionBoundFeature(&drawParams);
    s_Physical.EnableExtensionBoundFeature(&tsem);
#endif

    VkPhysicalDeviceDynamicRenderingFeaturesKHR drendering{};
    drendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    drendering.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    sync2.synchronization2 = VK_TRUE;

#ifdef VKIT_API_VERSION_1_3
    if (apiVersion >= VKIT_API_VERSION_1_3)
    {
        VKit::DeviceFeatures features{};
        features.Vulkan13.dynamicRendering = VK_TRUE;
        features.Vulkan13.synchronization2 = VK_TRUE;
        if (!s_Physical.EnableFeatures(features))
            return Result<>::Error(Error_MissingFeature,
                                   "[ONYX][CORE] Failed to enable dynamic rendering and synchronization2");
    }
    else
    {
        s_Physical.EnableExtensionBoundFeature(&drendering);
        s_Physical.EnableExtensionBoundFeature(&sync2);
    }
#else
    s_Physical.EnableExtensionBoundFeature(&drendering);
    s_Physical.EnableExtensionBoundFeature(&sync2);
#endif

    VKit::LogicalDevice::Builder builder{&s_Instance, &s_Physical};
    builder.RequireQueue(VKit::Queue_Graphics)
        .RequestQueue(VKit::Queue_Present, s_QueueRequests[VKit::Queue_Present])
        .RequestQueue(VKit::Queue_Transfer, s_QueueRequests[VKit::Queue_Transfer])
        .RequestQueue(VKit::Queue_Compute, s_QueueRequests[VKit::Queue_Compute]);

    if (s_QueueRequests[VKit::Queue_Graphics] > 1)
        builder.RequestQueue(VKit::Queue_Graphics, s_QueueRequests[VKit::Queue_Graphics] - 1);

    if (s_Callbacks.OnLogicalDeviceCreation)
        s_Callbacks.OnLogicalDeviceCreation(builder);

    const auto devres = builder.Build();

    TKIT_RETURN_ON_ERROR(devres);
    s_Device = devres.GetValue();

    TKIT_LOG_INFO("[ONYX][CORE] Created vulkan device: {}",
                  s_Device.GetInfo().PhysicalDevice->GetInfo().Properties.Core.deviceName);
    TKIT_LOG_WARNING_IF(!(s_Device.GetInfo().PhysicalDevice->GetInfo().Flags & VKit::DeviceFlag_Optimal),
                        "[ONYX][CORE] The device is suitable, but not optimal");
    TKIT_LOG_INFO_IF(s_Device.GetInfo().PhysicalDevice->GetInfo().Flags & VKit::DeviceFlag_Optimal,
                     "[ONYX][CORE] The device is optimal");

    SUBMIT_DELETION(s_Device);
    return Result<>::Ok();
}

ONYX_NO_DISCARD static Result<> createVulkanAllocator()
{
    TKIT_LOG_INFO("[ONYX][CORE] Creating vulkan allocator");
    VKit::AllocatorSpecs specs{};
    if (s_Callbacks.OnAllocatorCreation)
        s_Callbacks.OnAllocatorCreation(specs);

    const auto result = VKit::CreateAllocator(s_Device);
    TKIT_RETURN_ON_ERROR(result);
    s_VulkanAllocator = result.GetValue();

    PUSH_DELETER(VKit::DestroyAllocator(s_VulkanAllocator));
    return Result<>::Ok();
}

ONYX_NO_DISCARD static Result<> createInstance(const bool validationLayers)
{
    TKIT_LOG_INFO("[ONYX][CORE] Creating vulkan instance");
    u32 extensionCount;

    VKit::Instance::Builder builder{};
    builder.SetApplicationName("Onyx").RequestApiVersion(1, 3, 0).RequireApiVersion(1, 2, 0).SetApplicationVersion(1, 2,
                                                                                                                   0);
    if (validationLayers)
        builder.RequestValidationLayers();

    const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    const TKit::Span<const char *const> extensionSpan(extensions, extensionCount);
    for (u32 i = 0; i < extensionCount; ++i)
        builder.RequireExtension(extensions[i]);
    if (s_Callbacks.OnInstanceCreation)
        s_Callbacks.OnInstanceCreation(builder);

    const auto iresult = builder.Build();
    TKIT_RETURN_ON_ERROR(iresult);

    s_Instance = iresult.GetValue();
    TKIT_LOG_INFO("[ONYX][CORE] Created vulkan instance. API version: {}.{}.{}",
                  VKIT_EXPAND_VERSION(s_Instance.GetInfo().ApiVersion));

#if defined(TKIT_ENABLE_INFO_LOGS) || defined(TKIT_ENABLE_ERROR_LOGS)
    const bool vlayers = s_Instance.GetInfo().Flags & VKit::InstanceFlag_HasValidationLayers;
#endif

    TKIT_LOG_INFO_IF(vlayers, "[ONYX][CORE] Validation layers enabled");
    TKIT_LOG_ERROR_IF(validationLayers && !vlayers,
                      "[ONYX][CORE] Validation layers were requested, but could not be enabled");
    TKIT_LOG_INFO_IF(!vlayers, "[ONYX][CORE] Validation layers disabled");
    SUBMIT_DELETION(s_Instance);
    return Result<>::Ok();
}

static void initializeAllocators(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][CORE] Initializing allocators");
    // these are purposefully leaked
    for (u32 i = 0; i < TKit::MaxThreads; ++i)
    {
        const VKit::Allocation &userAlloc = specs.Allocators[i];
        VKit::Allocation &libAlloc = s_Allocation[i];
        if (userAlloc.Arena)
            libAlloc.Arena = userAlloc.Arena;
        else if (!libAlloc.Arena)
            libAlloc.Arena = new TKit::ArenaAllocator(static_cast<u32>(i == 0 ? 1_mib : 4_kib), TKIT_CACHE_LINE_SIZE);
        if (userAlloc.Stack)
            libAlloc.Stack = userAlloc.Stack;
        else if (!libAlloc.Stack)
            libAlloc.Stack = new TKit::StackAllocator(static_cast<u32>(i == 0 ? 1_mib : 4_kib), TKIT_CACHE_LINE_SIZE);

        if (userAlloc.Tier)
            libAlloc.Tier = userAlloc.Tier;
        else if (!libAlloc.Tier)
            libAlloc.Tier = new TKit::TierAllocator(libAlloc.Arena, 64, static_cast<u32>(i == 0 ? 256_kib : 4_kib));
    }
    VKit::Allocation &libAlloc = s_Allocation[0];
    if (TKit::Memory::GetArena() != libAlloc.Arena)
    {
        TKit::Memory::PushArena(libAlloc.Arena);
        s_PushedAlloc |= 1 << 0;
    }
    if (TKit::Memory::GetStack() != libAlloc.Stack)
    {
        TKit::Memory::PushStack(libAlloc.Stack);
        s_PushedAlloc |= 1 << 1;
    }
    if (TKit::Memory::GetTier() != libAlloc.Tier)
    {
        TKit::Memory::PushTier(libAlloc.Tier);
        s_PushedAlloc |= 1 << 2;
    }
}

#ifdef TKIT_ENABLE_ERROR_LOGS
static void glfwErrorCallback(const i32 errorCode, const char *description)
{
    TKIT_LOG_ERROR("[ONYX][GLFW] An error ocurred with code {} and the following description: {}", errorCode,
                   description);
}
#endif

ONYX_NO_DISCARD static Result<> initializeGlfw(const u32 platform)
{
    TKIT_LOG_INFO("[ONYX][CORE] Initializing GLFW");
#ifdef TKIT_ENABLE_ERROR_LOGS
    glfwSetErrorCallback(glfwErrorCallback);
#endif
    glfwInitHint(GLFW_PLATFORM, platform);
    if (glfwInit() != GLFW_TRUE)
        return Result<>::Error(Error_InitializationFailed, "[ONYX][CORE] GLFW failed to initialize");

    PUSH_DELETER(glfwTerminate());
    TKIT_LOG_WARNING_IF(!glfwVulkanSupported(), "[ONYX][CORE] Vulkan is not supported, according to GLFW");
    return Result<>::Ok();
}

void terminateAllocators()
{
    if (s_PushedAlloc & 4)
        TKit::Memory::PopTier();
    if (s_PushedAlloc & 2)
        TKit::Memory::PopStack();
    if (s_PushedAlloc & 1)
        TKit::Memory::PopArena();
}

ONYX_NO_DISCARD static Result<> initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][CORE] Initializing Onyx");

    initializeAllocators(specs);
    PUSH_DELETER(terminateAllocators());
#ifdef ONYX_FONTCONFIG
    FcInit();
#endif

    VKit::Specs vspecs{};
    vspecs.Allocators = s_Allocation[0];

    PUSH_DELETER(VKit::Core::Terminate());
    TKIT_RETURN_IF_FAILED(VKit::Core::Initialize(vspecs));

    if (specs.TaskManager)
        s_TaskManager = specs.TaskManager;
    else
    {
        s_DefaultTaskManager.Construct(TKit::MaxThreads - 1);
        s_TaskManager = s_DefaultTaskManager.Get();
        PUSH_DELETER(s_DefaultTaskManager.Destruct());
    }
    s_Callbacks = specs.Callbacks;

    TKIT_RETURN_IF_FAILED(initializeGlfw(specs.Platform));
    TKIT_RETURN_IF_FAILED(createInstance(specs.EnableValidationLayers));
    TKIT_RETURN_IF_FAILED(createDevice(specs.QueueRequests));
    TKIT_RETURN_IF_FAILED(createVulkanAllocator());

    PUSH_DELETER(Execution::Terminate());
    TKIT_RETURN_IF_FAILED(Execution::Initialize(specs.ExecutionSpecs ? *specs.ExecutionSpecs : Execution::Specs{}));

    PUSH_DELETER(Assets::Terminate());
    TKIT_RETURN_IF_FAILED(Assets::Initialize(specs.AssetSpecs ? *specs.AssetSpecs : Assets::Specs{}));

    PUSH_DELETER(Descriptors::Terminate());
    TKIT_RETURN_IF_FAILED(
        Descriptors::Initialize(specs.DescriptorSpecs ? *specs.DescriptorSpecs : Descriptors::Specs{}));

    PUSH_DELETER(Shaders::Terminate());
    TKIT_RETURN_IF_FAILED(Shaders::Initialize(specs.ShadersSpecs ? *specs.ShadersSpecs : Shaders::Specs{}));

    PUSH_DELETER(Pipelines::Terminate());
    TKIT_RETURN_IF_FAILED(Pipelines::Initialize());

    PUSH_DELETER(Renderer::Terminate());
    TKIT_RETURN_IF_FAILED(Renderer::Initialize());

    return Result<>::Ok();
}

Result<> Initialize(const Specs &specs)
{
    const auto result = initialize(specs);
    if (!result)
        Terminate();
    return result;
}

void Terminate()
{
    if (s_Device)
        ONYX_CHECK_EXPRESSION(DeviceWaitIdle());
    s_DeletionQueue.Flush();
}

TKit::ITaskManager *GetTaskManager()
{
    return s_TaskManager;
}
void SetTaskManager(TKit::ITaskManager *taskManager)
{
    s_TaskManager = taskManager;
}

const VKit::Instance &GetInstance()
{
    TKIT_ASSERT(s_Instance, "[ONYX][CORE] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance;
}
const VKit::Vulkan::InstanceTable *GetInstanceTable()
{
    TKIT_ASSERT(s_Instance, "[ONYX][CORE] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance.GetInfo().Table;
};
const VKit::LogicalDevice &GetDevice()
{
    TKIT_ASSERT(s_Device, "[ONYX][CORE] Vulkan device is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Device;
}
const VKit::Vulkan::DeviceTable *GetDeviceTable()
{
    TKIT_ASSERT(s_Device, "[ONYX][CORE] Vulkan device is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Device.GetInfo().Table;
};
Result<> DeviceWaitIdle()
{
    TKIT_ASSERT(s_Device, "[ONYX][CORE] Vulkan device is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Device.WaitIdle();
}

VmaAllocator GetVulkanAllocator()
{
    TKIT_ASSERT(s_VulkanAllocator,
                "[ONYX][CORE] Vulkan allocator is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_VulkanAllocator;
}

VKit::DeletionQueue &GetDeletionQueue()
{
    return s_DeletionQueue;
}

} // namespace Onyx::Core
