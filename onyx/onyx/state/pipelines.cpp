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

    VKit::PipelineLayout CompositorLayout{};
    VKit::Shader CompositorVertexShader{};
    VKit::Shader CompositorFragmentShader{};
};

static TKit::Storage<PipelineData> s_PipelineData{};

template <Dimension D> ShaderData &getShaders(const RenderPass pass)
{
    return s_PipelineData->Shaders[D - 2][pass];
}

ONYX_NO_DISCARD static Result<> createPipelineLayouts()
{
    const VKit::DescriptorSetLayout &flayout2 = Descriptors::GetDescriptorLayout<D2>(RenderPass_Fill);
    const VKit::DescriptorSetLayout &flayout3 = Descriptors::GetDescriptorLayout<D3>(RenderPass_Fill);

    const VKit::DescriptorSetLayout &stlayout2 = Descriptors::GetDescriptorLayout<D2>(RenderPass_Stencil);
    const VKit::DescriptorSetLayout &stlayout3 = Descriptors::GetDescriptorLayout<D3>(RenderPass_Stencil);

    const VKit::DescriptorSetLayout &shlayout2 = Descriptors::GetDescriptorLayout<D2>(RenderPass_Shadow);
    const VKit::DescriptorSetLayout &shlayout3 = Descriptors::GetDescriptorLayout<D3>(RenderPass_Shadow);

    const VKit::LogicalDevice &device = GetDevice();
    auto layoutResult =
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(flayout2)
            .AddPushConstantRange<FillPushConstantData<D2>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_PipelineData->Layouts[Dim2][RenderPass_Fill] = layoutResult.GetValue();

    layoutResult =
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(flayout3)
            .AddPushConstantRange<FillPushConstantData<D3>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_PipelineData->Layouts[Dim3][RenderPass_Fill] = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(stlayout2)
                       .AddPushConstantRange<StencilPushConstantData>(VK_SHADER_STAGE_VERTEX_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_PipelineData->Layouts[Dim2][RenderPass_Stencil] = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(stlayout3)
                       .AddPushConstantRange<StencilPushConstantData>(VK_SHADER_STAGE_VERTEX_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_PipelineData->Layouts[Dim3][RenderPass_Stencil] = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(shlayout2)
                       .AddPushConstantRange<ShadowPushConstantData<D2>>(VK_SHADER_STAGE_VERTEX_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_PipelineData->Layouts[Dim2][RenderPass_Shadow] = layoutResult.GetValue();

    layoutResult =
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(shlayout3)
            .AddPushConstantRange<ShadowPushConstantData<D3>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_PipelineData->Layouts[Dim3][RenderPass_Shadow] = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(Descriptors::GetDistanceDescriptorLayout())
                       .AddPushConstantRange<DistancePushConstantData>(VK_SHADER_STAGE_COMPUTE_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_PipelineData->DistanceLayout = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(Descriptors::GetCompositorDescriptorLayout())
                       .AddPushConstantRange<u32>(VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_PipelineData->CompositorLayout = layoutResult.GetValue();

    if (IsDebugUtilsEnabled())
    {
        u32 i = 2;
        for (auto &dims : s_PipelineData->Layouts)
        {
            u32 j = 0;
            for (auto &passes : dims)
            {
                TKIT_RETURN_IF_FAILED(
                    passes.SetName(TKit::Format("onyx-pipeline-layout-{}D-{}", i, ToString(RenderPass(j++))).c_str()));
            }
            ++i;
        }
    }
    return Result<>::Ok();
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

ONYX_NO_DISCARD static Result<> createShaders()
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
    const TKit::FixedArray<std::string, RenderPass_Count> passes = {"fill", "stencil", "shadow"};
    const TKit::FixedArray<std::string, D_Count> dims = {"2D", "3D"};

    TKit::StackArray<std::string> names{};
    names.Reserve(u32(Geometry_Count) * u32(RenderPass_Count) * 2);

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
        .AddModule("compositor")
        .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
        .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
        .Load();

    auto cmpres = compiler.Compile();
    TKIT_RETURN_ON_ERROR(cmpres);
    Shaders::Compilation &cmp = cmpres.GetValue();

    u32 idx = 0;
    const auto createShader = [&](const Geometry geo, ShaderData &data) -> Result<> {
        const std::string &name = names[idx++];
        auto result = cmp.CreateShader("mainVS", name.c_str());
        TKIT_RETURN_ON_ERROR(result);
        data.VertexShaders[geo] = result.GetValue();

        result = cmp.CreateShader("mainFS", name.c_str());
        TKIT_RETURN_ON_ERROR(result);
        data.FragmentShaders[geo] = result.GetValue();

        if (IsDebugUtilsEnabled())
        {
            const std::string v = "onyx-vertex-shader-" + name;
            TKIT_RETURN_IF_FAILED(data.VertexShaders[geo].SetName(v.c_str()));
            const std::string f = "onyx-fragment-shader-" + name;
            return data.FragmentShaders[geo].SetName(v.c_str());
        }

        return Result<>::Ok();
    };

    for (u32 i = 0; i < geos.GetSize(); ++i)
    {
        const Geometry geo = Geometry(i);
        for (u32 j = 0; j < passes.GetSize(); ++j)
        {
            const RenderPass rpass = RenderPass(j);
            TKIT_RETURN_IF_FAILED(createShader(geo, getShaders<D2>(rpass)), cmp.Destroy());
            TKIT_RETURN_IF_FAILED(createShader(geo, getShaders<D3>(rpass)), cmp.Destroy());
        }
    }

    auto result = cmp.CreateShader("main", "distance");
    TKIT_RETURN_ON_ERROR(result, cmp.Destroy());
    s_PipelineData->DistanceComputeShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "compositor");
    TKIT_RETURN_ON_ERROR(result, cmp.Destroy());
    s_PipelineData->CompositorVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "compositor");
    TKIT_RETURN_ON_ERROR(result, cmp.Destroy());
    s_PipelineData->CompositorFragmentShader = result.GetValue();

    cmp.Destroy();
    return Result<>::Ok();
}

static void destroyShaders()
{
    for (auto &dims : s_PipelineData->Shaders)
        for (auto &passes : dims)
            passes.Destroy();
    s_PipelineData->DistanceComputeShader.Destroy();
    s_PipelineData->CompositorVertexShader.Destroy();
    s_PipelineData->CompositorFragmentShader.Destroy();
}

Result<> ReloadShaders()
{
    destroyShaders();
    return createShaders();
}

Result<> Initialize()
{
    TKIT_LOG_INFO("[ONYX][PIPELINES] Initializing");
    s_PipelineData.Construct();
    TKIT_RETURN_IF_FAILED(createPipelineLayouts());
    return createShaders();
}
void Terminate()
{
    destroyShaders();
    for (auto &dims : s_PipelineData->Layouts)
        for (auto &passes : dims)
            passes.Destroy();
    s_PipelineData->DistanceLayout.Destroy();
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
const VKit::PipelineLayout &GetCompositorPipelineLayout()
{
    return s_PipelineData->CompositorLayout;
}

template <Dimension D>
static VKit::GraphicsPipeline::Builder createGeometryPipelineBuilder(const StencilPass pass, const Geometry geo,
                                                                     const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const RenderPass rpass = GetRenderPass(pass);
    const ShaderData &shaders = getShaders<D>(rpass);

    VKit::GraphicsPipeline::Builder builder{GetDevice(), GetPipelineLayout<D>(rpass), renderInfo};
    auto &colorBuilder = builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                             .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                             .SetViewportCount(1)
                             .AddShaderStage(shaders.VertexShaders[geo], VK_SHADER_STAGE_VERTEX_BIT)
                             .AddShaderStage(shaders.FragmentShaders[geo], VK_SHADER_STAGE_FRAGMENT_BIT)
                             .BeginColorAttachment()
                             .EnableBlending();

    if (D == D2 || rpass == RenderPass_Fill)
        builder.EnableDepthTest().EnableDepthWrite();
    else
        colorBuilder.DisableBlending();

    const auto stencilFlags = VKit::StencilOperationFlag_Front | VKit::StencilOperationFlag_Back;

    if (pass == StencilPass_DoStencilWriteDoFill || pass == StencilPass_DoStencilWriteNoFill)
        builder.EnableStencilTest()
            .SetStencilFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilPassOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilDepthFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilCompareOperation(VK_COMPARE_OP_ALWAYS, stencilFlags)
            .SetStencilCompareMask(0xFF, stencilFlags)
            .SetStencilWriteMask(0xFF, stencilFlags)
            .SetStencilReference(1, stencilFlags);
    else if (pass == StencilPass_DoStencilTestNoFill)
    {
        builder.EnableStencilTest()
            .DisableDepthWrite()
            .SetStencilFailOperation(VK_STENCIL_OP_KEEP, stencilFlags)
            .SetStencilPassOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilDepthFailOperation(VK_STENCIL_OP_KEEP, stencilFlags)
            .SetStencilCompareOperation(VK_COMPARE_OP_NOT_EQUAL, stencilFlags)
            .SetStencilCompareMask(0xFF, stencilFlags)
            .SetStencilWriteMask(0, stencilFlags)
            .SetStencilReference(1, stencilFlags);
    }
    if (pass == StencilPass_DoStencilWriteNoFill)
        colorBuilder.SetColorWriteMask(0);

    colorBuilder.EndColorAttachment();

    return builder;
}

template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createGeometryCirclePipeline(
    const StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, Geometry_Circle, renderInfo);
    return builder.Bake().Build();
}
template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createGeometryStaticMeshPipeline(
    const StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, Geometry_Static, renderInfo);

    builder.AddBindingDescription<StatVertex<D>>();
    if constexpr (D == D2)
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D2>, Position));
        if (pass != StencilPass_DoStencilWriteNoFill && pass != StencilPass_DoStencilTestNoFill)
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D2>, TexCoord));
    }
    else
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Position));
        if (pass != StencilPass_DoStencilWriteNoFill && pass != StencilPass_DoStencilTestNoFill)
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D3>, TexCoord));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Normal));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StatVertex<D3>, Tangent));
        }
    }

    return builder.Bake().Build();
}

template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createGeometryParametricMeshPipeline(
    const StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, Geometry_Parametric, renderInfo);

    builder.AddBindingDescription<ParaVertex<D>>();
    if constexpr (D == D2)
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParaVertex<D2>, Position));
        if (pass != StencilPass_DoStencilWriteNoFill && pass != StencilPass_DoStencilTestNoFill)
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParaVertex<D2>, TexCoord));
        builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParaVertex<D2>, Region));
    }
    else
    {
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParaVertex<D3>, Position));
        if (pass != StencilPass_DoStencilWriteNoFill && pass != StencilPass_DoStencilTestNoFill)
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParaVertex<D3>, TexCoord));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParaVertex<D3>, Normal));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ParaVertex<D3>, Tangent));
        }
        else
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParaVertex<D3>, Normal));
        builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParaVertex<D3>, Region));
    }

    return builder.Bake().Build();
}

template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createGeometryGlyphMeshPipeline(
    const StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, Geometry_Glyph, renderInfo);
    builder.AddBindingDescription<GlyphVertex>();
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, Position));
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, AtlasCoord));
    if (pass != StencilPass_DoStencilWriteNoFill && pass != StencilPass_DoStencilTestNoFill)
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, TexCoord));

    return builder.Bake().Build();
}

template <Dimension D>
Result<VKit::GraphicsPipeline> CreateGeometryPipeline(const StencilPass pass, const Geometry geo,
                                                      const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    switch (geo)
    {
    case Geometry_Circle:
        return createGeometryCirclePipeline<D>(pass, renderInfo);
    case Geometry_Static:
        return createGeometryStaticMeshPipeline<D>(pass, renderInfo);
    case Geometry_Parametric:
        return createGeometryParametricMeshPipeline<D>(pass, renderInfo);
    case Geometry_Glyph:
        return createGeometryGlyphMeshPipeline<D>(pass, renderInfo);
    default:
        return Result<VKit::GraphicsPipeline>::Error(
            Error_BadInput, TKit::Format("[ONYX][PIPELINES] Unrecognized geometry {}", u8(geo)));
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
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createShadowCirclePipeline(
    const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createShadowPipelineBuilder<D>(Geometry_Circle, renderInfo);
    return builder.Bake().Build();
}
template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createShadowStaticMeshPipeline(
    const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createShadowPipelineBuilder<D>(Geometry_Static, renderInfo);

    builder.AddBindingDescription<StatVertex<D>>();
    if constexpr (D == D2)
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D2>, Position));
    else
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Position));

    return builder.Bake().Build();
}

template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createShadowParametricMeshPipeline(
    const VkPipelineRenderingCreateInfoKHR &renderInfo)
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

    return builder.Bake().Build();
}

template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createShadowGlyphMeshPipeline(
    const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    VKit::GraphicsPipeline::Builder builder = createShadowPipelineBuilder<D>(Geometry_Glyph, renderInfo);
    builder.AddBindingDescription<GlyphVertex>();
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, Position));
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, AtlasCoord));
    return builder.Bake().Build();
}

template <Dimension D> Result<VKit::GraphicsPipeline> CreateShadowPipeline(const Geometry geo, const VkFormat format)
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
        return Result<VKit::GraphicsPipeline>::Error(
            Error_BadInput, TKit::Format("[ONYX][PIPELINES] Unrecognized geometry {}", u8(geo)));
    }
}

Result<VKit::ComputePipeline> CreateDistancePipeline()
{
    VKit::ComputePipelineSpecs specs{};
    specs.ComputeShader = s_PipelineData->DistanceComputeShader;
    specs.Layout = s_PipelineData->DistanceLayout;
    return VKit::ComputePipeline::Create(GetDevice(), specs);
}

Result<VKit::GraphicsPipeline> CreateCompositorPipeline()
{
    VkPipelineRenderingCreateInfoKHR rinfo{};
    const VkFormat format = Platform::GetSurfaceFormat().format;
    rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachmentFormats = &format;
    rinfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    rinfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    return VKit::GraphicsPipeline::Builder(GetDevice(), s_PipelineData->CompositorLayout, rinfo)
        .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .SetViewportCount(1)
        .AddShaderStage(s_PipelineData->CompositorVertexShader, VK_SHADER_STAGE_VERTEX_BIT)
        .AddShaderStage(s_PipelineData->CompositorFragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
        .BeginColorAttachment()
        .DisableBlending()
        .EndColorAttachment()
        .Bake()
        .Build();
}

template const VKit::PipelineLayout &GetPipelineLayout<D2>(RenderPass pass);
template const VKit::PipelineLayout &GetPipelineLayout<D3>(RenderPass pass);

template Result<VKit::GraphicsPipeline> CreateGeometryPipeline<D2>(StencilPass pass, Geometry geo,
                                                                   const VkPipelineRenderingCreateInfoKHR &renderInfo);
template Result<VKit::GraphicsPipeline> CreateGeometryPipeline<D3>(StencilPass pass, Geometry geo,
                                                                   const VkPipelineRenderingCreateInfoKHR &renderInfo);
template Result<VKit::GraphicsPipeline> CreateShadowPipeline<D2>(Geometry geo, VkFormat format);
template Result<VKit::GraphicsPipeline> CreateShadowPipeline<D3>(Geometry geo, VkFormat format);

} // namespace Onyx::Pipelines
