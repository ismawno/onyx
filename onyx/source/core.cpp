#include "pch.hpp"
#include "core.hpp"
#include "resources.hpp"
#include "renderer.hpp"
#include "conversion.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "imgui_backend.hpp"
#endif

#include "graph/core.hpp"

#include "tkit/multiprocessing/thread_pool.hpp"
#ifdef ONYX_FONTCONFIG
#    include <fontconfig/fontconfig.h>
#endif

namespace Onyx
{
constexpr u32 Dim2 = 0;
constexpr u32 Dim3 = 1;

/////////////////////////////////////////////
/// PLATFORM
/////////////////////////////////////////////

static TKit::Storage<TKit::StaticArray<Window *, ONYX_MAX_VIEWS>> s_Windows{};

static void platform_Initialize()
{
    TKIT_LOG_INFO("[ONYX][PLATFORM] Initializing");
    s_Windows.Construct();
}

static void platform_Terminate()
{
    DestroyWindows();
    s_Windows.Destruct();
}

Window *CreateWindow(const WindowSpecs &specs)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Window *window = tier->Create<Window>(specs);
    s_Windows->Append(window);
    return window;
}

static void removeWindow(const Window *window)
{
    for (u32 i = 0; i < s_Windows->GetSize(); ++i)
        if (s_Windows->At(i) == window)
        {
            s_Windows->RemoveUnordered(s_Windows->begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][PLATFORM] Window '{}' not found", window->GetTitle());
}

void DestroyWindow(const Window *window)
{
    removeWindow(window);
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(window);
}

void DestroyWindows()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (const Window *window : *s_Windows)
        tier->Destroy(window);
    s_Windows->Clear();
}

/////////////////////////////////////////////
/// END PLATFORM
/////////////////////////////////////////////

/////////////////////////////////////////////
/// EXECUTION
/////////////////////////////////////////////

struct CommandPool
{
    Graph::CommandPool Pool;
    Graph::Tracker Tracker{};
};
static Graph::CommandPool s_Graphics = NullHandle;
static TKit::Storage<TKit::ArenaArray<CommandPool>> s_CommandPools{};
static Specs s_Specs{};

Graph::CommandPool FindAvailableCommandPool()
{
    const CommandPool *pending = nullptr;
    for (CommandPool &pool : *s_CommandPools)
        if (!pool.Tracker.InUse())
        {
            Graph::CommandPool_Reset(pool.Pool);
            return pool.Pool;
        }
        else if (!pending || pending->Tracker.InFlightValue > pool.Tracker.InFlightValue)
            pending = &pool;

    TKIT_ASSERT(pending, "[ONYX][EXECUTION] Failed to find an available command pool");
    TKIT_BEGIN_INFO_CLOCK();
    Graph::Queue_WaitForTracker(pending->Tracker);
    TKIT_END_INFO_CLOCK(Milliseconds, "[ONYX][EXECUTION] Waited {:.2f} ms for a new command pool");
    return pending->Pool;
}

void MarkCommandPoolInUse(const Graph::CommandPool pool, const u64 inFlightValue)
{
    for (CommandPool &p : *s_CommandPools)
        if (p.Pool == pool)
        {
            p.Tracker.InFlightValue = inFlightValue;
            return;
        }
    TKIT_FATAL("[ONYX][EXECUTION] Failed to find command pool with handle {:010x}", pool);
}

static void execution_Initialize()
{
    TKIT_LOG_INFO("[ONYX][EXECUTION] Initializing");
    s_CommandPools.Construct();
    // divided by two so we dont exhaust graph pool
    const u32 maxPools = Graph::GetSpecs().MaxCommandPools / 2;

    s_CommandPools->Reserve(maxPools);
    for (u32 i = 0; i < maxPools; ++i)
    {
        CommandPool &pool = s_CommandPools->Append();
        pool.Pool = Graph::CommandPool_Create(Graph::Queue_Graphics, Graph::CommandPoolFlag_CreateTransient);
        pool.Tracker.Queue = Graph::Queue_Graphics;
        if (Graph::IsValidationEnabled())
        {
            const TKit::StackString name = TKit::StackString::Format("onyx-ring-graphics-command-pool-index-{}", i);
            Graph::CommandPool_SetName(pool.Pool, name.CString());
        }
    }

    s_Graphics = Graph::CommandPool_Create(Graph::Queue_Graphics, Graph::CommandPoolFlag_CreateTransient);
    if (Graph::IsValidationEnabled())
        Graph::CommandPool_SetName(s_Graphics, "onyx-transient-graphics-command-pool");
}
static void execution_Terminate()
{
    Graph::CommandPool_Destroy(s_Graphics);
    s_Graphics = Graph::NullHandle;
    for (const CommandPool &pool : *s_CommandPools)
        Graph::CommandPool_Destroy(pool.Pool);

    s_CommandPools.Destruct();
}

Graph::CommandPool GetTransientGraphicsCommandPool()
{
    return s_Graphics;
}

/////////////////////////////////////////////
/// END EXECUTION
/////////////////////////////////////////////

/////////////////////////////////////////////
/// DESCRIPTORS
/////////////////////////////////////////////

struct DescriptorData
{
    ten<Graph::DescriptorSetLayout, D_Count, RenderPass_Count> Layouts{};
    TKit::FixedArray<Graph::DescriptorSetLayout, StandalonePass_Count> StandaloneLayouts{};
};

static TKit::Storage<DescriptorData> s_DescriptorData{};

static void createDescriptorData()
{
    using Binding = Graph::DescriptorBinding;
    using BindingArray = TKit::StaticArray16<Binding>;

    constexpr Graph::DescriptorType buffer = Graph::Descriptor_StorageBuffer;
    constexpr Graph::DescriptorType sampler = Graph::Descriptor_Sampler;
    constexpr Graph::DescriptorType sampledImage = Graph::Descriptor_SampledImage;
    constexpr Graph::DescriptorType combined = Graph::Descriptor_CombinedImageSampler;
    constexpr Graph::DescriptorType storageImage = Graph::Descriptor_StorageImage;

    constexpr Graph::ShaderStageFlags compute = Graph::ShaderStageFlag_Compute;
    constexpr Graph::ShaderStageFlags vertex = Graph::ShaderStageFlag_Vertex;
    constexpr Graph::ShaderStageFlags fragment = Graph::ShaderStageFlag_Fragment;

    // bind unused is disabled for now
    constexpr Graph::DescriptorBindingFlags pbound = Graph::DescriptorBindingFlag_PartiallyBound;
    constexpr Graph::DescriptorBindingFlags updateAfterBind = Graph::DescriptorBindingFlag_UpdateUnusedWhilePending;

    BindingArray flat{};
    flat.Append(Binding::Create(ONYX_GEOMETRY_PASS_INSTANCES_BINDING_POINT, buffer, vertex | fragment));
    flat.Append(Binding::Create(ONYX_GEOMETRY_PASS_SAMPLERS_BINDING_POINT, sampler, fragment, ONYX_MAX_SAMPLERS,
                                pbound | updateAfterBind));
    flat.Append(Binding::Create(ONYX_GEOMETRY_PASS_TEXTURES_BINDING_POINT, sampledImage, fragment, ONYX_MAX_TEXTURES,
                                pbound | updateAfterBind));
    flat.Append(Binding::Create(ONYX_GEOMETRY_PASS_TEXTURE_OFFSETS_BINDING_POINT, buffer, fragment));
    flat.Append(Binding::Create(ONYX_GEOMETRY_PASS_BOUNDS_BINDING_POINT, buffer, vertex));

    s_DescriptorData->Layouts[Dim2][RenderPass_Flat] = Graph::DescriptorSetLayout_Create(flat);
    s_DescriptorData->Layouts[Dim3][RenderPass_Flat] = Graph::DescriptorSetLayout_Create(flat);

    // flat is also the common prefix of the shaded bindings
    BindingArray shaded = flat;
    shaded.Append(Binding::Create(ONYX_GEOMETRY_PASS_MATERIALS_BINDING_POINT, buffer, fragment));
    shaded.Append(Binding::Create(ONYX_GEOMETRY_PASS_POINT_LIGHTS_BINDING_POINT, buffer, fragment));
    shaded.Append(Binding::Create(ONYX_GEOMETRY_PASS_DIRECTIONAL_LIGHTS_BINDING_POINT, buffer, fragment));
    shaded.Append(Binding::Create(ONYX_GEOMETRY_PASS_SHADOW_SAMPLER_BINDING_POINT, sampler, fragment));
    shaded.Append(Binding::Create(ONYX_GEOMETRY_PASS_SHADOW_COMPARE_SAMPLER_BINDING_POINT, sampler, fragment));

    // we perform a copy to not modify shaded
    BindingArray shaded2 = shaded;
    shaded2.Append(Binding::Create(ONYX_2D_GEOMETRY_PASS_SHADOW_MAPS_BINDING_POINT, sampledImage, fragment,
                                   ONYX_2D_MAX_RAY_MARCH_AND_OCCLUSION_MAPS, pbound | updateAfterBind));
    s_DescriptorData->Layouts[Dim2][RenderPass_Shaded] = Graph::DescriptorSetLayout_Create(shaded2);

    shaded.Append(Binding::Create(ONYX_3D_GEOMETRY_PASS_POINT_MAPS_BINDING_POINT, sampledImage, fragment,
                                  ONYX_MAX_TEXTURE_MAPS, pbound));
    shaded.Append(Binding::Create(ONYX_3D_GEOMETRY_PASS_DIRECTIONAL_MAPS_BINDING_POINT, sampledImage, fragment,
                                  ONYX_MAX_TEXTURE_MAPS, pbound));
    shaded.Append(Binding::Create(ONYX_3D_GEOMETRY_PASS_SPOT_MAPS_BINDING_POINT, sampledImage, fragment,
                                  ONYX_MAX_TEXTURE_MAPS, pbound));
    shaded.Append(Binding::Create(ONYX_3D_GEOMETRY_PASS_SPOT_LIGHTS_BINDING_POINT, buffer, fragment));
    s_DescriptorData->Layouts[Dim3][RenderPass_Shaded] = Graph::DescriptorSetLayout_Create(shaded);

    BindingArray shadow{};
    shadow.Append(Binding::Create(ONYX_GEOMETRY_PASS_INSTANCES_BINDING_POINT, buffer, vertex | fragment));
    shadow.Append(
        Binding::Create(ONYX_GEOMETRY_PASS_SAMPLERS_BINDING_POINT, sampler, fragment, ONYX_MAX_SAMPLERS, pbound));
    shadow.Append(
        Binding::Create(ONYX_GEOMETRY_PASS_TEXTURES_BINDING_POINT, sampledImage, fragment, ONYX_MAX_TEXTURES, pbound));
    shadow.Append(Binding::Create(ONYX_GEOMETRY_PASS_BOUNDS_BINDING_POINT, buffer, vertex));

    s_DescriptorData->Layouts[Dim3][RenderPass_Shadow] = Graph::DescriptorSetLayout_Create(shadow);

    shadow.Append(Binding::Create(ONYX_GEOMETRY_PASS_MATERIALS_BINDING_POINT, buffer, fragment));
    s_DescriptorData->Layouts[Dim2][RenderPass_Shadow] = Graph::DescriptorSetLayout_Create(shadow);

    // NOTE(Isma): Removed bind unused here
    const Binding rayMarch[] = {
        Binding::Create(ONYX_2D_RAY_MARCH_PASS_OCCLUSION_MAP_BINDING_POINT, storageImage, compute,
                        ONYX_2D_MAX_RAY_MARCH_AND_OCCLUSION_MAPS, pbound | updateAfterBind),
        Binding::Create(ONYX_2D_RAY_MARCH_PASS_RAY_MARCH_MAP_BINDING_POINT, storageImage, compute,
                        ONYX_2D_MAX_RAY_MARCH_AND_OCCLUSION_MAPS, pbound | updateAfterBind),
    };
    s_DescriptorData->StandaloneLayouts[StandalonePass_RayMarch] = Graph::DescriptorSetLayout_Create(rayMarch);

    constexpr Binding blend[] = {
        Binding::Create(ONYX_BLEND_PASS_TRANSPARENT_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS,
                        pbound | updateAfterBind),
        Binding::Create(ONYX_BLEND_PASS_REVEALAGE_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS,
                        pbound | updateAfterBind),
    };
    s_DescriptorData->StandaloneLayouts[StandalonePass_Blend] = Graph::DescriptorSetLayout_Create(blend);

    constexpr Binding postProcess[] = {
        Binding::Create(ONYX_POST_PROCESS_PASS_COLOR_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS,
                        pbound | updateAfterBind),
        Binding::Create(ONYX_POST_PROCESS_PASS_OUTLINE_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS,
                        pbound | updateAfterBind),
        Binding::Create(ONYX_POST_PROCESS_PASS_STENCIL_ATTACHMENTS_BINDING, sampledImage, fragment,
                        ONYX_MAX_ATTACHMENTS, pbound | updateAfterBind),
    };
    s_DescriptorData->StandaloneLayouts[StandalonePass_PostProcess] = Graph::DescriptorSetLayout_Create(postProcess);

    constexpr Binding compositor[] = {
        Binding::Create(ONYX_COMPOSITOR_PASS_COLOR_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS,
                        pbound | updateAfterBind),
    };
    s_DescriptorData->StandaloneLayouts[StandalonePass_Compositor] = Graph::DescriptorSetLayout_Create(compositor);

    if (Graph::IsValidationEnabled())
    {
        s_DescriptorData->Layouts.IterateMultiIndex([&](const u32 i, const u32 j) {
            Graph::DescriptorSetLayout_SetName(
                s_DescriptorData->Layouts[i][j],
                TKit::StackString::Format("onyx-descriptor-set-layout-{}D-{}", i + 2, ToString(RenderPass(j)))
                    .CString());
        });

        Graph::DescriptorSetLayout_SetName(s_DescriptorData->StandaloneLayouts[StandalonePass_RayMarch],
                                           "onyx-ray-march-descriptor-set-layout");
        Graph::DescriptorSetLayout_SetName(s_DescriptorData->StandaloneLayouts[StandalonePass_Blend],
                                           "onyx-blend-descriptor-set-layout");
        Graph::DescriptorSetLayout_SetName(s_DescriptorData->StandaloneLayouts[StandalonePass_PostProcess],
                                           "onyx-post-process-descriptor-set-layout");
        Graph::DescriptorSetLayout_SetName(s_DescriptorData->StandaloneLayouts[StandalonePass_Compositor],
                                           "onyx-compositor-descriptor-set-layout");
    }
}

static void descriptors_Initialize()
{
    TKIT_LOG_INFO("[ONYX][DESCRIPTORS] Initializing");
    s_DescriptorData.Construct();
    return createDescriptorData();
}
static void descriptors_Terminate()
{
    for (const Graph::DescriptorSetLayout layout : s_DescriptorData->Layouts)
        Graph::DescriptorSetLayout_Destroy(layout);

    for (const Graph::DescriptorSetLayout layout : s_DescriptorData->StandaloneLayouts)
        Graph::DescriptorSetLayout_Destroy(layout);
    s_DescriptorData.Destruct();
}

template <Dimension D> Graph::DescriptorSetLayout GetDescriptorLayout(const RenderPass pass)
{
    return s_DescriptorData->Layouts[D - 2][pass];
}
Graph::DescriptorSetLayout GetDescriptorLayout(const StandalonePass pass)
{
    return s_DescriptorData->StandaloneLayouts[pass];
}

template Graph::DescriptorSetLayout GetDescriptorLayout<D2>(RenderPass pass);
template Graph::DescriptorSetLayout GetDescriptorLayout<D3>(RenderPass pass);

/////////////////////////////////////////////
/// END DESCRIPTORS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// CORE
/////////////////////////////////////////////

static u8 s_PushedAlloc = 0;
static TKit::FixedArray<Graph::Allocation, TKit::MaxThreads> s_Allocation{};

static TKit::ITaskManager *s_TaskManager;
static TKit::Storage<TKit::TaskManager> s_DefaultTaskManager;
static TKit::Storage<TKit::ThreadPool> s_DefaultThreadPool;

static void allocators_Initialize(const Specs &specs)
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

static void allocators_Terminate()
{
    // if (resetArenas)
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

static void core_Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][CORE] Initializing");

    allocators_Initialize(specs);

    s_DefaultTaskManager.Construct();
    s_DefaultThreadPool.Construct(TKit::MaxThreads - 1);

    if (specs.Locale && std::setlocale(LC_ALL, specs.Locale))
        std::locale::global(std::locale(specs.Locale));

#ifdef ONYX_FONTCONFIG
    FcInit();
#endif

    Graph::Specs gspecs{};
    gspecs.TargetPlatform = specs.Platform;
    gspecs.Allocators = s_Allocation[0];
    gspecs.LoaderPath = specs.LoaderPath;
    gspecs.DumpPath = specs.DeviceFaultCrashDump;
    gspecs.EnabledCapabilities = Graph::Capability_IndependentBlend | Graph::Capability_MultiDrawIndirect |
                                 Graph::Capability_ShaderDrawParameters | Graph::Capability_ExtendedDynamicState |
                                 Graph::Capability_BindlessDescriptors | Graph::Capability_ImageMultiFormat |
                                 Graph::Capability_RequireGraphicsQueue;
    if (specs.Flags & InitializationFlag_EnableDebugUtils)
        gspecs.EnabledCapabilities |= Graph::Capability_DebugUtils;
    if (specs.Flags & InitializationFlag_EnablePrintfDebugFeature)
        gspecs.EnabledCapabilities |= Graph::Capability_DebugPrintf;
    if (specs.Flags & InitializationFlag_EnableDeviceFaultExtension)
        gspecs.EnabledCapabilities |= Graph::Capability_FaultDump;

    // execution
    gspecs.MaxCommandPools = specs.MaxCommandPools;
    gspecs.MaxCommandBuffers = specs.MaxCommandBuffers;
    gspecs.MaxDescriptorSets = specs.MaxDescriptorSets;

    // resources
    gspecs.MaxBuffers = specs.MaxBuffers;
    gspecs.MaxImages = specs.MaxImages;
    gspecs.MaxSamplers = specs.MaxSamplers;
    gspecs.MaxImageViews = specs.MaxImageViews;

    // pipeline
    gspecs.MaxShaders = specs.MaxShaders;
    gspecs.MaxCompilations = specs.MaxCompilations;
    gspecs.MaxReflections = specs.MaxReflections;
    gspecs.MaxPipelineLayouts = specs.MaxPipelineLayouts;
    gspecs.MaxPipelines = specs.MaxPipelines;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_StorageBuffer] = 1024;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_Sampler] = 2048;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_SampledImage] = 16384;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_CombinedImageSampler] = 2048;
    gspecs.DescriptorPoolSizes[Graph::Descriptor_StorageImage] = 512;
    Graph::Initialize(gspecs);

    if (specs.TaskManager)
        s_TaskManager = specs.TaskManager;
    else if (specs.Flags & InitializationFlag_DefaultTaskManagerSingleThread)
        s_TaskManager = &s_DefaultTaskManager.Get();
    else
        s_TaskManager = &s_DefaultThreadPool.Get();
}
static void core_Terminate()
{
    Graph::Terminate();

    s_DefaultTaskManager.Destruct();
    s_DefaultThreadPool.Destruct();

    allocators_Terminate();
}

void Frontend_Initialize();
void Frontend_Terminate();

void Initialize(const Specs &specs)
{
    TKIT_BEGIN_INFO_CLOCK();
    s_Specs = specs;

    core_Initialize(specs);
    platform_Initialize();
    execution_Initialize();
    descriptors_Initialize();
    Renderer_Initialize();
    Resources_Initialize();

#ifdef ONYX_ENABLE_IMGUI
    ImGuiBackend_Initialize();
#endif

    Frontend_Initialize();

    TKIT_END_INFO_CLOCK(Seconds, "[ONYX][CORE] Done in {:.2f} seconds");
}

void Terminate()
{
    Frontend_Terminate();
#ifdef ONYX_ENABLE_IMGUI
    ImGuiBackend_Terminate();
#endif
    Resources_Terminate();
    Renderer_Terminate();
    descriptors_Terminate();
    execution_Terminate();
    platform_Terminate();
    core_Terminate();
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

const Specs &GetSpecs()
{
    return s_Specs;
}

/////////////////////////////////////////////
/// END CORE
/////////////////////////////////////////////
} // namespace Onyx
