#define VMA_IMPLEMENTATION
#include "onyx/core/pch.hpp"
#include "onyx/core/core.hpp"
#include "vkit/pipeline/pipeline_layout.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/rendering/render_specs.hpp"
#include "onyx/core/shaders.hpp"
#include "tkit/core/logging.hpp"

#include "onyx/core/glfw.hpp"

#include <filesystem>

namespace Onyx
{
static TKit::ITaskManager *s_TaskManager;

static VKit::Instance s_Instance{};
static VKit::LogicalDevice s_Device{};

static VKit::DescriptorPool s_DescriptorPool{};
static VKit::DescriptorSetLayout s_TransformStorageLayout{};
static VKit::DescriptorSetLayout s_LightStorageLayout{};

static VKit::PipelineLayout s_GraphicsPipelineLayout2D{};
static VKit::PipelineLayout s_GraphicsPipelineLayout3D{};

static VKit::CommandPool s_CommandPool{};

#ifdef TKIT_ENABLE_VULKAN_PROFILING
static TKit::VkProfilingContext s_ProfilingContext;
static VkCommandBuffer s_ProfilingCommandBuffer;
#endif

static VkQueue s_GraphicsQueue = VK_NULL_HANDLE;
static VkQueue s_PresentQueue = VK_NULL_HANDLE;

static std::mutex s_GraphicsMutex{};
static std::mutex s_PresentMutex{};

static VmaAllocator s_VulkanAllocator = VK_NULL_HANDLE;

static void createDevice(const VkSurfaceKHR p_Surface) noexcept
{
    TKIT_LOG_INFO("Creating Vulkan device...");
    const auto physres = VKit::PhysicalDevice::Selector(&s_Instance)
                             .SetSurface(p_Surface)
                             .PreferType(VKit::PhysicalDevice::Discrete)
                             .AddFlags(VKit::PhysicalDevice::Selector::Flag_AnyType |
                                       VKit::PhysicalDevice::Selector::Flag_PortabilitySubset |
                                       VKit::PhysicalDevice::Selector::Flag_RequireGraphicsQueue)
                             .Select();
    VKIT_ASSERT_RESULT(physres);
    const VKit::PhysicalDevice &phys = physres.GetValue();

    const auto devres = VKit::LogicalDevice::Create(s_Instance, phys);
    VKIT_ASSERT_RESULT(devres);
    s_Device = devres.GetValue();

    s_GraphicsQueue = s_Device.GetQueue(VKit::QueueType::Graphics);
    s_PresentQueue = s_Device.GetQueue(VKit::QueueType::Present);
}

static void createVulkanAllocator() noexcept
{
    TKIT_LOG_INFO("Creating Vulkan allocator...");
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = s_Device.GetPhysicalDevice();
    allocatorInfo.device = s_Device.GetDevice();
    allocatorInfo.instance = s_Instance.GetInstance();
    allocatorInfo.vulkanApiVersion = s_Instance.GetInfo().ApiVersion;
    allocatorInfo.flags = 0;
    allocatorInfo.pVulkanFunctions = nullptr;
    TKIT_ASSERT_RETURNS(vmaCreateAllocator(&allocatorInfo, &s_VulkanAllocator), VK_SUCCESS,
                        "Failed to create vulkan allocator");
}

static void createCommandPool() noexcept
{
    TKIT_LOG_INFO("Creating global command pool...");
    VKit::CommandPool::Specs specs{};
    specs.QueueFamilyIndex = s_Device.GetPhysicalDevice().GetInfo().GraphicsIndex;
    specs.Flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    const auto poolres = VKit::CommandPool::Create(s_Device, specs);
    VKIT_ASSERT_RESULT(poolres);
    s_CommandPool = poolres.GetValue();
}

#ifdef TKIT_ENABLE_VULKAN_PROFILING
static void createProfilingContext() noexcept
{
    TKIT_LOG_INFO("Creating Vulkan profiling context...");
    const auto cmdres = s_CommandPool.Allocate();
    VKIT_ASSERT_RESULT(cmdres);
    s_ProfilingCommandBuffer = cmdres.GetValue();

    s_ProfilingContext = TKIT_PROFILE_CREATE_VULKAN_CONTEXT(s_Device.GetPhysicalDevice(), s_Device, s_GraphicsQueue,
                                                            s_ProfilingCommandBuffer);
}
#endif

static void createDescriptorData() noexcept
{
    TKIT_LOG_INFO("Creating global descriptor data...");
    const auto poolResult = VKit::DescriptorPool::Builder(s_Device)
                                .SetMaxSets(ONYX_MAX_DESCRIPTOR_SETS)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ONYX_MAX_DESCRIPTORS)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8)
                                .Build();

    VKIT_ASSERT_RESULT(poolResult);
    s_DescriptorPool = poolResult.GetValue();

    auto layoutResult = VKit::DescriptorSetLayout::Builder(s_Device)
                            .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_TransformStorageLayout = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(s_Device)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_LightStorageLayout = layoutResult.GetValue();
}

static void createPipelineLayouts() noexcept
{
    TKIT_LOG_INFO("Creating global pipeline layouts...");
    auto layoutResult =
        VKit::PipelineLayout::Builder(s_Device).AddDescriptorSetLayout(s_TransformStorageLayout).Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_GraphicsPipelineLayout2D = layoutResult.GetValue();

    layoutResult =
        VKit::PipelineLayout::Builder(s_Device)
            .AddDescriptorSetLayout(s_TransformStorageLayout)
            .AddDescriptorSetLayout(s_LightStorageLayout)
            .AddPushConstantRange<PushConstantData3D>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_GraphicsPipelineLayout3D = layoutResult.GetValue();
}

static void createShaders() noexcept
{
    TKIT_LOG_INFO("Creating global shaders...");
    Shaders<D2, DrawMode::Fill>::Initialize();
    Shaders<D2, DrawMode::Stencil>::Initialize();
    Shaders<D3, DrawMode::Fill>::Initialize();
    Shaders<D3, DrawMode::Stencil>::Initialize();
}

void Core::Initialize(TKit::ITaskManager *p_TaskManager) noexcept
{
    TKIT_LOG_INFO("Initializing Onyx...");
    const auto sysres = VKit::System::Initialize();
    VKIT_ASSERT_VULKAN_RESULT(sysres);

    TKIT_ASSERT_RETURNS(glfwInit(), GLFW_TRUE, "Failed to initialize GLFW");
    u32 extensionCount;
    const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    const std::span<const char *> extensionSpan(extensions, extensionCount);

    TKIT_LOG_INFO("Creating Vulkan instance...");
    VKit::Instance::Builder builder{};
    builder.SetApplicationName("Onyx").RequireApiVersion(1, 1, 0).RequireExtensions(extensionSpan);
#ifdef TKIT_ENABLE_ASSERTS
    builder.RequireValidationLayers();
#endif

    const auto result = builder.Build();
    VKIT_ASSERT_RESULT(result);

    s_Instance = result.GetValue();
    s_TaskManager = p_TaskManager;
}

void Core::Terminate() noexcept
{
    if (s_Device)
    {
        s_Device.WaitIdle();
        Shaders<D2, DrawMode::Fill>::Terminate();
        Shaders<D2, DrawMode::Stencil>::Terminate();
        Shaders<D3, DrawMode::Fill>::Terminate();
        Shaders<D3, DrawMode::Stencil>::Terminate();

#ifdef TKIT_ENABLE_VULKAN_PROFILING
        TKIT_PROFILE_DESTROY_VULKAN_CONTEXT(s_ProfilingContext);
        s_CommandPool.Deallocate(s_ProfilingCommandBuffer);
#endif

        DestroyCombinedPrimitiveBuffers();
        s_GraphicsPipelineLayout2D.Destroy();
        s_GraphicsPipelineLayout3D.Destroy();

        s_TransformStorageLayout.Destroy();
        s_LightStorageLayout.Destroy();
        s_DescriptorPool.Destroy();
        s_CommandPool.Destroy();
        vmaDestroyAllocator(s_VulkanAllocator);
        s_Device.Destroy();
    }
    glfwTerminate();
    s_Instance.Destroy();
}

void Core::CreateDevice(const VkSurfaceKHR p_Surface) noexcept
{
    TKIT_ASSERT(s_Instance, "Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    TKIT_ASSERT(!s_Device, "Device has already been created");

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

const VKit::Instance &Core::GetInstance() noexcept
{
    TKIT_ASSERT(s_Instance, "Vulkan instance is not initialized! Forgot to call Onyx::Core::Initialize?");
    return s_Instance;
}
const VKit::LogicalDevice &Core::GetDevice() noexcept
{
    return s_Device;
}
bool Core::IsDeviceCreated() noexcept
{
    return s_Device;
}
void Core::DeviceWaitIdle() noexcept
{
    LockGraphicsAndPresentQueues();
    s_Device.WaitIdle();
    UnlockGraphicsAndPresentQueues();
}

VKit::CommandPool &Core::GetCommandPool() noexcept
{
    return s_CommandPool;
}
VmaAllocator Core::GetVulkanAllocator() noexcept
{
    return s_VulkanAllocator;
}

const VKit::DescriptorPool &Core::GetDescriptorPool() noexcept
{
    return s_DescriptorPool;
}
const VKit::DescriptorSetLayout &Core::GetTransformStorageDescriptorSetLayout() noexcept
{
    return s_TransformStorageLayout;
}
const VKit::DescriptorSetLayout &Core::GetLightStorageDescriptorSetLayout() noexcept
{
    return s_LightStorageLayout;
}

template <Dimension D> VertexBuffer<D> Core::CreateVertexBuffer(const std::span<const Vertex<D>> p_Vertices) noexcept
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::VertexSpecs specs{};
    specs.Allocator = s_VulkanAllocator;
    specs.Data = p_Vertices;
    specs.CommandPool = &GetCommandPool();
    specs.Queue = GetGraphicsQueue();
    const auto result = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
IndexBuffer Core::CreateIndexBuffer(const std::span<const Index> p_Indices) noexcept
{
    typename VKit::DeviceLocalBuffer<Index>::IndexSpecs specs{};
    specs.Allocator = s_VulkanAllocator;
    specs.Data = p_Indices;
    specs.CommandPool = &GetCommandPool();
    specs.Queue = GetGraphicsQueue();
    const auto result = VKit::DeviceLocalBuffer<Index>::CreateIndexBuffer(specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> MutableVertexBuffer<D> Core::CreateMutableVertexBuffer(const VkDeviceSize p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<Vertex<D>>::VertexSpecs specs{};
    specs.Allocator = s_VulkanAllocator;
    specs.Capacity = p_Capacity;
    const auto result = VKit::HostVisibleBuffer<Vertex<D>>::CreateVertexBuffer(specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
MutableIndexBuffer Core::CreateMutableIndexBuffer(const VkDeviceSize p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<Index>::IndexSpecs specs{};
    specs.Allocator = s_VulkanAllocator;
    specs.Capacity = p_Capacity;
    const auto result = VKit::HostVisibleBuffer<Index>::CreateIndexBuffer(specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> VkPipelineLayout Core::GetGraphicsPipelineLayout() noexcept
{
    if constexpr (D == D2)
        return s_GraphicsPipelineLayout2D;
    else
        return s_GraphicsPipelineLayout3D;
}

VkQueue Core::GetGraphicsQueue() noexcept
{
    return s_GraphicsQueue;
}
VkQueue Core::GetPresentQueue() noexcept
{
    return s_PresentQueue;
}

std::mutex &Core::GetGraphicsMutex() noexcept
{
    return s_GraphicsMutex;
}
std::mutex &Core::GetPresentMutex() noexcept
{
    return s_GraphicsQueue == s_PresentQueue ? s_GraphicsMutex : s_PresentMutex;
}

void Core::LockGraphicsAndPresentQueues() noexcept
{
    if (s_GraphicsQueue != s_PresentQueue)
        std::lock(s_GraphicsMutex, s_PresentMutex);
    else
        s_GraphicsMutex.lock();
}
void Core::UnlockGraphicsAndPresentQueues() noexcept
{
    if (s_GraphicsQueue != s_PresentQueue)
    {
        s_GraphicsMutex.unlock();
        s_PresentMutex.unlock();
    }
    else
        s_GraphicsMutex.unlock();
}

#ifdef TKIT_ENABLE_VULKAN_PROFILING
TKit::VkProfilingContext Core::GetProfilingContext() noexcept
{
    return s_ProfilingContext;
}
#endif

template VertexBuffer<D2> Core::CreateVertexBuffer<D2>(std::span<const Vertex<D2>>) noexcept;
template VertexBuffer<D3> Core::CreateVertexBuffer<D3>(std::span<const Vertex<D3>>) noexcept;

template MutableVertexBuffer<D2> Core::CreateMutableVertexBuffer<D2>(VkDeviceSize) noexcept;
template MutableVertexBuffer<D3> Core::CreateMutableVertexBuffer<D3>(VkDeviceSize) noexcept;

template VkPipelineLayout Core::GetGraphicsPipelineLayout<D2>() noexcept;
template VkPipelineLayout Core::GetGraphicsPipelineLayout<D3>() noexcept;

} // namespace Onyx
