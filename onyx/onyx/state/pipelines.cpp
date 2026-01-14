#include "onyx/core/pch.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/core/core.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/property/vertex.hpp"
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

template <Dimension D> ShaderData &getShaders(const DrawPass p_Pass)
{
    if constexpr (D == D2)
        return p_Pass == DrawPass_Fill ? s_FillShaders2 : s_OutlineShaders2;
    else
        return p_Pass == DrawPass_Fill ? s_FillShaders3 : s_OutlineShaders3;
}

static void createPipelineLayouts()
{
    TKIT_LOG_INFO("[ONYX] Creating pipeline layouts");
    const VkDescriptorSetLayout slayout = Descriptors::GetInstanceDataStorageDescriptorSetLayout();
    const VkDescriptorSetLayout llayout = Descriptors::GetLightStorageDescriptorSetLayout();

    const VKit::LogicalDevice &device = Core::GetDevice();
    auto layoutResult = VKit::PipelineLayout::Builder(device)
                            .AddDescriptorSetLayout(slayout)
                            .AddPushConstantRange<PushConstantData<Shading_Unlit>>(VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    VKIT_CHECK_RESULT(layoutResult);
    s_UnlitLayout = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(slayout)
                       .AddDescriptorSetLayout(llayout)
                       .AddPushConstantRange<PushConstantData<Shading_Lit>>(VK_SHADER_STAGE_VERTEX_BIT |
                                                                            VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    VKIT_CHECK_RESULT(layoutResult);
    s_LitLayout = layoutResult.GetValue();
}

static void createShaders()
{
    TKIT_LOG_INFO("[ONYX] Creating global shaders");
    Shaders::Compilation cmp = Shaders::Compiler()
                                   .AddSearchPath(ONYX_ROOT_PATH "/onyx/shaders")
                                   .AddModule("mesh-2D")
                                   .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                                   .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                                   .Load()
                                   .AddModule("circle-2D")
                                   .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                                   .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                                   .Load()
                                   .AddModule("mesh-fill-3D")
                                   .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                                   .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                                   .Load()
                                   .AddModule("circle-fill-3D")
                                   .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                                   .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                                   .Load()
                                   .AddModule("mesh-stencil-3D")
                                   .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                                   .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                                   .Load()
                                   .AddModule("circle-stencil-3D")
                                   .DeclareEntryPoint("mainVS", ShaderStage_Vertex)
                                   .DeclareEntryPoint("mainFS", ShaderStage_Fragment)
                                   .Load()
                                   .Compile();

    s_FillShaders2.MeshVertexShader = cmp.CreateShader("mainVS", "mesh-2D");
    s_FillShaders2.MeshFragmentShader = cmp.CreateShader("mainFS", "mesh-2D");

    s_FillShaders2.CircleVertexShader = cmp.CreateShader("mainVS", "circle-2D");
    s_FillShaders2.CircleFragmentShader = cmp.CreateShader("mainFS", "circle-2D");

    s_FillShaders3.MeshVertexShader = cmp.CreateShader("mainVS", "mesh-fill-3D");
    s_FillShaders3.MeshFragmentShader = cmp.CreateShader("mainFS", "mesh-fill-3D");

    s_FillShaders3.CircleVertexShader = cmp.CreateShader("mainVS", "circle-fill-3D");
    s_FillShaders3.CircleFragmentShader = cmp.CreateShader("mainFS", "circle-fill-3D");

    s_OutlineShaders2 = s_FillShaders2;

    s_OutlineShaders3.MeshVertexShader = cmp.CreateShader("mainVS", "mesh-stencil-3D");
    s_OutlineShaders3.MeshFragmentShader = cmp.CreateShader("mainFS", "mesh-stencil-3D");

    s_OutlineShaders3.CircleVertexShader = cmp.CreateShader("mainVS", "circle-stencil-3D");
    s_OutlineShaders3.CircleFragmentShader = cmp.CreateShader("mainFS", "circle-stencil-3D");
    cmp.Destroy();
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
static VKit::GraphicsPipeline::Builder createPipelineBuilder(const StencilPass p_Pass,
                                                             const VkPipelineRenderingCreateInfoKHR &p_RenderInfo,
                                                             const VKit::Shader &p_VertexShader,
                                                             const VKit::Shader &p_FragmentShader)
{
    const Shading shading = GetShading<D>(p_Pass);
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
    else if (GetDrawMode(p_Pass) == DrawPass_Outline)
        colorBuilder.DisableBlending();

    const auto stencilFlags = VKit::StencilOperationFlag_Front | VKit::StencilOperationFlag_Back;

    if (p_Pass == StencilPass_DoStencilWriteDoFill || p_Pass == StencilPass_DoStencilWriteNoFill)
        builder.EnableStencilTest()
            .SetStencilFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilPassOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilDepthFailOperation(VK_STENCIL_OP_REPLACE, stencilFlags)
            .SetStencilCompareOperation(VK_COMPARE_OP_ALWAYS, stencilFlags)
            .SetStencilCompareMask(0xFF, stencilFlags)
            .SetStencilWriteMask(0xFF, stencilFlags)
            .SetStencilReference(1, stencilFlags);
    else if (p_Pass == StencilPass_DoStencilTestNoFill)
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
    if (p_Pass == StencilPass_DoStencilWriteNoFill)
        colorBuilder.SetColorWriteMask(0);

    colorBuilder.EndColorAttachment();

    return builder;
}

template <Dimension D>
VKit::GraphicsPipeline CreateStaticMeshPipeline(const StencilPass p_Pass,
                                                const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    const DrawPass dpass = GetDrawMode(p_Pass);
    const ShaderData &shaders = getShaders<D>(dpass);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(p_Pass, p_RenderInfo, shaders.MeshVertexShader, shaders.MeshFragmentShader);

    builder.AddBindingDescription<StatVertex<D>>(VK_VERTEX_INPUT_RATE_VERTEX);
    if (D == D2 || p_Pass == StencilPass_DoStencilWriteNoFill || p_Pass == StencilPass_DoStencilTestNoFill)
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D>, Position));
    else
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Position))
            .AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Normal));

    const auto result = builder.Bake().Build();
    VKIT_CHECK_RESULT(result);
    return result.GetValue();
}
template <Dimension D>
VKit::GraphicsPipeline CreateCirclePipeline(const StencilPass p_Pass,
                                            const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    const DrawPass dpass = GetDrawMode(p_Pass);
    const ShaderData &shaders = getShaders<D>(dpass);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(p_Pass, p_RenderInfo, shaders.CircleVertexShader, shaders.CircleFragmentShader);

    const auto result = builder.Bake().Build();
    VKIT_CHECK_RESULT(result);
    return result.GetValue();
}

template VKit::GraphicsPipeline CreateStaticMeshPipeline<D2>(StencilPass p_Pass,
                                                             const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);
template VKit::GraphicsPipeline CreateStaticMeshPipeline<D3>(StencilPass p_Pass,
                                                             const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

template VKit::GraphicsPipeline CreateCirclePipeline<D2>(StencilPass p_Pass,
                                                         const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);
template VKit::GraphicsPipeline CreateCirclePipeline<D3>(StencilPass p_Pass,
                                                         const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);
} // namespace Onyx::Pipelines
