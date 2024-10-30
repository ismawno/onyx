#include "core/pch.hpp"
#include "onyx/rendering/render_specs.hpp"

namespace ONYX
{
template <u32 N, bool FullDrawPass>
    requires(IsDim<N>())
struct ShaderPaths;

template <bool FullDrawPass> struct ShaderPaths<2, FullDrawPass>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh2D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle2D.frag.spv";
};

template <> struct ShaderPaths<3, true>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh3D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle3D.frag.spv";
};

template <> struct ShaderPaths<3, false>
{
    static constexpr const char *MeshVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh-outline3D.vert.spv";
    static constexpr const char *MeshFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/mesh-outline3D.frag.spv";

    static constexpr const char *CircleVertex = ONYX_ROOT_PATH "/onyx/shaders/bin/circle-outline3D.vert.spv";
    static constexpr const char *CircleFragment = ONYX_ROOT_PATH "/onyx/shaders/bin/circle-outline3D.frag.spv";
};

template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
static Pipeline::Specs defaultPipelineSpecs(const char *vpath, const char *fpath, const VkRenderPass p_RenderPass,
                                            const VkDescriptorSetLayout *p_Layouts, const u32 p_LayoutCount) noexcept
{
    Pipeline::Specs specs{};
    if constexpr (N == 3)
    {
        specs.PushConstantRange.size = sizeof(PushConstantData3D);
        specs.PushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        specs.PipelineLayoutInfo.pushConstantRangeCount = 1;
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;
    }
    else if constexpr (!IsFullDrawPass<Mode>())
        specs.ColorBlendAttachment.blendEnable = VK_FALSE;

    if constexpr (Mode == StencilMode::StencilWriteFill || Mode == StencilMode::StencilWriteNoFill)
    {
        specs.DepthStencilInfo.stencilTestEnable = VK_TRUE;
        specs.DepthStencilInfo.front.failOp = VK_STENCIL_OP_KEEP;
        specs.DepthStencilInfo.front.passOp = VK_STENCIL_OP_REPLACE;
        specs.DepthStencilInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
        specs.DepthStencilInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
        specs.DepthStencilInfo.front.compareMask = 0xFF;
        specs.DepthStencilInfo.front.writeMask = 0xFF;
        specs.DepthStencilInfo.front.reference = 1;
        specs.DepthStencilInfo.back = specs.DepthStencilInfo.front;
    }
    else if constexpr (Mode == StencilMode::StencilTest)
    {
        specs.DepthStencilInfo.stencilTestEnable = VK_TRUE;
        specs.DepthStencilInfo.depthWriteEnable = VK_FALSE;
        specs.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.DepthStencilInfo.front.compareOp = VK_COMPARE_OP_EQUAL;
        specs.DepthStencilInfo.front.reference = 1;
        specs.DepthStencilInfo.front.compareMask = 0xFF;
        specs.DepthStencilInfo.front.writeMask = 0;
        specs.DepthStencilInfo.back = specs.DepthStencilInfo.front;
    }
    if constexpr (Mode == StencilMode::StencilWriteNoFill)
        specs.ColorBlendAttachment.colorWriteMask = 0;

    specs.PipelineLayoutInfo.pSetLayouts = p_Layouts;
    specs.PipelineLayoutInfo.setLayoutCount = p_LayoutCount;

    specs.VertexShaderPath = vpath;
    specs.FragmentShaderPath = fpath;

    specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    specs.RenderPass = p_RenderPass;

    return specs;
}

template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
Pipeline::Specs MeshRendererSpecs<N, Mode>::CreatePipelineSpecs(const VkRenderPass p_RenderPass,
                                                                const VkDescriptorSetLayout *p_Layouts,
                                                                const u32 p_LayoutCount) noexcept
{
    Pipeline::Specs specs = defaultPipelineSpecs<N, Mode>(ShaderPaths<N, IsFullDrawPass<Mode>()>::MeshVertex,
                                                          ShaderPaths<N, IsFullDrawPass<Mode>()>::MeshFragment,
                                                          p_RenderPass, p_Layouts, p_LayoutCount);

    const auto &bdesc = Vertex<N>::GetBindingDescriptions();
    const auto &attdesc = Vertex<N>::GetAttributeDescriptions();
    specs.BindingDescriptions = std::span<const VkVertexInputBindingDescription>(bdesc);
    specs.AttributeDescriptions = std::span<const VkVertexInputAttributeDescription>(attdesc);

    return specs;
}
template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
Pipeline::Specs PrimitiveRendererSpecs<N, Mode>::CreatePipelineSpecs(const VkRenderPass p_RenderPass,
                                                                     const VkDescriptorSetLayout *p_Layouts,
                                                                     const u32 p_LayoutCount) noexcept
{
    return MeshRendererSpecs<N, Mode>::CreatePipelineSpecs(p_RenderPass, p_Layouts, p_LayoutCount);
}

template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
Pipeline::Specs PolygonRendererSpecs<N, Mode>::CreatePipelineSpecs(const VkRenderPass p_RenderPass,
                                                                   const VkDescriptorSetLayout *p_Layouts,
                                                                   const u32 p_LayoutCount) noexcept
{
    return MeshRendererSpecs<N, Mode>::CreatePipelineSpecs(p_RenderPass, p_Layouts, p_LayoutCount);
}

template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
Pipeline::Specs CircleRendererSpecs<N, Mode>::CreatePipelineSpecs(const VkRenderPass p_RenderPass,
                                                                  const VkDescriptorSetLayout *p_Layouts,
                                                                  const u32 p_LayoutCount) noexcept
{
    return defaultPipelineSpecs<N, Mode>(ShaderPaths<N, IsFullDrawPass<Mode>()>::CircleVertex,
                                         ShaderPaths<N, IsFullDrawPass<Mode>()>::CircleFragment, p_RenderPass,
                                         p_Layouts, p_LayoutCount);
}

template struct MeshRendererSpecs<2, StencilMode::NoStencilWriteFill>;
template struct MeshRendererSpecs<2, StencilMode::StencilWriteFill>;
template struct MeshRendererSpecs<2, StencilMode::StencilWriteNoFill>;
template struct MeshRendererSpecs<2, StencilMode::StencilTest>;

template struct PrimitiveRendererSpecs<2, StencilMode::NoStencilWriteFill>;
template struct PrimitiveRendererSpecs<2, StencilMode::StencilWriteFill>;
template struct PrimitiveRendererSpecs<2, StencilMode::StencilWriteNoFill>;
template struct PrimitiveRendererSpecs<2, StencilMode::StencilTest>;

template struct PolygonRendererSpecs<2, StencilMode::NoStencilWriteFill>;
template struct PolygonRendererSpecs<2, StencilMode::StencilWriteFill>;
template struct PolygonRendererSpecs<2, StencilMode::StencilWriteNoFill>;
template struct PolygonRendererSpecs<2, StencilMode::StencilTest>;

template struct CircleRendererSpecs<2, StencilMode::NoStencilWriteFill>;
template struct CircleRendererSpecs<2, StencilMode::StencilWriteFill>;
template struct CircleRendererSpecs<2, StencilMode::StencilWriteNoFill>;
template struct CircleRendererSpecs<2, StencilMode::StencilTest>;

template struct MeshRendererSpecs<3, StencilMode::NoStencilWriteFill>;
template struct MeshRendererSpecs<3, StencilMode::StencilWriteFill>;
template struct MeshRendererSpecs<3, StencilMode::StencilWriteNoFill>;
template struct MeshRendererSpecs<3, StencilMode::StencilTest>;

template struct PrimitiveRendererSpecs<3, StencilMode::NoStencilWriteFill>;
template struct PrimitiveRendererSpecs<3, StencilMode::StencilWriteFill>;
template struct PrimitiveRendererSpecs<3, StencilMode::StencilWriteNoFill>;
template struct PrimitiveRendererSpecs<3, StencilMode::StencilTest>;

template struct PolygonRendererSpecs<3, StencilMode::NoStencilWriteFill>;
template struct PolygonRendererSpecs<3, StencilMode::StencilWriteFill>;
template struct PolygonRendererSpecs<3, StencilMode::StencilWriteNoFill>;
template struct PolygonRendererSpecs<3, StencilMode::StencilTest>;

template struct CircleRendererSpecs<3, StencilMode::NoStencilWriteFill>;
template struct CircleRendererSpecs<3, StencilMode::StencilWriteFill>;
template struct CircleRendererSpecs<3, StencilMode::StencilWriteNoFill>;
template struct CircleRendererSpecs<3, StencilMode::StencilTest>;

} // namespace ONYX