#pragma once

#include "onyx/data/buffers.hpp"

namespace Onyx
{
// Consider removing the ability to create a mesh with host visible memory (DONE)
// This mesh represents an immutable set of data that is meant to be used for rendering. It is not meant to be modified

/**
 * @brief Represents an immutable mesh used for rendering.
 *
 * This class encapsulates vertex and optional index data,
 * and provides methods for binding and drawing the mesh.
 *
 * It is not intended to be modified after creation. It must be manually destroyed when no longer needed using the
 * `Destroy()` method.
 *
 * @tparam D The dimensionality of the mesh (`D2` or `D3`).
 */
template <Dimension D> class Mesh
{
  public:
    /**
     * @brief Creates a mesh with the given vertices.
     *
     * @param p_Vertices A host buffer of vertices to initialize the mesh.
     */
    static VKit::Result<Mesh> Create(const HostVertexBuffer<D> &p_Vertices) noexcept;

    /**
     * @brief Creates a mesh with the given vertices and indices.
     *
     * @param p_Vertices A host buffer of vertices to initialize the mesh.
     * @param p_Indices A host buffer of indices for indexed drawing.
     */
    static VKit::Result<Mesh> Create(const HostVertexBuffer<D> &p_Vertices, const HostIndexBuffer &p_Indices) noexcept;

    /**
     * @brief Creates a mesh with the given index and vertex data.
     *
     * @param p_Data The index and vertex data to initialize the mesh.
     */
    static VKit::Result<Mesh> Create(const IndexVertexHostData<D> &p_Data) noexcept;

    Mesh() noexcept = default;
    Mesh(const DeviceLocalVertexBuffer<D> &p_VertexBuffer) noexcept;
    Mesh(const DeviceLocalVertexBuffer<D> &p_VertexBuffer, const DeviceLocalIndexBuffer &p_IndexBuffer) noexcept;

    // TODO: Make sure no redundant bind calls are made
    // These bind and draw commands operate with a single vertex and index buffer. Not ideal when instancing could be
    // used. Plus, the same buffer may be bound multiple times if this is not handled with care

    /**
     * @brief Destroys the mesh and releases its resources.
     */
    void Destroy() noexcept;

    /**
     * @brief Binds the vertex (and index) buffers to the given command buffer.
     *
     * @param p_CommandBuffer The Vulkan command buffer to bind the buffers to.
     */
    void Bind(VkCommandBuffer p_CommandBuffer) const noexcept;

    /**
     * @brief Draws the mesh using non-indexed drawing.
     *
     * @param p_CommandBuffer The Vulkan command buffer to issue the draw command to.
     * @param p_InstanceCount Number of instances to draw.
     * @param p_FirstInstance Offset of the first instance to draw.
     * @param p_FirstVertex Offset of the first vertex to draw.
     */
    void Draw(VkCommandBuffer p_CommandBuffer, u32 p_InstanceCount = 0, u32 p_FirstInstance = 0,
              u32 p_FirstVertex = 0) const noexcept;

    /**
     * @brief Draws the mesh using indexed drawing.
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
     * @brief Checks if the mesh has indices for indexed drawing.
     *
     * @return true if the mesh has indices, false otherwise.
     */
    bool HasIndices() const noexcept;

    /**
     * @brief Gets the vertex buffer of the mesh.
     *
     * @return Reference to the vertex buffer.
     */
    const DeviceLocalVertexBuffer<D> &GetVertexBuffer() const noexcept;

    /**
     * @brief Gets the index buffer of the mesh.
     *
     * @note This function is undefined behavior if HasIndices() returns false.
     *
     * @return Reference to the index buffer.
     */
    const DeviceLocalIndexBuffer &GetIndexBuffer() const noexcept; // This is UB if HasIndices returns false

    /**
     * @brief Loads a mesh from a file.
     *
     * @param p_Path The file path to load the mesh from.
     * @param p_Transform An optional transform to be applied to all vertices of the mesh.
     * @return A result containing the loaded mesh or an error.
     */
    static VKit::FormattedResult<Mesh> Load(std::string_view p_Path, const fmat<D> *p_Transform = nullptr) noexcept;

    friend bool operator==(const Mesh &p_Lhs, const Mesh &p_Rhs) noexcept
    {
        return p_Lhs.m_VertexBuffer.GetHandle() == p_Rhs.m_VertexBuffer.GetHandle() &&
               p_Lhs.m_IndexBuffer.GetHandle() == p_Rhs.m_IndexBuffer.GetHandle();
    }

    operator bool() const noexcept;

  private:
    DeviceLocalVertexBuffer<D> m_VertexBuffer{};
    DeviceLocalIndexBuffer m_IndexBuffer{};
};

} // namespace Onyx

namespace std
{
template <Onyx::Dimension D> struct hash<Onyx::Mesh<D>>
{
    size_t operator()(const Onyx::Mesh<D> &p_Mesh) const noexcept;
};
} // namespace std
