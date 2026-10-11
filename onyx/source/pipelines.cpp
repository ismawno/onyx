#include "pch.hpp"
#include "pipelines.hpp"
#include "conversion.hpp"
#include "descriptors.hpp"
#include "instance.hpp"
#include "attachment.hpp"
#ifndef ONYX_COMPILE_SHADERS_AT_RUNTIME
#    include "spirv.hpp"
#endif
#include "graph/pipeline.hpp"
#include "graph/shader.hpp"
#include "onyx/vertex.hpp"
#include "platform.hpp"
#include "tkit/preprocessor/utils.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx::Pipelines
{
constexpr u32 Dim2 = 0;
constexpr u32 Dim3 = 1;

struct ShaderData
{
    TKit::FixedArray<Graph::Shader, Geometry_Count> VertexShaders{};
    TKit::FixedArray<Graph::Shader, Geometry_Count> OpaqueFragmentShaders{};
    TKit::FixedArray<Graph::Shader, Geometry_Count> TransparentFragmentShaders{};

    void Destroy()
    {
        for (const Graph::Shader sh : VertexShaders)
            Graph::Shader_Destroy(sh);
        for (const Graph::Shader sh : OpaqueFragmentShaders)
            Graph::Shader_Destroy(sh);
        for (const Graph::Shader sh : TransparentFragmentShaders)
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

template <Dimension D> ShaderData &getShaders(const RenderPass pass)
{
    return s_PipelineData->Shaders[D - 2][pass];
}

static void createPipelineLayouts()
{
    constexpr Graph::ShaderStageFlags vertex = Graph::ShaderStageFlag_Vertex;
    constexpr Graph::ShaderStageFlags fragment = Graph::ShaderStageFlag_Fragment;
    constexpr Graph::ShaderStageFlags compute = Graph::ShaderStageFlag_Compute;

    const auto create = [](const Graph::DescriptorSetLayout set, const Graph::ShaderStageFlags stages, const u32 size) {
        const Graph::PushConstantRange range = Graph::PushConstantRange::Create(stages, size);
        return Graph::PipelineLayout_Create({.DescriptorSetLayouts = set, .PushRanges = range});
    };

    s_PipelineData->Layouts[Dim2][RenderPass_Shaded] = create(Descriptors::GetDescriptorLayout<D2>(RenderPass_Shaded),
                                                              vertex | fragment, sizeof(ShadedPushConstantData<D2>));

    s_PipelineData->Layouts[Dim3][RenderPass_Shaded] = create(Descriptors::GetDescriptorLayout<D3>(RenderPass_Shaded),
                                                              vertex | fragment, sizeof(ShadedPushConstantData<D3>));

    s_PipelineData->Layouts[Dim2][RenderPass_Flat] =
        create(Descriptors::GetDescriptorLayout<D2>(RenderPass_Flat), vertex, sizeof(FlatPushConstantData));

    s_PipelineData->Layouts[Dim3][RenderPass_Flat] =
        create(Descriptors::GetDescriptorLayout<D3>(RenderPass_Flat), vertex, sizeof(FlatPushConstantData));

    s_PipelineData->Layouts[Dim2][RenderPass_Shadow] =
        create(Descriptors::GetDescriptorLayout<D2>(RenderPass_Shadow), vertex, sizeof(ShadowPushConstantData<D2>));

    s_PipelineData->Layouts[Dim3][RenderPass_Shadow] = create(Descriptors::GetDescriptorLayout<D3>(RenderPass_Shadow),
                                                              vertex | fragment, sizeof(ShadowPushConstantData<D3>));

    s_PipelineData->Standalone[StandalonePass_RayMarch].Layout =
        create(Descriptors::GetDescriptorLayout(StandalonePass_RayMarch), compute, sizeof(RayMarchPushConstantData));

    s_PipelineData->Standalone[StandalonePass_Blend].Layout =
        create(Descriptors::GetDescriptorLayout(StandalonePass_Blend), fragment, sizeof(BlendPushConstantData));

    s_PipelineData->Standalone[StandalonePass_PostProcess].Layout = create(
        Descriptors::GetDescriptorLayout(StandalonePass_PostProcess), fragment, sizeof(PostProcessPushConstantData));

    s_PipelineData->Standalone[StandalonePass_Compositor].Layout = create(
        Descriptors::GetDescriptorLayout(StandalonePass_Compositor), fragment, sizeof(CompositorPushConstantData));

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
    const DeviceInfo dev = GetRenderBackendInfo().Device;
    if (dev.Driver != Driver_MesaIntelOpenSource && dev.Driver != Driver_MesaLLVMPipe)
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

    const TKit::FixedArray<TKit::String, Geometry_Count> geos = {"circle", "static", "parametric", "glyph", "dynamic"};
    const TKit::FixedArray<TKit::String, RenderPass_Count> passes = {"flat", "shaded", "shadow"};
    const TKit::FixedArray<TKit::String, D_Count> dims = {"2D", "3D"};

    const Graph::EntryPoint geometryEntries[] = {{"mainVS", Graph::ShaderStageFlag_Vertex},
                                                 {"mainFSO", Graph::ShaderStageFlag_Fragment},
                                                 {"mainFST", Graph::ShaderStageFlag_Fragment}};
    const Graph::EntryPoint fullVertexEntries[] = {{"mainVS", Graph::ShaderStageFlag_Vertex}};
    const Graph::EntryPoint rayMarchEntries[] = {{"main", Graph::ShaderStageFlag_Compute}};
    const Graph::EntryPoint fragmentEntries[] = {{"mainFS", Graph::ShaderStageFlag_Fragment}};

    // module names and the module array must not reallocate, since the compilation specs point into them
    TKit::StackArray<TKit::String> names{};
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

void ReloadShaders()
{
    destroyShaders();
    return createShaders();
}

void Initialize()
{
    TKIT_LOG_INFO("[ONYX][PIPELINES] Initializing");
#ifndef ONYX_COMPILE_SHADERS_AT_RUNTIME
    InitializeBinaries();
#endif
    s_PipelineData.Construct();
    createPipelineLayouts();
    return createShaders();
}
void Terminate()
{
    destroyShaders();
    for (const auto layout : s_PipelineData->Layouts)
        Graph::PipelineLayout_Destroy(layout);

    for (const auto &st : s_PipelineData->Standalone)
        Graph::PipelineLayout_Destroy(st.Layout);

    s_PipelineData.Destruct();
}

template <Dimension D> Graph::PipelineLayout GetPipelineLayout(const RenderPass pass)
{
    return s_PipelineData->Layouts[D - 2][pass];
}
Graph::PipelineLayout GetPipelineLayout(const StandalonePass pass)
{
    return s_PipelineData->Standalone[pass].Layout;
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
Graph::Pipeline CreateGeometryPipeline(const PipelinePass pass, const BlendPass bpass, const Geometry geo)
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

    const u32 depth2 = u32(Renderer::IsDepthSupportedFor2D());
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

    const u32 acount = opaque ? 2 : 3;
    TKit::FixedArray<Graph::ColorAttachment, 3> attachments{};
    TKit::FixedArray<Graph::Format, 3> formats{};

    formats[0] = opaque ? cf : tf;
    formats[1] = cf;
    formats[2] = rf;

    Graph::ColorAttachment &color = attachments[0];
    color.BlendEnabled = !opaque || geo == Geometry_Glyph;
    color.SrcColor = opaqueParams ? Graph::BlendFactor_SrcAlpha : Graph::BlendFactor_One;
    color.DstColor = opaqueParams ? Graph::BlendFactor_OneMinusSrcAlpha : Graph::BlendFactor_One;
    color.SrcAlpha = Graph::BlendFactor_One;
    color.DstAlpha = opaqueParams ? Graph::BlendFactor_OneMinusSrcAlpha : Graph::BlendFactor_One;
    color.WriteMask = mainMask;

    attachments[1].WriteMask = outlineMask;

    if (!opaque)
    {
        Graph::ColorAttachment &revealage = attachments[2];
        revealage.BlendEnabled = true;
        revealage.SrcColor = Graph::BlendFactor_Zero;
        revealage.DstColor = Graph::BlendFactor_OneMinusSrcColor;
        revealage.WriteMask = mainMask;
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
    specs.ColorAttachments = {attachments.GetData(), acount};
    specs.ColorAttachmentFormats = {formats.GetData(), acount};
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

    constexpr Graph::DynamicStateFlags vpScissor = Graph::DynamicStateFlag_Viewport | Graph::DynamicStateFlag_Scissor;

    const bool dynCull = D == D3 && geo != Geometry_Circle && geo != Geometry_Glyph;
    specs.DynamicState = vpScissor | (dynCull ? Graph::DynamicStateFlag_CullMode : 0);

    return Graph::Pipeline_CreateGraphics(GetPipelineLayout<D>(rpass), specs);
}

template <Dimension D> Graph::Pipeline CreateShadowPipeline(const Geometry geo, const Graph::Format format)
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

    setLocation(attributes);
    Graph::GraphicsPipelineSpecs specs{};
    specs.ShaderStages = stages;
    specs.VertexBindings = bindings;
    specs.VertexAttributes = attributes;

    specs.DynamicState = Graph::DynamicStateFlag_Viewport | Graph::DynamicStateFlag_Scissor;
    if constexpr (D == D2)
    {
        specs.ColorAttachments = attachment;
        specs.ColorAttachmentFormats = format;
        specs.DepthStencil.DepthTestEnabled = false;
        specs.DepthStencil.DepthWriteEnabled = false;
    }
    else
    {
        specs.DepthFormat = format;
        specs.DepthStencil.DepthTestEnabled = true;
        specs.DepthStencil.DepthWriteEnabled = true;
        if (geo != Geometry_Circle && geo != Geometry_Glyph)
            specs.DynamicState |= Graph::DynamicStateFlag_CullMode;
    }
    return Graph::Pipeline_CreateGraphics(GetPipelineLayout<D>(RenderPass_Shadow), specs);
}

Graph::Pipeline CreateRayMarchPipeline()
{
    const StandalonePipelineData &data = s_PipelineData->Standalone[StandalonePass_RayMarch];
    Graph::ShaderStageInfo stage{};
    stage.Shader = data.Shader;
    stage.Stage = Graph::ShaderStageFlag_Compute;
    return Graph::Pipeline_CreateCompute(data.Layout, stage);
}

static Graph::Pipeline createFullPassPipeline(const StandalonePass pass, const Graph::Format format,
                                              const Graph::ColorAttachment &attachment)
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
    specs.ColorAttachmentFormats = format;
    specs.DepthStencil.DepthTestEnabled = false;
    specs.DepthStencil.DepthWriteEnabled = false;
    return Graph::Pipeline_CreateGraphics(data.Layout, specs);
}

Graph::Pipeline CreateBlendPipeline()
{
    Graph::ColorAttachment attachment{};
    attachment.BlendEnabled = true;
    attachment.SrcColor = Graph::BlendFactor_SrcAlpha;
    attachment.DstColor = Graph::BlendFactor_OneMinusSrcAlpha;
    // NOTE(Isma, 25/06/06): Should consider not writing to the alpha channel. Causes alpha values to be
    // inherited. For now it actually lets us having modals in ui, so ill keep it. but its a bit
    // counterintuitive
    attachment.SrcAlpha = Graph::BlendFactor_SrcAlpha;
    attachment.DstAlpha = Graph::BlendFactor_OneMinusSrcAlpha;
    return createFullPassPipeline(StandalonePass_Blend, GetAttachmentFormat(Attachment_Intermediate), attachment);
}

Graph::Pipeline CreatePostProcessPipeline()
{
    return createFullPassPipeline(StandalonePass_PostProcess, GetAttachmentFormat(Attachment_Final), {});
}

Graph::Pipeline CreateCompositorPipeline()
{
    Graph::ColorAttachment attachment{};
    attachment.BlendEnabled = true;
    return createFullPassPipeline(StandalonePass_Compositor, SurfaceFormat, attachment);
}

template Graph::PipelineLayout GetPipelineLayout<D2>(RenderPass pass);
template Graph::PipelineLayout GetPipelineLayout<D3>(RenderPass pass);

template Graph::Pipeline CreateGeometryPipeline<D2>(PipelinePass pass, BlendPass bpass, Geometry geo);
template Graph::Pipeline CreateGeometryPipeline<D3>(PipelinePass pass, BlendPass bpass, Geometry geo);
template Graph::Pipeline CreateShadowPipeline<D2>(Geometry geo, Graph::Format format);
template Graph::Pipeline CreateShadowPipeline<D3>(Geometry geo, Graph::Format format);

} // namespace Onyx::Pipelines
