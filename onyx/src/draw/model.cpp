#include "core/pch.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

#ifndef ONYX_MAX_REGULAR_POLYGON_SIDES
#    define ONYX_MAX_REGULAR_POLYGON_SIDES 32
#endif

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

// Array layout:
// 0 - ONYX_MAX_REGULAR_POLYGON_SIDES - 1: 2D regular polygons
// ONYX_MAX_REGULAR_POLYGON_SIDES - 2 * ONYX_MAX_REGULAR_POLYGON_SIDES - 1: 3D regular polygons
// ONYX_MAX_REGULAR_POLYGON_SIDES * 2: Line 2D
// ONYX_MAX_REGULAR_POLYGON_SIDES * 2 + 1: Line 3D
// ONYX_MAX_REGULAR_POLYGON_SIDES * 2 + 2: Circle 2D
// ONYX_MAX_REGULAR_POLYGON_SIDES * 2 + 3: Circle 3D
// ONYX_MAX_REGULAR_POLYGON_SIDES * 2 + 4: Rectangular Prism
// ONYX_MAX_REGULAR_POLYGON_SIDES * 2 + 5: Sphere

static std::array<const Model *, 2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 6> s_Models;
static DynamicArray<const Model *> s_UserModels;

ONYX_DIMENSION_TEMPLATE static const Model *createTriangleModel() noexcept
{
    if constexpr (N == 2)
    {
        static constexpr std::array<Vertex<2>, 3> vertices{
            Vertex<2>{{0.f, -.5f}},
            Vertex<2>{{.5f, .5f}},
            Vertex<2>{{-.5f, .5f}},
        };
        static constexpr std::span<const Vertex<2>> verticesSpan{vertices};
        return new Model(verticesSpan);
    }
    else
    {
        static constexpr std::array<Vertex<3>, 3> vertices{
            Vertex<3>{{0.f, -.5f, 0.f}},
            Vertex<3>{{.5f, .5f, 0.f}},
            Vertex<3>{{-.5f, .5f, 0.f}},
        };
        static constexpr std::span<const Vertex<3>> verticesSpan{vertices};
        return new Model(verticesSpan);
    }
}

ONYX_DIMENSION_TEMPLATE static const Model *createRectangleModel() noexcept
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
            Vertex<3>{{-.5f, -.5f, 0.f}},
            Vertex<3>{{.5f, -.5f, 0.f}},
            Vertex<3>{{.5f, .5f, 0.f}},
            Vertex<3>{{-.5f, .5f, 0.f}},
        };
        static constexpr std::array<Index, 6> indices{0, 1, 2, 2, 3, 0};
        static constexpr std::span<const Vertex<3>> verticesSpan{vertices};
        static constexpr std::span<const Index> indicesSpan{indices};
        return new Model(verticesSpan, indicesSpan);
    }
}

template <u32 N, usize Sides>
    requires(Sides > 2)
static const Model *createRegularPolygonModel()
{
    if constexpr (Sides == 3)
        return createTriangleModel<N>();
    else if constexpr (Sides == 4)
        return createRectangleModel<N>();
    else
    {
        std::array<Vertex<N>, Sides> vertices;
        std::array<Index, Sides * 3> indices;

        const f32 angle = 2.f * glm::pi<f32>() / (Sides - 1);
        for (Index i = 0; i < Sides; ++i)
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
}

template <u32 N, usize MaxSides> static void createAllRegularPolygonModels()
{
    if constexpr (MaxSides < 3)
        return;
    else
    {
        static constexpr usize index = MaxSides - 3 + (N - 2) * ONYX_MAX_REGULAR_POLYGON_SIDES;
        s_Models[index] = createRegularPolygonModel<N, MaxSides>();
        createAllRegularPolygonModels<N, MaxSides - 1>();
    }
}

ONYX_DIMENSION_TEMPLATE static const Model *createLineModel() noexcept
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

ONYX_DIMENSION_TEMPLATE static const Model *createCircleModel() noexcept
{
    return createRegularPolygonModel<N, ONYX_CIRCLE_VERTICES>();
}

static const Model *createRectangularPrismModel() noexcept
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

    std::array<Vertex<3>, latPartitions *(lonPartitions - 2) + 2> vertices;
    std::array<Index, 6 * latPartitions *(lonPartitions - 2)> indices;

    static constexpr f32 latAngle = glm::pi<f32>() / (latPartitions - 1);
    static constexpr f32 lonAngle = glm::two_pi<f32>() / (lonPartitions - 1);

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

void Model::CreatePrimitiveModels() noexcept
{
    createAllRegularPolygonModels<2, ONYX_MAX_REGULAR_POLYGON_SIDES>();
    createAllRegularPolygonModels<3, ONYX_MAX_REGULAR_POLYGON_SIDES>();

    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES] = createLineModel<2>();
    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 1] = createLineModel<3>();

    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 2] = createCircleModel<2>();
    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 3] = createCircleModel<3>();

    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 4] = createRectangularPrismModel();
    s_Models[2 * ONYX_MAX_REGULAR_POLYGON_SIDES + 5] = createSphereModel();
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
ONYX_DIMENSION_TEMPLATE const Model *Model::GetRectangle() noexcept
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

    vertices.push_back(Vertex<N>{vec<N>{0.f}});
    for (Index i = 0; i < p_Vertices.size(); ++i)
    {
        vertices.push_back(Vertex<N>{p_Vertices[i]});
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
Model *Model::CreatePolygon3D(const std::span<const vec3> p_Vertices,
                              const Properties p_VertexBufferProperties) noexcept
{
    return CreatePolygon<3>(p_Vertices, p_VertexBufferProperties);
}

const Model *Model::GetRectangularPrism() noexcept
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