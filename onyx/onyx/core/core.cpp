#include "onyx/core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/platform/glfw.hpp"
#include "onyx/platform/platform.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/rendering/renderer.hpp"
#ifdef ONYX_ENABLE_SHADER_API
#    include "onyx/state/shaders.hpp"
#endif
#include "onyx/state/descriptors.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/asset/assets.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/backend.hpp"
#endif

#include "vkit/memory/allocator.hpp"

#include "tkit/container/stack_array.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/utils/debug.hpp"
#ifdef ONYX_FONTCONFIG
#    include <fontconfig/fontconfig.h>
#endif
#include <fstream>
#include <filesystem>

using namespace Onyx::Detail;
// TODO(Isma): Better error handling. if a function cannot keep a sane state after an error, it should not return an
// error
namespace Onyx
{
static u8 s_PushedAlloc = 0;
static TKit::FixedArray<VKit::Allocation, TKit::MaxThreads> s_Allocation{};

static TKit::ITaskManager *s_TaskManager;
static TKit::Storage<TKit::TaskManager> s_DefaultTaskManager;
static TKit::Storage<TKit::ThreadPool> s_DefaultThreadPool;

static TKit::Storage<VKit::Instance> s_Instance{};
static TKit::Storage<VKit::PhysicalDevice> s_Physical{};
static TKit::Storage<VKit::LogicalDevice> s_Device{};
static TKit::Storage<VKit::DeletionQueue> s_DeletionQueue{};

static TKit::FixedArray<u32, VKit::Queue_Count> s_QueueRequests;

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;
static TKit::Storage<InitCallbacks> s_Callbacks{};
static const char *s_DumpPath;

#define PUSH_DELETER(code) s_DeletionQueue->Push([=] { code; })
#define SUBMIT_DELETION(object) s_DeletionQueue->SubmitForDeletion(object)

static void createDevice(const TKit::FixedArray<u32, VKit::Queue_Count> &queueRequests, const InitializationFlags flags)
{
    for (u32 i = 0; i < VKit::Queue_Count; ++i)
    {
        TKIT_LOG_WARNING_IF(queueRequests[i] > 1,
                            "[ONYX][CORE] Requesting more than one queue per type can expose instabilities in poorly "
                            "supported platforms, specially when using multiple windows");
        TKIT_ASSERT(i == VKit::Queue_Compute || queueRequests[i] != 0,
                    "[ONYX][CORE] The queue request count for all queues must be at least 1 except for compute queues, "
                    "which are not directly used by this framework");
    }

    s_QueueRequests = queueRequests;
    TKIT_LOG_INFO("[ONYX][CORE] Initializing Vulkit");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *dummy = glfwCreateWindow(120, 120, "Eduardo", nullptr, nullptr);
    TKIT_ASSERT(dummy, "[ONYX][CORE] Failed to create a dummy window so choose a device based on surface capabilities");

    VkSurfaceKHR surface;
    ONYX_CHECK_EXPRESSION(glfwCreateWindowSurface(*s_Instance, dummy, nullptr, &surface));
    VKit::PhysicalDevice::Selector selector(s_Instance.Get());

    selector.SetSurface(surface)
        .PreferType(VKit::Device_Discrete)
        .AddFlags(VKit::DeviceSelectorFlag_AnyType | VKit::DeviceSelectorFlag_PortabilitySubset |
                  VKit::DeviceSelectorFlag_RequireGraphicsQueue | VKit::DeviceSelectorFlag_RequirePresentQueue |
                  VKit::DeviceSelectorFlag_RequireTransferQueue)
        .RequireExtension("VK_KHR_dynamic_rendering")
        .RequireExtension("VK_KHR_timeline_semaphore")
        .RequireExtension("VK_KHR_synchronization2")
        .RequireExtension("VK_KHR_copy_commands2")
        .RequireExtension("VK_KHR_image_format_list")
        .RequireExtension("VK_EXT_descriptor_indexing")
        .RequireApiVersion(1, 2, 0)
        .RequestApiVersion(1, 4, 0);
    if (flags & InitializationFlag_EnableDeviceFaultExtension)
        selector.RequestExtension("VK_EXT_device_fault");

    if (s_Callbacks->OnPhysicalDeviceCreation)
        s_Callbacks->OnPhysicalDeviceCreation(selector);

    *s_Physical = ONYX_CHECK_EXPRESSION(selector.Select());

    s_Instance->GetInfo().Table->DestroySurfaceKHR(*s_Instance, surface, nullptr);
    glfwDestroyWindow(dummy);

    TKIT_LOG_INFO("[ONYX][CORE] Selected vulkan device: {}. API version: {}.{}.{}",
                  s_Physical->GetInfo().Properties.Core.deviceName,
                  VKIT_EXPAND_VERSION(s_Physical->GetInfo().ApiVersion));

    TKIT_LOG_WARNING_IF(!(s_Physical->GetInfo().Flags & VKit::DeviceFlag_Optimal),
                        "[ONYX][CORE] The device is suitable, but not optimal");

    TKIT_LOG_INFO("[ONYX][CORE] The following device extensions were enabled");
#ifdef TKIT_ENABLE_INFO_LOGS
    for (const std::string &ext : s_Physical->GetInfo().EnabledExtensions)
        TKIT_LOG_INFO("[ONYX][CORE]     {}", ext);
#endif

    TKIT_LOG_ERROR_IF((flags & InitializationFlag_EnableDeviceFaultExtension) &&
                          !s_Physical->IsExtensionEnabled("VK_EXT_device_fault"),
                      "[ONYX][CORE] The device fault extension (VK_EXT_device_fault) could not be enabled");

    TKIT_LOG_INFO_IF(s_Physical->GetInfo().Flags & VKit::DeviceFlag_Optimal, "[ONYX][CORE] The device is optimal");

    const u32 apiVersion = s_Physical->GetInfo().ApiVersion;

    VkPhysicalDeviceFaultFeaturesEXT faultFeatures{};
    if ((flags & InitializationFlag_EnableDeviceFaultExtension) &&
        s_Physical->IsExtensionEnabled("VK_EXT_device_fault"))
    {
        faultFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;

        const auto table = GetInstanceTable();
        VkPhysicalDeviceFeatures2KHR features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
        features2.pNext = &faultFeatures;
        table->GetPhysicalDeviceFeatures2KHR(*s_Physical, &features2);

        TKIT_LOG_WARNING_IF(!faultFeatures.deviceFaultVendorBinary,
                            "[ONYX][CORE] The 'deviceFaultVendorBinary' feature is not supported");
        TKIT_LOG_ERROR_IF(!faultFeatures.deviceFault, "[ONYX][CORE] The 'deviceFault' feature is not supported. The "
                                                      "extension 'VK_EXT_device_fault' is virtually useless");
        s_Physical->EnableExtensionBoundFeature(&faultFeatures);
    }

    VkPhysicalDeviceShaderDrawParameterFeatures drawParams{};
    drawParams.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;

    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR tsem{};
    tsem.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
    tsem.timelineSemaphore = VK_TRUE;

    VKit::DeviceFeatures features{};
    features.Core.independentBlend = VK_TRUE;
    features.Core.drawIndirectFirstInstance = VK_TRUE;
    features.Core.multiDrawIndirect = VK_TRUE;
    features.Vulkan11.shaderDrawParameters = VK_TRUE;
    features.Vulkan12.timelineSemaphore = VK_TRUE;
    features.Vulkan12.descriptorBindingPartiallyBound = VK_TRUE;
    features.Vulkan12.runtimeDescriptorArray = VK_TRUE;
    features.Vulkan12.descriptorIndexing = VK_TRUE;
    features.Vulkan12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    // features.Vulkan12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features.Vulkan12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR drendering{};
    drendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    drendering.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    sync2.synchronization2 = VK_TRUE;

    if (apiVersion >= VKIT_API_VERSION_1_3)
    {
        features.Vulkan13.dynamicRendering = VK_TRUE;
        features.Vulkan13.synchronization2 = VK_TRUE;
        TKIT_ASSERT(s_Physical->EnableFeatures(features),
                    "[ONYX][CORE] Failed to enable dynamic rendering and synchronization2");
    }
    else
    {
        s_Physical->EnableExtensionBoundFeature(&drendering);
        s_Physical->EnableExtensionBoundFeature(&sync2);
    }

    TKIT_ASSERT(s_Physical->EnableFeatures(features),
                "[ONYX][CORE] Failed to enable timeline semaphores and shader draw parameters");

    VKit::LogicalDevice::Builder builder{s_Instance.Get(), s_Physical.Get()};
    builder.RequireQueue(VKit::Queue_Graphics)
        .RequestQueue(VKit::Queue_Present, s_QueueRequests[VKit::Queue_Present])
        .RequestQueue(VKit::Queue_Transfer, s_QueueRequests[VKit::Queue_Transfer])
        .RequestQueue(VKit::Queue_Compute, s_QueueRequests[VKit::Queue_Compute])
        .RequestQueue(VKit::Queue_Graphics, s_QueueRequests[VKit::Queue_Graphics] - 1);

    if (s_Callbacks->OnLogicalDeviceCreation)
        s_Callbacks->OnLogicalDeviceCreation(builder);

    *s_Device = ONYX_CHECK_EXPRESSION(builder.Build());

    SUBMIT_DELETION(*s_Device);
    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_EXPRESSION(s_Device->SetName("onyx-device"));
    }
}

static void createVulkanAllocator()
{
    TKIT_LOG_INFO("[ONYX][CORE] Creating vulkan allocator");
    VKit::AllocatorSpecs specs{};
    if (s_Callbacks->OnAllocatorCreation)
        s_Callbacks->OnAllocatorCreation(specs);

    s_VulkanAllocator = ONYX_CHECK_EXPRESSION(VKit::CreateAllocator(*s_Device));

    PUSH_DELETER(VKit::DestroyAllocator(s_VulkanAllocator));
}

static void createInstance(InitializationFlags flags)
{
    TKIT_LOG_INFO("[ONYX][CORE] Creating vulkan instance");
    u32 extensionCount;

    VKit::Instance::Builder builder{};

    builder.SetApplicationName("Onyx")
        .RequestApiVersion(1, 4, 0)
        .RequireApiVersion(1, 2, 0)
        .SetApplicationVersion(1, 2, 0)
        .RequestExtension("VK_KHR_get_physical_device_properties2")
        .RequestExtension("VK_KHR_portability_enumeration");

    // TODO(Isma): Add extension for debug printf
    const InitializationFlags debugFeatFlags =
        InitializationFlag_EnableDeviceAssistedDebugFeature | InitializationFlag_EnableBestPracticesDebugFeature |
        InitializationFlag_EnableSyncValidationDebugFeature | InitializationFlag_EnablePrintfDebugFeature;

    const InitializationFlags validationFlags = debugFeatFlags | InitializationFlag_EnableDebugUtilsExtension;

    if (flags & validationFlags)
        flags |= InitializationFlag_EnableValidationLayers;

    if (flags & InitializationFlag_EnableValidationLayers)
    {
        TKIT_LOG_INFO("[ONYX][CORE] Validation layers (VK_LAYER_KHRONOS_validation) have been requested");
        builder.RequestLayer("VK_LAYER_KHRONOS_validation");
    }
    if (flags & InitializationFlag_EnableDebugUtilsExtension)
    {
        TKIT_LOG_INFO("[ONYX][CORE] Debug utils extension (VK_EXT_debug_utils) has been requested");
        builder.RequestExtension("VK_EXT_debug_utils");
    }

    if (flags & debugFeatFlags)
        builder.RequestExtension("VK_EXT_validation_features");

    if (flags & InitializationFlag_EnableDeviceAssistedDebugFeature)
    {
        TKIT_LOG_INFO("[ONYX][CORE] Device assisted debug feature has been requested");
        builder.SetValidationFeature(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
        builder.SetValidationFeature(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
    }
    if (flags & InitializationFlag_EnableBestPracticesDebugFeature)
    {
        TKIT_LOG_INFO("[ONYX][CORE] Best practices debug feature has been requested");
        builder.SetValidationFeature(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
    }
    if (flags & InitializationFlag_EnablePrintfDebugFeature)
    {
        TKIT_LOG_INFO("[ONYX][CORE] Printf debug feature has been requested");
        builder.SetValidationFeature(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
    }
    if (flags & InitializationFlag_EnableSyncValidationDebugFeature)
    {
        TKIT_LOG_INFO("[ONYX][CORE] Sync validation debug feature has been requested");
        builder.SetValidationFeature(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
    }

    const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    const TKit::Span<const char *const> extensionSpan(extensions, extensionCount);
    for (u32 i = 0; i < extensionCount; ++i)
        builder.RequireExtension(extensions[i]);

    if (s_Callbacks->OnInstanceCreation)
        s_Callbacks->OnInstanceCreation(builder);

    *s_Instance = ONYX_CHECK_EXPRESSION(builder.Build());
    const VKit::Instance::Info &info = s_Instance->GetInfo();
    TKIT_LOG_INFO("[ONYX][CORE] Created vulkan instance. API version: {}.{}.{}", VKIT_EXPAND_VERSION(info.ApiVersion));

    TKIT_LOG_INFO("[ONYX][CORE] The following instance layers were enabled");
#ifdef TKIT_ENABLE_INFO_LOGS
    for (const char *layer : info.EnabledLayers)
        TKIT_LOG_INFO("[ONYX][CORE]     {}", layer);
#endif

    TKIT_LOG_INFO("[ONYX][CORE] The following instance extensions were enabled");
#ifdef TKIT_ENABLE_INFO_LOGS
    for (const char *ext : info.EnabledExtensions)
        TKIT_LOG_INFO("[ONYX][CORE]     {}", ext);
#endif

    TKIT_LOG_ERROR_IF((flags & InitializationFlag_EnableValidationLayers) &&
                          !s_Instance->IsLayerEnabled("VK_LAYER_KHRONOS_validation"),
                      "[ONYX][CORE] Validation layers (VK_LAYER_KHRONOS_validation) could not be enabled");
    TKIT_LOG_ERROR_IF((flags & InitializationFlag_EnableDebugUtilsExtension) &&
                          !s_Instance->IsExtensionEnabled("VK_EXT_debug_utils"),
                      "[ONYX][CORE] Debug utils extension (VK_EXT_debug_utils) could not be enabled");
    TKIT_LOG_ERROR_IF((flags & debugFeatFlags) && !s_Instance->IsExtensionEnabled("VK_EXT_validation_features"),
                      "[ONYX][CORE] Validation features extension (VK_EXT_validation_features) could not be enabled");
    SUBMIT_DELETION(*s_Instance);
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
            libAlloc.Arena = new TKit::ArenaAllocator(i == 0 ? 1_mib : 16_kib, TKIT_CACHE_LINE_SIZE);
        if (userAlloc.Stack)
            libAlloc.Stack = userAlloc.Stack;
        else if (!libAlloc.Stack)
            libAlloc.Stack = new TKit::StackAllocator(i == 0 ? 1_mib : 4_kib, TKIT_CACHE_LINE_SIZE);

        if (userAlloc.Tier)
            libAlloc.Tier = userAlloc.Tier;
        else if (!libAlloc.Tier)
        {
            const TKit::TierDescriptions desc{{.Allocator = libAlloc.Arena,
                                               .MaxAllocation = i == 0 ? 1_mib : 4_kib,
                                               .TierSlotDecay = i == 0 ? 0.8f : 0.85f}};
            if (i == 0)
            {
                TKIT_LOG_INFO("[ONYX][CORE] Tier allocator for the main thread has allocated {:L} bytes of memory",
                              desc.GetBufferSize());
            }
            else
            {
                TKIT_LOG_INFO("[ONYX][CORE] Tier allocators for each of the secondary threads have allocated {:L} "
                              "bytes of memory",
                              desc.GetBufferSize());
            }
            libAlloc.Tier = new TKit::TierAllocator(desc);
        }
    }
    VKit::Allocation &libAlloc = s_Allocation[0];
    if (TKit::GetArena() != libAlloc.Arena)
    {
        TKit::PushArena(libAlloc.Arena);
        s_PushedAlloc |= 1 << 0;
    }
    if (TKit::GetStack() != libAlloc.Stack)
    {
        TKit::PushStack(libAlloc.Stack);
        s_PushedAlloc |= 1 << 1;
    }
    if (TKit::GetTier() != libAlloc.Tier)
    {
        TKit::PushTier(libAlloc.Tier);
        s_PushedAlloc |= 1 << 2;
    }
}

static void terminateAllocators(const bool resetArenas)
{
    if (resetArenas)
        for (const VKit::Allocation &alloc : s_Allocation)
            alloc.Arena->Reset();

    if (s_PushedAlloc & 4)
        TKit::PopTier();
    if (s_PushedAlloc & 2)
        TKit::PopStack();
    if (s_PushedAlloc & 1)
        TKit::PopArena();
}

TKit::ITaskManager *GetTaskManager()
{
    return s_TaskManager;
}

const VKit::Instance &GetInstance()
{
    TKIT_ASSERT(*s_Instance, "[ONYX][CORE] Vulkan instance is not initialized! Forgot to call Onyx::Initialize?");
    return *s_Instance;
}
const VKit::Vulkan::InstanceTable *GetInstanceTable()
{
    TKIT_ASSERT(*s_Instance, "[ONYX][CORE] Vulkan instance is not initialized! Forgot to call Onyx::Initialize?");
    return s_Instance->GetInfo().Table;
};
const VKit::LogicalDevice &GetDevice()
{
    TKIT_ASSERT(*s_Device, "[ONYX][CORE] Vulkan device is not initialized! Forgot to call Onyx::Initialize?");
    return *s_Device;
}
const VKit::Vulkan::DeviceTable *GetDeviceTable()
{
    TKIT_ASSERT(*s_Device, "[ONYX][CORE] Vulkan device is not initialized! Forgot to call Onyx::Initialize?");
    return s_Device->GetInfo().Table;
};
bool IsDebugUtilsEnabled()
{
    return s_Instance->IsExtensionEnabled("VK_EXT_debug_utils");
}
void DeviceWaitIdle()
{
    TKIT_BEGIN_DEBUG_CLOCK();
    TKIT_ASSERT(*s_Device, "[ONYX][CORE] Vulkan device is not initialized! Forgot to call Onyx::Initialize?");
    ONYX_CHECK_EXPRESSION(s_Device->WaitIdle());
    TKIT_END_DEBUG_CLOCK(Milliseconds, "[ONYX][CORE] Waiting for device took {:.2f} milliseconds");
    return Execution::UpdateCompletedQueueTimelines();
}

VmaAllocator GetVulkanAllocator()
{
    TKIT_ASSERT(s_VulkanAllocator,
                "[ONYX][CORE] Vulkan allocator is not initialized! Forgot to call Onyx::Initialize?");
    return s_VulkanAllocator;
}

VKit::DeletionQueue &GetDeletionQueue()
{
    return *s_DeletionQueue;
}

static const char *toString(const VkDeviceFaultAddressTypeEXT faultType)
{
    switch (faultType)
    {
    case VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT:
        return "NONE";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT:
        return "READ_INVALID";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT:
        return "WRITE_INVALID";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT:
        return "EXECUTE_INVALID";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT:
        return "INSTRUCTION_POINTER_UNKNOWN";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT:
        return "INSTRUCTION_POINTER_INVALID";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT:
        return "INSTRUCTION_POINTER_FAULT";
    default:
        return "UNKNOWN";
    }
}

void HandleVulkanResult(const VkResult result)
{
#ifdef TKIT_ENABLE_ERROR_LOGS
    if (result != VK_ERROR_DEVICE_LOST)
        return;
    if (!s_Physical->IsExtensionEnabled("VK_EXT_device_fault"))
    {
        TKIT_LOG_ERROR("[ONYX][CORE] A device lost error was encountered, but could not study it further because the "
                       "'VK_EXT_device_fault' extension was not enabled");
        return;
    }

    VkDeviceFaultCountsEXT counts{};
    counts.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT;
    const auto &device = GetDevice();
    const auto table = GetDeviceTable();

    ONYX_CHECK_EXPRESSION(table->GetDeviceFaultInfoEXT(device, &counts, nullptr));

    TKit::StackArray<VkDeviceFaultAddressInfoEXT> addresses{};
    TKit::StackArray<VkDeviceFaultVendorInfoEXT> vendors{};
    TKit::StackArray<std::byte> vendorBinary{};

    addresses.Resize(counts.addressInfoCount);
    vendors.Resize(counts.vendorInfoCount);
    vendorBinary.Resize(u32(counts.vendorBinarySize));

    VkDeviceFaultInfoEXT faultInfo{};
    faultInfo.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT;
    faultInfo.pAddressInfos = addresses.GetData();
    faultInfo.pVendorInfos = vendors.GetData();
    faultInfo.pVendorBinaryData = vendorBinary.GetData();

    ONYX_CHECK_EXPRESSION(table->GetDeviceFaultInfoEXT(device, &counts, &faultInfo));

    TKIT_LOG_ERROR("[ONYX][CORE] Device fault description: {}", faultInfo.description);

    for (u32 i = 0; i < counts.addressInfoCount; ++i)
    {
        const VkDeviceFaultAddressInfoEXT &info = addresses[i];
        TKIT_LOG_ERROR("[ONYX][CORE]    Address[{}]: {}", i, toString(info.addressType));
        TKIT_LOG_ERROR("[ONYX][CORE]        addr={:#18X}", info.reportedAddress);
        TKIT_LOG_ERROR("[ONYX][CORE]        precision={}", info.addressPrecision);

        if (info.addressPrecision > 1)
        {
            const u64 lo = info.reportedAddress & ~(info.addressPrecision - 1);
            const u64 hi = lo + info.addressPrecision;
            TKIT_LOG_ERROR("        Actual address in range [{:#18X}, {:#18X}]", lo, hi);
        }
    }

    for (u32 i = 0; i < counts.vendorInfoCount; ++i)
    {
        const VkDeviceFaultVendorInfoEXT &info = vendors[i];
        TKIT_LOG_ERROR("[ONYX][CORE]    Vendor[{}]: {}", i, info.description);
        TKIT_LOG_ERROR("[ONYX][CORE]        faultcode={:#18X}", info.vendorFaultCode);
        TKIT_LOG_ERROR("[ONYX][CORE]        faultdata={:#18X}", info.vendorFaultData);
    }
    if (counts.vendorBinarySize == 0)
        return;

    if (vendorBinary.GetSize() < sizeof(VkDeviceFaultVendorBinaryHeaderVersionOneEXT))
    {
        TKIT_LOG_ERROR("[ONYX][CORE] Cannot retrieve vendor binary data: It is too small to be contained in the vulkan "
                       "header version one");
        return;
    }

    const VkDeviceFaultVendorBinaryHeaderVersionOneEXT *header =
        reinterpret_cast<const VkDeviceFaultVendorBinaryHeaderVersionOneEXT *>(vendorBinary.GetData());
#endif

    TKIT_LOG_ERROR("[ONYX][CORE] Vendor binary ({:L} bytes)", vendorBinary.GetSize());
    TKIT_LOG_ERROR("[ONYX][CORE]    vendor={:#010x}", header->vendorID);
    TKIT_LOG_ERROR("[ONYX][CORE]    device={:#010x}", header->deviceID);

    namespace fs = std::filesystem;
    const auto path = s_DumpPath ? fs::path(s_DumpPath) : fs::temp_directory_path();
    std::ofstream f{path, std::ios::binary};
    if (!f)
    {
        TKIT_LOG_ERROR("[ONYX][CORE] Failed to open file at '{}' to write device fault dump", path.string());
        return;
    }
    f.write(reinterpret_cast<const char *>(vendorBinary.GetData()), std::streamsize(vendorBinary.GetSize()));

    TKIT_LOG_ERROR("[ONYX][CORE] Wrote crash dump to '{}'", path.string());
}

void Initialize(const Specs &specs)
{
    TKIT_BEGIN_INFO_CLOCK();
    TKIT_LOG_INFO("[ONYX][CORE] Initializing");
    TKIT_LOG_INFO("[ONYX][CORE] Vulkan headers version: {}.{}.{}", VKIT_EXPAND_VERSION(VK_HEADER_VERSION_COMPLETE));
    if (specs.Locale)
        std::locale::global(std::locale(specs.Locale));
    s_DumpPath = specs.DeviceFaultCrashDump;
    s_Instance.Construct();
    s_Physical.Construct();
    s_Device.Construct();
    s_DeletionQueue.Construct();
    s_Callbacks.Construct(specs.Callbacks);

    initializeAllocators(specs);
    PUSH_DELETER(terminateAllocators(specs.Flags & InitializationFlag_ResetArenasOnTermination));
#ifdef ONYX_FONTCONFIG
    FcInit();
#endif

    VKit::Specs vspecs{};
    vspecs.Allocators = s_Allocation[0];
    vspecs.LoaderPath = "/nix/store/b1bldnpjpys7np3361plhp2wxcaw9iwr-vulkan-loader-1.4.328.0/lib/libvulkan.so";
    // vspecs.LoaderPath = specs.VulkanLoaderPath;

    PUSH_DELETER(VKit::Terminate());
    ONYX_CHECK_EXPRESSION(VKit::Initialize(vspecs));

    if (specs.TaskManager)
        s_TaskManager = specs.TaskManager;
    else if (specs.Flags & InitializationFlag_DefaultTaskManagerSingleThread)
    {
        s_DefaultTaskManager.Construct();
        s_TaskManager = s_DefaultTaskManager.Get();
        PUSH_DELETER(s_DefaultTaskManager.Destruct());
    }
    else
    {
        s_DefaultThreadPool.Construct(TKit::MaxThreads - 1);
        s_TaskManager = s_DefaultThreadPool.Get();
        PUSH_DELETER(s_DefaultThreadPool.Destruct());
    }

    PUSH_DELETER(Platform::Terminate());
    Platform::Initialize(specs.PlatformSpecs ? *specs.PlatformSpecs : Platform::Specs{});

    createInstance(specs.Flags);
    createDevice(specs.QueueRequests, specs.Flags);
    createVulkanAllocator();

    PUSH_DELETER(Platform::DestroyWindows());
    PUSH_DELETER(Execution::Terminate());
    Execution::Initialize(specs.ExecutionSpecs ? *specs.ExecutionSpecs : Execution::Specs{});

    PUSH_DELETER(Descriptors::Terminate());
    Descriptors::Initialize(specs.DescriptorSpecs ? *specs.DescriptorSpecs : Descriptors::Specs{});

#ifdef ONYX_ENABLE_SHADER_API
    PUSH_DELETER(Shaders::Terminate());
    Shaders::Initialize(specs.ShadersSpecs ? *specs.ShadersSpecs : Shaders::Specs{});
#endif

    PUSH_DELETER(Pipelines::Terminate());
    Pipelines::Initialize();

    PUSH_DELETER(Renderer::Terminate());
    Renderer::Initialize(specs.RendererSpecs ? *specs.RendererSpecs : Renderer::Specs{});

    PUSH_DELETER(Assets::Terminate());
    Assets::Initialize(specs.AssetSpecs ? *specs.AssetSpecs : Assets::Specs{});

#ifdef ONYX_ENABLE_IMGUI
    PUSH_DELETER(ImGuiBackend::Terminate());
    ImGuiBackend::Initialize();
#endif

    TKIT_END_INFO_CLOCK(Seconds, "[ONYX][CORE] Done in {:.2f} seconds");
}

void Terminate()
{
    if (*s_Device)
        DeviceWaitIdle();

    s_Callbacks.Destruct();
    s_DeletionQueue.Destruct();
    s_Device.Destruct();
    s_Physical.Destruct();
    s_Instance.Destruct();
}

TKit::ArenaAllocator *GetArena(const u32 threadIndex)
{
    return s_Allocation[threadIndex].Arena;
}
TKit::StackAllocator *GetStack(const u32 threadIndex)
{
    return s_Allocation[threadIndex].Stack;
}
TKit::TierAllocator *GetTier(const u32 threadIndex)
{
    return s_Allocation[threadIndex].Tier;
}

void PushArena(const u32 threadIndex)
{
    TKit::PushArena(s_Allocation[threadIndex].Arena);
}
void PushStack(const u32 threadIndex)
{
    TKit::PushStack(s_Allocation[threadIndex].Stack);
}
void PushTier(const u32 threadIndex)
{
    TKit::PushTier(s_Allocation[threadIndex].Tier);
}
} // namespace Onyx
