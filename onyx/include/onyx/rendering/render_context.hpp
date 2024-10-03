#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/camera/camera.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"
#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/rendering/swap_chain.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class ONYX_API IRenderContext
{
  protected:
    // this method MUST be called externally (ie by a derived class). it wont be called automatically
    void initializeRenderers(VkRenderPass p_RenderPass, VkDescriptorSetLayout p_Layout) noexcept;

    KIT::Storage<MeshRenderer<N>> m_MeshRenderer;
    KIT::Storage<CircleRenderer<N>> m_CircleRenderer;
};

ONYX_DIMENSION_TEMPLATE class RenderContext;

template <> class ONYX_API RenderContext<2> final : IRenderContext<2>
{
  public:
    RenderContext(VkRenderPass p_RenderPass) noexcept;

    void Render(VkCommandBuffer p_CommandBuffer) noexcept;
};
template <> class ONYX_API RenderContext<3> final : IRenderContext<3>
{
  public:
    RenderContext(VkRenderPass p_RenderPass) noexcept;
    ~RenderContext() noexcept;

    void Render(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer) noexcept;

  private:
    struct GlobalUniformHelper
    {
        GlobalUniformHelper(const DescriptorPool::Specs &p_PoolSpecs,
                            const std::span<const VkDescriptorSetLayoutBinding> p_Bindings,
                            const Buffer::Specs &p_BufferSpecs) noexcept
            : Pool(p_PoolSpecs), Layout(p_Bindings), UniformBuffer(p_BufferSpecs)
        {
        }
        DescriptorPool Pool;
        DescriptorSetLayout Layout;
        Buffer UniformBuffer;
        std::array<VkDescriptorSet, SwapChain::MAX_FRAMES_IN_FLIGHT> DescriptorSets;
    };

    void createGlobalUniformHelper() noexcept;

    KIT::Storage<GlobalUniformHelper> m_GlobalUniformHelper;
    KIT::Ref<Device> m_Device;

    vec3 LightDirection{0.f, -1.f, 0.f};
    f32 LightIntensity = 0.9f;
    f32 AmbientIntensity = 0.1f;
};

using RenderContext2D = RenderContext<2>;
using RenderContext3D = RenderContext<3>;
} // namespace ONYX