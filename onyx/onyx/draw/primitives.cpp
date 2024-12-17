#include "onyx/core/pch.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/core/core.hpp"
#include "tkit/container/storage.hpp"

namespace Onyx
{
template <Dimension D> using BufferLayout = std::array<PrimitiveDataLayout, Primitives<D>::AMOUNT>;
template <Dimension D> struct IndexVertexBuffers
{
    IndexVertexBuffers(const std::span<const Vertex<D>> p_Vertices, const std::span<const Index> p_Indices,
                       const BufferLayout<D> &p_Layout) noexcept
        : Layout{p_Layout}
    {
        Vertices = Core::CreateVertexBuffer(p_Vertices);
        Indices = Core::CreateIndexBuffer(p_Indices);
    }
    ~IndexVertexBuffers() noexcept
    {
        Vertices.Destroy();
        Indices.Destroy();
    }

    VertexBuffer<D> Vertices;
    IndexBuffer Indices;
    BufferLayout<D> Layout;
};

static TKit::Storage<IndexVertexBuffers<D2>> s_Buffers2D;
static TKit::Storage<IndexVertexBuffers<D3>> s_Buffers3D;

template <Dimension D> static TKit::Storage<IndexVertexBuffers<D>> &getBuffers() noexcept
{
    if constexpr (D == D2)
        return s_Buffers2D;
    else
        return s_Buffers3D;
}

template <Dimension D> const VertexBuffer<D> &IPrimitives<D>::GetVertexBuffer() noexcept
{
    return getBuffers<D>()->Vertices;
}
template <Dimension D> const IndexBuffer &IPrimitives<D>::GetIndexBuffer() noexcept
{
    return getBuffers<D>()->Indices;
}
template <Dimension D> const PrimitiveDataLayout &IPrimitives<D>::GetDataLayout(const usize p_PrimitiveIndex) noexcept
{
    return getBuffers<D>()->Layout[p_PrimitiveIndex];
}

template <Dimension D> static IndexVertexData<D> createRegularPolygonBuffers(const usize p_Sides) noexcept
{
    IndexVertexData<D> data{};
    data.Vertices.reserve(p_Sides);
    data.Indices.reserve(p_Sides * 3 - 6);

    const f32 angle = 2.f * glm::pi<f32>() / p_Sides;
    for (Index i = 0; i < 3; ++i)
    {
        const f32 x = 0.5f * glm::cos(angle * i);
        const f32 y = 0.5f * glm::sin(angle * i);

        data.Indices.push_back(i);
        if constexpr (D == D2)
            data.Vertices.push_back(Vertex<D>{vec<D>{x, y}});
        else
            data.Vertices.push_back(Vertex<D>{vec<D>{x, y, 0.f}, vec<D>{0.f, 0.f, 1.f}});
    }

    for (Index i = 3; i < p_Sides; ++i)
    {
        const f32 x = 0.5f * glm::cos(angle * i);
        const f32 y = 0.5f * glm::sin(angle * i);
        data.Indices.push_back(0);
        data.Indices.push_back(i - 1);
        data.Indices.push_back(i);

        if constexpr (D == D2)
            data.Vertices.push_back(Vertex<D>{vec<D>{x, y}});
        else
            data.Vertices.push_back(Vertex<D>{vec<D>{x, y, 0.f}, vec<D>{0.f, 0.f, 1.f}});
    }

    return data;
}

template <Dimension D> static IndexVertexData<D> load(const std::string_view p_Path) noexcept
{
    const auto result = Load<D>(p_Path);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> static void createBuffers(const std::span<const char *const> p_Paths) noexcept
{
    BufferLayout<D> layout;
    IndexVertexData<D> data{};

    static constexpr usize toLoad = Primitives<D>::AMOUNT - ONYX_REGULAR_POLYGON_COUNT;
    for (usize i = 0; i < Primitives<D>::AMOUNT; ++i)
    {
        const IndexVertexData<D> buffers =
            i < toLoad ? load<D>(p_Paths[i]) : createRegularPolygonBuffers<D>(i - toLoad + 3);

        layout[i].VerticesStart = static_cast<u32>(data.Vertices.size());
        layout[i].IndicesStart = static_cast<u32>(data.Indices.size());
        layout[i].IndicesSize = static_cast<u32>(buffers.Indices.size());

        data.Vertices.insert(data.Vertices.end(), buffers.Vertices.begin(), buffers.Vertices.end());
        data.Indices.insert(data.Indices.end(), buffers.Indices.begin(), buffers.Indices.end());
    }

    auto &buffers = getBuffers<D>();
    const std::span<const Vertex<D>> vertices{data.Vertices};
    const std::span<const Index> indices{data.Indices};

    buffers.Create(vertices, indices, layout);
}

void CreateCombinedPrimitiveBuffers() noexcept
{
    const std::array<const char *, Primitives<D2>::AMOUNT> paths2D = {ONYX_ROOT_PATH "/onyx/models/triangle.obj",
                                                                      ONYX_ROOT_PATH "/onyx/models/square.obj"};

    const std::array<const char *, Primitives<D3>::AMOUNT> paths3D = {
        ONYX_ROOT_PATH "/onyx/models/triangle.obj", ONYX_ROOT_PATH "/onyx/models/square.obj",
        ONYX_ROOT_PATH "/onyx/models/cube.obj", ONYX_ROOT_PATH "/onyx/models/sphere.obj",
        ONYX_ROOT_PATH "/onyx/models/cylinder.obj"};

    createBuffers<D2>(paths2D);
    createBuffers<D3>(paths3D);
}

void DestroyCombinedPrimitiveBuffers() noexcept
{
    s_Buffers2D.Destroy();
    s_Buffers3D.Destroy();
}

template struct IPrimitives<D2>;
template struct IPrimitives<D3>;

} // namespace Onyx