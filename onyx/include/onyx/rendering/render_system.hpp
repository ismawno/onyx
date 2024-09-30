#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/camera/camera.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
struct ONYX_API DrawInfo
{
    VkCommandBuffer CommandBuffer;
    VkDescriptorSet DescriptorSet;
    mat4 Projection;
};

ONYX_DIMENSION_TEMPLATE struct PushConstantData;

template <> struct ONYX_API PushConstantData<2>
{
    mat4 ModelTransform;
    vec4 Color;
};

template <> struct ONYX_API PushConstantData<3>
{
    mat4 ModelTransform;
    mat4 ColorAndNormalMatrix;
};

ONYX_DIMENSION_TEMPLATE class ONYX_API RenderSystem
{
    KIT_NON_COPYABLE(RenderSystem)

  public:
    struct Specs
    {
        const char *VertexShaderPath = Core::GetPrimitiveVertexShaderPath<N>();
        const char *FragmentShaderPath = Core::GetPrimitiveFragmentShaderPath<N>();

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

    // Could use directly PushConstantData<N> but I want to keep DrawData clean because its more exposed
    struct DrawData
    {
        const Model *Model;
        mat4 ModelTransform;
        vec4 Color;
    };

    RenderSystem(const Specs &p_Specs) noexcept;
    void Display(const DrawInfo &p_Info) const noexcept;

    // These two methods will cause issues (races and inconsistencies) in concurrent mode if they are called from a
    // thread that does not own the window execution. Even by protecting them by mutexes, calling them from a different
    // thread will cause flickering (that is why I havent even bothered to protect them with mutexes)
    // Every render system should submit from the thread owning the window
    void SubmitRenderData(const RenderSystem &p_RenderSystem) noexcept;
    void SubmitRenderData(const DrawData &p_Data) noexcept;
    void ClearRenderData() noexcept;

  private:
    Pipeline m_Pipeline;
    DynamicArray<DrawData> m_DrawData;
    mutable const Model *m_BoundModel = nullptr;
};

using RenderSystem2D = RenderSystem<2>;
using RenderSystem3D = RenderSystem<3>;
} // namespace ONYX