#pragma once

#include "onyx/rendering/render_specs.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

#ifndef ONYX_BUFFER_INITIAL_CAPACITY
#    define ONYX_BUFFER_INITIAL_CAPACITY 4
#endif

namespace ONYX
{
template <Dimension D, PipelineMode PMode> class ONYX_API MeshRenderer
{
    KIT_NON_COPYABLE(MeshRenderer)

    using InstanceData = typename MeshRendererSpecs<D, GetDrawMode<PMode>()>::InstanceData;
    using RenderInfo = typename MeshRendererSpecs<D, GetDrawMode<PMode>()>::RenderInfo;

  public:
    MeshRenderer(VkRenderPass p_RenderPass) noexcept;
    ~MeshRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const InstanceData &p_InstanceData, const KIT::Ref<const Model<D>> &p_Model) noexcept;
    void Render(const RenderInfo &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    KIT::Storage<Pipeline> m_Pipeline;

    // Could actually use a pointer to the model instead of a reference and take extra care the model still lives
    // while drawing
    using HostInstanceData = HashMap<KIT::Ref<const Model<D>>, DynamicArray<InstanceData>>;

    HostInstanceData m_HostInstanceData;
    DeviceInstanceData<InstanceData> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

template <Dimension D, PipelineMode PMode> class ONYX_API PrimitiveRenderer
{
    KIT_NON_COPYABLE(PrimitiveRenderer)

    using InstanceData = typename PrimitiveRendererSpecs<D, GetDrawMode<PMode>()>::InstanceData;
    using RenderInfo = typename PrimitiveRendererSpecs<D, GetDrawMode<PMode>()>::RenderInfo;

  public:
    PrimitiveRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PrimitiveRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const InstanceData &p_InstanceData, usize p_PrimitiveIndex) noexcept;
    void Render(const RenderInfo &p_Info) noexcept;

    void Flush() noexcept;

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool;
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;
    KIT::Storage<Pipeline> m_Pipeline;

    using HostInstanceData = std::array<DynamicArray<InstanceData>, Primitives<D>::AMOUNT>;

    HostInstanceData m_HostInstanceData;
    DeviceInstanceData<InstanceData> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

template <Dimension D, PipelineMode PMode> class ONYX_API PolygonRenderer
{
    KIT_NON_COPYABLE(PolygonRenderer)

    using InstanceData = typename PolygonRendererSpecs<D, GetDrawMode<PMode>()>::InstanceData;
    using RenderInfo = typename PolygonRendererSpecs<D, GetDrawMode<PMode>()>::RenderInfo;

    using HostInstanceData = typename PolygonRendererSpecs<D, GetDrawMode<PMode>()>::HostInstanceData;
    using DeviceInstanceData = typename PolygonRendererSpecs<D, GetDrawMode<PMode>()>::DeviceInstanceData;

  public:
    PolygonRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PolygonRenderer() noexcept;

    void Draw(u32 p_FrameIndex, const InstanceData &p_InstanceData, std::span<const vec<D>> p_Vertices) noexcept;
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
    DynamicArray<Vertex<D>> m_Vertices;
    DynamicArray<Index> m_Indices;
};

template <Dimension D, PipelineMode PMode> class ONYX_API CircleRenderer
{
    KIT_NON_COPYABLE(CircleRenderer)

    using InstanceData = typename CircleRendererSpecs<D, GetDrawMode<PMode>()>::InstanceData;
    using RenderInfo = typename CircleRendererSpecs<D, GetDrawMode<PMode>()>::RenderInfo;

  public:
    CircleRenderer(VkRenderPass p_RenderPass) noexcept;
    ~CircleRenderer() noexcept;

    // Circle args already contained in instance data
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