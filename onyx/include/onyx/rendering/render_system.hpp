#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/camera/camera.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
class ONYX_API RenderSystem
{
    KIT_NON_COPYABLE(RenderSystem)

  public:
    ONYX_DIMENSION_TEMPLATE struct Specs
    {
        const char *VertexShaderPath = Core::GetVertexShaderPath<N>();
        const char *FragmentShaderPath = Core::GetFragmentShaderPath<N>();

        std::array<VkVertexInputBindingDescription, Vertex<N>::BINDINGS> BindingDescriptions =
            Vertex<N>::BindingDescriptions();
        std::array<VkVertexInputAttributeDescription, Vertex<N>::ATTRIBUTES> AttributeDescriptions =
            Vertex<N>::AttributeDescriptions();
        VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;
        VkRenderPass RenderPass = VK_NULL_HANDLE;
    };
    using Specs2D = Specs<2>;
    using Specs3D = Specs<3>;

    struct DrawInfo
    {
        VkCommandBuffer CommandBuffer;
        VkDescriptorSet DescriptorSet;
        mat4 Projection;
    };

    struct PushConstantData
    {
        mat4 ModelTransform;
        vec4 Color;
    };

    struct DrawData
    {
        const Model *Model;
        PushConstantData Data;
    };

    ONYX_DIMENSION_TEMPLATE RenderSystem(const Specs<N> &p_Specs) noexcept;

    void Display(const DrawInfo &p_Info) noexcept;
    void SubmitRenderData(const DrawData &p_Data) noexcept;

  private:
    Pipeline m_Pipeline;
    DynamicArray<DrawData> m_DrawData;
    mutable const Model *m_BoundModel = nullptr;

    // Protectes access to m_DrawData (you may draw to a secondary window in the main thread, which will trigger
    // m_DrawData update)
    mutable std::mutex m_Mutex;
};
} // namespace ONYX