#include "onyx/core/pch.hpp"
#include "onyx/object/primitives.hpp"
#include "onyx/core/core.hpp"
#include "tkit/container/storage.hpp"

namespace Onyx::Detail
{
template <Dimension D> using BufferLayout = TKit::Array<PrimitiveDataLayout, Primitives<D>::AMOUNT>;
template <Dimension D> struct IndexVertexBuffers
{
    IndexVertexBuffers(const HostVertexBuffer<D> &p_Vertices, const HostIndexBuffer &p_Indices,
                       const BufferLayout<D> &p_Layout)
        : Layout{p_Layout}
    {
        Vertices = CreateDeviceLocalVertexBuffer(p_Vertices);
        Indices = CreateDeviceLocalIndexBuffer(p_Indices);
    }
    ~IndexVertexBuffers()
    {
        Vertices.Destroy();
        Indices.Destroy();
    }

    DeviceLocalVertexBuffer<D> Vertices;
    DeviceLocalIndexBuffer Indices;
    BufferLayout<D> Layout;
};

static TKit::Storage<IndexVertexBuffers<D2>> s_Buffers2D;
static TKit::Storage<IndexVertexBuffers<D3>> s_Buffers3D;

template <Dimension D> static TKit::Storage<IndexVertexBuffers<D>> &getBuffers()
{
    if constexpr (D == D2)
        return s_Buffers2D;
    else
        return s_Buffers3D;
}

template <Dimension D> const DeviceLocalVertexBuffer<D> &IPrimitives<D>::GetVertexBuffer()
{
    return getBuffers<D>()->Vertices;
}
template <Dimension D> const DeviceLocalIndexBuffer &IPrimitives<D>::GetIndexBuffer()
{
    return getBuffers<D>()->Indices;
}
template <Dimension D> const PrimitiveDataLayout &IPrimitives<D>::GetDataLayout(const u32 p_PrimitiveIndex)
{
    return getBuffers<D>()->Layout[p_PrimitiveIndex];
}

template <Dimension D> static IndexVertexHostData<D> createRegularPolygonBuffers(const u32 p_Sides)
{
    IndexVertexHostData<D> data{};
    data.Vertices.Resize(p_Sides);
    data.Indices.Resize(p_Sides * 3 - 6);

    u32 vindex = 0;
    u32 iindex = 0;
    const f32 angle = 2.f * glm::pi<f32>() / p_Sides;
    for (Index i = 0; i < 3; ++i)
    {
        const f32 x = 0.5f * glm::cos(angle * i);
        const f32 y = 0.5f * glm::sin(angle * i);

        data.Indices[iindex++] = i;
        if constexpr (D == D2)
            data.Vertices[vindex++] = Vertex<D2>{fvec2{x, y}};
        else
            data.Vertices[vindex++] = Vertex<D3>{fvec3{x, y, 0.f}, fvec3{0.f, 0.f, 1.f}};
    }

    for (Index i = 3; i < p_Sides; ++i)
    {
        const f32 x = 0.5f * glm::cos(angle * i);
        const f32 y = 0.5f * glm::sin(angle * i);
        data.Indices[iindex++] = 0;
        data.Indices[iindex++] = i - 1;
        data.Indices[iindex++] = i;

        if constexpr (D == D2)
            data.Vertices[vindex++] = Vertex<D2>{fvec2{x, y}};
        else
            data.Vertices[vindex++] = Vertex<D3>{fvec3{x, y, 0.f}, fvec3{0.f, 0.f, 1.f}};
    }

    return data;
}

template <Dimension D> static IndexVertexHostData<D> load(const std::string_view p_Path)
{
    const auto result = Load<D>(p_Path);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> static void createCombinedBuffers(const TKit::Span<const char *const> p_Paths)
{
    BufferLayout<D> layout{};
    IndexVertexHostData<D> combinedData{};

    constexpr u32 toLoad = Primitives<D>::AMOUNT - ONYX_REGULAR_POLYGON_COUNT;
    for (u32 i = 0; i < Primitives<D>::AMOUNT; ++i)
    {
        const IndexVertexHostData<D> buffers =
            i < toLoad ? load<D>(p_Paths[i]) : createRegularPolygonBuffers<D>(i - toLoad + 3);

        auto &vertices = combinedData.Vertices;
        auto &indices = combinedData.Indices;

        layout[i].VerticesStart = vertices.GetSize();
        layout[i].IndicesStart = indices.GetSize();
        layout[i].IndicesCount = buffers.Indices.GetSize();

        vertices.Insert(vertices.end(), buffers.Vertices.begin(), buffers.Vertices.end());
        indices.Insert(indices.end(), buffers.Indices.begin(), buffers.Indices.end());
    }

    auto &buffers = getBuffers<D>();
    buffers.Construct(combinedData.Vertices, combinedData.Indices, layout);
}

void CreateCombinedPrimitiveBuffers()
{
    TKIT_LOG_INFO("[ONYX] Creating primitive vertex and index buffers");
    const TKit::Array<const char *, Primitives<D2>::AMOUNT> paths2D = {ONYX_ROOT_PATH "/onyx/meshes/triangle.obj",
                                                                       ONYX_ROOT_PATH "/onyx/meshes/square.obj"};

    const TKit::Array<const char *, Primitives<D3>::AMOUNT> paths3D = {
        ONYX_ROOT_PATH "/onyx/meshes/triangle.obj",    ONYX_ROOT_PATH "/onyx/meshes/square.obj",
        ONYX_ROOT_PATH "/onyx/meshes/cube.obj",        ONYX_ROOT_PATH "/onyx/meshes/8-sphere.obj",
        ONYX_ROOT_PATH "/onyx/meshes/16-sphere.obj",   ONYX_ROOT_PATH "/onyx/meshes/32-sphere.obj",
        ONYX_ROOT_PATH "/onyx/meshes/64-sphere.obj",   ONYX_ROOT_PATH "/onyx/meshes/128-sphere.obj",
        ONYX_ROOT_PATH "/onyx/meshes/8-cylinder.obj",  ONYX_ROOT_PATH "/onyx/meshes/16-cylinder.obj",
        ONYX_ROOT_PATH "/onyx/meshes/32-cylinder.obj", ONYX_ROOT_PATH "/onyx/meshes/64-cylinder.obj",
        ONYX_ROOT_PATH "/onyx/meshes/128-cylinder.obj"};

    createCombinedBuffers<D2>(paths2D);
    createCombinedBuffers<D3>(paths3D);

    Core::GetDeletionQueue().Push([] {
        s_Buffers2D.Destruct();
        s_Buffers3D.Destruct();
    });
}

template struct ONYX_API IPrimitives<D2>;
template struct ONYX_API IPrimitives<D3>;

} // namespace Onyx::Detail
