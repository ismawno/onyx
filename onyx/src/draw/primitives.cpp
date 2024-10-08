#include "core/pch.hpp"
#include "onyx/draw/primitives.hpp"
#include "kit/container/storage.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE using BufferLayout = std::array<PrimitiveDataLayout, Primitives<N>::AMOUNT>;
ONYX_DIMENSION_TEMPLATE struct IndexVertexBuffers
{
    IndexVertexBuffers(const Buffer::Specs &p_VertexSpecs, const Buffer::Specs &p_IndexSpecs,
                       const BufferLayout &p_Layout) noexcept
        : Vertices{p_VertexSpecs}, Indices{p_IndexSpecs}, Layout{p_Layout}
    {
    }

    Buffer Vertices;
    Buffer Indices;
    BufferLayout Layout;
};

static KIT::Storage<IndexVertexBuffers<2>> s_Buffers2D;
static KIT::Storage<IndexVertexBuffers<3>> s_Buffers3D;

ONYX_DIMENSION_TEMPLATE static KIT::Storage<IndexVertexBuffers<N>> &getBuffers() noexcept
{
    if constexpr (N == 2)
        return s_Buffers2D;
    else
        return s_Buffers3D;
}

ONYX_DIMENSION_TEMPLATE const Buffer *IPrimitives<N>::GetVertexBuffer() noexcept
{
    return &getBuffers<N>()->Vertices;
}
ONYX_DIMENSION_TEMPLATE const Buffer *IPrimitives<N>::GetIndexBuffer() noexcept
{
    return &getBuffers<N>()->Indices;
}
ONYX_DIMENSION_TEMPLATE const PrimitiveDataLayout &IPrimitives<N>::GetDataLayout(const usize p_PrimitiveIndex) noexcept
{
    return getBuffers<N>()->Layout[p_PrimitiveIndex];
}

ONYX_DIMENSION_TEMPLATE usize IPrimitives<N>::GetTriangleIndex() noexcept
{
    return 0;
}
ONYX_DIMENSION_TEMPLATE usize IPrimitives<N>::GetSquareIndex() noexcept
{
    return 1;
}
ONYX_DIMENSION_TEMPLATE usize IPrimitives<N>::GetNGonIndex(const u32 p_Sides) noexcept
{
    KIT_ASSERT(p_Sides < ONYX_MAX_REGULAR_POLYGON_SIDES && p_Sides >= 3, "NGon sides must be between 3 and {}",
               ONYX_MAX_REGULAR_POLYGON_SIDES);
    return (N - 1) * 2 + p_Sides - 3;
}

usize Primitives<3>::GetCubeIndex() noexcept
{
    return 2;
}
usize Primitives<3>::GetSphereIndex() noexcept
{
    return 3;
}

ONYX_DIMENSION_TEMPLATE static void createBuffers(const std::span<const char *const> p_Paths) noexcept
{
    BufferLayout layout;
    IndexVertexData<N> data{};

    for (usize i = 0; i < Primitives<N>::AMOUNT; ++i)
    {
        const IndexVertexData<N> buffers = Load<N>(p_Paths[i]);

        layout[i].VerticesStart = data.Vertices.size();
        layout[i].VerticesSize = buffers.Vertices.size();
        layout[i].IndicesStart = data.Indices.size();
        layout[i].IndicesSize = buffers.Indices.size();

        data.Vertices.insert(data.Vertices.end(), buffers.Vertices.begin(), buffers.Vertices.end());
        data.Indices.insert(data.Indices.end(), buffers.Indices.begin(), buffers.Indices.end());
    }
    auto &buffers = getBuffers<N>();

    Buffer::Specs vertexSpecs{};
    vertexSpecs.InstanceCount = data.Vertices.size();
    vertexSpecs.InstanceSize = sizeof(Vertex<N>);
    vertexSpecs.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertexSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    Buffer::Specs indexSpecs{};
    indexSpecs.InstanceCount = data.Indices.size();
    indexSpecs.InstanceSize = sizeof(Index);
    indexSpecs.Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    indexSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    buffers.Create(vertexSpecs, indexSpecs, layout);

    Buffer::Specs stagingSpecs = vertexSpecs;
    stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    Buffer vertexStaging(stagingSpecs);
    vertexStaging.Map();
    vertexStaging.Write(data.Vertices);
    vertexStaging.Flush();
    vertexStaging.Unmap();

    buffers->Vertices.CopyFrom(vertexStaging);

    stagingSpecs = indexSpecs;
    stagingSpecs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    Buffer indexStaging(stagingSpecs);
    indexStaging.Map();
    indexStaging.Write(data.Indices);
    indexStaging.Flush();
    indexStaging.Unmap();

    buffers->Indices.CopyFrom(indexStaging);
}

void CreateCombinedPrimitiveBuffers() noexcept
{
    const std::array<const char *, Primitives2D::AMOUNT> paths2D = {ONYX_ROOT_PATH "/onyx/models/triangle.obj",
                                                                    ONYX_ROOT_PATH "/onyx/models/square.obj"};

    const std::array<const char *, Primitives3D::AMOUNT> paths3D = {
        ONYX_ROOT_PATH "/onyx/models/triangle.obj", ONYX_ROOT_PATH "/onyx/models/square.obj",
        ONYX_ROOT_PATH "/onyx/models/cube.obj", ONYX_ROOT_PATH "/onyx/models/sphere.obj"};

    createBuffers<2>(paths2D);
    createBuffers<3>(paths3D);
}

void DestroyCombinedPrimitiveBuffers() noexcept
{
    s_Buffers2D.Destroy();
    s_Buffers3D.Destroy();
}

template <u32 N, usize Sides>
    requires(Sides > 2)
static IndexVertexData<N> createRegularPolygonBuffers() noexcept
{
    if constexpr (Sides == 3)
        return loadTriangleModel<N>();
    else if constexpr (Sides == 4)
        return loadSquareModel<N>();
    else
    {
        std::array<Vertex<N>, Sides> vertices;
        std::array<Index, Sides * 3> indices;

        const f32 angle = 2.f * glm::pi<f32>() / (Sides - 1);
        for (Index i = 0; i < Sides; ++i)
        {
            const f32 x = glm::cos(angle * i);
            const f32 y = glm::sin(angle * i);
            indices[i * 3] = 0;
            indices[i * 3 + 1] = i;
            indices[i * 3 + 2] = i + 1;

            if constexpr (N == 2)
                vertices[i] = Vertex<N>{vec<N>{x, y}};
            else
                vertices[i] = Vertex<N>{vec<N>{x, y, 0.f}, vec<N>{0.f, 0.f, 1.f}};
        }

        const std::span<const Vertex<N>> verticesSpan{vertices};
        const std::span<const Index> indicesSpan{indices};
        return new Model(verticesSpan, indicesSpan);
    }
}

template <u32 N, usize MaxSides> static void createAllRegularPolygonBuffers() noexcept
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

template struct Primitives<2>;
template struct Primitives<3>;

} // namespace ONYX