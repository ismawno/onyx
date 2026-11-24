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
#include "graph/shader.hpp"
#include "graph/pipeline.hpp"
#include "graph/shader.hpp"
#include "graph/execution.hpp"
#include "graph/pipeline.hpp"
#include "tkit/profiling/clock.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/utils/utils.hpp"

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
    u32 FirstInstance = 0;
    u32 InstanceCount = 0;

    // only relevant for dynamic mesh instance data
    u32 VertexOffset = 0;
    u32 FirstIndex = 0;
    u32 IndexCount = 0;
    //

    ViewMask ViewMask = 0;

    Resource MeshHandle = NullHandle;
    RenderState RenderState = 0;
};

struct InstanceDeviceBuffer
{
    Graph::Buffer Buffer = Graph::NullHandle;
#ifdef ONYX_ENABLE_HOOKS
    Graph::Buffer HooksBuffer = Graph::NullHandle;
#endif
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

struct RangeSpan
{
    u32 Start;
    u32 End;
};

template <Dimension D> struct GeometryPipelineFieldDesc
{
    static constexpr u32 BitCount = RenderState_GeometryBitCount;

    static constexpr bool IsValid(const RenderState state)
    {
        const bool hasFill = (state & FillMode_Mask) != FillMode_None;
        const bool hasOutline = (state & OutlineMode_Mask) != OutlineMode_Disabled;
        const bool hasOther = state & (CullMode_Mask | BlendMode_Mask);
        return ContextFieldDesc<D>::IsValid(state) && (hasFill || hasOutline) && hasFill != hasOutline &&
               (!hasOutline || !hasOther);
    }
};

template <Dimension D> struct ShadowPipelineFieldDesc
{
    static constexpr u32 BitCount = RenderState_ShadowBitCount;

    static constexpr RenderState Canonicalize(const RenderState state)
    {
        RenderState mask = (BlendMode_Mask | OutlineMode_Mask);
        if constexpr (D == D2)
            mask |= CullMode_Mask;
        return state & ~mask;
    }
    static constexpr bool IsValid(const RenderState state)
    {
        return ContextFieldDesc<D>::IsValid(state) && (state & FillMode_Mask) == FillMode_Shaded;
    }
};

template <typename T, Dimension D>
using GeometryPipelineFieldMap = TKit::StaticBitFieldMap<T, GeometryPipelineFieldDesc<D>>;
template <typename T, Dimension D>
using ShadowPipelineFieldMap = TKit::StaticBitFieldMap<T, ShadowPipelineFieldDesc<D>>;

struct PipelineRun
{
    Graph::Pipeline Pipeline = Graph::NullHandle;
    RenderPass Pass = RenderPass_Count;
    ViewMask ViewMask = 0;
    TKit::TierArray<RangeSpan> Spans{};
    u32 CommandCount = 0;
};

template <Dimension D> struct ResourceGroup
{
    TKit::FixedArray<InstanceDeviceBuffer, Geometry_Count> InstanceBuffers{};
    LightResources<D> LightData{};

    ten<Graph::DescriptorSet, RenderPass_Count, Geometry_Count> Descriptors{};

    // for blend modes, each pipeline runs within a different pass
    ten<GeometryPipelineFieldMap<PipelineRun, D>, Geometry_Count> OpaqueRuns{};
    ten<GeometryPipelineFieldMap<PipelineRun, D>, Geometry_Count> TransparentRuns{};
    ViewMask TransparentViews = 0;
    //
    ten<ShadowPipelineFieldMap<PipelineRun, D>, Geometry_Count> ShadowRuns{};

    Graph::Buffer VertexBuffer = Graph::NullHandle;
    Graph::Buffer IndexBuffer = Graph::NullHandle;
#ifdef ONYX_ENABLE_HOOKS
    Graph::Buffer HookGlobalsBuffer = Graph::NullHandle;
#endif

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
    ten<ShadowPipelineFieldMap<Graph::Pipeline, D2>, Geometry_Count> Pipelines{}; // occlusion pipelines
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
    ten<ShadowPipelineFieldMap<Graph::Pipeline, D3>, Geometry_Count> Pipelines{};
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
    ten<GeometryPipelineFieldMap<Graph::Pipeline, D>, Geometry_Count> Pipelines{};
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

static constexpr u32 Dim2 = 0;
static constexpr u32 Dim3 = 1;

static void createPipelineLayouts()
{
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

#ifndef ONYX_COMPILE_SHADERS_AT_RUNTIME
static Graph::Shader shaderFromBinary(const ShaderBinary &binary)
{
    return Graph::Shader_Create(rcast<const u32 *>(binary.Code), binary.Count);
}
#endif

#ifdef GRAPH_HAS_SHADER_COMPILATION_BACKEND
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
#    ifdef ONYX_ENABLE_HOOKS
static const char *toShader(const HookType type)
{
    switch (type)
    {
    case HookType_F32:
        return "float";
    case HookType_F32V2:
        return "float2";
    case HookType_F32V3:
        return "float3";
    case HookType_F32V4:
        return "float4";
    case HookType_I32:
        return "int";
    case HookType_I32V2:
        return "int2";
    case HookType_I32V3:
        return "int3";
    case HookType_I32V4:
        return "int4";
    case HookType_U32:
        return "uint";
    case HookType_U32V2:
        return "uint2";
    case HookType_U32V3:
        return "uint3";
    case HookType_U32V4:
        return "uint4";
    default:
        TKIT_FATAL("[ONYX][SHADER-HOOK] Unrecognized shader hook type: {}", u8(type));
        return nullptr;
    }
}
static const char *toShader(const Graph::Format format)
{
    switch (format)
    {
    case Graph::Format_R8_UNORM:
    case Graph::Format_R8_SNORM:
    case Graph::Format_R8_SRGB:
    case Graph::Format_R16_UNORM:
    case Graph::Format_R16_SNORM:
    case Graph::Format_R16_SFLOAT:
    case Graph::Format_R32_SFLOAT:
        return "float";

    case Graph::Format_R8G8_UNORM:
    case Graph::Format_R8G8_SNORM:
    case Graph::Format_R8G8_SRGB:
    case Graph::Format_R16G16_UNORM:
    case Graph::Format_R16G16_SNORM:
    case Graph::Format_R16G16_SFLOAT:
    case Graph::Format_R32G32_SFLOAT:
        return "float2";

    case Graph::Format_R8G8B8_UNORM:
    case Graph::Format_R8G8B8_SNORM:
    case Graph::Format_R8G8B8_SRGB:
    case Graph::Format_R16G16B16_UNORM:
    case Graph::Format_R16G16B16_SNORM:
    case Graph::Format_R16G16B16_SFLOAT:
    case Graph::Format_R32G32B32_SFLOAT:
    case Graph::Format_B10G11R11_UnsignedFloat:
        return "float3";

    case Graph::Format_R8G8B8A8_UNORM:
    case Graph::Format_R8G8B8A8_SNORM:
    case Graph::Format_R8G8B8A8_SRGB:
    case Graph::Format_B8G8R8A8_UNORM:
    case Graph::Format_B8G8R8A8_SNORM:
    case Graph::Format_B8G8R8A8_SRGB:
    case Graph::Format_R16G16B16A16_UNORM:
    case Graph::Format_R16G16B16A16_SNORM:
    case Graph::Format_R16G16B16A16_SFLOAT:
    case Graph::Format_R32G32B32A32_SFLOAT:
        return "float4";

    case Graph::Format_R8_UINT:
    case Graph::Format_R16_UINT:
    case Graph::Format_R32_UINT:
        return "uint";
    case Graph::Format_R8G8_UINT:
    case Graph::Format_R16G16_UINT:
    case Graph::Format_R32G32_UINT:
        return "uint2";
    case Graph::Format_R8G8B8_UINT:
    case Graph::Format_R16G16B16_UINT:
    case Graph::Format_R32G32B32_UINT:
        return "uint3";
    case Graph::Format_R8G8B8A8_UINT:
    case Graph::Format_B8G8R8A8_UINT:
    case Graph::Format_R16G16B16A16_UINT:
    case Graph::Format_R32G32B32A32_UINT:
        return "uint4";

    case Graph::Format_R8_SINT:
    case Graph::Format_R16_SINT:
    case Graph::Format_R32_SINT:
        return "int";
    case Graph::Format_R8G8_SINT:
    case Graph::Format_R16G16_SINT:
    case Graph::Format_R32G32_SINT:
        return "int2";
    case Graph::Format_R8G8B8_SINT:
    case Graph::Format_R16G16B16_SINT:
    case Graph::Format_R32G32B32_SINT:
        return "int3";
    case Graph::Format_R8G8B8A8_SINT:
    case Graph::Format_B8G8R8A8_SINT:
    case Graph::Format_R16G16B16A16_SINT:
    case Graph::Format_R32G32B32A32_SINT:
        return "int4";

    default:
        TKIT_FATAL("[ONYX][SHADER-HOOK] Format {} cannot be used as a shader hook output", u8(format));
        return nullptr;
    }
}
static const char *toShader(const HookInterp interp)
{
    switch (interp)
    {
    case HookInterp_Smooth:
        return "";
    case HookInterp_Flat:
        return "nointerpolation ";
    case HookInterp_NoPerspective:
        return "noperspective ";
    default:
        TKIT_FATAL("[ONYX][SHADER-HOOK] Unrecognized shader hook interpolation: {}", u8(interp));
        return nullptr;
    }
}
static constexpr const char *s_HookEnums = R"(
[UnscopedEnum]
enum HookPass : uint
{
    HookPass_Flat,
    HookPass_Shaded,
    HookPass_Shadow,
}

[UnscopedEnum]
enum HookGeometry : uint
{
    HookGeometry_Circle,
    HookGeometry_Static,
    HookGeometry_Parametric,
    HookGeometry_Dynamic,
    HookGeometry_Glyph,
}

)";

// {0}: uint4 count, {1}: binding point
static constexpr const char *s_HookGlobalsApi = R"(
struct HookGlobals
{{
    uint4 Data[{0}];

    uint LoadU32(const uint offset) {{ return Data[offset / 16][(offset / 4) % 4]; }}
    int LoadI32(const uint offset) {{ return asint(LoadU32(offset)); }}
    float LoadF32(const uint offset) {{ return asfloat(LoadU32(offset)); }}
    float2 LoadF32V2(const uint offset) {{ return float2(LoadF32(offset), LoadF32(offset + 4)); }}
    float3 LoadF32V3(const uint offset) {{ return float3(LoadF32V2(offset), LoadF32(offset + 8)); }}
    float4 LoadF32V4(const uint offset) {{ return float4(LoadF32V3(offset), LoadF32(offset + 12)); }}
}}

[[vk::binding({1})]]
ConstantBuffer<HookGlobals> g_HookGlobals;

)";

// {0}: uint count, {1}: binding point
static constexpr const char *s_HookInstanceApi = R"(
struct HookInstance
{{
    uint Data[{0}];

    uint LoadU32(const uint offset) {{ return Data[offset / 4]; }}
    int LoadI32(const uint offset) {{ return asint(LoadU32(offset)); }}
    float LoadF32(const uint offset) {{ return asfloat(LoadU32(offset)); }}
    float2 LoadF32V2(const uint offset) {{ return float2(LoadF32(offset), LoadF32(offset + 4)); }}
    float3 LoadF32V3(const uint offset) {{ return float3(LoadF32V2(offset), LoadF32(offset + 8)); }}
    float4 LoadF32V4(const uint offset) {{ return float4(LoadF32V3(offset), LoadF32(offset + 12)); }}
}}

[[vk::binding({1})]]
StructuredBuffer<HookInstance, Std430DataLayout> g_HookInstances;

)";

template <Dimension D>
static TKit::TierString generateHookTypes(const TKit::Span<const HookVarying> varyings,
                                          const TKit::Span<const HookOutput> outputs, const u32 firstTarget)
{
    const SpecificSpecs<D> &specs = GetSpecs().GetSpecific<D>();
    const bool hasGlobals = specs.HookGlobalsSize != 0;
    const bool hasInstance = specs.HookInstanceSize != 0;
    const bool is3D = D == D3;

    TKit::TierString src{};
    src += is3D ? "typedef float3 HookVec;\n" : "typedef float2 HookVec;\n";
    src += s_HookEnums;

    if (hasGlobals)
        src += TKit::TierString::Format(s_HookGlobalsApi, (specs.HookGlobalsSize + 15) / 16,
                                        ONYX_GEOMETRY_PASS_HOOK_GLOBALS_BINDING_POINT);
    if (hasInstance)
        src += TKit::TierString::Format(s_HookInstanceApi, specs.HookInstanceSize / 4,
                                        ONYX_GEOMETRY_PASS_HOOK_INSTANCES_BINDING_POINT);

    if (!varyings.IsEmpty())
    {
        src += "struct HookVaryings\n{\n";
        for (const HookVarying &v : varyings)
            src += TKit::TierString::Format("    {}{} {};\n", toShader(v.Interpolation), toShader(v.Type), v.Name);
        src += "}\n\n";
    }

    if (!outputs.IsEmpty())
    {
        src += "struct HookOutputs\n{\n";
        for (u32 i = 0; i < outputs.GetSize(); ++i)
            src += TKit::TierString::Format("    {} {} : SV_Target{};\n", toShader(outputs[i].Format), outputs[i].Name,
                                            firstTarget + i);
        src += "}\n\n";
    }

    const auto common = [&] {
        if (is3D)
            src += "    float3x3 TBN;\n";
        src += "    uint InstanceIndex;\n    HookPass Pass;\n    HookGeometry Geometry;\n";
        if (hasGlobals)
            src += "    HookGlobals Globals;\n";
        if (hasInstance)
            src += "    HookInstance Instance;\n";
    };

    src += "struct VertexHookInput\n{\n    HookVec LocalPosition;\n    HookVec WorldPosition;\n    float2 TexCoord;\n"
           "    uint VertexIndex;\n";
    common();
    src += "}\n\n";

    src += "struct VertexHookOutput\n{\n    HookVec WorldPosition;\n";
    if (!varyings.IsEmpty())
        src += "    HookVaryings Varyings;\n";
    src += "}\n\n";

    src += "struct FragmentHookInput\n{\n    float4 Color;\n    HookVec WorldPosition;\n    float2 TexCoord;\n"
           "    float2 ScreenPosition;\n    bool HasMaterial;\n";
    common();
    if (!varyings.IsEmpty())
        src += "    HookVaryings Varyings;\n";
    src += "}\n\n";

    src += "struct FragmentHookOutput\n{\n    float4 Color;\n";
    if (!outputs.IsEmpty())
        src += "    HookOutputs Outputs;\n";
    src += "}\n";

    return src;
}
#    endif
Result<> Renderer_CompileShaders(const RendererShaderCompileOptions &ropts)
{
    const ShaderCompileOptions &opts = ropts.Options;
#    ifdef ONYX_ENABLE_HOOKS
    const HookCompileOptions *hopts = ropts.HookOptions;
    TKIT_ASSERT(!hopts || (opts.Dimensions[Dim2] + opts.Dimensions[Dim3]) == 1,
                "[ONYX][RENDERER] When compiling shaders for a hook exactly one dimension must be toggled");
    TKIT_ASSERT(!hopts || hopts->VertexOrOpaque || hopts->Transparent,
                "[ONYX][RENDERER] When compiling shaders for a hook at least one non-null shader "
                "destination must be provided");
    TKIT_ASSERT(!hopts || hopts->Stage != HookStage_Count,
                "[ONYX][RENDERER] When compiling shaders for a hook only vertex or fragment shaders are supported");
    TKIT_ASSERT(!hopts || hopts->Stage == HookStage_Vertex || !opts.RenderPasses[RenderPass_Shadow],
                "[ONYX][RENDERER], When compiling shaders for a hook shadow render passes are only allowed to be "
                "toggled on if the hook is a vertex hook");
    TKIT_ASSERT(!hopts || hopts->Stage == HookStage_Fragment || !hopts->Transparent,
                "[ONYX][RENDERER] When compiling shaders for a vertex hook Transparent must be null");
    TKIT_ASSERT(!hopts || hopts->Stage == HookStage_Fragment || hopts->Transparent,
                "[ONYX][RENDERER] When compiling shaders for a fragment hook Transparent must not be null");
    TKIT_ASSERT(!hopts || hopts->HookBitField != 0,
                "[ONYX][RENDERER] When compiling shaders for a hook its bit field must not be zero");
#    endif

    TKit::StaticArray<Graph::ShaderArgument, 3> args{};
    if (isOldMesa())
    {
        TKIT_LOG_WARNING("[ONYX][RENDERER] Old mesa version detected (pre-25.3.3) which contains a bug regarding "
                         "optimized spir-v code. Setting optimizations to 0 as a fix");
        args.Append(Graph::ShaderArgument::Create(Graph::ShaderArgument_Optimization, 0));
    }
    else
        args.Append(Graph::ShaderArgument::Create(Graph::ShaderArgument_Optimization, 3));

    if (Graph::IsValidationEnabled())
        args.Append(Graph::ShaderArgument::Create(Graph::ShaderArgument_DebugInformation));

    TKit::StaticArray<const char *, 2> searchPaths{};
    searchPaths.Append(ONYX_SHADERS_PATH);
#    ifdef ONYX_ENABLE_HOOKS
    if (hopts && !hopts->SearchPath.IsEmpty())
        searchPaths.Append(hopts->SearchPath.GetData());
#    endif

    const TKit::FixedArray<TKit::TierString, D_Count> dimNames = {"2D", "3D"};
    const TKit::FixedArray<TKit::TierString, Geometry_Count> geoNames = {"circle", "static", "parametric", "dynamic",
                                                                         "glyph"};
    const TKit::FixedArray<TKit::TierString, RenderPass_Count> passNames = {"flat", "shaded", "shadow"};
    constexpr TKit::FixedArray<const char *, StandalonePass_Count> stndNames = {"ray-march", "blend", "outline", "fxaa",
                                                                                "compositor"};

    constexpr Graph::EntryPoint vsEntry = {"mainVS", Graph::ShaderStageFlag_Vertex};
    constexpr Graph::EntryPoint fsoEntry = {"mainFSO", Graph::ShaderStageFlag_Fragment};
    constexpr Graph::EntryPoint fstEntry = {"mainFST", Graph::ShaderStageFlag_Fragment};
    TKit::StaticArray<Graph::EntryPoint, 3> geometryEntries;
#    ifdef ONYX_ENABLE_HOOKS
    if (hopts)
    {
        if (hopts->Stage == HookStage_Vertex)
            geometryEntries.Append(vsEntry);
        else
        {
            geometryEntries.Append(fsoEntry);
            geometryEntries.Append(fstEntry);
        }
    }
    else
#    endif
    {
        geometryEntries.Append(vsEntry);
        geometryEntries.Append(fsoEntry);
        geometryEntries.Append(fstEntry);
    }

    constexpr Graph::EntryPoint fullVertexEntry = {"mainVS", Graph::ShaderStageFlag_Vertex};
    constexpr Graph::EntryPoint rayMarchEntry = {"main", Graph::ShaderStageFlag_Compute};
    constexpr Graph::EntryPoint fragmentEntry = {"mainFS", Graph::ShaderStageFlag_Fragment};

    // module names and the module array must not reallocate, since the compilation specs point into them
    constexpr u32 combinations = u32(Geometry_Count) * u32(RenderPass_Count) * u32(D_Count);
    TKit::FixedArray<TKit::TierString, combinations> names{};
    // one extra for the full vertex pass, and another extra for possible hook module
    TKit::FixedArray<Graph::ShaderModule, combinations + StandalonePass_Count + 2> modules{};

    u32 index = 0;
    for (u32 geo = 0; geo < Geometry_Count; ++geo)
        for (u32 rpass = 0; rpass < RenderPass_Count; ++rpass)
            for (u32 dim = 0; dim < D_Count; ++dim)
                if (opts.Geometries[geo] && opts.RenderPasses[rpass] && opts.Dimensions[dim])
                {
                    TKit::TierString &name = names[index];
                    Graph::ShaderModule &module = modules[index++];

                    name = geoNames[geo] + "-" + passNames[rpass] + "-" + dimNames[dim];
#    ifdef ONYX_ENABLE_HOOKS
                    if (hopts)
                        module = Graph::ShaderModule::Create(name.CString(), geometryEntries);
                    else
#    endif
                    {
                        const u32 count = rpass != RenderPass_Shadow ? 3 : 2;
                        module = Graph::ShaderModule::Create(
                            name.CString(), TKit::Span<const Graph::EntryPoint>{geometryEntries.GetData(), count});
                    }
                }

    constexpr TKit::FixedArray<Graph::EntryPoint, StandalonePass_Count> stndEntries = {
        rayMarchEntry, fragmentEntry, fragmentEntry, fragmentEntry, fragmentEntry};

    const bool fullVertexShaderMissing = s_LayoutData->FullPassVertexShader == Graph::NullHandle;
#    ifdef ONYX_ENABLE_HOOKS
    if (!hopts)
#    endif
    {
        if (fullVertexShaderMissing)
            modules[index++] = Graph::ShaderModule::Create("full-vertex", fullVertexEntry);
        for (u32 pass = 0; pass < StandalonePass_Count; ++pass)
            if (opts.Standalone[pass])
                modules[index++] = Graph::ShaderModule::Create(stndNames[pass], stndEntries[pass]);
    }

    Graph::CompilationSpecs cspecs{};
    cspecs.Modules = {modules.GetData(), index};
    cspecs.Arguments = args;
    cspecs.SearchPaths = searchPaths;
#    ifdef ONYX_ENABLE_HOOKS

    TKit::StaticArray<Graph::ShaderMacro, 6> hookMacros{};
    TKit::StackString globalsSize;
    TKit::StackString instanceSize;

    TKit::StaticArray<Graph::ShaderModule, 2> hookModules{};
    TKit::TierString hookTypeSource{};
    const TKit::TierString userSource = TKit::TierString::Format("import onyx_hook_types;\n#line 1\n{}", hopts->Source);

    if (hopts)
    {
        const auto &sp2 = GetSpecs().GetSpecific<D2>();
        const auto &sp3 = GetSpecs().GetSpecific<D3>();
        const bool d2 = opts.Dimensions[Dim2];

        const TKit::Span<const HookOutput> outputs = d2 ? sp2.HookOutputs : sp3.HookOutputs;
        if (d2)
            hookTypeSource = generateHookTypes<D2>(hopts->Varyings, outputs, ONYX_ENGINE_TARGET_COUNT);
        else
            hookTypeSource = generateHookTypes<D3>(hopts->Varyings, outputs, ONYX_ENGINE_TARGET_COUNT);
        hookModules.Append(Graph::ShaderModule::Create("onyx_hook_types", hookTypeSource.CString()));

        hookMacros.Append("ONYX_ENABLE_HOOKS");
        if (hopts->Stage == HookStage_Vertex)
            hookMacros.Append("ONYX_VERTEX_HOOK");
        else
            hookMacros.Append("ONYX_FRAGMENT_HOOK");
        const u32 gsize = d2 ? sp2.HookGlobalsSize : sp3.HookGlobalsSize;
        const u32 isize = d2 ? sp2.HookInstanceSize : sp3.HookInstanceSize;
        if (gsize != 0)
        {
            globalsSize = std::to_string(gsize);
            hookMacros.Append("ONYX_HOOK_GLOBALS_SIZE", globalsSize.CString());
        }
        if (isize != 0)
        {
            instanceSize = std::to_string(isize);
            hookMacros.Append("ONYX_HOOK_INSTANCE_SIZE", instanceSize.CString());
        }
        if (!outputs.IsEmpty())
            hookMacros.Append("ONYX_HOOK_OUTPUTS");
        if (!hopts->Varyings.IsEmpty())
            hookMacros.Append("ONYX_HOOK_VARYINGS");

        hookModules.Append(Graph::ShaderModule::Create("onyx_hook", userSource.CString()));
        cspecs.Macros = hookMacros;
        cspecs.Libraries = hookModules;
    }
#    endif

    const auto res = Graph::Compilation_Create(cspecs);
    ONYX_RETURN_ERROR_IF(!res, Error_CompilationFailed, "[ONYX][RENDERER] Failed to compile shaders with error: {}",
                         res.GetError());

    const Graph::Compilation cmp = *res;

    const auto replaceShader = [&](Graph::Shader &toReplace, const Graph::Shader replacement) {
        if (toReplace != Graph::NullHandle)
            Graph::Shader_Destroy(toReplace);
        toReplace = replacement;
    };

    index = 0;
    const auto createShader = [&](const char *entry, const TKit::StringView name) {
        return Graph::Compilation_CreateShader(cmp, entry, name.GetData());
    };
    const auto updateShaders = [&]<Dimension D>(const Geometry geo, const RenderPass rpass) {
        const bool isShadow = rpass == RenderPass_Shadow;
        ShaderData &data = getShaders<D>(rpass);

        const TKit::TierString &name = names[index++];

#    ifdef ONYX_ENABLE_HOOKS
        if (hopts)
        {
            const auto setShaderIfNull = [&](Graph::Shader &toReplace, const char *entry) {
                if (toReplace == Graph::NullHandle)
                    toReplace = createShader(entry, name);
            };
            if (hopts->Stage == HookStage_Vertex)
                setShaderIfNull(hopts->VertexOrOpaque->At(rpass)[geo], "mainVS");
            else
            {
                setShaderIfNull(hopts->VertexOrOpaque->At(rpass)[geo], "mainFSO");
                setShaderIfNull(hopts->Transparent->At(rpass)[geo], "mainFST");
            }
        }
        else
#    endif
        {
            replaceShader(data.VertexShaders[geo], createShader("mainVS", name));
            replaceShader(data.OpaqueFragmentShaders[geo], createShader("mainFSO", name));
            if (!isShadow)
                replaceShader(data.TransparentFragmentShaders[geo], createShader("mainFST", name));

            if (Graph::IsValidationEnabled())
            {
                const TKit::StackString v = "onyx-vertex-shader-" + name;
                const TKit::StackString of = "onyx-opaque-fragment-shader-" + name;
                Graph::Shader_SetName(data.VertexShaders[geo], v.CString());
                Graph::Shader_SetName(data.OpaqueFragmentShaders[geo], of.CString());
                if (!isShadow)
                {
                    const TKit::StackString tf = "onyx-transparent-fragment-shader-" + name;
                    Graph::Shader_SetName(data.TransparentFragmentShaders[geo], tf.CString());
                }
            }
        }
    };

    const auto clearPipeline = [](Graph::Pipeline &pip) {
        if (pip != Graph::NullHandle)
        {
            Graph::DeviceWaitIdle();
            Graph::Pipeline_Destroy(pip);
            pip = Graph::NullHandle;
        }
    };

    const auto removeStalePipelines = [&]<Dimension D>(const Geometry geo, const RenderPass rpass) {
        const bool isShadow = rpass == RenderPass_Shadow;
        RendererData<D> &rdata = getRendererData<D>();
        if (isShadow)
        {
            ShadowPipelineFieldMap<Graph::Pipeline, D> &pips = rdata.Shadows.Pipelines[geo];
            // pretty much all pipelines in the case of no hook
            RenderState mask = FillMode_Mask;
            RenderState value = FillMode_Shaded;
#    ifdef ONYX_ENABLE_HOOKS
            if (hopts)
            {
                mask |= VertexHook_Mask;
                value |= hopts->HookBitField;
            }
#    endif
            pips.Query(value, mask, [&](Graph::Pipeline &pip) { clearPipeline(pip); });
        }
        else
        {
            GeometryPipelineFieldMap<Graph::Pipeline, D> &pips = rdata.Pipelines[geo];
            RenderState mask = FillMode_Mask;
            RenderState value = rpass == RenderPass_Shaded ? FillMode_Shaded : FillMode_Flat;
#    ifdef ONYX_ENABLE_HOOKS
            if (hopts)
            {
                const bool isVertex = hopts->Stage == HookStage_Vertex;
                mask |= isVertex ? VertexHook_Mask : FragmentHook_Mask;
                value |= hopts->HookBitField;
            }
#    endif
            pips.Query(value, mask, [&](Graph::Pipeline &pip) { clearPipeline(pip); });
        }
    };

    // we create/update shaders
    for (u32 geo = 0; geo < Geometry_Count; ++geo)
        for (u32 rpass = 0; rpass < RenderPass_Count; ++rpass)
            if (opts.Geometries[geo] && opts.RenderPasses[rpass])
            {
                if (opts.Dimensions[Dim2])
                    updateShaders.template operator()<D2>(Geometry(geo), RenderPass(rpass));
                if (opts.Dimensions[Dim3])
                    updateShaders.template operator()<D3>(Geometry(geo), RenderPass(rpass));
            }
    // we then recreate pipelines
    for (u32 geo = 0; geo < Geometry_Count; ++geo)
        for (u32 rpass = 0; rpass < RenderPass_Count; ++rpass)
            if (opts.Geometries[geo] && opts.RenderPasses[rpass])
            {
                if (opts.Dimensions[Dim2])
                    removeStalePipelines.template operator()<D2>(Geometry(geo), RenderPass(rpass));
                if (opts.Dimensions[Dim3])
                    removeStalePipelines.template operator()<D3>(Geometry(geo), RenderPass(rpass));
            }

    if (fullVertexShaderMissing)
        s_LayoutData->FullPassVertexShader = createShader("mainVS", "full-vertex");

#    ifdef ONYX_ENABLE_HOOKS
    if (!hopts)
#    endif
        for (u32 pass = 0; pass < StandalonePass_Count; ++pass)
            if (opts.Standalone[pass])
            {
                replaceShader(s_LayoutData->Standalone[pass].Shader,
                              createShader(stndEntries[pass].Name, stndNames[pass]));

                Graph::Pipeline &pip = s_StandalonePipelines[pass];
                if (pip != Graph::NullHandle)
                {
                    Graph::Pipeline_Destroy(pip);
                    pip = Graph::NullHandle;
                }
            }

    Graph::Compilation_Destroy(cmp);
    return Result<>::Ok();
}
#endif

static void createShaders()
{
#ifdef ONYX_COMPILE_SHADERS_AT_RUNTIME
    ONYX_CHECK_RESULT(Renderer_CompileShaders(ShaderCompileOptions::Full()));
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

template <Dimension D> static Graph::Pipeline createGeometryPipeline(const Geometry geo, const RenderState state)
{
    constexpr Graph::Format f1u = Graph::Format_R32_UINT;
    constexpr Graph::Format f2 = Graph::Format_R32G32_SFLOAT;
    constexpr Graph::Format f3 = Graph::Format_R32G32B32_SFLOAT;
    constexpr Graph::Format f4 = Graph::Format_R32G32B32A32_SFLOAT;

    const bool outlined = (state & OutlineMode_Mask) == OutlineMode_Enabled;
    const bool shaded = (state & FillMode_Mask) == FillMode_Shaded;
    const RenderPass rpass = shaded ? RenderPass_Shaded : RenderPass_Flat;

    const bool opaque = (state & BlendMode_Mask) == BlendMode_None;
    const bool opaqueParams = opaque && geo == Geometry_Glyph;
    const bool needsConstant = D == D2 && shaded;

    Graph::Shader vs = Graph::NullHandle;
    Graph::Shader fs = Graph::NullHandle;
#ifdef ONYX_ENABLE_HOOKS
    const HookShaders hsh = Hook_GetFromRenderState<D>(rpass, geo, state);
    vs = hsh.Vertex;
    fs = opaque ? hsh.Opaque : hsh.Transparent;
#endif
    const ShaderData &shaders = getShaders<D>(rpass);
    if (vs == Graph::NullHandle)
        vs = shaders.VertexShaders[geo];
    if (fs == Graph::NullHandle)
        fs = opaque ? shaders.OpaqueFragmentShaders[geo] : shaders.TransparentFragmentShaders[geo];

    const u32 depth2 = u32(isDepth2DSupported());
    const Graph::SpecializationEntry entry{0, 0, sizeof(u32)};

    Graph::ShaderStageInfo stages[2]{};
    stages[0].Shader = vs;
    stages[0].Stage = Graph::ShaderStageFlag_Vertex;
    stages[1].Shader = fs;
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
    const Graph::ColorWriteMask mainMask = outlined ? 0 : full;
    const Graph::ColorWriteMask outlineMask = outlined ? full : 0;

    TKit::StaticArray<Graph::ColorAttachment, 3 + ONYX_MAX_HOOK_OUTPUTS> attachments{};

    Graph::ColorAttachment &color = attachments.Append();
    color.BlendEnabled = !opaque || geo == Geometry_Glyph;
    color.SrcColor = opaqueParams ? Graph::BlendFactor_SrcAlpha : Graph::BlendFactor_One;
    color.DstColor = opaqueParams ? Graph::BlendFactor_OneMinusSrcAlpha : Graph::BlendFactor_One;
    color.SrcAlpha = Graph::BlendFactor_One;
    color.DstAlpha = opaqueParams ? Graph::BlendFactor_OneMinusSrcAlpha : Graph::BlendFactor_One;
    color.WriteMask = mainMask;
    color.Format = opaque ? cf : tf;

    Graph::ColorAttachment &outline = attachments.Append();
    outline.WriteMask = outlineMask;
    outline.Format = cf;

    if (!opaque)
    {
        Graph::ColorAttachment &revealage = attachments.Append();
        revealage.BlendEnabled = true;
        revealage.SrcColor = Graph::BlendFactor_Zero;
        revealage.DstColor = Graph::BlendFactor_OneMinusSrcColor;
        revealage.WriteMask = mainMask;
        revealage.Format = rf;
    }

#ifdef ONYX_ENABLE_HOOKS
    const TKit::StaticArray<HookOutput, ONYX_MAX_HOOK_OUTPUTS> &houtputs = GetSpecs().GetSpecific<D>().HookOutputs;
    if (!houtputs.IsEmpty())
    {
        if (opaque)
        {
            Graph::ColorAttachment &empty = attachments.Append();
            empty.WriteMask = 0;
            empty.Format = Graph::Format_Undefined;
        }

        for (const HookOutput &hout : houtputs)
        {
            Graph::ColorAttachment &att = attachments.Append(color);
            att.Format = hout.Format;
            att.BlendEnabled = false;
            if (fs != hsh.Opaque && fs != hsh.Transparent)
                att.WriteMask = 0;
        }
    }
#endif

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
            if (shaded)
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
            if (shaded)
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
            if (shaded)
            {
                attributes.Append(vertexAttribute(f3, offsetof(DynamicVertex<D3>, Normal)));
                attributes.Append(vertexAttribute(f4, offsetof(DynamicVertex<D3>, Tangent)));
            }
            attributes.Append(vertexAttribute(f1u, offsetof(DynamicVertex<D3>, Color)));
        }
        break;
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized geometry {}", u8(geo));
        return Graph::NullHandle;
    }

    setLocation(attributes);
    Graph::GraphicsPipelineSpecs specs{};
    specs.ShaderStages = stages;
    specs.VertexBindings = bindings;
    specs.VertexAttributes = attributes;
    specs.ColorAttachments = attachments;
    specs.DepthFormat = GetAttachmentFormat(AttachmentKind_DepthStencil);
    specs.StencilFormat = specs.DepthFormat;
    specs.Multisample.Samples = GetSpecs().SampleCount;

    const bool depth = (state & FillMode_Mask) != FillMode_None;
    specs.DepthStencil.DepthTestEnabled = depth;
    specs.DepthStencil.DepthWriteEnabled = depth && opaque;

    if constexpr (D == D2)
        if (geo != Geometry_Glyph)
        {
            if ((state & CullMode_Mask) == CullMode_Back)
                specs.Rasterization.CullMode = Graph::CullMode_Back;
            else if ((state & CullMode_Mask) == CullMode_Front)
                specs.Rasterization.CullMode = Graph::CullMode_Front;
        }

    if (outlined)
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

    const Graph::Pipeline pip = Graph::Pipeline_CreateGraphics(getPipelineLayout<D>(rpass), specs);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format(
            "onyx-renderer-geometry-pipeline-{}D-geometry-{}-state-{}", u8(D), ToString(Geometry(geo)), state);

        Graph::Pipeline_SetName(pip, name.CString());
    }

    return pip;
}

template <Dimension D> static Graph::Pipeline createShadowPipeline(const Geometry geo, const RenderState state)
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;
    Graph::Format format;
    if constexpr (D == D2)
        format = sdata.OcclusionFormat;
    else
        format = sdata.ShadowFormat;

    constexpr Graph::Format f1u = Graph::Format_R32_UINT;
    constexpr Graph::Format f2 = Graph::Format_R32G32_SFLOAT;
    constexpr Graph::Format f3 = Graph::Format_R32G32B32_SFLOAT;

    Graph::Shader vs = Graph::NullHandle;
#ifdef ONYX_ENABLE_HOOKS
    const HookShaders hsh = Hook_GetFromRenderState<D>(RenderPass_Shadow, geo, state);
    vs = hsh.Vertex;
#endif
    const ShaderData &shaders = getShaders<D>(RenderPass_Shadow);
    if (vs == Graph::NullHandle)
        vs = shaders.VertexShaders[geo];

    Graph::ShaderStageInfo stages[2]{};
    stages[0].Shader = vs;
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
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized geometry {}", u8(geo));
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
    }

    if constexpr (D == D2)
        if (geo != Geometry_Glyph)
        {
            if ((state & CullMode_Mask) == CullMode_Back)
                specs.Rasterization.CullMode = Graph::CullMode_Back;
            else if ((state & CullMode_Mask) == CullMode_Front)
                specs.Rasterization.CullMode = Graph::CullMode_Front;
        }

    const Graph::Pipeline pip = Graph::Pipeline_CreateGraphics(getPipelineLayout<D>(RenderPass_Shadow), specs);
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-shadow-pipeline-{}D-geometry-{}", u8(D), ToString(Geometry(geo)));
        Graph::Pipeline_SetName(pip, name.CString());
    }
    return pip;
}

static Graph::Pipeline createStandalonePipeline(const StandalonePass pass)
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

    Graph::Pipeline pip = Graph::NullHandle;
    switch (pass)
    {
    case StandalonePass_RayMarch: {
        const StandalonePipelineData &data = s_LayoutData->Standalone[StandalonePass_RayMarch];
        Graph::ShaderStageInfo stage{};
        stage.Shader = data.Shader;
        stage.Stage = Graph::ShaderStageFlag_Compute;
        pip = Graph::Pipeline_CreateCompute(data.Layout, stage);
        break;
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
        pip = createFullPassPipeline(attachment);
        break;
    }
    case StandalonePass_Outline:
        pip = createFullPassPipeline({.Format = GetAttachmentFormat(AttachmentKind_SrgbColor)});
        break;
    case StandalonePass_FXAA:
        pip = createFullPassPipeline({.Format = GetAttachmentFormat(AttachmentKind_LinearColor)});
        break;
    case StandalonePass_Compositor: {
        Graph::ColorAttachment attachment{};
        attachment.BlendEnabled = true;
        attachment.Format = GetAttachmentFormat(AttachmentKind_LinearColor);
        pip = createFullPassPipeline(attachment);
        break;
    }
    default:
        return Graph::NullHandle;
    }

    if (Graph::IsValidationEnabled())
        Graph::Pipeline_SetName(pip, TKit::StackString::Format("onyx-standalone-pipeline-{}", u8(pass)).CString());
    return pip;
}

template <Dimension D> static Graph::Pipeline getOrCreateGeometryPipeline(const Geometry geo, const RenderState state)
{
    RendererData<D> &rdata = getRendererData<D>();
    bool didInsert;
    Graph::Pipeline &pip = rdata.Pipelines[geo].TryInsert(&didInsert, state);
    if (didInsert || pip == Graph::NullHandle)
        pip = createGeometryPipeline<D>(geo, state);
    return pip;
}

template <Dimension D> static Graph::Pipeline getOrCreateShadowPipeline(const Geometry geo, const RenderState state)
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;
    bool didInsert;
    Graph::Pipeline &pip = sdata.Pipelines[geo].TryInsert(&didInsert, state);
    if (didInsert || pip == Graph::NullHandle)
        pip = createShadowPipeline<D>(geo, state);
    return pip;
}

static Graph::Pipeline getOrCreateStandalonePipeline(const StandalonePass pass)
{
    Graph::Pipeline &pip = s_StandalonePipelines[pass];
    if (pip == Graph::NullHandle)
        pip = createStandalonePipeline(pass);
    return pip;
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

template <Dimension D>
static void updateBufferDescriptorSets(const ResourceGroup<D> &group, const Geometry geo, const u32 binding,
                                       const Graph::Buffer buffer)
{
    if (buffer != Graph::NullHandle)
    {
        const Graph::DescriptorBufferInfo info = Graph::DescriptorBufferInfo::Create(buffer);
        for (u32 i = 0; i < RenderPass_Count; ++i)
            Graph::DescriptorSet_Write(group.Descriptors[i][geo], binding, info);
    }
}

template <Dimension D> static void updateInstanceDescriptorSets(const ResourceGroup<D> &group, const Geometry geo)
{
    const InstanceDeviceBuffer &buff = group.InstanceBuffers[geo];
    updateBufferDescriptorSets(group, geo, ONYX_GEOMETRY_PASS_INSTANCES_BINDING_POINT, buff.Buffer);
#ifdef ONYX_ENABLE_HOOKS
    updateBufferDescriptorSets(group, geo, ONYX_GEOMETRY_PASS_HOOK_INSTANCES_BINDING_POINT, buff.HooksBuffer);
#endif
}
#ifdef ONYX_ENABLE_HOOKS
template <Dimension D> static void updateHookGlobalsDescriptorSets(const ResourceGroup<D> &group, const Geometry geo)
{
    updateBufferDescriptorSets(group, geo, ONYX_GEOMETRY_PASS_HOOK_GLOBALS_BINDING_POINT, group.HookGlobalsBuffer);
}
#endif

template <Dimension D> static void updateLightDescriptorSets(const ResourceGroup<D> &group, const LightType light)
{
    if (group.LightData.Buffers[light] != Graph::NullHandle)
    {
        const Graph::DescriptorBufferInfo info = Graph::DescriptorBufferInfo::Create(group.LightData.Buffers[light]);
        for (const Graph::DescriptorSet set : group.Descriptors[RenderPass_Shaded])
            Graph::DescriptorSet_Write(set, lightToBinding(light), info);
    }
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

template <Dimension D>
static Graph::Buffer createInstanceBuffer(const Geometry geo, const u32 instances, const u32 instanceSize = 0)
{
    if (instances == 0)
        return Graph::NullHandle;
    return createBuffer(instances * (instanceSize == 0 ? GetInstanceSize<D>(geo) : instanceSize), s_InstanceBufferFlags,
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

template <Dimension D> static void destroyPipelines()
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    for (const auto &map : rdata.Pipelines)
        for (const Graph::Pipeline p : map)
            Graph::Pipeline_Destroy(p);

    for (const auto &map : sdata.Pipelines)
        for (const Graph::Pipeline p : map)
            Graph::Pipeline_Destroy(p);
}

static void destroyPipelines()
{
    destroyPipelines<D2>();
    destroyPipelines<D3>();
    for (const Graph::Pipeline pip : s_StandalonePipelines)
        if (pip != Graph::NullHandle)
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
            for (u32 j = 0; j < maxMaps; ++j)
            {
                const Graph::Image img = l3.ShadowMaps[ltype].Maps.Append(
                    Graph::Image_Create(u32v2{sdata.ShadowResolutions[ltype]}, ispecs, flags));

                if (Graph::IsValidationEnabled())
                {
                    Graph::Image_SetName(
                        img, TKit::StackString::Format("onyx-texture-map-3D-{}-{}", ToString(ltype), j).CString());
                    Graph::Image_SetViewNames(
                        img, TKit::StackString::Format("onyx-texture-map-view-3D-{}-{}", ToString(ltype), j).CString());
                }
                const Graph::DescriptorImageInfo info = Graph::DescriptorImageInfo::Create(
                    Graph::Image_GetView(img, mainViewIndex), Graph::ImageLayout_ShaderReadOnly);
                writeDescriptor<D>(group, binding, info, RenderPass_Shaded, j);
            }
        }
    }
}

template <Dimension D> static ResourceGroup<D> createResourceGroup()
{
    ResourceGroup<D> group{};
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
#ifdef ONYX_ENABLE_HOOKS
        if (specs.HookInstanceSize != 0)
            group.InstanceBuffers[geo].HooksBuffer =
                createInstanceBuffer<D>(geo, specs.MaxInstances, specs.HookInstanceSize);
#endif
        updateInstanceDescriptorSets(group, geo);
    }

    createLightResources(group);
    // TODO(Isma): Define maximum vertices/indices, not instances alone
    group.VertexBuffer = createVertexBuffer<D>(specs.MaxDynamicVertices);
    group.IndexBuffer = createIndexBuffer<D>(specs.MaxDynamicIndices);
#ifdef ONYX_ENABLE_HOOKS
    if (specs.HookGlobalsSize != 0)
    {
        group.HookGlobalsBuffer =
            createBuffer(specs.HookGlobalsSize, Graph::BufferFlag_Uniform | Graph::BufferFlag_HostMapped,
                         "onyx-renderer-hook-globals-buffer-{}D", u8(D));

        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            updateHookGlobalsDescriptorSets(group, Geometry(geo));
    }
#endif

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
    {
        destroy(buff.Buffer);
#ifdef ONYX_ENABLE_HOOKS
        destroy(buff.HooksBuffer);
#endif
    }
    group.LightData.Destroy();

    destroy(group.VertexBuffer);
    destroy(group.IndexBuffer);

    destroy(group.DrawBuffer.Buffer);
    destroy(group.IndexedDrawBuffer.Buffer);
#ifdef ONYX_ENABLE_HOOKS
    destroy(group.HookGlobalsBuffer);
#endif
}

template <Dimension D> static ResourceGroup<D> *findAvailableResourceGroup()
{
    RendererData<D> &rdata = getRendererData<D>();
    ResourceGroup<D> *pending = nullptr;

    const auto reset = [](ResourceGroup<D> &group) {
        for (InstanceDeviceBuffer &buff : group.InstanceBuffers)
            buff.Ranges.Clear();

        for (auto &map : group.OpaqueRuns)
            map.Clear();
        for (auto &map : group.TransparentRuns)
            map.Clear();
        for (auto &map : group.ShadowRuns)
            map.Clear();
        group.TransparentViews = 0;

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

    for (Graph::Pipeline &pip : s_StandalonePipelines)
        pip = Graph::NullHandle;

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

static Geometry getGeometry(const PooledMesh pm)
{
    switch (pm)
    {
    case PooledMesh_Static:
        return Geometry_Static;
    case PooledMesh_Parametric:
        return Geometry_Parametric;
    case PooledMesh_Glyph:
        return Geometry_Glyph;
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized pooled mesh {}", u8(pm));
        return Geometry_Count;
    }
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
    default:
        TKIT_FATAL("[ONYX][RENDERER] The geometry '{}' does not have a resource type associated", ToString(geo));
        return Resource_Count;
    }
}

template <Dimension D> static void transfer([[maybe_unused]] const TransferInfo &info)
{
    RendererData<D> &rdata = getRendererData<D>();
    LightData<D> &ldata = rdata.Lights;
    ldata.Clear();

    ResourceGroup<D> *group = findAvailableResourceGroup<D>();
    rdata.ActiveResourceGroup = group;

    const TKit::TierArray<RenderContext<D> *> &contexts = rdata.Contexts;
    if (contexts.IsEmpty())
        return;

#ifdef ONYX_ENABLE_HOOKS
    const HookGlobalData &hdata = info.GetHookGlobalData<D>();
    if (hdata.Size != 0 && group->HookGlobalsBuffer != Graph::NullHandle)
        Graph::Buffer_Write(group->HookGlobalsBuffer, hdata.Data, {.Size = hdata.Size});
#endif

    struct MergedContextData
    {
        ContextFieldMap<TKit::TierArray<TKit::Pair<const InstanceHostBuffer *, ViewMask>>, D> Circles{};
        ContextFieldMap<TKit::TierArray<TKit::Pair<const InstanceMeshGroup *, ViewMask>>, D> DynamicMeshes{};
        ten<ContextFieldMap<TKit::TierArray<TKit::Pair<const InstancePoolGroup *, ViewMask>>, D>, PooledMesh_Count>
            Meshes{};
    };

    MergedContextData mergedData{};
    const auto merge = [&]<typename T>(const ContextFieldMap<T, D> &src,
                                       ContextFieldMap<TKit::TierArray<TKit::Pair<const T *, ViewMask>>, D> &dst,
                                       const ViewMask vmask) {
        for (u32 i = 0; i < src.GetSize(); ++i)
            if (!src.GetValue(i).IsEmpty())
            {
                const RenderState state = src.GetKey(i);
                dst.TryInsert(state).Append(&src.GetValue(i), vmask);
            }
    };

    for (RenderContext<D> *ctx : contexts)
    {
        const ContextInstanceData<D> &idata = ctx->GetInstanceData();
        merge(idata.Circles, mergedData.Circles, ctx->ViewMask);
        merge(idata.DynamicMeshes, mergedData.DynamicMeshes, ctx->ViewMask);
        for (u32 pm = 0; pm < PooledMesh_Count; ++pm)
            merge(idata.Meshes[pm], mergedData.Meshes[pm], ctx->ViewMask);
    }

    TKit::FixedArray<u32, Geometry_Count> ioffsets{};

    u32 vertexOffset = 0;
    u32 firstIndex = 0;

    [[maybe_unused]] const SpecificSpecs<D> &specs = GetSpecs().GetSpecific<D>();
    const auto copyInstanceData = [&](const Geometry geo, const RenderState state, const InstanceHostBuffer &idata,
                                      const ViewMask vmask, const Resource handle = NullHandle) {
        // this actually can happen to circles
        // TKIT_ASSERT(idata.Instances != 0,
        //             "[ONYX][RENDERER] Found an instance data buffer to be copied with no instances. Context should be
        //             " "responsible of excluding empty buffers, not renderer");

        InstanceDeviceBuffer &buff = group->InstanceBuffers[geo];

        TKIT_ASSERT(ioffsets[geo] + idata.Instances <= specs.MaxInstances,
                    "[ONYX][RENDERER] This frame draws at least {} {}D instances of geometry '{}', but "
                    ".Specs{}.MaxInstances is {}. Raise it in the specs given to Onyx::Initialize",
                    ioffsets[geo] + idata.Instances, u8(D), ToString(geo), u8(D), specs.MaxInstances);

        InstanceRange &range = buff.Ranges.Append();
        range.RenderState = state;
        range.FirstInstance = ioffsets[geo];
        range.InstanceCount = idata.Instances;
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
                        vertexOffset + data.Vertices.GetSize(), u8(D), firstIndex + data.Indices.GetSize(), u8(D),
                        specs.MaxDynamicVertices, specs.MaxDynamicIndices);

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
        const u32 isize = GetInstanceSize<D>(geo);
#ifdef ONYX_ENABLE_HOOKS
        if (buff.HooksBuffer != Graph::NullHandle)
        {
            const u32 hisize = specs.HookInstanceSize;
            Graph::Buffer_Write(buff.HooksBuffer, idata.HookData.GetData(),
                                {.Size = range.InstanceCount * hisize, .DstOffset = range.FirstInstance * hisize});
        }
#endif
        Graph::Buffer_Write(buff.Buffer, idata.Data.GetData(),
                            {.Size = range.InstanceCount * isize, .DstOffset = range.FirstInstance * isize});
        ioffsets[geo] += range.InstanceCount;
    };

    const auto addRangeSpan = [&](const Geometry geo, const RenderState state, const ViewMask vmask,
                                  const RangeSpan &span) {
        if (span.Start == span.End)
            return;

        const auto updateRun = [&](PipelineRun &run) {
            run.Spans.Append(span);
            run.ViewMask |= vmask;
            run.CommandCount += span.End - span.Start;
        };

        const RenderState fill = state & FillMode_Mask;
        const RenderState outline = state & OutlineMode_Mask;
        const RenderState blend = state & BlendMode_Mask;

        if (fill != FillMode_None)
        {
            const RenderState onlyFill = state & ~OutlineMode_Mask;
            const Graph::Pipeline gpip = getOrCreateGeometryPipeline<D>(geo, onlyFill);

            const bool shaded = fill == FillMode_Shaded;
            const RenderPass rpass = shaded ? RenderPass_Shaded : RenderPass_Flat;

            if (blend == BlendMode_OIT)
            {
                updateRun(group->TransparentRuns[geo].TryInsert(onlyFill, gpip, rpass));
                group->TransparentViews |= vmask;
            }
            else
                updateRun(group->OpaqueRuns[geo].TryInsert(onlyFill, gpip, rpass));

            if (shaded)
            {
                const Graph::Pipeline spip = getOrCreateShadowPipeline<D>(geo, onlyFill);
                updateRun(group->ShadowRuns[geo].TryInsert(onlyFill, spip, RenderPass_Shadow));
            }
        }
        if (outline == OutlineMode_Enabled)
        {
            const RenderState onlyOutline = state & ~(FillMode_Mask | CullMode_Mask | BlendMode_Mask);
            const Graph::Pipeline opip = getOrCreateGeometryPipeline<D>(geo, onlyOutline);
            updateRun(group->OpaqueRuns[geo].TryInsert(onlyOutline, opip, RenderPass_Flat));
        }
    };

    const auto createSpan = [&](const Geometry geo) {
        RangeSpan memRangeSpan;
        memRangeSpan.Start = group->InstanceBuffers[geo].Ranges.GetSize();
        memRangeSpan.End = memRangeSpan.Start;
        return memRangeSpan;
    };

    for (u32 i = 0; i < mergedData.Circles.GetSize(); ++i)
    {
        const RenderState state = mergedData.Circles.GetKey(i);

        RangeSpan memRangeSpan = createSpan(Geometry_Circle);
        ViewMask vmask = 0;
        for (const auto &pair : mergedData.Circles.GetValue(i))
        {
            copyInstanceData(Geometry_Circle, state, *pair.First, pair.Second);
            vmask |= pair.Second;
            ++memRangeSpan.End;
        }

        addRangeSpan(Geometry_Circle, state, vmask, memRangeSpan);
    }

    for (u32 i = 0; i < mergedData.DynamicMeshes.GetSize(); ++i)
    {
        const RenderState state = mergedData.DynamicMeshes.GetKey(i);

        RangeSpan memRangeSpan = createSpan(Geometry_Dynamic);
        ViewMask vmask = 0;
        for (const auto &pair : mergedData.DynamicMeshes.GetValue(i))
        {
            vmask |= pair.Second;
            const InstanceMeshGroup *mgroup = pair.First;
            for (const u32 mid : mgroup->Registry.Ids)
            {
                const Resource handle = Handle_CreateForResource(Resource_DynamicMesh, mid);
                copyInstanceData(Geometry_Dynamic, state, mgroup->Buffers[mid], pair.Second, handle);
                ++memRangeSpan.End;
            }
        }
        addRangeSpan(Geometry_Dynamic, state, vmask, memRangeSpan);
    }
    for (u32 pm = 0; pm < PooledMesh_Count; ++pm)
    {
        const auto &map = mergedData.Meshes[pm];
        const Geometry geo = getGeometry(PooledMesh(pm));

        for (u32 i = 0; i < map.GetSize(); ++i)
        {
            const RenderState state = map.GetKey(i);

            RangeSpan memRangeSpan = createSpan(geo);
            ViewMask vmask = 0;
            for (const auto &pair : map.GetValue(i))
            {
                vmask |= pair.Second;
                const InstancePoolGroup *pgroup = pair.First;
                for (const u32 pid : pgroup->Registry.Ids)
                {
                    const InstanceMeshGroup &mgroup = pgroup->ResourceGroups[pid];
                    for (const u32 mid : mgroup.Registry.Ids)
                    {
                        const Resource handle = Handle_CreateForResource(getResourceType(geo), mid, pid);
                        copyInstanceData(geo, state, mgroup.Buffers[mid], pair.Second, handle);
                        ++memRangeSpan.End;
                    }
                }
            }

            addRangeSpan(geo, state, vmask, memRangeSpan);
        }
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

void Renderer_Transfer(const TransferInfo &info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Transfer");
    transfer<D2>(info);
    transfer<D3>(info);
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

static Graph::DrawIndirectCommand createCircleCommand(const InstanceRange &range)
{
    Graph::DrawIndirectCommand cmd;
    cmd.FirstInstance = range.FirstInstance;
    cmd.InstanceCount = range.InstanceCount;
    cmd.FirstVertex = 0;
    cmd.VertexCount = 6;
    return cmd;
}

template <Dimension D> static Graph::DrawIndexedIndirectCommand createMeshCommand(const InstanceRange &range)
{
    const MeshRange rg = Mesh_GetRange<D>(range.MeshHandle);
    Graph::DrawIndexedIndirectCommand cmd;
    cmd.FirstInstance = range.FirstInstance;
    cmd.InstanceCount = range.InstanceCount;
    cmd.FirstIndex = rg.IndexOffset;
    cmd.IndexCount = rg.IndexCount;
    cmd.VertexOffset = i32(rg.VertexOffset);
    return cmd;
}

template <Dimension D> static Graph::DrawIndexedIndirectCommand createDynamicMeshCommand(const InstanceRange &range)
{
    Graph::DrawIndexedIndirectCommand cmd;
    cmd.FirstInstance = range.FirstInstance;
    cmd.InstanceCount = range.InstanceCount;
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
        att.ClearValue.Color.Float32 = f32v4{0.f};
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

template <Dimension D>
static void setupPipelineState(const Graph::CommandBuffer cmd, const RenderPass rpass, const Geometry geo,
                               const Graph::PipelineLayout playout, const Graph::Pipeline pipeline)
{
    const RendererData<D> &rdata = getRendererData<D>();
    const Graph::DescriptorSet set = rdata.ActiveResourceGroup->Descriptors[rpass][geo];

    Graph::Command_BindPipeline(cmd, pipeline);
    Graph::Command_BindDescriptorSets(cmd, playout, Graph::BindPoint_Graphics, set);
}

template <Dimension D, typename FieldDesc>
static void recordPipelineRuns(const Graph::CommandBuffer cmd, const RenderPass rpass,
                               const Graph::PipelineLayout playout,
                               const ten<TKit::StaticBitFieldMap<PipelineRun, FieldDesc>, Geometry_Count> &runs,
                               const ViewMask viewBit)
{
    RendererData<D> &rdata = getRendererData<D>();
    ResourceGroup<D> *group = rdata.ActiveResourceGroup;

    for (u32 g = 0; g < Geometry_Count; ++g)
    {
        const Geometry geo = Geometry(g);

        for (const PipelineRun &run : runs[g])
            if (rpass == run.Pass && (run.ViewMask & viewBit))
            {
                const auto forEachRange = [&](auto func) {
                    for (const RangeSpan &span : run.Spans)
                        for (u32 i = span.Start; i < span.End; ++i)
                        {
                            const InstanceRange &range = group->InstanceBuffers[geo].Ranges[i];
                            if (range.ViewMask & viewBit)
                                func(range);
                        }
                };

                setupPipelineState<D>(cmd, rpass, geo, playout, run.Pipeline);
                // could factor this condition outside the loop with some effort
                if (geo == Geometry_Circle)
                {
                    TKit::StackArray<Graph::DrawIndirectCommand> cmds{};
                    cmds.Reserve(run.CommandCount);
                    forEachRange([&](const InstanceRange &range) { cmds.Append(createCircleCommand(range)); });
                    TKIT_ASSERT(!cmds.IsEmpty(), "[ONYX][RENDERER] Renderer should not be asked to submit 0 commands");

                    DrawCommandBuffer &drawBuffer = rdata.ActiveResourceGroup->DrawBuffer;
                    const u32 cmdCount = cmds.GetSize();
                    const usz cmdSize = cmds.GetBytes();
                    const usz offset = drawBuffer.Offset;
                    TKIT_ASSERT(offset + cmdSize <=
                                    GetSpecs().GetSpecific<D>().MaxDrawCommands * sizeof(Graph::DrawIndirectCommand),
                                "[ONYX][RENDERER] This frame records more than .Specs{}.MaxDrawCommands = {} circle "
                                "draw commands. "
                                "There is one per context, blend pass, render mode and pass, in every view and shadow "
                                "map render. "
                                "Raise it in the specs given to Onyx::Initialize",
                                u8(D), GetSpecs().GetSpecific<D>().MaxDrawCommands);

                    Graph::Buffer_Write(drawBuffer.Buffer, cmds.GetData(), {.Size = cmdSize, .DstOffset = offset});
                    drawBuffer.Offset += cmdSize;

                    Graph::Command_DrawIndirect(cmd, drawBuffer.Buffer, cmdCount, offset);
                }
                else
                {
                    const ResourceType rtype = getResourceType(geo);
                    TKit::StackArray<Graph::DrawIndexedIndirectCommand> cmds{};
                    cmds.Reserve(run.CommandCount);

                    const auto copyDrawBuffer = [&]() -> DrawCommandBuffer & {
                        DrawCommandBuffer &drawBuffer = rdata.ActiveResourceGroup->IndexedDrawBuffer;
                        const usz offset = drawBuffer.Offset;
                        const usz cmdSize = cmds.GetBytes();

                        TKIT_ASSERT(
                            offset + cmdSize <=
                                GetSpecs().GetSpecific<D>().MaxDrawCommands * sizeof(Graph::DrawIndexedIndirectCommand),
                            "[ONYX][RENDERER] This frame records more than .Specs{}.MaxDrawCommands = {} mesh draw "
                            "commands. "
                            "There is one per mesh, context, blend pass, render mode and pass, in every view and "
                            "shadow map "
                            "render. Raise it in the specs given to Onyx::Initialize",
                            u8(D), GetSpecs().GetSpecific<D>().MaxDrawCommands);
                        Graph::Buffer_Write(drawBuffer.Buffer, cmds.GetData(), {.Size = cmdSize, .DstOffset = offset});
                        return drawBuffer;
                    };

                    if (geo == Geometry_Dynamic)
                    {
                        forEachRange([&](const InstanceRange &range) {
                            ONYX_CHECK_RESOURCE_IS_NOT_NULL(range.MeshHandle);
                            ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(range.MeshHandle, getResourceType(geo), D);
                            cmds.Append(createDynamicMeshCommand<D>(range));
                        });
                        const u32 cmdCount = cmds.GetSize();
                        const usz cmdSize = cmds.GetBytes();

                        Graph::Command_BindVertexBuffers(cmd, rdata.ActiveResourceGroup->VertexBuffer);
                        Graph::Command_BindIndexBuffer(cmd, rdata.ActiveResourceGroup->IndexBuffer, s_IndexType);

                        DrawCommandBuffer &drawBuffer = copyDrawBuffer();
                        Graph::Command_DrawIndexedIndirect(cmd, drawBuffer.Buffer, cmdCount, drawBuffer.Offset);
                        drawBuffer.Offset += cmdSize;
                    }
                    else
                    {
                        TKit::FixedArray<u32, ONYX_POSSIBLE_RESOURCE_POOLS> counts{};
                        TKit::FixedArray<u32, ONYX_POSSIBLE_RESOURCE_POOLS> starts{};

                        forEachRange([&](const InstanceRange &range) {
                            ONYX_CHECK_RESOURCE_IS_NOT_NULL(range.MeshHandle);
                            ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(range.MeshHandle);
                            ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(range.MeshHandle, getResourceType(geo), D);
                            ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(range.MeshHandle, getResourceType(geo), D);

                            const u32 pid = Handle_GetResourcePoolId(range.MeshHandle);
                            counts[pid]++;
                        });

                        u32 totalCount = 0;
                        for (u32 pid = 1; pid < ONYX_POSSIBLE_RESOURCE_POOLS; ++pid)
                        {
                            totalCount += counts[pid - 1];
                            starts[pid] = totalCount;
                        }
                        totalCount += counts[ONYX_POSSIBLE_RESOURCE_POOLS - 1];

                        cmds.Resize(totalCount);

                        forEachRange([&](const InstanceRange &range) {
                            const u32 pid = Handle_GetResourcePoolId(range.MeshHandle);
                            cmds[starts[pid]++] = createMeshCommand<D>(range);
                        });
                        const usz cmdSize = cmds.GetBytes();

                        DrawCommandBuffer &drawBuffer = copyDrawBuffer();
                        u32 start = 0;
                        for (u32 pid = 0; pid < ONYX_POSSIBLE_RESOURCE_POOLS; ++pid)
                            if (counts[pid] != 0)
                            {
                                bindMeshBuffers<D>(cmd, Handle_CreateForResourcePool(rtype, pid));
                                const usz offset =
                                    drawBuffer.Offset + start * sizeof(Graph::DrawIndexedIndirectCommand);
                                Graph::Command_DrawIndexedIndirect(cmd, drawBuffer.Buffer, counts[pid], offset);
                                start += counts[pid];
                            }
                        drawBuffer.Offset += cmdSize;
                    }
                }
            }
    }
}

template <Dimension D> static f32m4 unpackTransform(const PackedTransform<D> &packed)
{
    if constexpr (D == D2)
        return f32m4{f32v4{packed[0], 0.f, 0.f}, f32v4{packed[1], 0.f, 0.f}, f32v4{0.f, 0.f, 0.f, 0.f},
                     f32v4{packed[2], 0.f, 1.f}};
    else
        return f32m4{f32v4{packed[0], 0.f}, f32v4{packed[1], 0.f}, f32v4{packed[2], 0.f}, f32v4{packed[3], 1.f}};
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

            const auto drawSceneWithMap = [&](const Graph::Image map, const f32m4 &projView, const u32 viewIndex = 0) {
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
                recordPipelineRuns<D>(cmd, RenderPass_Shadow, playout, group->ShadowRuns, viewBit);
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
                    drawSceneWithMap(ocmap, Transform<D2>::Promote(pv));
                }
                else
                    drawSceneWithMap(ocmap, unpackTransform<D>(data.ProjectionView));

                endShadowTransitionLayout<D2>(cmd, ocmap);

                transitionLayout(cmd, smap, Graph::ImageLayout_General, Graph::AccessFlag_ShaderRead,
                                 Graph::AccessFlag_ShaderWrite, Graph::PipelineStageFlag_FragmentShader,
                                 Graph::PipelineStageFlag_ComputeShader);

                const Graph::DescriptorSet set = l2.RayMarchDescriptor;

                Graph::Command_BindPipeline(cmd, getOrCreateStandalonePipeline(StandalonePass_RayMarch));
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
                        drawSceneWithMap(smap, proj * view, j);
                    }
                }
                else if constexpr (isDir)
                {
                    for (u32 j = 0; j < data.CascadeCount; ++j)
                        if ((1U << j) & data.CascadeEnable)
                            drawSceneWithMap(smap, unpackTransform<D>(data.Cascades[j].ProjectionView), j);
                }
                else
                    drawSceneWithMap(smap, data.ProjectionView);
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
static void renderGeometry(const Graph::CommandBuffer cmd, const ViewInfo<D> &vinfo,
                           const ten<GeometryPipelineFieldMap<PipelineRun, D>, Geometry_Count> &runs,
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

    LightData<D> &ldata = rdata.Lights;
    ShadowData<D> &sdata = rdata.Shadows;

    {
        const Graph::PipelineLayout playout = getPipelineLayout<D>(RenderPass_Flat);
        Graph::Command_PushConstants(cmd, playout, Graph::ShaderStageFlag_Vertex, &vinfo.ProjectionView);
        recordPipelineRuns<D>(cmd, RenderPass_Flat, playout, runs, vinfo.ViewBit);
    }

    {
        const Graph::PipelineLayout playout = getPipelineLayout<D>(RenderPass_Shaded);
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
        recordPipelineRuns<D>(cmd, RenderPass_Shaded, playout, runs, vinfo.ViewBit);
    }
}

template <Dimension D>
static void renderViews(const Graph::CommandBuffer cmd, const TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &views)
{
    RendererData<D> &rdata = getRendererData<D>();
    ResourceGroup<D> *group = rdata.ActiveResourceGroup;
    const Graph::Tracker &tracker = group->Tracker;

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
        const ViewMask vbit = rv->GetViewBit();
        if (shadows)
            renderShadows<D>(cmd, vbit);

        rv->CacheMatrices();
        const ViewInfo<D> vinfo = rv->CreateViewInfo();

        rv->MarkCurrentAttachmentsInUse(tracker);

        const bool transparency = group->TransparentViews & vbit;

        const ShadedFlags shFlags = shadows ? ShadedFlags(ShadedFlag_Shadows) : ShadedFlags(0);
        rv->BeginOpaquePass(cmd, transparency);
        renderGeometry<D>(cmd, vinfo, group->OpaqueRuns, shFlags);
        rv->EndOpaquePass(cmd, transparency);
        if (transparency)
        {
            rv->BeginTransparentPass(cmd);
            renderGeometry<D>(cmd, vinfo, group->TransparentRuns, shFlags);
            rv->EndTransparentPass(cmd);
            rv->BeginBlendPass(cmd);
            Graph::Command_BindPipeline(cmd, getOrCreateStandalonePipeline(StandalonePass_Blend));

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
            Graph::Command_BindPipeline(cmd, getOrCreateStandalonePipeline(StandalonePass_Outline));

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
            Graph::Command_BindPipeline(cmd, getOrCreateStandalonePipeline(StandalonePass_FXAA));

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

    Graph::Command_BindPipeline(cmd, getOrCreateStandalonePipeline(StandalonePass_Compositor));

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
