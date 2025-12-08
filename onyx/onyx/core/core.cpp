#include "onyx/core/pch.hpp"
#include "onyx/core/core.hpp"
#include "onyx/data/state.hpp"
#include "onyx/core/shaders.hpp"
#include "onyx/object/primitives.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "vkit/pipeline/pipeline_layout.hpp"
#include "vkit/core/core.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/memory/block_allocator.hpp"

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

#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
static TKit::VkProfilingContext s_GraphicsContext;
// static TKit::VkProfilingContext s_TransferContext;
static VkCommandBuffer s_ProfilingGraphicsCmd;
// static VkCommandBuffer s_ProfilingTransferCmd;
#endif

static TKit::Array4<u32> s_QueueRequests;
static TKit::Array4<TKit::StaticArray64<QueueHandle *>> s_Queues;

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;
static InitCallbacks s_Callbacks{};

static TKit::BlockAllocator s_QueueAllocator = TKit::BlockAllocator::CreateFromType<QueueHandle>(128);

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
    TKIT_LOG_WARNING_IF(!(s_Device.GetInfo().PhysicalDevice.GetInfo().Flags & VKit::PhysicalDevice::Flag_Optimal),
                        "[ONYX] The device is suitable, but not optimal");
    TKIT_LOG_INFO_IF(s_Device.GetInfo().PhysicalDevice.GetInfo().Flags & VKit::PhysicalDevice::Flag_Optimal,
                     "[ONYX] The device is optimal");

    const u32 maxCount = *std::max_element(s_QueueRequests.begin(), s_QueueRequests.end());
    for (u32 i = 0; i < maxCount; ++i)
    {
        TKit::Array4<QueueHandle *> handles{nullptr, nullptr, nullptr, nullptr};

        for (u32 j = 0; j < 4; ++j)
        {
            const u32 findex = phys.GetInfo().FamilyIndices[j];
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
                qh->Queue = queue;
                qh->Index = i;
                qh->FamilyIndex = findex;
                qh->UsageCount = 0;
                handles[j] = qh;

                TKIT_LOG_DEBUG("[ONYX] Retrieved {} queue <{}, {}>", VKit::ToString(static_cast<VKit::QueueType>(j)),
                               findex, i);
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
    const u32 gindex = Core::GetFamilyIndex(VKit::Queue_Graphics);
    const u32 tindex = Core::GetFamilyIndex(VKit::Queue_Transfer);
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

#ifdef TKIT_ENABLE_VULKAN_INSTRUMENTATION
static void createProfilingContext()
{
    TKIT_LOG_INFO("[ONYX] Creating Vulkan profiling context");
    auto cmdres = s_GraphicsPool.Allocate();
    VKIT_ASSERT_RESULT(cmdres);
    s_ProfilingGraphicsCmd = cmdres.GetValue();

    s_GraphicsContext = TKIT_PROFILE_CREATE_VULKAN_CONTEXT(
        s_Instance, s_Device.GetInfo().PhysicalDevice, s_Device, s_GraphicsQueue, s_ProfilingGraphicsCmd,
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
        s_DefaultManager.Construct();
        s_TaskManager = s_DefaultManager.Get();
        s_DeletionQueue.Push([] { s_DefaultManager.Destruct(); });
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
    const bool vlayers = s_Instance.GetInfo().Flags & VKit::Instance::Flag_HasValidationLayers;
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

void Core::Terminate()
{
    for (const auto &stack : s_Queues)
        for (QueueHandle *qh : stack)
            s_QueueAllocator.Destroy(qh);

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
    return s_Device.GetInfo().Table;
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

bool Core::IsSeparateTransferMode()
{
    return GetFamilyIndex(VKit::Queue_Graphics) != GetFamilyIndex(VKit::Queue_Transfer);
}

u32 Core::GetFamilyIndex(const VKit::QueueType p_Type)
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

QueueHandle *Core::BorrowQueue(const VKit::QueueType p_Type)
{
    QueueHandle *qh = chooseQueue(p_Type);
    ++qh->UsageCount;
    return qh;
}

void Core::ReturnQueue(QueueHandle *p_Queue)
{
    TKIT_ASSERT(p_Queue->UsageCount != 0, "[ONYX] Cannot return a queue that has 0 usage count");
    --p_Queue->UsageCount;
}

namespace Detail
{
QueueData BorrowQueueData()
{
    QueueData data;
    data.Graphics = chooseQueue(VKit::Queue_Graphics);
    data.Transfer = chooseQueue(VKit::Queue_Transfer);
    data.Present = chooseQueue(VKit::Queue_Present);

#ifdef TKIT_ENABLE_DEBUG_LOGS
    TKit::Array<QueueHandle *, 3> handles{data.Graphics, data.Transfer, data.Present};
    TKit::Array<VKit::QueueType, 3> types{VKit::Queue_Graphics, VKit::Queue_Transfer, VKit::Queue_Present};
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
} // namespace Detail

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
