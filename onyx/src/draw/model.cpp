#include "core/pch.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Model::Model(const std::span<const Vertex<N>> p_Vertices,
                                     const Properties p_VertexBufferProperties) noexcept
    : m_VertexBufferProperties(p_VertexBufferProperties), m_HasIndices(false)
{
    m_Device = Core::GetDevice();
    createVertexBuffer(p_Vertices);
}

ONYX_DIMENSION_TEMPLATE Model::Model(const std::span<const Vertex<N>> p_Vertices,
                                     const std::span<const Index> p_Indices,
                                     const Properties p_VertexBufferProperties) noexcept
    : m_VertexBufferProperties(p_VertexBufferProperties), m_HasIndices(true)
{
    m_Device = Core::GetDevice();
    createVertexBuffer(p_Vertices);
    createIndexBuffer(p_Indices);
}

Model::~Model() noexcept
{
    if (m_HasIndices)
        m_IndexBuffer.Destroy();
    m_VertexBuffer.Destroy();
}

ONYX_DIMENSION_TEMPLATE void Model::createVertexBuffer(const std::span<const Vertex<N>> p_Vertices) noexcept
{
    KIT_ASSERT(!p_Vertices.empty(), "Cannot create model with no vertices");

    Buffer::Specs specs{};
    specs.InstanceCount = p_Vertices.size();
    specs.InstanceSize = sizeof(Vertex<N>);
    specs.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (m_VertexBufferProperties & HOST_VISIBLE)
    {
        specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        specs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        m_VertexBuffer.Create(specs);

        m_VertexBuffer->Map();
        m_VertexBuffer->Write(p_Vertices.data());
        if ((m_VertexBufferProperties & HOST_COHERENT) == 0)
            m_VertexBuffer->Flush();

        // Do not unmap. Assuming buffer will be changed if these are the flags provided
        // m_VertexBuffer->Unmap();
    }
    else
    {
        specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        specs.Usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        m_VertexBuffer.Create(specs);

        Buffer::Specs stagingSpecs = specs;
        stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        Buffer stagingBuffer(stagingSpecs);
        stagingBuffer.Map();
        stagingBuffer.Write(p_Vertices.data());
        stagingBuffer.Flush();
        stagingBuffer.Unmap();

        m_VertexBuffer->CopyFrom(stagingBuffer);
    }
}

void Model::createIndexBuffer(const std::span<const Index> p_Indices) noexcept
{
    KIT_ASSERT(!p_Indices.empty(), "If specified, indices must not be empty");
    Buffer::Specs specs{};
    specs.InstanceCount = p_Indices.size();
    specs.InstanceSize = sizeof(Index);
    specs.Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    specs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    m_IndexBuffer.Create(specs);

    Buffer::Specs stagingSpecs = specs;
    stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    Buffer stagingBuffer(stagingSpecs);
    stagingBuffer.Map();
    stagingBuffer.Write(p_Indices.data());
    stagingBuffer.Flush();
    stagingBuffer.Unmap();

    m_IndexBuffer->CopyFrom(stagingBuffer);
}

void Model::Bind(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    const VkBuffer buffer = m_VertexBuffer->GetBuffer();
    const VkDeviceSize offset = 0;

    // This actually takes an array of buffers, but I am only using one
    vkCmdBindVertexBuffers(p_CommandBuffer, 0, 1, &buffer, &offset);

    if (m_HasIndices)
        vkCmdBindIndexBuffer(p_CommandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

bool Model::HasIndices() const noexcept
{
    return m_HasIndices;
}

void Model::Draw(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    if (m_HasIndices)
        vkCmdDrawIndexed(p_CommandBuffer, static_cast<u32>(m_IndexBuffer->GetInstanceCount()), 1, 0, 0, 0);
    else
        vkCmdDraw(p_CommandBuffer, static_cast<u32>(m_VertexBuffer->GetInstanceCount()), 1, 0, 0);
}

const Buffer &Model::GetVertexBuffer() const noexcept
{
    return *m_VertexBuffer;
}
Buffer &Model::GetVertexBuffer() noexcept
{
    return *m_VertexBuffer;
}

bool Model::IsMutable() const noexcept
{
    return m_VertexBufferProperties & HOST_VISIBLE;
}
bool Model::MustFlush() const noexcept
{
    return !(m_VertexBufferProperties & HOST_COHERENT);
}

ONYX_DIMENSION_TEMPLATE Model *Model::Create(const std::span<const Vertex<N>> p_Vertices,
                                             const Properties p_VertexBufferProperties) noexcept
{
    Model *model = new Model(p_Vertices, p_VertexBufferProperties);
    s_UserModels.push_back(model);
    return model;
}

ONYX_DIMENSION_TEMPLATE Model *Model::Create(const std::span<const Vertex<N>> p_Vertices,
                                             const std::span<const Index> p_Indices,
                                             const Properties p_VertexBufferProperties) noexcept
{
    Model *model = new Model(p_Vertices, p_Indices, p_VertexBufferProperties);
    s_UserModels.push_back(model);
    return model;
}

// this loads and stores the model in the user models
ONYX_DIMENSION_TEMPLATE Model *Model::Load(const std::string_view p_Path) noexcept
{
    Model *model = load<N>(p_Path);
    s_UserModels.push_back(model);
    return model;
}
Model *Model::Load2D(const std::string_view p_Path) noexcept
{
    return Load<2>(p_Path);
}
Model *Model::Load3D(const std::string_view p_Path) noexcept
{
    return Load<3>(p_Path);
}

void Model::CreatePrimitiveModels() noexcept
{
    createAllRegularPolygonModels<2, ONYX_MAX_REGULAR_POLYGON_SIDES>();
    createAllRegularPolygonModels<3, ONYX_MAX_REGULAR_POLYGON_SIDES>();

    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES] = createLineModel<2>();
    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 1] = createLineModel<3>();

    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 2] = loadCircleModel<2>();
    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 3] = loadCircleModel<3>();

    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 4] = loadCubeModel();
    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 5] = loadSphereModel();
}

void Model::DestroyPrimitiveModels() noexcept
{
    for (const Model *model : s_UserModels)
        delete model;
    for (const Model *model : s_Models)
        delete model;
}

ONYX_DIMENSION_TEMPLATE const Model *Model::GetRegularPolygon(const u32 p_Sides) noexcept
{
    KIT_ASSERT(p_Sides >= 3, "Regular polygon must have at least 3 sides");
    KIT_ASSERT(p_Sides <= ONYX_MAX_REGULAR_POLYGON_SIDES, "Regular polygon must have at most {} sides",
               ONYX_MAX_REGULAR_POLYGON_SIDES);
    return s_Models[p_Sides - 3 + (N - 2) * ONYX_MAX_REGULAR_POLYGON_SIDES];
}
ONYX_DIMENSION_TEMPLATE const Model *Model::GetTriangle() noexcept
{
    return s_Models[(N - 2) * ONYX_MAX_REGULAR_POLYGON_SIDES];
}
ONYX_DIMENSION_TEMPLATE const Model *Model::GetSquare() noexcept
{
    return s_Models[1 + (N - 2) * ONYX_MAX_REGULAR_POLYGON_SIDES];
}
ONYX_DIMENSION_TEMPLATE const Model *Model::GetLine() noexcept
{
    return s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + (N - 2)];
}
ONYX_DIMENSION_TEMPLATE const Model *Model::GetCircle() noexcept
{
    return s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 2 + (N - 2)];
}
ONYX_DIMENSION_TEMPLATE Model *Model::CreatePolygon(const std::span<const vec<N>> p_Vertices,
                                                    const Properties p_VertexBufferProperties) noexcept
{
    KIT_ASSERT(p_Vertices.size() >= 3, "Polygon must have at least 3 vertices");
    DynamicArray<Vertex<N>> vertices;
    DynamicArray<Index> indices{static_cast<u32>(p_Vertices.size() * 3)};

    vertices.reserve(p_Vertices.size() + 1);

    if constexpr (N == 2)
        vertices.push_back(Vertex<N>{vec<N>{0.f}});
    else
        vertices.push_back(Vertex<N>{vec<N>{0.f}, {0.f, 0.f, 1.f}});
    for (Index i = 0; i < p_Vertices.size(); ++i)
    {
        if constexpr (N == 2)
            vertices.push_back(Vertex<N>{p_Vertices[i]});
        else
            vertices.push_back(Vertex<N>{p_Vertices[i], {0.f, 0.f, 1.f}});

        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    indices.back() = 1;

    const std::span<const Vertex<N>> verticesSpan{vertices};
    const std::span<const Index> indicesSpan{indices};
    Model *model = new Model(verticesSpan, indicesSpan, p_VertexBufferProperties);
    s_UserModels.push_back(model);
    return model;
}

const Model *Model::GetRegularPolygon2D(const u32 p_Sides) noexcept
{
    return GetRegularPolygon<2>(p_Sides);
}
const Model *Model::GetTriangle2D() noexcept
{
    return GetTriangle<2>();
}
const Model *Model::GetSquare2D() noexcept
{
    return GetSquare<2>();
}
const Model *Model::GetLine2D() noexcept
{
    return GetLine<2>();
}
const Model *Model::GetCircle2D() noexcept
{
    return GetCircle<2>();
}
Model *Model::CreatePolygon2D(const std::span<const vec2> p_Vertices,
                              const Properties p_VertexBufferProperties) noexcept
{
    return CreatePolygon<2>(p_Vertices, p_VertexBufferProperties);
}

const Model *Model::GetRegularPolygon3D(const u32 p_Sides) noexcept
{
    return GetRegularPolygon<3>(p_Sides);
}
const Model *Model::GetTriangle3D() noexcept
{
    return GetTriangle<3>();
}
const Model *Model::GetSquare3D() noexcept
{
    return GetSquare<3>();
}
const Model *Model::GetLine3D() noexcept
{
    return GetLine<3>();
}
const Model *Model::GetCircle3D() noexcept
{
    return GetCircle<3>();
}
Model *Model::CreatePolygon3D(const std::span<const vec3> p_Vertices,
                              const Properties p_VertexBufferProperties) noexcept
{
    return CreatePolygon<3>(p_Vertices, p_VertexBufferProperties);
}

const Model *Model::GetCube() noexcept
{
    return s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 4];
}
const Model *Model::GetSphere() noexcept
{
    return s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 5];
}

// template Model::Model(const std::span<const Vertex<2>>, const Properties) noexcept;
// template Model::Model(const std::span<const Vertex<2>>, const std::span<const Index>, const Properties) noexcept;
// template Model::Model(const std::span<const Vertex<3>>, const Properties) noexcept;
// template Model::Model(const std::span<const Vertex<3>>, const std::span<const Index>, const Properties) noexcept;

// template void Model::createVertexBuffer(const std::span<const Vertex<2>>, const Properties) noexcept;
// template void Model::createVertexBuffer(const std::span<const Vertex<3>>, const Properties) noexcept;

} // namespace ONYX