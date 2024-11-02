#pragma once

#include "onyx/core/device.hpp"
#include "onyx/draw/vertex.hpp"
#include "onyx/buffer/index_buffer.hpp"
#include "onyx/buffer/vertex_buffer.hpp"

#include "kit/container/storage.hpp"

namespace ONYX
{
// Consider removing the ability to create a model with host visible memory (DONE)
// This model represents an immutable set of data that is meant to be used for rendering. It is not meant to be modified

template <Dimension D> class ONYX_API Model : public KIT::RefCounted<Model<D>>
{
    KIT_NON_COPYABLE(Model)
  public:
    KIT_BLOCK_ALLOCATED_SERIAL(Model<D>, 32)

    Model(std::span<const Vertex<D>> p_Vertices) noexcept;
    Model(std::span<const Vertex<D>> p_Vertices, std::span<const Index> p_Indices) noexcept;

    ~Model() noexcept;

    // TODO: Make sure no redundant bind calls are made
    // These bind and draw commands operate with a single vertex and index buffer. Not ideal when instancing could be
    // used. Plus, the same buffer may be bound multiple times if this is not handled with care
    void Bind(VkCommandBuffer p_CommandBuffer) const noexcept;

    void Draw(VkCommandBuffer p_CommandBuffer, u32 p_InstanceCount = 0, u32 p_FirstInstance = 0,
              u32 p_FirstVertex = 0) const noexcept;

    void DrawIndexed(VkCommandBuffer p_CommandBuffer, u32 p_InstanceCount = 0, u32 p_FirstInstance = 0,
                     u32 p_FirstIndex = 0, u32 p_VertexOffset = 0) const noexcept;

    bool HasIndices() const noexcept;

    const VertexBuffer<D> &GetVertexBuffer() const noexcept;
    const IndexBuffer &GetIndexBuffer() const noexcept; // This is UB if HasIndices returns false

    static KIT::Scope<const Model> Load(std::string_view p_Path) noexcept;

  private:
    KIT::Ref<Device> m_Device;
    VertexBuffer<D> m_VertexBuffer;
    KIT::Storage<IndexBuffer> m_IndexBuffer;

    bool m_HasIndices;
};

} // namespace ONYX