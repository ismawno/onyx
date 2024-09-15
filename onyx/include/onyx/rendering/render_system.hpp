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
            Vertex<N>::GetBindingDescriptions();
        std::array<VkVertexInputAttributeDescription, Vertex<N>::ATTRIBUTES> AttributeDescriptions =
            Vertex<N>::GetAttributeDescriptions();
        VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;
        VkRenderPass RenderPass = VK_NULL_HANDLE;

        // Could be changed to be multiple
        VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;
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

    void Display(const DrawInfo &p_Info) const noexcept;

    // These two methods will cause issues (races and inconsistencies) in concurrent mode if they are called from a
    // thread that does not own the window execution. Even by protecting them by mutexes, calling them from a different
    // thread will cause flickering (that is why I havent even bothered to protect them with mutexes)
    void SubmitRenderData(const RenderSystem &p_RenderSystem) noexcept;
    void SubmitRenderData(const DrawData &p_Data) noexcept;
    void ClearRenderData() noexcept;

  private:
    Pipeline m_Pipeline;
    DynamicArray<DrawData> m_DrawData;
    mutable const Model *m_BoundModel = nullptr;
};
} // namespace ONYX