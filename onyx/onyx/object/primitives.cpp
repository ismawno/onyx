#include "onyx/core/pch.hpp"
#include "onyx/object/primitives.hpp"
#include "onyx/core/core.hpp"
#include "tkit/container/storage.hpp"

namespace Onyx::Detail
{
template <Dimension D> using BufferLayout = TKit::Array<PrimitiveDataLayout, Primitives<D>::Count>;
template <Dimension D> struct IndexVertexBuffers
{
    IndexVertexBuffers(const HostVertexBuffer<D> &p_Vertices, const HostIndexBuffer &p_Indices,
                       const BufferLayout<D> &p_Layout)
        : Layout{p_Layout}
    {
        Vertices =
            CreateBuffer<Vertex<D>>(VKit::Buffer::Flag_VertexBuffer | VKit::Buffer::Flag_DeviceLocal, p_Vertices);
        Indices = CreateBuffer<Index>(VKit::Buffer::Flag_IndexBuffer | VKit::Buffer::Flag_DeviceLocal, p_Indices);
    }
    ~IndexVertexBuffers()
    {
        Vertices.Destroy();
        Indices.Destroy();
    }

    VKit::Buffer Vertices;
    VKit::Buffer Indices;
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

template <Dimension D> const VKit::Buffer &IPrimitives<D>::GetVertexBuffer()
{
    return getBuffers<D>()->Vertices;
}
template <Dimension D> const VKit::Buffer &IPrimitives<D>::GetIndexBuffer()
{
    return getBuffers<D>()->Indices;
}
template <Dimension D> const PrimitiveDataLayout &IPrimitives<D>::GetDataLayout(const u32 p_PrimitiveIndex)
{
    return getBuffers<D>()->Layout[p_PrimitiveIndex];
}

#ifdef TKIT_ENABLE_ASSERTS
template <Dimension D> void validateData(const IndexVertexHostData<D> &p_Data, const u32 p_Offset = 0)
{
    Index mx = 0;
    for (const Index i : p_Data.Indices)
        if (i > mx)
            mx = i;

    mx -= p_Offset;
    TKIT_ASSERT(
        mx < p_Data.Vertices.GetSize(),
        "[ONYX] Index and vertex host data creation is invalid. An index exceeds vertex bounds. Index: {}, size: {}",
        mx, p_Data.Vertices.GetSize());
}
#    define VALIDATE(...) validateData(__VA_ARGS__)
#else
#    define VALIDATE(...)
#endif

template <Dimension D> IndexVertexHostData<D> createTriangle()
{
    IndexVertexHostData<D> data{};
    const auto addVertex = [&data](const f32 p_X, const f32 p_Y) {
        if constexpr (D == D2)
            data.Vertices.Append(Vertex<D2>{f32v2{p_X, p_Y}});
        else
            data.Vertices.Append(Vertex<D3>{f32v3{p_X, p_Y, 0.f}, f32v3{0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const Index p_Index) { data.Indices.Append(p_Index); };

    addVertex(0.f, 0.5f);
    addVertex(-0.433013f, -0.25f);
    addVertex(0.433013f, -0.25f);

    addIndex(0);
    addIndex(1);
    addIndex(2);
    VALIDATE(data);
    return data;
}

template <Dimension D> static IndexVertexHostData<D> createSquare()
{
    IndexVertexHostData<D> data{};
    const auto addVertex = [&data](const f32 p_X, const f32 p_Y) {
        if constexpr (D == D2)
            data.Vertices.Append(Vertex<D2>{f32v2{p_X, p_Y}});
        else
            data.Vertices.Append(Vertex<D3>{f32v3{p_X, p_Y, 0.f}, f32v3{0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const Index p_Index) { data.Indices.Append(p_Index); };

    addVertex(-0.5f, -0.5f);
    addVertex(0.5f, -0.5f);
    addVertex(-0.5f, 0.5f);
    addVertex(0.5f, 0.5f);

    addIndex(0);
    addIndex(1);
    addIndex(2);
    addIndex(1);
    addIndex(3);
    addIndex(2);

    VALIDATE(data);
    return data;
}

IndexVertexHostData<D3> createCube()
{
    IndexVertexHostData<D3> data{};
    const auto addVertex = [&data](const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_N0, const f32 p_N1,
                                   const f32 p_N2) {
        data.Vertices.Append(Vertex<D3>{f32v3{p_X, p_Y, p_Z}, f32v3{p_N0, p_N1, p_N2}});
    };

    const auto addQuad = [&data](const Index p_A, const Index p_B, const Index p_C, const Index p_D) {
        data.Indices.Append(p_A);
        data.Indices.Append(p_B);
        data.Indices.Append(p_C);

        data.Indices.Append(p_A);
        data.Indices.Append(p_C);
        data.Indices.Append(p_D);
    };

    Index base = 0;

    addVertex(-0.5f, 0.5f, -0.5f, -1.f, 0.f, 0.f);
    addVertex(-0.5f, -0.5f, -0.5f, -1.f, 0.f, 0.f);
    addVertex(-0.5f, -0.5f, 0.5f, -1.f, 0.f, 0.f);
    addVertex(-0.5f, 0.5f, 0.5f, -1.f, 0.f, 0.f);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;

    addVertex(-0.5f, 0.5f, 0.5f, 0.f, 0.f, 1.f);
    addVertex(-0.5f, -0.5f, 0.5f, 0.f, 0.f, 1.f);
    addVertex(0.5f, -0.5f, 0.5f, 0.f, 0.f, 1.f);
    addVertex(0.5f, 0.5f, 0.5f, 0.f, 0.f, 1.f);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;

    addVertex(0.5f, 0.5f, 0.5f, 1.f, 0.f, 0.f);
    addVertex(0.5f, -0.5f, 0.5f, 1.f, 0.f, 0.f);
    addVertex(0.5f, -0.5f, -0.5f, 1.f, 0.f, 0.f);
    addVertex(0.5f, 0.5f, -0.5f, 1.f, 0.f, 0.f);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;

    addVertex(0.5f, 0.5f, -0.5f, 0.f, 0.f, -1.f);
    addVertex(0.5f, -0.5f, -0.5f, 0.f, 0.f, -1.f);
    addVertex(-0.5f, -0.5f, -0.5f, 0.f, 0.f, -1.f);
    addVertex(-0.5f, 0.5f, -0.5f, 0.f, 0.f, -1.f);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;

    addVertex(-0.5f, 0.5f, 0.5f, 0.f, 1.f, 0.f);
    addVertex(0.5f, 0.5f, 0.5f, 0.f, 1.f, 0.f);
    addVertex(0.5f, 0.5f, -0.5f, 0.f, 1.f, 0.f);
    addVertex(-0.5f, 0.5f, -0.5f, 0.f, 1.f, 0.f);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;

    addVertex(0.5f, -0.5f, 0.5f, 0.f, -1.f, 0.f);
    addVertex(-0.5f, -0.5f, 0.5f, 0.f, -1.f, 0.f);
    addVertex(-0.5f, -0.5f, -0.5f, 0.f, -1.f, 0.f);
    addVertex(0.5f, -0.5f, -0.5f, 0.f, -1.f, 0.f);
    addQuad(base + 0, base + 1, base + 2, base + 3);

    VALIDATE(data);
    return data;
}

template <Dimension D, bool Inverted = false, bool Counter = true>
static IndexVertexHostData<D> createRegularPolygon(const Index p_Sides, const f32v<D> &p_VertexOffset = f32v<D>{0.f},
                                                   const Index p_IndexOffset = 0,
                                                   const f32v3 &p_Normal = f32v3{0.f, 0.f, 1.f})
{
    IndexVertexHostData<D> data{};
    const auto addVertex = [&](const f32v<D> &p_Vertex) {
        if constexpr (D == D2)
            data.Vertices.Append(Vertex<D2>{p_Vertex + p_VertexOffset});
        else
            data.Vertices.Append(Vertex<D3>{p_Vertex + p_VertexOffset, p_Normal});
    };
    const auto addIndex = [&data, p_IndexOffset](const Index p_Index) { data.Indices.Append(p_Index + p_IndexOffset); };

    addVertex(f32v<D>{0.f});
    const f32 angle = 2.f * Math::Pi<f32>() / p_Sides;
    for (Index i = 0; i < p_Sides; ++i)
    {
        const f32 c = 0.5f * Math::Cosine(i * angle);
        const f32 s = 0.5f * Math::Sine(i * angle);
        if constexpr (D == D2)
            addVertex(f32v2{c, s});
        else if constexpr (Inverted)
            addVertex(f32v3{0.f, c, s});
        else
            addVertex(f32v3{c, s, 0.f});
        if constexpr (Counter)
        {
            addIndex(0);
            addIndex(i + 1);

            const Index idx = i + 2;
            addIndex(idx > p_Sides ? 1 : idx);
        }
        else
        {
            addIndex(0);
            const Index idx = i + 2;
            addIndex(idx > p_Sides ? 1 : idx);
            addIndex(i + 1);
        }
    }
    VALIDATE(data, p_IndexOffset);
    return data;
}

static IndexVertexHostData<D3> createSphere(const Index p_Rings, const Index p_Sectors)
{
    const Index rings = p_Rings + 2;
    IndexVertexHostData<D3> data{};
    const auto addVertex = [&data](const f32 p_X, const f32 p_Y, const f32 p_Z) {
        const f32v3 vertex = f32v3{p_X, p_Y, p_Z};
        data.Vertices.Append(Vertex<D3>{vertex, Math::Normalize(vertex)});
    };
    const auto addIndex = [&data, p_Sectors, rings](const Index p_Ring, const Index p_Sector) {
        Index idx;
        if (p_Ring == 0)
            idx = 0;
        else if (p_Ring == rings - 1)
            idx = 1 + (rings - 2) * p_Sectors;
        else
            idx = 1 + p_Sector + (p_Ring - 1) * p_Sectors;
        data.Indices.Append(idx);
    };

    addVertex(0.f, 0.5f, 0.f);
    for (Index i = 1; i < rings - 1; ++i)
    {
        const f32 v = static_cast<f32>(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        for (Index j = 0; j < p_Sectors; ++j)
        {
            const f32 u = static_cast<f32>(j) / p_Sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            addVertex(0.5f * ps * tc, 0.5f * pc, 0.5f * ps * ts);

            const Index ii = i - 1;
            const Index jj = (j + 1) % p_Sectors;
            addIndex(i, jj);
            addIndex(i, j);
            addIndex(ii, j);
            if (i != 1 && i != rings - 1)
            {
                addIndex(i, jj);
                addIndex(ii, j);
                addIndex(ii, jj);
            }
        }
    }
    addVertex(0.f, -0.5f, 0.f);
    for (Index j = 0; j < p_Sectors; ++j)
    {
        addIndex(rings - 2, j);
        addIndex(rings - 2, (j + 1) % p_Sectors);
        addIndex(rings - 1, j);
    }

    VALIDATE(data);
    return data;
}

static IndexVertexHostData<D3> createCylinder(const u32 p_Sides)
{
    const IndexVertexHostData<D3> left =
        createRegularPolygon<D3, true, false>(p_Sides, f32v3{-0.5f, 0.f, 0.f}, 0, f32v3{-1.f, 0.f, 0.f});

    const IndexVertexHostData<D3> right = createRegularPolygon<D3, true, true>(
        p_Sides, f32v3{0.5f, 0.f, 0.f}, left.Vertices.GetSize(), f32v3{1.f, 0.f, 0.f});

    IndexVertexHostData<D3> data{};
    data.Indices.Insert(data.Indices.end(), left.Indices.begin(), left.Indices.end());
    data.Indices.Insert(data.Indices.end(), right.Indices.begin(), right.Indices.end());

    data.Vertices.Insert(data.Vertices.end(), left.Vertices.begin(), left.Vertices.end());
    data.Vertices.Insert(data.Vertices.end(), right.Vertices.begin(), right.Vertices.end());

    const auto addVertex = [&data](const f32 p_X, const f32 p_Y, const f32 p_Z) {
        const f32v3 vertex = f32v3{p_X, p_Y, p_Z};
        data.Vertices.Append(Vertex<D3>{vertex, Math::Normalize(vertex)});
    };

    const u32 offset = left.Vertices.GetSize() + right.Vertices.GetSize();
    const auto addIndex = [&data, offset](const Index p_Index) { data.Indices.Append(p_Index + offset); };

    const f32 angle = 2.f * Math::Pi<f32>() / p_Sides;
    for (u32 i = 0; i < p_Sides; ++i)
    {
        const f32 c = 0.5f * Math::Cosine(i * angle);
        const f32 s = 0.5f * Math::Sine(i * angle);

        addVertex(-0.5f, c, s);
        addVertex(0.5f, c, s);
        const u32 ii = 2 * i;
        addIndex(ii);
        addIndex((ii + 2) % (2 * p_Sides));
        addIndex(ii + 1);

        addIndex(ii + 1);
        addIndex((ii + 2) % (2 * p_Sides));
        addIndex((ii + 3) % (2 * p_Sides));
    }

    VALIDATE(data);
    return data;
}

template <Dimension D> static void createCombinedBuffers()
{
    TKit::Array<IndexVertexHostData<D>, Primitives<D>::Count> data{};
    data[0] = createTriangle<D>();
    data[1] = createSquare<D>();

    if constexpr (D == D3)
    {
        data[2] = createCube();
        u32 resolution = 8;
        for (u32 i = 0; i < 5; ++i)
        {
            data[i + 3] = createSphere(resolution, 2 * resolution);
            data[i + 8] = createCylinder(resolution);
            resolution *= 2;
        }
    }
    const u32 offset = D == D2 ? 2 : 13;
    for (u32 i = 0; i < ONYX_REGULAR_POLYGON_COUNT; ++i)
        data[i + offset] = createRegularPolygon<D>(i + 3);

    BufferLayout<D> layout{};
    IndexVertexHostData<D> combinedData{};

    for (u32 i = 0; i < Primitives<D>::Count; ++i)
    {
        const IndexVertexHostData<D> &buffers = data[i];
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

    createCombinedBuffers<D2>();
    createCombinedBuffers<D3>();

    Core::GetDeletionQueue().Push([] {
        s_Buffers2D.Destruct();
        s_Buffers3D.Destruct();
    });
}

template struct ONYX_API IPrimitives<D2>;
template struct ONYX_API IPrimitives<D3>;

} // namespace Onyx::Detail
