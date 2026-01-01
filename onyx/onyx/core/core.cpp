#include "onyx/core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/core/glfw.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/state/pipelines.hpp"

#include "vkit/core/core.hpp"
#include "vkit/memory/allocator.hpp"
#include "vkit/vulkan/vulkan.hpp"

#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/memory/block_allocator.hpp"
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

static VKit::CommandPool s_GraphicsPool{};
static VKit::CommandPool s_TransferPool{};

static TKit::FixedArray4<u32> s_QueueRequests;
static TKit::FixedArray4<TKit::Array64<QueueHandle *>> s_Queues;

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;
static InitCallbacks s_Callbacks{};

static TKit::BlockAllocator s_QueueAllocator = TKit::BlockAllocator::CreateFromType<QueueHandle>(128);

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

    VKIT_ASSERT_RESULT(devres);
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
    VKIT_ASSERT_RESULT(result);
    s_VulkanAllocator = result.GetValue();
    TKIT_LOG_INFO("[ONYX] Creating Vulkan allocator");

    s_DeletionQueue.Push([] { VKit::DestroyAllocator(s_VulkanAllocator); });
}

static void createCommandPool()
{
    TKIT_LOG_INFO("[ONYX] Creating global command pools");
    const u32 gindex = GetFamilyIndex(VKit::Queue_Graphics);
    const u32 tindex = GetFamilyIndex(VKit::Queue_Transfer);
    auto poolres = VKit::CommandPool::Create(
        s_Device, gindex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    VKIT_ASSERT_RESULT(poolres);
    s_GraphicsPool = poolres.GetValue();
    s_DeletionQueue.SubmitForDeletion(s_GraphicsPool);
    if (gindex != tindex)
    {
        poolres = VKit::CommandPool::Create(
            s_Device, tindex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        VKIT_ASSERT_RESULT(poolres);
        s_TransferPool = poolres.GetValue();
        s_DeletionQueue.SubmitForDeletion(s_TransferPool);
    }
    else
        s_TransferPool = s_GraphicsPool;
}

static void retrieveDeviceQueues()
{
    const VKit::PhysicalDevice &phys = s_Device.GetInfo().PhysicalDevice;
    const u32 maxCount = *std::max_element(s_QueueRequests.begin(), s_QueueRequests.end());
    for (u32 i = 0; i < maxCount; ++i)
    {
        TKit::FixedArray4<QueueHandle *> handles{nullptr, nullptr, nullptr, nullptr};

        for (u32 j = 0; j < 4; ++j)
        {
            const VKit::QueueType qtype = static_cast<VKit::QueueType>(j);
            const u32 findex = phys.GetInfo().FamilyIndices[qtype];
            const auto res = s_Device.GetQueue(findex, i);
            if (!res)
                continue;
            const VkQueue queue = res.GetValue();
            QueueHandle *qh = nullptr;
            for (u32 k = 0; k < j; ++k)
                if (handles[k] && handles[k]->Queue == queue)
                {
                    qh = handles[k];
                    break;
                }

            if (!qh)
            {
                qh = s_QueueAllocator.Create<QueueHandle>();
                s_DeletionQueue.Push([qh] { s_QueueAllocator.Destroy(qh); });

                qh->Queue = queue;
                qh->Index = i;
                qh->FamilyIndex = findex;
                qh->UsageCount = 0;
#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
                if (qtype == VKit::Queue_Graphics)
                {
                    const auto cmdres = s_GraphicsPool.Allocate();
                    VKIT_ASSERT_RESULT(cmdres);
                    VkCommandBuffer cmd = cmdres.GetValue();

                    qh->ProfilingContext = TKIT_PROFILE_CREATE_VULKAN_CONTEXT(
                        s_Instance, phys, s_Device, qh->Queue, cmd, VKit::Vulkan::vkGetInstanceProcAddr,
                        s_Instance.GetInfo().Table.vkGetDeviceProcAddr);

                    s_DeletionQueue.Push([qh] { TKIT_PROFILE_DESTROY_VULKAN_CONTEXT(qh->ProfilingContext); });
                }
#endif
                handles[j] = qh;
                TKIT_LOG_DEBUG("[ONYX] Retrieved {} queue <{}, {}>", VKit::ToString(qtype), findex, i);
            }
            s_Queues[j].Append(qh);
        }
    }

#if defined(TKIT_ENABLE_INFO_LOGS) || defined(TKIT_ENABLE_WARNING_LOGS)
    for (u32 i = 0; i < 4; ++i)
    {
        const char *name = VKit::ToString(static_cast<VKit::QueueType>(i));
        TKIT_LOG_INFO("[ONYX] {} family index: {}", name, phys.GetInfo().FamilyIndices[i]);

        const u32 count = s_QueueRequests[i];
        TKIT_LOG_INFO_IF(count > 0 && count == s_Queues[i].GetSize(), "[ONYX] Successfully retrieved {} {} queues",
                         count, name);
        TKIT_LOG_WARNING_IF(count > 0 && count > s_Queues[i].GetSize(),
                            "[ONYX] Could not retrieve {} {} queues. Only managed to obtain {}", count, name,
                            s_Queues[i].GetSize());
    }
#endif
}

void Initialize(const Specs &p_Specs)
{
    TKIT_LOG_INFO("[ONYX] Creating Vulkan instance");
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
    VKIT_ASSERT_RESULT(sysres);

    glfwInitHint(GLFW_PLATFORM, p_Specs.Platform);
    TKIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "[ONYX] Failed to initialize GLFW");
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
    VKIT_ASSERT_RESULT(result);

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
    TKIT_ASSERT(s_Instance, "[ONYX] Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    TKIT_ASSERT(!s_Device, "[ONYX] Device has already been created");

    createDevice(p_Surface);
    createVulkanAllocator();
    createCommandPool();
    retrieveDeviceQueues();
    Assets::Initialize();
    Pipelines::Initialize();
    s_DeletionQueue.Push([] {
        Pipelines::Terminate();
        Assets::Terminate();
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
    VKIT_ASSERT_EXPRESSION(s_Device.WaitIdle());
}

VKit::CommandPool &GetGraphicsPool()
{
    return s_GraphicsPool;
}
VKit::CommandPool &GetTransferPool()
{
    return s_TransferPool;
}
VmaAllocator GetVulkanAllocator()
{
    return s_VulkanAllocator;
}

VKit::DeletionQueue &GetDeletionQueue()
{
    return s_DeletionQueue;
}

bool IsSeparateTransferMode()
{
    return GetFamilyIndex(VKit::Queue_Graphics) != GetFamilyIndex(VKit::Queue_Transfer);
}

u32 GetFamilyIndex(const VKit::QueueType p_Type)
{
    return s_Device.GetInfo().PhysicalDevice.GetInfo().FamilyIndices[p_Type];
}

static QueueHandle *chooseQueue(const VKit::QueueType p_Type)
{
    const auto &stack = s_Queues[p_Type];
    TKIT_ASSERT(!stack.IsEmpty(),
                "[ONYX] There are no {} queues available. Ensure they are requested through Onyx's initialization "
                "specification",
                VKit::ToString(p_Type));

    QueueHandle *qh = nullptr;
    for (QueueHandle *q : stack)
        if (!qh || q->UsageCount < qh->UsageCount)
            qh = q;
    TKIT_ASSERT(qh, "[ONYX] Failed to find a queue of type {}", VKit::ToString(p_Type));
    return qh;
}

QueueHandle *BorrowQueue(const VKit::QueueType p_Type)
{
    QueueHandle *qh = chooseQueue(p_Type);
    ++qh->UsageCount;
    return qh;
}

void ReturnQueue(QueueHandle *p_Queue)
{
    TKIT_ASSERT(p_Queue->UsageCount != 0, "[ONYX] Cannot return a queue that has 0 usage count");
    --p_Queue->UsageCount;
}
} // namespace Onyx::Core
namespace Onyx::Detail
{
QueueData BorrowQueueData()
{
    QueueData data;
    data.Graphics = Core::chooseQueue(VKit::Queue_Graphics);
    data.Transfer = Core::chooseQueue(VKit::Queue_Transfer);
    data.Present = Core::chooseQueue(VKit::Queue_Present);

#ifdef TKIT_ENABLE_DEBUG_LOGS
    TKit::FixedArray<QueueHandle *, 3> handles{data.Graphics, data.Transfer, data.Present};
    TKit::FixedArray<VKit::QueueType, 3> types{VKit::Queue_Graphics, VKit::Queue_Transfer, VKit::Queue_Present};
    for (u32 i = 0; i < 3; ++i)
        TKIT_LOG_DEBUG("[ONYX] Borrowing {} queue <{}, {}>, which is being used in {} other instances",
                       VKit::ToString(types[i]), handles[i]->FamilyIndex, handles[i]->Index, handles[i]->UsageCount);
#endif

    ++data.Graphics->UsageCount;
    ++data.Transfer->UsageCount;
    ++data.Present->UsageCount;

    return data;
}

void ReturnQueueData(const QueueData &p_Data)
{
    Core::ReturnQueue(p_Data.Graphics);
    Core::ReturnQueue(p_Data.Transfer);
    Core::ReturnQueue(p_Data.Present);
}
} // namespace Onyx::Detail
