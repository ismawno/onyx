#pragma once

#include "onyx/draw/vertex.hpp"
#include "onyx/draw/data.hpp"

#include "tkit/container/storage.hpp"

namespace Onyx
{
// Consider removing the ability to create a model with host visible memory (DONE)
// This model represents an immutable set of data that is meant to be used for rendering. It is not meant to be modified

/**
 * @brief Represents an immutable model used for rendering.
 *
 * This class encapsulates vertex and optional index data,
 * and provides methods for binding and drawing the model.
 *
 * It is not intended to be modified after creation. It must be manually destroyed when no longer needed using the
 * `Destroy()` method.
 *
 * @tparam D The dimensionality of the model (`D2` or `D3`).
 */
template <Dimension D> class Model
{
  public:
    /**
     * @brief Creates a model with the given vertices.
     *
     * @param p_Vertices A span of vertices to initialize the model.
     */
    static VKit::Result<Model> Create(TKit::Span<const Vertex<D>> p_Vertices) noexcept;

    /**
     * @brief Creates a model with the given vertices and indices.
     *
     * @param p_Vertices A span of vertices to initialize the model.
     * @param p_Indices A span of indices for indexed drawing.
     */
    static VKit::Result<Model> Create(TKit::Span<const Vertex<D>> p_Vertices,
                                      TKit::Span<const Index> p_Indices) noexcept;

    Model() noexcept = default;
    Model(const DeviceLocalVertexBuffer<D> &p_VertexBuffer) noexcept;
    Model(const DeviceLocalVertexBuffer<D> &p_VertexBuffer, const DeviceLocalIndexBuffer &p_IndexBuffer) noexcept;

    // TODO: Make sure no redundant bind calls are made
    // These bind and draw commands operate with a single vertex and index buffer. Not ideal when instancing could be
    // used. Plus, the same buffer may be bound multiple times if this is not handled with care

    /**
     * @brief Destroys the model and releases its resources.
     */
    void Destroy() noexcept;

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
     * @return Reference to the vertex buffer.
     */
    const DeviceLocalVertexBuffer<D> &GetVertexBuffer() const noexcept;

    /**
     * @brief Gets the index buffer of the model.
     *
     * @note This function is undefined behavior if HasIndices() returns false.
     *
     * @return Reference to the index buffer.
     */
    const DeviceLocalIndexBuffer &GetIndexBuffer() const noexcept; // This is UB if HasIndices returns false

    /**
     * @brief Loads a model from a file.
     *
     * @param p_Path The file path to load the model from.
     * @return A result containing the loaded model or an error.
     */
    static VKit::FormattedResult<Model> Load(std::string_view p_Path) noexcept;

    friend bool operator==(const Model &p_Lhs, const Model &p_Rhs) noexcept
    {
        return p_Lhs.m_VertexBuffer.GetBuffer() == p_Rhs.m_VertexBuffer.GetBuffer() &&
               p_Lhs.m_IndexBuffer.GetBuffer() == p_Rhs.m_IndexBuffer.GetBuffer();
    }

  private:
    DeviceLocalVertexBuffer<D> m_VertexBuffer{};
    DeviceLocalIndexBuffer m_IndexBuffer{};
};

} // namespace Onyx

namespace std
{
template <Onyx::Dimension D> struct hash<Onyx::Model<D>>
{
    size_t operator()(const Onyx::Model<D> &p_Model) const noexcept;
};
} // namespace std
