#include "pch.hpp"
#include "onyx/context.hpp"
#include "onyx/overlay.hpp"
#include "onyx/vertex.hpp"
#include "core.hpp"
#include "instance.hpp"
#include "buffer.hpp"
#include "resources.hpp"
#include "renderer.hpp"
#include "conversion.hpp"
#ifndef ONYX_COMPILE_SHADERS_AT_RUNTIME
#    include "spirv.hpp"
#endif
#include "graph/pipeline.hpp"
#include "graph/shader.hpp"
#include "graph/execution.hpp"
#include "graph/pipeline.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/profiling/macros.hpp"

#ifdef ONYX_ENABLE_IMGUI
#    include <imgui.h>
#    include "imgui_backend.hpp"
#    ifdef ONYX_ENABLE_IMPLOT
#        include <implot.h>
#    endif
#endif

namespace Onyx
{
using namespace Detail;
struct Range
{
    u32 Offset = 0;
    u32 Count = 0;
};
struct ContextInstanceRange
{
    usz Offset = 0;
    usz Size = 0;
    ViewMask ViewMask = 0;
    u64 Generation = 0;
    u32 ContextIndex = TKIT_U32_MAX;
};

struct TransferRange
{
    Graph::Tracker Tracker{};

    usz Offset = 0;
    usz Size = 0;
};

// note that graphics ranges, even tho purely gpu resources (not accessed/written by cpu directly) still have gpu usage
// trackers. originally i thought this was needed (dumb, i know) and then i realized i dont need trackers for gpu only
// resources as the sync is already laid out with gpu barriers and semaphores, so i was about to remove them. but ive
// decided to keep them only bc they serve as a good heuristic when tryin to choose resources. if i choose resources not
// in use by the gpu, i may minimize waits on the gpu/semaphore barriers. and because marking these resources as used
// pretty much costs nothing, ive decided to keep them at least for now
struct GraphicsRange
{
    Graph::Tracker TransferTracker{};
    Graph::Tracker GraphicsTracker{};

    usz Offset = 0;
    usz Size = 0;

    // the generation is used to identify ranges. in the case of light, which range is the current one in use for
    // lights. in the case of vertex/index, to map graphics instance ranges with vertex/index graphics ranges
    u64 Generation = 0;

    bool InUseByTransfer() const
    {
        return TransferTracker.InUse();
    }
    bool InUseByGraphics() const
    {
        return GraphicsTracker.InUse();
    }
    bool InUse() const
    {
        return InUseByTransfer() || InUseByGraphics();
    }
};

template <typename T> struct Pool
{
    Graph::Buffer Buffer = Graph::NullHandle;
    TKit::TierArray<T> Ranges{};
};

using TransferPool = Pool<TransferRange>;
using GraphicsPool = Pool<GraphicsRange>;

struct Arena
{
    TransferPool Transfer{};
    GraphicsPool Graphics{};
    // starting at one to honor Active*Range starting as well as one, so that value will never get assigned to any range
    // (++prefix will prevent this value showing up)
    u64 LatestGeneration = 1;
};

using TransferInstanceRange = TransferRange;
struct GraphicsInstanceRange
{
    Graph::Tracker TransferTracker{};
    Graph::Tracker GraphicsTracker{};

    usz Offset = 0;
    usz Size = 0;
    ViewMask ViewMask = 0;

    Resource MeshHandle = NullHandle;

    // only used for dynamic meshes
    // start at one to signal that at the beginning, no vertex/index range are associated with these
    u64 ActiveVertexGeneration = 1;
    u64 ActiveIndexGeneration = 1;

    GraphicsRange *ActiveVertexRange = nullptr;
    GraphicsRange *ActiveIndexRange = nullptr;
    //

    RenderModeFlags RenderFlags = 0;
    BlendPass Blend = BlendPass_None;
    TKit::TierArray<ContextInstanceRange> ContextRanges{};

    bool InUseByTransfer() const
    {
        return TransferTracker.InUse();
    }
    bool InUseByGraphics() const
    {
        return GraphicsTracker.InUse();
    }
    bool InUse() const
    {
        return InUseByTransfer() || InUseByGraphics();
    }
};

using TransferInstancePool = Pool<TransferInstanceRange>;
using GraphicsInstancePool = Pool<GraphicsInstanceRange>;

struct InstanceArena
{
    TransferInstancePool Transfer{};
    GraphicsInstancePool Graphics{};
};

using TransferLightRange = TransferRange;

using TransferLightPool = Pool<TransferLightRange>;
using GraphicsLightPool = Pool<GraphicsRange>;

struct DrawInfo
{
    u32 FirstInstance = 0;
    u32 InstanceCount = 0;
};

using InstanceDrawInfo = DrawInfo;

struct LightArena
{
    TransferLightPool Transfer{};
    GraphicsLightPool Graphics{};

    GraphicsRange *ActiveRange = nullptr;
    // starting at one, while ranges start at 0. meaning no active range will be able to be found
    // start at one to signal that at the beginning, no light range is active
    u64 ActiveGeneration = 1;

    u32 LightCount = 0;
};

struct GeometryData
{
    TKit::FixedArray<InstanceArena, Geometry_Count> Arenas{};
    Arena VertexArena{};
    Arena IndexArena{};
    ten<Graph::Pipeline, BlendPass_Count, PipelinePass_Count, Geometry_Count> Pipelines{};
};

template <typename LightParams> struct ContextLights
{
    TKit::TierArray<LightParams> Parameters{};
    TKit::TierArray<typename LightParams::InstanceData> Data{};
};

template <Dimension D> struct LightInstances
{
    ContextLights<PointLightParameters<D>> Points{};
    ContextLights<DirectionalLightParameters<D>> Directionals{};

    void ClearParameters()
    {
        Points.Parameters.Clear();
        Directionals.Parameters.Clear();
    }
};

template <> struct LightInstances<D3>
{
    ContextLights<PointLightParameters<D3>> Points{};
    ContextLights<DirectionalLightParameters<D3>> Directionals{};
    ContextLights<SpotLightParameters> Spots{};

    void ClearParameters()
    {
        Points.Parameters.Clear();
        Directionals.Parameters.Clear();
        Spots.Parameters.Clear();
    }
};

template <Dimension D> struct LightData
{
    LightInstances<D> Instances{};
    TKit::FixedArray<LightArena, LightTypeCount<D>> Arenas{};
    TKit::FixedArray<Range, LightTypeCount<D>> Ranges{};
};

struct TextureMap
{
    Graph::Image Image = Graph::NullHandle;
    // images have no extent getter in Graph, so it is cached at creation
    u32v2 Extent{0};
    Graph::Tracker Tracker{};
};

using TextureMapArray = TKit::StaticArray<TextureMap, ONYX_MAX_TEXTURE_MAPS>;

template <Dimension D> struct ShadowData;

template <> struct ShadowData<D2>
{
    TKit::FixedArray<TextureMapArray, LightTypeCount<D2>> ShadowMaps{};

    TextureMapArray OcclusionMaps{};

    ten<Graph::Pipeline, Geometry_Count> Pipelines{}; // occlusion pipelines
    // compute pipeline!
    Graph::Pipeline RayMarchPipeline{};
    //
    Graph::Format ShadowFormat = Graph::Format_Undefined; // not usually supported when used as 1D
    Graph::DescriptorSet RayMarchSet = Graph::NullHandle;
    ViewMask DirtyShadowViews = 0;

    static constexpr Graph::Format OcclusionFormat = Graph::Format_R8_UNORM;
    static constexpr Graph::Format PrimaryShadowFormat =
        Graph::Format_D16_UNORM; // not usually supported when used as 1D
                                 // storage. thats why a fallback is provided
    static constexpr Graph::Format FallbackShadowFormat = Graph::Format_R16_UNORM;
    static constexpr u32 OcclusionResolution = 1024;
    static constexpr TKit::FixedArray<u32, LightTypeCount<D2>> ShadowResolutions{1024, 1024};
};

using ShadowMapId = u32;

template <> struct ShadowData<D3>
{
    TKit::FixedArray<TextureMapArray, LightTypeCount<D3>> ShadowMaps{};

    ten<Graph::Pipeline, Geometry_Count> Pipelines{};
    ViewMask DirtyShadowViews = 0;

    static constexpr Graph::Format ShadowFormat = Graph::Format_D32_SFLOAT;
    static constexpr TKit::FixedArray<u32, LightTypeCount<D3>> ShadowResolutions{512, 2048, 1024};
};

template <Dimension D> struct ContextInfo
{
    RenderContext<D> *Context = nullptr;
    u64 Generation = 0;

    bool IsDirty() const
    {
        return Context->IsDirty(Generation);
    }
};

template <Dimension D> struct RendererData
{
    TKit::TierArray<ContextInfo<D>> Contexts{};
    TKit::TierArray<Graph::BufferMemoryBarrierInfo> AcquireBarriers{};
    ten<Graph::DescriptorSet, RenderPass_Count, Geometry_Count> Descriptors{};

    GeometryData Geometry{};
    LightData<D> Lights{};
    ShadowData<D> Shadows{};

    bool IsContextRangeClean(const ContextInstanceRange &crange) const
    {
        return crange.ContextIndex != TKIT_U32_MAX &&
               !Contexts[crange.ContextIndex].Context->IsDirty(crange.Generation);
    }
    bool IsContextRangeClean(const ViewMask viewBit, const ContextInstanceRange &crange) const
    {
        return (crange.ViewMask & viewBit) && crange.ContextIndex != TKIT_U32_MAX &&
               !Contexts[crange.ContextIndex].Context->IsDirty(crange.Generation);
    }
    bool IsAnyContextRangeClean(const GraphicsInstanceRange &grange) const
    {
        for (const ContextInstanceRange &crange : grange.ContextRanges)
            if (IsContextRangeClean(crange))
                return true;
        return false;
    }
    bool IsAnyContextRangeDirty(const GraphicsInstanceRange &grange) const
    {
        for (const ContextInstanceRange &crange : grange.ContextRanges)
            if (!IsContextRangeClean(crange))
                return true;
        return false;
    }
    bool AreAllContextRangesClean(const GraphicsInstanceRange &grange) const
    {
        for (const ContextInstanceRange &crange : grange.ContextRanges)
            if (!IsContextRangeClean(crange))
                return false;
        return true;
    }
    bool AreAllContextRangesDirty(const GraphicsInstanceRange &grange) const
    {
        for (const ContextInstanceRange &crange : grange.ContextRanges)
            if (IsContextRangeClean(crange))
                return false;
        return true;
    }
};

struct DrawBuffer
{
    Graph::Buffer Buffer = Graph::NullHandle;
    Graph::Tracker Tracker{};
};

struct ShaderData
{
    TKit::FixedArray<Graph::Shader, Geometry_Count> VertexShaders{};
    TKit::FixedArray<Graph::Shader, Geometry_Count> OpaqueFragmentShaders{};
    TKit::FixedArray<Graph::Shader, Geometry_Count> TransparentFragmentShaders{};

    ShaderData()
    {
        for (Graph::Shader &sh : VertexShaders)
            sh = Graph::NullHandle;
        for (Graph::Shader &sh : OpaqueFragmentShaders)
            sh = Graph::NullHandle;
        for (Graph::Shader &sh : TransparentFragmentShaders)
            sh = Graph::NullHandle;
    }

    void Destroy()
    {
        for (const Graph::Shader sh : VertexShaders)
            if (sh != Graph::NullHandle)
                Graph::Shader_Destroy(sh);
        for (const Graph::Shader sh : OpaqueFragmentShaders)
            if (sh != Graph::NullHandle)
                Graph::Shader_Destroy(sh);
        for (const Graph::Shader sh : TransparentFragmentShaders)
            if (sh != Graph::NullHandle)
                Graph::Shader_Destroy(sh);
    }
};

struct StandalonePipelineData
{
    Graph::PipelineLayout Layout = Graph::NullHandle;
    Graph::Shader Shader = Graph::NullHandle;
};

struct PipelineData
{
    ten<Graph::PipelineLayout, D_Count, RenderPass_Count> Layouts{};
    ten<ShaderData, D_Count, RenderPass_Count> Shaders{};

    Graph::Shader FullPassVertexShader = Graph::NullHandle;
    TKit::FixedArray<StandalonePipelineData, StandalonePass_Count> Standalone{};
};

static TKit::Storage<PipelineData> s_PipelineData{};
static TKit::Storage<TKit::TierArray<DrawBuffer>> s_DrawBuffers{};
static TKit::Storage<TKit::TierArray<DrawBuffer>> s_IndexedDrawBuffers{};

static TKit::Storage<RendererData<D2>> s_RendererData2{};
static TKit::Storage<RendererData<D3>> s_RendererData3{};
static Graph::Pipeline s_BlendPipeline = Graph::NullHandle;
static Graph::Pipeline s_PostProcessPipeline = Graph::NullHandle;
static Graph::Pipeline s_CompositorPipeline = Graph::NullHandle;

static Graph::Sampler s_LinearSampler = Graph::NullHandle;
static Graph::Sampler s_CompareSampler = Graph::NullHandle;
static Graph::Sampler s_NearSampler = Graph::NullHandle;

static u64 s_SyncPointCount = 0;

template <Dimension D> static ShaderData &getShaders(const RenderPass pass)
{
    return s_PipelineData->Shaders[D - 2][pass];
}

template <Dimension D> static RendererData<D> &getRendererData()
{
    if constexpr (D == D2)
        return *s_RendererData2;
    else
        return *s_RendererData3;
}

template <Dimension D> static Graph::PipelineLayout getPipelineLayout(const RenderPass pass)
{
    return s_PipelineData->Layouts[D - 2][pass];
}
Graph::PipelineLayout getPipelineLayout(const StandalonePass pass)
{
    return s_PipelineData->Standalone[pass].Layout;
}

template <Dimension D> static u32 getLightSize(const LightType light)
{
    switch (light)
    {
    case Light_Point:
        return sizeof(PointLightData<D>);
    case Light_Directional:
        return sizeof(DirectionalLightData<D>);
    case Light_Spot:
        return sizeof(SpotLightData);
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized light type");
        return 0;
    }
}

template <Dimension D> static void updateInstanceDescriptorSets(const Geometry geo)
{
    RendererData<D> &rdata = getRendererData<D>();
    const Graph::DescriptorBufferInfo info =
        Graph::DescriptorBufferInfo::Create(rdata.Geometry.Arenas[geo].Graphics.Buffer);
    for (u32 i = 0; i < RenderPass_Count; ++i)
        Graph::DescriptorSet_Write(rdata.Descriptors[i][geo], ONYX_INSTANCES_BINDING_POINT, info);
}

static u32 lightToBinding(const LightType light)
{
    switch (light)
    {
    case Light_Point:
        return ONYX_POINT_LIGHTS_BINDING_POINT;
    case Light_Directional:
        return ONYX_DIRECTIONAL_LIGHTS_BINDING_POINT;
    case Light_Spot:
        return ONYX_SPOT_LIGHTS_BINDING_POINT;
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized light type: {}", u8(light));
        return 0;
    }
}

static void createPipelineLayouts()
{
    constexpr u32 Dim2 = 0;
    constexpr u32 Dim3 = 1;

    constexpr Graph::ShaderStageFlags vertex = Graph::ShaderStageFlag_Vertex;
    constexpr Graph::ShaderStageFlags fragment = Graph::ShaderStageFlag_Fragment;
    constexpr Graph::ShaderStageFlags compute = Graph::ShaderStageFlag_Compute;

    const auto create = [](const Graph::DescriptorSetLayout set, const Graph::ShaderStageFlags stages, const u32 size) {
        const Graph::PushConstantRange range = Graph::PushConstantRange::Create(stages, size);
        return Graph::PipelineLayout_Create({.DescriptorSetLayouts = set, .PushRanges = range});
    };

    s_PipelineData->Layouts[Dim2][RenderPass_Shaded] =
        create(GetDescriptorLayout<D2>(RenderPass_Shaded), vertex | fragment, sizeof(ShadedPushConstantData<D2>));

    s_PipelineData->Layouts[Dim3][RenderPass_Shaded] =
        create(GetDescriptorLayout<D3>(RenderPass_Shaded), vertex | fragment, sizeof(ShadedPushConstantData<D3>));

    s_PipelineData->Layouts[Dim2][RenderPass_Flat] =
        create(GetDescriptorLayout<D2>(RenderPass_Flat), vertex, sizeof(FlatPushConstantData));

    s_PipelineData->Layouts[Dim3][RenderPass_Flat] =
        create(GetDescriptorLayout<D3>(RenderPass_Flat), vertex, sizeof(FlatPushConstantData));

    s_PipelineData->Layouts[Dim2][RenderPass_Shadow] =
        create(GetDescriptorLayout<D2>(RenderPass_Shadow), vertex, sizeof(ShadowPushConstantData<D2>));

    s_PipelineData->Layouts[Dim3][RenderPass_Shadow] =
        create(GetDescriptorLayout<D3>(RenderPass_Shadow), vertex | fragment, sizeof(ShadowPushConstantData<D3>));

    s_PipelineData->Standalone[StandalonePass_RayMarch].Layout =
        create(GetDescriptorLayout(StandalonePass_RayMarch), compute, sizeof(RayMarchPushConstantData));

    s_PipelineData->Standalone[StandalonePass_Blend].Layout =
        create(GetDescriptorLayout(StandalonePass_Blend), fragment, sizeof(BlendPushConstantData));

    s_PipelineData->Standalone[StandalonePass_PostProcess].Layout =
        create(GetDescriptorLayout(StandalonePass_PostProcess), fragment, sizeof(PostProcessPushConstantData));

    s_PipelineData->Standalone[StandalonePass_Compositor].Layout =
        create(GetDescriptorLayout(StandalonePass_Compositor), fragment, sizeof(CompositorPushConstantData));

    if (Graph::IsValidationEnabled())
    {
        s_PipelineData->Layouts.IterateMultiIndex([&](const u32 i, const u32 j) {
            Graph::PipelineLayout_SetName(
                s_PipelineData->Layouts[i][j],
                TKit::StackString::Format("onyx-pipeline-layout-{}D-{}", i + 2, ToString(RenderPass(j))).CString());
        });
        Graph::PipelineLayout_SetName(s_PipelineData->Standalone[StandalonePass_RayMarch].Layout,
                                      "onyx-ray-march-pipeline-layout");
        Graph::PipelineLayout_SetName(s_PipelineData->Standalone[StandalonePass_Blend].Layout,
                                      "onyx-blend-pipeline-layout");
        Graph::PipelineLayout_SetName(s_PipelineData->Standalone[StandalonePass_PostProcess].Layout,
                                      "onyx-post-process-pipeline-layout");
        Graph::PipelineLayout_SetName(s_PipelineData->Standalone[StandalonePass_Compositor].Layout,
                                      "onyx-compositor-pipeline-layout");
    }
}

#ifdef ONYX_COMPILE_SHADERS_AT_RUNTIME
static bool isOldMesa()
{
    const Graph::DeviceInfo dev = Graph::GetRenderBackendInfo().Device;
    if (dev.Driver != Graph::Driver_MesaANV && dev.Driver != Graph::Driver_MesaLLVMPipe)
        return false;

    u32 major;
    u32 minor;
    u32 patch;

#    ifdef TKIT_COMPILER_MSVC
    if (sscanf_s(dev.DriverInfo, "Mesa %u.%u.%u", &major, &minor, &patch) != 3)
        return false;
#    else
    if (std::sscanf(dev.DriverInfo, "Mesa %u.%u.%u", &major, &minor, &patch) != 3)
        return false;
#    endif

    if (major > 25)
        return false;
    if (major == 25 && minor > 3)
        return false;
    if (major == 25 && minor == 3 && patch >= 3)
        return false;
    return true;
}
#else
static Graph::Shader shaderFromBinary(const ShaderBinary &binary)
{
    return Graph::Shader_Create(rcast<const u32 *>(binary.Code), binary.Count);
}
#endif

// at some point ill need this
// static void reloadShaders()
// {
//     destroyShaders();
//     createShaders();
// }

static void createShaders()
{
#ifdef ONYX_COMPILE_SHADERS_AT_RUNTIME
    TKit::StackArray<Graph::ShaderArgument, 2> args{};
    if (isOldMesa())
    {
        TKIT_LOG_WARNING("[ONYX][SHADERS] Old mesa version detected (pre-25.3.3) which contains a bug regarding "
                         "optimized spir-v code. Setting optimizations to 0 as a fix");
        args.Append(Graph::ShaderArgument::Create(Graph::ShaderArgument_Optimization, 0));
    }
    else
        args.Append(Graph::ShaderArgument::Create(Graph::ShaderArgument_Optimization, 3));
    if (Graph::IsValidationEnabled())
        args.Append(Graph::ShaderArgument::Create(Graph::ShaderArgument_DebugInformation));

    const char *const searchPaths[] = {ONYX_SHADERS_PATH};

    const TKit::FixedArray<TKit::TierString, Geometry_Count> geos = {"circle", "static", "parametric", "glyph",
                                                                     "dynamic"};
    const TKit::FixedArray<TKit::TierString, RenderPass_Count> passes = {"flat", "shaded", "shadow"};
    const TKit::FixedArray<TKit::TierString, D_Count> dims = {"2D", "3D"};

    const Graph::EntryPoint geometryEntries[] = {{"mainVS", Graph::ShaderStageFlag_Vertex},
                                                 {"mainFSO", Graph::ShaderStageFlag_Fragment},
                                                 {"mainFST", Graph::ShaderStageFlag_Fragment}};
    const Graph::EntryPoint fullVertexEntries[] = {{"mainVS", Graph::ShaderStageFlag_Vertex}};
    const Graph::EntryPoint rayMarchEntries[] = {{"main", Graph::ShaderStageFlag_Compute}};
    const Graph::EntryPoint fragmentEntries[] = {{"mainFS", Graph::ShaderStageFlag_Fragment}};

    // module names and the module array must not reallocate, since the compilation specs point into them
    TKit::StackArray<TKit::TierString> names{};
    names.Reserve(u32(Geometry_Count) * u32(RenderPass_Count) * u32(D_Count));

    TKit::StackArray<Graph::ShaderModule> modules{};
    modules.Reserve(u32(Geometry_Count) * u32(RenderPass_Count) * u32(D_Count) + 5);

    // TODO(Isma): Would be nice to parallelize this
    for (const TKit::TierString &geo : geos)
        for (u32 rpass = 0; rpass < passes.GetSize(); ++rpass)
            for (const TKit::TierString &dim : dims)
            {
                const TKit::TierString &name = names.Append(geo + "-" + passes[rpass] + "-" + dim);
                const u32 count = rpass != RenderPass_Shadow ? 3 : 2;
                modules.Append(Graph::ShaderModule::Create(
                    name.GetData(), TKit::Span<const Graph::EntryPoint>{geometryEntries, count}));
            }

    modules.Append(Graph::ShaderModule::Create("full-vertex", fullVertexEntries));
    modules.Append(Graph::ShaderModule::Create("ray-march", rayMarchEntries));
    modules.Append(Graph::ShaderModule::Create("blend", fragmentEntries));
    modules.Append(Graph::ShaderModule::Create("post-process", fragmentEntries));
    modules.Append(Graph::ShaderModule::Create("compositor", fragmentEntries));

    Graph::CompilationSpecs cspecs{};
    cspecs.Modules = modules;
    cspecs.Arguments = args;
    cspecs.SearchPaths = searchPaths;
    const Graph::Compilation cmp = ONYX_CHECK_RESULT(Graph::Compilation_Create(cspecs));

    u32 idx = 0;
    const auto createShader = [&](const Geometry geo, ShaderData &data, const bool hasTransparent) {
        const TKit::TierString &name = names[idx++];
        data.VertexShaders[geo] = Graph::Compilation_CreateShader(cmp, "mainVS", name.GetData());
        data.OpaqueFragmentShaders[geo] = Graph::Compilation_CreateShader(cmp, "mainFSO", name.GetData());
        if (hasTransparent)
            data.TransparentFragmentShaders[geo] = Graph::Compilation_CreateShader(cmp, "mainFST", name.GetData());

        if (Graph::IsValidationEnabled())
        {
            const TKit::TierString v = "onyx-vertex-shader-" + name;
            const TKit::TierString of = "onyx-opaque-fragment-shader-" + name;
            Graph::Shader_SetName(data.VertexShaders[geo], v.CString());
            Graph::Shader_SetName(data.OpaqueFragmentShaders[geo], of.CString());
            if (hasTransparent)
            {
                const TKit::TierString tf = "onyx-transparent-fragment-shader-" + name;
                Graph::Shader_SetName(data.TransparentFragmentShaders[geo], tf.CString());
            }
        }
    };

    for (u32 i = 0; i < geos.GetSize(); ++i)
    {
        const Geometry geo = Geometry(i);
        for (u32 j = 0; j < passes.GetSize(); ++j)
        {
            const RenderPass rpass = RenderPass(j);
            createShader(geo, getShaders<D2>(rpass), rpass != RenderPass_Shadow);
            createShader(geo, getShaders<D3>(rpass), rpass != RenderPass_Shadow);
        }
    }

    s_PipelineData->FullPassVertexShader = Graph::Compilation_CreateShader(cmp, "mainVS", "full-vertex");
    s_PipelineData->Standalone[StandalonePass_RayMarch].Shader =
        Graph::Compilation_CreateShader(cmp, "main", "ray-march");
    s_PipelineData->Standalone[StandalonePass_Blend].Shader = Graph::Compilation_CreateShader(cmp, "mainFS", "blend");
    s_PipelineData->Standalone[StandalonePass_PostProcess].Shader =
        Graph::Compilation_CreateShader(cmp, "mainFS", "post-process");
    s_PipelineData->Standalone[StandalonePass_Compositor].Shader =
        Graph::Compilation_CreateShader(cmp, "mainFS", "compositor");

    Graph::Compilation_Destroy(cmp);
#else
    for (u32 dim = 0; dim < D_Count; ++dim)
        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            for (u32 rpass = 0; rpass < RenderPass_Count; ++rpass)
            {
                ShaderData &sdata = s_PipelineData->Shaders[dim][rpass];
                sdata.VertexShaders[geo] = shaderFromBinary(g_ShaderBinaryData.VertexShaders[dim][rpass][geo]);
                sdata.OpaqueFragmentShaders[geo] =
                    shaderFromBinary(g_ShaderBinaryData.OpaqueFragmentShaders[dim][rpass][geo]);
                if (rpass != RenderPass_Shadow)
                    sdata.TransparentFragmentShaders[geo] =
                        shaderFromBinary(g_ShaderBinaryData.TransparentFragmentShaders[dim][rpass][geo]);
            }

    s_PipelineData->FullPassVertexShader = shaderFromBinary(g_ShaderBinaryData.FullVertex);

    s_PipelineData->Standalone[StandalonePass_RayMarch].Shader = shaderFromBinary(g_ShaderBinaryData.RayMarch);
    s_PipelineData->Standalone[StandalonePass_Blend].Shader = shaderFromBinary(g_ShaderBinaryData.Blend);
    s_PipelineData->Standalone[StandalonePass_PostProcess].Shader = shaderFromBinary(g_ShaderBinaryData.PostProcess);
    s_PipelineData->Standalone[StandalonePass_Compositor].Shader = shaderFromBinary(g_ShaderBinaryData.Compositor);
#endif
}

static void destroyShaders()
{
    for (auto &sdata : s_PipelineData->Shaders)
        sdata.Destroy();

    Graph::Shader_Destroy(s_PipelineData->FullPassVertexShader);
    for (const auto &st : s_PipelineData->Standalone)
        Graph::Shader_Destroy(st.Shader);
}

template <typename T> static Graph::VertexBinding vertexBinding()
{
    return {u32(sizeof(T)), Graph::VertexInputRate_Vertex};
}
static Graph::VertexAttribute vertexAttribute(const Graph::Format format, const u32 offset)
{
    return {.Binding = 0, .Offset = offset, .Format = format};
}
static void setLocation(TKit::StaticArray8<Graph::VertexAttribute> &atts)
{
    for (u32 i = 0; i < atts.GetSize(); ++i)
        atts[i].Location = i;
}

template <Dimension D> static void updateLightDescriptorSets(const LightType light)
{
    RendererData<D> &rdata = getRendererData<D>();
    const Graph::DescriptorBufferInfo info =
        Graph::DescriptorBufferInfo::Create(rdata.Lights.Arenas[light].Graphics.Buffer);
    for (const Graph::DescriptorSet set : rdata.Descriptors[RenderPass_Shaded])
        Graph::DescriptorSet_Write(set, lightToBinding(light), info);
}

static bool isDepth2DSupported()
{
    return s_RendererData2->Shadows.ShadowFormat == ShadowData<D2>::PrimaryShadowFormat;
}

template <Dimension D>
static Graph::Pipeline createGeometryPipeline(const PipelinePass pass, const BlendPass bpass, const Geometry geo)
{
    constexpr Graph::Format f1u = Graph::Format_R32_UINT;
    constexpr Graph::Format f2 = Graph::Format_R32G32_SFLOAT;
    constexpr Graph::Format f3 = Graph::Format_R32G32B32_SFLOAT;
    constexpr Graph::Format f4 = Graph::Format_R32G32B32A32_SFLOAT;

    const RenderPass rpass = GetRenderPass(pass);
    const ShaderData &shaders = getShaders<D>(rpass);

    const bool opaque = bpass == BlendPass_Opaque;
    const bool opaqueParams = opaque && geo == Geometry_Glyph;
    const bool needsConstant = D == D2 && pass == PipelinePass_Shaded;

    const u32 depth2 = u32(isDepth2DSupported());
    const Graph::SpecializationEntry entry{0, 0, sizeof(u32)};

    Graph::ShaderStageInfo stages[2]{};
    stages[0].Shader = shaders.VertexShaders[geo];
    stages[0].Stage = Graph::ShaderStageFlag_Vertex;
    stages[1].Shader = opaque ? shaders.OpaqueFragmentShaders[geo] : shaders.TransparentFragmentShaders[geo];
    stages[1].Stage = Graph::ShaderStageFlag_Fragment;
    if (needsConstant)
    {
        stages[1].Specialization.Entries = entry;
        stages[1].Specialization.Data = &depth2;
        stages[1].Specialization.DataSize = sizeof(u32);
    }

    const Graph::Format cf = GetAttachmentFormat(Attachment_Intermediate);
    const Graph::Format tf = GetAttachmentFormat(Attachment_Transparent);
    const Graph::Format rf = GetAttachmentFormat(Attachment_Revealage);

    const Graph::ColorWriteMask full = Graph::ColorWrite_All;
    const Graph::ColorWriteMask mainMask = pass != PipelinePass_Outlined ? full : 0;
    const Graph::ColorWriteMask outlineMask = pass == PipelinePass_Outlined ? full : 0;

    TKit::FixedArray<Graph::ColorAttachment, 3> attachments{};

    Graph::ColorAttachment &color = attachments[0];
    color.BlendEnabled = !opaque || geo == Geometry_Glyph;
    color.SrcColor = opaqueParams ? Graph::BlendFactor_SrcAlpha : Graph::BlendFactor_One;
    color.DstColor = opaqueParams ? Graph::BlendFactor_OneMinusSrcAlpha : Graph::BlendFactor_One;
    color.SrcAlpha = Graph::BlendFactor_One;
    color.DstAlpha = opaqueParams ? Graph::BlendFactor_OneMinusSrcAlpha : Graph::BlendFactor_One;
    color.WriteMask = mainMask;
    color.Format = opaque ? cf : tf;

    attachments[1].WriteMask = outlineMask;
    attachments[1].Format = cf;

    if (!opaque)
    {
        Graph::ColorAttachment &revealage = attachments[2];
        revealage.BlendEnabled = true;
        revealage.SrcColor = Graph::BlendFactor_Zero;
        revealage.DstColor = Graph::BlendFactor_OneMinusSrcColor;
        revealage.WriteMask = mainMask;
        revealage.Format = rf;
    }

    TKit::StaticArray8<Graph::VertexBinding> bindings{};
    TKit::StaticArray8<Graph::VertexAttribute> attributes{};
    switch (geo)
    {
    case Geometry_Circle:
        break;
    case Geometry_Static:
        bindings.Append(vertexBinding<StaticVertex<D>>());
        if constexpr (D == D2)
        {
            attributes.Append(vertexAttribute(f2, offsetof(StaticVertex<D2>, Position)));
            attributes.Append(vertexAttribute(f2, offsetof(StaticVertex<D2>, TexCoord)));
        }
        else
        {
            attributes.Append(vertexAttribute(f3, offsetof(StaticVertex<D3>, Position)));
            attributes.Append(vertexAttribute(f2, offsetof(StaticVertex<D3>, TexCoord)));
            if (pass == PipelinePass_Shaded)
            {
                attributes.Append(vertexAttribute(f3, offsetof(StaticVertex<D3>, Normal)));
                attributes.Append(vertexAttribute(f4, offsetof(StaticVertex<D3>, Tangent)));
            }
        }
        break;
    case Geometry_Parametric:
        bindings.Append(vertexBinding<ParametricVertex<D>>());
        if constexpr (D == D2)
        {
            attributes.Append(vertexAttribute(f2, offsetof(ParametricVertex<D2>, Position)));
            attributes.Append(vertexAttribute(f2, offsetof(ParametricVertex<D2>, TexCoord)));
            attributes.Append(vertexAttribute(f1u, offsetof(ParametricVertex<D2>, Region)));
        }
        else
        {
            attributes.Append(vertexAttribute(f3, offsetof(ParametricVertex<D3>, Position)));
            attributes.Append(vertexAttribute(f2, offsetof(ParametricVertex<D3>, TexCoord)));
            attributes.Append(vertexAttribute(f3, offsetof(ParametricVertex<D3>, Normal)));
            if (pass == PipelinePass_Shaded)
                attributes.Append(vertexAttribute(f4, offsetof(ParametricVertex<D3>, Tangent)));
            attributes.Append(vertexAttribute(f1u, offsetof(ParametricVertex<D3>, Region)));
        }
        break;
    case Geometry_Glyph:
        bindings.Append(vertexBinding<GlyphVertex>());
        attributes.Append(vertexAttribute(f2, offsetof(GlyphVertex, Position)));
        attributes.Append(vertexAttribute(f2, offsetof(GlyphVertex, AtlasCoord)));
        attributes.Append(vertexAttribute(f2, offsetof(GlyphVertex, TexCoord)));
        break;
    case Geometry_Dynamic:
        bindings.Append(vertexBinding<DynamicVertex<D>>());
        if constexpr (D == D2)
        {
            attributes.Append(vertexAttribute(f2, offsetof(DynamicVertex<D2>, Position)));
            attributes.Append(vertexAttribute(f2, offsetof(DynamicVertex<D2>, TexCoord)));
            attributes.Append(vertexAttribute(f1u, offsetof(DynamicVertex<D2>, Color)));
        }
        else
        {
            attributes.Append(vertexAttribute(f3, offsetof(DynamicVertex<D3>, Position)));
            attributes.Append(vertexAttribute(f2, offsetof(DynamicVertex<D3>, TexCoord)));
            if (pass == PipelinePass_Shaded)
            {
                attributes.Append(vertexAttribute(f3, offsetof(DynamicVertex<D3>, Normal)));
                attributes.Append(vertexAttribute(f4, offsetof(DynamicVertex<D3>, Tangent)));
            }
            attributes.Append(vertexAttribute(f1u, offsetof(DynamicVertex<D3>, Color)));
        }
        break;
    default:
        TKIT_FATAL("[ONYX][PIPELINES] Unrecognized geometry {}", u8(geo));
        return Graph::NullHandle;
    }

    setLocation(attributes);
    Graph::GraphicsPipelineSpecs specs{};
    specs.ShaderStages = stages;
    specs.VertexBindings = bindings;
    specs.VertexAttributes = attributes;
    specs.ColorAttachments = {attachments.GetData(), u32(opaque ? 2 : 3)};
    specs.DepthFormat = GetAttachmentFormat(Attachment_DepthStencil);
    specs.StencilFormat = specs.DepthFormat;

    const bool depth = pass == PipelinePass_Shaded || pass == PipelinePass_Flat;
    specs.DepthStencil.DepthTestEnabled = depth;
    specs.DepthStencil.DepthWriteEnabled = depth && opaque;

    if (pass == PipelinePass_Outlined)
    {
        Graph::StencilOpState stencil{};
        stencil.PassOp = Graph::StencilOp_IncrementAndClamp;
        stencil.CompareOp = Graph::CompareOp_Always;
        stencil.CompareMask = 0xFF;
        stencil.WriteMask = 0xFF;
        specs.DepthStencil.StencilTestEnabled = true;
        specs.DepthStencil.Front = stencil;
        specs.DepthStencil.Back = stencil;
    }

    const bool dynCull = D == D3 && geo != Geometry_Circle && geo != Geometry_Glyph;
    specs.DynamicState = dynCull ? Graph::DynamicStateFlag_CullMode : 0;

    return Graph::Pipeline_CreateGraphics(getPipelineLayout<D>(rpass), specs);
}

template <Dimension D> static Graph::Pipeline createShadowPipeline(const Geometry geo, const Graph::Format format)
{
    constexpr Graph::Format f1u = Graph::Format_R32_UINT;
    constexpr Graph::Format f2 = Graph::Format_R32G32_SFLOAT;
    constexpr Graph::Format f3 = Graph::Format_R32G32B32_SFLOAT;

    const ShaderData &shaders = getShaders<D>(RenderPass_Shadow);

    Graph::ShaderStageInfo stages[2]{};
    stages[0].Shader = shaders.VertexShaders[geo];
    stages[0].Stage = Graph::ShaderStageFlag_Vertex;
    stages[1].Shader = shaders.OpaqueFragmentShaders[geo];
    stages[1].Stage = Graph::ShaderStageFlag_Fragment;

    TKit::StaticArray8<Graph::VertexBinding> bindings{};
    TKit::StaticArray8<Graph::VertexAttribute> attributes{};
    switch (geo)
    {
    case Geometry_Circle:
        break;
    case Geometry_Static:
        bindings.Append(vertexBinding<StaticVertex<D>>());
        if constexpr (D == D2)
            attributes.Append(vertexAttribute(f2, offsetof(StaticVertex<D2>, Position)));
        else
            attributes.Append(vertexAttribute(f3, offsetof(StaticVertex<D3>, Position)));
        break;
    case Geometry_Parametric:
        bindings.Append(vertexBinding<ParametricVertex<D>>());
        if constexpr (D == D2)
        {
            attributes.Append(vertexAttribute(f2, offsetof(ParametricVertex<D2>, Position)));
            attributes.Append(vertexAttribute(f1u, offsetof(ParametricVertex<D2>, Region)));
        }
        else
        {
            attributes.Append(vertexAttribute(f3, offsetof(ParametricVertex<D3>, Position)));
            attributes.Append(vertexAttribute(f3, offsetof(ParametricVertex<D3>, Normal)));
            attributes.Append(vertexAttribute(f1u, offsetof(ParametricVertex<D3>, Region)));
        }
        break;
    case Geometry_Glyph:
        bindings.Append(vertexBinding<GlyphVertex>());
        attributes.Append(vertexAttribute(f2, offsetof(GlyphVertex, Position)));
        attributes.Append(vertexAttribute(f2, offsetof(GlyphVertex, AtlasCoord)));
        break;
    case Geometry_Dynamic:
        bindings.Append(vertexBinding<DynamicVertex<D>>());
        if constexpr (D == D2)
            attributes.Append(vertexAttribute(f2, offsetof(DynamicVertex<D2>, Position)));
        else
            attributes.Append(vertexAttribute(f3, offsetof(DynamicVertex<D3>, Position)));
        break;
    default:
        TKIT_FATAL("[ONYX][PIPELINES] Unrecognized geometry {}", u8(geo));
        return Graph::NullHandle;
    }

    Graph::ColorAttachment attachment{};
    attachment.WriteMask = Graph::ColorWrite_R;
    attachment.Format = format;

    setLocation(attributes);
    Graph::GraphicsPipelineSpecs specs{};
    specs.ShaderStages = stages;
    specs.VertexBindings = bindings;
    specs.VertexAttributes = attributes;

    if constexpr (D == D2)
    {
        specs.ColorAttachments = attachment;
        specs.DepthStencil.DepthTestEnabled = false;
        specs.DepthStencil.DepthWriteEnabled = false;
    }
    else
    {
        specs.DepthFormat = format;
        specs.DepthStencil.DepthTestEnabled = true;
        specs.DepthStencil.DepthWriteEnabled = true;
        if (geo != Geometry_Circle && geo != Geometry_Glyph)
            specs.DynamicState = Graph::DynamicStateFlag_CullMode;
    }
    return Graph::Pipeline_CreateGraphics(getPipelineLayout<D>(RenderPass_Shadow), specs);
}

static Graph::Pipeline createRayMarchPipeline()
{
    const StandalonePipelineData &data = s_PipelineData->Standalone[StandalonePass_RayMarch];
    Graph::ShaderStageInfo stage{};
    stage.Shader = data.Shader;
    stage.Stage = Graph::ShaderStageFlag_Compute;
    return Graph::Pipeline_CreateCompute(data.Layout, stage);
}

static Graph::Pipeline createFullPassPipeline(const StandalonePass pass, const Graph::ColorAttachment &attachment)
{
    const StandalonePipelineData &data = s_PipelineData->Standalone[pass];

    Graph::ShaderStageInfo stages[2]{};
    stages[0].Shader = s_PipelineData->FullPassVertexShader;
    stages[0].Stage = Graph::ShaderStageFlag_Vertex;
    stages[1].Shader = data.Shader;
    stages[1].Stage = Graph::ShaderStageFlag_Fragment;

    Graph::GraphicsPipelineSpecs specs{};
    specs.ShaderStages = stages;
    specs.ColorAttachments = attachment;
    specs.DepthStencil.DepthTestEnabled = false;
    specs.DepthStencil.DepthWriteEnabled = false;
    return Graph::Pipeline_CreateGraphics(data.Layout, specs);
}

static Graph::Pipeline createBlendPipeline()
{
    Graph::ColorAttachment attachment{};
    attachment.BlendEnabled = true;
    attachment.SrcColor = Graph::BlendFactor_SrcAlpha;
    attachment.DstColor = Graph::BlendFactor_OneMinusSrcAlpha;
    // NOTE(Isma, 25/06/06): Should consider not writing to the alpha channel. Causes alpha values to be
    // inherited. For now it actually lets us having modals in ui, so ill keep it. but its a bit
    // counterintuitive
    attachment.SrcAlpha = Graph::BlendFactor_One;
    attachment.DstAlpha = Graph::BlendFactor_OneMinusSrcAlpha;
    attachment.Format = GetAttachmentFormat(Attachment_Intermediate);
    return createFullPassPipeline(StandalonePass_Blend, attachment);
}

static Graph::Pipeline createPostProcessPipeline()
{
    return createFullPassPipeline(StandalonePass_PostProcess, {.Format = GetAttachmentFormat(Attachment_Final)});
}

static Graph::Pipeline createCompositorPipeline()
{
    Graph::ColorAttachment attachment{};
    attachment.BlendEnabled = true;
    attachment.Format = SurfaceFormat;
    return createFullPassPipeline(StandalonePass_Compositor, attachment);
}

template <Dimension D>
static Graph::Buffer createTransferInstanceBuffer(const Geometry geo,
                                                  const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const Graph::BufferFlags flags = Graph::BufferFlags(Buffer_Staging) | Graph::BufferFlag_Destination;
    const Graph::Buffer buffer = Graph::Buffer_Create(GetInstanceSize<D>(geo) * instances, flags);

    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-transfer-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}

template <Dimension D>
static Graph::Buffer createTransferLightBuffer(const LightType light,
                                               const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const Graph::Buffer buffer = Graph::Buffer_Create(instances * getLightSize<D>(light), Buffer_Staging);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-transfer-light-buffer-{}D-type-{}", u8(D), ToString(light));
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}

template <Dimension D>
static Graph::Buffer createTransferVertexBuffer(const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const Graph::BufferFlags flags = Graph::BufferFlag_Vertex | Graph::BufferFlag_HostMapped |
                                     Graph::BufferFlag_HostRandomAccess | Graph::BufferFlag_Staging |
                                     Graph::BufferFlag_Destination;
    const Graph::Buffer buffer = Graph::Buffer_Create(instances * sizeof(DynamicVertex<D>), flags);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-renderer-transfer-vertex-buffer-{}D", u8(D));
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}

template <Dimension D>
static Graph::Buffer createTransferIndexBuffer(const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const Graph::BufferFlags flags = Graph::BufferFlag_Index | Graph::BufferFlag_HostMapped |
                                     Graph::BufferFlag_HostRandomAccess | Graph::BufferFlag_Staging |
                                     Graph::BufferFlag_Destination;
    const Graph::Buffer buffer = Graph::Buffer_Create(instances * sizeof(Index), flags);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-renderer-transfer-index-buffer-{}D", u8(D));
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}

template <Dimension D>
static Graph::Buffer createGraphicsInstanceBuffer(const Geometry geo,
                                                  const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const Graph::BufferFlags flags = Graph::BufferFlags(Buffer_DeviceStorage) | Graph::BufferFlag_Source;
    const Graph::Buffer buffer = Graph::Buffer_Create(instances * GetInstanceSize<D>(geo), flags);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-graphics-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}

template <Dimension D>
static Graph::Buffer createGraphicsLightBuffer(const LightType light,
                                               const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const Graph::Buffer buffer = Graph::Buffer_Create(instances * getLightSize<D>(light), Buffer_DeviceStorage);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-graphics-light-buffer-{}D-type-{}", u8(D), ToString(light));
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}

template <Dimension D>
static Graph::Buffer createGraphicsVertexBuffer(const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const Graph::BufferFlags flags =
        Graph::BufferFlag_Vertex | Graph::BufferFlag_DeviceLocal | Graph::BufferFlag_Source;

    const Graph::Buffer buffer = Graph::Buffer_Create(instances * sizeof(DynamicVertex<D>), flags);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-renderer-graphics-vertex-buffer-{}D", u8(D));
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}
template <Dimension D>
static Graph::Buffer createGraphicsIndexBuffer(const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const Graph::BufferFlags flags = Graph::BufferFlag_Index | Graph::BufferFlag_DeviceLocal | Graph::BufferFlag_Source;

    const Graph::Buffer buffer = Graph::Buffer_Create(instances * sizeof(Index), flags);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-renderer-graphics-index-buffer-{}D", u8(D));
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}

template <Dimension D> static void createPipelines()
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    rdata.Geometry.Pipelines.IterateMultiIndex([&](const u32 bpass, const u32 ppass, const u32 geo) {
        rdata.Geometry.Pipelines[bpass][ppass][geo] =
            createGeometryPipeline<D>(PipelinePass(ppass), BlendPass(bpass), Geometry(geo));

        if (Graph::IsValidationEnabled())
        {
            const TKit::StackString name = TKit::StackString::Format(
                "onyx-renderer-geometry-pipeline-{}D-{}-pass-{}-geometry-'{}'", u8(D), ToString(BlendPass(bpass)),
                ToString(PipelinePass(ppass)), ToString(Geometry(geo)));

            Graph::Pipeline_SetName(rdata.Geometry.Pipelines[bpass][ppass][geo], name.CString());
        }
    });

    for (u32 geo = 0; geo < Geometry_Count; ++geo)
    {
        Graph::Format format;
        if constexpr (D == D2)
            format = sdata.OcclusionFormat;
        else
            format = sdata.ShadowFormat;

        sdata.Pipelines[geo] = createShadowPipeline<D>(Geometry(geo), format);
        if (Graph::IsValidationEnabled())
        {
            const TKit::StackString name = TKit::StackString::Format("onyx-renderer-shadow-pipeline-{}D-geometry-'{}'",
                                                                     u8(D), ToString(Geometry(geo)));
            Graph::Pipeline_SetName(sdata.Pipelines[geo], name.CString());
        }
    }

    if constexpr (D == D2)
    {
        sdata.RayMarchPipeline = createRayMarchPipeline();
        if (Graph::IsValidationEnabled())
            Graph::Pipeline_SetName(sdata.RayMarchPipeline, "onyx-renderer-ray-march-pipeline");
    }
}

static void createPipelines()
{
    s_BlendPipeline = createBlendPipeline();
    s_PostProcessPipeline = createPostProcessPipeline();
    s_CompositorPipeline = createCompositorPipeline();

    if (Graph::IsValidationEnabled())
    {
        Graph::Pipeline_SetName(s_BlendPipeline, "onyx-blend-pipeline");
        Graph::Pipeline_SetName(s_PostProcessPipeline, "onyx-post-process-pipeline");
        Graph::Pipeline_SetName(s_CompositorPipeline, "onyx-compositor-pipeline");
    }

    createPipelines<D2>();
    createPipelines<D3>();
}

template <Dimension D> static void destroyPipelines()
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;
    for (const Graph::Pipeline p : rdata.Geometry.Pipelines)
        Graph::Pipeline_Destroy(p);
    for (const Graph::Pipeline p : sdata.Pipelines)
        Graph::Pipeline_Destroy(p);
    if constexpr (D == D2)
        Graph::Pipeline_Destroy(sdata.RayMarchPipeline);
}

static void destroyPipelines()
{
    destroyPipelines<D2>();
    destroyPipelines<D3>();
    Graph::Pipeline_Destroy(s_BlendPipeline);
    Graph::Pipeline_Destroy(s_PostProcessPipeline);
    Graph::Pipeline_Destroy(s_CompositorPipeline);
}

template <Dimension D> static void initializeLights()
{
    LightData<D> &ldata = getRendererData<D>().Lights;
    for (u32 i = 0; i < LightTypeCount<D>; ++i)
    {
        const LightType light = LightType(i);
        TransferLightPool &tpool = ldata.Arenas[light].Transfer;

        tpool.Buffer = createTransferLightBuffer<D>(light);
        tpool.Ranges.Append(TransferLightRange{.Size = Graph::Buffer_GetSize(tpool.Buffer)});

        GraphicsLightPool &gpool = ldata.Arenas[light].Graphics;
        gpool.Buffer = createGraphicsLightBuffer<D>(light);

        gpool.Ranges.Append(GraphicsRange{.Size = Graph::Buffer_GetSize(gpool.Buffer)});
        updateLightDescriptorSets<D>(light);
    }
}

template <Dimension D> struct ShadowParams;

template <Dimension D> static void initializeShadows()
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;

    Renderer_BindSampler<D>(ONYX_SHADOW_SAMPLER_BINDING_POINT, s_LinearSampler, RenderPass_Shaded);
    Renderer_BindSampler<D>(ONYX_SHADOW_COMPARE_SAMPLER_BINDING_POINT, s_CompareSampler, RenderPass_Shaded);

    if constexpr (D == D2)
    {
        sdata.RayMarchSet = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_RayMarch));

        if (Graph::Format_IsSupported(sdata.PrimaryShadowFormat, Graph::ImageType_1D,
                                      Graph::ImageFlag_Sampled | Graph::ImageFlag_Storage))
            sdata.ShadowFormat = ShadowData<D2>::PrimaryShadowFormat;
        else
            sdata.ShadowFormat = ShadowData<D2>::FallbackShadowFormat;
    }
}

template <Dimension D> static void initialize()
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);

        TransferInstancePool &tpool = rdata.Geometry.Arenas[geo].Transfer;
        tpool.Buffer = createTransferInstanceBuffer<D>(geo);
        tpool.Ranges.Append(TransferInstanceRange{.Size = Graph::Buffer_GetSize(tpool.Buffer)});

        GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;
        gpool.Buffer = createGraphicsInstanceBuffer<D>(geo);
        gpool.Ranges.Append(GraphicsInstanceRange{.Size = Graph::Buffer_GetSize(gpool.Buffer)});

        for (u32 j = 0; j < RenderPass_Count; ++j)
        {
            const RenderPass rpass = RenderPass(j);
            const Graph::DescriptorSet set = Graph::DescriptorSet_Create(GetDescriptorLayout<D>(rpass));
            if (Graph::IsValidationEnabled())
            {
                const TKit::StackString name = TKit::StackString::Format(
                    "onyx-renderer-descriptor-set-{}D-geometry-{}-pass-{}", u8(D), ToString(geo), ToString(rpass));
                Graph::DescriptorSet_SetName(set, name.CString());
            }

            const Graph::DescriptorBufferInfo info = Graph::DescriptorBufferInfo::Create(gpool.Buffer);
            Graph::DescriptorSet_Write(set, ONYX_INSTANCES_BINDING_POINT, info);
            rdata.Descriptors[j][i] = set;
        }
    }

    TransferPool &vtpool = rdata.Geometry.VertexArena.Transfer;
    vtpool.Buffer = createTransferVertexBuffer<D>();
    vtpool.Ranges.Append(TransferRange{.Size = Graph::Buffer_GetSize(vtpool.Buffer)});

    TransferPool &itpool = rdata.Geometry.IndexArena.Transfer;
    itpool.Buffer = createTransferIndexBuffer<D>();
    itpool.Ranges.Append(TransferRange{.Size = Graph::Buffer_GetSize(itpool.Buffer)});

    GraphicsPool &vgpool = rdata.Geometry.VertexArena.Graphics;
    vgpool.Buffer = createGraphicsVertexBuffer<D>();
    vgpool.Ranges.Append(GraphicsRange{.Size = Graph::Buffer_GetSize(vgpool.Buffer)});

    GraphicsPool &igpool = rdata.Geometry.IndexArena.Graphics;
    igpool.Buffer = createGraphicsIndexBuffer<D>();
    igpool.Ranges.Append(GraphicsRange{.Size = Graph::Buffer_GetSize(igpool.Buffer)});

    initializeLights<D>();
    return initializeShadows<D>();
}

template <Dimension D> static void terminateShadows()
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;
    for (auto &maps : sdata.ShadowMaps)
        for (auto &map : maps)
            Graph::Image_Destroy(map.Image);
    if constexpr (D == D2)
        for (auto &map : sdata.OcclusionMaps)
            Graph::Image_Destroy(map.Image);
}

template <Dimension D> static void terminate()
{
    RendererData<D> &rdata = getRendererData<D>();

    Graph::DeviceWaitIdle();
    for (InstanceArena &arena : rdata.Geometry.Arenas)
    {
        Graph::Buffer_Destroy(arena.Transfer.Buffer);
        Graph::Buffer_Destroy(arena.Graphics.Buffer);
    }
    for (LightArena &arena : rdata.Lights.Arenas)
    {
        Graph::Buffer_Destroy(arena.Transfer.Buffer);
        Graph::Buffer_Destroy(arena.Graphics.Buffer);
    }

    Graph::Buffer_Destroy(rdata.Geometry.VertexArena.Graphics.Buffer);
    Graph::Buffer_Destroy(rdata.Geometry.VertexArena.Transfer.Buffer);
    Graph::Buffer_Destroy(rdata.Geometry.IndexArena.Graphics.Buffer);
    Graph::Buffer_Destroy(rdata.Geometry.IndexArena.Transfer.Buffer);

    terminateShadows<D>();

    TKit::TierAllocator *tier = TKit::GetTier();
    for (const ContextInfo<D> &info : rdata.Contexts)
        tier->Destroy(info.Context);
}

void Renderer_Initialize()
{
    TKIT_LOG_INFO("[ONYX][RENDERER] Initializing");
    s_DrawBuffers.Construct();
    s_IndexedDrawBuffers.Construct();
    s_RendererData2.Construct();
    s_RendererData3.Construct();
    s_PipelineData.Construct();

#ifndef ONYX_COMPILE_SHADERS_AT_RUNTIME
    InitializeBinaries();
#endif

    createPipelineLayouts();
    createShaders();

    Graph::SamplerSpecs linear{};
    linear.Wraps = Graph::Wrap_ClampToBorder;
    linear.Border = Graph::BorderColor_FloatOpaqueWhite;
    s_LinearSampler = Graph::Sampler_Create(linear);

    Graph::SamplerSpecs nearest{};
    nearest.Filters = Graph::Filter_Nearest;
    s_NearSampler = Graph::Sampler_Create(nearest);

    Graph::SamplerSpecs compare{};
    compare.Compare = Graph::CompareOp_LessOrEqual;
    compare.Wraps = Graph::Wrap_ClampToBorder;
    compare.Border = Graph::BorderColor_FloatOpaqueWhite;
    s_CompareSampler = Graph::Sampler_Create(compare);

    if (Graph::IsValidationEnabled())
    {
        Graph::Sampler_SetName(s_LinearSampler, "onyx-linear-sampler");
        Graph::Sampler_SetName(s_NearSampler, "onyx-near-sampler");
        Graph::Sampler_SetName(s_CompareSampler, "onyx-compare-sampler");
    }

    initialize<D2>();
    initialize<D3>();
    createPipelines();
}
void Renderer_Terminate()
{
    terminate<D2>();
    terminate<D3>();

    destroyPipelines();

    for (DrawBuffer &buffer : *s_DrawBuffers)
        Graph::Buffer_Destroy(buffer.Buffer);
    for (DrawBuffer &buffer : *s_IndexedDrawBuffers)
        Graph::Buffer_Destroy(buffer.Buffer);
    Graph::Sampler_Destroy(s_LinearSampler);
    Graph::Sampler_Destroy(s_NearSampler);
    Graph::Sampler_Destroy(s_CompareSampler);

    destroyShaders();
    for (const auto layout : s_PipelineData->Layouts)
        Graph::PipelineLayout_Destroy(layout);

    for (const auto &st : s_PipelineData->Standalone)
        Graph::PipelineLayout_Destroy(st.Layout);

    s_RendererData2.Destruct();
    s_RendererData3.Destruct();
    s_IndexedDrawBuffers.Destruct();
    s_DrawBuffers.Destruct();
    s_PipelineData.Destruct();
}

template <Dimension D> RenderContext<D> *Renderer_CreateContext(const u32 immediateDynamicMeshCapacity)
{
    RendererData<D> &rdata = getRendererData<D>();
    ContextInfo<D> info;

    TKit::TierAllocator *tier = TKit::GetTier();
    info.Context = tier->Create<RenderContext<D>>(immediateDynamicMeshCapacity);
    info.Generation = info.Context->GetGeneration();

    rdata.Contexts.Append(info);
    return info.Context;
}

template <Dimension D> static u32 getContextIndex(const RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < rdata.Contexts.GetSize(); ++i)
        if (rdata.Contexts[i].Context == context)
            return i;
    TKIT_FATAL("[ONYX][RENDERER] Render context ({}) index not found", scast<const void *>(context));
    return TKIT_U32_MAX;
}

template <Dimension D> void Renderer_DestroyContext(const RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u32 index = getContextIndex(context);
    for (InstanceArena &arena : rdata.Geometry.Arenas)
        for (GraphicsInstanceRange &grange : arena.Graphics.Ranges)
        {
            ViewMask vmask = 0;
            for (ContextInstanceRange &crange : grange.ContextRanges)
            {
                if (crange.ContextIndex != TKIT_U32_MAX && crange.ContextIndex > index)
                    --crange.ContextIndex;
                else if (crange.ContextIndex == index)
                {
                    crange.ViewMask = 0;
                    crange.ContextIndex = TKIT_U32_MAX;
                }
                vmask |= crange.ViewMask;
            }
            grange.ViewMask = vmask;
        }

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(context);
    rdata.Contexts.RemoveOrdered(rdata.Contexts.begin() + index);
}

template <Dimension D> void flushAllContexts()
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const ContextInfo<D> &info : rdata.Contexts)
        info.Context->Flush();
}

void Renderer_FlushAllContexts()
{
    flushAllContexts<D2>();
    flushAllContexts<D3>();
}

void Renderer_ReloadPipelines()
{
    Graph::DeviceWaitIdle();
    destroyPipelines();
    return createPipelines();
}

template <Dimension D> static void addTarget(const ViewMask vmask)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const ContextInfo<D> &info : rdata.Contexts)
        info.Context->AddTarget(vmask);
}
template <Dimension D> static void removeTarget(const ViewMask vmask)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const ContextInfo<D> &info : rdata.Contexts)
        info.Context->RemoveTarget(vmask);
}

void Renderer_AddTarget(const ViewMask vmask)
{
    addTarget<D2>(vmask);
    addTarget<D3>(vmask);
}
void Renderer_RemoveTarget(const ViewMask vmask)
{
    removeTarget<D2>(vmask);
    removeTarget<D3>(vmask);
}

template <Dimension D>
void Renderer_BindBuffer(const u32 binding, const Graph::DescriptorBufferInfo &info, const RenderPass pass,
                         const u32 dstElement)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const Graph::DescriptorSet set : rdata.Descriptors[pass])
        Graph::DescriptorSet_Write(set, binding, info, dstElement);
}
template <Dimension D>
void Renderer_BindImage(const u32 binding, const Graph::DescriptorImageInfo &info, const RenderPass pass,
                        const u32 dstElement)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const Graph::DescriptorSet set : rdata.Descriptors[pass])
        Graph::DescriptorSet_Write(set, binding, info, dstElement);
}
template <Dimension D>
void Renderer_BindSampler(const u32 binding, const Graph::Sampler sampler, const RenderPass pass, const u32 dstElement)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const Graph::DescriptorSet set : rdata.Descriptors[pass])
        Graph::DescriptorSet_Write(set, binding, sampler, dstElement);
}

Graph::Sampler Renderer_GetNearSampler()
{
    return s_NearSampler;
}

#ifdef TKIT_ENABLE_ENSURE
template <typename Range> static void validateRanges(const char *name, const Pool<Range> &pool)
{
    const auto &ranges = pool.Ranges;
    const usz bsize = Graph::Buffer_GetSize(pool.Buffer);
    usz size = 0;
    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        const Range &range = ranges[i];
        TKIT_ENSURE(bsize >= range.Offset + range.Size,
                    "[ONYX][RENDERER] A {} memory range with index {} ({} total) exceeds buffer "
                    "size. Buffer size is {} bytes, which is smaller than offset + size = {} + {} = {}",
                    name, i, ranges.GetSize(), bsize, range.Offset, range.Size, range.Offset + range.Size);
        if (i != 0)
        {
            const Range &prange = ranges[i - 1];
            TKIT_ENSURE(prange.Offset + prange.Size == range.Offset,
                        "[ONYX][RENDERER] A {} memory range pair with indices {} and {} ({} total) are not perfectly "
                        "next to each other, meaning offset{} + size{} != offset{} -> {} + {} = {} != {}",
                        name, i, i - 1, ranges.GetSize(), i - 1, i - 1, i, prange.Offset, prange.Size,
                        prange.Offset + prange.Size, range.Offset);
        }
        size += range.Size;
    }
    TKIT_ENSURE(size == bsize,
                "[ONYX][RENDERER] The sum of the {} memory range sizes ({:L}) does not equal the one of the "
                "buffer ({:L})",
                name, size, bsize);
}

static void validateGraphicsInstanceRanges(const GraphicsInstancePool &gpool)
{
    const auto &granges = gpool.Ranges;
    const usz bsize = Graph::Buffer_GetSize(gpool.Buffer);
    usz gsize = 0;
    for (u32 i = 0; i < granges.GetSize(); ++i)
    {
        const GraphicsInstanceRange &grange = granges[i];
        TKIT_ASSERT(bsize >= grange.Offset + grange.Size,
                    "[ONYX][RENDERER] A graphics instance memory range with index {} ({} total) exceeds buffer size"
                    ". Buffer size is {:L} bytes, which is smaller than offset + size = {:L} + {:L} = {:L}",
                    i, granges.GetSize(), bsize, grange.Offset, grange.Size, grange.Offset + grange.Size);
        if (i != 0)
        {
            const GraphicsInstanceRange &pgrange = granges[i - 1];
            TKIT_ASSERT(pgrange.Offset + pgrange.Size == grange.Offset,
                        "[ONYX][RENDERER] A graphics instance memory range pair with indices {} and {} ({} total) are "
                        "not perfectly "
                        "next to each other, meaning offset{} + size{} != offset{} -> {:L} + {:L} = {:L} != {:L}",
                        i, i - 1, granges.GetSize(), i - 1, i - 1, i, pgrange.Offset, pgrange.Size,
                        pgrange.Offset + pgrange.Size, grange.Offset);
        }
        const auto &cranges = grange.ContextRanges;
        usz csize = 0;
        ViewMask vmask = 0;
        for (u32 j = 0; j < cranges.GetSize(); ++j)
        {
            const ContextInstanceRange &crange = cranges[j];
            TKIT_ASSERT(
                (crange.ViewMask & grange.ViewMask) || crange.ViewMask == grange.ViewMask || crange.ViewMask == 0,
                "[ONYX][RENDERER] A context memory range with index {} ({} total) has one or more view bits not "
                "present in graphics range view mask (context range has {:032b} while graphics range has {:032b})",
                i, cranges.GetSize(), crange.ViewMask, grange.ViewMask);

            vmask |= crange.ViewMask;
            TKIT_ASSERT(
                j != 0 || crange.Offset == 0,
                "[ONYX][RENDERER] First context range offset of a graphics range offset must be zero, but is {:L}",
                crange.Offset);

            TKIT_ASSERT(j != cranges.GetSize() - 1 || crange.Offset + crange.Size == grange.Size,
                        "[ONYX][RENDERER] Last context expression: offset + size = {} + {} = {} must be equal to "
                        "graphics context size of {}",
                        crange.Offset, crange.Size, crange.Offset + crange.Size, grange.Size);

            TKIT_ASSERT(grange.Size >= crange.Offset + crange.Size,
                        "[ONYX][RENDERER] A context memory range with index {} ({} total) exceeds graphics range size. "
                        "Range size is {:L} bytes, which is smaller than offset + size = {:L} + {:L} = {:L}",
                        j, cranges.GetSize(), grange.Size, crange.Offset, crange.Size, crange.Offset + crange.Size);
            TKIT_ASSERT(bsize >= crange.Offset + crange.Size,
                        "[ONYX][RENDERER] A context memory range with index {} ({} total) exceeds buffer size. "
                        "Buffer size is {:L} bytes, which is smaller than offset + size = {:L} + {:L} = {:L}",
                        j, cranges.GetSize(), bsize, crange.Offset, crange.Size, crange.Offset + crange.Size);
            if (j != 0)
            {
                const ContextInstanceRange &pcrange = cranges[j - 1];
                TKIT_ASSERT(pcrange.Offset + pcrange.Size == crange.Offset,
                            "[ONYX][RENDERER] A context memory range pair with indices {} and {} ({} total) are not "
                            "perfectly "
                            "next to each other, meaning offset{} + size{} != offset{} -> {:L} + {:L} = {:L} != {:L}",
                            j, j - 1, cranges.GetSize(), j - 1, j - 1, j, pcrange.Offset, pcrange.Size,
                            pcrange.Offset + pcrange.Size, crange.Offset);
            }
            csize += crange.Size;
        }
        TKIT_ASSERT(vmask == grange.ViewMask,
                    "[ONYX][RENDERER] Combined context range viewmasks ({:032b}) does not match the view mask of "
                    "the graphics range ({:032b})",
                    vmask, grange.ViewMask);

        TKIT_ASSERT(csize <= grange.Size,
                    "[ONYX][RENDERER] The sum of the context memory range sizes ({:L}) exceeds the size of its "
                    "parent range ({:L})",
                    csize, grange.Size);
        gsize += grange.Size;
    }
    TKIT_ASSERT(
        gsize == bsize,
        "[ONYX][RENDERER] The sum of the graphics instance memory range sizes ({:L}) does not equal the one of the "
        "buffer ({:L})",
        gsize, bsize);
}

template <Dimension D> static void validateRanges()
{
    const RendererData<D> &rdata = getRendererData<D>();
    for (const InstanceArena &arena : rdata.Geometry.Arenas)
    {
        validateRanges("transfer instance", arena.Transfer);
        validateGraphicsInstanceRanges(arena.Graphics);
    }

    validateRanges("transfer vertex", rdata.Geometry.VertexArena.Transfer);
    validateRanges("graphics vertex", rdata.Geometry.VertexArena.Graphics);

    validateRanges("transfer index", rdata.Geometry.IndexArena.Transfer);
    validateRanges("graphics index", rdata.Geometry.IndexArena.Graphics);

    const LightData<D> &ldata = rdata.Lights;
    for (const LightArena &arena : ldata.Arenas)
    {
        validateRanges("transfer light", arena.Transfer);
        validateRanges("graphics light", arena.Graphics);
        for (u32 i = 0; i < arena.Graphics.Ranges.GetSize(); ++i)
        {
            const GraphicsRange &grange = arena.Graphics.Ranges[i];
            TKIT_ASSERT(grange.Generation <= arena.ActiveGeneration,
                        "[ONYX][RENDERER] Found graphics light memory range of index {} whose generation {} exceeds "
                        "the one of the arena {}",
                        i, grange.Generation, arena.ActiveGeneration);
        }
    }
}
#endif

template <Dimension D, typename Range>
static Range *handlePoolResize(const usz requiredMem, Graph::Buffer &nbuffer, Graph::Buffer &buffer,
                               TKit::TierArray<Range> &ranges, const bool copyOldContents = false)
{
    for (const Range &range : ranges)
    {
        if constexpr (std::is_same_v<Range, GraphicsInstanceRange> || std::is_same_v<Range, GraphicsRange>)
        {
            Graph::Queue_WaitForTracker(range.TransferTracker);
            Graph::Queue_WaitForTracker(range.GraphicsTracker);
        }
        else
            Graph::Queue_WaitForTracker(range.Tracker);
    }

    const usz size = Graph::Buffer_GetSize(buffer);
    const Graph::BufferCopy copy{.Size = size};

    if (copyOldContents)
    {
        if constexpr (std::is_same_v<Range, GraphicsInstanceRange> || std::is_same_v<Range, GraphicsRange>)
            Graph::CommandPool_ImmediateSubmission(
                GetTransientTransferCommandPool(),
                [&](const Graph::CommandBuffer cmd) { Graph::Command_CopyBuffer(cmd, buffer, nbuffer, copy); });
        else if constexpr (std::is_same_v<Range, TransferInstanceRange> || std::is_same_v<Range, TransferRange>)
            Graph::Buffer_Write(nbuffer, Graph::Buffer_GetData(buffer), copy);
    }

    Graph::Buffer_Destroy(buffer);
    buffer = nbuffer;

    Range smallRange{};
    smallRange.Offset = size;
    smallRange.Size = requiredMem;

    ranges.Append(smallRange);
    const usz nsize = Graph::Buffer_GetSize(nbuffer);
    const usz offset = size + requiredMem;
    if (nsize == offset)
        return &ranges[ranges.GetSize() - 1];

    Range bigRange{};
    bigRange.Offset = offset;
    bigRange.Size = nsize - offset;

    ranges.Append(bigRange);

    return &ranges[ranges.GetSize() - 2];
}

template <typename Range>
static Range *splitRange(const u32 index, TKit::TierArray<Range> &ranges, const usz requiredMem)
{
    Range &range = ranges[index];
    if (range.Size == requiredMem)
        return &range;

    Range child;
    child.Size = requiredMem;
    child.Offset = range.Offset;

    range.Offset += requiredMem;
    range.Size -= requiredMem;
    if constexpr (std::is_same_v<Range, GraphicsInstanceRange>)
    {
        range.ViewMask = 0;
        range.MeshHandle = NullHandle;
        range.RenderFlags = 0;
        range.ContextRanges.Clear();
    }

    ranges.Insert(ranges.begin() + index, child);
    return &ranges[index];
}

static u32 computeNewInstanceCount(const u32 instanceSize, const Graph::Buffer buffer, const usz requiredMem)
{
    const usz size = Graph::Buffer_GetSize(buffer);
    const u32 icount = 2 * u32(Math::Max(requiredMem, size) / instanceSize);

    TKIT_LOG_DEBUG("[ONYX][RENDERER] Failed to find an available range with {:L} bytes of memory. A new buffer "
                   "will be created with more memory (from {:L} to {:L} bytes)",
                   requiredMem, size, icount * instanceSize);
    return icount;
}

template <Dimension D, typename Range, typename Pool, typename F>
static Range *findTransferRange(Pool &pool, const usz requiredMem, const F createBuffer,
                                const bool copyOldContents = false)
{
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    for (u32 i = 0; i < ranges.GetSize(); ++i)
        if (ranges[i].Size >= requiredMem && !ranges[i].Tracker.InUse())
            return splitRange(i, ranges, requiredMem);

    Graph::Buffer nbuffer = createBuffer();
    return handlePoolResize<D>(requiredMem, nbuffer, pool.Buffer, ranges, copyOldContents);
}

template <Dimension D>
static TransferInstanceRange *findTransferInstanceRange(const Geometry geo, TransferInstancePool &pool,
                                                        const usz requiredMem)
{
    return findTransferRange<D, TransferInstanceRange>(
        pool, requiredMem,
        [&] {
            return createTransferInstanceBuffer<D>(
                geo, computeNewInstanceCount(GetInstanceSize<D>(geo), pool.Buffer, requiredMem));
        },
        true);
}

template <Dimension D>
static TransferLightRange *findTransferLightRange(const LightType light, TransferLightPool &pool, const usz requiredMem)
{
    return findTransferRange<D, TransferLightRange>(pool, requiredMem, [&] {
        return createTransferLightBuffer<D>(light,
                                            computeNewInstanceCount(getLightSize<D>(light), pool.Buffer, requiredMem));
    });
}

template <Dimension D> static TransferRange *findTransferVertexRange(TransferLightPool &pool, const usz requiredMem)
{
    return findTransferRange<D, TransferRange>(
        pool, requiredMem,
        [&] {
            return createTransferVertexBuffer<D>(
                computeNewInstanceCount(sizeof(DynamicVertex<D>), pool.Buffer, requiredMem));
        },
        true);
}
template <Dimension D> static TransferRange *findTransferIndexRange(TransferLightPool &pool, const usz requiredMem)
{
    return findTransferRange<D, TransferRange>(
        pool, requiredMem,
        [&] { return createTransferIndexBuffer<D>(computeNewInstanceCount(sizeof(Index), pool.Buffer, requiredMem)); },
        true);
}

template <Dimension D, typename Range, typename Pool, typename F1, typename F2>
static Range *findGraphicsRange(Pool &pool, const usz requiredMem, const F1 createBuffer, const F2 canRangeSplit,
                                bool *resized = nullptr, const bool copyOldContents = false)
{
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        Range &range = ranges[i];
        if (range.Size >= requiredMem && !range.InUse() && canRangeSplit(range))
            return splitRange(i, ranges, requiredMem);
    }
    if (resized)
        *resized = true;

    Graph::Buffer &buffer = pool.Buffer;
    for (u32 i = rdata.AcquireBarriers.GetSize() - 1; i < rdata.AcquireBarriers.GetSize(); --i)
    {
        const Graph::BufferMemoryBarrierInfo &barrier = rdata.AcquireBarriers[i];
        if (barrier.Buffer == buffer)
            rdata.AcquireBarriers.RemoveUnordered(rdata.AcquireBarriers.begin() + i);
    }

    Graph::Buffer nbuffer = createBuffer();
    return handlePoolResize<D>(requiredMem, nbuffer, buffer, ranges, copyOldContents);
}
template <Dimension D, typename Range, typename Pool, typename F>
static Range *findGraphicsRange(Pool &pool, const usz requiredMem, const F createBuffer, bool *resized = nullptr,
                                const bool copyOldContents = false)
{
    return findGraphicsRange<D, Range>(
        pool, requiredMem, createBuffer, [](const auto &) { return true; }, resized, copyOldContents);
}

template <Dimension D>
static GraphicsInstanceRange *findGraphicsInstanceRange(const Geometry geo, GraphicsInstancePool &pool,
                                                        const usz requiredMem)
{
    RendererData<D> &rdata = getRendererData<D>();
    bool resized = false;
    GraphicsInstanceRange *range = findGraphicsRange<D, GraphicsInstanceRange>(
        pool, requiredMem,
        [&] {
            return createGraphicsInstanceBuffer<D>(
                geo, computeNewInstanceCount(GetInstanceSize<D>(geo), pool.Buffer, requiredMem));
        },
        [&](const GraphicsInstanceRange &range) { return rdata.AreAllContextRangesDirty(range); }, &resized, true);
    if (resized)
        updateInstanceDescriptorSets<D>(geo);
    return range;
}

template <Dimension D>
static GraphicsRange *findGraphicsLightRange(const LightType light, GraphicsLightPool &pool, const usz requiredMem)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindGraphicsLightRange");
    bool resized = false;
    GraphicsRange *range = findGraphicsRange<D, GraphicsRange>(
        pool, requiredMem,
        [&] {
            return createGraphicsLightBuffer<D>(
                light, computeNewInstanceCount(getLightSize<D>(light), pool.Buffer, requiredMem));
        },
        &resized);
    if (resized)
        updateLightDescriptorSets<D>(light);
    return range;
}

template <Dimension D> GraphicsRange *findGraphicsVertexRange(GraphicsPool &pool, const usz requiredMem)
{
    return findGraphicsRange<D, GraphicsRange>(
        pool, requiredMem,
        [&] {
            return createGraphicsVertexBuffer<D>(
                computeNewInstanceCount(sizeof(DynamicVertex<D>), pool.Buffer, requiredMem));
        },
        nullptr, true);
}
template <Dimension D> GraphicsRange *findGraphicsIndexRange(GraphicsPool &pool, const usz requiredMem)
{
    return findGraphicsRange<D, GraphicsRange>(
        pool, requiredMem,
        [&] { return createGraphicsIndexBuffer<D>(computeNewInstanceCount(sizeof(Index), pool.Buffer, requiredMem)); },
        nullptr, true);
}

// the transfer and graphics queues may or may not belong to the same family. if they do not, ownership must be released
// by the transfer queue and acquired by the graphics queue
static bool isSeparateTransfer()
{
    return !Graph::BelongToTheSameFamily(Graph::Queue_Transfer, Graph::Queue_Graphics);
}

static Graph::BufferMemoryBarrierInfo createAcquireBarrier(const Graph::Buffer deviceLocalBuffer, const usz offset,
                                                           const usz size)
{
    const bool needsTransfer = isSeparateTransfer();

    Graph::BufferMemoryBarrierInfo barrier{};
    barrier.Offset = offset;
    barrier.Size = size;
    barrier.Buffer = deviceLocalBuffer;
    barrier.AcceptFrom = needsTransfer ? Graph::Queue_Transfer : Graph::Queue_None;
    barrier.SrcStages = needsTransfer ? 0 : Graph::PipelineStageFlag_Transfer;
    barrier.SrcAccess = needsTransfer ? 0 : Graph::AccessFlag_TransferWrite;
    barrier.DstStages = Graph::PipelineStageFlag_VertexShader;
    barrier.DstAccess = Graph::AccessFlag_ShaderRead;

    return barrier;
}
static Graph::BufferMemoryBarrierInfo createReleaseBarrier(const Graph::Buffer deviceLocalBuffer, const usz offset,
                                                           const usz size)
{
    TKIT_ASSERT(isSeparateTransfer(), "[ONYX][RENDERER] Cannot create a release barrier if the graphics and transfer "
                                      "queues belong to the same family");

    Graph::BufferMemoryBarrierInfo barrier{};
    barrier.Offset = offset;
    barrier.Size = size;
    barrier.Buffer = deviceLocalBuffer;
    barrier.HandTo = Graph::Queue_Graphics;
    barrier.SrcStages = Graph::PipelineStageFlag_Transfer;
    barrier.SrcAccess = Graph::AccessFlag_TransferWrite;

    return barrier;
}

static ResourceType getResourceType(const Geometry geo)
{
    switch (geo)
    {
    case Geometry_Static:
        return Resource_StaticMesh;
    case Geometry_Parametric:
        return Resource_ParametricMesh;
    case Geometry_Glyph:
        return Resource_GlyphMesh;
    case Geometry_Dynamic:
        return Resource_DynamicMesh;
    case Geometry_Circle:
        return Resource_MeshPoolCount;
    default:
        TKIT_FATAL("[ONYX][RENDERER] The geometry '{}' does not have a resource type associated", ToString(geo));
        return Resource_Count;
    }
}

static u32 findAvailableOcclusionMap()
{
    ShadowData<D2> &sdata = s_RendererData2->Shadows;
    const u32 size = sdata.OcclusionMaps.GetSize();
    for (u32 i = 0; i < size; ++i)
        if (!sdata.OcclusionMaps[i].Tracker.InUse())
            return i;

    TextureMap &map = sdata.OcclusionMaps.Append();
    map.Extent = u32v2{sdata.OcclusionResolution};

    const Graph::ImageViewSpecs view{};
    Graph::ImageSpecs specs{};
    specs.ImageViews = view;
    specs.Formats = sdata.OcclusionFormat;
    map.Image = Graph::Image_Create(map.Extent, specs, Graph::ImageFlag_ColorAttachment | Graph::ImageFlag_Storage);

    if (Graph::IsValidationEnabled())
    {
        Graph::Image_SetName(map.Image, TKit::StackString::Format("onyx-occlusion-map-{}", size).CString());
        Graph::Image_SetViewNames(map.Image, TKit::StackString::Format("onyx-occlusion-map-view-{}", size).CString());
    }

    const Graph::DescriptorImageInfo info =
        Graph::DescriptorImageInfo::Create(Graph::Image_GetView(map.Image), Graph::ImageLayout_General);
    Graph::DescriptorSet_Write(sdata.RayMarchSet, ONYX_OCCLUSION_MAP_BINDING_POINT, info, size);
    return size;
}

// these two are here bc 2D cpu shadow map textures are NOT shared, but they are in the descriptor array. so sometimes
// we have to hop in and out of desc index or pool index space
template <Dimension D> u32 computeShadowMapDescriptorIndex(const LightType ltype, const u32 idx)
{
    if constexpr (D == D3)
        return idx;
    else
        return idx + ltype * ONYX_MAX_TEXTURE_MAPS;
}
template <Dimension D> u32 computeShadowMapPoolIndex(const LightType ltype, const u32 idx)
{
    if constexpr (D == D3)
        return idx;
    else
        return idx - ltype * ONYX_MAX_TEXTURE_MAPS;
}

template <Dimension D>
static Range findAvailableTextureMapRange(const LightType light, const Graph::Format format, const u32 count)
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;
    const u32 resolution = sdata.ShadowResolutions[light];
    TextureMapArray &maps = sdata.ShadowMaps[light];

    Range range{};
    for (u32 i = 0; i < maps.GetSize(); ++i)
    {
        const TextureMap &map = maps[i];
        ++range.Count;
        if (map.Tracker.InUse())
        {
            range.Offset = i + 1;
            range.Count = 0;
        }
        if (range.Count == count)
            return range;
    }

    Graph::ImageFlags flags = Graph::ImageFlag_Sampled;
    if constexpr (D == D3)
        flags |= Graph::ImageFlag_DepthAttachment;
    else
    {
        flags |= Graph::ImageFlag_Storage;
        if (isDepth2DSupported())
            flags |= Graph::ImageFlag_Depth;
        else
            flags |= Graph::ImageFlag_Color;
    }

    for (u32 i = range.Count; i < count; ++i)
    {
        TextureMap &map = maps.Append();
        map.Extent = u32v2{resolution, D == D3 ? resolution : 1};

        Graph::ImageFlags iflags = flags;
        Graph::ImageSpecs specs{};
        specs.Formats = format;

        // one view per layer, followed by a view of the whole image
        TKit::StackArray<Graph::ImageViewSpecs> views{};
        views.Reserve(64); // more than enough
        u32 viewIndex = 0;
        if constexpr (D == D2)
        {
            specs.Type = Graph::ImageType_1D;
            views.Append();
        }
        else
        {
            constexpr TKit::FixedArray<u32, Light_Count> lcounts = {6, ONYX_MAX_CASCADES, 1};
            const u32 lcount = lcounts[light];

            specs.ArrayLayers = lcount;
            for (u32 j = 0; j < lcount; ++j)
            {
                Graph::ImageViewSpecs &layer = views.Append();
                layer.Range.MipStart = 0;
                layer.Range.MipCount = 1;
                layer.Range.LayerStart = j;
                layer.Range.LayerCount = 1;
                layer.Range.Aspect = Graph::ImageAspectFlag_Depth;
            }
            if (light == Light_Point)
            {
                iflags |= Graph::ImageFlag_CubeCompatible;
                views.Append().Type = Graph::ImageViewType_Cube;
            }
            else if (light == Light_Directional)
                views.Append().Type = Graph::ImageViewType_2DArray;
            else
                views.Append();

            // the whole image view is the last one
            viewIndex = lcount;
        }
        specs.ImageViews = views;

        map.Image = Graph::Image_Create(map.Extent, specs, iflags);
        if (Graph::IsValidationEnabled())
        {
            Graph::Image_SetName(map.Image, TKit::StackString::Format("onyx-texture-map-{}D-'{}'-{}", u8(D),
                                                                      ToString(light), range.Offset + i)
                                                .CString());
            Graph::Image_SetViewNames(map.Image, TKit::StackString::Format("onyx-texture-map-view-{}D-'{}'-{}", u8(D),
                                                                           ToString(light), range.Offset + i)
                                                     .CString());
        }

        Graph::DescriptorImageInfo info = Graph::DescriptorImageInfo::Create(Graph::Image_GetView(map.Image, viewIndex),
                                                                             Graph::ImageLayout_ShaderReadOnly);

        const u32 dstElement = computeShadowMapDescriptorIndex<D>(light, range.Offset + i);
        if constexpr (D == D2)
        {
            Renderer_BindImage<D>(ONYX_SHADOW_MAPS_BINDING_POINT, info, RenderPass_Shaded, dstElement);
            info.Layout = Graph::ImageLayout_General;
            Graph::DescriptorSet_Write(getRendererData<D2>().Shadows.RayMarchSet, ONYX_RAY_MARCH_MAP_BINDING_POINT,
                                       info, dstElement);
        }
        else
        {
            constexpr TKit::FixedArray<u32, Light_Count> bindings = {
                ONYX_POINT_MAPS_BINDING_POINT, ONYX_DIRECTIONAL_MAPS_BINDING_POINT, ONYX_SPOT_MAPS_BINDING_POINT};
            Renderer_BindImage<D>(bindings[light], info, RenderPass_Shaded, dstElement);
        }
    }
    range.Count = count;
    return range;
}

template <Dimension D>
PointLightData<D> createLightData(const ViewMask vmask, const u32 shadowMapOffset,
                                  const PointLightParameters<D> &params)
{
    PointLightData<D> data;
    if constexpr (D == D2)
    {
        data.Direction = f32v2{Math::Cosine(params.Angle), Math::Sine(params.Angle)};
        data.Angle = params.Angle;
        data.Decay = params.Decay;
        data.Extent = params.Extent;
    }
    data.LightSize = params.LightSize;
    data.Position = params.Position;
    data.Intensity = params.Intensity;
    data.LightRadius = params.LightRadius;
    data.ShadowRadius = params.ShadowRadius;
    data.Color = params.Tint.ToLinear().Pack();
    data.ViewMask = vmask;
    data.ShadowMapOffset = shadowMapOffset;
    data.Flags = params.Flags;
    return data;
}

template <typename T> static T cascadeLerp(const T mn, const T mx, const f32 t, const f32 lambda)
{
    const T ln = Math::LinearLerp(mn, mx, t);
    const T lg = Math::LogLerp(mn, mx, t);
    return Math::LinearLerp(ln, lg, lambda);
}

static TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> createCascades(const f32v3 &dir, const u32 count,
                                                                       const f32 lambda, const f32 overlap,
                                                                       const FixedCascadeParameters &params)
{
    TKIT_ASSERT(count <= ONYX_MAX_CASCADES, "[ONYX] The maximum amount of cascades is {}", ONYX_MAX_CASCADES);
    TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> cascades;

    for (u32 i = 0; i < count; ++i)
    {
        CascadeData &c = cascades[i];
        const f32 t = f32(i + 1) / f32(count);

        const f32 width = cascadeLerp(params.MinSize, params.MaxSize, t, lambda);

        const f32m4 proj = Transform<D3>::Orthographic(-width, width, -width, width, params.Near, params.Far);
        const f32m4 view = Transform<D3>::LookTowards(params.ViewPosition, dir);
        c.ProjectionView = PackTransform<D3>(proj * view);

        c.InvSize = 0.5f * f32v2{proj[0][0], proj[1][1]};
        c.DepthRange = params.Far - params.Near;
        c.Split = width * (1.f + overlap);
    }
    return cascades;
}

static TKit::FixedArray8<f32v4> getCameraCorners(const f32m4 &pv)
{
    const f32m4 ipv = Math::Inverse(pv);
    TKit::FixedArray8<f32v4> corners;
    u32 idx = 0;
    for (u32 i = 0; i < 2; ++i)
        for (u32 j = 0; j < 2; ++j)
            for (u32 k = 0; k < 2; ++k)
            {
                const f32v4 norm = f32v4{2.f * i - 1.f, 2.f * j - 1.f, f32(k), 1.f};
                const f32v4 wpos = ipv * norm;
                corners[idx++] = wpos / wpos[3];
            }
    return corners;
}

static TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> createCascades(const f32v3 &dir, const RenderView<D3> *rview,
                                                                       const f32v3 &offset, const f32v2 &extent,
                                                                       const u32 count, const f32 zmul,
                                                                       const f32 lambda, const f32 overlap,
                                                                       u32 &enableFlags)
{
    const TKit::FixedArray8<f32v4> globalCorners = getCameraCorners(rview->GetProjectionView());

    TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> cascades;
    f32 cnear;
    f32 cfar;
    const Camera<D3> *camera = rview->GetCamera();
    const CameraMode mode = camera->Mode;
    if (mode == CameraMode_Perspective)
    {
        cnear = camera->PerspParameters.Near;
        cfar = camera->PerspParameters.Far;
    }
    else
    {
        cnear = camera->OrthoParameters.Near;
        cfar = camera->OrthoParameters.Far;
    }
    const f32 range = cfar - cnear;
    f32 split0 = cnear;
    enableFlags = 0;
    for (u32 i = 0; i < count; ++i)
    {
        CascadeData &c = cascades[i];

        const f32 t = f32(i + 1) / count;
        const f32 split1 = cascadeLerp(cnear, cfar, t, lambda);

        const f32 t0 = (split0 - cnear) / range;
        const f32 t1 = (split1 - cnear) / range;

        TKit::FixedArray8<f32v4> corners;
        for (u32 j = 0; j < 4; ++j)
        {
            corners[2 * j] = Math::LinearLerp(globalCorners[2 * j], globalCorners[2 * j + 1], t0);
            corners[2 * j + 1] = Math::LinearLerp(globalCorners[2 * j], globalCorners[2 * j + 1], t1);
        }

        f32v3 center{0.f};
        for (u32 j = 0; j < 8; ++j)
            center += f32v3{corners[j]};
        center /= 8.f;

        const f32m4 view = Transform<D3>::LookTowards(center, dir);
        f32v3 min{TKIT_F32_MAX};
        f32v3 max{TKIT_F32_MIN};
        for (const f32v4 &cr : corners)
        {
            const f32v3 lc = f32v3{view * cr};
            min = Math::Min(min, lc);
            max = Math::Max(max, lc);
        }

        const f32v2 ofview = f32v2{view * f32v4{offset, 1.f}};
        const f32v2 cmin = ofview - extent;
        const f32v2 cmax = ofview + extent;

        min[0] = Math::Max(min[0], cmin[0]);
        min[1] = Math::Max(min[1], cmin[1]);

        max[0] = Math::Min(max[0], cmax[0]);
        max[1] = Math::Min(max[1], cmax[1]);

        if (min[0] >= max[0] || min[1] >= max[1])
            continue;

        enableFlags |= 1U << i;

        if (min[2] < 0.f)
            min[2] *= zmul;
        else
            min[2] /= zmul;

        if (max[2] < 0.f)
            max[2] /= zmul;
        else
            max[2] *= zmul;

        const f32m4 proj = Transform<D3>::Orthographic(min, max);
        c.ProjectionView = PackTransform<D3>(proj * view);

        c.InvSize = 0.5f * f32v2{proj[0][0], proj[1][1]};
        c.DepthRange = max[2] - min[2];
        c.Split = split1 * (1.f + overlap);

        split0 = split1;
    }
    return cascades;
}

template <Dimension D>
DirectionalLightData<D> createLightData(const ViewMask vmask, const u32 shadowMapOffset,
                                        const DirectionalLightParameters<D> &params)
{
    DirectionalLightData<D> data;

    if constexpr (D == D3)
    {
        const f32v3 dir = Math::Normalize(params.Direction);
        const f32m2x3 orth = Transform<D3>::CreateOrthogonalBase(dir);

        data.Offset = params.Offset[0] * orth[0] + params.Offset[1] * orth[1];
        data.Extent = params.Extent;
        data.PlaneVec1 = orth[0];
        data.PlaneVec2 = orth[1];

        if (params.Flags & LightFlag_CastShadows)
        {
            const ShadowCascadeParameters &c = params.Cascades;
            TKIT_ASSERT(
                c.Count != 0,
                "[ONYX][RENDERER] If a directional light casts shadows, its cascade count must be greater than 0");

            data.CascadeEnable = (1U << ONYX_MAX_CASCADES) - 1;
            const TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> cascades =
                c.View ? createCascades(dir, c.View, data.Offset, params.Extent, c.Count, c.FittedParameters.ZMul,
                                        c.Lambda, c.Overlap, data.CascadeEnable)
                       : createCascades(dir, c.Count, c.Lambda, c.Overlap, c.FixedParameters);

            data.Cascades = cascades;
            data.CascadeCount = c.Count;
        }
        data.Direction = dir;
    }
    else
    {
        const f32v2 dir = f32v2{Math::Cosine(params.Angle), Math::Sine(params.Angle)};
        const f32v2 normal = f32v2{-dir[1], dir[0]};
        const f32v2 pos = params.ShadowOffset * dir + params.LightOffset * normal;

        const f32 le = params.LightExtent;
        const f32 se = params.ShadowExtent;

        const f32m3 orth = Transform<D2>::Orthographic(-se, se, -le, le);
        const f32m3 view = Transform<D2>::ComputeInverseTransform(pos, f32v2{1.f}, params.Angle);

        data.ProjectionView = PackTransform<D2>(orth * view);
        data.Direction = dir;
        data.LightOffset = params.LightOffset;
        data.LightExtent = le;
        data.ShadowExtent = se;
    }

    data.Decay = params.Decay;
    data.TanAngleSize = Math::Tangent(params.AngleSize);
    data.Intensity = params.Intensity;
    data.Color = params.Tint.ToLinear().Pack();
    data.ViewMask = vmask;
    data.ShadowMapOffset = shadowMapOffset;
    data.Flags = params.Flags;
    return data;
}

SpotLightData createLightData(const ViewMask vmask, const u32 shadowMapOffset, const SpotLightParameters &params)
{
    SpotLightData data;
    data.Direction = Math::Normalize(params.Direction);

    const f32 near = ONYX_SPOT_LIGHT_NEAR_RANGE_FACTOR * params.ShadowRange;
    const f32 far = params.ShadowRange;

    data.Near = near;
    data.Far = far;

    const f32m4 proj = Transform<D3>::Perspective(params.FieldOfView, near, far);
    const f32m4 view = Transform<D3>::LookTowards(params.Position, data.Direction);

    data.ProjectionView = proj * view;
    data.ShadowInvSize = 0.5f * Math::Absolute(f32v2{proj[0][0], proj[1][1]});

    data.Position = params.Position;
    data.LightSize = params.LightSize;
    data.CosHalfPov = Math::Cosine(0.5f * params.FieldOfView);
    data.LightRange = params.LightRange;
    data.Decay = params.Decay;
    data.Intensity = params.Intensity;
    data.Color = params.Tint.ToLinear().Pack();
    data.ShadowMapOffset = shadowMapOffset;
    data.ViewMask = vmask;
    data.Flags = params.Flags;
    return data;
}

template <Dimension D>
static void transfer(const Graph::CommandBuffer command, TransferSubmitInfo &info,
                     TKit::StackArray<Graph::BufferMemoryBarrierInfo> *release, const Graph::Tracker &tracker,
                     const u32 maxLights)
{

    RendererData<D> &rdata = getRendererData<D>();
    TKit::TierArray<ContextInfo<D>> &contexts = rdata.Contexts;
    if (contexts.IsEmpty())
        return;

    TKit::StackArray<u32> dirtyContexts{};
    dirtyContexts.Reserve(rdata.Contexts.GetSize());

    TKit::FixedArray<TKit::StackArray<ViewMask>, LightTypeCount<D>> lightViews{};
    for (u32 i = 0; i < LightTypeCount<D>; ++i)
        lightViews[i].Reserve(maxLights);

    using LightUpdateFlags = u8;
    enum LightUpdateFlagBit : LightUpdateFlags
    {
        LightUpdateFlag_Point = 1U << 0,
        LightUpdateFlag_Directional = 1U << 1,
        LightUpdateFlag_Spot = 1U << 2,
    };

    LightUpdateFlags toUpdate = 0;

    LightData<D> &ldata = rdata.Lights;
    ldata.Instances.ClearParameters();

    ShadowData<D> &sdata = rdata.Shadows;

    ten<LocalResourceRegistry, BlendPass_Count, RenderMode_Count, Resource_MeshPoolCount, ONYX_MAX_RESOURCE_POOLS>
        meshRegistry{};
    ten<LocalResourceRegistry, BlendPass_Count, RenderMode_Count> dynMeshRegistry{};

    for (u32 i = 0; i < contexts.GetSize(); ++i)
    {
        ContextInfo<D> &cinfo = contexts[i];
        RenderContext<D> *ctx = cinfo.Context;

        const ViewMask vmask = ctx->ViewMask;
        if (!vmask)
            continue;

        const bool isDirty = cinfo.IsDirty();
        if (isDirty)
        {
            dirtyContexts.Append(i);

            const ContextInstanceData &idata = ctx->GetInstanceData();
            ForEachResourceGroup<D>([&](const u32 bpass, const u32 rmode, const u32 mtype, const u32 pid) {
                const InstanceResourceGroup &group = idata.Meshes[bpass][rmode][mtype][pid];
                for (const u32 rid : group.Registry.ResourceIds)
                    meshRegistry[bpass][rmode][mtype][pid].RegisterResourceId(rid);
            });

            idata.DynamicMeshes.IterateMultiIndex([&](const u32 bpass, const u32 rmode) {
                const InstanceResourceGroup &group = idata.DynamicMeshes[bpass][rmode];
                for (const u32 rid : group.Registry.ResourceIds)
                    dynMeshRegistry[bpass][rmode].RegisterResourceId(rid);
            });
            cinfo.Generation = ctx->GetGeneration();
        }
        const auto gatherLights =
            [&]<typename LightParams>(const LightType ltype, const TKit::TierArray<LightParams> &src,
                                      TKit::TierArray<LightParams> &dst, const LightUpdateFlags update) {
                TKit::StackArray<ViewMask> &vmasks = lightViews[ltype];
                LightFlags flags = 0;

                for (u32 i = 0; i < src.GetSize(); ++i)
                {
                    const LightParams &params = src[i];
                    flags |= params.Flags;

                    dst.Append(params);
                    vmasks.Append(vmask);
                }
                toUpdate |= (update * isDirty * !src.IsEmpty()) |
                            ((flags & LightFlag_CastShadows) *
                             (LightUpdateFlag_Point | LightUpdateFlag_Directional | LightUpdateFlag_Spot));

                sdata.DirtyShadowViews |= vmask * (flags & LightFlag_CastShadows);
            };

        gatherLights(Light_Point, ctx->GetPointLightData(), ldata.Instances.Points.Parameters, LightUpdateFlag_Point);
        gatherLights(Light_Directional, ctx->GetDirectionalLightData(), ldata.Instances.Directionals.Parameters,
                     LightUpdateFlag_Directional);
        if constexpr (D == D3)
            gatherLights(Light_Spot, ctx->GetSpotLightData(), ldata.Instances.Spots.Parameters, LightUpdateFlag_Spot);
    }

    const auto checkLightCountChange =
        [&]<typename LightParams>(const LightType ltype, const LightUpdateFlags flags,
                                  ContextLights<LightParams> &clights) -> LightUpdateFlags {
        const u32 ncount = clights.Parameters.GetSize();
        u32 &count = ldata.Arenas[ltype].LightCount;
        if (ncount != count)
        {
            count = ncount;
            clights.Data.Clear();
            return flags;
        }
        return 0;
    };
    toUpdate |= checkLightCountChange(Light_Point, LightUpdateFlag_Point, ldata.Instances.Points);
    toUpdate |= checkLightCountChange(Light_Directional, LightUpdateFlag_Directional, ldata.Instances.Directionals);
    if constexpr (D == D3)
        toUpdate |= checkLightCountChange(Light_Spot, LightUpdateFlag_Spot, ldata.Instances.Spots);

    const auto copyLightRanges = [&]<typename LightParams>(const LightType ltype, ContextLights<LightParams> &clights) {
        info.Command = command;
        clights.Data.Clear();

        LightArena &arena = ldata.Arenas[ltype];
        TransferLightPool &tpool = arena.Transfer;
        GraphicsLightPool &gpool = arena.Graphics;

        using LightData = typename LightParams::InstanceData;
        const usz requiredMem = sizeof(LightData) * clights.Parameters.GetSize();

        const TKit::StackArray<ViewMask> &vmasks = lightViews[ltype];
        for (u32 i = 0; i < clights.Parameters.GetSize(); ++i)
        {
            const LightParams &light = clights.Parameters[i];

            const ViewMask vm = vmasks[i];
            const u32 count = std::popcount(vm);
            u32 shadowOffset = TKIT_U32_MAX;

            if (light.Flags & LightFlag_CastShadows)
            {
                const Range range = findAvailableTextureMapRange<D>(ltype, sdata.ShadowFormat, count);
                // this is a small hack: shadow maps wont ever be used by the transfer queue, but this way we prevent
                // lights from taking the same shadow map this run
                for (u32 j = 0; j < range.Count; ++j)
                    sdata.ShadowMaps[ltype][j + range.Offset].Tracker = tracker;

                shadowOffset = computeShadowMapDescriptorIndex<D>(ltype, range.Offset);
            }
            clights.Data.Append(createLightData(vm, shadowOffset, light));
        }

        TransferLightRange *trange = findTransferLightRange<D>(ltype, tpool, requiredMem);
        trange->Tracker = tracker;
        Graph::Buffer_Write(tpool.Buffer, clights.Data.GetData(),
                            {.Size = trange->Size, .SrcOffset = 0, .DstOffset = trange->Offset});

        GraphicsRange *grange = findGraphicsLightRange<D>(ltype, gpool, requiredMem);
        grange->TransferTracker = tracker;
        grange->GraphicsTracker = {};
        grange->Generation = ++arena.ActiveGeneration;

        Graph::BufferCopy copy{};
        copy.Size = requiredMem;
        copy.SrcOffset = trange->Offset;
        copy.DstOffset = grange->Offset;

        Graph::Command_CopyBuffer(command, tpool.Buffer, gpool.Buffer, copy);
        rdata.AcquireBarriers.Append(createAcquireBarrier(gpool.Buffer, grange->Offset, requiredMem));
        if (release)
            release->Append(createReleaseBarrier(gpool.Buffer, grange->Offset, requiredMem));
    };

    if ((toUpdate & LightUpdateFlag_Point) && !ldata.Instances.Points.Parameters.IsEmpty())
        copyLightRanges(Light_Point, ldata.Instances.Points);
    if ((toUpdate & LightUpdateFlag_Directional) && !ldata.Instances.Directionals.Parameters.IsEmpty())
        copyLightRanges(Light_Directional, ldata.Instances.Directionals);

    if constexpr (D == D3)
        if ((toUpdate & LightUpdateFlag_Spot) && !ldata.Instances.Spots.Parameters.IsEmpty())
            copyLightRanges(Light_Spot, ldata.Instances.Spots);

    if (dirtyContexts.IsEmpty())
        return;

    // this struct is used to create the appropiate memory barriers in the copies ranges. takes a buffer so this works
    // for both instance and v/i buffers
    struct RangePair
    {
        Graph::Buffer *GraphicsBuffer;
        usz GraphicsOffset;
        usz RequiredMemory;
    };

    // the actual copy commands for each data type (instance, vertex and index). can belong to any buffer (in the case
    // of vertex/index there is only one buffer for each so its easy enough)
    struct BufferCopies
    {
        BufferCopies(const u32 incount, const u32 dynCount)
        {
            Instance.Reserve(incount);
            Vertex.Reserve(dynCount);
            Index.Reserve(dynCount);
        }
        TKit::StackArray<Graph::BufferCopy> Instance{};
        TKit::StackArray<Graph::BufferCopy> Vertex{};
        TKit::StackArray<Graph::BufferCopy> Index{};
    };

    // a "copy struct slice" to identify what chunks of the global copy array belongs to which instance buffer, as there
    // are many (one pair transfer-graphics per geometry type). this is only needed for instances. for v/i, there is
    // only one buffer so all copies go to those
    struct BufferCopySlice
    {
        Graph::Buffer *Transfer;
        Graph::Buffer *Graphics;
        u32 Offset;
        u32 Size;
    };

    const u32 bcount = Resources_GetDistinctBatchDrawCount<D>();
    const u32 dynCount = DynamicMesh_GetCount<D>();
    const u32 upperCapacity =
        dirtyContexts.GetSize() * bcount * u32(RenderMode_Count) * u32(BlendPass_Count) * u32(Geometry_Count);

    BufferCopies copies{upperCapacity, dynCount};

    TKit::StackArray<RangePair> ranges{};
    ranges.Reserve(upperCapacity);

    TKit::StackArray<BufferCopySlice> instanceCopyCmds{};
    instanceCopyCmds.Reserve(upperCapacity);

    TKit::StackArray<ContextInstanceRange> contextRanges{};
    contextRanges.Reserve(dirtyContexts.GetSize());

    const auto findInstanceRanges = [&](const u32 rmode, const u32 bpass, const u32 geo, const Resource handle,
                                        const auto getInstanceData) {
        u64 vgen = 0;
        u64 igen = 0;
        if (geo == Geometry_Dynamic)
        {
            const DynamicMeshData<D> &data = DynamicMesh_GetData<D>(handle);
            if (data.Vertices.IsEmpty() || data.Indices.IsEmpty())
                return;

            Arena &varena = rdata.Geometry.VertexArena;
            Arena &iarena = rdata.Geometry.IndexArena;

            vgen = ++varena.LatestGeneration;
            igen = ++iarena.LatestGeneration;

            const auto processTransferArena = [&](TransferPool &pool, const auto &data, const bool vertices) {
                TransferRange *trng = vertices ? findTransferVertexRange<D>(pool, data.GetBytes())
                                               : findTransferIndexRange<D>(pool, data.GetBytes());
                trng->Tracker = tracker;
                Graph::Buffer_Write(pool.Buffer, data.GetData(),
                                    {.Size = trng->Size, .SrcOffset = 0, .DstOffset = trng->Offset});
                return trng;
            };

            const TransferRange *vtrng = processTransferArena(varena.Transfer, data.Vertices, true);
            const TransferRange *itrng = processTransferArena(iarena.Transfer, data.Indices, false);

            const auto processGraphicsArena = [&](GraphicsPool &pool, const usz reqMem, const TransferRange *trng,
                                                  const bool vertices) {
                GraphicsRange *grng =
                    vertices ? findGraphicsVertexRange<D>(pool, reqMem) : findGraphicsIndexRange<D>(pool, reqMem);
                grng->TransferTracker = tracker;

                Graph::BufferCopy &copy = vertices ? copies.Vertex.Append() : copies.Index.Append();
                copy.SrcOffset = trng->Offset;
                copy.DstOffset = grng->Offset;
                copy.Size = reqMem;
                ranges.Append(&pool.Buffer, grng->Offset, reqMem);
                return grng;
            };

            GraphicsRange *vgrng = processGraphicsArena(varena.Graphics, data.Vertices.GetBytes(), vtrng, true);
            GraphicsRange *igrng = processGraphicsArena(iarena.Graphics, data.Indices.GetBytes(), itrng, false);

            vgrng->Generation = vgen;
            igrng->Generation = igen;
        }

        TransferInstancePool &tpool = rdata.Geometry.Arenas[geo].Transfer;
        GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;

        contextRanges.Clear();
        usz requiredMem = 0;
        ViewMask viewMask = 0;
        for (const u32 idx : dirtyContexts)
        {
            const RenderContext<D> *ctx = rdata.Contexts[idx].Context;
            const auto &idata = getInstanceData(ctx);
            if (idata.Instances == 0)
                continue;

            ContextInstanceRange &crange = contextRanges.Append();
            crange.ContextIndex = idx;
            crange.Offset = requiredMem;
            crange.Size = idata.Instances * idata.InstanceSize;
            crange.Generation = ctx->GetGeneration();

            const ViewMask vm = ctx->ViewMask;
            viewMask |= vm;
            crange.ViewMask = vm;

            requiredMem += crange.Size;
        }
        if (requiredMem == 0)
            return;

        TransferInstanceRange *trange = findTransferInstanceRange<D>(Geometry(geo), tpool, requiredMem);
        trange->Tracker = tracker;

        for (const ContextInstanceRange &crange : contextRanges)
        {
            const RenderContext<D> *ctx = contexts[crange.ContextIndex].Context;

            const auto &idata = getInstanceData(ctx);
            Graph::Buffer_Write(tpool.Buffer, idata.Data.GetData(),
                                {.Size = crange.Size, .SrcOffset = 0, .DstOffset = trange->Offset + crange.Offset});
        }

        GraphicsInstanceRange *grange = findGraphicsInstanceRange<D>(Geometry(geo), gpool, requiredMem);
        grange->Blend = BlendPass(bpass);
        grange->MeshHandle = handle;
        grange->ContextRanges = contextRanges;
        grange->ViewMask = viewMask;
        grange->RenderFlags = GetRenderModeFlags(RenderMode(rmode));
        grange->TransferTracker = tracker;
        grange->GraphicsTracker = {};
        if (geo == Geometry_Dynamic)
        {
            grange->ActiveVertexGeneration = vgen;
            grange->ActiveIndexGeneration = igen;
        }

        Graph::BufferCopy &copy = copies.Instance.Append();
        copy.SrcOffset = trange->Offset;
        copy.DstOffset = grange->Offset;
        copy.Size = requiredMem;

        ranges.Append(&gpool.Buffer, grange->Offset, requiredMem);
    };

    const auto gatherInstanceRanges = [&](const u32 rmode, const u32 geo) {
        TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindRanges");
        TransferInstancePool &tpool = rdata.Geometry.Arenas[geo].Transfer;
        GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;

        BufferCopySlice copyCmdSlice{};
        copyCmdSlice.Transfer = &tpool.Buffer;
        copyCmdSlice.Graphics = &gpool.Buffer;
        copyCmdSlice.Offset = copies.Instance.GetSize();

        if (geo == Geometry_Circle)
            for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
                findInstanceRanges(rmode, bpass, Geometry_Circle, NullHandle,
                                   [rmode, bpass](const RenderContext<D> *ctx) -> const auto & {
                                       return ctx->GetInstanceData().Circles[BlendPass(bpass)][rmode];
                                   });
        else if (geo == Geometry_Dynamic)
            for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
            {
                const TKit::TierArray<Resource> &resources = dynMeshRegistry[bpass][rmode].ResourceIds;
                for (const u32 rid : resources)
                    findInstanceRanges(rmode, bpass, geo, Handle_CreateForResource(Resource_DynamicMesh, rid),
                                       [rmode, bpass, rid](const RenderContext<D> *ctx) -> const auto & {
                                           return ctx->GetInstanceData().DynamicMeshes[bpass][rmode].Buffers[rid];
                                       });
            }
        else
        {
            const ResourceType rtype = getResourceType(Geometry(geo));
            const TKit::Span<const u32> poolIds = ResourcePool_GetIds<D>(rtype);
            for (const u32 pid : poolIds)
            {
                for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
                {
                    const TKit::TierArray<Resource> &resources = meshRegistry[bpass][rmode][rtype][pid].ResourceIds;
                    for (const u32 rid : resources)
                        findInstanceRanges(
                            rmode, bpass, geo, Handle_CreateForResource(rtype, rid, pid),
                            [rtype, rmode, bpass, pid, rid](const RenderContext<D> *ctx) -> const auto & {
                                return ctx->GetInstanceData().Meshes[bpass][rmode][rtype][pid].Buffers[rid];
                            });
                }
            }
        }

        copyCmdSlice.Size = copies.Instance.GetSize() - copyCmdSlice.Offset;
        if (copyCmdSlice.Size != 0)
            instanceCopyCmds.Append(copyCmdSlice);
    };

    for (u32 rmode = 0; rmode < RenderMode_Count; ++rmode)
        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            gatherInstanceRanges(rmode, geo);

    for (const RangePair &range : ranges)
    {
        rdata.AcquireBarriers.Append(
            createAcquireBarrier(*range.GraphicsBuffer, range.GraphicsOffset, range.RequiredMemory));

        if (release)
            release->Append(createReleaseBarrier(*range.GraphicsBuffer, range.GraphicsOffset, range.RequiredMemory));
    }
    for (const BufferCopySlice &cmdSlice : instanceCopyCmds)
    {
        const TKit::Span<const Graph::BufferCopy> cpspan{copies.Instance.GetData() + cmdSlice.Offset, cmdSlice.Size};
        Graph::Command_CopyBuffer(command, *cmdSlice.Transfer, *cmdSlice.Graphics, cpspan);
    }
    if (!copies.Vertex.IsEmpty())
        Graph::Command_CopyBuffer(command, rdata.Geometry.VertexArena.Transfer.Buffer,
                                  rdata.Geometry.VertexArena.Graphics.Buffer, copies.Vertex);

    if (!copies.Index.IsEmpty())
        Graph::Command_CopyBuffer(command, rdata.Geometry.IndexArena.Transfer.Buffer,
                                  rdata.Geometry.IndexArena.Graphics.Buffer, copies.Index);

    info.Command = command;
}

TransferSubmitInfo Renderer_Transfer(const Graph::CommandBuffer command, const u32 maxLights,
                                     const u32 maxReleaseBarriers)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Transfer");
    TransferSubmitInfo submitInfo{};
    const bool separate = isSeparateTransfer();
    TKit::StackArray<Graph::BufferMemoryBarrierInfo> release{};
    if (separate)
        release.Reserve(maxReleaseBarriers);

    const u64 transferFlight = Graph::Queue_ReserveTimelineValue(Graph::Queue_Transfer);

    Graph::Tracker tracker{};
    tracker.Queue = Graph::Queue_Transfer;
    tracker.InFlightValue = transferFlight;

    transfer<D2>(command, submitInfo, separate ? &release : nullptr, tracker, maxLights);
    transfer<D3>(command, submitInfo, separate ? &release : nullptr, tracker, maxLights);

#ifdef TKIT_ENABLE_ENSURE
    validateRanges<D2>();
    validateRanges<D3>();
#endif

    if (separate)
        Graph::Command_BufferBarrier(command, release);

    if (submitInfo)
        submitInfo.InFlightValue = transferFlight;

    s_SyncPointCount = 0;
    for (const InstanceArena &arena : getRendererData<D2>().Geometry.Arenas)
        s_SyncPointCount += arena.Graphics.Ranges.GetSize();
    for (const InstanceArena &arena : getRendererData<D3>().Geometry.Arenas)
        s_SyncPointCount += arena.Graphics.Ranges.GetSize();
    return submitInfo;
}

void Renderer_SubmitTransfer(const Graph::CommandPool pool, const TKit::Span<const TransferSubmitInfo> info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::SubmitTransfer");
    TKit::StackArray<Graph::SubmitInfo> submits{};
    submits.Reserve(info.GetSize());

    u64 maxFlight = 0;
    for (const TransferSubmitInfo &inf : info)
    {
        TKIT_ASSERT(inf, "[ONYX][RENDERER] A submission must have a valid transfer command buffer to be submitted");
        if (inf.InFlightValue > maxFlight)
            maxFlight = inf.InFlightValue;

        Graph::SubmitInfo &sinfo = submits.Append();
        sinfo.Commands = inf.Command;
        sinfo.SignalValue = inf.InFlightValue;
    }

    MarkCommandPoolInUse(pool, maxFlight);
    Graph::Queue_Submit(Graph::Queue_Transfer, submits);
}

template <Dimension D> void gatherAcquireBarriers(TKit::StackArray<Graph::BufferMemoryBarrierInfo> &barriers)
{
    RendererData<D> &rdata = getRendererData<D>();
    barriers.Insert(barriers.end(), rdata.AcquireBarriers.begin(), rdata.AcquireBarriers.end());
    rdata.AcquireBarriers.Clear();
}

template <Dimension D> void prepareRender()
{
    RendererData<D> &rdata = getRendererData<D>();
    LightData<D> &ldata = rdata.Lights;
    for (u32 i = 0; i < ldata.Arenas.GetSize(); ++i)
    {
        ldata.Ranges[i] = Range{};

        LightArena &arena = ldata.Arenas[i];
        arena.ActiveRange = nullptr;
        if (arena.LightCount != 0)
            for (GraphicsRange &grange : arena.Graphics.Ranges)
                if (grange.Generation == arena.ActiveGeneration)
                {
                    arena.ActiveRange = &grange;
                    const u32 isize = getLightSize<D>(LightType(i));
                    TKIT_ASSERT(arena.LightCount == grange.Size / isize,
                                "[ONYX][RENDERER] Light count mismatch between cached arena value and graphics range");
                    ldata.Ranges[i].Count = arena.LightCount;
                    ldata.Ranges[i].Offset = grange.Offset / isize;
                    break;
                }
    }

    for (GraphicsInstanceRange &grange : rdata.Geometry.Arenas[Geometry_Dynamic].Graphics.Ranges)
    {
        grange.ActiveVertexRange = nullptr;
        grange.ActiveIndexRange = nullptr;
        for (GraphicsRange &vgrange : rdata.Geometry.VertexArena.Graphics.Ranges)
            if (grange.ActiveVertexGeneration == vgrange.Generation)
            {
                grange.ActiveVertexRange = &vgrange;
                break;
            }
        for (GraphicsRange &igrange : rdata.Geometry.IndexArena.Graphics.Ranges)
            if (grange.ActiveIndexGeneration == igrange.Generation)
            {
                grange.ActiveIndexRange = &igrange;
                break;
            }
        // TKIT_ASSERT(grange.ActiveVertexRange,
        //             "[ONYX][RENDERING] Graphics instance range failed to find a vertex graphics range");
        // TKIT_ASSERT(grange.ActiveIndexRange,
        //             "[ONYX][RENDERING] Graphics instance range failed to find an index graphics range");
    }
}

void Renderer_PrepareRender()
{
    prepareRender<D2>();
    prepareRender<D3>();
}
void Renderer_ApplyAcquireBarriers(const Graph::CommandBuffer cmd)
{
    TKit::StackArray<Graph::BufferMemoryBarrierInfo> barriers{};
    barriers.Reserve(s_RendererData2->AcquireBarriers.GetSize() + s_RendererData3->AcquireBarriers.GetSize());
    gatherAcquireBarriers<D2>(barriers);
    gatherAcquireBarriers<D3>(barriers);
    if (!barriers.IsEmpty())
        Graph::Command_BufferBarrier(cmd, barriers);
}

static constexpr Graph::BufferFlags s_DrawBufferFlags =
    Graph::BufferFlag_HostMapped | Graph::BufferFlag_HostRandomAccess | Graph::BufferFlag_Indirect;

template <typename T>
ONYX_NO_DISCARD static Graph::Buffer *findAvailableDrawBuffer(TKit::TierArray<DrawBuffer> &buffers, const u32 drawCount,
                                                              const Graph::Tracker &tracker)
{
    for (DrawBuffer &db : buffers)
        if (!db.Tracker.InUse())
        {
            GrowBufferIfNeeded<T>(db.Buffer, drawCount, s_DrawBufferFlags);
            db.Tracker = tracker;
            return &db.Buffer;
        }

    DrawBuffer &db = buffers.Append();

    db.Buffer = Graph::Buffer_Create<T>(drawCount, s_DrawBufferFlags);

    if (Graph::IsValidationEnabled())
        Graph::Buffer_SetName(db.Buffer, "onyx-draw-buffer");

    db.Tracker = tracker;
    return &db.Buffer;
}

static Graph::Buffer *findAvailableDrawBuffer(const u32 drawCount, const Graph::Tracker &tracker)
{
    return findAvailableDrawBuffer<Graph::DrawIndirectCommand>(*s_DrawBuffers, drawCount, tracker);
}
static Graph::Buffer *findAvailableIndexedDrawBuffer(const u32 drawCount, const Graph::Tracker &tracker)
{
    return findAvailableDrawBuffer<Graph::DrawIndexedIndirectCommand>(*s_IndexedDrawBuffers, drawCount, tracker);
}

static constexpr Graph::IndexType s_IndexType = sizeof(Index) == 1   ? Graph::IndexType_Unsigned8
                                                : sizeof(Index) == 2 ? Graph::IndexType_Unsigned16
                                                                     : Graph::IndexType_Unsigned32;

template <Dimension D> static void bindMeshBuffers(const ResourcePool pool, const Graph::CommandBuffer command)
{
    const MeshBuffers buffers = ResourcePool_GetMeshBuffers<D>(pool);

    Graph::Command_BindVertexBuffers(command, buffers.VertexBuffer);
    Graph::Command_BindIndexBuffer(command, buffers.IndexBuffer, s_IndexType);
}

static Graph::DrawIndirectCommand createCircleCommand(const u32 firstInstance, const u32 instanceCount)
{
    Graph::DrawIndirectCommand cmd;
    cmd.FirstInstance = firstInstance;
    cmd.InstanceCount = instanceCount;
    cmd.FirstVertex = 0;
    cmd.VertexCount = 6;
    return cmd;
}

template <Dimension D>
static Graph::DrawIndexedIndirectCommand createMeshCommand(const Resource mesh, const u32 firstInstance,
                                                           const u32 instanceCount)
{
    const MeshRange range = Mesh_GetRange<D>(mesh);
    Graph::DrawIndexedIndirectCommand cmd;
    cmd.FirstInstance = firstInstance;
    cmd.InstanceCount = instanceCount;
    cmd.FirstIndex = range.IndexOffset;
    cmd.IndexCount = range.IndexCount;
    cmd.VertexOffset = i32(range.VertexOffset);
    return cmd;
}

template <Dimension D>
static Graph::DrawIndexedIndirectCommand createDynamicMeshCommand(const GraphicsInstanceRange &grange,
                                                                  const u32 firstInstance, const u32 instanceCount)
{
    TKIT_ASSERT(grange.ActiveVertexRange && grange.ActiveIndexRange,
                "[ONYX][RENDERER] Cannot create a dynamic mesh command when any of the active vertex/index memory "
                "ranges are null");

    Graph::DrawIndexedIndirectCommand cmd;
    cmd.FirstInstance = firstInstance;
    cmd.InstanceCount = instanceCount;
    cmd.FirstIndex = grange.ActiveIndexRange->Offset / sizeof(Index);
    cmd.IndexCount = grange.ActiveIndexRange->Size / sizeof(Index);
    cmd.VertexOffset = i32(grange.ActiveVertexRange->Offset / sizeof(DynamicVertex<D>));

    return cmd;
}

static void transitionLayout(const Graph::CommandBuffer cmd, const Graph::Image image, const Graph::ImageLayout layout,
                             const Graph::AccessFlags srcAccess, const Graph::AccessFlags dstAccess,
                             const Graph::PipelineStageFlags srcStage, const Graph::PipelineStageFlags dstStage)
{
    Graph::ImageMemoryBarrierInfo barrier{};
    barrier.Image = image;
    barrier.NewLayout = layout;
    barrier.SrcStages = srcStage;
    barrier.SrcAccess = srcAccess;
    barrier.DstStages = dstStage;
    barrier.DstAccess = dstAccess;
    Graph::Command_ImageBarrier(cmd, barrier);
}

template <Dimension D> static void beginShadowTransitionLayout(const Graph::CommandBuffer cmd, TextureMap &map)
{
    const Graph::ImageLayout attLayout =
        D == D3 ? Graph::ImageLayout_DepthStencilAttachment : Graph::ImageLayout_ColorAttachment;
    transitionLayout(cmd, map.Image, attLayout, Graph::AccessFlag_ShaderRead,
                     D == D3 ? Graph::AccessFlag_DepthStencilAttachmentWrite : Graph::AccessFlag_ColorAttachmentWrite,
                     D == D3 ? Graph::PipelineStageFlag_FragmentShader : Graph::PipelineStageFlag_ComputeShader,
                     D == D3 ? Graph::PipelineStageFlag_EarlyFragmentTests
                             : Graph::PipelineStageFlag_ColorAttachmentOutput);
}

// NOTE: the depth maps used to be transitioned to VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL. Graph has no such layout, and the
// descriptors referencing these maps are written with the shader read only layout, so that is the one used instead
template <Dimension D> static void endShadowTransitionLayout(const Graph::CommandBuffer cmd, TextureMap &map)
{
    transitionLayout(cmd, map.Image, D == D3 ? Graph::ImageLayout_ShaderReadOnly : Graph::ImageLayout_General,
                     D == D3 ? Graph::AccessFlag_DepthStencilAttachmentWrite : Graph::AccessFlag_ColorAttachmentWrite,
                     Graph::AccessFlag_ShaderRead,
                     D == D3 ? Graph::PipelineStageFlag_LateFragmentTests
                             : Graph::PipelineStageFlag_ColorAttachmentOutput,
                     D == D3 ? Graph::PipelineStageFlag_FragmentShader : Graph::PipelineStageFlag_ComputeShader);
}

template <Dimension D>
static void beginShadowPass(const Graph::CommandBuffer cmd, TextureMap &map, const u32 imageViewIndex = 0)
{
    const Graph::ImageLayout attLayout =
        D == D3 ? Graph::ImageLayout_DepthStencilAttachment : Graph::ImageLayout_ColorAttachment;

    Graph::AttachmentInfo att{};
    att.View = Graph::Image_GetView(map.Image, imageViewIndex);
    att.Layout = attLayout;
    att.LoadOp = Graph::LoadOp_Clear;
    att.StoreOp = Graph::StoreOp_Store;

    Graph::RenderPassInfo renderInfo{};
    renderInfo.RenderArea = {{0, 0}, map.Extent};
    renderInfo.LayerCount = 1;
    if constexpr (D == D3)
    {
        att.ClearValue.DepthStencil = {1.f, 0};
        renderInfo.DepthAttachment = &att;
    }
    else
    {
        att.ClearValue.Color = f32v4{0.f};
        renderInfo.ColorAttachments = att;
    }

    Graph::Command_SetViewport(cmd, f32v2{0.f}, f32v2{map.Extent});
    Graph::Command_SetScissor(cmd, i32v2{0}, map.Extent);

    Graph::Command_BeginRenderPass(cmd, renderInfo);
}

static void endShadowPass(const Graph::CommandBuffer cmd)
{
    Graph::Command_EndRenderPass(cmd);
}

static void addTransferTrackerIfNeeded(TKit::StackArray<Graph::Tracker> &transferTrackers,
                                       const Graph::Tracker &tracker)
{
    bool found = false;
    for (Graph::Tracker &tr : transferTrackers)
    {
        if (tr.Queue == tracker.Queue)
        {
            found = true;
            if (tr.InFlightValue < tracker.InFlightValue)
                tr.InFlightValue = tracker.InFlightValue;
            break;
        }
    }
    if (!found)
        transferTrackers.Append(tracker);
}

template <Dimension D, typename F>
static void collectDrawInfo(const Graph::Tracker &tracker, const Geometry geo, const ViewMask viewBit,
                            const F &insertCommand, const RenderModeFlags flags,
                            TKit::StackArray<Graph::Tracker> *transferTrackers = nullptr,
                            const BlendPass bpass = BlendPass_All)
{
    RendererData<D> &rdata = getRendererData<D>();
    GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;
    const u32 instanceSize = GetInstanceSize<D>(geo);
    const ResourceType rtype = getResourceType(geo);

    for (GraphicsInstanceRange &grange : gpool.Ranges)
    {
        if (!(grange.ViewMask & viewBit) || !(grange.RenderFlags & flags) ||
            (bpass != BlendPass_All && grange.Blend != bpass))
            continue;

        TKIT_ASSERT(!grange.ContextRanges.IsEmpty(),
                    "[ONYX][RENDERER] Context ranges cannot be empty for a graphics memory range");

        TKIT_ASSERT(geo != Geometry_Circle || grange.MeshHandle == NullHandle,
                    "[ONYX][RENDERER] Circle geometry is required to have a null mesh handle");

        usz offset = grange.Offset;
        usz size = 0;

        bool found = false;
        for (const ContextInstanceRange &crange : grange.ContextRanges)
        {
            if (rdata.IsContextRangeClean(viewBit, crange))
                size += crange.Size;
            else if (size != 0)
            {
                const u32 fi = u32(offset / instanceSize);
                const u32 ic = u32(size / instanceSize);

                offset += size + crange.Size;
                size = 0;
                insertCommand(rtype, grange, fi, ic);
                found = true;
            }
            else
                offset += crange.Size;
        }
        // TKIT_ASSERT(size != 0 || offset > grange.Offset,
        //             "[ONYX][RENDERER] Found labeled graphics memory pool range for window with view bit {} with
        //             " "no context ranges targetting said view", viewBit);
        if (size != 0)
        {
            const u32 fi = u32(offset / instanceSize);
            const u32 ic = u32(size / instanceSize);
            insertCommand(rtype, grange, fi, ic);
        }
        else if (!found)
            continue;

        grange.GraphicsTracker = tracker;
        if (transferTrackers && grange.InUseByTransfer())
            addTransferTrackerIfNeeded(*transferTrackers, grange.TransferTracker);

        if (grange.ActiveVertexRange)
        {
            grange.ActiveVertexRange->GraphicsTracker = tracker;
            if (transferTrackers && grange.ActiveVertexRange->InUseByTransfer())
                addTransferTrackerIfNeeded(*transferTrackers, grange.ActiveVertexRange->TransferTracker);
        }
        if (grange.ActiveIndexRange)
        {
            grange.ActiveIndexRange->GraphicsTracker = tracker;
            if (transferTrackers && grange.ActiveIndexRange->InUseByTransfer())
                addTransferTrackerIfNeeded(*transferTrackers, grange.ActiveIndexRange->TransferTracker);
        }
    }
}

template <Dimension D>
static void setupState(const Graph::CommandBuffer cmd, const RenderPass rpass, const Geometry geo,
                       const Graph::PipelineLayout playout, const Graph::Pipeline pipeline)
{
    const RendererData<D> &rdata = getRendererData<D>();
    const Graph::DescriptorSet set = rdata.Descriptors[rpass][geo];

    Graph::Command_BindPipeline(cmd, pipeline);
    Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Graphics, set);
}

enum CullMode : u8
{
    CullMode_None,
    CullMode_Back,
    CullMode_Count,
};

// TODO(Isma): These could be stack arrays
//  per draw cmd
using CircleDrawCommands = TKit::TierArray<Graph::DrawIndirectCommand>;

using IndexedCommands = TKit::TierArray<Graph::DrawIndexedIndirectCommand>;

// per mesh type per resource pool per cull mode per draw cmd
using PerCullPerCmd = ten<IndexedCommands, CullMode_Count>;
using MeshDrawCommands = ten<IndexedCommands, Resource_MeshPoolCount, ONYX_MAX_RESOURCE_POOLS, CullMode_Count>;

// per cull mode per draw cmd
using DynMeshDrawCommands = ten<IndexedCommands, CullMode_Count>;

TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4127)
template <Dimension D>
static void submitDrawCommands(const Graph::Tracker &tracker, const Graph::CommandBuffer cmd, const RenderPass rpass,
                               const Graph::PipelineLayout playout,
                               const ten<Graph::Pipeline, Geometry_Count> &pipelines,
                               const CircleDrawCommands &circleCmds, const MeshDrawCommands &meshCmds,
                               const DynMeshDrawCommands &dynMeshCmds)
{
    const u32 drawCount = circleCmds.GetSize();
    const usz size = circleCmds.GetBytes();
    if (drawCount != 0)
    {
        setupState<D>(cmd, rpass, Geometry_Circle, playout, pipelines[Geometry_Circle]);
        Graph::Buffer *dbuffer = findAvailableDrawBuffer(drawCount, tracker);

        Graph::Buffer_Write(*dbuffer, circleCmds.GetData(), {.Size = size});
        Graph::Buffer_Flush(*dbuffer);
        Graph::Command_DrawIndirect(cmd, *dbuffer, drawCount);
    }

    // NOTE(Isma): Bit of a mess, should consider cleaning this up

    const auto hasCommands = [](const PerCullPerCmd &drawCmds) {
        return !drawCmds[CullMode_None].IsEmpty() || !drawCmds[CullMode_Back].IsEmpty();
    };

    const auto drawCulledMeshes = [&]([[maybe_unused]] const Geometry geo, const PerCullPerCmd &drawCmds) {
        const TKit::TierArray<Graph::DrawIndexedIndirectCommand> &cmds1 = drawCmds[CullMode_None];
        const TKit::TierArray<Graph::DrawIndexedIndirectCommand> &cmds2 = drawCmds[CullMode_Back];

        const u32 dc1 = cmds1.GetSize();
        const u32 dc2 = cmds2.GetSize();
        const u32 drawCount = dc1 + dc2;

        const usz size1 = cmds1.GetBytes();
        const usz size2 = cmds2.GetBytes();

        TKIT_ASSERT((D == D3 && geo != Geometry_Glyph) || size2 == 0,
                    "[ONYX][RENDERER] No back culling draw commands must be submitted for flat geometry");

        Graph::Buffer *dbuffer = findAvailableIndexedDrawBuffer(drawCount, tracker);

        if (D == D2 || size1 != 0)
            Graph::Buffer_Write(*dbuffer, cmds1.GetData(), {.Size = size1});
        if constexpr (D == D3)
            if (size2 != 0)
                Graph::Buffer_Write(*dbuffer, cmds2.GetData(), {.Size = size2, .DstOffset = size1});

        Graph::Buffer_Flush(*dbuffer);

        if constexpr (D == D3)
            if (geo != Geometry_Glyph)
                Graph::Command_SetCullMode(cmd, Graph::CullMode_None);

        if (D == D2 || size1 != 0)
            Graph::Command_DrawIndexedIndirect(cmd, *dbuffer, dc1);
        if constexpr (D == D3)
            if (size2 != 0)
            {
                Graph::Command_SetCullMode(cmd, Graph::CullMode_Back);
                Graph::Command_DrawIndexedIndirect(cmd, *dbuffer, dc2, size1);
            }
    };

    const auto renderMeshGeometry = [&](const Geometry geo) {
        setupState<D>(cmd, rpass, geo, playout, pipelines[geo]);

        const ResourceType rtype = getResourceType(geo);
        const TKit::Span<const u32> poolIds = ResourcePool_GetIds<D>(rtype);

        for (const ResourcePool pid : poolIds)
            if (hasCommands(meshCmds[rtype][pid]))
            {
                bindMeshBuffers<D>(Handle_CreateForResourcePool(rtype, pid), cmd);
                drawCulledMeshes(geo, meshCmds[rtype][pid]);
            }
    };

    renderMeshGeometry(Geometry_Static);
    renderMeshGeometry(Geometry_Parametric);
    renderMeshGeometry(Geometry_Glyph);

    if (hasCommands(dynMeshCmds))
    {
        RendererData<D> &rdata = getRendererData<D>();
        setupState<D>(cmd, rpass, Geometry_Dynamic, playout, pipelines[Geometry_Dynamic]);
        Graph::Command_BindVertexBuffers(cmd, rdata.Geometry.VertexArena.Graphics.Buffer);
        Graph::Command_BindIndexBuffer(cmd, rdata.Geometry.IndexArena.Graphics.Buffer, s_IndexType);

        drawCulledMeshes(Geometry_Dynamic, dynMeshCmds);
    }
}
TKIT_COMPILER_WARNING_IGNORE_POP()

template <Dimension D> static f32m4 unpackTransform(const PackedTransform<D> &packed)
{
    if constexpr (D == D2)
        return f32m4{f32v4{packed[0], 0.f, 0.f}, f32v4{packed[1], 0.f, 0.f}, f32v4{0.f, 0.f, 0.f, 0.f},
                     f32v4{packed[2], 0.f, 1.f}};
    else
        return f32m4{f32v4{packed[0], 0.f}, f32v4{packed[1], 0.f}, f32v4{packed[2], 0.f}, f32v4{packed[3], 1.f}};
}

static CullMode getCullMode(const Resource handle)
{
    return CullMode(Resources_IsBackCulled(handle));
}

template <Dimension D>
static void renderShadows(const Graph::CommandBuffer cmd, const ViewMask viewBit, const Graph::Tracker &tracker)
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    const ViewMask dirtyViews = sdata.DirtyShadowViews;
    sdata.DirtyShadowViews &= ~viewBit;

    const auto processLight = [&]<typename LightParams>(const LightType ltype,
                                                        const ContextLights<LightParams> &lights) {
        using LightData = LightParams::InstanceData;
        TKIT_ASSERT(
            lights.Parameters.GetSize() == lights.Data.GetSize(),
            "[ONYX][RENDERER] Light count and light instance data count must match, but former has {} element(s) "
            "while latter has {}",
            lights.Parameters.GetSize(), lights.Data.GetSize());

        const u32 size = lights.Data.GetSize();
        for (u32 i = 0; i < size; ++i)
        {
            const LightData &data = lights.Data[i];

            static constexpr bool isPoint = std::is_same_v<LightParams, PointLightParameters<D>>;
            static constexpr bool isDir = std::is_same_v<LightParams, DirectionalLightParameters<D>>;
            static constexpr bool isSpot = std::is_same_v<LightParams, SpotLightParameters>;

            if (!(data.ViewMask & viewBit) || !(data.Flags & LightFlag_CastShadows))
                continue;
            if constexpr (D == D3 && isDir)
                if (data.CascadeEnable == 0)
                    continue;

            const LightParams &params = lights.Parameters[i];

            const u32 viewIndex = std::popcount(data.ViewMask & (viewBit - 1));
            const u32 shindex = data.ShadowMapOffset + viewIndex;
            const u32 shPoolIndex = computeShadowMapPoolIndex<D>(ltype, shindex);

            TextureMapArray &shadowMaps = sdata.ShadowMaps[ltype];
            TextureMap &smap = shadowMaps[shPoolIndex];

            smap.Tracker = tracker;

            if (!(dirtyViews & viewBit))
                continue;

            CircleDrawCommands circleCmds{};
            DynMeshDrawCommands dynMeshCmds{};
            MeshDrawCommands meshCmds{};
            const auto insertCommand = [&](const ResourceType rtype, const GraphicsInstanceRange &grange, const u32 fi,
                                           const u32 ic) {
                if (grange.MeshHandle == NullHandle) // circles sentry
                    circleCmds.Append(createCircleCommand(fi, ic));
                else
                {
                    ONYX_CHECK_RESOURCE_IS_NOT_NULL(grange.MeshHandle);
#ifdef TKIT_ENABLE_ENSURE
                    if (rtype != Resource_DynamicMesh)
                    {
                        ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(grange.MeshHandle);
                        ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(grange.MeshHandle, rtype, D);
                    }
#endif
                    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(grange.MeshHandle, rtype, D);

                    const u32 pid = Handle_GetResourcePoolId(grange.MeshHandle);
                    PerCullPerCmd &cmds = rtype == Resource_DynamicMesh ? dynMeshCmds : meshCmds[rtype][pid];
                    const Graph::DrawIndexedIndirectCommand cmd = rtype == Resource_DynamicMesh
                                                                      ? createDynamicMeshCommand<D>(grange, fi, ic)
                                                                      : createMeshCommand<D>(grange.MeshHandle, fi, ic);

                    if constexpr (D == D2)
                        cmds[CullMode_None].Append(cmd);
                    else
                        cmds[getCullMode(grange.MeshHandle)].Append(cmd);
                }
            };
            for (u32 j = 0; j < Geometry_Count; ++j)
            {
                const Geometry geo = Geometry(j);
                // filtering by _Shaded saves us from a lot of computation (obviously) but most importantly avoids
                // misinterpretation of the MatOrSamplerTex field in the instance data with flat render modes
                collectDrawInfo<D>(tracker, geo, viewBit, insertCommand, RenderModeFlag_Shaded);
            }

            const auto processMap = [&](TextureMap &map, const f32m4 &projView, const u32 viewIndex = 0) {
                beginShadowPass<D>(cmd, map, viewIndex);
                const Graph::PipelineLayout playout = getPipelineLayout<D>(RenderPass_Shadow);

                ShadowPushConstantData<D> pdata;
                pdata.LightProjection = projView;

                Graph::ShaderStageFlags flags = Graph::ShaderStageFlag_Vertex;
                if constexpr (D == D3)
                {
                    flags |= Graph::ShaderStageFlag_Fragment;
                    if constexpr (isSpot || isPoint)
                        pdata.DepthBias = params.DepthBias;
                    else
                        pdata.DepthBias = params.Cascades.DepthBias[viewIndex];

                    if constexpr (isPoint)
                    {
                        pdata.LightPos = data.Position;
                        pdata.Far = data.ShadowRadius;
                    }
                    else
                        pdata.Far = 0.f;
                }

                Graph::Command_PushConstants(cmd, playout, flags, &pdata);
                submitDrawCommands<D>(tracker, cmd, RenderPass_Shadow, playout, sdata.Pipelines, circleCmds, meshCmds,
                                      dynMeshCmds);

                endShadowPass(cmd);
            };

            if constexpr (D == D2)
            {
                const u32 ocindex = findAvailableOcclusionMap();
                TextureMap &ocmap = sdata.OcclusionMaps[ocindex];
                ocmap.Tracker = tracker;

                beginShadowTransitionLayout<D2>(cmd, ocmap);
                if constexpr (isPoint)
                {
                    const f32 r = data.ShadowRadius;
                    f32m3 pv = Transform<D2>::Orthographic(-r, r, -r, r);
                    pv[2][0] -= data.Position[0] * pv[0][0];
                    pv[2][1] -= data.Position[1] * pv[1][1];
                    processMap(ocmap, Transform<D2>::Promote(pv));
                }
                else
                    processMap(ocmap, unpackTransform<D>(data.ProjectionView));

                endShadowTransitionLayout<D2>(cmd, ocmap);

                transitionLayout(cmd, smap.Image, Graph::ImageLayout_General, Graph::AccessFlag_ShaderRead,
                                 Graph::AccessFlag_ShaderWrite, Graph::PipelineStageFlag_FragmentShader,
                                 Graph::PipelineStageFlag_ComputeShader);

                const Graph::DescriptorSet set = sdata.RayMarchSet;

                Graph::Command_BindPipeline(cmd, sdata.RayMarchPipeline);
                const Graph::PipelineLayout playout = getPipelineLayout(StandalonePass_RayMarch);
                Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Compute, set);

                RayMarchPushConstantData pdata;
                pdata.OcclusionMapIndex = ocindex;
                pdata.OcclusionResolution = sdata.OcclusionResolution;
                pdata.ShadowMapIndex = shindex;
                pdata.ShadowResolution = sdata.ShadowResolutions[ltype];
                pdata.DistanceBias = params.DepthBias;

                if constexpr (isPoint)
                {
                    const f32 ext = Math::Pi() * data.Extent;
                    const f32 angle = params.Angle;
                    pdata.StartAngle = angle - ext;
                    pdata.EndAngle = angle + ext;
                    pdata.Mode = ONYX_MARCH_MODE_POINT_LIGHT;
                }
                else
                    pdata.Mode = ONYX_MARCH_MODE_DIRECTIONAL_LIGHT;

                Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Compute, &pdata);

                constexpr u32 groupSize = 64;
                Graph::Command_Dispatch(cmd, (pdata.ShadowResolution + groupSize - 1) / groupSize);

                transitionLayout(cmd, smap.Image, Graph::ImageLayout_ShaderReadOnly, Graph::AccessFlag_ShaderWrite,
                                 Graph::AccessFlag_ShaderRead, Graph::PipelineStageFlag_ComputeShader,
                                 Graph::PipelineStageFlag_FragmentShader);
            }
            else
            {
                beginShadowTransitionLayout<D3>(cmd, smap);
                // TODO(Isma): Try to pseudo optimize this by working out the views?
                if constexpr (isPoint)
                {
                    const f32 near = ONYX_POINT_LIGHT_NEAR_RADIUS_FACTOR * data.ShadowRadius;
                    const f32 far = data.ShadowRadius;
                    const f32m4 proj = Transform<D3>::Perspective(0.5f * Math::Pi(), near, far);

                    constexpr f32v3 faceDir[6] = {
                        f32v3{1.f, 0.f, 0.f},  f32v3{-1.f, 0.f, 0.f}, f32v3{0.f, 1.f, 0.f},
                        f32v3{0.f, -1.f, 0.f}, f32v3{0.f, 0.f, 1.f},  f32v3{0.f, 0.f, -1.f},
                    };
                    constexpr f32v3 faceUp[6] = {
                        f32v3{0.f, -1.f, 0.f}, f32v3{0.f, -1.f, 0.f}, f32v3{0.f, 0.f, 1.f},
                        f32v3{0.f, 0.f, -1.f}, f32v3{0.f, -1.f, 0.f}, f32v3{0.f, -1.f, 0.f},
                    };
                    for (u32 j = 0; j < 6; ++j)
                    {
                        const f32m4 view = Transform<D3>::LookTowards(data.Position, faceDir[j], faceUp[j]);
                        processMap(smap, proj * view, j);
                    }
                }
                else if constexpr (isDir)
                {
                    for (u32 j = 0; j < data.CascadeCount; ++j)
                        if ((1U << j) & data.CascadeEnable)
                            processMap(smap, unpackTransform<D>(data.Cascades[j].ProjectionView), j);
                }
                else
                    processMap(smap, data.ProjectionView);
                endShadowTransitionLayout<D3>(cmd, smap);
            }
        }
    };

    const LightData<D> &ldata = rdata.Lights;
    processLight(Light_Point, ldata.Instances.Points);
    processLight(Light_Directional, ldata.Instances.Directionals);
    if constexpr (D == D3)
        processLight(Light_Spot, ldata.Instances.Spots);
}

template <Dimension D>
static void renderGeometry(const Graph::CommandBuffer cmd, const ViewInfo<D> &vinfo, const BlendPass bpass,
                           const Graph::Tracker &tracker, TKit::StackArray<Graph::Tracker> &transferTrackers,
                           const bool shadows)
{
    const ViewMask viewBit = vinfo.ViewBit;

    RendererData<D> &rdata = getRendererData<D>();

    // TODO(Isma): At some point would be good letting the user decide what strategy to use to aggregate ambient
    Color ambient{0.f, 0.f};
    for (const ContextInfo<D> &info : rdata.Contexts)
        if (info.Context->ViewMask & viewBit)
        {
            const Color &a = info.Context->GetAmbientLight();
            ambient.rgba = Math::Max(ambient.rgba, a.rgba);
        }
    const u32 ambientColor = ambient.ToLinear().Pack();

    TKit::FixedArray<CircleDrawCommands, PipelinePass_Count> circleCmds{};
    TKit::FixedArray<DynMeshDrawCommands, PipelinePass_Count> dynMeshCmds{};
    TKit::FixedArray<MeshDrawCommands, PipelinePass_Count> meshCmds{};
    TKit::FixedArray<u32, PipelinePass_Count> cmdCount{0, 0, 0};

    const auto insertCommand = [&](const ResourceType rtype, const GraphicsInstanceRange &grange, const u32 fi,
                                   const u32 ic) {
        u32 pcount = 0;
        TKit::FixedArray<PipelinePass, 3> passes;
        const RenderModeFlags flags = grange.RenderFlags;
        if (flags & RenderModeFlag_Shaded)
            passes[pcount++] = PipelinePass_Shaded;
        if (flags & RenderModeFlag_Flat)
            passes[pcount++] = PipelinePass_Flat;
        if (flags & RenderModeFlag_Outlined)
            passes[pcount++] = PipelinePass_Outlined;
        TKIT_ASSERT(pcount != 0, "[ONYX][RENDERER] Pass count should not be zero");

        if (grange.MeshHandle == NullHandle) // circles sentry
            for (u32 i = 0; i < pcount; ++i)
            {
                const PipelinePass p = passes[i];
                circleCmds[p].Append(createCircleCommand(fi, ic));
                ++cmdCount[p];
            }
        else
        {
            ONYX_CHECK_RESOURCE_IS_NOT_NULL(grange.MeshHandle);
#ifdef TKIT_ENABLE_ENSURE
            if (rtype != Resource_DynamicMesh)
            {
                ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(grange.MeshHandle);
                ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(grange.MeshHandle, rtype, D);
            }
#endif
            ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(grange.MeshHandle, rtype, D);

            const u32 pid = Handle_GetResourcePoolId(grange.MeshHandle);

            CullMode cull;
            if constexpr (D == D2)
                cull = CullMode_None;
            else
                cull = getCullMode(grange.MeshHandle);

            const Graph::DrawIndexedIndirectCommand cmd = rtype == Resource_DynamicMesh
                                                              ? createDynamicMeshCommand<D>(grange, fi, ic)
                                                              : createMeshCommand<D>(grange.MeshHandle, fi, ic);
            if (rtype == Resource_DynamicMesh)
                for (u32 i = 0; i < pcount; ++i)
                {
                    const PipelinePass p = passes[i];
                    dynMeshCmds[p][cull].Append(cmd);
                    ++cmdCount[p];
                }
            else
                for (u32 i = 0; i < pcount; ++i)
                {
                    const PipelinePass p = passes[i];
                    meshCmds[p][rtype][pid][cull].Append(cmd);
                    ++cmdCount[p];
                }
        }
    };

    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        collectDrawInfo<D>(tracker, geo, viewBit, insertCommand,
                           RenderModeFlag_Shaded | RenderModeFlag_Outlined | RenderModeFlag_Flat, &transferTrackers,
                           bpass);
    }

    LightData<D> &ldata = rdata.Lights;
    for (u32 i = 0; i < ldata.Arenas.GetSize(); ++i)
    {
        LightArena &arena = ldata.Arenas[i];
        if (arena.LightCount == 0)
            continue;

        TKIT_ASSERT(arena.ActiveRange && arena.ActiveRange->Generation == arena.ActiveGeneration,
                    "[ONYX][RENDERER] Active light graphics range arena does not have a valid generation or is just "
                    "null (forgot to call Renderer::PrepareRender()?)");

        arena.ActiveRange->GraphicsTracker = tracker;
        if (arena.ActiveRange->InUseByTransfer())
            addTransferTrackerIfNeeded(transferTrackers, arena.ActiveRange->TransferTracker);
    }

    ShadowData<D> &sdata = rdata.Shadows;
    for (u32 i = 0; i < PipelinePass_Count; ++i)
    {
        if (cmdCount[i] == 0)
            continue;
        const PipelinePass pass = PipelinePass(i);
        const RenderPass rpass = GetRenderPass(pass);
        const Graph::PipelineLayout playout = getPipelineLayout<D>(rpass);
        if (rpass == RenderPass_Flat)
            Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Vertex, &vinfo.ProjectionView);
        else
        {
            ShadedPushConstantData<D> pdata;
            pdata.ProjectionView = vinfo.ProjectionView;
            pdata.Flags = ShadedFlag_Shadows * shadows;

            for (u32 j = 0; j < LightTypeCount<D>; ++j)
            {
                pdata.TexelSizes[j] = 1.f / f32(sdata.ShadowResolutions[j]);
                pdata.LightRanges[j] = (ldata.Ranges[j].Offset << 16) | ldata.Ranges[j].Count;
            }

            if constexpr (D == D3)
            {
                pdata.ViewPosition = vinfo.ViewPosition;
                pdata.ViewForward = vinfo.ViewForward;
            }

            pdata.ViewBit = viewBit;
            pdata.AmbientColor = ambientColor;
            Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Vertex | Graph::ShaderStageFlag_Fragment,
                                         &pdata);
        }

        const u32 idx = bpass == BlendPass_All ? BlendPass_Opaque : bpass;
        submitDrawCommands<D>(tracker, cmd, rpass, playout, rdata.Geometry.Pipelines[idx][pass], circleCmds[pass],
                              meshCmds[pass], dynMeshCmds[pass]);
    }
}

static RenderSubmitInfo createRenderSubmitInfo(const Graph::CommandBuffer command, const u64 graphicsFlight,
                                               const RenderTargetInfo &tinfo,
                                               TKit::StackArray<Graph::Tracker> &transferTrackers)
{
    RenderSubmitInfo submitInfo{};
    submitInfo.Command = command;
    submitInfo.Swapchain = tinfo.Swapchain;
    submitInfo.InFlightValue = graphicsFlight;

    for (const Graph::Tracker &ttracker : transferTrackers)
    {
        Graph::WaitInfo &wait = submitInfo.WaitValues.Append();
        wait.Value = ttracker.InFlightValue;
        wait.QueueType = ttracker.Queue;
        wait.StageFlags = Graph::PipelineStageFlag_VertexShader;
    }
    return submitInfo;
}

template <Dimension D>
static void renderViews(const TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &views, const Graph::CommandBuffer cmd,
                        const Graph::Tracker &tracker, TKit::StackArray<Graph::Tracker> &transferTrackers)
{
    TKit::StackArray<RenderView<D> *> ppViews{};
    ppViews.Reserve(views.GetSize());

    for (RenderView<D> *rv : views)
    {
        const RenderViewFlags flags = rv->GetFlags();
        if (flags & RenderViewFlag_Hidden)
            continue;
        if (flags & RenderViewFlag_PostProcess)
            ppViews.Append(rv);

        const bool shadows = flags & RenderViewFlag_Shadows;
        if (shadows)
            renderShadows<D>(cmd, rv->GetViewBit(), tracker);

        rv->CacheMatrices();
        const ViewInfo<D> vinfo = rv->CreateViewInfo();

        rv->MarkCurrentAttachmentsInUse(tracker);

        const bool transparency = flags & RenderViewFlag_Transparency;
        const BlendPass opaquePass = transparency ? BlendPass_Opaque : BlendPass_All;

        rv->BeginOpaquePass(cmd);
        renderGeometry<D>(cmd, vinfo, opaquePass, tracker, transferTrackers, shadows);
        rv->EndOpaquePass(cmd);
        if (transparency)
        {
            rv->BeginTransparentPass(cmd);
            renderGeometry<D>(cmd, vinfo, BlendPass_Transparent, tracker, transferTrackers, shadows);
            rv->EndTransparentPass(cmd);
            rv->BeginBlendPass(cmd);
            Graph::Command_BindPipeline(cmd, s_BlendPipeline);

            const Graph::PipelineLayout playout = getPipelineLayout(StandalonePass_Blend);
            const Graph::DescriptorSet set = rv->GetBlendSet();
            Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Graphics, set);

            const u32 idx = rv->GetAttachmentIndex();
            TKIT_ASSERT(idx < ONYX_MAX_ATTACHMENTS,
                        "[ONYX][RENDERER] The maximum amount of attachments has been exceeded ({} >= {})", idx,
                        ONYX_MAX_ATTACHMENTS);

            static_assert(std::is_same_v<BlendPushConstantData, u32>);
            Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Fragment, &idx);
            Graph::Command_Draw(cmd, 6);

            rv->EndBlendPass(cmd);
        }
    }

    if (!ppViews.IsEmpty())
    {
        const Graph::PipelineLayout playout = getPipelineLayout(StandalonePass_PostProcess);
        for (RenderView<D> *rv : ppViews)
        {
            rv->BeginPostProcess(cmd);
            Graph::Command_BindPipeline(cmd, s_PostProcessPipeline);

            const Graph::DescriptorSet set = rv->GetPostProcessSet();
            Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Graphics, set);

            PostProcessPushConstantData pdata;
            pdata.AttachmentIndex = rv->GetAttachmentIndex();

            TKIT_ASSERT(pdata.AttachmentIndex < ONYX_MAX_ATTACHMENTS,
                        "[ONYX][RENDERER] The maximum amount of attachments has been exceeded ({} >= {})",
                        pdata.AttachmentIndex, ONYX_MAX_ATTACHMENTS);

            pdata.Extent = rv->GetRenderExtent();
            pdata.MaxOutlineWidth = rv->MaxOutlineWidth;
            pdata.Flags = rv->GetFlags();

            Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Fragment, &pdata);
            Graph::Command_Draw(cmd, 6);

            rv->EndPostProcess(cmd);
        }
    }
}

template <Dimension D>
static void renderCompositor(const TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &views,
                             const Graph::CommandBuffer cmd, const Graph::PipelineLayout playout)
{
    for (const RenderView<D> *rv : views)
    {
        if (rv->GetFlags() & RenderViewFlag_Hidden)
            continue;

        const Graph::DescriptorSet set = rv->GetCompositorSet();
        Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Graphics, set);

        // TODO(Isma): Here, you must get the scissor normalized wrt viewport, re-normalize to screen with the viewport
        // coordinates, and multiply by the parent extent
        const Viewport vp = rv->GetAbsoluteViewport();
        const Viewport nvp = rv->GetNormalizedViewport();
        const f32v2 pext = f32v2{rv->GetParentExtent()};

        // the +2 here adds a very small offset that prevents some (rounding??) error that causes a bit of undefined
        // texture to be rendered, clipped from the view's attachment
        // EDIT: removed, causes the render target not to cover the whole screen
        // const f32 padding = 2.f;
        Scissor sc = rv->GetNormalizedScissor();
        sc.Position = (sc.Position * nvp.Extent + nvp.Position) * pext; // (pext + padding);
        sc.Extent *= nvp.Extent * pext;                                 // (pext - padding);

        const u32 idx = rv->GetAttachmentIndex();

        Graph::Command_SetViewport(cmd, vp.Position, vp.Extent, vp.Depth);
        Graph::Command_SetScissor(cmd, i32v2{sc.Position}, u32v2{sc.Extent});
        Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Fragment, &idx);
        Graph::Command_Draw(cmd, 6);
    }
}

template <typename Target>
RenderSubmitInfo render(const Graph::CommandBuffer cmd, Target *target, const RenderFlags flags)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Render");

    const u64 graphicsFlight = Graph::Queue_ReserveTimelineValue(Graph::Queue_Graphics);
    const RenderTargetInfo tinfo = target->CreateRenderTargetInfo();

    TKit::StackArray<Graph::Tracker> transferTrackers{};
    transferTrackers.Reserve(s_SyncPointCount);

    Graph::Tracker tracker{};
    tracker.Queue = Graph::Queue_Graphics;
    tracker.InFlightValue = graphicsFlight;

    renderViews(tinfo.Views2, cmd, tracker, transferTrackers);
    renderViews(tinfo.Views3, cmd, tracker, transferTrackers);

    // windows do not need to mark their image as in use: the swap chain is tracked by Graph on submission
    if constexpr (!std::is_same_v<Target, Window>)
        target->MarkWriteImageInUse(tracker);

    target->BeginRendering(cmd);

    Graph::Command_BindPipeline(cmd, s_CompositorPipeline);

    const Graph::PipelineLayout playout = getPipelineLayout(StandalonePass_Compositor);
    renderCompositor(tinfo.Views2, cmd, playout);
    renderCompositor(tinfo.Views3, cmd, playout);

#ifdef ONYX_ENABLE_IMGUI
    if constexpr (std::is_same_v<Target, Window>)
        if (flags & RenderFlag_ImGui)
        {
            ImGui::Render();
            ImGuiBackend_RenderData(ImGui::GetDrawData(), cmd);
            ImGuiBackend_UpdatePlatformWindows();
        }
#else
    TKIT_UNUSED(flags);
#endif

    target->EndRendering(cmd);
    return createRenderSubmitInfo(cmd, graphicsFlight, tinfo, transferTrackers);
}

RenderSubmitInfo Renderer_Render(const Graph::CommandBuffer cmd, Window *window, const RenderFlags flags)
{
    return render(cmd, window, flags);
}
RenderSubmitInfo Renderer_Render(const Graph::CommandBuffer cmd, RenderTexture *rtex)
{
    return render(cmd, rtex, 0);
}

void Renderer_SubmitRender(const Graph::CommandPool pool, const TKit::Span<const RenderSubmitInfo> info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::SubmitRender");
    TKIT_ASSERT(!info.IsEmpty(), "[ONYX][RENDERER] Must at least provide one submission");
    TKit::StackArray<Graph::SubmitInfo> submits{};
    submits.Reserve(info.GetSize());

    u64 maxFlight = 0;
    for (const RenderSubmitInfo &rinfo : info)
    {
        TKIT_ASSERT(rinfo.Command != Graph::NullHandle,
                    "[ONYX][RENDERER] A submission must have a valid graphics command buffer to be submitted");
        if (rinfo.InFlightValue > maxFlight)
            maxFlight = rinfo.InFlightValue;

        Graph::SubmitInfo &sinfo = submits.Append();
        sinfo.Commands = rinfo.Command;
        if (!rinfo.WaitValues.IsEmpty())
            sinfo.Waits = TKit::Span<const Graph::WaitInfo>{rinfo.WaitValues.GetData(), rinfo.WaitValues.GetSize()};
        // when a swap chain is provided, Graph takes care of the image available/render finished semaphores
        sinfo.Swapchain = rinfo.Swapchain;
        sinfo.SignalValue = rinfo.InFlightValue;
    }

    MarkCommandPoolInUse(pool, maxFlight);
    Graph::Queue_Submit(Graph::Queue_Graphics, submits);
}

template <typename Range, typename F> static void coalesceRanges(Pool<Range> &pool, F &&isInUse)
{
    Range mergeRange{};
    TKit::StackArray<Range> ranges{};
    ranges.Reserve(pool.Ranges.GetSize());

    for (const Range &range : pool.Ranges)
    {
        if (isInUse(range))
        {
            if (mergeRange.Size != 0)
            {
                ranges.Append(mergeRange);
                mergeRange.Offset += mergeRange.Size + range.Size;
                mergeRange.Size = 0;
            }
            else
                mergeRange.Offset += range.Size;
            ranges.Append(range);
        }
        else
            mergeRange.Size += range.Size;
    }
    if (mergeRange.Size != 0)
        ranges.Append(mergeRange);

    TKIT_ASSERT(!ranges.IsEmpty(),
                "[ONYX][RENDERER] All memory ranges for a pool have been removed after coalesce operation!");
    pool.Ranges = ranges;
}

template <typename Range> static void coalesceRanges(Pool<Range> &pool)
{
    coalesceRanges(pool, [](const Range &range) { return range.Tracker.InUse(); });
}

template <Dimension D> static void coalesceGraphicsInstanceRanges(GraphicsInstancePool &gpool, const u32 maxRanges)
{
    const RendererData<D> &rdata = getRendererData<D>();
    GraphicsInstanceRange gmergeRange{};
    TKit::StackArray<GraphicsInstanceRange> granges{};
    granges.Reserve(maxRanges);

    for (const GraphicsInstanceRange &grange : gpool.Ranges)
    {
        if (grange.InUse())
        {
            if (gmergeRange.Size != 0)
            {
                granges.Append(gmergeRange);
                gmergeRange.Offset += gmergeRange.Size + grange.Size;
                gmergeRange.Size = 0;
            }
            else
                gmergeRange.Offset += grange.Size;
            granges.Append(grange);
        }
        else if (!grange.ContextRanges.IsEmpty())
        {
            TKit::StackArray<ContextInstanceRange> cranges{};
            cranges.Reserve(grange.ContextRanges.GetSize());

            TKIT_ASSERT(grange.Size != 0, "[ONYX][RENDERER] Graphics memory range should not have reached a zero "
                                          "size if there are context ranges left");
            GraphicsInstanceRange ngrange{};
            ngrange.Offset = grange.Offset;
            ngrange.RenderFlags = grange.RenderFlags;
            ngrange.MeshHandle = grange.MeshHandle;

            // usz leftover = grange.Size;
            for (const ContextInstanceRange &crange : grange.ContextRanges)
            {
                // leftover -= crange.Size;
                if (rdata.IsContextRangeClean(crange))
                {
                    if (gmergeRange.Size != 0)
                    {
                        granges.Append(gmergeRange);
                        gmergeRange.Offset += gmergeRange.Size + crange.Size;
                        gmergeRange.Size = 0;
                    }
                    else
                        gmergeRange.Offset += crange.Size;

                    ContextInstanceRange &ncrange = cranges.Append(crange);
                    ncrange.Offset = ngrange.Size;
                    ngrange.Size += crange.Size;
                    ngrange.ViewMask |= crange.ViewMask;
                }
                else
                {
                    if (ngrange.Size != 0)
                    {
                        ngrange.ContextRanges = cranges;
                        granges.Append(ngrange);
                        ngrange.Offset += ngrange.Size + crange.Size;
                        ngrange.Size = 0;
                        ngrange.ViewMask = 0;
                        cranges.Clear();
                    }
                    else
                        ngrange.Offset += crange.Size;
                    gmergeRange.Size += crange.Size;
                }
            }
            if (ngrange.Size != 0)
            {
                ngrange.ContextRanges = cranges;
                granges.Append(ngrange);
            }
            // gmergeRange.Size += leftover;
        }
        else
            gmergeRange.Size += grange.Size;
    }
    if (gmergeRange.Size != 0)
        granges.Append(gmergeRange);

    TKIT_ASSERT(!granges.IsEmpty(),
                "[ONYX][RENDERER] All memory ranges for the graphics pool have been removed after coalesce operation!");
    gpool.Ranges = granges;
}

template <Dimension D> void coalesce(const u32 maxRanges)
{
#ifdef TKIT_ENABLE_ENSURE
    validateRanges<D>();
#endif
    RendererData<D> &rdata = getRendererData<D>();
    for (InstanceArena &arena : rdata.Geometry.Arenas)
    {
        coalesceRanges(arena.Transfer);
        coalesceGraphicsInstanceRanges<D>(arena.Graphics, maxRanges);
    }
    LightData<D> &ldata = rdata.Lights;
    for (LightArena &arena : ldata.Arenas)
    {
        coalesceRanges(arena.Transfer);
        coalesceRanges(arena.Graphics, [&arena](const GraphicsRange &grange) {
            return grange.Generation == arena.ActiveGeneration || grange.InUse();
        });
    }

    coalesceRanges(rdata.Geometry.VertexArena.Transfer);
    coalesceRanges(rdata.Geometry.IndexArena.Transfer);

    // NOTE(Isma): This is NOT ideal at all
    coalesceRanges(rdata.Geometry.VertexArena.Graphics, [&](const GraphicsRange &vgrange) {
        if (vgrange.InUse())
            return true;
        for (const GraphicsInstanceRange &grange : rdata.Geometry.Arenas[Geometry_Dynamic].Graphics.Ranges)
            if (grange.ActiveVertexGeneration == vgrange.Generation)
                return true;
        return false;
    });
    coalesceRanges(rdata.Geometry.IndexArena.Graphics, [&](const GraphicsRange &vgrange) {
        if (vgrange.InUse())
            return true;
        for (const GraphicsInstanceRange &grange : rdata.Geometry.Arenas[Geometry_Dynamic].Graphics.Ranges)
            if (grange.ActiveIndexGeneration == vgrange.Generation)
                return true;
        return false;
    });

#ifdef TKIT_ENABLE_ENSURE
    validateRanges<D>();
#endif
}

void Renderer_Coalesce(const u32 maxRanges)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Coalesce");
    coalesce<D2>(maxRanges);
    coalesce<D3>(maxRanges);
}

template <typename... Args> static TKit::StackString fmt(fmt::format_string<Args...> str, Args &&...args)
{
    return TKit::StackString::Format(str, std::forward<Args>(args)...);
}
static TKit::StackString fmts(const usz bytes)
{
    if (bytes > 1_gib)
        return fmt("{:.2f} gib", f32(bytes) / f32(1_gib));
    if (bytes > 1_mib)
        return fmt("{:.2f} mib", f32(bytes) / f32(1_mib));
    if (bytes > 1_kib)
        return fmt("{:.2f} kib", f32(bytes) / f32(1_kib));
    return fmt("{:L} b", bytes);
}
static TKit::StackString fmtb(const usz bytes)
{
    return fmt("{:L} b", bytes);
}

static constexpr OverlayTreeFlags s_DrawLines = OverlayTreeFlag_DrawLines;
template <Dimension D, typename Range>
static void displayRanges(Overlay *ov, const TKit::StringView name, const Pool<Range> &pool, const u64 generation = 0)
{
    const RendererData<D> &rdata = getRendererData<D>();
    if (ov->PushTree({&pool, fmt("{} pool ranges ({})", name, pool.Ranges.GetSize())}, s_DrawLines))
    {
        ov->Text("Buffer size: {}", fmts(Graph::Buffer_GetSize(pool.Buffer)));
        for (const Range &range : pool.Ranges)
            if constexpr (std::is_same_v<Range, TransferInstanceRange> || std::is_same_v<Range, TransferLightRange>)
                ov->Text("{} ({}): {} - {}", range.Tracker.InUse() ? "IN-USE" : "FREE", fmts(range.Size),
                         fmtb(range.Offset), fmtb(range.Offset + range.Size));

            else if constexpr (std::is_same_v<Range, GraphicsInstanceRange>)
            {
                if (ov->PushTree(
                        {&range,
                         fmt("{} ({}): {} - {}",
                             range.InUse() ? "IN-USE"
                                           : (rdata.AreAllContextRangesDirty(range)
                                                  ? "FREE"
                                                  : (rdata.AreAllContextRangesClean(range) ? "CLEAN" : "FRAGMENTED")),
                             fmts(range.Size), fmtb(range.Offset), fmtb(range.Offset + range.Size))},
                        s_DrawLines))
                {
                    ov->Text("In use by transfer queue: {}", range.TransferTracker.InUse() ? "YES" : "NO");
                    ov->Text("In use by graphics queue: {}", range.GraphicsTracker.InUse() ? "YES" : "NO");
                    if (range.MeshHandle != NullHandle)
                        ov->Text("Mesh handle: {}", range.MeshHandle);

                    if (range.RenderFlags != 0)
                        ov->Text("Render mode: {}", ToString(GetRenderMode(range.RenderFlags)));
                    const TKit::StackString vmask = TKit::StackString::Format("{:032b}", range.ViewMask);
                    ov->Text("View mask: {}", vmask.CString());
                    if (ov->PushTree({&range.ContextRanges, fmt("Context ranges ({})", range.ContextRanges.GetSize())},
                                     s_DrawLines))
                    {
                        for (const ContextInstanceRange &crange : range.ContextRanges)
                            if (ov->PushTree(
                                    {&crange,
                                     fmt("{} ({}): {} - {}", rdata.IsContextRangeClean(crange) ? "CLEAN" : "DIRTY",
                                         fmts(crange.Size), fmtb(crange.Offset), fmtb(crange.Offset + crange.Size))},
                                    s_DrawLines))
                            {
                                if (crange.ContextIndex != TKIT_U32_MAX)
                                {
                                    ov->Text("Context index: {}", crange.ContextIndex);
                                    ov->Text("Context generation: {}", crange.Generation);
                                }
                                else
                                    ov->Text("Context index: None");

                                const TKit::StackString cvmask = TKit::StackString::Format("{:032b}", crange.ViewMask);
                                ov->Text("View mask: %s", cvmask.CString());
                                ov->PopTree();
                            }
                        ov->PopTree();
                    }
                    ov->PopTree();
                }
            }
            else
                ov->Text("{} ({}): {} - {}",
                         range.InUse() ? "IN-USE" : (range.Generation == generation ? "CLEAN" : "FREE"),
                         fmts(range.Size), fmtb(range.Offset), fmtb(range.Offset + range.Size));
        ov->PopTree();
    }
}

template <Dimension D, typename TRange, typename GRange>
void plotRanges(Overlay *ov, const Pool<TRange> &tpool, const Pool<GRange> &gpool, const u64 generation = 0)
{
    const RendererData<D> &rdata = getRendererData<D>();
    const OverlayStyle &st = ov->GetStyle();

    constexpr TKit::FixedArray<const char *, 5> status = {"FREE", "IN-USE", "CLEAN", "DIRTY", "FRAGMENTED"};
    const TKit::FixedArray<Color, 5> colors = {Color::FromHexadecimal(0x6B7280B3), Color::FromHexadecimal(0x22C55EB3),
                                               Color::FromHexadecimal(0x3B82F6B3), Color::FromHexadecimal(0xF59E0BB3),
                                               Color::FromHexadecimal(0xF97316B3)};

    const usz maxSize = Math::Max(Graph::Buffer_GetSize(tpool.Buffer), Graph::Buffer_GetSize(gpool.Buffer));
    if (maxSize == 0)
        return;

    const f32 invMax = 1.f / f32(maxSize);
    constexpr bool hasContextRow = std::is_same_v<GRange, GraphicsInstanceRange>;
    // constexpr u32 rowCount = 2 + hasContextRow;
    const f32 rowHeight = 24.f;
    // const f32 rowGap = 4.f;
    const f32 labelWidth = 64.f;
    // const f32 plotHeight = rowCount * (rowHeight + rowGap);

    ov->BeginPanel(LayoutPanelParameters{.FillColor = st[OverlayColor_ScrollAreaBorders],
                                         .Direction = LayoutDirection_TopToBottom,
                                         .Alignment = {Alignment_Center, Alignment_Top},
                                         .Sizing = {LayoutSizing::Grow(), LayoutSizing::Fit()},
                                         .Padding = st[OverlayStyle_ContentAreaPadding],
                                         .ChildGap = st[OverlayStyle_ChildGap]});

    ov->TextRaw("Memory ranges");

    // a row is: [label] [bar area with normalized panels inside]
    const auto drawRow = [&](const char *label) {
        ov->BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight,
                                             .Alignment = {Alignment_Left, Alignment_Center},
                                             .Sizing = {LayoutSizing::Grow(), LayoutSizing::Absolute(rowHeight)}});

        ov->BeginPanel(LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                             .Sizing = {LayoutSizing::Absolute(labelWidth), LayoutSizing::Grow()}});
        ov->TextRaw(label);
        ov->EndPanel();

        // bar container - ranges go inside as normalized-offset panels
        ov->BeginPanel(LayoutPanelParameters{.FillColor = Color::FromHexadecimal(0x1A1A1AFF),
                                             .Alignment = {Alignment_Left, Alignment_Top},
                                             .Sizing = LayoutSizing::Grow()});
        ov->PushId(label);
    };

    const auto endRow = [&]() {
        ov->PopId();
        ov->EndPanel(); // bar container
        ov->EndPanel(); // row
    };

    const auto drawBlock = [&](const usz offset, const usz size, const u32 colorIdx, const char *tooltipLabel,
                               const Resource meshHandle = NullHandle, const RenderMode rmode = RenderMode_None) {
        const f32 gap = 0.003f;
        const f32 normOffset = f32(offset) * invMax + 0.5f * gap;
        const f32 normSize = Math::Max(f32(size) * invMax - gap, 0.001f);

        const LayoutId id = ov->IdFromStack(offset);
        ov->Panel(
            id, LayoutPanelParameters{.FillColor = colors[colorIdx],
                                      .Sizing = LayoutSizing::Normalized({normSize, 1.f}),
                                      .SelfOffset = {LayoutOffset::Normalized(normOffset), LayoutOffset::Absolute(0.f)},
                                      .Floating = {.Enable = true,
                                                   .DrawOnTop = false,
                                                   .Clip = true,
                                                   .Attachment = {Alignment_Left, Alignment_Top},
                                                   .Alignment = {Alignment_Left, Alignment_Top}}});

        if (rmode != RenderMode_None)
            ov->SetItemTooltip(OverlayFocusFlag_ShortDelay, "{} - Offset: {} - Size: {} - Mesh: {:#010x} - Mode: {}",
                               tooltipLabel, fmtb(offset), fmts(size), meshHandle, ToString(rmode));
        else
            ov->SetItemTooltip(OverlayFocusFlag_ShortDelay, "{} - Offset: {} - Size: {}", tooltipLabel, fmtb(offset),
                               fmts(size));
    };

    // transfer row
    drawRow("Transfer");
    for (const TRange &trange : tpool.Ranges)
        drawBlock(trange.Offset, trange.Size, trange.Tracker.InUse() ? 1 : 0, status[trange.Tracker.InUse() ? 1 : 0]);
    endRow();

    // graphics row
    drawRow("Graphics");
    for (const GRange &range : gpool.Ranges)
    {
        if constexpr (hasContextRow)
        {
            const u32 idx =
                range.InUse()
                    ? 1
                    : (rdata.AreAllContextRangesDirty(range) ? 0 : (rdata.AreAllContextRangesClean(range) ? 2 : 4));
            drawBlock(range.Offset, range.Size, idx, status[idx], range.MeshHandle,
                      range.RenderFlags != 0 ? GetRenderMode(range.RenderFlags) : RenderMode_None);
        }
        else
        {
            const u32 idx = range.InUse() ? 1 : (range.Generation == generation ? 2 : 0);
            drawBlock(range.Offset, range.Size, idx, status[idx]);
        }
    }
    endRow();

    // context row (only for instance ranges)
    if constexpr (hasContextRow)
    {
        drawRow("Context");
        for (const GRange &range : gpool.Ranges)
            for (const ContextInstanceRange &crange : range.ContextRanges)
            {
                const u32 idx = rdata.IsContextRangeClean(crange) ? 2 : 3;
                drawBlock(range.Offset + crange.Offset, crange.Size, idx, status[idx]);
            }
        endRow();
    }

    // axis labels
    ov->BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight,
                                         .Alignment = {Alignment_Left, Alignment_Center},
                                         .Sizing = {LayoutSizing::Grow(), LayoutSizing::Fit()}});

    ov->Panel(LayoutPanelParameters{.Sizing = {LayoutSizing::Absolute(labelWidth), LayoutSizing::Fit()}});
    ov->TextRaw("0 b");
    ov->Panel(LayoutPanelParameters{.Sizing = LayoutSizing::Grow()});
    ov->TextRaw(fmts(maxSize));
    ov->EndPanel();

    // legend
    ov->BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight,
                                         .Alignment = {Alignment_Center, Alignment_Center},
                                         .Sizing = {LayoutSizing::Grow(), LayoutSizing::Fit()},
                                         .ChildGap = st[OverlayStyle_ChildGap]});

    const u32 legendCount = hasContextRow ? 5 : 3;
    for (u32 i = 0; i < legendCount; ++i)
    {
        ov->BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight,
                                             .Alignment = {Alignment_Left, Alignment_Center},
                                             .Sizing = LayoutSizing::Fit(),
                                             .ChildGap = 4.f});

        ov->Panel(LayoutPanelParameters{.FillColor = colors[i], .Sizing = LayoutSizing::Absolute(12.f)});
        ov->TextRaw(status[i]);
        ov->EndPanel();
    }
    ov->EndPanel();

    ov->EndPanel(); // outer container
}

template <Dimension D> void Renderer_DisplayMemoryLayout(Overlay *ov)
{
    const RendererData<D> &rdata = getRendererData<D>();
    const LightData<D> &ldata = rdata.Lights;
    ov->PushId(&rdata);
    if (ov->Button("Coalesce##Button"))
        coalesce<D>(512);

    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        const InstanceArena &arena = rdata.Geometry.Arenas[geo];
        if (ov->PushTree({&arena, ToString(geo)}, s_DrawLines))
        {
            displayRanges<D>(ov, "Transfer", arena.Transfer);
            displayRanges<D>(ov, "Graphics", arena.Graphics);
            plotRanges<D>(ov, arena.Transfer, arena.Graphics);
            ov->PopTree();
        }
    }
    for (u32 i = 0; i < LightTypeCount<D>; ++i)
    {
        const LightType light = LightType(i);
        const LightArena &arena = ldata.Arenas[light];
        if (ov->PushTree({&arena, ToString(light)}, s_DrawLines))
        {
            displayRanges<D>(ov, "Transfer", arena.Transfer);
            displayRanges<D>(ov, "Graphics", arena.Graphics, arena.ActiveGeneration);
            plotRanges<D>(ov, arena.Transfer, arena.Graphics, arena.ActiveGeneration);
            ov->PopTree();
        }
    }
    if (ov->PushTree("Vertex buffer", s_DrawLines))
    {
        const Arena &arena = rdata.Geometry.VertexArena;
        displayRanges<D>(ov, "Transfer", arena.Transfer);
        displayRanges<D>(ov, "Graphics", arena.Graphics);
        plotRanges<D>(ov, arena.Transfer, arena.Graphics);
        ov->PopTree();
    }
    if (ov->PushTree("Index buffer", s_DrawLines))
    {
        const Arena &arena = rdata.Geometry.IndexArena;
        displayRanges<D>(ov, "Transfer", arena.Transfer);
        displayRanges<D>(ov, "Graphics", arena.Graphics);
        plotRanges<D>(ov, arena.Transfer, arena.Graphics);
        ov->PopTree();
    }
    ov->PopId();
}

template void Renderer_DisplayMemoryLayout<D2>(Overlay *ov);
template void Renderer_DisplayMemoryLayout<D3>(Overlay *ov);

template RenderContext<D2> *Renderer_CreateContext(u32 immediateDynamicMeshCapacity);
template RenderContext<D3> *Renderer_CreateContext(u32 immediateDynamicMeshCapacity);

template void Renderer_DestroyContext(const RenderContext<D2> *context);
template void Renderer_DestroyContext(const RenderContext<D3> *context);

template void Renderer_BindBuffer<D2>(u32 binding, const Graph::DescriptorBufferInfo &info, RenderPass pass,
                                      u32 dstElement);
template void Renderer_BindBuffer<D3>(u32 binding, const Graph::DescriptorBufferInfo &info, RenderPass pass,
                                      u32 dstElement);

template void Renderer_BindImage<D2>(u32 binding, const Graph::DescriptorImageInfo &info, RenderPass pass,
                                     u32 dstElement);
template void Renderer_BindImage<D3>(u32 binding, const Graph::DescriptorImageInfo &info, RenderPass pass,
                                     u32 dstElement);

template void Renderer_BindSampler<D2>(u32 binding, Graph::Sampler sampler, RenderPass pass, u32 dstElement);
template void Renderer_BindSampler<D3>(u32 binding, Graph::Sampler sampler, RenderPass pass, u32 dstElement);

} // namespace Onyx
