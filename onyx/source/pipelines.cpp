#include "pch.hpp"
#include "pipelines.hpp"
#include "conversion.hpp"
#include "core.hpp"
#include "descriptors.hpp"
#include "instance.hpp"
#include "attachment.hpp"
#include "renderer.hpp"
#ifdef ONYX_COMPILE_SHADERS_ON_EXEC
#    include "shaders.hpp"
#else
#    include "vkit/state/shader.hpp"
#    include "spirv.hpp"
#endif
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
    TKit::FixedArray<VKit::Shader, Geometry_Count> VertexShaders{};
    TKit::FixedArray<VKit::Shader, Geometry_Count> OpaqueFragmentShaders{};
    TKit::FixedArray<VKit::Shader, Geometry_Count> TransparentFragmentShaders{};

    void Destroy()
    {
        for (VKit::Shader &sh : VertexShaders)
            sh.Destroy();
        for (VKit::Shader &sh : OpaqueFragmentShaders)
            sh.Destroy();
        for (VKit::Shader &sh : TransparentFragmentShaders)
            sh.Destroy();
    }
};

struct StandalonePipelineData
{
    VKit::PipelineLayout Layout{};
    VKit::Shader Shader{};
};

struct PipelineData
{
    TKit::FixedArray<TKit::FixedArray<VKit::PipelineLayout, RenderPass_Count>, D_Count> Layouts{};
    TKit::FixedArray<TKit::FixedArray<ShaderData, RenderPass_Count>, D_Count> Shaders{};

    VKit::Shader FullPassVertexShader{};
    TKit::FixedArray<StandalonePipelineData, StandalonePass_Count> Standalone{};
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

    s_PipelineData->Layouts[Dim2][RenderPass_Shaded] = ONYX_CHECK_VKIT_RESULT(
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(shaded2)
            .AddPushConstantRange<ShadedPushConstantData<D2>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build());

    s_PipelineData->Layouts[Dim3][RenderPass_Shaded] = ONYX_CHECK_VKIT_RESULT(
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(shaded3)
            .AddPushConstantRange<ShadedPushConstantData<D3>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build());

    s_PipelineData->Layouts[Dim2][RenderPass_Flat] =
        ONYX_CHECK_VKIT_RESULT(VKit::PipelineLayout::Builder(device)
                                   .AddDescriptorSetLayout(flat2)
                                   .AddPushConstantRange<FlatPushConstantData>(VK_SHADER_STAGE_VERTEX_BIT)
                                   .Build());

    s_PipelineData->Layouts[Dim3][RenderPass_Flat] =
        ONYX_CHECK_VKIT_RESULT(VKit::PipelineLayout::Builder(device)
                                   .AddDescriptorSetLayout(flat3)
                                   .AddPushConstantRange<FlatPushConstantData>(VK_SHADER_STAGE_VERTEX_BIT)
                                   .Build());

    s_PipelineData->Layouts[Dim2][RenderPass_Shadow] =
        ONYX_CHECK_VKIT_RESULT(VKit::PipelineLayout::Builder(device)
                                   .AddDescriptorSetLayout(shadow2)
                                   .AddPushConstantRange<ShadowPushConstantData<D2>>(VK_SHADER_STAGE_VERTEX_BIT)
                                   .Build());

    s_PipelineData->Layouts[Dim3][RenderPass_Shadow] = ONYX_CHECK_VKIT_RESULT(
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(shadow3)
            .AddPushConstantRange<ShadowPushConstantData<D3>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build());

    s_PipelineData->Standalone[StandalonePass_RayMarch].Layout =
        ONYX_CHECK_VKIT_RESULT(VKit::PipelineLayout::Builder(device)
                                   .AddDescriptorSetLayout(Descriptors::GetDescriptorLayout(StandalonePass_RayMarch))
                                   .AddPushConstantRange<RayMarchPushConstantData>(VK_SHADER_STAGE_COMPUTE_BIT)
                                   .Build());

    s_PipelineData->Standalone[StandalonePass_Blend].Layout =
        ONYX_CHECK_VKIT_RESULT(VKit::PipelineLayout::Builder(device)
                                   .AddDescriptorSetLayout(Descriptors::GetDescriptorLayout(StandalonePass_Blend))
                                   .AddPushConstantRange<BlendPushConstantData>(VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .Build());

    s_PipelineData->Standalone[StandalonePass_PostProcess].Layout =
        ONYX_CHECK_VKIT_RESULT(VKit::PipelineLayout::Builder(device)
                                   .AddDescriptorSetLayout(Descriptors::GetDescriptorLayout(StandalonePass_PostProcess))
                                   .AddPushConstantRange<PostProcessPushConstantData>(VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .Build());

    s_PipelineData->Standalone[StandalonePass_Compositor].Layout =
        ONYX_CHECK_VKIT_RESULT(VKit::PipelineLayout::Builder(device)
                                   .AddDescriptorSetLayout(Descriptors::GetDescriptorLayout(StandalonePass_Compositor))
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
                ONYX_CHECK_VKIT_RESULT(passes.SetName(
                    TKit::StackString::Format("onyx-pipeline-layout-{}D-{}", i, ToString(RenderPass(j++))).CString()));
            }
            ++i;
        }
        ONYX_CHECK_VKIT_RESULT(
            s_PipelineData->Standalone[StandalonePass_RayMarch].Layout.SetName("onyx-ray-march-pipeline-layout"));
        ONYX_CHECK_VKIT_RESULT(
            s_PipelineData->Standalone[StandalonePass_Blend].Layout.SetName("onyx-blend-pipeline-layout"));
        ONYX_CHECK_VKIT_RESULT(
            s_PipelineData->Standalone[StandalonePass_PostProcess].Layout.SetName("onyx-post-process-pipeline-layout"));
        ONYX_CHECK_VKIT_RESULT(
            s_PipelineData->Standalone[StandalonePass_Compositor].Layout.SetName("onyx-compositor-pipeline-layout"));
    }
}

#ifdef ONYX_ENABLE_SHADER_API
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

#    ifdef TKIT_COMPILER_MSVC
    if (sscanf_s(props.driverInfo, "Mesa %u.%u.%u", &major, &minor, &patch) != 3)
        return false;
#    else
    if (std::sscanf(props.driverInfo, "Mesa %u.%u.%u", &major, &minor, &patch) != 3)
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
static VKit::Shader shaderFromBinary(const ShaderBinary &binary)
{
    return ONYX_CHECK_VKIT_RESULT(VKit::Shader::Create(GetDevice(), rcast<const u32 *>(binary.Code), binary.Count));
}
#endif

static void createShaders()
{
#ifdef ONYX_COMPILE_SHADERS_ON_EXEC
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

    compiler.AddSearchPath(ONYX_SHADERS_PATH);

    const TKit::FixedArray<TKit::String, Geometry_Count> geos = {"circle", "static", "parametric", "glyph", "dynamic"};
    const TKit::FixedArray<TKit::String, RenderPass_Count> passes = {"flat", "shaded", "shadow"};
    const TKit::FixedArray<TKit::String, D_Count> dims = {"2D", "3D"};

    TKit::StackArray<TKit::String> names{};
    names.Reserve(u32(Geometry_Count) * u32(RenderPass_Count) * u32(D_Count));

    // TODO(Isma): Would be nice to parallelize this
    for (const TKit::String &geo : geos)
        for (u32 rpass = 0; rpass < passes.GetSize(); ++rpass)
            for (const TKit::String &dim : dims)
            {
                const TKit::String &name = names.Append(geo + "-" + passes[rpass] + "-" + dim);
                auto &module = compiler.AddModule(name.GetData())
                                   .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                                   .DeclareEntryPoint("mainFSO", ShaderStage_Fragment);
                if (rpass != RenderPass_Shadow)
                    module.DeclareEntryPoint("mainFST", ShaderStage_Fragment);
                module.Load();
            }

    compiler.AddModule("full-vertex")
        .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
        .Load()
        .AddModule("ray-march")
        .DeclareEntryPoint("main", ShaderStage_Compute)
        .Load()
        .AddModule("blend")
        .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
        .Load()
        .AddModule("post-process")
        .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
        .Load()
        .AddModule("compositor")
        .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
        .Load();

    Shaders::Compilation cmp = ONYX_CHECK_RESULT(compiler.Compile());

    u32 idx = 0;
    const auto createShader = [&](const Geometry geo, ShaderData &data, const bool hasTransparent) {
        const TKit::String &name = names[idx++];
        data.VertexShaders[geo] = ONYX_CHECK_RESULT(cmp.CreateShader("mainVS", name.GetData()));
        data.OpaqueFragmentShaders[geo] = ONYX_CHECK_RESULT(cmp.CreateShader("mainFSO", name.GetData()));
        if (hasTransparent)
            data.TransparentFragmentShaders[geo] = ONYX_CHECK_RESULT(cmp.CreateShader("mainFST", name.GetData()));

        if (IsDebugUtilsEnabled())
        {
            const TKit::String v = "onyx-vertex-shader-" + name;
            const TKit::String of = "onyx-opaque-fragment-shader-" + name;
            ONYX_CHECK_VKIT_RESULT(data.VertexShaders[geo].SetName(v.CString()));
            ONYX_CHECK_VKIT_RESULT(data.OpaqueFragmentShaders[geo].SetName(of.CString()));
            if (hasTransparent)
            {
                const TKit::String tf = "onyx-transparent-fragment-shader-" + name;
                ONYX_CHECK_VKIT_RESULT(data.TransparentFragmentShaders[geo].SetName(tf.CString()));
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

    s_PipelineData->FullPassVertexShader = ONYX_CHECK_RESULT(cmp.CreateShader("mainVS", "full-vertex"));
    s_PipelineData->Standalone[StandalonePass_RayMarch].Shader =
        ONYX_CHECK_RESULT(cmp.CreateShader("main", "ray-march"));

    s_PipelineData->Standalone[StandalonePass_Blend].Shader = ONYX_CHECK_RESULT(cmp.CreateShader("mainFS", "blend"));
    s_PipelineData->Standalone[StandalonePass_PostProcess].Shader =
        ONYX_CHECK_RESULT(cmp.CreateShader("mainFS", "post-process"));
    s_PipelineData->Standalone[StandalonePass_Compositor].Shader =
        ONYX_CHECK_RESULT(cmp.CreateShader("mainFS", "compositor"));

    cmp.Destroy();
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
    for (auto &dims : s_PipelineData->Shaders)
        for (auto &passes : dims)
            passes.Destroy();

    s_PipelineData->FullPassVertexShader.Destroy();
    for (auto &st : s_PipelineData->Standalone)
        st.Shader.Destroy();
}

void ReloadShaders()
{
    destroyShaders();
    return createShaders();
}

void Initialize()
{
    TKIT_LOG_INFO("[ONYX][PIPELINES] Initializing");
#ifndef ONYX_COMPILE_SHADERS_ON_EXEC
    InitializeBinaries();
#endif
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
    for (auto &st : s_PipelineData->Standalone)
        st.Layout.Destroy();

    s_PipelineData.Destruct();
}

template <Dimension D> const VKit::PipelineLayout &GetPipelineLayout(const RenderPass pass)
{
    return s_PipelineData->Layouts[D - 2][pass];
}
const VKit::PipelineLayout &GetPipelineLayout(const StandalonePass pass)
{
    return s_PipelineData->Standalone[pass].Layout;
}

template <Dimension D>
static VKit::GraphicsPipeline::Builder createGeometryPipelineBuilder(const PipelinePass pass, const Geometry geo,
                                                                     const VkPipelineRenderingCreateInfoKHR &renderInfo,
                                                                     VkSpecializationInfo &spInfo,
                                                                     VkSpecializationMapEntry &entry)
{
    const RenderPass rpass = GetRenderPass(pass);
    const ShaderData &shaders = getShaders<D>(rpass);

    const VkColorComponentFlags full =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    const bool needsConstant = D == D2 && pass == PipelinePass_Shaded;
    const u32 depth2 = u32(Renderer::IsDepthSupportedFor2D());
    if (needsConstant)
    {
        entry.constantID = 0;
        entry.offset = 0;
        entry.size = sizeof(u32);
        spInfo.dataSize = sizeof(u32);
        spInfo.pData = &depth2;
        spInfo.mapEntryCount = 1;
        spInfo.pMapEntries = &entry;
    }

    VKit::GraphicsPipeline::Builder builder{GetDevice(), GetPipelineLayout<D>(rpass), renderInfo};
    const bool opaque = renderInfo.colorAttachmentCount == 2;
    const bool opaqueParams = opaque && geo == Geometry_Glyph;

    const VkBlendFactor csrc = opaqueParams ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
    const VkBlendFactor cdst = opaqueParams ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE;

    const VkBlendFactor asrc = VK_BLEND_FACTOR_ONE;
    const VkBlendFactor adst = opaqueParams ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE;

    builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .SetViewportCount(1)
        .AddShaderStage(shaders.VertexShaders[geo], VK_SHADER_STAGE_VERTEX_BIT)
        .AddShaderStage(opaque ? shaders.OpaqueFragmentShaders[geo] : shaders.TransparentFragmentShaders[geo],
                        VK_SHADER_STAGE_FRAGMENT_BIT, 0, needsConstant ? &spInfo : nullptr)
        .BeginColorAttachment()
        .EnableBlending(!opaque || geo == Geometry_Glyph)
        .SetColorBlendFactors(csrc, cdst)
        .SetAlphaBlendFactors(asrc, adst)
        .SetColorWriteMask(pass != PipelinePass_Outlined ? full : 0)
        .EndColorAttachment()
        .BeginColorAttachment()
        .SetColorWriteMask(pass == PipelinePass_Outlined ? full : 0)
        .EndColorAttachment();

    if (D == D3 && geo != Geometry_Circle && geo != Geometry_Glyph)
        builder.AddDynamicState(VK_DYNAMIC_STATE_CULL_MODE_EXT);

    if (!opaque)
        builder.BeginColorAttachment()
            .EnableBlending()
            .SetColorBlendFactors(VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR)
            .SetColorWriteMask(pass != PipelinePass_Outlined ? full : 0)
            .EndColorAttachment();

    if (pass == PipelinePass_Shaded || pass == PipelinePass_Flat)
    {
        builder.EnableDepthTest();
        if (opaque)
            builder.EnableDepthWrite();
    }

    if (pass == PipelinePass_Outlined)
    {
        const auto stencilFlags = VKit::StencilOperationFlag_Front | VKit::StencilOperationFlag_Back;
        builder.EnableStencilTest()
            .SetStencilFailOperation(VK_STENCIL_OP_KEEP, stencilFlags) // irrelevant
            .SetStencilPassOperation(VK_STENCIL_OP_INCREMENT_AND_CLAMP, stencilFlags)
            .SetStencilDepthFailOperation(VK_STENCIL_OP_KEEP, stencilFlags)
            .SetStencilCompareOperation(VK_COMPARE_OP_ALWAYS, stencilFlags)
            .SetStencilCompareMask(0xFF, stencilFlags)
            .SetStencilWriteMask(0xFF, stencilFlags)
            .SetStencilReference(0, stencilFlags); // irrelevant
    }

    return builder;
}

template <Dimension D>
VKit::GraphicsPipeline CreateGeometryPipeline(const PipelinePass pass, const BlendPass bpass, const Geometry geo)
{
    const VkFormat cf = GetAttachmentFormat(Attachment_Intermediate);
    const VkFormat tf = GetAttachmentFormat(Attachment_Transparent);
    const VkFormat rf = GetAttachmentFormat(Attachment_Revealage);

    TKit::FixedArray<VkFormat, 2> opFormats{cf, cf};
    TKit::FixedArray<VkFormat, 3> trFormats{tf, cf, rf};

    VkPipelineRenderingCreateInfoKHR rinfo{};
    rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    if (bpass == BlendPass_Opaque)
    {
        rinfo.colorAttachmentCount = 2;
        rinfo.pColorAttachmentFormats = opFormats.GetData();
    }
    else
    {
        rinfo.colorAttachmentCount = 3;
        rinfo.pColorAttachmentFormats = trFormats.GetData();
    }
    rinfo.depthAttachmentFormat = GetAttachmentFormat(Attachment_DepthStencil);
    rinfo.stencilAttachmentFormat = rinfo.depthAttachmentFormat;

    VkSpecializationInfo spInfo{};
    VkSpecializationMapEntry entry{};

    VKit::GraphicsPipeline::Builder builder = createGeometryPipelineBuilder<D>(pass, geo, rinfo, spInfo, entry);
    switch (geo)
    {
    case Geometry_Circle:
        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());
    case Geometry_Static:
        builder.AddBindingDescription<StaticVertex<D>>();
        if constexpr (D == D2)
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StaticVertex<D2>, Position));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StaticVertex<D2>, TexCoord));
        }
        else
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StaticVertex<D3>, Position));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StaticVertex<D3>, TexCoord));
            if (pass == PipelinePass_Shaded)
            {
                builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StaticVertex<D3>, Normal));
                builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticVertex<D3>, Tangent));
            }
        }
        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());

    case Geometry_Parametric:
        builder.AddBindingDescription<ParametricVertex<D>>();
        if constexpr (D == D2)
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParametricVertex<D2>, Position));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParametricVertex<D2>, TexCoord));
            builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParametricVertex<D2>, Region));
        }
        else
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParametricVertex<D3>, Position));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParametricVertex<D3>, TexCoord));
            if (pass == PipelinePass_Shaded)
            {
                builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParametricVertex<D3>, Normal));
                builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                offsetof(ParametricVertex<D3>, Tangent));
            }
            else
                builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParametricVertex<D3>, Normal));
            builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParametricVertex<D3>, Region));
        }

        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());

    case Geometry_Glyph:
        builder.AddBindingDescription<GlyphVertex>();
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, Position));
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, AtlasCoord));
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, TexCoord));

        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());

    case Geometry_Dynamic:
        builder.AddBindingDescription<DynamicVertex<D>>();
        if constexpr (D == D2)
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(DynamicVertex<D2>, Position));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(DynamicVertex<D2>, TexCoord));
            builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(DynamicVertex<D2>, Color));
        }
        else
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(DynamicVertex<D3>, Position));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(DynamicVertex<D3>, TexCoord));
            if (pass == PipelinePass_Shaded)
            {
                builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(DynamicVertex<D3>, Normal));
                builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(DynamicVertex<D3>, Tangent));
            }
            builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(DynamicVertex<D3>, Color));
        }
        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());

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
        .AddShaderStage(shaders.OpaqueFragmentShaders[geo], VK_SHADER_STAGE_FRAGMENT_BIT)
        .SetViewportCount(1);

    if constexpr (D == D3)
    {
        builder.EnableDepthTest().EnableDepthWrite();
        if (geo != Geometry_Circle && geo != Geometry_Glyph)
            builder.AddDynamicState(VK_DYNAMIC_STATE_CULL_MODE_EXT);
    }
    if constexpr (D == D2)
        builder.BeginColorAttachment().SetColorWriteMask(VK_COLOR_COMPONENT_R_BIT).EndColorAttachment();

    return builder;
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

    VKit::GraphicsPipeline::Builder builder = createShadowPipelineBuilder<D>(geo, renderInfo);
    switch (geo)
    {
    case Geometry_Circle:
        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());
    case Geometry_Static:
        builder.AddBindingDescription<StaticVertex<D>>();
        if constexpr (D == D2)
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StaticVertex<D2>, Position));
        else
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StaticVertex<D3>, Position));

        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());

    case Geometry_Parametric:
        builder.AddBindingDescription<ParametricVertex<D>>();
        if constexpr (D == D2)
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ParametricVertex<D2>, Position));
            builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParametricVertex<D2>, Region));
        }
        else
        {
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParametricVertex<D3>, Position));
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParametricVertex<D3>, Normal));
            builder.AddAttributeDescription(0, VK_FORMAT_R32_UINT, offsetof(ParametricVertex<D3>, Region));
        }

        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());

    case Geometry_Glyph:
        builder.AddBindingDescription<GlyphVertex>();
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, Position));
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, AtlasCoord));

        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());
    case Geometry_Dynamic:
        builder.AddBindingDescription<DynamicVertex<D>>();
        if constexpr (D == D2)
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(DynamicVertex<D2>, Position));
        else
            builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(DynamicVertex<D3>, Position));

        return ONYX_CHECK_VKIT_RESULT(builder.Bake().Build());

    default:
        TKIT_FATAL("[ONYX][PIPELINES] Unrecognized geometry {}", u8(geo));
        return VKit::GraphicsPipeline{};
    }
}

VKit::ComputePipeline CreateRayMarchPipeline()
{
    VKit::ComputePipelineSpecs specs{};
    StandalonePipelineData &data = s_PipelineData->Standalone[StandalonePass_RayMarch];
    specs.ComputeShader = data.Shader;
    specs.Layout = data.Layout;
    return ONYX_CHECK_VKIT_RESULT(VKit::ComputePipeline::Create(GetDevice(), specs));
}

VKit::GraphicsPipeline CreateBlendPipeline()
{
    VkPipelineRenderingCreateInfoKHR rinfo{};
    const VkFormat format = GetAttachmentFormat(Attachment_Intermediate);
    rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachmentFormats = &format;
    rinfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    rinfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    StandalonePipelineData &data = s_PipelineData->Standalone[StandalonePass_Blend];
    return ONYX_CHECK_VKIT_RESULT(
        VKit::GraphicsPipeline::Builder(GetDevice(), data.Layout, rinfo)
            .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .SetViewportCount(1)
            .AddShaderStage(s_PipelineData->FullPassVertexShader, VK_SHADER_STAGE_VERTEX_BIT)
            .AddShaderStage(data.Shader, VK_SHADER_STAGE_FRAGMENT_BIT)
            .BeginColorAttachment()
            .EnableBlending()
            .SetColorBlendFactors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
            // NOTE(Isma, 25/06/06): Should consider not writing to the alpha channel. Causes alpha values to be
            // inherited. For now it actually lets us having modals in ui, so ill keep it. but its a bit
            // counterintuitive
            .SetAlphaBlendFactors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
            .EndColorAttachment()
            .Bake()
            .Build());
}

VKit::GraphicsPipeline CreatePostProcessPipeline()
{
    VkPipelineRenderingCreateInfoKHR rinfo{};
    const VkFormat format = GetAttachmentFormat(Attachment_Final);
    rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachmentFormats = &format;
    rinfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    rinfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    StandalonePipelineData &data = s_PipelineData->Standalone[StandalonePass_PostProcess];
    return ONYX_CHECK_VKIT_RESULT(VKit::GraphicsPipeline::Builder(GetDevice(), data.Layout, rinfo)
                                      .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                                      .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                                      .SetViewportCount(1)
                                      .AddShaderStage(s_PipelineData->FullPassVertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                                      .AddShaderStage(data.Shader, VK_SHADER_STAGE_FRAGMENT_BIT)
                                      .BeginColorAttachment()
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

    StandalonePipelineData &data = s_PipelineData->Standalone[StandalonePass_Compositor];
    return ONYX_CHECK_VKIT_RESULT(VKit::GraphicsPipeline::Builder(GetDevice(), data.Layout, rinfo)
                                      .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                                      .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                                      .SetViewportCount(1)
                                      .AddShaderStage(s_PipelineData->FullPassVertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                                      .AddShaderStage(data.Shader, VK_SHADER_STAGE_FRAGMENT_BIT)
                                      .BeginColorAttachment()
                                      .EnableBlending()
                                      .EndColorAttachment()
                                      .Bake()
                                      .Build());
}

template const VKit::PipelineLayout &GetPipelineLayout<D2>(RenderPass pass);
template const VKit::PipelineLayout &GetPipelineLayout<D3>(RenderPass pass);

template VKit::GraphicsPipeline CreateGeometryPipeline<D2>(PipelinePass pass, BlendPass bpass, Geometry geo);
template VKit::GraphicsPipeline CreateGeometryPipeline<D3>(PipelinePass pass, BlendPass bpass, Geometry geo);
template VKit::GraphicsPipeline CreateShadowPipeline<D2>(Geometry geo, VkFormat format);
template VKit::GraphicsPipeline CreateShadowPipeline<D3>(Geometry geo, VkFormat format);

} // namespace Onyx::Pipelines
