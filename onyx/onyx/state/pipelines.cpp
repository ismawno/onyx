#include "onyx/core/pch.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/property/vertex.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/platform/platform.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/preprocessor/utils.hpp"

namespace Onyx::Pipelines
{
constexpr u32 Dim2 = 0;
constexpr u32 Dim3 = 1;

struct ShaderData
{
    TKit::FixedArray<VKit::Shader, Geometry_Count> VertexShaders{};
    TKit::FixedArray<VKit::Shader, Geometry_Count> FragmentShaders{};

    void Destroy()
    {
        for (VKit::Shader &sh : VertexShaders)
            sh.Destroy();
        for (VKit::Shader &sh : FragmentShaders)
            sh.Destroy();
    }
};

struct PipelineData
{
    TKit::FixedArray<TKit::FixedArray<VKit::PipelineLayout, RenderPass_Count>, D_Count> Layouts{};
    TKit::FixedArray<TKit::FixedArray<ShaderData, RenderPass_Count>, D_Count> Shaders{};
    VKit::PipelineLayout DistanceLayout{};
    VKit::Shader DistanceComputeShader{};

    VKit::PipelineLayout PostProcessLayout{};
    VKit::Shader PostProcessVertexShader{};
    VKit::Shader PostProcessFragmentShader{};

    VKit::PipelineLayout CompositorLayout{};
    VKit::Shader CompositorVertexShader{};
    VKit::Shader CompositorFragmentShader{};
};

static TKit::Storage<PipelineData> s_PipelineData{};

template <Dimension D> ShaderData &getShaders(const RenderPass pass)
{
    return s_PipelineData->Shaders[D - 2][pass];
}

static void createPipelineLayouts()
{
    const VKit::DescriptorSetLayout &shaded2 = Descriptors::GetDescriptorLayout<D2>(RenderPass_Shaded);
    const VKit::DescriptorSetLayout &shaded3 = Descriptors::GetDescriptorLayout<D3>(RenderPass_Shaded);

    const VKit::DescriptorSetLayout &flat2 = Descriptors::GetDescriptorLayout<D2>(RenderPass_Flat);
    const VKit::DescriptorSetLayout &flat3 = Descriptors::GetDescriptorLayout<D3>(RenderPass_Flat);

    const VKit::DescriptorSetLayout &shadow2 = Descriptors::GetDescriptorLayout<D2>(RenderPass_Shadow);
    const VKit::DescriptorSetLayout &shadow3 = Descriptors::GetDescriptorLayout<D3>(RenderPass_Shadow);

    const VKit::LogicalDevice &device = GetDevice();

    s_PipelineData->Layouts[Dim2][RenderPass_Shaded] = ONYX_CHECK_EXPRESSION(
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(shaded2)
            .AddPushConstantRange<ShadedPushConstantData<D2>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build());

    s_PipelineData->Layouts[Dim3][RenderPass_Shaded] = ONYX_CHECK_EXPRESSION(
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(shaded3)
            .AddPushConstantRange<ShadedPushConstantData<D3>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build());

    s_PipelineData->Layouts[Dim2][RenderPass_Flat] =
        ONYX_CHECK_EXPRESSION(VKit::PipelineLayout::Builder(device)
                                  .AddDescriptorSetLayout(flat2)
                                  .AddPushConstantRange<FlatPushConstantData>(VK_SHADER_STAGE_VERTEX_BIT)
                                  .Build());

    s_PipelineData->Layouts[Dim3][RenderPass_Flat] =
        ONYX_CHECK_EXPRESSION(VKit::PipelineLayout::Builder(device)
                                  .AddDescriptorSetLayout(flat3)
                                  .AddPushConstantRange<FlatPushConstantData>(VK_SHADER_STAGE_VERTEX_BIT)
                                  .Build());

    s_PipelineData->Layouts[Dim2][RenderPass_Shadow] =
        ONYX_CHECK_EXPRESSION(VKit::PipelineLayout::Builder(device)
                                  .AddDescriptorSetLayout(shadow2)
                                  .AddPushConstantRange<ShadowPushConstantData<D2>>(VK_SHADER_STAGE_VERTEX_BIT)
                                  .Build());

    s_PipelineData->Layouts[Dim3][RenderPass_Shadow] = ONYX_CHECK_EXPRESSION(
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(shadow3)
            .AddPushConstantRange<ShadowPushConstantData<D3>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build());

    s_PipelineData->DistanceLayout =
        ONYX_CHECK_EXPRESSION(VKit::PipelineLayout::Builder(device)
                                  .AddDescriptorSetLayout(Descriptors::GetDistanceDescriptorLayout())
                                  .AddPushConstantRange<DistancePushConstantData>(VK_SHADER_STAGE_COMPUTE_BIT)
                                  .Build());

    s_PipelineData->PostProcessLayout =
        ONYX_CHECK_EXPRESSION(VKit::PipelineLayout::Builder(device)
                                  .AddDescriptorSetLayout(Descriptors::GetPostProcessDescriptorLayout())
                                  .AddPushConstantRange<PostProcessPushConstantData>(VK_SHADER_STAGE_FRAGMENT_BIT)
                                  .Build());

    s_PipelineData->CompositorLayout =
        ONYX_CHECK_EXPRESSION(VKit::PipelineLayout::Builder(device)
                                  .AddDescriptorSetLayout(Descriptors::GetCompositorDescriptorLayout())
                                  .AddPushConstantRange<CompositorPushConstantData>(VK_SHADER_STAGE_FRAGMENT_BIT)
                                  .Build());

    if (IsDebugUtilsEnabled())
    {
        u32 i = 2;
        for (auto &dims : s_PipelineData->Layouts)
        {
            u32 j = 0;
            for (auto &passes : dims)
            {
                ONYX_CHECK_EXPRESSION(
                    passes.SetName(TKit::Format("onyx-pipeline-layout-{}D-{}", i, ToString(RenderPass(j++))).c_str()));
            }
            ++i;
        }
        ONYX_CHECK_EXPRESSION(s_PipelineData->DistanceLayout.SetName("onyx-distance-pipeline-layout"));
        ONYX_CHECK_EXPRESSION(s_PipelineData->PostProcessLayout.SetName("onyx-post-process-pipeline-layout"));
        ONYX_CHECK_EXPRESSION(s_PipelineData->CompositorLayout.SetName("onyx-compositor-pipeline-layout"));
    }
}

static bool isOldMesa()
{
    const auto &device = GetDevice();
    const VKit::PhysicalDevice *phys = device.GetInfo().PhysicalDevice;

    const VkPhysicalDeviceVulkan12Properties &props = phys->GetInfo().Properties.Vulkan12;
    if (props.driverID != VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA_KHR && props.driverID != VK_DRIVER_ID_MESA_LLVMPIPE)
        return false;

    u32 major;
    u32 minor;
    u32 patch;

#ifdef TKIT_COMPILER_MSVC
    if (sscanf_s(props.driverInfo, "Mesa %u.%u.%u", &major, &minor, &patch) != 3)
        return false;
#else
    if (std::sscanf(props.driverInfo, "Mesa %u.%u.%u", &major, &minor, &patch) != 3)
        return false;
#endif

    if (major > 25)
        return false;
    if (major == 25 && minor > 3)
        return false;
    if (major == 25 && minor == 3 && patch >= 3)
        return false;
    return true;
}

static void createShaders()
{
    const bool oldMesa = isOldMesa();
    Shaders::Compiler compiler{};
    if (oldMesa)
    {
        TKIT_LOG_WARNING("[ONYX][SHADERS] Old mesa version detected (pre-25.3.3) which contains a bug regarding "
                         "optimized spir-v code. Setting optimizations to 0 as a fix");
        compiler.AddIntegerArgument(ShaderArgument_Optimization, 0);
    }
    else
        compiler.AddIntegerArgument(ShaderArgument_Optimization, 3);
    if (IsDebugUtilsEnabled())
        compiler.AddBooleanArgument(ShaderArgument_DebugInformation);

    compiler.AddSearchPath(ONYX_ROOT_PATH "/onyx/shaders");

    const TKit::FixedArray<std::string, Geometry_Count> geos = {"circle", "static", "parametric", "glyph"};
    const TKit::FixedArray<std::string, RenderPass_Count> passes = {"shaded", "flat", "shadow"};
    const TKit::FixedArray<std::string, D_Count> dims = {"2D", "3D"};

    TKit::StackArray<std::string> names{};
    names.Reserve(u32(Geometry_Count) * u32(RenderPass_Count) * u32(D_Count));

    for (const std::string &geo : geos)
        for (const std::string &pass : passes)
            for (const std::string &dim : dims)
            {
                const std::string &name = names.Append(geo + "-" + pass + "-" + dim);
                compiler.AddModule(name.c_str())
                    .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                    .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                    .Load();
            }

    compiler.AddModule("distance")
        .DeclareEntryPoint("main", ShaderStage_Compute)
        .Load()
        .AddModule("post-process")
        .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
        .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
        .Load()
        .AddModule("compositor")
        .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
        .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
        .Load();

    Shaders::Compilation cmp = ONYX_CHECK_EXPRESSION(compiler.Compile());

    u32 idx = 0;
    const auto createShader = [&](const Geometry geo, ShaderData &data) {
        const std::string &name = names[idx++];
        data.VertexShaders[geo] = ONYX_CHECK_EXPRESSION(cmp.CreateShader("mainVS", name.c_str()));
        data.FragmentShaders[geo] = ONYX_CHECK_EXPRESSION(cmp.CreateShader("mainFS", name.c_str()));

        if (IsDebugUtilsEnabled())
        {
            const std::string v = "onyx-vertex-shader-" + name;
            const std::string f = "onyx-fragment-shader-" + name;
            ONYX_CHECK_EXPRESSION(data.VertexShaders[geo].SetName(v.c_str()));
            ONYX_CHECK_EXPRESSION(data.FragmentShaders[geo].SetName(v.c_str()));
        }
    };

    for (u32 i = 0; i < geos.GetSize(); ++i)
    {
        const Geometry geo = Geometry(i);
        for (u32 j = 0; j < passes.GetSize(); ++j)
        {
            const RenderPass rpass = RenderPass(j);
            createShader(geo, getShaders<D2>(rpass));
            createShader(geo, getShaders<D3>(rpass));
        }
    }

    s_PipelineData->DistanceComputeShader = ONYX_CHECK_EXPRESSION(cmp.CreateShader("main", "distance"));

    s_PipelineData->PostProcessVertexShader = ONYX_CHECK_EXPRESSION(cmp.CreateShader("mainVS", "post-process"));
    s_PipelineData->PostProcessFragmentShader = ONYX_CHECK_EXPRESSION(cmp.CreateShader("mainFS", "post-process"));

    s_PipelineData->CompositorVertexShader = ONYX_CHECK_EXPRESSION(cmp.CreateShader("mainVS", "compositor"));
    s_PipelineData->CompositorFragmentShader = ONYX_CHECK_EXPRESSION(cmp.CreateShader("mainFS", "compositor"));

    cmp.Destroy();
}

static void destroyShaders()
{
    for (auto &dims : s_PipelineData->Shaders)
        for (auto &passes : dims)
            passes.Destroy();
    s_PipelineData->DistanceComputeShader.Destroy();
    s_PipelineData->PostProcessVertexShader.Destroy();
    s_PipelineData->PostProcessFragmentShader.Destroy();
    s_PipelineData->CompositorVertexShader.Destroy();
    s_PipelineData->CompositorFragmentShader.Destroy();
}

void ReloadShaders()
{
    destroyShaders();
    return createShaders();
}

void Initialize()
{
    TKIT_LOG_INFO("[ONYX][PIPELINES] Initializing");
    s_PipelineData.Construct();
    createPipelineLayouts();
    return createShaders();
}
void Terminate()
{
    destroyShaders();
    for (auto &dims : s_PipelineData->Layouts)
        for (auto &passes : dims)
            passes.Destroy();
    s_PipelineData->DistanceLayout.Destroy();
    s_PipelineData->PostProcessLayout.Destroy();
    s_PipelineData->CompositorLayout.Destroy();

    s_PipelineData.Destruct();
}

template <Dimension D> const VKit::PipelineLayout &GetPipelineLayout(const RenderPass pass)
{
    return s_PipelineData->Layouts[D - 2][pass];
}
const VKit::PipelineLayout &GetDistancePipelineLayout()
{
    return s_PipelineData->DistanceLayout;
}
const VKit::PipelineLayout &GetPostProcessPipelineLayout()
{
    return s_PipelineData->PostProcessLayout;
}
const VKit::PipelineLayout &GetCompositorPipelineLayout()
{
    return s_PipelineData->CompositorLayout;
}

template <Dimension D>
static VKit::GraphicsPipeline::Builder createGeometryPipelineBuilder(const PipelinePass pass, const Geometry geo,
                                                                     const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const RenderPass rpass = GetRenderPass(pass);
    const ShaderData &shaders = getShaders<D>(rpass);

    const VkColorComponentFlags full =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VKit::GraphicsPipeline::Builder builder{GetDevice(), GetPipelineLayout<D>(rpass), renderInfo};
    builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .SetViewportCount(1)
        .AddShaderStage(shaders.VertexShaders[geo], VK_SHADER_STAGE_VERTEX_BIT)
        .AddShaderStage(shaders.FragmentShaders[geo], VK_SHADER_STAGE_FRAGMENT_BIT)
        .BeginColorAttachment()
        .EnableBlending()
        .SetColorWriteMask(pass == PipelinePass_Shaded ? full : 0)
        .EndColorAttachment()
        .BeginColorAttachment()
        .SetColorWriteMask(pass == PipelinePass_Flat || pass == PipelinePass_Outlined ? full : 0)
        .EndColorAttachment();

    if (pass == PipelinePass_Shaded)
        builder.EnableDepthTest().EnableDepthWrite();

    if (pass == PipelinePass_Outlined)
    {
        const auto stencilFlags = VKit::StencilOperationFlag_Front | VKit::StencilOperationFlag_Back;
        builder.EnableStencilTest()
            .SetStencilFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilPassOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilDepthFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilCompareOperation(VK_COMPARE_OP_ALWAYS, stencilFlags)
            .SetStencilCompareMask(0xFF, stencilFlags)
            .SetStencilWriteMask(0xFF, stencilFlags)
            .SetStencilReference(1, stencilFlags);
    }

    return builder;
}

template <Dimension D>
static VKit::GraphicsPipeline createGeometryCirclePipeline(const PipelinePass pass,
                                                           const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, Geometry_Circle, renderInfo);
    return ONYX_CHECK_EXPRESSION(builder.Bake().Build());
}
template <Dimension D>
static VKit::GraphicsPipeline createGeometryStaticMeshPipeline(const PipelinePass pass,
                                                               const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, Geometry_Static, renderInfo);

    builder.AddBindingDescription<StatVertex<D>>();
    if constexpr (D == D2)
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D2>, Position));
        if (pass == PipelinePass_Shaded)
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D2>, TexCoord));
    }
    else
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Position));
        if (pass == PipelinePass_Shaded)
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D3>, TexCoord));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Normal));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StatVertex<D3>, Tangent));
        }
    }

    return ONYX_CHECK_EXPRESSION(builder.Bake().Build());
}

template <Dimension D>
static VKit::GraphicsPipeline createGeometryParametricMeshPipeline(const PipelinePass pass,
                                                                   const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, Geometry_Parametric, renderInfo);

    builder.AddBindingDescription<ParaVertex<D>>();
    if constexpr (D == D2)
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParaVertex<D2>, Position));
        if (pass == PipelinePass_Shaded)
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParaVertex<D2>, TexCoord));
        builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParaVertex<D2>, Region));
    }
    else
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParaVertex<D3>, Position));
        if (pass == PipelinePass_Shaded)
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParaVertex<D3>, TexCoord));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParaVertex<D3>, Normal));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ParaVertex<D3>, Tangent));
        }
        else
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParaVertex<D3>, Normal));
        builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParaVertex<D3>, Region));
    }

    return ONYX_CHECK_EXPRESSION(builder.Bake().Build());
}

template <Dimension D>
static VKit::GraphicsPipeline createGeometryGlyphMeshPipeline(const PipelinePass pass,
                                                              const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, Geometry_Glyph, renderInfo);
    builder.AddBindingDescription<GlyphVertex>();
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, Position));
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, AtlasCoord));
    if (pass == PipelinePass_Shaded)
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, TexCoord));

    return ONYX_CHECK_EXPRESSION(builder.Bake().Build());
}

template <Dimension D> VKit::GraphicsPipeline CreateGeometryPipeline(const PipelinePass pass, const Geometry geo)
{
    TKit::FixedArray<VkFormat, 2> formats{Platform::GetColorFormat(), Platform::GetColorFormat()};
    VkPipelineRenderingCreateInfoKHR rinfo{};
    rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rinfo.colorAttachmentCount = 2;
    rinfo.pColorAttachmentFormats = formats.GetData();
    rinfo.depthAttachmentFormat = Platform::GetDepthStencilFormat();
    rinfo.stencilAttachmentFormat = Platform::GetDepthStencilFormat();
    switch (geo)
    {
    case Geometry_Circle:
        return createGeometryCirclePipeline<D>(pass, rinfo);
    case Geometry_Static:
        return createGeometryStaticMeshPipeline<D>(pass, rinfo);
    case Geometry_Parametric:
        return createGeometryParametricMeshPipeline<D>(pass, rinfo);
    case Geometry_Glyph:
        return createGeometryGlyphMeshPipeline<D>(pass, rinfo);
    default:
        TKIT_FATAL("[ONYX][PIPELINES] Unrecognized geometry {}", u8(geo));
        return VKit::GraphicsPipeline{};
    }
}

template <Dimension D>
static VKit::GraphicsPipeline::Builder createShadowPipelineBuilder(const Geometry geo,
                                                                   const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const ShaderData &shaders = getShaders<D>(RenderPass_Shadow);
    VKit::GraphicsPipeline::Builder builder{GetDevice(), GetPipelineLayout<D>(RenderPass_Shadow), renderInfo};
    builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .AddShaderStage(shaders.VertexShaders[geo], VK_SHADER_STAGE_VERTEX_BIT)
        .AddShaderStage(shaders.FragmentShaders[geo], VK_SHADER_STAGE_FRAGMENT_BIT)
        .SetViewportCount(1);

    if constexpr (D == D3)
        builder.AddDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS).EnableDepthTest().EnableDepthWrite();
    if constexpr (D == D2)
        builder.BeginColorAttachment().SetColorWriteMask(VK_COLOR_COMPONENT_R_BIT).EndColorAttachment();

    return builder;
}

template <Dimension D>
static VKit::GraphicsPipeline createShadowCirclePipeline(const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createShadowPipelineBuilder<D>(Geometry_Circle, renderInfo);
    return ONYX_CHECK_EXPRESSION(builder.Bake().Build());
}
template <Dimension D>
static VKit::GraphicsPipeline createShadowStaticMeshPipeline(const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createShadowPipelineBuilder<D>(Geometry_Static, renderInfo);

    builder.AddBindingDescription<StatVertex<D>>();
    if constexpr (D == D2)
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D2>, Position));
    else
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Position));

    return ONYX_CHECK_EXPRESSION(builder.Bake().Build());
}

template <Dimension D>
static VKit::GraphicsPipeline createShadowParametricMeshPipeline(const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createShadowPipelineBuilder<D>(Geometry_Parametric, renderInfo);

    builder.AddBindingDescription<ParaVertex<D>>();
    if constexpr (D == D2)
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParaVertex<D2>, Position));
        builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParaVertex<D2>, Region));
    }
    else
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParaVertex<D3>, Position));
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParaVertex<D3>, Normal));
        builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParaVertex<D3>, Region));
    }

    return ONYX_CHECK_EXPRESSION(builder.Bake().Build());
}

template <Dimension D>
static VKit::GraphicsPipeline createShadowGlyphMeshPipeline(const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createShadowPipelineBuilder<D>(Geometry_Glyph, renderInfo);
    builder.AddBindingDescription<GlyphVertex>();
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, Position));
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, AtlasCoord));
    return ONYX_CHECK_EXPRESSION(builder.Bake().Build());
}

template <Dimension D> VKit::GraphicsPipeline CreateShadowPipeline(const Geometry geo, const VkFormat format)
{
    VkPipelineRenderingCreateInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    renderInfo.colorAttachmentCount = u32(D == D2);
    if constexpr (D == D2)
        renderInfo.pColorAttachmentFormats = &format;
    else
        renderInfo.depthAttachmentFormat = format;

    switch (geo)
    {
    case Geometry_Circle:
        return createShadowCirclePipeline<D>(renderInfo);
    case Geometry_Static:
        return createShadowStaticMeshPipeline<D>(renderInfo);
    case Geometry_Parametric:
        return createShadowParametricMeshPipeline<D>(renderInfo);
    case Geometry_Glyph:
        return createShadowGlyphMeshPipeline<D>(renderInfo);
    default:
        TKIT_FATAL("[ONYX][PIPELINES] Unrecognized geometry {}", u8(geo));
        return VKit::GraphicsPipeline{};
    }
}

VKit::ComputePipeline CreateDistancePipeline()
{
    VKit::ComputePipelineSpecs specs{};
    specs.ComputeShader = s_PipelineData->DistanceComputeShader;
    specs.Layout = s_PipelineData->DistanceLayout;
    return ONYX_CHECK_EXPRESSION(VKit::ComputePipeline::Create(GetDevice(), specs));
}

VKit::GraphicsPipeline CreatePostProcessPipeline()
{
    VkPipelineRenderingCreateInfoKHR rinfo{};
    const VkFormat format = Platform::GetColorFormat();
    rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachmentFormats = &format;
    rinfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    rinfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    return ONYX_CHECK_EXPRESSION(
        VKit::GraphicsPipeline::Builder(GetDevice(), s_PipelineData->PostProcessLayout, rinfo)
            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .SetViewportCount(1)
            .AddShaderStage(s_PipelineData->PostProcessVertexShader, VK_SHADER_STAGE_VERTEX_BIT)
            .AddShaderStage(s_PipelineData->PostProcessFragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
            .BeginColorAttachment()
            .DisableBlending()
            .EndColorAttachment()
            .Bake()
            .Build());
}

VKit::GraphicsPipeline CreateCompositorPipeline()
{
    VkPipelineRenderingCreateInfoKHR rinfo{};
    const VkFormat format = Platform::GetSurfaceFormat().format;
    rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachmentFormats = &format;
    rinfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    rinfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    return ONYX_CHECK_EXPRESSION(
        VKit::GraphicsPipeline::Builder(GetDevice(), s_PipelineData->CompositorLayout, rinfo)
            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .SetViewportCount(1)
            .AddShaderStage(s_PipelineData->CompositorVertexShader, VK_SHADER_STAGE_VERTEX_BIT)
            .AddShaderStage(s_PipelineData->CompositorFragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
            .BeginColorAttachment()
            .DisableBlending()
            .EndColorAttachment()
            .Bake()
            .Build());
}

template const VKit::PipelineLayout &GetPipelineLayout<D2>(RenderPass pass);
template const VKit::PipelineLayout &GetPipelineLayout<D3>(RenderPass pass);

template VKit::GraphicsPipeline CreateGeometryPipeline<D2>(PipelinePass pass, Geometry geo);
template VKit::GraphicsPipeline CreateGeometryPipeline<D3>(PipelinePass pass, Geometry geo);
template VKit::GraphicsPipeline CreateShadowPipeline<D2>(Geometry geo, VkFormat format);
template VKit::GraphicsPipeline CreateShadowPipeline<D3>(Geometry geo, VkFormat format);

} // namespace Onyx::Pipelines
