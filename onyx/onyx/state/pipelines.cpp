#include "onyx/core/pch.hpp"
#include "onyx/state/pipelines.hpp"
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

static TKit::Storage<VKit::PipelineLayout> s_UnlitLayout{};
static TKit::Storage<VKit::PipelineLayout> s_LitLayout{};

static TKit::Storage<ShaderData> s_FillShaders2{};
static TKit::Storage<ShaderData> s_FillShaders3{};
static TKit::Storage<ShaderData> s_OutlineShaders2{};
static TKit::Storage<ShaderData> s_OutlineShaders3{};

template <Dimension D> ShaderData &getShaders(const DrawPass pass)
{
    if constexpr (D == D2)
        return pass == DrawPass_Fill ? *s_FillShaders2 : *s_OutlineShaders2;
    else
        return pass == DrawPass_Fill ? *s_FillShaders3 : *s_OutlineShaders3;
}

ONYX_NO_DISCARD static Result<> createPipelineLayouts()
{
    const VkDescriptorSetLayout slayout = Descriptors::GetInstanceDataStorageDescriptorSetLayout();
    const VkDescriptorSetLayout llayout = Descriptors::GetLightStorageDescriptorSetLayout();

    const VKit::LogicalDevice &device = Core::GetDevice();
    auto layoutResult = VKit::PipelineLayout::Builder(device)
                            .AddDescriptorSetLayout(slayout)
                            .AddPushConstantRange<PushConstantData<Shading_Unlit>>(VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_UnlitLayout = layoutResult.GetValue();

    layoutResult = VKit::PipelineLayout::Builder(device)
                       .AddDescriptorSetLayout(slayout)
                       .AddDescriptorSetLayout(llayout)
                       .AddPushConstantRange<PushConstantData<Shading_Lit>>(VK_SHADER_STAGE_VERTEX_BIT |
                                                                            VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_LitLayout = layoutResult.GetValue();
    return Result<>::Ok();
}

ONYX_NO_DISCARD static Result<> createShaders()
{
    auto cmpres = Shaders::Compiler()
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

    Shaders::Compilation &cmp = cmpres.GetValue();
    auto result = cmp.CreateShader("mainVS", "mesh-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->MeshVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "mesh-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->MeshFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "circle-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->CircleVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "circle-2D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders2->CircleFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "mesh-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->MeshVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "mesh-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->MeshFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "circle-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->CircleVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "circle-fill-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_FillShaders3->CircleFragmentShader = result.GetValue();

    s_OutlineShaders2 = s_FillShaders2;

    result = cmp.CreateShader("mainVS", "mesh-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_OutlineShaders3->MeshVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "mesh-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_OutlineShaders3->MeshFragmentShader = result.GetValue();

    result = cmp.CreateShader("mainVS", "circle-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_OutlineShaders3->CircleVertexShader = result.GetValue();

    result = cmp.CreateShader("mainFS", "circle-stencil-3D");
    TKIT_RETURN_ON_ERROR(result);
    s_OutlineShaders3->CircleFragmentShader = result.GetValue();

    cmp.Destroy();
    return Result<>::Ok();
}

Result<> Initialize()
{
    TKIT_LOG_INFO("[ONYX][PIPELINES] Initializing");
    s_UnlitLayout.Construct();
    s_LitLayout.Construct();

    s_FillShaders2.Construct();
    s_FillShaders3.Construct();
    s_OutlineShaders2.Construct();
    s_OutlineShaders3.Construct();
    TKIT_RETURN_IF_FAILED(createPipelineLayouts());
    return createShaders();
}
void Terminate()
{
    s_FillShaders2->Destroy();
    s_FillShaders3->Destroy();
    s_OutlineShaders3->Destroy();
    s_UnlitLayout->Destroy();
    s_LitLayout->Destroy();

    s_UnlitLayout.Destruct();
    s_LitLayout.Destruct();

    s_FillShaders2.Destruct();
    s_FillShaders3.Destruct();
    s_OutlineShaders2.Destruct();
    s_OutlineShaders3.Destruct();
}

VkPipelineLayout GetGraphicsPipelineLayout(const Shading shading)
{
    return shading == Shading_Unlit ? *s_UnlitLayout : *s_LitLayout;
}

template <Dimension D>
static VKit::GraphicsPipeline::Builder createPipelineBuilder(const StencilPass pass,
                                                             const VkPipelineRenderingCreateInfoKHR &renderInfo,
                                                             const VKit::Shader &vertexShader,
                                                             const VKit::Shader &fragmentShader)
{
    const Shading shading = GetShading<D>(pass);
    VKit::GraphicsPipeline::Builder builder{Core::GetDevice(), GetGraphicsPipelineLayout(shading), renderInfo};
    auto &colorBuilder = builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                             .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                             .SetViewportCount(1)
                             .AddShaderStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT)
                             .AddShaderStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
                             .BeginColorAttachment()
                             .EnableBlending();

    if constexpr (D == D3)
        builder.EnableDepthTest().EnableDepthWrite();
    else if (GetDrawMode(pass) == DrawPass_Outline)
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
        if constexpr (D == D3)
            builder.DisableDepthTest();
    }
    if (pass == StencilPass_DoStencilWriteNoFill)
        colorBuilder.SetColorWriteMask(0);

    colorBuilder.EndColorAttachment();

    return builder;
}

template <Dimension D>
Result<VKit::GraphicsPipeline> CreateCirclePipeline(const StencilPass pass,
                                                    const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const DrawPass dpass = GetDrawMode(pass);
    const ShaderData &shaders = getShaders<D>(dpass);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(pass, renderInfo, shaders.CircleVertexShader, shaders.CircleFragmentShader);

    return builder.Bake().Build();
}
template <Dimension D>
Result<VKit::GraphicsPipeline> CreateStaticMeshPipeline(const StencilPass pass,
                                                        const VkPipelineRenderingCreateInfoKHR &renderInfo)
{
    const DrawPass dpass = GetDrawMode(pass);
    const ShaderData &shaders = getShaders<D>(dpass);

    VKit::GraphicsPipeline::Builder builder =
        createPipelineBuilder<D>(pass, renderInfo, shaders.MeshVertexShader, shaders.MeshFragmentShader);

    builder.AddBindingDescription<StatVertex<D>>(VK_VERTEX_INPUT_RATE_VERTEX);
    if (D == D2 || pass == StencilPass_DoStencilWriteNoFill || pass == StencilPass_DoStencilTestNoFill)
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32_SFLOAT, offsetof(StatVertex<D>, Position));
    else
        builder.AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Position))
            .AddAttributeDescription(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(StatVertex<D3>, Normal));

    return builder.Bake().Build();
}

template Result<VKit::GraphicsPipeline> CreateStaticMeshPipeline<D2>(
    StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo);
template Result<VKit::GraphicsPipeline> CreateStaticMeshPipeline<D3>(
    StencilPass pass, const VkPipelineRenderingCreateInfoKHR &renderInfo);

template Result<VKit::GraphicsPipeline> CreateCirclePipeline<D2>(StencilPass pass,
                                                                 const VkPipelineRenderingCreateInfoKHR &renderInfo);
template Result<VKit::GraphicsPipeline> CreateCirclePipeline<D3>(StencilPass pass,
                                                                 const VkPipelineRenderingCreateInfoKHR &renderInfo);
} // namespace Onyx::Pipelines
