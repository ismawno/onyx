#pragma once

#include "onyx/core/device.hpp"
#include "onyx/draw/vertex.hpp"
#include "onyx/buffer/index_buffer.hpp"
#include "onyx/buffer/vertex_buffer.hpp"

#include "kit/container/storage.hpp"

namespace Onyx
{
// Consider removing the ability to create a model with host visible memory (DONE)
// This model represents an immutable set of data that is meant to be used for rendering. It is not meant to be modified

/**
 * @brief Represents an immutable model used for rendering.
 *
 * This class encapsulates vertex and optional index data,
 * and provides methods for binding and drawing the model.
 * It is not intended to be modified after creation.
 *
 * @tparam D The dimensionality of the model (D2 or D3).
 */
template <Dimension D> class ONYX_API Model : public TKit::RefCounted<Model<D>>
{
    TKIT_NON_COPYABLE(Model)
  public:
    TKIT_BLOCK_ALLOCATED_SERIAL(Model<D>, 32)

    /**
     * @brief Constructs a model with the given vertices.
     *
     * @param p_Vertices A span of vertices to initialize the model.
     */
    Model(std::span<const Vertex<D>> p_Vertices) noexcept;

    /**
     * @brief Constructs a model with the given vertices and indices.
     *
     * @param p_Vertices A span of vertices to initialize the model.
     * @param p_Indices A span of indices for indexed drawing.
     */
    Model(std::span<const Vertex<D>> p_Vertices, std::span<const Index> p_Indices) noexcept;

    /**
     * @brief Destructor.
     */
    ~Model() noexcept;

    // TODO: Make sure no redundant bind calls are made
    // These bind and draw commands operate with a single vertex and index buffer. Not ideal when instancing could be
    // used. Plus, the same buffer may be bound multiple times if this is not handled with care

    /**
     * @brief Binds the vertex (and index) buffers to the given command buffer.
     *
     * @param p_CommandBuffer The Vulkan command buffer to bind the buffers to.
     */
    void Bind(VkCommandBuffer p_CommandBuffer) const noexcept;

    /**
     * @brief Draws the model using non-indexed drawing.
     *
     * @param p_CommandBuffer The Vulkan command buffer to issue the draw command to.
     * @param p_InstanceCount Number of instances to draw.
     * @param p_FirstInstance Offset of the first instance to draw.
     * @param p_FirstVertex Offset of the first vertex to draw.
     */
    void Draw(VkCommandBuffer p_CommandBuffer, u32 p_InstanceCount = 0, u32 p_FirstInstance = 0,
              u32 p_FirstVertex = 0) const noexcept;

    /**
     * @brief Draws the model using indexed drawing.
     *
     * @param p_CommandBuffer The Vulkan command buffer to issue the draw command to.
     * @param p_InstanceCount Number of instances to draw.
     * @param p_FirstInstance Offset of the first instance to draw.
     * @param p_FirstIndex Offset of the first index to draw.
     * @param p_VertexOffset Offset added to the vertex indices.
     */
    void DrawIndexed(VkCommandBuffer p_CommandBuffer, u32 p_InstanceCount = 0, u32 p_FirstInstance = 0,
                     u32 p_FirstIndex = 0, u32 p_VertexOffset = 0) const noexcept;

    /**
     * @brief Checks if the model has indices for indexed drawing.
     *
     * @return true if the model has indices, false otherwise.
     */
    bool HasIndices() const noexcept;

    /**
     * @brief Gets the vertex buffer of the model.
     *
     * @return const VertexBuffer<D>& Reference to the vertex buffer.
     */
    const VertexBuffer<D> &GetVertexBuffer() const noexcept;

    /**
     * @brief Gets the index buffer of the model.
     *
     * @note This function is undefined behavior if HasIndices() returns false.
     *
     * @return const IndexBuffer& Reference to the index buffer.
     */
    const IndexBuffer &GetIndexBuffer() const noexcept; // This is UB if HasIndices returns false

    /**
     * @brief Loads a model from a file.
     *
     * @param p_Path The file path to load the model from.
     * @return TKit::Scope<const Model> A scoped pointer to the loaded model.
     */
    static TKit::Scope<const Model> Load(std::string_view p_Path) noexcept;

  private:
    TKit::Ref<Device> m_Device;
    VertexBuffer<D> m_VertexBuffer;
    TKit::Storage<IndexBuffer> m_IndexBuffer;

    bool m_HasIndices;
};

} // namespace Onyx
