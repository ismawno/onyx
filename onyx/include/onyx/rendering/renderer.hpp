#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/rendering/swap_chain.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/buffer/storage_buffer.hpp"
#include "onyx/core/core.hpp"

#include <vulkan/vulkan.hpp>

#ifndef ONYX_BUFFER_INITIAL_CAPACITY
#    define ONYX_BUFFER_INITIAL_CAPACITY 4
#endif

#ifndef ONYX_MAX_POLYGON_SIDES
#    define ONYX_MAX_POLYGON_SIDES 16
#endif

#define ONYX_MAX_POLYGON_COUNT (ONYX_MAX_POLYGON_SIDES - 2)

// This is a hard limit. Cannot be changed without changing the shaders
#define ONYX_MAX_DIRECTIONAL_LIGHTS 7

// Because of batch rendering, draw order is not guaranteed

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

    KIT::StaticArray<vec4, ONYX_MAX_DIRECTIONAL_LIGHTS> DirectionalLights;
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

using DrawData2D = DrawData<2>;
using DrawData3D = DrawData<3>;

ONYX_DIMENSION_TEMPLATE struct ONYX_API PerFrameData
{
    KIT_NON_COPYABLE(PerFrameData)
    PerFrameData(const usize p_Capacity)
    {
        for (usize i = 0; i < SwapChain::MFIF; ++i)
        {
            StorageBuffers[i].Create(p_Capacity);
            StorageSizes[i] = 0;
        }
    }
    ~PerFrameData() noexcept
    {
        for (usize i = 0; i < SwapChain::MFIF; ++i)
            StorageBuffers[i].Destroy();
    }

    std::array<KIT::Storage<StorageBuffer<DrawData<N>>>, SwapChain::MFIF> StorageBuffers;
    std::array<VkDescriptorSet, SwapChain::MFIF> DescriptorSets;
    std::array<usize, SwapChain::MFIF> StorageSizes;
};

ONYX_DIMENSION_TEMPLATE class ONYX_API MeshRenderer
{
    KIT_NON_COPYABLE(MeshRenderer)
  public:
    MeshRenderer(VkRenderPass p_RenderPass) noexcept;
    ~MeshRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const KIT::Ref<const Model<N>> &p_Model, const DrawData<N> &p_DrawData) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Storage<Pipeline> m_Pipeline;
    // Could actually use a pointer to the model instead of a reference and take extra care the model still lives
    // while drawing
    HashMap<KIT::Ref<const Model<N>>, DynamicArray<DrawData<N>>> m_BatchData;
    PerFrameData<N> m_PerFrameData{ONYX_BUFFER_INITIAL_CAPACITY};

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

    void Draw(u32 p_FrameIndex, usize p_PrimitiveIndex, const DrawData<N> &p_DrawData) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Storage<Pipeline> m_Pipeline;

    std::array<DynamicArray<DrawData<N>>, Primitives<N>::AMOUNT> m_BatchData;
    PerFrameData<N> m_PerFrameData{ONYX_BUFFER_INITIAL_CAPACITY};

    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
};

using PrimitiveRenderer2D = PrimitiveRenderer<2>;
using PrimitiveRenderer3D = PrimitiveRenderer<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API PolygonRenderer
{
    KIT_NON_COPYABLE(PolygonRenderer)
  public:
    PolygonRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PolygonRenderer() noexcept;

    void Draw(u32 p_FrameIndex, std::span<const vec<N>> p_Vertices, const DrawData<N> &p_DrawData) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    struct PolygonPerFrameData : PerFrameData<N>
    {
        PolygonPerFrameData(const usize p_Capacity) : PerFrameData<N>(p_Capacity)
        {
            for (usize i = 0; i < SwapChain::MFIF; ++i)
            {
                VertexBuffers[i].Create(p_Capacity);
                IndexBuffers[i].Create(p_Capacity);
            }
        }

        ~PolygonPerFrameData() noexcept
        {
            for (usize i = 0; i < SwapChain::MFIF; ++i)
            {
                VertexBuffers[i].Destroy();
                IndexBuffers[i].Destroy();
            }
        }

        std::array<KIT::Storage<MutableVertexBuffer<N>>, SwapChain::MFIF> VertexBuffers;
        std::array<KIT::Storage<MutableIndexBuffer>, SwapChain::MFIF> IndexBuffers;
    };

    struct PolygonDrawData : DrawData<N>
    {
        PrimitiveDataLayout Layout;
    };

    KIT::Storage<Pipeline> m_Pipeline;

    // Batch data maps perfectly to the number of polygons to be drawn i.e number of entries in storage buffer.
    // StorageSizes is not needed
    DynamicArray<PolygonDrawData> m_BatchData;
    DynamicArray<Vertex<N>> m_Vertices;
    DynamicArray<Index> m_Indices;
    PolygonPerFrameData m_PerFrameData{ONYX_BUFFER_INITIAL_CAPACITY};

    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
};

using PolygonRenderer2D = PolygonRenderer<2>;
using PolygonRenderer3D = PolygonRenderer<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API CircleRenderer
{
    KIT_NON_COPYABLE(CircleRenderer)
  public:
    CircleRenderer(VkRenderPass p_RenderPass) noexcept;
    ~CircleRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const DrawData<N> &p_DrawData) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Storage<Pipeline> m_Pipeline;

    // Batch data maps perfectly to the number of circles to be drawn i.e number of entries in storage buffer.
    // StorageSizes is not needed
    DynamicArray<DrawData<N>> m_BatchData;
    PerFrameData<N> m_PerFrameData{ONYX_BUFFER_INITIAL_CAPACITY};

    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
};

using CircleRenderer2D = CircleRenderer<2>;
using CircleRenderer3D = CircleRenderer<3>;

} // namespace ONYX