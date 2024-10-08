#pragma once

#include "onyx/core/device.hpp"
#include "onyx/draw/vertex.hpp"
#include "onyx/rendering/buffer.hpp"

#include "kit/container/storage.hpp"

namespace ONYX
{
// I have been struggling a bit with the design of the model class

// A model may (and in the majority of cases will) have an index buffer to save vertex memory data, but some times (for
// line strips for example) this extra buffer wont be needed. This "forces" me to have the buffers dynamically
// allocated, so that I can nullify the index buffer in case it is not needed. This extra indirection annoys me. I could
// make the buffer class default constructible and set the vulkan properties to null handles. But that just renders
// the API more confusing and unsafe. I could fix all of this with inheritance

// A model may be stored in device local memory, when it is not expected to be modified once created and thus
// cannot be mapped to a cpu memory region, or stored in a way that allows this mapping. The first option creates an
// immutable model, and the second a mutable one. All of this is handled with flags under the hood, so I can just have
// those flags be passed through the constructor and thats it. But I dont want to expose a write API when in some cases
// the model cant just be written to. I could fix all of this with inheritance

// But then what...? Have a base class Model that is immutable and only uses a vertex buffer? Thats a bland name, it
// doesnt specify a lot of the properties of the model. Should I call it then ImmutableModel? How do I specify that an
// index buffer is not used with its name? Do I even need to? (probably not). And what about the derived classes? Three
// more derived classes for the three remaining cases? Thats annoying. And now the Model class has to be virtual. I just
// dont like any of the options

// I have ended up implementing a simple basic Model class and thats it. This class is not intended to be used directly
// by my imaginary users, so I should not be thinking much about this design. I kind of new from the beginning this was
// the approach that would best work for me, but I have a difficult time sacrificing design for simplicity or viceversa

using Index = u32;

class ONYX_API Model
{
    KIT_NON_COPYABLE(Model)
  public:
    KIT_BLOCK_ALLOCATED_SERIAL(Model, 32)

    enum Properties : u8
    {
        DEVICE_LOCAL = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        HOST_VISIBLE = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        HOST_COHERENT = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    ONYX_DIMENSION_TEMPLATE Model(std::span<const Vertex<N>> p_Vertices,
                                  Properties p_VertexBufferProperties = DEVICE_LOCAL) noexcept;

    ONYX_DIMENSION_TEMPLATE Model(std::span<const Vertex<N>> p_Vertices, std::span<const Index> p_Indices,
                                  Properties p_VertexBufferProperties = DEVICE_LOCAL) noexcept;

    ~Model() noexcept;

    // TODO: Make sure no redundant bind calls are made
    // These bind and draw commands operate with a single vertex and index buffer. Not ideal when instancing could be
    // used. Plus, the same buffer may be bound multiple times if this is not handled with care
    void Bind(VkCommandBuffer p_CommandBuffer) const noexcept;
    void Draw(VkCommandBuffer p_CommandBuffer) const noexcept;

    bool HasIndices() const noexcept;

    const Buffer &GetVertexBuffer() const noexcept;
    Buffer &GetVertexBuffer() noexcept;

    bool IsMutable() const noexcept;
    bool MustFlush() const noexcept;

    static void CreatePrimitiveModels() noexcept;
    static void DestroyPrimitiveModels() noexcept;

    // These functions create a model owned by Onyx itself, which will get deleted automatically when calling
    // Core::Terminate(). These are not thread safe
    ONYX_DIMENSION_TEMPLATE static Model *Create(std::span<const Vertex<N>> p_Vertices,
                                                 Properties p_VertexBufferProperties = DEVICE_LOCAL) noexcept;
    ONYX_DIMENSION_TEMPLATE static Model *Create(std::span<const Vertex<N>> p_Vertices,
                                                 std::span<const Index> p_Indices,
                                                 Properties p_VertexBufferProperties = DEVICE_LOCAL) noexcept;
    ONYX_DIMENSION_TEMPLATE static Model *Load(std::string_view p_Path) noexcept;
    static Model *Load2D(std::string_view p_Path) noexcept;
    static Model *Load3D(std::string_view p_Path) noexcept;

    ONYX_DIMENSION_TEMPLATE static const Model *GetRegularPolygon(u32 p_Sides) noexcept;
    ONYX_DIMENSION_TEMPLATE static const Model *GetTriangle() noexcept;
    ONYX_DIMENSION_TEMPLATE static const Model *GetSquare() noexcept;
    ONYX_DIMENSION_TEMPLATE static const Model *GetLine() noexcept;
    ONYX_DIMENSION_TEMPLATE static const Model *GetCircle() noexcept;
    ONYX_DIMENSION_TEMPLATE static Model *CreatePolygon(std::span<const vec<N>> p_Vertices,
                                                        Properties p_VertexBufferProperties = DEVICE_LOCAL) noexcept;

    static const Model *GetRegularPolygon2D(u32 p_Sides) noexcept;
    static const Model *GetTriangle2D() noexcept;
    static const Model *GetSquare2D() noexcept;
    static const Model *GetLine2D() noexcept;
    static const Model *GetCircle2D() noexcept;
    static Model *CreatePolygon2D(std::span<const vec2> p_Vertices,
                                  Properties p_VertexBufferProperties = DEVICE_LOCAL) noexcept;

    static const Model *GetRegularPolygon3D(u32 p_Sides) noexcept;
    static const Model *GetTriangle3D() noexcept;
    static const Model *GetSquare3D() noexcept;
    static const Model *GetLine3D() noexcept;
    static const Model *GetCircle3D() noexcept;
    static Model *CreatePolygon3D(std::span<const vec3> p_Vertices,
                                  Properties p_VertexBufferProperties = DEVICE_LOCAL) noexcept;

    static const Model *GetCube() noexcept;
    static const Model *GetSphere() noexcept;

    // Commented for now, I dont think I will need this
    // static Model *CreatePolyhedron(std::span<const Vertex3D> p_Vertices) noexcept;

  private:
    ONYX_DIMENSION_TEMPLATE void createVertexBuffer(std::span<const Vertex<N>> p_Vertices) noexcept;
    void createIndexBuffer(std::span<const Index> p_Indices) noexcept;

    KIT::Ref<Device> m_Device;
    KIT::Storage<Buffer> m_VertexBuffer;
    KIT::Storage<Buffer> m_IndexBuffer;

    Properties m_VertexBufferProperties;

    bool m_HasIndices;
};

} // namespace ONYX