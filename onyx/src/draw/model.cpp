#include "core/pch.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

#ifndef ONYX_CIRCLE_VERTICES
#    define ONYX_CIRCLE_VERTICES 32
#endif

#ifndef ONYX_SPHERE_LATITUDE_PARTITIONS
#    define ONYX_SPHERE_LATITUDE_PARTITIONS 32
#endif

#ifndef ONYX_SPHERE_LONGITUDE_PARTITIONS
#    define ONYX_SPHERE_LONGITUDE_PARTITIONS 32
#endif

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Model::Model(const std::span<const Vertex<N>> p_Vertices,
                                     const Properties p_VertexBufferProperties) noexcept
{
    m_Device = Core::GetDevice();
    createVertexBuffer(p_Vertices, p_VertexBufferProperties);
}

ONYX_DIMENSION_TEMPLATE Model::Model(const std::span<const Vertex<N>> p_Vertices,
                                     const std::span<const Index> p_Indices,
                                     const Properties p_VertexBufferProperties) noexcept
{
    m_Device = Core::GetDevice();
    createVertexBuffer(p_Vertices, p_VertexBufferProperties);
    createIndexBuffer(p_Indices);
}

ONYX_DIMENSION_TEMPLATE void Model::createVertexBuffer(const std::span<const Vertex<N>> p_Vertices,
                                                       const Properties p_VertexBufferProperties) noexcept
{
    KIT_ASSERT(!p_Vertices.empty(), "Cannot create model with no vertices");

    Buffer::Specs specs{};
    specs.InstanceCount = p_Vertices.size();
    specs.InstanceSize = sizeof(Vertex<N>);
    specs.Properties = p_VertexBufferProperties;
    if (p_VertexBufferProperties & HOST_VISIBLE)
    {
        specs.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        m_VertexBuffer = KIT::Scope<Buffer>::Create(specs);

        m_VertexBuffer->Map();
        m_VertexBuffer->Write(p_Vertices.data());
        if ((p_VertexBufferProperties & HOST_COHERENT) == 0)
            m_VertexBuffer->Flush();

        // Do not unmap. Assuming buffer will be changed if these are the flags provided
        // m_VertexBuffer->Unmap();
    }
    else
    {
        specs.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        m_VertexBuffer = KIT::Scope<Buffer>::Create(specs);

        Buffer::Specs stagingSpecs = specs;
        stagingSpecs.Properties = HOST_VISIBLE;
        stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

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
    specs.Properties = DEVICE_LOCAL;
    specs.Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    m_IndexBuffer = KIT::Scope<Buffer>::Create(specs);

    Buffer::Specs stagingSpecs = specs;
    stagingSpecs.Properties = HOST_VISIBLE;
    stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

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

    if (m_IndexBuffer)
        vkCmdBindIndexBuffer(p_CommandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

bool Model::HasIndices() const noexcept
{
    return m_IndexBuffer != nullptr;
}

void Model::Draw(const VkCommandBuffer p_CommandBuffer) const noexcept
{
    if (m_IndexBuffer)
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

static const Model *s_Rectangle2D;
static const Model *s_Line2D;
static const Model *s_Circle2D;

static const Model *s_Rectangle3D;
static const Model *s_Line3D;
static const Model *s_Circle3D;

static const Model *s_Cube3D;
static const Model *s_Sphere3D;

ONYX_DIMENSION_TEMPLATE static const Model *createRectangleModel()
{
    if constexpr (N == 2)
    {
        static constexpr std::array<Vertex<2>, 4> vertices{
            Vertex<2>{{-.5f, -.5f}},
            Vertex<2>{{.5f, -.5f}},
            Vertex<2>{{.5f, .5f}},
            Vertex<2>{{-.5f, .5f}},
        };
        static constexpr std::array<Index, 6> indices{0, 1, 2, 2, 3, 0};
        static constexpr std::span<const Vertex<2>> verticesSpan{vertices};
        static constexpr std::span<const Index> indicesSpan{indices};
        return new Model(verticesSpan, indicesSpan);
    }
    else
    {
        static constexpr std::array<Vertex<3>, 4> vertices{
            Vertex<3>{{-.5f, -.5f, .5f}},
            Vertex<3>{{.5f, -.5f, .5f}},
            Vertex<3>{{.5f, .5f, .5f}},
            Vertex<3>{{-.5f, .5f, .5f}},
        };
        static constexpr std::array<Index, 6> indices{0, 1, 2, 2, 3, 0};
        static constexpr std::span<const Vertex<3>> verticesSpan{vertices};
        static constexpr std::span<const Index> indicesSpan{indices};
        return new Model(verticesSpan, indicesSpan);
    }
}

ONYX_DIMENSION_TEMPLATE static const Model *createLineModel()
{
    if constexpr (N == 2)
    {
        static constexpr std::array<Vertex<2>, 2> vertices{
            Vertex<2>{{-.5f, 0.f}},
            Vertex<2>{{.5f, 0.f}},
        };
        static constexpr std::span<const Vertex<2>> verticesSpan{vertices};
        return new Model(verticesSpan);
    }
    else
    {
        static constexpr std::array<Vertex<3>, 2> vertices{
            Vertex<3>{{-.5f, 0.f, 0.f}},
            Vertex<3>{{.5f, 0.f, 0.f}},
        };
        static constexpr std::span<const Vertex<3>> verticesSpan{vertices};
        return new Model(verticesSpan);
    }
}

ONYX_DIMENSION_TEMPLATE static const Model *createCircleModel()
{
    std::array<Vertex<N>, ONYX_CIRCLE_VERTICES> vertices;
    std::array<Index, ONYX_CIRCLE_VERTICES * 3> indices;

    const f32 angle = 2.f * glm::pi<f32>() / (ONYX_CIRCLE_VERTICES - 1);
    for (Index i = 0; i < ONYX_CIRCLE_VERTICES; ++i)
    {
        const f32 x = glm::cos(angle * i);
        const f32 y = glm::sin(angle * i);
        if constexpr (N == 2)
            vertices[i] = Vertex<N>{vec<N>{x, y}};
        else
            vertices[i] = Vertex<N>{vec<N>{x, y, 0.f}};

        indices[i * 3] = 0;
        indices[i * 3 + 1] = i;
        indices[i * 3 + 2] = i + 1;
    }

    const std::span<const Vertex<N>> verticesSpan{vertices};
    const std::span<const Index> indicesSpan{indices};
    return new Model(verticesSpan, indicesSpan);
}

static const Model *createCubeModel() noexcept
{
    static constexpr std::array<Vertex<3>, 8> vertices{
        Vertex<3>{{-.5f, -.5f, -.5f}}, Vertex<3>{{-.5f, .5f, .5f}},
        Vertex<3>{{-.5f, -.5f, .5f}},  Vertex<3>{{-.5f, .5f, -.5f}},

        Vertex<3>{{.5f, -.5f, -.5f}},  Vertex<3>{{.5f, .5f, .5f}},
        Vertex<3>{{.5f, -.5f, .5f}},   Vertex<3>{{.5f, .5f, -.5f}},

    };
    static constexpr std::array<Index, 36> indices{0, 1, 2, 0, 3, 1, 4, 5, 6, 4, 7, 5, 0, 6, 2, 0, 4, 6,
                                                   3, 5, 1, 3, 7, 5, 2, 5, 1, 2, 6, 5, 0, 7, 3, 0, 4, 7};
    static constexpr std::span<const Vertex<3>> verticesSpan{vertices};
    static constexpr std::span<const Index> indicesSpan{indices};
    return new Model(verticesSpan, indicesSpan);
}

static const Model *createSphereModel() noexcept
{
    static constexpr Index latPartitions = ONYX_SPHERE_LATITUDE_PARTITIONS;
    static constexpr Index lonPartitions = ONYX_SPHERE_LONGITUDE_PARTITIONS;

    static_assert(latPartitions >= 3, "Latitude partitions must be at least 3");
    static_assert(lonPartitions >= 3, "Longitude partitions must be at least 3");

    std::array<Vertex<3>, latPartitions * lonPartitions> vertices;
    std::array<Index, latPartitions * lonPartitions * 3 + 2> indices;

    const f32 latAngle = glm::pi<f32>() / (latPartitions - 1);
    const f32 lonAngle = glm::two_pi<f32>() / (lonPartitions - 1);

    Index vindex = 0;
    Index iindex = 0;

    vertices[vindex++] = Vertex<3>{{0.f, 0.f, 1.f}};
    for (Index i = 1; i < lonPartitions - 1; ++i)
    {
        const f32 lon = lonAngle * i;
        const f32 xy = glm::sin(lon);
        const f32 z = glm::cos(lon);
        for (Index j = 0; j < latPartitions; ++j)
        {
            const f32 lat = latAngle * j;
            const f32 x = xy * glm::sin(lat);
            const f32 y = xy * glm::cos(lat);
            vertices[vindex++] = Vertex<3>{{x, y, z}};
        }
    }
    vertices[vindex++] = Vertex<3>{{0.f, 0.f, -1.f}};

    for (Index j = 0; j < latPartitions; ++j)
    {
        indices[iindex++] = 0;
        indices[iindex++] = j + 1;
        indices[iindex++] = (j + 2) % (latPartitions + 1);
    }

    for (Index i = 0; i < lonPartitions - 3; ++i)
        for (Index j = 0; j < latPartitions; ++j)
        {
            const Index idx1 = latPartitions * i + j + 1;
            const Index idx21 = latPartitions * (i + 1) + j + 1;
            const Index idx12 = latPartitions * i + (j + 2) % (latPartitions + 1);
            const Index idx2 = latPartitions * (i + 1) + (j + 2) % (latPartitions + 1);

            indices[iindex++] = idx1;
            indices[iindex++] = idx21;
            indices[iindex++] = idx2;

            indices[iindex++] = idx1;
            indices[iindex++] = idx12;
            indices[iindex++] = idx2;
        }
    for (Index j = 0; j < latPartitions; ++j)
    {
        indices[iindex++] = vindex - 1;
        indices[iindex++] = latPartitions * (lonPartitions - 3) + (j + 1);
        indices[iindex++] = latPartitions * (lonPartitions - 3) + (j + 2) % (latPartitions + 1);
    }

    const std::span<const Vertex<3>> verticesSpan{vertices};
    const std::span<const Index> indicesSpan{indices};
    return new Model(verticesSpan, indicesSpan);
}

void Model::CreatePrimitiveModels() noexcept
{
    s_Rectangle2D = createRectangleModel<2>();
    s_Rectangle3D = createRectangleModel<3>();

    s_Line2D = createLineModel<2>();
    s_Line3D = createLineModel<3>();

    s_Circle2D = createCircleModel<2>();
    s_Circle3D = createCircleModel<3>();

    s_Cube3D = createCubeModel();
    s_Sphere3D = createSphereModel();
}

void Model::DestroyPrimitiveModels() noexcept
{
    delete s_Rectangle2D;
    delete s_Rectangle3D;

    delete s_Line2D;
    delete s_Line3D;

    delete s_Circle2D;
    delete s_Circle3D;

    delete s_Cube3D;
    delete s_Sphere3D;
}

ONYX_DIMENSION_TEMPLATE const Model *Model::GetRectangle() noexcept
{
    if constexpr (N == 2)
        return s_Rectangle2D;
    else
        return s_Rectangle3D;
}
ONYX_DIMENSION_TEMPLATE const Model *Model::GetLine() noexcept
{
    if constexpr (N == 2)
        return s_Line2D;
    else
        return s_Line3D;
}
ONYX_DIMENSION_TEMPLATE const Model *Model::GetCircle() noexcept
{
    if constexpr (N == 2)
        return s_Circle2D;
    else
        return s_Circle3D;
}

const Model *Model::GetRectangle2D() noexcept
{
    return GetRectangle<2>();
}
const Model *Model::GetLine2D() noexcept
{
    return GetLine<2>();
}
const Model *Model::GetCircle2D() noexcept
{
    return GetCircle<2>();
}

const Model *Model::GetRectangle3D() noexcept
{
    return GetRectangle<3>();
}
const Model *Model::GetLine3D() noexcept
{
    return GetLine<3>();
}
const Model *Model::GetCircle3D() noexcept
{
    return GetCircle<3>();
}

const Model *Model::GetCube() noexcept
{
    return s_Cube3D;
}
const Model *Model::GetSphere() noexcept
{
    return s_Sphere3D;
}

// template Model::Model(const std::span<const Vertex<2>>, const Properties) noexcept;
// template Model::Model(const std::span<const Vertex<2>>, const std::span<const Index>, const Properties) noexcept;
// template Model::Model(const std::span<const Vertex<3>>, const Properties) noexcept;
// template Model::Model(const std::span<const Vertex<3>>, const std::span<const Index>, const Properties) noexcept;

// template void Model::createVertexBuffer(const std::span<const Vertex<2>>, const Properties) noexcept;
// template void Model::createVertexBuffer(const std::span<const Vertex<3>>, const Properties) noexcept;

} // namespace ONYX