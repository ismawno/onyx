#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/rendering/swap_chain.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/buffer/storage_buffer.hpp"
#include "onyx/core/core.hpp"

#include <vulkan/vulkan.hpp>

#ifndef ONYX_STORAGE_BUFFER_INITIAL_CAPACITY
#    define ONYX_STORAGE_BUFFER_INITIAL_CAPACITY 4
#endif

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct RenderInfo;

template <> struct ONYX_API RenderInfo<2>
{
    VkCommandBuffer CommandBuffer;
    u32 FrameIndex;
};

template <> struct ONYX_API RenderInfo<3>
{
    VkCommandBuffer CommandBuffer;
    u32 FrameIndex;

    vec3 LightDirection;
    f32 LightIntensity;
    f32 AmbientIntensity;
};

ONYX_DIMENSION_TEMPLATE struct DrawData;

template <> struct ONYX_API DrawData<2>
{
    mat4 Transform;
    vec4 Color;
};
template <> struct ONYX_API DrawData<3>
{
    mat4 Transform;
    mat4 NormalMatrix;
    vec4 Color;
};

ONYX_DIMENSION_TEMPLATE struct ONYX_API PerFrameData
{
    KIT_NON_COPYABLE(PerFrameData)
    PerFrameData(const usize p_Capacity)
    {
        for (usize i = 0; i < SwapChain::MFIF; ++i)
        {
            Buffers[i].Create(p_Capacity);
            Sizes[i] = 0;
        }
    }
    ~PerFrameData() noexcept
    {
        for (usize i = 0; i < SwapChain::MFIF; ++i)
            Buffers[i].Destroy();
    }

    std::array<KIT::Storage<StorageBuffer<DrawData<N>>>, SwapChain::MFIF> Buffers;
    std::array<VkDescriptorSet, SwapChain::MFIF> DescriptorSets;
    std::array<usize, SwapChain::MFIF> Sizes;
};

ONYX_DIMENSION_TEMPLATE class ONYX_API MeshRenderer
{
    KIT_NON_COPYABLE(MeshRenderer)
  public:
    MeshRenderer(VkRenderPass p_RenderPass) noexcept;
    ~MeshRenderer() noexcept;

    void Draw(const KIT::Ref<const Model<N>> &p_Model, const mat<N> &p_Transform, const vec4 &p_Color,
              u32 p_FrameIndex) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Storage<Pipeline> m_Pipeline;
    // Could actually use a pointer to the model instead of a reference and take extra care the model still lives
    // while drawing
    HashMap<KIT::Ref<const Model<N>>, DynamicArray<DrawData<N>>> m_BatchData;
    PerFrameData<N> m_PerFrameData{ONYX_STORAGE_BUFFER_INITIAL_CAPACITY};

    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
};

using MeshRenderer2D = MeshRenderer<2>;
using MeshRenderer3D = MeshRenderer<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API PrimitiveRenderer
{
    KIT_NON_COPYABLE(PrimitiveRenderer)
  public:
    PrimitiveRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PrimitiveRenderer() noexcept;

    void Draw(usize p_PrimitiveIndex, const mat<N> &p_Transform, const vec4 &p_Color, u32 p_FrameIndex) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Storage<Pipeline> m_Pipeline;

    std::array<DynamicArray<DrawData<N>>, Primitives<N>::AMOUNT> m_BatchData;
    PerFrameData<N> m_PerFrameData{ONYX_STORAGE_BUFFER_INITIAL_CAPACITY};

    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
};

using PrimitiveRenderer2D = PrimitiveRenderer<2>;
using PrimitiveRenderer3D = PrimitiveRenderer<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API CircleRenderer
{
    KIT_NON_COPYABLE(CircleRenderer)
  public:
    CircleRenderer(VkRenderPass p_RenderPass) noexcept;
    ~CircleRenderer() noexcept;

    void Draw(const mat<N> &p_Transform, const vec4 &p_Color, u32 p_FrameIndex) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Storage<Pipeline> m_Pipeline;

    DynamicArray<DrawData<N>> m_BatchData;
    PerFrameData<N> m_PerFrameData{ONYX_STORAGE_BUFFER_INITIAL_CAPACITY};

    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
};

using CircleRenderer2D = CircleRenderer<2>;
using CircleRenderer3D = CircleRenderer<3>;

} // namespace ONYX