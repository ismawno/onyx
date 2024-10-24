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
    VkDescriptorSet LightStorageBuffers;
    u32 FrameIndex;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    vec4 AmbientColor;
};

ONYX_DIMENSION_TEMPLATE struct MaterialData;

template <> struct MaterialData<2>
{
    Color Color = ONYX::Color::WHITE;
};

template <> struct MaterialData<3>
{
    Color Color = ONYX::Color::WHITE;
    f32 DiffuseContribution = 0.8f;
    f32 SpecularContribution = 0.2f;
    f32 SpecularSharpness = 32.f;
};

using MaterialData2D = MaterialData<2>;
using MaterialData3D = MaterialData<3>;

ONYX_DIMENSION_TEMPLATE struct InstanceData;

// Could actually save some space by using smaller matrices in the 2D case and removing the last row, as it always is 0
// 0 1 but i dont want to deal with the alignment management tbh

template <> struct ONYX_API InstanceData<2>
{
    mat4 Transform;
    MaterialData2D Material;
};
template <> struct ONYX_API InstanceData<3>
{
    mat4 Transform;
    mat4 NormalMatrix;
    mat4 ProjectionView; // The projection view may vary between shapes
    vec4 ViewPosition;
    MaterialData3D Material;
};

using InstanceData2D = InstanceData<2>;
using InstanceData3D = InstanceData<3>;

ONYX_DIMENSION_TEMPLATE struct ONYX_API DeviceInstanceData
{
    KIT_NON_COPYABLE(DeviceInstanceData)
    DeviceInstanceData(usize p_Capacity) noexcept;
    ~DeviceInstanceData() noexcept;

    std::array<KIT::Storage<StorageBuffer<InstanceData<N>>>, SwapChain::MFIF> StorageBuffers;
    std::array<VkDescriptorSet, SwapChain::MFIF> DescriptorSets;
    std::array<usize, SwapChain::MFIF> StorageSizes;
};

ONYX_DIMENSION_TEMPLATE class ONYX_API MeshRenderer
{
    KIT_NON_COPYABLE(MeshRenderer)
  public:
    MeshRenderer(VkRenderPass p_RenderPass) noexcept;
    ~MeshRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const KIT::Ref<const Model<N>> &p_Model,
              const InstanceData<N> &p_InstanceData) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Storage<Pipeline> m_Pipeline;
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    // Could actually use a pointer to the model instead of a reference and take extra care the model still lives
    // while drawing
    using HostInstanceData = HashMap<KIT::Ref<const Model<N>>, DynamicArray<InstanceData<N>>>;

    HostInstanceData m_HostInstanceData;
    DeviceInstanceData<N> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

using MeshRenderer2D = MeshRenderer<2>;
using MeshRenderer3D = MeshRenderer<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API PrimitiveRenderer
{
    KIT_NON_COPYABLE(PrimitiveRenderer)
  public:
    PrimitiveRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PrimitiveRenderer() noexcept;

    void Draw(u32 p_FrameIndex, usize p_PrimitiveIndex, const InstanceData<N> &p_InstanceData) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    KIT::Storage<Pipeline> m_Pipeline;

    using HostInstanceData = std::array<DynamicArray<InstanceData<N>>, Primitives<N>::AMOUNT>;

    HostInstanceData m_HostInstanceData;
    DeviceInstanceData<N> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

using PrimitiveRenderer2D = PrimitiveRenderer<2>;
using PrimitiveRenderer3D = PrimitiveRenderer<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API PolygonRenderer
{
    KIT_NON_COPYABLE(PolygonRenderer)
  public:
    PolygonRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PolygonRenderer() noexcept;

    void Draw(u32 p_FrameIndex, std::span<const vec<N>> p_Vertices, const InstanceData<N> &p_InstanceData) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    struct PolygonDeviceInstanceData : DeviceInstanceData<N>
    {
        PolygonDeviceInstanceData(const usize p_Capacity) : DeviceInstanceData<N>(p_Capacity)
        {
            for (usize i = 0; i < SwapChain::MFIF; ++i)
            {
                VertexBuffers[i].Create(p_Capacity);
                IndexBuffers[i].Create(p_Capacity);
            }
        }

        ~PolygonDeviceInstanceData() noexcept
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

    struct PolygonInstanceData : InstanceData<N>
    {
        PrimitiveDataLayout Layout;
    };

    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    KIT::Storage<Pipeline> m_Pipeline;

    // Batch data maps perfectly to the number of polygons to be drawn i.e number of entries in storage buffer.
    // StorageSizes is not needed
    DynamicArray<PolygonInstanceData> m_HostInstanceData;
    PolygonDeviceInstanceData m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
    DynamicArray<Vertex<N>> m_Vertices;
    DynamicArray<Index> m_Indices;
};

using PolygonRenderer2D = PolygonRenderer<2>;
using PolygonRenderer3D = PolygonRenderer<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API CircleRenderer
{
    KIT_NON_COPYABLE(CircleRenderer)
  public:
    CircleRenderer(VkRenderPass p_RenderPass) noexcept;
    ~CircleRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const InstanceData<N> &p_InstanceData) noexcept;
    void Render(const RenderInfo<N> &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Storage<Pipeline> m_Pipeline;
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;

    // Batch data maps perfectly to the number of circles to be drawn i.e number of entries in storage buffer.
    // StorageSizes is not needed
    DynamicArray<InstanceData<N>> m_HostInstanceData;
    DeviceInstanceData<N> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

using CircleRenderer2D = CircleRenderer<2>;
using CircleRenderer3D = CircleRenderer<3>;

} // namespace ONYX