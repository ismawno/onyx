#include "onyx/core/pch.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/core/core.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/state/shaders.hpp"
#include "vkit/state/pipeline_layout.hpp"
#include "tkit/preprocessor/utils.hpp"

namespace Onyx::Pipelines
{
struct ShaderData
{
    VKit::Shader MeshVertexShader{};
    VKit::Shader MeshFragmentShader{};
    VKit::Shader CircleVertexShader{};
    VKit::Shader CircleFragmentShader{};

    void Destroy()
    {
        MeshVertexShader.Destroy();
        MeshFragmentShader.Destroy();
        CircleVertexShader.Destroy();
        CircleFragmentShader.Destroy();
    }
};

static VKit::PipelineLayout s_UnlitLayout{};
static VKit::PipelineLayout s_LitLayout{};

static ShaderData s_FillShaders2;
static ShaderData s_FillShaders3;
static ShaderData s_OutlineShaders2;
static ShaderData s_OutlineShaders3;

template <Dimension D> ShaderData &getShaders(const DrawMode p_Mode)
{
    if constexpr (D == D2)
        return p_Mode == Draw_Fill ? s_FillShaders2 : s_OutlineShaders2;
    else
        return p_Mode == Draw_Fill ? s_FillShaders3 : s_OutlineShaders3;
}

static bool utilsWasModified(const std::string &p_BinaryPath)
{
    const char *sourcePath = ONYX_ROOT_PATH "/onyx/shaders/utils.glsl";
    return VKit::Shader::MustCompile(sourcePath, p_BinaryPath);
}
static VKit::Shader createShader(const char *p_SourcePath)
{
    const std::string binaryPath = Shaders::CreateShaderDefaultBinaryPath(p_SourcePath);
    if (utilsWasModified(binaryPath))
        Shaders::Compile(p_SourcePath, binaryPath);

    return Shaders::Create(p_SourcePath);
}

static void createPipelineLayouts()
{
    TKIT_LOG_INFO("[ONYX] Creating pipeline layouts");
    const VkDescriptorSetLayout slayout = Assets::GetInstanceDataStorageDescriptorSetLayout();
    const VkDescriptorSetLayout llayout = Assets::GetLightStorageDescriptorSetLayout();

    const VKit::LogicalDevice &device = Core::GetDevice();
    auto layoutResult = VKit::PipelineLayout::Builder(device)
                            .AddDescriptorSetLayout(slayout)
                            .AddPushConstantRange<Detail::PushConstantData<Shading_Unlit>>(VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_UnlitLayout = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(slayout)
                       .AddDescriptorSetLayout(llayout)
                       .AddPushConstantRange<Detail::PushConstantData<Shading_Lit>>(VK_SHADER_STAGE_VERTEX_BIT |
                                                                                    VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    VKIT_ASSERT_RESULT(layoutResult);
    s_LitLayout = layoutResult.GetValue();
}

static void createShaders()
{
    TKIT_LOG_INFO("[ONYX] Creating global shaders");
    s_FillShaders2.MeshVertexShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-2D.vert");
    s_FillShaders2.MeshFragmentShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-2D.frag");

    s_FillShaders2.CircleVertexShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/circle-2D.vert");
    s_FillShaders2.CircleFragmentShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/circle-2D.frag");

    s_FillShaders3.MeshVertexShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-fill-3D.vert");
    s_FillShaders3.MeshFragmentShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-fill-3D.frag");

    s_FillShaders3.CircleVertexShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/circle-fill-3D.vert");
    s_FillShaders3.CircleFragmentShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/circle-fill-3D.frag");

    s_OutlineShaders2 = s_FillShaders2;

    s_OutlineShaders3.MeshVertexShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-stencil-3D.vert");
    s_OutlineShaders3.MeshFragmentShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-stencil-3D.frag");

    s_OutlineShaders3.CircleVertexShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/circle-stencil-3D.vert");
    s_OutlineShaders3.CircleFragmentShader = createShader(ONYX_ROOT_PATH "/onyx/shaders/circle-stencil-3D.frag");
}

void Initialize()
{
    createPipelineLayouts();
    createShaders();
}
void Terminate()
{
    s_FillShaders2.Destroy();
    s_FillShaders3.Destroy();
    s_OutlineShaders3.Destroy();
    s_UnlitLayout.Destroy();
    s_LitLayout.Destroy();
}

VkPipelineLayout GetGraphicsPipelineLayout(const Shading p_Shading)
{
    return p_Shading == Shading_Unlit ? s_UnlitLayout : s_LitLayout;
}

template <Dimension D>
static VKit::GraphicsPipeline::Builder createPipelineBuilder(const PipelineMode p_Mode,
                                                             const VkPipelineRenderingCreateInfoKHR &p_RenderInfo,
                                                             const VKit::Shader &p_VertexShader,
                                                             const VKit::Shader &p_FragmentShader)
{
    const Shading shading = GetShading<D>(p_Mode);
    VKit::GraphicsPipeline::Builder builder{Core::GetDevice(), GetGraphicsPipelineLayout(shading), p_RenderInfo};
    auto &colorBuilder = builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                             .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                             .SetViewportCount(1)
                             .AddShaderStage(p_VertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                             .AddShaderStage(p_FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
                             .BeginColorAttachment()
                             .EnableBlending();

    if constexpr (D == D3)
        builder.EnableDepthTest().EnableDepthWrite();
    else if (GetDrawMode(p_Mode) == Draw_Outline)
        colorBuilder.DisableBlending();

    const auto stencilFlags = VKit::StencilOperationFlag_Front | VKit::StencilOperationFlag_Back;

    if (p_Mode == Pipeline_DoStencilWriteDoFill || p_Mode == Pipeline_DoStencilWriteNoFill)
        builder.EnableStencilTest()
            .SetStencilFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilPassOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilDepthFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilCompareOperation(VK_COMPARE_OP_ALWAYS, stencilFlags)
            .SetStencilCompareMask(0xFF, stencilFlags)
            .SetStencilWriteMask(0xFF, stencilFlags)
            .SetStencilReference(1, stencilFlags);
    else if (p_Mode == Pipeline_DoStencilTestNoFill)
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
        if constexpr (D == D3)
            builder.DisableDepthTest();
    }
    if (p_Mode == Pipeline_DoStencilWriteNoFill)
        colorBuilder.SetColorWriteMask(0);

    colorBuilder.EndColorAttachment();

    return builder;
}

template <Dimension D>
VKit::GraphicsPipeline CreateStaticMeshPipeline(const PipelineMode p_Mode,
                                                const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    const DrawMode dmode = GetDrawMode(p_Mode);
    const ShaderData &shaders = getShaders<D>(dmode);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(p_Mode, p_RenderInfo, shaders.MeshVertexShader, shaders.MeshFragmentShader);

    builder.AddBindingDescription<StatVertex<D>>(VK_VERTEX_INPUT_RATE_VERTEX);
    if constexpr (D == D2)
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D2>, Position));
    else
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Position))
            .AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Normal));

    const auto result = builder.Bake().Build();
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
template <Dimension D>
VKit::GraphicsPipeline CreateCirclePipeline(const PipelineMode p_Mode,
                                            const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    const DrawMode dmode = GetDrawMode(p_Mode);
    const ShaderData &shaders = getShaders<D>(dmode);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(p_Mode, p_RenderInfo, shaders.CircleVertexShader, shaders.CircleFragmentShader);

    const auto result = builder.Bake().Build();
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template VKit::GraphicsPipeline CreateStaticMeshPipeline<D2>(PipelineMode p_Mode,
                                                             const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);
template VKit::GraphicsPipeline CreateStaticMeshPipeline<D3>(PipelineMode p_Mode,
                                                             const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

template VKit::GraphicsPipeline CreateCirclePipeline<D2>(PipelineMode p_Mode,
                                                         const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);
template VKit::GraphicsPipeline CreateCirclePipeline<D3>(PipelineMode p_Mode,
                                                         const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);
} // namespace Onyx::Pipelines
