#include "core/pch.hpp"
#include "onyx/draw/primitives.hpp"
#include "kit/container/storage.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE using BufferLayout = std::array<PrimitiveDataLayout, Primitives<N>::AMOUNT>;
ONYX_DIMENSION_TEMPLATE struct IndexVertexBuffers
{
    IndexVertexBuffers(const std::span<const Vertex<N>> p_Vertices, const std::span<const Index> p_Indices,
                       const BufferLayout &p_Layout) noexcept
        : Vertices{p_Vertices}, Indices{p_Indices}, Layout{p_Layout}
    {
    }

    VertexBuffer<N> Vertices;
    IndexBuffer Indices;
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

ONYX_DIMENSION_TEMPLATE const VertexBuffer<N> *IPrimitives<N>::GetVertexBuffer() noexcept
{
    return &getBuffers<N>()->Vertices;
}
ONYX_DIMENSION_TEMPLATE const IndexBuffer *IPrimitives<N>::GetIndexBuffer() noexcept
{
    return &getBuffers<N>()->Indices;
}
ONYX_DIMENSION_TEMPLATE const PrimitiveDataLayout &IPrimitives<N>::GetDataLayout(const usize p_PrimitiveIndex) noexcept
{
    return getBuffers<N>()->Layout[p_PrimitiveIndex];
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
    const std::span<const Vertex<N>> vertices{data.Vertices};
    const std::span<const Index> indices{data.Indices};

    buffers.Create(vertices, indices, layout);
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