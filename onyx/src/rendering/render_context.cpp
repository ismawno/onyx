#include "core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"

namespace ONYX
{
struct GlobalUBO
{
    vec4 LightDirection;
    f32 LightIntensity;
    f32 AmbientIntensity;
};

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::initializeRenderers(const VkRenderPass p_RenderPass,
                                                                    const VkDescriptorSetLayout p_Layout) noexcept
{
    m_MeshRenderer.Create(p_RenderPass, p_Layout);
    m_CircleRenderer.Create(p_RenderPass, p_Layout);
}

RenderContext<2>::RenderContext(const VkRenderPass p_RenderPass) noexcept
{
    initializeRenderers(p_RenderPass, nullptr);
}

void RenderContext<2>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    RenderInfo<2> info{};
    info.CommandBuffer = p_CommandBuffer;

    m_MeshRenderer->Render(info);
    m_CircleRenderer->Render(info);
}

RenderContext<3>::RenderContext(const VkRenderPass p_RenderPass) noexcept
{
    m_Device = Core::GetDevice();
    createGlobalUniformHelper();
    initializeRenderers(p_RenderPass, m_GlobalUniformHelper->Layout.GetLayout());
}

RenderContext<3>::~RenderContext() noexcept
{
    m_GlobalUniformHelper.Destroy();
}

void RenderContext<3>::Render(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    GlobalUBO ubo{};
    ubo.LightDirection = vec4{glm::normalize(LightDirection), 0.f};
    ubo.LightIntensity = LightIntensity;
    ubo.AmbientIntensity = AmbientIntensity;

    m_GlobalUniformHelper->UniformBuffer.WriteAt(p_FrameIndex, &ubo);
    m_GlobalUniformHelper->UniformBuffer.FlushAt(p_FrameIndex);

    RenderInfo<3> info{};
    info.CommandBuffer = p_CommandBuffer;
    info.GlobalDescriptorSet = m_GlobalUniformHelper->DescriptorSets[p_FrameIndex];

    m_MeshRenderer->Render(info);
    m_CircleRenderer->Render(info);
}

void RenderContext<3>::createGlobalUniformHelper() noexcept
{
    DescriptorPool::Specs poolSpecs{};
    poolSpecs.MaxSets = SwapChain::MAX_FRAMES_IN_FLIGHT;
    poolSpecs.PoolSizes.push_back(
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT});

    static constexpr std::array<VkDescriptorSetLayoutBinding, 1> bindings = {
        {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          nullptr}}};

    const auto &props = m_Device->GetProperties();
    Buffer::Specs bufferSpecs{};
    bufferSpecs.InstanceCount = SwapChain::MAX_FRAMES_IN_FLIGHT;
    bufferSpecs.InstanceSize = sizeof(GlobalUBO);
    bufferSpecs.Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    bufferSpecs.MinimumAlignment =
        glm::max(props.limits.minUniformBufferOffsetAlignment, props.limits.nonCoherentAtomSize);

    m_GlobalUniformHelper.Create(poolSpecs, bindings, bufferSpecs);
    m_GlobalUniformHelper->UniformBuffer.Map();

    for (usize i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const auto info = m_GlobalUniformHelper->UniformBuffer.DescriptorInfoAt(i);
        DescriptorWriter writer{&m_GlobalUniformHelper->Layout, &m_GlobalUniformHelper->Pool};
        writer.WriteBuffer(0, &info);
        m_GlobalUniformHelper->DescriptorSets[i] = writer.Build();
    }
}

} // namespace ONYX