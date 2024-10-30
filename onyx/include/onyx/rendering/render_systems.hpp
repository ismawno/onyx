#pragma once

#include "onyx/rendering/render_specs.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

#ifndef ONYX_BUFFER_INITIAL_CAPACITY
#    define ONYX_BUFFER_INITIAL_CAPACITY 4
#endif

namespace ONYX
{
template <typename Specs> class ONYX_API MeshRenderer
{
    KIT_NON_COPYABLE(MeshRenderer)
    static constexpr u32 N = Specs::Dimension;
    static constexpr StencilMode Mode = Specs::STMode;

    using InstanceData = typename Specs::InstanceData;
    using RenderInfo = typename Specs::RenderInfo;

  public:
    MeshRenderer(VkRenderPass p_RenderPass) noexcept;
    ~MeshRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const KIT::Ref<const Model<N>> &p_Model, const InstanceData &p_InstanceData) noexcept;
    void Render(const RenderInfo &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    KIT::Storage<Pipeline> m_Pipeline;

    // Could actually use a pointer to the model instead of a reference and take extra care the model still lives
    // while drawing
    using HostInstanceData = HashMap<KIT::Ref<const Model<N>>, DynamicArray<InstanceData>>;

    HostInstanceData m_HostInstanceData;
    DeviceInstanceData<InstanceData> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

template <typename Specs> class ONYX_API PrimitiveRenderer
{
    KIT_NON_COPYABLE(PrimitiveRenderer)
    static constexpr u32 N = Specs::Dimension;
    static constexpr StencilMode Mode = Specs::STMode;

    using InstanceData = typename Specs::InstanceData;
    using RenderInfo = typename Specs::RenderInfo;

  public:
    PrimitiveRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PrimitiveRenderer() noexcept;

    void Draw(u32 p_FrameIndex, usize p_PrimitiveIndex, const InstanceData &p_InstanceData) noexcept;
    void Render(const RenderInfo &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    KIT::Storage<Pipeline> m_Pipeline;

    using HostInstanceData = std::array<DynamicArray<InstanceData>, Primitives<N>::AMOUNT>;

    HostInstanceData m_HostInstanceData;
    DeviceInstanceData<InstanceData> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

template <typename Specs> class ONYX_API PolygonRenderer
{
    KIT_NON_COPYABLE(PolygonRenderer)
    static constexpr u32 N = Specs::Dimension;
    static constexpr StencilMode Mode = Specs::STMode;

    using InstanceData = typename Specs::InstanceData;
    using RenderInfo = typename Specs::RenderInfo;

    using HostInstanceData = typename Specs::HostInstanceData;
    using DeviceInstanceData = typename Specs::DeviceInstanceData;

  public:
    PolygonRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PolygonRenderer() noexcept;

    void Draw(u32 p_FrameIndex, std::span<const vec<N>> p_Vertices, const InstanceData &p_InstanceData) noexcept;
    void Render(const RenderInfo &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    KIT::Storage<Pipeline> m_Pipeline;

    // Batch data maps perfectly to the number of polygons to be drawn i.e number of entries in storage buffer.
    // StorageSizes is not needed
    DynamicArray<HostInstanceData> m_HostInstanceData;
    DeviceInstanceData m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
    DynamicArray<Vertex<N>> m_Vertices;
    DynamicArray<Index> m_Indices;
};

template <typename Specs> class ONYX_API CircleRenderer
{
    KIT_NON_COPYABLE(CircleRenderer)
    static constexpr u32 N = Specs::Dimension;
    static constexpr StencilMode Mode = Specs::STMode;

    using InstanceData = typename Specs::InstanceData;
    using RenderInfo = typename Specs::RenderInfo;

  public:
    CircleRenderer(VkRenderPass p_RenderPass) noexcept;
    ~CircleRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const InstanceData &p_InstanceData) noexcept;
    void Render(const RenderInfo &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    KIT::Storage<Pipeline> m_Pipeline;

    // Batch data maps perfectly to the number of circles to be drawn i.e number of entries in storage buffer.
    // StorageSizes is not needed
    DynamicArray<InstanceData> m_HostInstanceData;
    DeviceInstanceData<InstanceData> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

} // namespace ONYX