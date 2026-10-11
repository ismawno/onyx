#include "pch.hpp"
#include "onyx/specs.hpp"
#include "onyx.hpp"
#include "onyx/core.hpp"
#include "resources.hpp"
#include "glfw.hpp"
#include "platform.hpp"
#include "pipelines.hpp"
#include "renderer.hpp"
#include "descriptors.hpp"
#include "execution.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "imgui_backend.hpp"
#endif

#include "graph/core.hpp"

#include "tkit/multiprocessing/thread_pool.hpp"
#ifdef ONYX_FONTCONFIG
#    include <fontconfig/fontconfig.h>
#endif

using namespace Onyx::Detail;
// TODO(Isma): Better error handling. if a function cannot keep a sane state after an error, it should not return an
// error
namespace Onyx
{
static u8 s_PushedAlloc = 0;
static TKit::FixedArray<Graph::Allocation, TKit::MaxThreads> s_Allocation{};

static TKit::ITaskManager *s_TaskManager;
static TKit::Storage<TKit::TaskManager> s_DefaultTaskManager;
static TKit::Storage<TKit::ThreadPool> s_DefaultThreadPool;

static TKit::Storage<DeletionQueue> s_DeletionQueue{};

#define PUSH_DELETER(code) s_DeletionQueue->Push([=] { code; })
#define SUBMIT_DELETION(object) s_DeletionQueue->SubmitForDeletion(object)

void DeletionQueue::Push(std::function<void()> &&deleter)
{
    m_Deleters.Append(std::move(deleter));
}
void DeletionQueue::Flush()
{
    for (u32 i = m_Deleters.GetSize(); i > 0; --i)
        m_Deleters[i - 1]();
    m_Deleters.Clear();
}
void DeletionQueue::Dismiss()
{
    m_Deleters.Clear();
}

static void initializeAllocators(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][CORE] Initializing allocators");
    // these are purposefully leaked
    for (u32 i = 0; i < TKit::MaxThreads; ++i)
    {
        const Allocation &alloc = specs.Allocators[i];

        Graph::Allocation userAlloc{};
        userAlloc.Arena = alloc.Arena;
        userAlloc.Stack = alloc.Stack;
        userAlloc.Tier = alloc.Tier;

        Graph::Allocation &libAlloc = s_Allocation[i];
        if (userAlloc.Arena)
            libAlloc.Arena = userAlloc.Arena;
        else if (!libAlloc.Arena)
            libAlloc.Arena = new TKit::ArenaAllocator(i == 0 ? 8_mib : 16_kib, TKIT_CACHE_LINE_SIZE);
        if (userAlloc.Stack)
            libAlloc.Stack = userAlloc.Stack;
        else if (!libAlloc.Stack)
            libAlloc.Stack = new TKit::StackAllocator(i == 0 ? 8_mib : 16_kib, TKIT_CACHE_LINE_SIZE);

        if (userAlloc.Tier)
            libAlloc.Tier = userAlloc.Tier;
        else if (!libAlloc.Tier)
        {
            const TKit::TierDescriptions desc{{.Allocator = libAlloc.Arena,
                                               .MaxAllocation = i == 0 ? 2_mib : 4_kib,
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
            libAlloc.Tier = new TKit::TierAllocator(desc, 32);
        }
    }
    Graph::Allocation &libAlloc = s_Allocation[0];
    if (TKit::GetArena() != libAlloc.Arena)
    {
        TKit::PushArena(libAlloc.Arena);
        s_PushedAlloc |= 1U << 0;
    }
    if (TKit::GetStack() != libAlloc.Stack)
    {
        TKit::PushStack(libAlloc.Stack);
        s_PushedAlloc |= 1U << 1;
    }
    if (TKit::GetTier() != libAlloc.Tier)
    {
        TKit::PushTier(libAlloc.Tier);
        s_PushedAlloc |= 1U << 2;
    }
}

static void terminateAllocators(const bool resetArenas)
{
    if (resetArenas)
        for (const Graph::Allocation &alloc : s_Allocation)
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

DeletionQueue &GetDeletionQueue()
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

void Initialize(const Specs &specs)
{
    TKIT_BEGIN_INFO_CLOCK();
    TKIT_LOG_INFO("[ONYX][CORE] Initializing");
    if (specs.Locale)
        std::locale::global(std::locale(specs.Locale));

    initializeAllocators(specs);
    s_DeletionQueue.Construct();

    PUSH_DELETER(terminateAllocators(specs.Flags & InitializationFlag_ResetArenasOnTermination));
#ifdef ONYX_FONTCONFIG
    FcInit();
#endif

    Graph::Specs gspecs{};
    gspecs.Allocators = s_Allocation[0];
    gspecs.LoaderPath = specs.LoaderPath;
    gspecs.DumpPath = specs.DeviceFaultCrashDump;
    gspecs.EnabledCapabilities = Graph::Capability_IndependentBlend | Graph::Capability_MultiDrawIndirect |
                                 Graph::Capability_ShaderDrawParameters | Graph::Capability_ExtendedDynamicState |
                                 Graph::Capability_BindlessDescriptors | Graph::Capability_ImageMultiFormat |
                                 Graph::Capability_RequireGraphicsQueue | Graph::Capability_RequireTransferQueue;
    gspecs.MaxCommandPools = 1024;
    gspecs.MaxBuffers = 1024;
    gspecs.MaxImages = 512;
    gspecs.MaxDescriptorSets = 1024;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_StorageBuffer] = 16384;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_Sampler] = 1024;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_SampledImage] = 131072;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_CombinedImageSampler] = 1024;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_StorageImage] = 512;

    PUSH_DELETER(Graph::Terminate());
    Graph::Initialize(gspecs);

    if (specs.TaskManager)
        s_TaskManager = specs.TaskManager;
    else if (specs.Flags & InitializationFlag_DefaultTaskManagerSingleThread)
    {
        s_DefaultTaskManager.Construct();
        s_TaskManager = &s_DefaultTaskManager.Get();
        PUSH_DELETER(s_DefaultTaskManager.Destruct());
    }
    else
    {
        s_DefaultThreadPool.Construct(TKit::MaxThreads - 1);
        s_TaskManager = &s_DefaultThreadPool.Get();
        PUSH_DELETER(s_DefaultThreadPool.Destruct());
    }

    PUSH_DELETER(Platform::Terminate());
    Platform::Initialize(specs.PlatformSpecs ? *specs.PlatformSpecs : Platform::Specs{});

    PUSH_DELETER(Platform::DestroyWindows());
    PUSH_DELETER(Execution::Terminate());
    Execution::Initialize(specs.ExecutionSpecs ? *specs.ExecutionSpecs : Execution::Specs{});

    PUSH_DELETER(Descriptors::Terminate());
    Descriptors::Initialize(specs.DescriptorSpecs ? *specs.DescriptorSpecs : Descriptors::Specs{});

    PUSH_DELETER(Pipelines::Terminate());
    Pipelines::Initialize();

    PUSH_DELETER(Renderer::Terminate());
    Renderer::Initialize(specs.RendererSpecs ? *specs.RendererSpecs : Renderer::Specs{});

    PUSH_DELETER(Resources::Terminate());
    Resources::Initialize(specs.ResourceSpecs ? *specs.ResourceSpecs : Resources::Specs{});

#ifdef ONYX_ENABLE_IMGUI
    PUSH_DELETER(ImGuiBackend::Terminate());
    ImGuiBackend::Initialize();
#endif

    PUSH_DELETER(TerminateApi());
    InitializeApi();

    TKIT_END_INFO_CLOCK(Seconds, "[ONYX][CORE] Done in {:.2f} seconds");
}

void Terminate()
{
    Graph::Terminate();
    s_DeletionQueue.Destruct();
}

TKit::TierString Error::ToString() const
{
    TKit::TierString str = TKit::TierString::Format("[ONYX] Error code: '{}'", Onyx::ToString(m_ErrorCode));
    if (m_CheapMessage)
        str += TKit::TierString::Format(" - Message: '{}'", m_CheapMessage);
    else if (!m_FormattedMessage.IsEmpty())
        str += TKit::TierString::Format(" - Message: '{}'", m_FormattedMessage);
    return str;
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
