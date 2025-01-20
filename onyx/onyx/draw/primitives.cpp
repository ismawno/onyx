#include "onyx/core/pch.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/core/core.hpp"
#include "tkit/container/storage.hpp"

namespace Onyx
{
template <Dimension D> using BufferLayout = TKit::Array<PrimitiveDataLayout, Primitives<D>::AMOUNT>;
template <Dimension D> struct IndexVertexBuffers
{
    IndexVertexBuffers(const TKit::Span<const Vertex<D>> p_Vertices, const TKit::Span<const Index> p_Indices,
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
template <Dimension D> const PrimitiveDataLayout &IPrimitives<D>::GetDataLayout(const u32 p_PrimitiveIndex) noexcept
{
    return getBuffers<D>()->Layout[p_PrimitiveIndex];
}

template <Dimension D> static IndexVertexData<D> createRegularPolygonBuffers(const u32 p_Sides) noexcept
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
            data.Vertices.push_back(Vertex<D>{fvec<D>{x, y}});
        else
            data.Vertices.push_back(Vertex<D>{fvec<D>{x, y, 0.f}, fvec<D>{0.f, 0.f, 1.f}});
    }

    for (Index i = 3; i < p_Sides; ++i)
    {
        const f32 x = 0.5f * glm::cos(angle * i);
        const f32 y = 0.5f * glm::sin(angle * i);
        data.Indices.push_back(0);
        data.Indices.push_back(i - 1);
        data.Indices.push_back(i);

        if constexpr (D == D2)
            data.Vertices.push_back(Vertex<D>{fvec<D>{x, y}});
        else
            data.Vertices.push_back(Vertex<D>{fvec<D>{x, y, 0.f}, fvec<D>{0.f, 0.f, 1.f}});
    }

    return data;
}

template <Dimension D> static IndexVertexData<D> load(const std::string_view p_Path) noexcept
{
    const auto result = Load<D>(p_Path);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> static void createBuffers(const TKit::Span<const char *const> p_Paths) noexcept
{
    BufferLayout<D> layout;
    IndexVertexData<D> data{};

    static constexpr u32 toLoad = Primitives<D>::AMOUNT - ONYX_REGULAR_POLYGON_COUNT;
    for (u32 i = 0; i < Primitives<D>::AMOUNT; ++i)
    {
        const IndexVertexData<D> buffers =
            i < toLoad ? load<D>(p_Paths[i]) : createRegularPolygonBuffers<D>(i - toLoad + 3);

        layout[i].VerticesStart = data.Vertices.size();
        layout[i].IndicesStart = data.Indices.size();
        layout[i].IndicesSize = buffers.Indices.size();

        data.Vertices.insert(data.Vertices.end(), buffers.Vertices.begin(), buffers.Vertices.end());
        data.Indices.insert(data.Indices.end(), buffers.Indices.begin(), buffers.Indices.end());
    }

    auto &buffers = getBuffers<D>();
    const TKit::Span<const Vertex<D>> vertices{data.Vertices};
    const TKit::Span<const Index> indices{data.Indices};

    buffers.Construct(vertices, indices, layout);
}

void CreateCombinedPrimitiveBuffers() noexcept
{
    const TKit::Array<const char *, Primitives<D2>::AMOUNT> paths2D = {ONYX_ROOT_PATH "/onyx/models/triangle.obj",
                                                                       ONYX_ROOT_PATH "/onyx/models/square.obj"};

    const TKit::Array<const char *, Primitives<D3>::AMOUNT> paths3D = {
        ONYX_ROOT_PATH "/onyx/models/triangle.obj", ONYX_ROOT_PATH "/onyx/models/square.obj",
        ONYX_ROOT_PATH "/onyx/models/cube.obj", ONYX_ROOT_PATH "/onyx/models/sphere.obj",
        ONYX_ROOT_PATH "/onyx/models/cylinder.obj"};

    createBuffers<D2>(paths2D);
    createBuffers<D3>(paths3D);

    TKIT_LOG_INFO("[ONYX] Created primitive vertex and index buffers");

    Core::GetDeletionQueue().Push([] {
        s_Buffers2D.Destruct();
        s_Buffers3D.Destruct();
    });
}

template struct IPrimitives<D2>;
template struct IPrimitives<D3>;

} // namespace Onyx