#include "pch.hpp"
#include "onyx/context.hpp"
#include "onyx/vertex.hpp"
#include "core.hpp"
#include "instance.hpp"
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
struct InstanceRange
{
    usz Offset = 0;
    usz Size = 0;

    // only relevant for dynamic mesh instance data
    u32 VertexOffset = 0;
    u32 FirstIndex = 0;
    u32 IndexCount = 0;
    //

    ViewMask ViewMask = 0;

    Resource MeshHandle = NullHandle;
    RenderModeFlags RenderFlags = 0;
    BlendPass Blend = BlendPass_None;
};

struct InstanceDeviceBuffer
{
    Graph::Buffer Buffer = Graph::NullHandle;
    TKit::TierArray<InstanceRange> Ranges{};
};

struct TextureMapPool
{
    TKit::ArenaArray<Graph::Image> Maps{};
    u32 Cursor = 0;

    void Destroy() const
    {
        for (const Graph::Image img : Maps)
            Graph::Image_Destroy(img);
    }
};

template <Dimension D> struct LightResources;
template <> struct LightResources<D2>
{
    // the instance buffers holding the light's instance data
    TKit::FixedArray<Graph::Buffer, LightTypeCount<D2>> Buffers{};
    //
    TextureMapPool ShadowMaps{};
    TextureMapPool OcclusionMaps{};

    Graph::DescriptorSet RayMarchDescriptor = Graph::NullHandle;

    void Reset()
    {
        ShadowMaps.Cursor = 0;
        OcclusionMaps.Cursor = 0;
    }

    void Destroy() const
    {
        for (u32 i = 0; i < LightTypeCount<D2>; ++i)
            if (Buffers[i] != Graph::NullHandle)
                Graph::Buffer_Destroy(Buffers[i]);
        ShadowMaps.Destroy();
        OcclusionMaps.Destroy();
    }

    Graph::Image GetShadowMap(const LightType, const u32 idx) const
    {
        return ShadowMaps.Maps[idx];
    }
};

template <> struct LightResources<D3>
{
    // the instance buffer holding the light's instance data
    TKit::FixedArray<Graph::Buffer, LightTypeCount<D3>> Buffers{};
    //
    TKit::FixedArray<TextureMapPool, LightTypeCount<D3>> ShadowMaps{};

    void Reset()
    {
        for (TextureMapPool &pool : ShadowMaps)
            pool.Cursor = 0;
    }

    void Destroy() const
    {
        for (u32 i = 0; i < LightTypeCount<D3>; ++i)
        {
            if (Buffers[i] != Graph::NullHandle)
                Graph::Buffer_Destroy(Buffers[i]);
            ShadowMaps[i].Destroy();
        }
    }
    Graph::Image GetShadowMap(const LightType ltype, const u32 idx) const
    {
        return ShadowMaps[ltype].Maps[idx];
    }
};

struct DrawCommandBuffer
{
    Graph::Buffer Buffer = Graph::NullHandle;
    usz Offset = 0;
};

template <Dimension D> struct ResourceGroup
{
    TKit::FixedArray<InstanceDeviceBuffer, Geometry_Count> InstanceBuffers{};
    LightResources<D> LightData{};

    ten<Graph::DescriptorSet, RenderPass_Count, Geometry_Count> Descriptors{};

    Graph::Buffer VertexBuffer = Graph::NullHandle;
    Graph::Buffer IndexBuffer = Graph::NullHandle;

    DrawCommandBuffer DrawBuffer{};
    DrawCommandBuffer IndexedDrawBuffer{};

    Graph::Tracker Tracker{0, Graph::Queue_Graphics};
};

template <typename LightParams> struct ContextLights
{
    TKit::TierArray<LightParams> Parameters{};
    TKit::TierArray<typename LightParams::InstanceData> Data{};

    void Clear()
    {
        Parameters.Clear();
        Data.Clear();
    }
};

template <Dimension D> struct LightData
{
    ContextLights<PointLightParameters<D>> Points{};
    ContextLights<DirectionalLightParameters<D>> Directionals{};

    void Clear()
    {
        Points.Clear();
        Directionals.Clear();
    }
};

template <> struct LightData<D3>
{
    ContextLights<PointLightParameters<D3>> Points{};
    ContextLights<DirectionalLightParameters<D3>> Directionals{};
    ContextLights<SpotLightParameters> Spots{};

    void Clear()
    {
        Points.Clear();
        Directionals.Clear();
        Spots.Clear();
    }
};

template <Dimension D> struct ShadowData;

template <> struct ShadowData<D2>
{
    ten<Graph::Pipeline, Geometry_Count> Pipelines{}; // occlusion pipelines
    Graph::Format ShadowFormat = Graph::Format_Undefined;

    u32 ShadowResolution;
    u32 OcclusionResolution;

    u32 GetShadowResolution(const u32) const
    {
        return ShadowResolution;
    }

    static constexpr Graph::Format OcclusionFormat = Graph::Format_R8_UNORM;
    static constexpr Graph::Format PrimaryShadowFormat =
        Graph::Format_D16_UNORM; // not usually supported when used as 1D
                                 // storage. thats why a fallback is provided
    static constexpr Graph::Format FallbackShadowFormat = Graph::Format_R16_UNORM;
};

using ShadowMapId = u32;

template <> struct ShadowData<D3>
{
    ten<Graph::Pipeline, Geometry_Count> Pipelines{};
    TKit::FixedArray<u32, LightTypeCount<D3>> ShadowResolutions;

    u32 GetShadowResolution(const u32 ltype) const
    {
        return ShadowResolutions[ltype];
    }

    static constexpr Graph::Format ShadowFormat = Graph::Format_D32_SFLOAT;
};

template <Dimension D> struct RendererData
{
    TKit::TierArray<RenderContext<D> *> Contexts{};
    TKit::ArenaArray<ResourceGroup<D>> ResourceGroups{};
    ten<Graph::Pipeline, BlendPass_Count, PipelinePass_Count, Geometry_Count> Pipelines{};
    ResourceGroup<D> *ActiveResourceGroup = nullptr;

    LightData<D> Lights{};
    ShadowData<D> Shadows{};
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

struct LayoutData
{
    ten<Graph::PipelineLayout, D_Count, RenderPass_Count> Layouts{};
    ten<ShaderData, D_Count, RenderPass_Count> Shaders{};

    Graph::Shader FullPassVertexShader = Graph::NullHandle;
    TKit::FixedArray<StandalonePipelineData, StandalonePass_Count> Standalone{};
};

static TKit::Storage<LayoutData> s_LayoutData{};

static TKit::Storage<RendererData<D2>> s_RendererData2{};
static TKit::Storage<RendererData<D3>> s_RendererData3{};
static TKit::FixedArray<Graph::Pipeline, StandalonePass_Count> s_StandalonePipelines{};

static Graph::Sampler s_LinearSampler = Graph::NullHandle;
static Graph::Sampler s_EdgeSampler = Graph::NullHandle;
static Graph::Sampler s_CompareSampler = Graph::NullHandle;
static Graph::Sampler s_NearSampler = Graph::NullHandle;

template <Dimension D> static RendererData<D> &getRendererData()
{
    if constexpr (D == D2)
        return *s_RendererData2;
    else
        return *s_RendererData3;
}

static bool isDepth2DSupported()
{
    return s_RendererData2->Shadows.ShadowFormat == ShadowData<D2>::PrimaryShadowFormat;
}

template <Dimension D> static ShaderData &getShaders(const RenderPass pass)
{
    return s_LayoutData->Shaders[D - 2][pass];
}

template <Dimension D> static Graph::PipelineLayout getPipelineLayout(const RenderPass pass)
{
    return s_LayoutData->Layouts[D - 2][pass];
}
Graph::PipelineLayout getPipelineLayout(const StandalonePass pass)
{
    return s_LayoutData->Standalone[pass].Layout;
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

    s_LayoutData->Layouts[Dim2][RenderPass_Shaded] =
        create(GetDescriptorLayout<D2>(RenderPass_Shaded), vertex | fragment, sizeof(ShadedPushConstantData<D2>));

    s_LayoutData->Layouts[Dim3][RenderPass_Shaded] =
        create(GetDescriptorLayout<D3>(RenderPass_Shaded), vertex | fragment, sizeof(ShadedPushConstantData<D3>));

    s_LayoutData->Layouts[Dim2][RenderPass_Flat] =
        create(GetDescriptorLayout<D2>(RenderPass_Flat), vertex, sizeof(FlatPushConstantData));

    s_LayoutData->Layouts[Dim3][RenderPass_Flat] =
        create(GetDescriptorLayout<D3>(RenderPass_Flat), vertex, sizeof(FlatPushConstantData));

    s_LayoutData->Layouts[Dim2][RenderPass_Shadow] =
        create(GetDescriptorLayout<D2>(RenderPass_Shadow), vertex, sizeof(ShadowPushConstantData<D2>));

    s_LayoutData->Layouts[Dim3][RenderPass_Shadow] =
        create(GetDescriptorLayout<D3>(RenderPass_Shadow), vertex | fragment, sizeof(ShadowPushConstantData<D3>));

    s_LayoutData->Standalone[StandalonePass_RayMarch].Layout =
        create(GetDescriptorLayout(StandalonePass_RayMarch), compute, sizeof(RayMarchPushConstantData));

    s_LayoutData->Standalone[StandalonePass_Blend].Layout =
        create(GetDescriptorLayout(StandalonePass_Blend), fragment, sizeof(BlendPushConstantData));

    s_LayoutData->Standalone[StandalonePass_Outline].Layout =
        create(GetDescriptorLayout(StandalonePass_Outline), fragment, sizeof(OutlinePushConstantData));

    s_LayoutData->Standalone[StandalonePass_FXAA].Layout =
        create(GetDescriptorLayout(StandalonePass_FXAA), fragment, sizeof(FXAAPushConstantData));

    s_LayoutData->Standalone[StandalonePass_Compositor].Layout =
        create(GetDescriptorLayout(StandalonePass_Compositor), fragment, sizeof(CompositorPushConstantData));

    if (Graph::IsValidationEnabled())
    {
        s_LayoutData->Layouts.IterateMultiIndex([&](const u32 i, const u32 j) {
            Graph::PipelineLayout_SetName(
                s_LayoutData->Layouts[i][j],
                TKit::StackString::Format("onyx-pipeline-layout-{}D-{}", i + 2, ToString(RenderPass(j))).CString());
        });
        Graph::PipelineLayout_SetName(s_LayoutData->Standalone[StandalonePass_RayMarch].Layout,
                                      "onyx-ray-march-pipeline-layout");
        Graph::PipelineLayout_SetName(s_LayoutData->Standalone[StandalonePass_Blend].Layout,
                                      "onyx-blend-pipeline-layout");
        Graph::PipelineLayout_SetName(s_LayoutData->Standalone[StandalonePass_Outline].Layout,
                                      "onyx-outline-pipeline-layout");
        Graph::PipelineLayout_SetName(s_LayoutData->Standalone[StandalonePass_Compositor].Layout,
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

    s_LayoutData->FullPassVertexShader = Graph::Compilation_CreateShader(cmp, "mainVS", "full-vertex");
    s_LayoutData->Standalone[StandalonePass_RayMarch].Shader =
        Graph::Compilation_CreateShader(cmp, "main", "ray-march");
    s_LayoutData->Standalone[StandalonePass_Blend].Shader = Graph::Compilation_CreateShader(cmp, "mainFS", "blend");
    s_LayoutData->Standalone[StandalonePass_Outline].Shader =
        Graph::Compilation_CreateShader(cmp, "mainFS", "post-process");
    s_LayoutData->Standalone[StandalonePass_Compositor].Shader =
        Graph::Compilation_CreateShader(cmp, "mainFS", "compositor");

    Graph::Compilation_Destroy(cmp);
#else
    for (u32 dim = 0; dim < D_Count; ++dim)
        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            for (u32 rpass = 0; rpass < RenderPass_Count; ++rpass)
            {
                ShaderData &sdata = s_LayoutData->Shaders[dim][rpass];
                sdata.VertexShaders[geo] = shaderFromBinary(g_ShaderBinaryData.VertexShaders[dim][rpass][geo]);
                sdata.OpaqueFragmentShaders[geo] =
                    shaderFromBinary(g_ShaderBinaryData.OpaqueFragmentShaders[dim][rpass][geo]);
                if (rpass != RenderPass_Shadow)
                    sdata.TransparentFragmentShaders[geo] =
                        shaderFromBinary(g_ShaderBinaryData.TransparentFragmentShaders[dim][rpass][geo]);
            }

    s_LayoutData->FullPassVertexShader = shaderFromBinary(g_ShaderBinaryData.FullVertex);

    s_LayoutData->Standalone[StandalonePass_RayMarch].Shader = shaderFromBinary(g_ShaderBinaryData.RayMarch);
    s_LayoutData->Standalone[StandalonePass_Blend].Shader = shaderFromBinary(g_ShaderBinaryData.Blend);
    s_LayoutData->Standalone[StandalonePass_Outline].Shader = shaderFromBinary(g_ShaderBinaryData.Outline);
    s_LayoutData->Standalone[StandalonePass_FXAA].Shader = shaderFromBinary(g_ShaderBinaryData.Fxaa);
    s_LayoutData->Standalone[StandalonePass_Compositor].Shader = shaderFromBinary(g_ShaderBinaryData.Compositor);
#endif
}

static void destroyShaders()
{
    for (auto &sdata : s_LayoutData->Shaders)
        sdata.Destroy();

    Graph::Shader_Destroy(s_LayoutData->FullPassVertexShader);
    for (const auto &st : s_LayoutData->Standalone)
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

    const Graph::Format cf = GetAttachmentFormat(AttachmentKind_SrgbColor);
    const Graph::Format tf = GetAttachmentFormat(AttachmentKind_Accumulation);
    const Graph::Format rf = GetAttachmentFormat(AttachmentKind_Revealage);

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
    specs.DepthFormat = GetAttachmentFormat(AttachmentKind_DepthStencil);
    specs.StencilFormat = specs.DepthFormat;
    specs.Multisample.Samples = GetSpecs().SampleCount;

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

static Graph::Pipeline createStandalonePassPipeline(const StandalonePass pass)
{
    const auto createFullPassPipeline = [pass](const Graph::ColorAttachment &attachment) {
        const StandalonePipelineData &data = s_LayoutData->Standalone[pass];

        Graph::ShaderStageInfo stages[2]{};
        stages[0].Shader = s_LayoutData->FullPassVertexShader;
        stages[0].Stage = Graph::ShaderStageFlag_Vertex;
        stages[1].Shader = data.Shader;
        stages[1].Stage = Graph::ShaderStageFlag_Fragment;

        Graph::GraphicsPipelineSpecs specs{};
        specs.ShaderStages = stages;
        specs.ColorAttachments = attachment;
        specs.DepthStencil.DepthTestEnabled = false;
        specs.DepthStencil.DepthWriteEnabled = false;
        return Graph::Pipeline_CreateGraphics(data.Layout, specs);
    };

    switch (pass)
    {
    case StandalonePass_RayMarch: {
        const StandalonePipelineData &data = s_LayoutData->Standalone[StandalonePass_RayMarch];
        Graph::ShaderStageInfo stage{};
        stage.Shader = data.Shader;
        stage.Stage = Graph::ShaderStageFlag_Compute;
        return Graph::Pipeline_CreateCompute(data.Layout, stage);
    }
    case StandalonePass_Blend: {
        Graph::ColorAttachment attachment{};
        attachment.BlendEnabled = true;
        attachment.SrcColor = Graph::BlendFactor_SrcAlpha;
        attachment.DstColor = Graph::BlendFactor_OneMinusSrcAlpha;
        // NOTE(Isma, 25/06/06): Should consider not writing to the alpha channel. Causes alpha values to be
        // inherited. For now it actually lets us having modals in ui, so ill keep it. but its a bit
        // counterintuitive
        attachment.SrcAlpha = Graph::BlendFactor_One;
        attachment.DstAlpha = Graph::BlendFactor_OneMinusSrcAlpha;
        attachment.Format = GetAttachmentFormat(AttachmentKind_SrgbColor);
        return createFullPassPipeline(attachment);
    }
    case StandalonePass_Outline:
        return createFullPassPipeline({.Format = GetAttachmentFormat(AttachmentKind_SrgbColor)});
    case StandalonePass_FXAA:
        return createFullPassPipeline({.Format = GetAttachmentFormat(AttachmentKind_LinearColor)});
    case StandalonePass_Compositor: {
        Graph::ColorAttachment attachment{};
        attachment.BlendEnabled = true;
        attachment.Format = GetAttachmentFormat(AttachmentKind_LinearColor);
        return createFullPassPipeline(attachment);
    }
    default:
        return Graph::NullHandle;
    }
}

static u32 lightToBinding(const LightType light)
{
    switch (light)
    {
    case Light_Point:
        return ONYX_GEOMETRY_PASS_POINT_LIGHTS_BINDING_POINT;
    case Light_Directional:
        return ONYX_GEOMETRY_PASS_DIRECTIONAL_LIGHTS_BINDING_POINT;
    case Light_Spot:
        return ONYX_3D_GEOMETRY_PASS_SPOT_LIGHTS_BINDING_POINT;
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized light type: {}", u8(light));
        return 0;
    }
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

template <Dimension D> static void updateInstanceDescriptorSets(const ResourceGroup<D> &group, const Geometry geo)
{
    if (group.InstanceBuffers[geo].Buffer == Graph::NullHandle)
        return;
    const Graph::DescriptorBufferInfo info = Graph::DescriptorBufferInfo::Create(group.InstanceBuffers[geo].Buffer);
    for (u32 i = 0; i < RenderPass_Count; ++i)
        Graph::DescriptorSet_Write(group.Descriptors[i][geo], ONYX_GEOMETRY_PASS_INSTANCES_BINDING_POINT, info);
}

template <Dimension D> static void updateLightDescriptorSets(const ResourceGroup<D> &group, const LightType light)
{
    if (group.LightData.Buffers[light] == Graph::NullHandle)
        return;
    const Graph::DescriptorBufferInfo info = Graph::DescriptorBufferInfo::Create(group.LightData.Buffers[light]);
    for (const Graph::DescriptorSet set : group.Descriptors[RenderPass_Shaded])
        Graph::DescriptorSet_Write(set, lightToBinding(light), info);
}

template <typename... Args>
static Graph::Buffer createBuffer(const usz size, const Graph::BufferFlags flags, const fmt::format_string<Args...> str,
                                  Args &&...args)
{
    const Graph::Buffer buffer = Graph::Buffer_Create(size, flags);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format(str, std::forward<Args>(args)...);
        Graph::Buffer_SetName(buffer, name.CString());
    }
    return buffer;
}

static constexpr Graph::BufferFlags s_InstanceBufferFlags = Graph::BufferFlag_Storage | Graph::BufferFlag_HostMapped;
static constexpr Graph::BufferFlags s_LightBufferFlags = Graph::BufferFlag_Storage | Graph::BufferFlag_HostMapped;
static constexpr Graph::BufferFlags s_VertexBufferFlags = Graph::BufferFlag_Vertex | Graph::BufferFlag_HostMapped;
static constexpr Graph::BufferFlags s_IndexBufferFlags = Graph::BufferFlag_Index | Graph::BufferFlag_HostMapped;

template <Dimension D> static Graph::Buffer createInstanceBuffer(const Geometry geo, const u32 instances)
{
    if (instances == 0)
        return Graph::NullHandle;
    return createBuffer(instances * GetInstanceSize<D>(geo), s_InstanceBufferFlags,
                        "onyx-renderer-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
}

template <Dimension D> static Graph::Buffer createLightBuffer(const LightType light, const u32 instances)
{
    if (instances == 0)
        return Graph::NullHandle;
    return createBuffer(instances * getLightSize<D>(light), s_LightBufferFlags,
                        "onyx-renderer-light-buffer-{}D-type-{}", u8(D), ToString(light));
}

template <Dimension D> static Graph::Buffer createVertexBuffer(const u32 instances)
{
    if (instances == 0)
        return Graph::NullHandle;
    return createBuffer(instances * sizeof(DynamicVertex<D>), s_VertexBufferFlags, "onyx-renderer-vertex-buffer-{}D",
                        u8(D));
}

template <Dimension D> static Graph::Buffer createIndexBuffer(const u32 instances)
{
    if (instances == 0)
        return Graph::NullHandle;
    return createBuffer(instances * sizeof(Index), s_IndexBufferFlags, "onyx-renderer-index-buffer-{}D", u8(D));
}

template <Dimension D> static void createPipelines()
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    rdata.Pipelines.IterateMultiIndex([&](const u32 bpass, const u32 ppass, const u32 geo) {
        rdata.Pipelines[bpass][ppass][geo] =
            createGeometryPipeline<D>(PipelinePass(ppass), BlendPass(bpass), Geometry(geo));

        if (Graph::IsValidationEnabled())
        {
            const TKit::StackString name = TKit::StackString::Format(
                "onyx-renderer-geometry-pipeline-{}D-{}-pass-{}-geometry-'{}'", u8(D), ToString(BlendPass(bpass)),
                ToString(PipelinePass(ppass)), ToString(Geometry(geo)));

            Graph::Pipeline_SetName(rdata.Pipelines[bpass][ppass][geo], name.CString());
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
}

static void createPipelines()
{
    for (u32 i = 0; i < StandalonePass_Count; ++i)
        s_StandalonePipelines[i] = createStandalonePassPipeline(StandalonePass(i));

    if (Graph::IsValidationEnabled())
    {
        Graph::Pipeline_SetName(s_StandalonePipelines[StandalonePass_RayMarch], "onyx-blend-ray-march");
        Graph::Pipeline_SetName(s_StandalonePipelines[StandalonePass_Blend], "onyx-blend-pipeline");
        Graph::Pipeline_SetName(s_StandalonePipelines[StandalonePass_Outline], "onyx-outline-pipeline");
        Graph::Pipeline_SetName(s_StandalonePipelines[StandalonePass_FXAA], "onyx-fxaa-pipeline");
        Graph::Pipeline_SetName(s_StandalonePipelines[StandalonePass_Compositor], "onyx-compositor-pipeline");
    }

    createPipelines<D2>();
    createPipelines<D3>();
}

template <Dimension D> static void destroyPipelines()
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;
    for (const Graph::Pipeline p : rdata.Pipelines)
        Graph::Pipeline_Destroy(p);
    for (const Graph::Pipeline p : sdata.Pipelines)
        Graph::Pipeline_Destroy(p);
}

static void destroyPipelines()
{
    destroyPipelines<D2>();
    destroyPipelines<D3>();
    for (const Graph::Pipeline pip : s_StandalonePipelines)
        Graph::Pipeline_Destroy(pip);
}

template <Dimension D> static void createLightResources(ResourceGroup<D> &group)
{
    LightResources<D> &lres = group.LightData;
    const SpecificSpecs<D> &specs = GetSpecs().GetSpecific<D>();
    for (u32 i = 0; i < LightTypeCount<D>; ++i)
    {
        const LightType ltype = LightType(i);
        lres.Buffers[ltype] = createLightBuffer<D>(ltype, specs.MaxLightInstances);
        updateLightDescriptorSets(group, ltype);
    }

    if constexpr (D == D2)
    {
        ShadowData<D2> &sdata = getRendererData<D2>().Shadows;
        LightResources<D2> &l2 = lres;
        const Graph::DescriptorSet set = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_RayMarch));
        if (Graph::IsValidationEnabled())
            Graph::DescriptorSet_SetName(set, "onyx-renderer-descriptor-set-ray-march");

        l2.RayMarchDescriptor = set;
        const Graph::ImageFlags flags = Graph::ImageFlag_Sampled | Graph::ImageFlag_Storage |
                                        (isDepth2DSupported() ? Graph::ImageFlag_Depth : Graph::ImageFlag_Color);

        Graph::ImageSpecs ispecs{};
        ispecs.Type = Graph::ImageType_1D;
        ispecs.Formats = sdata.ShadowFormat;

        TKIT_ASSERT(specs.MaxShadowMaps + specs.MaxOcclusionMaps <= ONYX_2D_MAX_RAY_MARCH_AND_OCCLUSION_MAPS,
                    "[ONYX][RENDERER] Max shadow and occlusion map entries must not exceed together more than "
                    "ONYX_2D_MAX_RAY_MARCH_AND_OCCLUSION_MAPS = {}, but {} + {} = {}. Decrease the 2D values of "
                    ".MaxShadowMaps and .MaxOcclusionMaps fields given to Onyx::Initialize",
                    ONYX_2D_MAX_RAY_MARCH_AND_OCCLUSION_MAPS, specs.MaxShadowMaps, specs.MaxOcclusionMaps,
                    specs.MaxShadowMaps + specs.MaxOcclusionMaps);

        u32 maxMaps = specs.MaxShadowMaps;
        l2.ShadowMaps.Maps.Reserve(maxMaps);
        for (u32 i = 0; i < maxMaps; ++i)
        {
            const Graph::Image img =
                l2.ShadowMaps.Maps.Append(Graph::Image_Create(u32v2{sdata.ShadowResolution, 1}, ispecs, flags));
            Graph::Image_AddView(img);
            if (Graph::IsValidationEnabled())
            {
                Graph::Image_SetName(img, TKit::StackString::Format("onyx-texture-map-2D-{}", i).CString());
                Graph::Image_SetViewNames(img, TKit::StackString::Format("onyx-texture-map-view-2D-{}", i).CString());
            }
            Graph::DescriptorImageInfo info =
                Graph::DescriptorImageInfo::Create(Graph::Image_GetView(img), Graph::ImageLayout_ShaderReadOnly);

            writeDescriptor<D>(group, ONYX_2D_GEOMETRY_PASS_SHADOW_MAPS_BINDING_POINT, info, RenderPass_Shaded, i);

            info.Layout = Graph::ImageLayout_General;
            Graph::DescriptorSet_Write(l2.RayMarchDescriptor, ONYX_2D_RAY_MARCH_PASS_RAY_MARCH_MAP_BINDING_POINT, info,
                                       i);
        }

        maxMaps = specs.MaxOcclusionMaps;
        l2.OcclusionMaps.Maps.Reserve(maxMaps);
        ispecs.Type = Graph::ImageType_2D;
        ispecs.Formats = sdata.OcclusionFormat;
        for (u32 i = 0; i < maxMaps; ++i)
        {
            const Graph::Image img = l2.OcclusionMaps.Maps.Append(Graph::Image_Create(
                u32v2{sdata.OcclusionResolution}, ispecs, Graph::ImageFlag_ColorAttachment | Graph::ImageFlag_Storage));

            Graph::Image_AddView(img);
            if (Graph::IsValidationEnabled())
            {
                Graph::Image_SetName(img, TKit::StackString::Format("onyx-occlusion-map-{}", i).CString());
                Graph::Image_SetViewNames(img, TKit::StackString::Format("onyx-occlusion-map-view-{}", i).CString());
            }

            const Graph::DescriptorImageInfo info =
                Graph::DescriptorImageInfo::Create(Graph::Image_GetView(img), Graph::ImageLayout_General);
            Graph::DescriptorSet_Write(l2.RayMarchDescriptor, ONYX_2D_RAY_MARCH_PASS_OCCLUSION_MAP_BINDING_POINT, info,
                                       i);
        }
    }
    else
    {
        ShadowData<D3> &sdata = getRendererData<D3>().Shadows;
        LightResources<D3> &l3 = lres;

        constexpr TKit::FixedArray<u32, LightTypeCount<D3>> lcounts = {6, ONYX_3D_MAX_CASCADES, 0};
        for (u32 i = 0; i < LightTypeCount<D3>; ++i)
        {
            TKit::StackArray<Graph::ImageViewSpecs> views{};
            views.Reserve(Math::Max(6, ONYX_3D_MAX_CASCADES) + 1);

            Graph::ImageFlags flags = Graph::ImageFlag_Sampled | Graph::ImageFlag_DepthAttachment;
            const LightType ltype = LightType(i);
            const u32 lcount = lcounts[ltype];

            Graph::ImageSpecs ispecs{};
            ispecs.Formats = sdata.ShadowFormat;
            for (u32 j = 0; j < lcount; ++j)
            {
                Graph::ImageViewSpecs &layer = views.Append();
                layer.Range.MipStart = 0;
                layer.Range.MipCount = 1;
                layer.Range.LayerStart = j;
                layer.Range.LayerCount = 1;
                layer.Range.Aspect = Graph::ImageAspectFlag_Depth;
            }
            u32 binding;
            if (ltype == Light_Point)
            {
                flags |= Graph::ImageFlag_CubeCompatible;
                views.Append().Type = Graph::ImageViewType_Cube;
                binding = ONYX_3D_GEOMETRY_PASS_POINT_MAPS_BINDING_POINT;
                ispecs.ArrayLayers = lcount;
            }
            else if (ltype == Light_Directional)
            {
                views.Append().Type = Graph::ImageViewType_2DArray;
                binding = ONYX_3D_GEOMETRY_PASS_DIRECTIONAL_MAPS_BINDING_POINT;
                ispecs.ArrayLayers = lcount;
            }
            else
            {
                views.Append();
                binding = ONYX_3D_GEOMETRY_PASS_SPOT_MAPS_BINDING_POINT;
            }
            const u32 mainViewIndex = lcount;

            ispecs.ImageViews = views;

            const u32 maxMaps = specs.MaxShadowMaps[ltype];
            TKIT_ASSERT(maxMaps <= ONYX_MAX_TEXTURE_MAPS,
                        "[ONYX][RENDERER] Max shadow map entries must not exceed ONYX_MAX_TEXTURE_MAPS = {}, but is "
                        "{}. Decrease the 3D value of the .MaxShadowMaps[{}] field given to Onyx::Initialize",
                        ONYX_MAX_TEXTURE_MAPS, maxMaps, u32(ltype));

            l3.ShadowMaps[ltype].Maps.Reserve(maxMaps);
            for (u32 i = 0; i < maxMaps; ++i)
            {
                const Graph::Image img = l3.ShadowMaps[ltype].Maps.Append(
                    Graph::Image_Create(u32v2{sdata.ShadowResolutions[ltype]}, ispecs, flags));

                if (Graph::IsValidationEnabled())
                {
                    Graph::Image_SetName(
                        img, TKit::StackString::Format("onyx-texture-map-3D-'{}'-{}", ToString(ltype), i).CString());
                    Graph::Image_SetViewNames(
                        img,
                        TKit::StackString::Format("onyx-texture-map-view-3D-'{}'-{}", ToString(ltype), i).CString());
                }
                const Graph::DescriptorImageInfo info = Graph::DescriptorImageInfo::Create(
                    Graph::Image_GetView(img, mainViewIndex), Graph::ImageLayout_ShaderReadOnly);
                writeDescriptor<D>(group, binding, info, RenderPass_Shaded, i);
            }
        }
    }
}

template <Dimension D> static ResourceGroup<D> createResourceGroup()
{
    ResourceGroup<D> group;
    const SpecificSpecs<D> &specs = GetSpecs().GetSpecific<D>();
    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        group.InstanceBuffers[geo] = {.Buffer = createInstanceBuffer<D>(geo, specs.MaxInstances)};
        for (u32 j = 0; j < RenderPass_Count; ++j)
        {
            const RenderPass pass = RenderPass(j);
            const Graph::DescriptorSet set = Graph::DescriptorSet_Create(GetDescriptorLayout<D>(pass));
            if (Graph::IsValidationEnabled())
            {
                const TKit::StackString name = TKit::StackString::Format(
                    "onyx-renderer-descriptor-set-{}D-geometry-{}-pass-{}", u8(D), ToString(geo), ToString(pass));
                Graph::DescriptorSet_SetName(set, name.CString());
            }
            group.Descriptors[j][i] = set;
        }
        updateInstanceDescriptorSets(group, geo);
    }

    createLightResources(group);
    // TODO(Isma): Define maximum vertices/indices, not instances alone
    group.VertexBuffer = createVertexBuffer<D>(specs.MaxDynamicVertices);
    group.IndexBuffer = createIndexBuffer<D>(specs.MaxDynamicIndices);

    if (specs.MaxDrawCommands != 0)
    {
        group.DrawBuffer.Buffer = Graph::Buffer_Create<Graph::DrawIndirectCommand>(
            specs.MaxDrawCommands, Graph::BufferFlag_HostMapped | Graph::BufferFlag_Indirect);
        group.IndexedDrawBuffer.Buffer = Graph::Buffer_Create<Graph::DrawIndexedIndirectCommand>(
            specs.MaxDrawCommands, Graph::BufferFlag_HostMapped | Graph::BufferFlag_Indirect);
    }

    return group;
}

template <Dimension D> static void destroyResourceGroup(const ResourceGroup<D> &group)
{
    const auto destroy = [](const Graph::Buffer buff) {
        if (buff != Graph::NullHandle)
            Graph::Buffer_Destroy(buff);
    };

    for (const InstanceDeviceBuffer &buff : group.InstanceBuffers)
        destroy(buff.Buffer);
    group.LightData.Destroy();

    destroy(group.VertexBuffer);
    destroy(group.IndexBuffer);

    destroy(group.DrawBuffer.Buffer);
    destroy(group.IndexedDrawBuffer.Buffer);
}

template <Dimension D> static ResourceGroup<D> *findAvailableResourceGroup()
{
    RendererData<D> &rdata = getRendererData<D>();
    ResourceGroup<D> *pending = nullptr;

    const auto reset = [](ResourceGroup<D> &group) {
        for (InstanceDeviceBuffer &buff : group.InstanceBuffers)
            buff.Ranges.Clear();
        group.LightData.Reset();
        group.DrawBuffer.Offset = 0;
        group.IndexedDrawBuffer.Offset = 0;
    };

    for (ResourceGroup<D> &group : rdata.ResourceGroups)
        if (!group.Tracker.InUse())
        {
            reset(group);
            return &group;
        }
        else if (!pending || pending->Tracker.InFlightValue > group.Tracker.InFlightValue)
            pending = &group;

    TKIT_ASSERT(pending, "[ONYX][RENDERER] Failed to find an available resource group");
    TKIT_BEGIN_INFO_CLOCK();
    Graph::Queue_WaitForTracker(pending->Tracker);
    TKIT_END_INFO_CLOCK(Milliseconds, "[ONYX][RENDERER] Waited {:.2f} ms for a new resource group");
    reset(*pending);
    return pending;
}

template <Dimension D> static void initialize()
{
    const SpecificSpecs<D> &specs = GetSpecs().GetSpecific<D>();
    if constexpr (D == D2)
    {
        ShadowData<D2> &sdata = getRendererData<D2>().Shadows;
        sdata.ShadowResolution = specs.ShadowResolution;
        sdata.OcclusionResolution = specs.OcclusionResolution;

        if (Graph::Format_IsSupported(sdata.PrimaryShadowFormat, Graph::ImageType_1D,
                                      Graph::ImageFlag_Sampled | Graph::ImageFlag_Storage))
            sdata.ShadowFormat = ShadowData<D2>::PrimaryShadowFormat;
        else
            sdata.ShadowFormat = ShadowData<D2>::FallbackShadowFormat;
    }
    else
        getRendererData<D3>().Shadows.ShadowResolutions = specs.ShadowResolutions;

    TKIT_ASSERT(specs.ResourceGroupCount != 0,
                "[ONYX][RENDERER] Resource group count (.ResourceGroupCount field) must be at least 1");

    RendererData<D> &rdata = getRendererData<D>();
    rdata.ResourceGroups.Reserve(specs.ResourceGroupCount);
    for (u32 i = 0; i < specs.ResourceGroupCount; ++i)
        rdata.ResourceGroups.Append(createResourceGroup<D>());

    Renderer_WriteSamplerDescriptor<D>(ONYX_GEOMETRY_PASS_SHADOW_SAMPLER_BINDING_POINT, s_LinearSampler,
                                       RenderPass_Shaded);
    Renderer_WriteSamplerDescriptor<D>(ONYX_GEOMETRY_PASS_SHADOW_COMPARE_SAMPLER_BINDING_POINT, s_CompareSampler,
                                       RenderPass_Shaded);
}

template <Dimension D> static void terminate()
{
    RendererData<D> &rdata = getRendererData<D>();

    Graph::DeviceWaitIdle();
    for (const ResourceGroup<D> &group : rdata.ResourceGroups)
        destroyResourceGroup(group);

    TKit::TierAllocator *tier = TKit::GetTier();
    for (const RenderContext<D> *ctx : rdata.Contexts)
        tier->Destroy(ctx);
}

void Renderer_Initialize()
{
    TKIT_LOG_INFO("[ONYX][RENDERER] Initializing");
    s_RendererData2.Construct();
    s_RendererData3.Construct();
    s_LayoutData.Construct();

#ifndef ONYX_COMPILE_SHADERS_AT_RUNTIME
    InitializeBinaries();
#endif

    createPipelineLayouts();
    createShaders();

    Graph::SamplerSpecs linear{};
    linear.Wraps = Graph::Wrap_ClampToBorder;
    linear.Border = Graph::BorderColor_FloatOpaqueWhite;
    s_LinearSampler = Graph::Sampler_Create(linear);
    s_EdgeSampler = Graph::Sampler_Create();

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
        Graph::Sampler_SetName(s_EdgeSampler, "onyx-edge-sampler");
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

    Graph::Sampler_Destroy(s_LinearSampler);
    Graph::Sampler_Destroy(s_EdgeSampler);
    Graph::Sampler_Destroy(s_NearSampler);
    Graph::Sampler_Destroy(s_CompareSampler);

    destroyShaders();
    for (const auto layout : s_LayoutData->Layouts)
        Graph::PipelineLayout_Destroy(layout);

    for (const auto &st : s_LayoutData->Standalone)
        Graph::PipelineLayout_Destroy(st.Layout);

    s_RendererData2.Destruct();
    s_RendererData3.Destruct();
    s_LayoutData.Destruct();
}

template <Dimension D> RenderContext<D> *Renderer_CreateContext(const RenderContextSpecs &specs)
{
    RendererData<D> &rdata = getRendererData<D>();

    TKit::TierAllocator *tier = TKit::GetTier();
    return rdata.Contexts.Append(tier->Create<RenderContext<D>>(specs));
}

template <Dimension D> static u32 getContextIndex(const RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < rdata.Contexts.GetSize(); ++i)
        if (rdata.Contexts[i] == context)
            return i;
    TKIT_FATAL("[ONYX][RENDERER] Render context ({}) index not found", TKit::FormatPointer(context));
    return TKIT_U32_MAX;
}

template <Dimension D> void Renderer_DestroyContext(const RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u32 index = getContextIndex(context);

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(context);
    rdata.Contexts.RemoveOrdered(rdata.Contexts.begin() + index);
}

template <Dimension D> void flushAllContexts()
{
    RendererData<D> &rdata = getRendererData<D>();
    for (RenderContext<D> *ctx : rdata.Contexts)
        ctx->Flush();
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
    for (RenderContext<D> *ctx : rdata.Contexts)
        ctx->AddTarget(vmask);
}
template <Dimension D> static void removeTarget(const ViewMask vmask)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (RenderContext<D> *ctx : rdata.Contexts)
        ctx->RemoveTarget(vmask);
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

template <Dimension D, typename T>
void writeDescriptor(const u32 binding, const T &info, const RenderPass pass, const u32 dstElement)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const ResourceGroup<D> &group : rdata.ResourceGroups)
        for (const Graph::DescriptorSet set : group.Descriptors[pass])
            Graph::DescriptorSet_Write(set, binding, info, dstElement);
}
template <Dimension D, typename T>
void writeDescriptor(const ResourceGroup<D> &group, const u32 binding, const T &info, const RenderPass pass,
                     const u32 dstElement)
{
    for (const Graph::DescriptorSet set : group.Descriptors[pass])
        Graph::DescriptorSet_Write(set, binding, info, dstElement);
}

template <Dimension D>
void Renderer_WriteBufferDescriptor(const u32 binding, const Graph::DescriptorBufferInfo &info, const RenderPass pass,
                                    const u32 dstElement)
{
    writeDescriptor<D>(binding, info, pass, dstElement);
}
template <Dimension D>
void Renderer_WriteImageDescriptor(const u32 binding, const Graph::DescriptorImageInfo &info, const RenderPass pass,
                                   const u32 dstElement)
{
    writeDescriptor<D>(binding, info, pass, dstElement);
}
template <Dimension D>
void Renderer_WriteSamplerDescriptor(const u32 binding, const Graph::Sampler sampler, const RenderPass pass,
                                     const u32 dstElement)
{
    writeDescriptor<D>(binding, sampler, pass, dstElement);
}

Graph::Sampler Renderer_GetNearSampler()
{
    return s_NearSampler;
}
Graph::Sampler Renderer_GetLinearSampler()
{
    return s_LinearSampler;
}
Graph::Sampler Renderer_GetEdgeSampler()
{
    return s_EdgeSampler;
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

static TKit::FixedArray<CascadeData, ONYX_3D_MAX_CASCADES> createCascades(const f32v3 &dir, const RenderView<D3> *rview,
                                                                          const f32v3 &offset, const f32v2 &extent,
                                                                          const u32 count, const f32 zmul,
                                                                          const f32 lambda, const f32 overlap,
                                                                          u32 &enableFlags)
{
    const TKit::FixedArray8<f32v4> globalCorners = getCameraCorners(rview->GetProjectionView());

    TKit::FixedArray<CascadeData, ONYX_3D_MAX_CASCADES> cascades;
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

        const f32v3 up = Math::Normalize(Math::Cross(Transform<D3>::GetUpDirection(dir), dir));
        const f32m4 view = Transform<D3>::LookTowards(center, dir, up);
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

            TKIT_ASSERT(c.View, "[ONYX][RENDERER] A view must be specified when using directional light shadows");

            data.CascadeEnable = (1U << ONYX_3D_MAX_CASCADES) - 1;
            data.Cascades = createCascades(dir, c.View, data.Offset, params.Extent, c.Count, c.ZMul, c.Lambda,
                                           c.Overlap, data.CascadeEnable);
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

    const f32 near = ONYX_3D_SPOT_LIGHT_NEAR_RANGE_FACTOR * params.ShadowRange;
    const f32 far = params.ShadowRange;

    data.Near = near;
    data.Far = far;

    const f32m4 proj = Transform<D3>::Perspective(params.FieldOfView, near, far);
    const f32v3 up = Math::Normalize(Math::Cross(Transform<D3>::GetUpDirection(data.Direction), data.Direction));
    const f32m4 view = Transform<D3>::LookTowards(params.Position, data.Direction, up);

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

template <Dimension D> static void transfer()
{
    [[maybe_unused]] const SpecificSpecs<D> &specs = GetSpecs().GetSpecific<D>();
    RendererData<D> &rdata = getRendererData<D>();
    LightData<D> &ldata = rdata.Lights;
    ldata.Clear();

    ResourceGroup<D> *group = findAvailableResourceGroup<D>();
    rdata.ActiveResourceGroup = group;

    const TKit::TierArray<RenderContext<D> *> &contexts = rdata.Contexts;
    if (contexts.IsEmpty())
        return;

    TKit::FixedArray<usz, Geometry_Count> offsets{};

    u32 vertexOffset = 0;
    u32 firstIndex = 0;

    for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
        for (u32 rmode = 0; rmode < RenderMode_Count; ++rmode)
        {
            const auto copyInstanceData = [&](const Geometry geo, const InstanceHostBuffer &idata, const ViewMask vmask,
                                              const Resource handle = NullHandle) {
                if (idata.Instances == 0)
                    return;

                InstanceDeviceBuffer &buff = group->InstanceBuffers[geo];

                TKIT_ASSERT(offsets[geo] / GetInstanceSize<D>(geo) + idata.Instances <= specs.MaxInstances,
                            "[ONYX][RENDERER] This frame draws at least {} {}D instances of geometry '{}', but "
                            ".Specs{}.MaxInstances is {}. Raise it in the specs given to Onyx::Initialize",
                            offsets[geo] / GetInstanceSize<D>(geo) + idata.Instances, u8(D), ToString(geo), u8(D),
                            specs.MaxInstances);

                InstanceRange &range = buff.Ranges.Append();
                range.Blend = BlendPass(bpass);
                range.Offset = offsets[geo];
                range.RenderFlags = GetRenderModeFlags(RenderMode(rmode));
                range.Size = idata.Instances * GetInstanceSize<D>(geo);
                range.ViewMask = vmask;
                range.MeshHandle = handle;
                if (handle != NullHandle && Handle_GetResourceType(handle) == Resource_DynamicMesh)
                {
                    const DynamicMeshData<D> &data = DynamicMesh_GetData<D>(handle);

                    TKIT_ASSERT(vertexOffset + data.Vertices.GetSize() <= specs.MaxDynamicVertices &&
                                    firstIndex + data.Indices.GetSize() <= specs.MaxDynamicIndices,
                                "[ONYX][RENDERER] This frame draws at least {} {}D dynamic mesh vertices and {} "
                                "indices, but .Specs{}.MaxDynamicVertices is {} and .MaxDynamicIndices is {}. Every "
                                "draw of a dynamic mesh copies its vertices and indices again. Raise them in the specs "
                                "given to Onyx::Initialize",
                                vertexOffset + data.Vertices.GetSize(), u8(D), firstIndex + data.Indices.GetSize(),
                                u8(D), specs.MaxDynamicVertices, specs.MaxDynamicIndices);

                    range.VertexOffset = vertexOffset;
                    range.FirstIndex = firstIndex;
                    range.IndexCount = data.Indices.GetSize();

                    Graph::Buffer_Write(
                        group->VertexBuffer, data.Vertices.GetData(),
                        {.Size = data.Vertices.GetBytes(), .DstOffset = vertexOffset * sizeof(DynamicVertex<D>)});

                    Graph::Buffer_Write(group->IndexBuffer, data.Indices.GetData(),
                                        {.Size = data.Indices.GetBytes(), .DstOffset = firstIndex * sizeof(Index)});

                    vertexOffset += data.Vertices.GetSize();
                    firstIndex += range.IndexCount;
                }

                Graph::Buffer_Write(buff.Buffer, idata.Data.GetData(), {.Size = range.Size, .DstOffset = range.Offset});
                offsets[geo] += range.Size;
            };
            for (RenderContext<D> *ctx : contexts)
                copyInstanceData(Geometry_Circle, ctx->GetInstanceData().Circles[bpass][rmode], ctx->ViewMask);

            for (RenderContext<D> *ctx : contexts)
            {
                const InstanceResourceGroup &dynGroup = ctx->GetInstanceData().DynamicMeshes[bpass][rmode];
                for (const u32 rid : dynGroup.Registry.ResourceIds)
                {
                    const Resource handle = Handle_CreateForResource(Resource_DynamicMesh, rid);
                    copyInstanceData(Geometry_Dynamic, dynGroup.Buffers[rid], ctx->ViewMask, handle);
                }
            }

            const auto copyCanonicalMeshes = [&](const Geometry geo) {
                const ResourceType rtype = getResourceType(geo);
                const TKit::Span<const u32> poolIds = ResourcePool_GetIds<D>(rtype);
                for (RenderContext<D> *ctx : contexts)
                    for (const u32 pid : poolIds)
                    {
                        const InstanceResourceGroup &meshGroup =
                            ctx->GetInstanceData().Meshes[bpass][rmode][rtype][pid];
                        for (const u32 rid : meshGroup.Registry.ResourceIds)
                        {
                            const Resource handle = Handle_CreateForResource(rtype, rid, pid);
                            copyInstanceData(geo, meshGroup.Buffers[rid], ctx->ViewMask, handle);
                        }
                    }
            };
            copyCanonicalMeshes(Geometry_Static);
            copyCanonicalMeshes(Geometry_Parametric);
            copyCanonicalMeshes(Geometry_Glyph);
        }

    for (RenderContext<D> *ctx : contexts)
    {
        const ViewMask vmask = ctx->ViewMask;
        const auto gatherLights = [&]<typename LightParams>([[maybe_unused]] const LightType ltype,
                                                            const TKit::TierArray<LightParams> &lightParams,
                                                            ContextLights<LightParams> &clights) {
            for (const LightParams &params : lightParams)
            {
                u32 shadowMapOffset = TKIT_U32_MAX;
                if (params.Flags & LightFlag_CastShadows)
                {
                    const u32 count = std::popcount(vmask);
                    if constexpr (D == D2)
                    {
                        LightResources<D2> &l2 = group->LightData;
                        shadowMapOffset = l2.ShadowMaps.Cursor;
                        l2.ShadowMaps.Cursor += count;
                        TKIT_ASSERT(l2.ShadowMaps.Cursor <= specs.MaxShadowMaps,
                                    "[ONYX][RENDERER] Shadow casting 2D lights need at least {} shadow maps this frame "
                                    "(one per light and per view its context targets), but .Specs2.MaxShadowMaps is "
                                    "{}. Raise it in the specs given to Onyx::Initialize",
                                    l2.ShadowMaps.Cursor, specs.MaxShadowMaps);
                    }
                    else
                    {
                        LightResources<D3> &l3 = group->LightData;
                        shadowMapOffset = l3.ShadowMaps[ltype].Cursor;
                        l3.ShadowMaps[ltype].Cursor += count;
                        TKIT_ASSERT(l3.ShadowMaps[ltype].Cursor <= specs.MaxShadowMaps[ltype],
                                    "[ONYX][RENDERER] Shadow casting 3D {} lights need at least {} shadow maps this "
                                    "frame (one per light and per view its context targets), but "
                                    ".Specs3.MaxShadowMaps[{}] is {}. Raise it in the specs given to Onyx::Initialize",
                                    ToString(ltype), l3.ShadowMaps[ltype].Cursor, u32(ltype),
                                    specs.MaxShadowMaps[ltype]);
                    }
                }
                clights.Parameters.Append(params);
                clights.Data.Append(createLightData(vmask, shadowMapOffset, params));
            }
        };

        gatherLights(Light_Point, ctx->GetPointLightParams(), ldata.Points);
        gatherLights(Light_Directional, ctx->GetDirectionalLightParams(), ldata.Directionals);
        if constexpr (D == D3)
            gatherLights(Light_Spot, ctx->GetSpotLightParams(), ldata.Spots);
    }
    const auto copyLights = [&]<typename LightParams>(const LightType ltype,
                                                      const ContextLights<LightParams> &clights) {
        if (clights.Data.IsEmpty())
            return;

        TKIT_ASSERT(clights.Data.GetSize() <= specs.MaxLightInstances,
                    "[ONYX][RENDERER] This frame has {} {}D {} lights, but .Specs{}.MaxLightInstances is {}. Raise it "
                    "in the specs given to Onyx::Initialize",
                    clights.Data.GetSize(), u8(D), ToString(ltype), u8(D), specs.MaxLightInstances);
        Graph::Buffer_Write(group->LightData.Buffers[ltype], clights.Data.GetData(), clights.Data.GetBytes());
    };
    copyLights(Light_Point, ldata.Points);
    copyLights(Light_Directional, ldata.Directionals);
    if constexpr (D == D3)
        copyLights(Light_Spot, ldata.Spots);
}

void Renderer_Transfer()
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Transfer");
    transfer<D2>();
    transfer<D3>();
}

static constexpr Graph::IndexType s_IndexType = sizeof(Index) == 1   ? Graph::IndexType_Unsigned8
                                                : sizeof(Index) == 2 ? Graph::IndexType_Unsigned16
                                                                     : Graph::IndexType_Unsigned32;

template <Dimension D> static void bindMeshBuffers(const Graph::CommandBuffer cmd, const ResourcePool pool)
{
    const MeshBuffers buffers = ResourcePool_GetMeshBuffers<D>(pool);

    Graph::Command_BindVertexBuffers(cmd, buffers.VertexBuffer);
    Graph::Command_BindIndexBuffer(cmd, buffers.IndexBuffer, s_IndexType);
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
static Graph::DrawIndexedIndirectCommand createDynamicMeshCommand(const InstanceRange &range, const u32 firstInstance,
                                                                  const u32 instanceCount)
{
    Graph::DrawIndexedIndirectCommand cmd;
    cmd.FirstInstance = firstInstance;
    cmd.InstanceCount = instanceCount;
    cmd.FirstIndex = range.FirstIndex;
    cmd.IndexCount = range.IndexCount;
    cmd.VertexOffset = i32(range.VertexOffset);
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

template <Dimension D> static void beginShadowTransitionLayout(const Graph::CommandBuffer cmd, const Graph::Image img)
{
    const Graph::ImageLayout attLayout =
        D == D3 ? Graph::ImageLayout_DepthStencilAttachment : Graph::ImageLayout_ColorAttachment;
    transitionLayout(cmd, img, attLayout, Graph::AccessFlag_ShaderRead,
                     D == D3 ? Graph::AccessFlag_DepthStencilAttachmentWrite : Graph::AccessFlag_ColorAttachmentWrite,
                     D == D3 ? Graph::PipelineStageFlag_FragmentShader : Graph::PipelineStageFlag_ComputeShader,
                     D == D3 ? Graph::PipelineStageFlag_EarlyFragmentTests
                             : Graph::PipelineStageFlag_ColorAttachmentOutput);
}

template <Dimension D> static void endShadowTransitionLayout(const Graph::CommandBuffer cmd, const Graph::Image img)
{
    transitionLayout(cmd, img, D == D3 ? Graph::ImageLayout_ShaderReadOnly : Graph::ImageLayout_General,
                     D == D3 ? Graph::AccessFlag_DepthStencilAttachmentWrite : Graph::AccessFlag_ColorAttachmentWrite,
                     Graph::AccessFlag_ShaderRead,
                     D == D3 ? Graph::PipelineStageFlag_LateFragmentTests
                             : Graph::PipelineStageFlag_ColorAttachmentOutput,
                     D == D3 ? Graph::PipelineStageFlag_FragmentShader : Graph::PipelineStageFlag_ComputeShader);
}

template <Dimension D>
static void beginShadowPass(const Graph::CommandBuffer cmd, const Graph::Image img, const u32 imageViewIndex = 0)
{
    const Graph::ImageLayout attLayout =
        D == D3 ? Graph::ImageLayout_DepthStencilAttachment : Graph::ImageLayout_ColorAttachment;

    Graph::AttachmentInfo att{};
    const u32v2 extent = Graph::Image_GetSize(img);
    att.View = Graph::Image_GetView(img, imageViewIndex);
    att.Layout = attLayout;
    att.LoadOp = Graph::LoadOp_Clear;
    att.StoreOp = Graph::StoreOp_Store;

    Graph::RenderPassInfo renderInfo{};
    renderInfo.RenderArea = {{0, 0}, extent};
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

    Graph::Command_SetViewport(cmd, f32v2{0.f}, f32v2{extent});
    Graph::Command_SetScissor(cmd, i32v2{0}, extent);

    Graph::Command_BeginRenderPass(cmd, renderInfo);
}

static void endShadowPass(const Graph::CommandBuffer cmd)
{
    Graph::Command_EndRenderPass(cmd);
}

template <Dimension D, typename F>
void forEachDrawCommand(const ViewMask viewBit, const RenderModeFlags flags, const F fun,
                        const BlendPass bpass = BlendPass_All)
{
    RendererData<D> &rdata = getRendererData<D>();
    ResourceGroup<D> *group = rdata.ActiveResourceGroup;

    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        const u32 instanceSize = GetInstanceSize<D>(geo);
        for (const InstanceRange &range : group->InstanceBuffers[geo].Ranges)
        {
            if (!(range.ViewMask & viewBit) || !(range.RenderFlags & flags) ||
                (bpass != BlendPass_All && range.Blend != bpass))
                continue;

            TKIT_ASSERT(geo != Geometry_Circle || range.MeshHandle == NullHandle,
                        "[ONYX][RENDERER] Circle geometry is required to have a null mesh handle");

            const u32 fi = u32(range.Offset / instanceSize);
            const u32 ic = u32(range.Size / instanceSize);
            fun(geo, range, fi, ic);
        }
    }
}

template <Dimension D>
static void setupPipelineState(const Graph::CommandBuffer cmd, const RenderPass rpass, const Geometry geo,
                               const Graph::PipelineLayout playout, const Graph::Pipeline pipeline)
{
    const RendererData<D> &rdata = getRendererData<D>();
    const Graph::DescriptorSet set = rdata.ActiveResourceGroup->Descriptors[rpass][geo];

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
static void recordDrawCommands(const Graph::CommandBuffer cmd, const RenderPass rpass,
                               const Graph::PipelineLayout playout,
                               const ten<Graph::Pipeline, Geometry_Count> &pipelines,
                               const CircleDrawCommands &circleCmds, const MeshDrawCommands &meshCmds,
                               const DynMeshDrawCommands &dynMeshCmds)
{
    RendererData<D> &rdata = getRendererData<D>();

    DrawCommandBuffer &drawBuffer = rdata.ActiveResourceGroup->DrawBuffer;
    DrawCommandBuffer &indexedDrawBuffer = rdata.ActiveResourceGroup->IndexedDrawBuffer;

    const u32 circleCmdCount = circleCmds.GetSize();

    if (circleCmdCount != 0)
    {
        const usz circleCmdSize = circleCmds.GetBytes();
        setupPipelineState<D>(cmd, rpass, Geometry_Circle, playout, pipelines[Geometry_Circle]);

        const usz offset = drawBuffer.Offset;
        TKIT_ASSERT(offset + circleCmdSize <=
                        GetSpecs().GetSpecific<D>().MaxDrawCommands * sizeof(Graph::DrawIndirectCommand),
                    "[ONYX][RENDERER] This frame records more than .Specs{}.MaxDrawCommands = {} circle draw commands. "
                    "There is one per context, blend pass, render mode and pass, in every view and shadow map render. "
                    "Raise it in the specs given to Onyx::Initialize",
                    u8(D), GetSpecs().GetSpecific<D>().MaxDrawCommands);

        Graph::Buffer_Write(drawBuffer.Buffer, circleCmds.GetData(), {.Size = circleCmdSize, .DstOffset = offset});
        drawBuffer.Offset += circleCmdSize;

        Graph::Command_DrawIndirect(cmd, drawBuffer.Buffer, circleCmdCount, offset);
    }

    // NOTE(Isma): Bit of a mess, should consider cleaning this up

    const auto drawMeshes = [&](const TKit::TierArray<Graph::DrawIndexedIndirectCommand> &cmds) {
        const u32 drawCount = cmds.GetSize();
        if (drawCount == 0)
            return;

        const usz size = cmds.GetBytes();
        const usz offset = indexedDrawBuffer.Offset;
        TKIT_ASSERT(offset + size <=
                        GetSpecs().GetSpecific<D>().MaxDrawCommands * sizeof(Graph::DrawIndexedIndirectCommand),
                    "[ONYX][RENDERER] This frame records more than .Specs{}.MaxDrawCommands = {} mesh draw commands. "
                    "There is one per mesh, context, blend pass, render mode and pass, in every view and shadow map "
                    "render. Raise it in the specs given to Onyx::Initialize",
                    u8(D), GetSpecs().GetSpecific<D>().MaxDrawCommands);
        Graph::Buffer_Write(indexedDrawBuffer.Buffer, cmds.GetData(), {.Size = size, .DstOffset = offset});
        indexedDrawBuffer.Offset += size;

        Graph::Command_DrawIndexedIndirect(cmd, indexedDrawBuffer.Buffer, drawCount, offset);
    };

    const auto drawCulledMeshes = [&]([[maybe_unused]] const Geometry geo, const PerCullPerCmd &cmds) {
        if (D == D2 || geo == Geometry_Glyph)
            drawMeshes(cmds[CullMode_None]);
        else
        {
            Graph::Command_SetCullMode(cmd, Graph::CullMode_None);
            drawMeshes(cmds[CullMode_None]);

            Graph::Command_SetCullMode(cmd, Graph::CullMode_Back);
            drawMeshes(cmds[CullMode_Back]);
        }
    };

    const auto renderMeshGeometry = [&](const Geometry geo) {
        setupPipelineState<D>(cmd, rpass, geo, playout, pipelines[geo]);

        const ResourceType rtype = getResourceType(geo);
        const TKit::Span<const u32> poolIds = ResourcePool_GetIds<D>(rtype);

        for (const ResourcePool pid : poolIds)
        {
            bindMeshBuffers<D>(cmd, Handle_CreateForResourcePool(rtype, pid));
            drawCulledMeshes(geo, meshCmds[rtype][pid]);
        }
    };

    renderMeshGeometry(Geometry_Static);
    renderMeshGeometry(Geometry_Parametric);
    renderMeshGeometry(Geometry_Glyph);

    if (!dynMeshCmds[CullMode_None].IsEmpty() || !dynMeshCmds[CullMode_Back].IsEmpty())
    {
        RendererData<D> &rdata = getRendererData<D>();
        setupPipelineState<D>(cmd, rpass, Geometry_Dynamic, playout, pipelines[Geometry_Dynamic]);
        Graph::Command_BindVertexBuffers(cmd, rdata.ActiveResourceGroup->VertexBuffer);
        Graph::Command_BindIndexBuffer(cmd, rdata.ActiveResourceGroup->IndexBuffer, s_IndexType);

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

template <Dimension D> static void renderShadows(const Graph::CommandBuffer cmd, const ViewMask viewBit)
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    ResourceGroup<D> *group = rdata.ActiveResourceGroup;

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

            if (!(data.ViewMask & viewBit) || !(data.Flags & LightFlag_CastShadows))
                continue;

            static constexpr bool isPoint = std::is_same_v<LightParams, PointLightParameters<D>>;
            static constexpr bool isDir = std::is_same_v<LightParams, DirectionalLightParameters<D>>;
            static constexpr bool isSpot = std::is_same_v<LightParams, SpotLightParameters>;

            if constexpr (D == D3 && isDir)
                if (data.CascadeEnable == 0)
                    continue;

            const LightParams &params = lights.Parameters[i];

            const u32 viewIndex = std::popcount(data.ViewMask & (viewBit - 1));
            const u32 shindex = data.ShadowMapOffset + viewIndex;
            const Graph::Image smap = group->LightData.GetShadowMap(ltype, shindex);

            CircleDrawCommands circleCmds{};
            DynMeshDrawCommands dynMeshCmds{};
            MeshDrawCommands meshCmds{};
            const auto insertCommand = [&](const Geometry geo, const InstanceRange &range, const u32 fi, const u32 ic) {
                if (geo == Geometry_Circle)
                    circleCmds.Append(createCircleCommand(fi, ic));
                else
                {
                    const ResourceType rtype = getResourceType(geo);
                    ONYX_CHECK_RESOURCE_IS_NOT_NULL(range.MeshHandle);
#ifdef TKIT_ENABLE_ENSURE
                    if (geo != Geometry_Dynamic)
                    {
                        ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(range.MeshHandle);
                        ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(range.MeshHandle, rtype, D);
                    }
#endif
                    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(range.MeshHandle, rtype, D);

                    const u32 pid = Handle_GetResourcePoolId(range.MeshHandle);
                    PerCullPerCmd &cmds = geo == Geometry_Dynamic ? dynMeshCmds : meshCmds[rtype][pid];
                    const Graph::DrawIndexedIndirectCommand cmd = geo == Geometry_Dynamic
                                                                      ? createDynamicMeshCommand<D>(range, fi, ic)
                                                                      : createMeshCommand<D>(range.MeshHandle, fi, ic);

                    if constexpr (D == D2)
                        cmds[CullMode_None].Append(cmd);
                    else
                        cmds[getCullMode(range.MeshHandle)].Append(cmd);
                }
            };
            forEachDrawCommand<D>(viewBit, RenderModeFlag_Shaded, insertCommand);

            const auto processMap = [&](const Graph::Image map, const f32m4 &projView, const u32 viewIndex = 0) {
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
                recordDrawCommands<D>(cmd, RenderPass_Shadow, playout, sdata.Pipelines, circleCmds, meshCmds,
                                      dynMeshCmds);
                endShadowPass(cmd);
            };

            if constexpr (D == D2)
            {
                LightResources<D2> &l2 = group->LightData;
                const u32 ocindex = l2.OcclusionMaps.Cursor++;

                TKIT_ASSERT(ocindex < l2.OcclusionMaps.Maps.GetSize(),
                            "[ONYX][RENDERER] Shadow casting 2D lights need at least {} occlusion maps this frame (one "
                            "per light and per view that renders shadows), but .Specs2.MaxOcclusionMaps is {}. Raise "
                            "it in the specs given to Onyx::Initialize",
                            ocindex + 1, l2.OcclusionMaps.Maps.GetSize());

                const Graph::Image ocmap = l2.OcclusionMaps.Maps[ocindex];

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

                transitionLayout(cmd, smap, Graph::ImageLayout_General, Graph::AccessFlag_ShaderRead,
                                 Graph::AccessFlag_ShaderWrite, Graph::PipelineStageFlag_FragmentShader,
                                 Graph::PipelineStageFlag_ComputeShader);

                const Graph::DescriptorSet set = l2.RayMarchDescriptor;

                Graph::Command_BindPipeline(cmd, s_StandalonePipelines[StandalonePass_RayMarch]);
                const Graph::PipelineLayout playout = getPipelineLayout(StandalonePass_RayMarch);
                Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Compute, set);

                RayMarchPushConstantData pdata;
                pdata.OcclusionMapIndex = ocindex;
                pdata.OcclusionResolution = sdata.OcclusionResolution;
                pdata.ShadowMapIndex = shindex;
                pdata.ShadowResolution = sdata.GetShadowResolution(ltype);
                pdata.DistanceBias = params.DepthBias;

                if constexpr (isPoint)
                {
                    const f32 ext = Math::Pi() * data.Extent;
                    const f32 angle = params.Angle;
                    pdata.StartAngle = angle - ext;
                    pdata.EndAngle = angle + ext;
                    pdata.Mode = ONYX_2D_MARCH_MODE_POINT_LIGHT;
                }
                else
                    pdata.Mode = ONYX_2D_MARCH_MODE_DIRECTIONAL_LIGHT;

                Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Compute, &pdata);

                constexpr u32 groupSize = 64;
                Graph::Command_Dispatch(cmd, (pdata.ShadowResolution + groupSize - 1) / groupSize);

                transitionLayout(cmd, smap, Graph::ImageLayout_ShaderReadOnly, Graph::AccessFlag_ShaderWrite,
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
    processLight(Light_Point, ldata.Points);
    processLight(Light_Directional, ldata.Directionals);
    if constexpr (D == D3)
        processLight(Light_Spot, ldata.Spots);
}

template <Dimension D>
static void renderGeometry(const Graph::CommandBuffer cmd, const ViewInfo<D> &vinfo, const BlendPass bpass,
                           const ShadedFlags flags)
{
    RendererData<D> &rdata = getRendererData<D>();
    const ViewMask viewBit = vinfo.ViewBit;

    // TODO(Isma): At some point would be good letting the user decide what strategy to use to aggregate ambient
    Color ambient{0.f, 0.f};
    for (const RenderContext<D> *ctx : rdata.Contexts)
        if (ctx->ViewMask & viewBit)
        {
            const Color &a = ctx->GetAmbientLight();
            ambient.rgba = Math::Max(ambient.rgba, a.rgba);
        }
    const u32 ambientColor = ambient.ToLinear().Pack();

    TKit::FixedArray<CircleDrawCommands, PipelinePass_Count> circleCmds{};
    TKit::FixedArray<DynMeshDrawCommands, PipelinePass_Count> dynMeshCmds{};
    TKit::FixedArray<MeshDrawCommands, PipelinePass_Count> meshCmds{};
    TKit::FixedArray<u32, PipelinePass_Count> cmdCount{};

    const auto insertCommand = [&](const Geometry geo, const InstanceRange &range, const u32 fi, const u32 ic) {
        u32 pcount = 0;
        TKit::FixedArray<PipelinePass, 3> passes;
        const RenderModeFlags flags = range.RenderFlags;
        if (flags & RenderModeFlag_Shaded)
            passes[pcount++] = PipelinePass_Shaded;
        if (flags & RenderModeFlag_Flat)
            passes[pcount++] = PipelinePass_Flat;
        if (flags & RenderModeFlag_Outlined)
            passes[pcount++] = PipelinePass_Outlined;
        TKIT_ASSERT(pcount != 0, "[ONYX][RENDERER] Pass count should not be zero");

        if (geo == Geometry_Circle)
            for (u32 i = 0; i < pcount; ++i)
            {
                const PipelinePass p = passes[i];
                circleCmds[p].Append(createCircleCommand(fi, ic));
                ++cmdCount[p];
            }
        else
        {
            const ResourceType rtype = getResourceType(geo);
            ONYX_CHECK_RESOURCE_IS_NOT_NULL(range.MeshHandle);
#ifdef TKIT_ENABLE_ENSURE
            if (geo != Geometry_Dynamic)
            {
                ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(range.MeshHandle);
                ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(range.MeshHandle, rtype, D);
            }
#endif
            ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(range.MeshHandle, rtype, D);

            const u32 pid = Handle_GetResourcePoolId(range.MeshHandle);

            CullMode cull;
            if constexpr (D == D2)
                cull = CullMode_None;
            else
                cull = getCullMode(range.MeshHandle);

            const Graph::DrawIndexedIndirectCommand cmd = geo == Geometry_Dynamic
                                                              ? createDynamicMeshCommand<D>(range, fi, ic)
                                                              : createMeshCommand<D>(range.MeshHandle, fi, ic);
            if (geo == Geometry_Dynamic)
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

    forEachDrawCommand<D>(viewBit, RenderModeFlag_Shaded | RenderModeFlag_Outlined | RenderModeFlag_Flat, insertCommand,
                          bpass);

    LightData<D> &ldata = rdata.Lights;
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
            pdata.Flags = flags;

            for (u32 j = 0; j < LightTypeCount<D>; ++j)
                pdata.TexelSizes[j] = 1.f / f32(sdata.GetShadowResolution(j));

            pdata.LightCounts[Light_Point] = ldata.Points.Data.GetSize();
            pdata.LightCounts[Light_Directional] = ldata.Directionals.Data.GetSize();

            if constexpr (D == D3)
            {
                pdata.LightCounts[Light_Spot] = ldata.Spots.Data.GetSize();
                pdata.ViewPosition = vinfo.ViewPosition;
                pdata.ViewForward = vinfo.ViewForward;
            }

            pdata.ViewBit = viewBit;
            pdata.AmbientColor = ambientColor;
            Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Vertex | Graph::ShaderStageFlag_Fragment,
                                         &pdata);
        }

        // when BlendPass_All is passed, it means transparent pass is disabled. in that case, all geometry has been
        // gathered in commands, and the pipeline used will be the opaque one
        const u32 idx = bpass == BlendPass_All ? BlendPass_Opaque : bpass;
        recordDrawCommands<D>(cmd, rpass, playout, rdata.Pipelines[idx][pass], circleCmds[pass], meshCmds[pass],
                              dynMeshCmds[pass]);
    }
}

template <Dimension D>
static void renderViews(const Graph::CommandBuffer cmd, const TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &views)
{
    RendererData<D> &rdata = getRendererData<D>();
    const Graph::Tracker &tracker = rdata.ActiveResourceGroup->Tracker;

    TKit::StackArray<RenderView<D> *> outlineViews{};
    outlineViews.Reserve(views.GetSize());

    TKit::StackArray<RenderView<D> *> fxaaViews{};
    fxaaViews.Reserve(views.GetSize());

    for (RenderView<D> *rv : views)
    {
        const RenderViewFlags flags = rv->GetFlags();
        if (flags & RenderViewFlag_Hidden)
            continue;
        if (flags & RenderViewFlag_Outlines)
            outlineViews.Append(rv);
        if (flags & RenderViewFlag_FXAA)
            fxaaViews.Append(rv);

        const bool shadows = flags & RenderViewFlag_Shadows;
        if (shadows)
            renderShadows<D>(cmd, rv->GetViewBit());

        rv->CacheMatrices();
        const ViewInfo<D> vinfo = rv->CreateViewInfo();

        rv->MarkCurrentAttachmentsInUse(tracker);

        const bool transparency = flags & RenderViewFlag_Transparency;
        const BlendPass bpass = transparency ? BlendPass_Opaque : BlendPass_All;

        rv->BeginOpaquePass(cmd);
        renderGeometry<D>(cmd, vinfo, bpass, shadows ? ShadedFlag_Shadows : 0);
        rv->EndOpaquePass(cmd);
        if (transparency)
        {
            rv->BeginTransparentPass(cmd);
            renderGeometry<D>(cmd, vinfo, BlendPass_Transparent, shadows ? ShadedFlag_Shadows : 0);
            rv->EndTransparentPass(cmd);
            rv->BeginBlendPass(cmd);
            Graph::Command_BindPipeline(cmd, s_StandalonePipelines[StandalonePass_Blend]);

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

    if (!outlineViews.IsEmpty())
    {
        const Graph::PipelineLayout playout = getPipelineLayout(StandalonePass_Outline);
        for (RenderView<D> *rv : outlineViews)
        {
            rv->BeginOutlinePass(cmd);
            Graph::Command_BindPipeline(cmd, s_StandalonePipelines[StandalonePass_Outline]);

            const Graph::DescriptorSet set = rv->GetOutlineSet();
            Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Graphics, set);

            OutlinePushConstantData pdata;
            pdata.AttachmentIndex = rv->GetAttachmentIndex();

            TKIT_ASSERT(pdata.AttachmentIndex < ONYX_MAX_ATTACHMENTS,
                        "[ONYX][RENDERER] The maximum amount of attachments has been exceeded ({} >= {})",
                        pdata.AttachmentIndex, ONYX_MAX_ATTACHMENTS);

            pdata.Extent = rv->GetImageExtent();
            pdata.MaxOutlineWidth = u32(f32(rv->MaxOutlineWidth) * rv->GetContentScale()[0]);

            Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Fragment, &pdata);
            Graph::Command_Draw(cmd, 6);

            rv->EndOutlinePass(cmd);
        }
    }

    if (!fxaaViews.IsEmpty())
    {
        const Graph::PipelineLayout playout = getPipelineLayout(StandalonePass_FXAA);
        for (RenderView<D> *rv : fxaaViews)
        {
            rv->BeginFXAAPass(cmd);
            Graph::Command_BindPipeline(cmd, s_StandalonePipelines[StandalonePass_FXAA]);

            const Graph::DescriptorSet set = rv->GetFXAASet();
            Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Graphics, set);

            const u32 attIdx = rv->GetAttachmentIndex();
            TKIT_ASSERT(attIdx < ONYX_MAX_ATTACHMENTS,
                        "[ONYX][RENDERER] The maximum amount of attachments has been exceeded ({} >= {})", attIdx,
                        ONYX_MAX_ATTACHMENTS);

            FXAAPushConstantData pdata;
            pdata.AttachmentIndex = attIdx;
            pdata.Mode = rv->AntiAliasingMode;
            Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Fragment, &pdata);
            Graph::Command_Draw(cmd, 6);

            rv->EndFXAAPass(cmd);
        }
    }
}

template <Dimension D>
static void compositeView(const Graph::CommandBuffer cmd, const RenderView<D> *rv, const Graph::PipelineLayout playout)
{
    if (rv->GetFlags() & RenderViewFlag_Hidden)
        return;

    const Graph::DescriptorSet set = rv->GetCompositorSet();
    Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Graphics, set);

    const Viewport &vp = rv->GetViewport();
    const Scissor &sc = rv->GetScissor();

    const u32 idx = rv->GetAttachmentIndex();

    const f32v2 &scale = rv->GetContentScale(); // parentExtent / parentSize
    Graph::Command_SetViewport(cmd, vp.Position * scale, vp.Size * scale, vp.Depth);
    Graph::Command_SetScissor(cmd, i32v2{Math::Round((vp.Position + sc.Position) * scale)},
                              u32v2{Math::Round(sc.Size * scale)});

    Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Fragment, &idx);
    Graph::Command_Draw(cmd, 6);
}

static void renderCompositor(const Graph::CommandBuffer cmd, const RenderTargetInfo &tinfo,
                             const Graph::PipelineLayout playout)
{
    const auto &views2 = tinfo.Views2;
    const auto &views3 = tinfo.Views3;

    u32 i2 = 0;
    u32 i3 = 0;

    const u32 s2 = views2.GetSize();
    const u32 s3 = views3.GetSize();
    while (i2 < s2 || i3 < s3)
    {
        if (i3 == s3 || (i2 < s2 && views2[i2]->Layer <= views3[i3]->Layer))
            compositeView(cmd, views2[i2++], playout);
        else
            compositeView(cmd, views3[i3++], playout);
    }
}

template <typename Target>
RenderSubmitInfo render(const Graph::CommandBuffer cmd, Target *target, const RenderFlags flags)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Render");

    TKIT_ASSERT(s_RendererData2->ActiveResourceGroup && s_RendererData3->ActiveResourceGroup,
                "[ONYX][RENDERER] No active resource group. Forgot to call Transfer()?");

    const u64 graphicsFlight = Graph::Queue_ReserveTimelineValue(Graph::Queue_Graphics);
    const RenderTargetInfo tinfo = target->CreateRenderTargetInfo();

    Graph::Tracker tracker{};
    tracker.Queue = Graph::Queue_Graphics;
    tracker.InFlightValue = graphicsFlight;

    s_RendererData2->ActiveResourceGroup->Tracker = tracker;
    s_RendererData3->ActiveResourceGroup->Tracker = tracker;

    renderViews(cmd, tinfo.Views2);
    renderViews(cmd, tinfo.Views3);

    // windows do not need to mark their image as in use: the swap chain is tracked by Graph on submission
    if constexpr (std::is_same_v<Target, RenderTexture>)
        target->MarkWriteImageInUse(tracker);

    target->BeginRendering(cmd);

    Graph::Command_BindPipeline(cmd, s_StandalonePipelines[StandalonePass_Compositor]);

    const Graph::PipelineLayout playout = getPipelineLayout(StandalonePass_Compositor);
    renderCompositor(cmd, tinfo, playout);

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
    RenderSubmitInfo info{};
    info.Command = cmd;
    info.InFlightValue = graphicsFlight;
    info.Swapchain = tinfo.Swapchain;
    return info;
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
        // when a swap chain is provided, Graph takes care of the image available/render finished semaphores
        sinfo.Swapchain = rinfo.Swapchain;
        sinfo.SignalValue = rinfo.InFlightValue;
    }

    MarkCommandPoolInUse(pool, maxFlight);
    Graph::Queue_Submit(Graph::Queue_Graphics, submits);

    s_RendererData2->ActiveResourceGroup = nullptr;
    s_RendererData3->ActiveResourceGroup = nullptr;
}

template RenderContext<D2> *Renderer_CreateContext(const RenderContextSpecs &specs);
template RenderContext<D3> *Renderer_CreateContext(const RenderContextSpecs &specs);

template void Renderer_DestroyContext(const RenderContext<D2> *context);
template void Renderer_DestroyContext(const RenderContext<D3> *context);

template void Renderer_WriteBufferDescriptor<D2>(u32 binding, const Graph::DescriptorBufferInfo &info, RenderPass pass,
                                                 u32 dstElement);
template void Renderer_WriteBufferDescriptor<D3>(u32 binding, const Graph::DescriptorBufferInfo &info, RenderPass pass,
                                                 u32 dstElement);

template void Renderer_WriteImageDescriptor<D2>(u32 binding, const Graph::DescriptorImageInfo &info, RenderPass pass,
                                                u32 dstElement);
template void Renderer_WriteImageDescriptor<D3>(u32 binding, const Graph::DescriptorImageInfo &info, RenderPass pass,
                                                u32 dstElement);

template void Renderer_WriteSamplerDescriptor<D2>(u32 binding, Graph::Sampler sampler, RenderPass pass, u32 dstElement);
template void Renderer_WriteSamplerDescriptor<D3>(u32 binding, Graph::Sampler sampler, RenderPass pass, u32 dstElement);

} // namespace Onyx
