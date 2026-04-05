#include "onyx/core/pch.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/property/vertex.hpp"
#include "tkit/preprocessor/utils.hpp"

namespace Onyx::Pipelines
{
struct ShaderData
{
    VKit::Shader CircleVertexShader{};
    VKit::Shader CircleFragmentShader{};
    VKit::Shader StaticVertexShader{};
    VKit::Shader StaticFragmentShader{};
    VKit::Shader ParametricVertexShader{};
    VKit::Shader ParametricFragmentShader{};
    VKit::Shader GlyphVertexShader{};
    VKit::Shader GlyphFragmentShader{};

    void Destroy()
    {
        CircleVertexShader.Destroy();
        CircleFragmentShader.Destroy();
        StaticVertexShader.Destroy();
        StaticFragmentShader.Destroy();
        ParametricVertexShader.Destroy();
        ParametricFragmentShader.Destroy();
        GlyphVertexShader.Destroy();
        GlyphFragmentShader.Destroy();
    }
};

static TKit::Storage<VKit::PipelineLayout> s_StencilPipLayout2{};
static TKit::Storage<VKit::PipelineLayout> s_StencilPipLayout3{};
static TKit::Storage<VKit::PipelineLayout> s_FillPipLayout2{};
static TKit::Storage<VKit::PipelineLayout> s_FillPipLayout3{};

static TKit::Storage<ShaderData> s_FillShaders2{};
static TKit::Storage<ShaderData> s_FillShaders3{};
static TKit::Storage<ShaderData> s_StencilShaders2{};
static TKit::Storage<ShaderData> s_StencilShaders3{};

template <Dimension D> ShaderData &getShaders(const DrawPass pass)
{
    if constexpr (D == D2)
        return pass == DrawPass_Fill ? *s_FillShaders2 : *s_StencilShaders2;
    else
        return pass == DrawPass_Fill ? *s_FillShaders3 : *s_StencilShaders3;
}

ONYX_NO_DISCARD static Result<> createPipelineLayouts()
{
    const VKit::DescriptorSetLayout &slayout2 = Descriptors::GetDescriptorSetLayout<D2>(DrawPass_Stencil);
    const VKit::DescriptorSetLayout &slayout3 = Descriptors::GetDescriptorSetLayout<D3>(DrawPass_Stencil);

    const VKit::DescriptorSetLayout &flayout2 = Descriptors::GetDescriptorSetLayout<D2>(DrawPass_Fill);
    const VKit::DescriptorSetLayout &flayout3 = Descriptors::GetDescriptorSetLayout<D3>(DrawPass_Fill);

    const VKit::LogicalDevice &device = GetDevice();
    auto layoutResult = VKit::PipelineLayout::Builder(device)
                            .AddDescriptorSetLayout(slayout2)
                            .AddPushConstantRange<StencilPushConstantData>(VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_StencilPipLayout2 = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(slayout3)
                       .AddPushConstantRange<StencilPushConstantData>(VK_SHADER_STAGE_VERTEX_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_StencilPipLayout3 = layoutResult.GetValue();

    layoutResult =
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(flayout2)
            .AddPushConstantRange<FillPushConstantData<D2>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_FillPipLayout2 = layoutResult.GetValue();

    layoutResult =
        VKit::PipelineLayout::Builder(device)
            .AddDescriptorSetLayout(flayout3)
            .AddPushConstantRange<FillPushConstantData<D3>>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_FillPipLayout3 = layoutResult.GetValue();

    if (CanNameObjects())
    {
        TKIT_RETURN_IF_FAILED(s_StencilPipLayout2->SetName("onyx-stencil-pipeline-layout-2D"));
        TKIT_RETURN_IF_FAILED(s_StencilPipLayout3->SetName("onyx-stencil-pipeline-layout-3D"));
        TKIT_RETURN_IF_FAILED(s_FillPipLayout2->SetName("onyx-fill-pipeline-layout-2D"));
        return s_FillPipLayout3->SetName("onyx-fill-pipeline-layout-3D");
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
    if (GetInstance().IsExtensionEnabled("VK_EXT_debug_utils"))
        compiler.AddBooleanArgument(ShaderArgument_DebugInformation);

    auto cmpres = compiler.AddSearchPath(ONYX_ROOT_PATH "/onyx/shaders")
                      .AddModule("circle-fill-2D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("circle-stencil-2D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("circle-fill-3D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("circle-stencil-3D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("static-fill-2D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("static-stencil-2D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("static-fill-3D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("static-stencil-3D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("parametric-fill-2D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("parametric-stencil-2D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("parametric-fill-3D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("parametric-stencil-3D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("glyph-fill-2D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("glyph-stencil-2D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("glyph-fill-3D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .AddModule("glyph-stencil-3D")
                      .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                      .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                      .Load()
                      .Compile();

    TKIT_RETURN_ON_ERROR(cmpres);
    Shaders::Compilation &cmp = cmpres.GetValue();

    auto result = cmp.CreateShader("mainVS", "circle-fill-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->CircleVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "circle-fill-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->CircleFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "circle-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->CircleVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "circle-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->CircleFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "circle-stencil-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders2->CircleVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "circle-stencil-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders2->CircleFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "circle-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders3->CircleVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "circle-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders3->CircleFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "static-fill-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->StaticVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "static-fill-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->StaticFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "static-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->StaticVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "static-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->StaticFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "static-stencil-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders2->StaticVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "static-stencil-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders2->StaticFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "static-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders3->StaticVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "static-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders3->StaticFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "parametric-fill-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->ParametricVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "parametric-fill-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->ParametricFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "parametric-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->ParametricVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "parametric-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->ParametricFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "parametric-stencil-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders2->ParametricVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "parametric-stencil-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders2->ParametricFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "parametric-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders3->ParametricVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "parametric-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders3->ParametricFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "glyph-fill-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->GlyphVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "glyph-fill-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->GlyphFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "glyph-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->GlyphVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "glyph-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->GlyphFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "glyph-stencil-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders2->GlyphVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "glyph-stencil-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders2->GlyphFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "glyph-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders3->GlyphVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "glyph-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_StencilShaders3->GlyphFragmentShader = result.GetValue();

    cmp.Destroy();

    if (CanNameObjects())
    {
        TKIT_RETURN_IF_FAILED(s_FillShaders2->CircleVertexShader.SetName("onyx-2D-vertex-shader-circle-fill"));
        TKIT_RETURN_IF_FAILED(s_FillShaders2->CircleFragmentShader.SetName("onyx-2D-fragment-shader-circle-fill"));

        TKIT_RETURN_IF_FAILED(s_FillShaders3->CircleVertexShader.SetName("onyx-3D-vertex-shader-circle-fill"));
        TKIT_RETURN_IF_FAILED(s_FillShaders3->CircleFragmentShader.SetName("onyx-3D-fragment-shader-circle-fill"));

        TKIT_RETURN_IF_FAILED(s_StencilShaders2->CircleVertexShader.SetName("onyx-2D-vertex-shader-circle-stencil"));
        TKIT_RETURN_IF_FAILED(
            s_StencilShaders2->CircleFragmentShader.SetName("onyx-2D-fragment-shader-circle-stencil"));

        TKIT_RETURN_IF_FAILED(s_StencilShaders3->CircleVertexShader.SetName("onyx-3D-vertex-shader-circle-stencil"));
        TKIT_RETURN_IF_FAILED(
            s_StencilShaders3->CircleFragmentShader.SetName("onyx-3D-fragment-shader-circle-stencil"));

        TKIT_RETURN_IF_FAILED(s_FillShaders3->StaticVertexShader.SetName("onyx-3D-vertex-shader-static-fill"));
        TKIT_RETURN_IF_FAILED(s_FillShaders3->StaticFragmentShader.SetName("onyx-3D-fragment-shader-static-fill"));

        TKIT_RETURN_IF_FAILED(s_FillShaders2->StaticVertexShader.SetName("onyx-2D-vertex-shader-static-fill"));
        TKIT_RETURN_IF_FAILED(s_FillShaders2->StaticFragmentShader.SetName("onyx-2D-fragment-shader-static-fill"));

        TKIT_RETURN_IF_FAILED(s_StencilShaders2->StaticVertexShader.SetName("onyx-2D-vertex-shader-static-stencil"));
        TKIT_RETURN_IF_FAILED(
            s_StencilShaders2->StaticFragmentShader.SetName("onyx-2D-fragment-shader-static-stencil"));

        TKIT_RETURN_IF_FAILED(s_StencilShaders3->StaticVertexShader.SetName("onyx-3D-vertex-shader-static-stencil"));
        TKIT_RETURN_IF_FAILED(
            s_StencilShaders3->StaticFragmentShader.SetName("onyx-3D-fragment-shader-static-stencil"));

        TKIT_RETURN_IF_FAILED(s_FillShaders2->ParametricVertexShader.SetName("onyx-2D-vertex-shader-parametric-fill"));
        TKIT_RETURN_IF_FAILED(
            s_FillShaders2->ParametricFragmentShader.SetName("onyx-2D-fragment-shader-parametric-fill"));

        TKIT_RETURN_IF_FAILED(s_FillShaders3->ParametricVertexShader.SetName("onyx-3D-vertex-shader-parametric-fill"));
        TKIT_RETURN_IF_FAILED(
            s_FillShaders3->ParametricFragmentShader.SetName("onyx-3D-fragment-shader-parametric-fill"));

        TKIT_RETURN_IF_FAILED(
            s_StencilShaders2->ParametricVertexShader.SetName("onyx-2D-vertex-shader-parametric-stencil"));
        TKIT_RETURN_IF_FAILED(
            s_StencilShaders2->ParametricFragmentShader.SetName("onyx-2D-fragment-shader-parametric-stencil"));

        TKIT_RETURN_IF_FAILED(
            s_StencilShaders3->ParametricVertexShader.SetName("onyx-3D-vertex-shader-parametric-stencil"));
        TKIT_RETURN_IF_FAILED(
            s_StencilShaders3->ParametricFragmentShader.SetName("onyx-3D-fragment-shader-parametric-stencil"));

        TKIT_RETURN_IF_FAILED(s_FillShaders2->GlyphVertexShader.SetName("onyx-2D-vertex-shader-glyph-fill"));
        TKIT_RETURN_IF_FAILED(s_FillShaders2->GlyphFragmentShader.SetName("onyx-2D-fragment-shader-glyph-fill"));

        TKIT_RETURN_IF_FAILED(s_FillShaders3->GlyphVertexShader.SetName("onyx-3D-vertex-shader-glyph-fill"));
        TKIT_RETURN_IF_FAILED(s_FillShaders3->GlyphFragmentShader.SetName("onyx-3D-fragment-shader-glyph-fill"));

        TKIT_RETURN_IF_FAILED(s_StencilShaders2->GlyphVertexShader.SetName("onyx-2D-vertex-shader-glyph-stencil"));
        TKIT_RETURN_IF_FAILED(s_StencilShaders2->GlyphFragmentShader.SetName("onyx-2D-fragment-shader-glyph-stencil"));

        TKIT_RETURN_IF_FAILED(s_StencilShaders3->GlyphVertexShader.SetName("onyx-3D-vertex-shader-glyph-stencil"));
        TKIT_RETURN_IF_FAILED(s_StencilShaders3->GlyphFragmentShader.SetName("onyx-3D-fragment-shader-glyph-stencil"));
    }

    return Result<>::Ok();
}

static void destroyShaders()
{
    s_FillShaders2->Destroy();
    s_FillShaders3->Destroy();
    s_StencilShaders2->Destroy();
    s_StencilShaders3->Destroy();
}

Result<> ReloadShaders()
{
    destroyShaders();
    return createShaders();
}

Result<> Initialize()
{
    TKIT_LOG_INFO("[ONYX][PIPELINES] Initializing");
    s_StencilPipLayout2.Construct();
    s_StencilPipLayout3.Construct();
    s_FillPipLayout2.Construct();
    s_FillPipLayout3.Construct();

    s_FillShaders2.Construct();
    s_FillShaders3.Construct();
    s_StencilShaders2.Construct();
    s_StencilShaders3.Construct();
    TKIT_RETURN_IF_FAILED(createPipelineLayouts());
    return createShaders();
}
void Terminate()
{
    destroyShaders();
    s_StencilPipLayout2->Destroy();
    s_StencilPipLayout3->Destroy();
    s_FillPipLayout2->Destroy();
    s_FillPipLayout3->Destroy();

    s_StencilPipLayout2.Destruct();
    s_StencilPipLayout3.Destruct();
    s_FillPipLayout2.Destruct();
    s_FillPipLayout3.Destruct();

    s_FillShaders2.Destruct();
    s_FillShaders3.Destruct();
    s_StencilShaders2.Destruct();
    s_StencilShaders3.Destruct();
}

template <Dimension D> const VKit::PipelineLayout &GetPipelineLayout(const DrawPass pass)
{
    if constexpr (D == D2)
        return pass == DrawPass_Fill ? *s_FillPipLayout2 : *s_StencilPipLayout2;
    else
        return pass == DrawPass_Fill ? *s_FillPipLayout3 : *s_StencilPipLayout3;
}

template <Dimension D>
static VKit::GraphicsPipeline::Builder createPipelineBuilder(const StencilPass pass,
                                                             const VkPipelineRenderingCreateInfoKHR &renderInfo,
                                                             const VKit::Shader &vertexShader,
                                                             const VKit::Shader &fragmentShader)
{
    const DrawPass dpass = GetDrawPass(pass);
    VKit::GraphicsPipeline::Builder builder{GetDevice(), GetPipelineLayout<D>(dpass), renderInfo};
    auto &colorBuilder = builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                             .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                             .SetViewportCount(1)
                             .AddShaderStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                             .AddShaderStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
                             .BeginColorAttachment()
                             .EnableBlending();

    if (D == D2 || dpass == DrawPass_Fill)
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
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createCirclePipeline(
    const StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const DrawPass dpass = GetDrawPass(pass);
    const ShaderData &shaders = getShaders<D>(dpass);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(pass, renderInfo, shaders.CircleVertexShader, shaders.CircleFragmentShader);

    return builder.Bake().Build();
}
template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createStaticMeshPipeline(
    const StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const DrawPass dpass = GetDrawPass(pass);
    const ShaderData &shaders = getShaders<D>(dpass);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(pass, renderInfo, shaders.StaticVertexShader, shaders.StaticFragmentShader);

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
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createParametricMeshPipeline(
    const StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const DrawPass dpass = GetDrawPass(pass);
    const ShaderData &shaders = getShaders<D>(dpass);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(pass, renderInfo, shaders.ParametricVertexShader, shaders.ParametricFragmentShader);

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
ONYX_NO_DISCARD static Result<VKit::GraphicsPipeline> createGlyphMeshPipeline(
    const StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const DrawPass dpass = GetDrawPass(pass);
    const ShaderData &shaders = getShaders<D>(dpass);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(pass, renderInfo, shaders.GlyphVertexShader, shaders.GlyphFragmentShader);
    builder.AddBindingDescription<GlyphVertex>();
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, Position));
    builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, AtlasCoord));

    return builder.Bake().Build();
}

template <Dimension D>
Result<VKit::GraphicsPipeline> CreatePipeline(const StencilPass pass, const Geometry geo,
                                              const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    switch (geo)
    {
    case Geometry_Circle:
        return createCirclePipeline<D>(pass, renderInfo);
    case Geometry_Static:
        return createStaticMeshPipeline<D>(pass, renderInfo);
    case Geometry_Parametric:
        return createParametricMeshPipeline<D>(pass, renderInfo);
    case Geometry_Glyph:
        return createGlyphMeshPipeline<D>(pass, renderInfo);
    default:
        return Result<VKit::GraphicsPipeline>::Error(
            Error_BadInput, TKit::Format("[ONYX][PIPELINES] Unrecognized geometry {}", u8(geo)));
    }
}

template const VKit::PipelineLayout &GetPipelineLayout<D2>(DrawPass pass);
template const VKit::PipelineLayout &GetPipelineLayout<D3>(DrawPass pass);

template Result<VKit::GraphicsPipeline> CreatePipeline<D2>(StencilPass pass, Geometry geo,
                                                           const VkPipelineRenderingCreateInfoKHR &renderInfo);
template Result<VKit::GraphicsPipeline> CreatePipeline<D3>(StencilPass pass, Geometry geo,
                                                           const VkPipelineRenderingCreateInfoKHR &renderInfo);

} // namespace Onyx::Pipelines
