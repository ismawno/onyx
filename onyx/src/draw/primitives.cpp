#include "core/pch.hpp"
#include "onyx/draw/primitives.hpp"
#include "kit/container/storage.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE using BufferLayout = std::array<PrimitiveDataLayout, Primitives<N>::AMOUNT>;
ONYX_DIMENSION_TEMPLATE struct IndexVertexBuffers
{
    IndexVertexBuffers(const std::span<const Vertex<N>> p_Vertices, const std::span<const Index> p_Indices,
                       const BufferLayout<N> &p_Layout) noexcept
        : Vertices{p_Vertices}, Indices{p_Indices}, Layout{p_Layout}
    {
    }

    VertexBuffer<N> Vertices;
    IndexBuffer Indices;
    BufferLayout<N> Layout;
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

ONYX_DIMENSION_TEMPLATE static IndexVertexData<N> createRegularPolygonBuffers(const usize p_Sides) noexcept
{
    IndexVertexData<N> data{};
    data.Vertices.reserve(p_Sides);
    data.Indices.reserve(p_Sides * 3 - 6);

    const f32 angle = 2.f * glm::pi<f32>() / p_Sides;
    for (Index i = 0; i < 3; ++i)
    {
        const f32 x = glm::cos(angle * i);
        const f32 y = glm::sin(angle * i);

        data.Indices.push_back(i);
        if constexpr (N == 2)
            data.Vertices.push_back(Vertex<N>{vec<N>{x, y}});
        else
            data.Vertices.push_back(Vertex<N>{vec<N>{x, y, 0.f}, vec<N>{0.f, 0.f, 1.f}});
    }

    for (Index i = 3; i < p_Sides; ++i)
    {
        const f32 x = glm::cos(angle * i);
        const f32 y = glm::sin(angle * i);
        data.Indices.push_back(0);
        data.Indices.push_back(i - 1);
        data.Indices.push_back(i);

        if constexpr (N == 2)
            data.Vertices.push_back(Vertex<N>{vec<N>{x, y}});
        else
            data.Vertices.push_back(Vertex<N>{vec<N>{x, y, 0.f}, vec<N>{0.f, 0.f, 1.f}});
    }

    return data;
}

ONYX_DIMENSION_TEMPLATE static void createBuffers(const std::span<const char *const> p_Paths) noexcept
{
    BufferLayout<N> layout;
    IndexVertexData<N> data{};

    static constexpr usize toLoad = Primitives<N>::AMOUNT - ONYX_REGULAR_POLYGON_COUNT;
    for (usize i = 0; i < Primitives<N>::AMOUNT; ++i)
    {
        const IndexVertexData<N> buffers =
            i < toLoad ? Load<N>(p_Paths[i]) : createRegularPolygonBuffers<N>(i - toLoad + 3);

        layout[i].VerticesStart = static_cast<u32>(data.Vertices.size());
        layout[i].IndicesStart = static_cast<u32>(data.Indices.size());
        layout[i].IndicesSize = static_cast<u32>(buffers.Indices.size());

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
        ONYX_ROOT_PATH "/onyx/models/cube.obj", ONYX_ROOT_PATH "/onyx/models/sphere.obj",
        ONYX_ROOT_PATH "/onyx/models/cylinder.obj"};

    createBuffers<2>(paths2D);
    createBuffers<3>(paths3D);
}

void DestroyCombinedPrimitiveBuffers() noexcept
{
    s_Buffers2D.Destroy();
    s_Buffers3D.Destroy();
}

template struct IPrimitives<2>;
template struct IPrimitives<3>;

} // namespace ONYX