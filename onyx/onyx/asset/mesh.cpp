#include "onyx/core/pch.hpp"
#include "onyx/asset/mesh.hpp"
#include "tkit/container/stack_array.hpp"
#ifdef ONYX_ENABLE_OBJ_LOAD
#    include <tiny_obj_loader.h>
#endif

namespace Onyx
{
#ifdef ONYX_ENABLE_OBJ_LOAD
template <Dimension D>
Result<StatMeshData<D>> LoadStaticMeshDataFromObjFile(const char *path, const LoadObjDataFlags flags)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, nullptr, &warn, &err, path))
        return Result<StatMeshData<D>>::Error(Error_FileNotFound,
                                              TKit::Format("[ONYX][MESH] Failed to load mesh: {}", err + warn));
    TKIT_LOG_WARNING_IF(!warn.empty(), "[ONYX][MESH] {}", warn);
    if (!err.empty())
        return Result<StatMeshData<D>>::Error(Error_LoadFailed,
                                              TKit::Format("[ONYX][MESH] Failed to load mesh: {}", err));
    if (shapes.empty())
        return Result<StatMeshData<D>>::Error(Error_LoadFailed,
                                              "[ONYX][MESH] Failed to load mesh. Shapes container was empty");

    std::unordered_map<StatVertex<D>, Index> uniqueVertices;
    StatMeshData<D> data;

    const u32 vcount = u32(attrib.vertices.size());

    data.Vertices.Reserve(vcount);
    data.Indices.Reserve(vcount);
    for (const auto &shape : shapes)
        for (const auto &index : shape.mesh.indices)
        {
            StatVertex<D> vertex{};
            for (Index i = 0; i < D; ++i)
                vertex.Position[i] = attrib.vertices[3 * index.vertex_index + i];
            if constexpr (D == D3)
                for (Index i = 0; i < 3; ++i)
                    vertex.Normal[i] = attrib.normals[3 * index.normal_index + i];

            if (!uniqueVertices.contains(vertex))
            {
                uniqueVertices[vertex] = Index(uniqueVertices.size());
                data.Vertices.Append(vertex);
            }
            data.Indices.Append(uniqueVertices[vertex]);
        }
    if (flags & LoadObjDataFlag_CenterVerticesAroundOrigin)
    {
        f32v<D> center{0.f};
        for (const StatVertex<D> &vx : data.Vertices)
            center += vx.Position;
        center /= data.Vertices.GetSize();
        for (StatVertex<D> &vx : data.Vertices)
            vx.Position -= center;
    }
    return data;
}
#endif
template <typename Vertex> static void removeUnusedVertices(MeshData<Vertex> &data, const u32 offset = 0)
{
    TKit::StackArray<u32> counts{};
    counts.Resize(data.Vertices.GetSize(), 0);

    for (const Index i : data.Indices)
        counts[i - Index(offset)]++;
    for (u32 i = counts.GetSize() - 1; i < counts.GetSize(); --i)
        if (counts[i] == 0)
        {
            data.Vertices.RemoveOrdered(data.Vertices.begin() + i);
            for (Index &j : data.Indices)
                if (j > i)
                    --j;
        }
}
#ifdef TKIT_ENABLE_ASSERTS
template <typename Vertex> static void validateMeshData(MeshData<Vertex> &data, const u32 offset = 0)
{
    Index mx = 0;
    for (const Index i : data.Indices)
        if (i > mx)
            mx = i;

    mx -= Index(offset);
    TKIT_ASSERT(mx < data.Vertices.GetSize(),
                "[ONYX][MESH] Index and vertex host data creation is invalid. An index exceeds vertex bounds. Index: "
                "{}, size: {}",
                mx, data.Vertices.GetSize());
    removeUnusedVertices(data, offset);
    TKit::StackArray<u32> counts{};
    counts.Resize(data.Vertices.GetSize(), 0);

    for (const Index i : data.Indices)
        counts[i - Index(offset)]++;
    for (const u32 c : counts)
    {
        TKIT_ASSERT(c != 0, "[ONYX][MESH] Found unused vertices in a mesh");
    }
}
#    define VALIDATE_MESH_DATA(...) validateMeshData(__VA_ARGS__)
#else
#    define VALIDATE_MESH_DATA(...) removeUnusedVertices(__VA_ARGS__)
#endif
template <Dimension D> StatMeshData<D> CreateTriangleMeshData()
{
    StatMeshData<D> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 u, const f32 v) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{f32v2{x, y}, f32v2{u, v}});
        else
            data.Vertices.Append(
                StatVertex<D3>{f32v3{x, y, 0.f}, f32v2{u, v}, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };

    addVertex(0.f, 0.5f, 0.5f, 1.f);
    addVertex(-0.433013f, -0.25f, 0.f, 0.f);
    addVertex(0.433013f, -0.25f, 1.f, 0.f);

    addIndex(0);
    addIndex(1);
    addIndex(2);
    VALIDATE_MESH_DATA(data);
    return data;
}
template <Dimension D> StatMeshData<D> CreateQuadMeshData()
{
    StatMeshData<D> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 u, const f32 v) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{f32v2{x, y}, f32v2{u, v}});
        else
            data.Vertices.Append(
                StatVertex<D3>{f32v3{x, y, 0.f}, f32v2{u, v}, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };

    addVertex(-0.5f, -0.5f, 0.f, 1.f);
    addVertex(0.5f, -0.5f, 1.f, 1.f);
    addVertex(-0.5f, 0.5f, 0.f, 0.f);
    addVertex(0.5f, 0.5f, 1.f, 0.f);

    addIndex(0);
    addIndex(1);
    addIndex(2);
    addIndex(1);
    addIndex(3);
    addIndex(2);

    VALIDATE_MESH_DATA(data);
    return data;
}

template <Dimension D, bool Inverted = false, bool Counter = true>
static StatMeshData<D> createRegularPolygon(const u32 sides, const f32v<D> &vertexOffset = f32v<D>{0.f},
                                            const u32 indexOffset = 0, const f32v3 &normal = f32v3{0.f, 0.f, 1.f},
                                            const f32v4 &tangent = f32v4{1.f, 0.f, 0.f, 1.f})
{
    TKIT_ASSERT(sides >= 3, "[ONYX][MESH] A regular polygon requires at least 3 sides");
    StatMeshData<D> data{};
    const auto addVertex = [&](const f32v<D> &vertex, const f32v2 &uv) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{vertex + vertexOffset, uv});
        else
            data.Vertices.Append(StatVertex<D3>{vertex + vertexOffset, uv, normal, tangent});
    };
    const auto addIndex = [&data, indexOffset](const u32 index) { data.Indices.Append(Index(index + indexOffset)); };

    addVertex(f32v<D>{0.f}, f32v2{0.5f});
    const f32 angle = 2.f * Math::Pi<f32>() / sides;
    for (u32 i = 0; i < sides; ++i)
    {
        const f32 c = 0.5f * Math::Cosine(i * angle);
        const f32 s = 0.5f * Math::Sine(i * angle);
        const f32v2 uv{c + 0.5f, s + 0.5f};
        if constexpr (D == D2)
            addVertex(f32v2{c, s}, uv);
        else if constexpr (Inverted)
            addVertex(f32v3{c, 0.f, s}, uv);
        else
            addVertex(f32v3{c, s, 0.f}, uv);
        if constexpr (Counter)
        {
            addIndex(0);
            addIndex(i + 1);

            const u32 idx = i + 2;
            addIndex(idx > sides ? 1 : idx);
        }
        else
        {
            addIndex(0);
            const u32 idx = i + 2;
            addIndex(idx > sides ? 1 : idx);
            addIndex(i + 1);
        }
    }
    VALIDATE_MESH_DATA(data, indexOffset);
    return data;
}
template <Dimension D> StatMeshData<D> CreateRegularPolygonMeshData(const u32 sides)
{
    return createRegularPolygon<D>(sides);
}

template <Dimension D> StatMeshData<D> CreatePolygonMeshData(const TKit::Span<const f32v2> vertices)
{
    TKIT_ASSERT(vertices.GetSize() >= 3, "[ONYX][MESH] A polygon must have at least 3 vertices");
    StatMeshData<D> data{};

    const auto addVertex = [&data](const f32v2 &vertex, const f32v2 &uv) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{vertex, uv});
        else
            data.Vertices.Append(
                StatVertex<D3>{f32v3{vertex, 0.f}, uv, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };

    f32v2 minv = vertices[0], maxv = vertices[0];
    for (const f32v2 &v : vertices)
    {
        minv = Math::Min(minv, v);
        maxv = Math::Max(maxv, v);
    }
    const f32v2 range = maxv - minv;
    const auto toTex = [&](const f32v2 &v) -> f32v2 {
        return (range[0] > 0.f && range[1] > 0.f) ? (v - minv) / range : f32v2{0.5f};
    };

    addVertex(f32v<D>{0.f}, f32v2{0.5f});
    const u32 size = vertices.GetSize();
    for (u32 i = 0; i < size; ++i)
    {
        const f32v2 &vertex = vertices[i];
        addVertex(vertex, toTex(vertex));
        addIndex(0);
        addIndex(i + 1);

        const u32 idx = i + 2;
        addIndex(idx > size ? 1 : idx);
    }
    return data;
}

static StatMeshData<D3> createBoxMeshData(const u32 offset = 0, const f32 push = 0.5f)
{
    StatMeshData<D3> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 n0, const f32 n1, const f32 n2,
                                   const f32 u, const f32 v, const f32v4 &tangent) {
        data.Vertices.Append(StatVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, f32v3{n0, n1, n2}, tangent});
    };
    const auto addQuad = [&data, offset](const Index a, const Index b, const Index c, const Index d) {
        data.Indices.Append(a + offset);
        data.Indices.Append(b + offset);
        data.Indices.Append(c + offset);
        data.Indices.Append(a + offset);
        data.Indices.Append(c + offset);
        data.Indices.Append(d + offset);
    };
    const f32v4 tPosZ{0.f, 0.f, 1.f, 1.f};  // -X face: U goes along +Z
    const f32v4 tPosX{1.f, 0.f, 0.f, 1.f};  // +Z face: U goes along +X
    const f32v4 tNegZ{0.f, 0.f, -1.f, 1.f}; // +X face: U goes along -Z
    const f32v4 tNegX{-1.f, 0.f, 0.f, 1.f}; // -Z face: U goes along -X
    const f32v4 tTopX{1.f, 0.f, 0.f, 1.f};  // +Y face: U goes along +X
    const f32v4 tBotX{-1.f, 0.f, 0.f, 1.f}; // -Y face: U goes along -X

    Index base = 0;
    addVertex(-push, 0.5f, -0.5f, -1.f, 0.f, 0.f, 0.f, 0.f, tPosZ);
    addVertex(-push, -0.5f, -0.5f, -1.f, 0.f, 0.f, 0.f, 1.f, tPosZ);
    addVertex(-push, -0.5f, 0.5f, -1.f, 0.f, 0.f, 1.f, 1.f, tPosZ);
    addVertex(-push, 0.5f, 0.5f, -1.f, 0.f, 0.f, 1.f, 0.f, tPosZ);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(-0.5f, 0.5f, push, 0.f, 0.f, 1.f, 0.f, 0.f, tPosX);
    addVertex(-0.5f, -0.5f, push, 0.f, 0.f, 1.f, 0.f, 1.f, tPosX);
    addVertex(0.5f, -0.5f, push, 0.f, 0.f, 1.f, 1.f, 1.f, tPosX);
    addVertex(0.5f, 0.5f, push, 0.f, 0.f, 1.f, 1.f, 0.f, tPosX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(push, 0.5f, 0.5f, 1.f, 0.f, 0.f, 0.f, 0.f, tNegZ);
    addVertex(push, -0.5f, 0.5f, 1.f, 0.f, 0.f, 0.f, 1.f, tNegZ);
    addVertex(push, -0.5f, -0.5f, 1.f, 0.f, 0.f, 1.f, 1.f, tNegZ);
    addVertex(push, 0.5f, -0.5f, 1.f, 0.f, 0.f, 1.f, 0.f, tNegZ);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(0.5f, 0.5f, -push, 0.f, 0.f, -1.f, 0.f, 0.f, tNegX);
    addVertex(0.5f, -0.5f, -push, 0.f, 0.f, -1.f, 0.f, 1.f, tNegX);
    addVertex(-0.5f, -0.5f, -push, 0.f, 0.f, -1.f, 1.f, 1.f, tNegX);
    addVertex(-0.5f, 0.5f, -push, 0.f, 0.f, -1.f, 1.f, 0.f, tNegX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(-0.5f, push, 0.5f, 0.f, 1.f, 0.f, 0.f, 0.f, tTopX);
    addVertex(0.5f, push, 0.5f, 0.f, 1.f, 0.f, 1.f, 0.f, tTopX);
    addVertex(0.5f, push, -0.5f, 0.f, 1.f, 0.f, 1.f, 1.f, tTopX);
    addVertex(-0.5f, push, -0.5f, 0.f, 1.f, 0.f, 0.f, 1.f, tTopX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(0.5f, -push, 0.5f, 0.f, -1.f, 0.f, 0.f, 0.f, tBotX);
    addVertex(-0.5f, -push, 0.5f, 0.f, -1.f, 0.f, 1.f, 0.f, tBotX);
    addVertex(-0.5f, -push, -0.5f, 0.f, -1.f, 0.f, 1.f, 1.f, tBotX);
    addVertex(0.5f, -push, -0.5f, 0.f, -1.f, 0.f, 0.f, 1.f, tBotX);
    addQuad(base + 0, base + 1, base + 2, base + 3);

    VALIDATE_MESH_DATA(data, offset);
    return data;
}

StatMeshData<D3> CreateBoxMeshData()
{
    return createBoxMeshData();
}
StatMeshData<D3> CreateSphereMeshData(u32 rings, const u32 sectors)
{
    rings += 2;
    StatMeshData<D3> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                   const f32v4 &tangent) {
        const f32v3 vertex = f32v3{x, y, z};
        data.Vertices.Append(StatVertex<D3>{vertex, f32v2{u, v}, Math::Normalize(vertex), tangent});
    };
    const auto addIndex = [&data, sectors, rings](const u32 ring, const u32 sector) {
        u32 idx;
        if (ring == 0)
            idx = sector;
        else if (ring == rings - 1)
            idx = sectors + (rings - 2) * (sectors + 1) + sector;
        else
            idx = sectors + sector + (ring - 1) * (sectors + 1);
        data.Indices.Append(Index(idx));
    };

    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 u = (f32(j) + 0.5f) / sectors;
        addVertex(0.f, 0.5f, 0.f, u, 0.f, f32v4{1.f, 0.f, 0.f, 1.f});
    }
    for (u32 i = 1; i < rings - 1; ++i)
    {
        const f32 v = f32(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        const f32 y = 0.5f * pc;
        for (u32 j = 0; j < sectors; ++j)
        {
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            addVertex(0.5f * ps * tc, y, 0.5f * ps * ts, u, v, f32v4{-ts, 0.f, tc, 1.f});

            const u32 ii = i - 1;
            const u32 jj = j + 1;
            addIndex(i, jj);
            addIndex(i, j);
            addIndex(ii, j);
            if (i != 1)
            {
                addIndex(i, jj);
                addIndex(ii, j);
                addIndex(ii, jj);
            }
        }
        addVertex(0.5f * ps, 0.5f * pc, 0.f, 1.0f, v, f32v4{0.f, 0.f, 1.f, 1.f});
    }
    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 u = (f32(j) + 0.5f) / sectors;
        addVertex(0.f, -0.5f, 0.f, u, 1.f, f32v4{1.f, 0.f, 0.f, 1.f});
    }

    for (u32 j = 0; j < sectors; ++j)
    {
        addIndex(rings - 2, j);
        addIndex(rings - 2, j + 1);
        addIndex(rings - 1, j);
    }

    VALIDATE_MESH_DATA(data);
    return data;
}
StatMeshData<D3> CreateCylinderMeshData(const u32 sides)
{
    const StatMeshData<D3> left = createRegularPolygon<D3, true, true>(
        sides, f32v3{0.f, -0.5f, 0.f}, 0, f32v3{0.f, -1.f, 0.f}, f32v4{0.f, 0.f, 1.f, 1.f});

    const StatMeshData<D3> right = createRegularPolygon<D3, true, false>(
        sides, f32v3{0.f, 0.5f, 0.f}, left.Vertices.GetSize(), f32v3{0.f, 1.f, 0.f}, f32v4{0.f, 0.f, 1.f, 1.f});

    StatMeshData<D3> data{};
    data.Indices.Insert(data.Indices.end(), left.Indices.begin(), left.Indices.end());
    data.Indices.Insert(data.Indices.end(), right.Indices.begin(), right.Indices.end());

    data.Vertices.Insert(data.Vertices.end(), left.Vertices.begin(), left.Vertices.end());
    data.Vertices.Insert(data.Vertices.end(), right.Vertices.begin(), right.Vertices.end());

    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                   const f32v4 &tangent) {
        data.Vertices.Append(StatVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, Math::Normalize(f32v3{x, 0.f, z}), tangent});
    };
    const u32 offset = left.Vertices.GetSize() + right.Vertices.GetSize();
    const auto addIndex = [&data, offset](const u32 index) { data.Indices.Append(Index(index + offset)); };

    const f32 angle = 2.f * Math::Pi<f32>() / sides;
    for (u32 i = 0; i < sides; ++i)
    {
        const f32 cc = Math::Cosine(i * angle);
        const f32 ss = Math::Sine(i * angle);

        const f32v4 tangent = f32v4{-ss, 0.f, cc, 1.f};

        const f32 x = 0.5f * cc;
        const f32 z = 0.5f * ss;

        const f32 u = f32(i) / sides;
        addVertex(x, -0.5f, z, u, 1.f, tangent);
        addVertex(x, 0.5f, z, u, 0.f, tangent);

        const u32 ii = 2 * i;
        addIndex(ii);
        addIndex(ii + 1);
        addIndex(ii + 2);

        addIndex(ii + 1);
        addIndex(ii + 3);
        addIndex(ii + 2);
    }
    addVertex(0.5f, -0.5f, 0.f, 1.f, 1.f, f32v4{0.f, 0.f, 1.f, 1.f});
    addVertex(0.5f, 0.5f, 0.f, 1.f, 0.f, f32v4{0.f, 0.f, 1.f, 1.f});

    VALIDATE_MESH_DATA(data);
    return data;
}

template <Dimension D> ParaMeshData<D> CreateStadiumMeshData()
{
    ParaMeshData<D> data{};
    data.Shape = ParametricShape_Stadium;
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 u, const f32 v,
                                   const ParametricRegionFlags region = 0) {
        if constexpr (D == D2)
            data.Vertices.Append(ParaVertex<D2>{f32v2{x, y}, f32v2{u, v}, region});
        else
            data.Vertices.Append(
                ParaVertex<D3>{f32v3{x, y, 0.f}, f32v2{u, v}, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}, region});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };
    constexpr u32 edge = StadiumRegion_Edge;
    constexpr u32 moon = StadiumRegion_Moon;

    addVertex(-0.5f, -0.5f, 0.f, 0.75f);
    addVertex(0.5f, -0.5f, 1.f, 0.75f);
    addVertex(-0.5f, 0.5f, 0.f, 0.25f);
    addVertex(0.5f, 0.5f, 1.f, 0.25f);

    addVertex(-0.5f, -1.f, 0.f, 1.f, edge | moon);
    addVertex(0.5f, -1.f, 1.f, 1.f, edge | moon);
    addVertex(-0.5f, -0.5f, 0.f, 0.75f, moon);
    addVertex(0.5f, -0.5f, 1.f, 0.75f, moon);

    addVertex(-0.5f, 0.5f, 0.f, 0.25f, moon);
    addVertex(0.5f, 0.5f, 1.f, 0.25f, moon);
    addVertex(-0.5f, 1.f, 0.f, 0.f, edge | moon);
    addVertex(0.5f, 1.f, 1.f, 0.f, edge | moon);

    for (u32 i = 0; i < 3; ++i)
    {
        addIndex(0 + i * 4);
        addIndex(1 + i * 4);
        addIndex(2 + i * 4);
        addIndex(1 + i * 4);
        addIndex(3 + i * 4);
        addIndex(2 + i * 4);
    }
    VALIDATE_MESH_DATA(data);
    return data;
}

template <Dimension D> ParaMeshData<D> CreateRoundedQuadMeshData()
{
    ParaMeshData<D> data{};
    data.Shape = ParametricShape_RoundedQuad;
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 u, const f32 v,
                                   const ParametricRegionFlags region = 0) {
        if constexpr (D == D2)
            data.Vertices.Append(ParaVertex<D2>{f32v2{x, y}, f32v2{u, v}, region});
        else
            data.Vertices.Append(
                ParaVertex<D3>{f32v3{x, y, 0.f}, f32v2{u, v}, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}, region});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };

    constexpr u32 hedge = RoundedQuadRegion_HorizontalEdge;
    constexpr u32 vedge = RoundedQuadRegion_VerticalEdge;
    constexpr u32 moon = RoundedQuadRegion_Moon;

    addVertex(-0.5f, -0.5f, 0.25f, 0.75f);
    addVertex(0.5f, -0.5f, 0.75f, 0.75f);
    addVertex(-0.5f, 0.5f, 0.25f, 0.25f);
    addVertex(0.5f, 0.5f, 0.75f, 0.25f);

    addVertex(-1.f, -0.5f, 0.f, 0.75f, hedge);
    addVertex(-0.5f, -0.5f, 0.25f, 0.75f);
    addVertex(-1.f, 0.5f, 0.f, 0.25f, hedge);
    addVertex(-0.5f, 0.5f, 0.25f, 0.25f);

    addVertex(0.5f, -0.5f, 0.75f, 0.75f);
    addVertex(1.f, -0.5f, 1.f, 0.75f, hedge);
    addVertex(0.5f, 0.5f, 0.75f, 0.25f);
    addVertex(1.f, 0.5f, 1.f, 0.25f, hedge);

    addVertex(-0.5f, -0.5f, 0.25f, 0.75f);
    addVertex(-0.5f, -1.f, 0.25f, 1.f, vedge);
    addVertex(0.5f, -0.5f, 0.75f, 0.75f);
    addVertex(0.5f, -1.f, 0.75f, 1.f, vedge);

    addVertex(-0.5f, 1.f, 0.25f, 0.f, vedge);
    addVertex(-0.5f, 0.5f, 0.25f, 0.25f);
    addVertex(0.5f, 1.f, 0.75f, 0.f, vedge);
    addVertex(0.5f, 0.5f, 0.75f, 0.25f);

    addVertex(-1.f, -0.5f, 0.f, 0.75f, moon | hedge);
    addVertex(-1.f, -1.f, 0.f, 1.f, moon | hedge | vedge);
    addVertex(-0.5f, -0.5f, 0.25f, 0.75f, moon);
    addVertex(-0.5f, -1.f, 0.25f, 1.f, moon | vedge);

    addVertex(0.5f, -1.f, 0.75f, 1.f, moon | vedge);
    addVertex(1.f, -1.f, 1.f, 1.f, moon | hedge | vedge);
    addVertex(0.5f, -0.5f, 0.75f, 0.75f, moon);
    addVertex(1.f, -0.5f, 1.f, 0.75f, moon | hedge);

    addVertex(-0.5f, 1.f, 0.25f, 0.f, moon | vedge);
    addVertex(-1.f, 1.f, 0.f, 0.f, moon | hedge | vedge);
    addVertex(-0.5f, 0.5f, 0.25f, 0.25f, moon);
    addVertex(-1.f, 0.5f, 0.f, 0.25f, moon | hedge);

    addVertex(1.f, 0.5f, 1.f, 0.25f, moon | hedge);
    addVertex(1.f, 1.f, 1.f, 0.f, moon | hedge | vedge);
    addVertex(0.5f, 0.5f, 0.75f, 0.25f, moon);
    addVertex(0.5f, 1.f, 0.75f, 0.f, moon | vedge);

    for (u32 i = 0; i < 9; ++i)
    {
        addIndex(0 + i * 4);
        addIndex(1 + i * 4);
        addIndex(2 + i * 4);
        addIndex(1 + i * 4);
        addIndex(3 + i * 4);
        addIndex(2 + i * 4);
    }

    VALIDATE_MESH_DATA(data);
    return data;
}

ParaMeshData<D3> CreateCapsuleMeshData(u32 rings, const u32 sectors)
{
    rings += 2;
    ParaMeshData<D3> data{};
    data.Shape = ParametricShape_Capsule;

    const auto addCylinderVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                           const f32v4 &tangent) {
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, Math::Normalize(f32v3{x, 0.f, z}), tangent,
                                            CapsuleRegion_Body});
    };
    u32 coffset = 0;
    const auto addSphereVertex = [&data, &coffset](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                                   const f32v4 &tangent) {
        f32v3 normal = f32v3{x, y, z};
        if (y < 0.f)
            normal[1] += 0.5f;
        else
            normal[1] -= 0.5f;
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, Math::Normalize(normal), tangent, 0});
        ++coffset;
    };

    const auto addSphereIndex = [&data, sectors, rings](const u32 ring, const u32 sector) {
        u32 idx;
        if (ring == 0)
            idx = sector;
        else if (ring == rings)
            idx = sectors + (rings - 1) * (sectors + 1) + sector;
        else
            idx = sectors + sector + (ring - 1) * (sectors + 1);
        data.Indices.Append(Index(idx));
    };
    const auto addCylinderIndex = [&data, &coffset](const u32 index) { data.Indices.Append(Index(index + coffset)); };

    const u32 halfRings = rings / 2;

    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 u = (f32(j) + 0.5f) / sectors;
        addSphereVertex(0.f, 1.f, 0.f, u, 0.f, f32v4{1.f, 0.f, 0.f, 1.f});
    }

    for (u32 i = 1; i < halfRings + 1; ++i)
    {
        const f32 v = f32(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        const f32 y = Math::Map(pc, 0.f, 1.f, 0.5f, 1.f);
        const f32 vv = Math::Map(v, 0.f, 0.5f, 0.f, 0.25f);
        for (u32 j = 0; j < sectors; ++j)
        {
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);

            addSphereVertex(0.5f * ps * tc, y, 0.5f * ps * ts, u, vv, f32v4{-ts, 0.f, tc, 1.f});

            const u32 ii = i - 1;
            const u32 jj = j + 1;
            addSphereIndex(i, jj);
            addSphereIndex(i, j);
            addSphereIndex(ii, j);
            if (i != 1)
            {
                addSphereIndex(i, jj);
                addSphereIndex(ii, j);
                addSphereIndex(ii, jj);
            }
        }
        addSphereVertex(0.5f * ps, y, 0.f, 1.f, vv, f32v4{0.f, 0.f, 1.f, 1.f});
    }

    for (u32 i = halfRings; i < rings - 1; ++i)
    {
        const f32 v = f32(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        const f32 y = Math::Map(pc, 0.f, -1.f, -0.5f, -1.f);
        const f32 vv = Math::Map(v, 0.5f, 1.f, 0.75f, 1.f);
        for (u32 j = 0; j < sectors; ++j)
        {
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);

            addSphereVertex(0.5f * ps * tc, y, 0.5f * ps * ts, u, vv, f32v4{-ts, 0.f, tc, 1.f});

            if (i != halfRings)
            {
                const u32 ii = i + 1;
                const u32 jj = j + 1;

                addSphereIndex(ii, jj);
                addSphereIndex(ii, j);
                addSphereIndex(i, j);
                addSphereIndex(ii, jj);
                addSphereIndex(i, j);
                addSphereIndex(i, jj);
            }
        }
        addSphereVertex(0.5f * ps, y, 0.f, 1.f, vv, f32v4{0.f, 0.f, 1.f, 1.f});
    }

    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 u = (f32(j) + 0.5f) / sectors;
        addSphereVertex(0.f, -1.f, 0.f, u, 1.f, f32v4{0.f, 1.f, 0.f, 1.f});
    }
    for (u32 j = 0; j < sectors; ++j)
    {
        addSphereIndex(rings - 1, j);
        addSphereIndex(rings - 1, j + 1);
        addSphereIndex(rings, j);
    }

    const f32 angle = 2.f * Math::Pi<f32>() / sectors;
    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 cc = Math::Cosine(j * angle);
        const f32 ss = Math::Sine(j * angle);

        const f32v4 tangent = f32v4{-ss, 0.f, cc, 1.f};

        const f32 x = 0.5f * cc;
        const f32 z = 0.5f * ss;

        const f32 u = f32(j) / sectors;
        addCylinderVertex(x, -0.5f, z, u, 0.75f, tangent);
        addCylinderVertex(x, 0.5f, z, u, 0.25f, tangent);

        const u32 ii = 2 * j;
        addCylinderIndex(ii);
        addCylinderIndex(ii + 1);
        addCylinderIndex(ii + 2);

        addCylinderIndex(ii + 1);
        addCylinderIndex(ii + 3);
        addCylinderIndex(ii + 2);
    }
    addCylinderVertex(0.5f, -0.5f, 0.f, 1.f, 0.75f, f32v4{0.f, 0.f, 1.f, 1.f});
    addCylinderVertex(0.5f, 0.5f, 0.f, 1.f, 0.25f, f32v4{0.f, 0.f, 1.f, 1.f});

    VALIDATE_MESH_DATA(data);
    return data;
}

// from this point on, up to the explicit instantiations, the code is pretty much ia generated with minor modifications
// on my part. i got tired of creating shape code. seeing i had spent more than one day creating the stadium, rounded
// quad and the capsule, i got a bit fed up. in fact, i did do the rounded box geometry, but didnt take into account
// uvs. when i tried to tackle it, i would have rather jumped out of my window. so i asked claude to do it
//
// idk how i feel about it. i want onyx to be mine, and this kind of feels dishonest. i dont know why i feel this way
// for this particular piece of unimportant code. but i guess it is the first time something has been so tedious, i
// decided to have it automatically generated
static f32v2 computeRoundedBoxTexCoords(const f32v3 &pos, const f32v3 &normal) noexcept
{
    const f32 ax = Math::Absolute(normal[0]);
    const f32 ay = Math::Absolute(normal[1]);
    const f32 az = Math::Absolute(normal[2]);

    f32 lu, lv;

    if (ay >= ax && ay >= az)
    {
        // Y-dominant face
        lu = pos[0];
        lv = (normal[1] > 0.f) ? pos[2] : -pos[2];
    }
    else if (ax >= ay && ax >= az)
    {
        // X-dominant face
        lu = (normal[0] > 0.f) ? -pos[2] : pos[2];
        lv = -pos[1];
    }
    else
    {
        // Z-dominant face
        lu = (normal[2] > 0.f) ? pos[0] : -pos[0];
        lv = -pos[1];
    }

    // Position range is [-1, 1], map to [0, 1]
    return f32v2{(lu + 1.f) * 0.5f, (lv + 1.f) * 0.5f};
}

static f32v2 computeRoundedBoxTexCoords_Forced(const f32v3 &pos, const u32 axis) noexcept
{
    f32 lu, lv;
    switch (axis)
    {
    case 0:
        lu = -pos[2];
        lv = -pos[1];
        break; // +X
    case 1:
        lu = pos[2];
        lv = -pos[1];
        break; // -X
    case 2:
        lu = pos[0];
        lv = pos[2];
        break; // +Y
    case 3:
        lu = pos[0];
        lv = -pos[2];
        break; // -Y
    case 4:
        lu = pos[0];
        lv = -pos[1];
        break; // +Z
    case 5:
        lu = -pos[0];
        lv = -pos[1];
        break; // -Z
    default:
        lu = lv = 0.f;
        break;
    }
    return f32v2{(lu + 1.f) * 0.5f, (lv + 1.f) * 0.5f};
}

static u32 chooseFace_02(const f32v3 &pos, const f32 sx, const f32 sz) noexcept
{
    f32v3 n = pos;
    for (u32 i = 0; i < 3; ++i)
        if (n[i] > 0.f)
            n[i] -= 0.5f;
        else
            n[i] += 0.5f;

    const f32 ax = Math::Absolute(n[0]);
    const f32 ay = Math::Absolute(n[1]);
    const f32 az = Math::Absolute(n[2]);

    if (ay > ax && ay > az)
        return (n[1] > 0.f) ? 2u : 3u; // +Y / -Y
    if (az >= ax)
        return (sz > 0.f) ? 4u : 5u; // +Z / -Z
    return (sx > 0.f) ? 0u : 1u;     // +X / -X
}

static f32v4 getFaceTangent(const u32 p_axis) noexcept
{
    switch (p_axis)
    {
    case 0:
        return f32v4{0.f, 0.f, -1.f, 1.f}; // +X
    case 1:
        return f32v4{0.f, 0.f, 1.f, 1.f}; // -X
    case 2:
        return f32v4{1.f, 0.f, 0.f, 1.f}; // +Y
    case 3:
        return f32v4{1.f, 0.f, 0.f, 1.f}; // -Y
    case 4:
        return f32v4{1.f, 0.f, 0.f, 1.f}; // +Z
    case 5:
        return f32v4{-1.f, 0.f, 0.f, 1.f}; // -Z
    default:
        return f32v4{1.f, 0.f, 0.f, 1.f};
    }
}

ParaMeshData<D3> CreateRoundedBoxMeshData(u32 rings, const u32 sectors)
{
    rings += 2;
    ParaMeshData<D3> data{};
    data.Shape = ParametricShape_RoundedBox;

    const auto addCylinderVertex02 = [&data](const f32 x, const f32 y, const f32 z, const u32 face) {
        f32v3 normal = f32v3{x, 0.f, z};
        if (x > 0.f)
            normal[0] -= 0.5f;
        else
            normal[0] += 0.5f;
        if (z > 0.f)
            normal[2] -= 0.5f;
        else
            normal[2] += 0.5f;
        const f32v3 n = Math::Normalize(normal);
        const f32v2 uv = computeRoundedBoxTexCoords_Forced(f32v3{x, y, z}, face);
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, uv, n, getFaceTangent(face), 0});
    };

    const auto addCylinderVertex12 = [&data](const f32 x, const f32 y, const f32 z, const u32 face) {
        f32v3 normal = f32v3{0, y, z};
        if (y > 0.f)
            normal[1] -= 0.5f;
        else
            normal[1] += 0.5f;
        if (z > 0.f)
            normal[2] -= 0.5f;
        else
            normal[2] += 0.5f;
        const f32v3 n = Math::Normalize(normal);
        const f32v2 uv = computeRoundedBoxTexCoords_Forced(f32v3{x, y, z}, face);
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, uv, n, getFaceTangent(face), 0});
    };

    const auto addCylinderVertex01 = [&data](const f32 x, const f32 y, const f32 z, const u32 face) {
        f32v3 normal = f32v3{x, y, 0.f};
        if (x > 0.f)
            normal[0] -= 0.5f;
        else
            normal[0] += 0.5f;
        if (y > 0.f)
            normal[1] -= 0.5f;
        else
            normal[1] += 0.5f;
        const f32v3 n = Math::Normalize(normal);
        const f32v2 uv = computeRoundedBoxTexCoords_Forced(f32v3{x, y, z}, face);
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, uv, n, getFaceTangent(face), 0});
    };

    const auto addSphereVertex = [&data](const f32 x, const f32 y, const f32 z, const u32 face) {
        f32v3 normal = f32v3{x, y, z};
        for (u32 i = 0; i < 3; ++i)
            if (normal[i] > 0.f)
                normal[i] -= 0.5f;
            else
                normal[i] += 0.5f;
        const f32v3 n = Math::Normalize(normal);
        const f32v2 uv = computeRoundedBoxTexCoords_Forced(f32v3{x, y, z}, face);
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, uv, n, getFaceTangent(face), 0});
    };

    const u32 quartSectors = sectors / 4;
    u32 offset = 0;
    const auto addCylinderIndex = [&data, &offset](const u32 index) { data.Indices.Append(Index(index + offset)); };

    const u32 halfRings = rings / 2;

    const auto addEdge02 = [&](const f32 sx, const f32 sz) {
        const f32 mnx = 0.5f * sx;
        const f32 mxx = sx;
        const f32 mnz = 0.5f * sz;
        const f32 mxz = sz;

        const u32 yFaceTop = 2u; // +Y
        const u32 yFaceBot = 3u; // -Y
        const u32 zFace = (sz > 0.f) ? 4u : 5u;
        const u32 xFace = (sx > 0.f) ? 0u : 1u;

        const u32 halfQuart = quartSectors / 2;

        const auto computeUpperVertex = [&](const u32 i, const u32 j) -> f32v3 {
            const f32 v = f32(i) / rings;
            const f32 phi = v * Math::Pi<f32>();
            const f32 pc = Math::Cosine(phi);
            const f32 ps = Math::Sine(phi);
            const f32 tx = Math::Map(ps, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(pc, 0.f, 1.f, 0.5f, 1.f);
            const f32 tz = Math::Map(ps, 0.f, 1.f, mnz, mxz);
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();
            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            const f32 x = Math::Map(ps * tc, ps, 0.f, tx, sx * 0.5f);
            const f32 z = Math::Map(ps * ts, ps, 0.f, tz, sz * 0.5f);
            return f32v3{x, y, z};
        };

        const auto computeLowerVertex = [&](const u32 i, const u32 j) -> f32v3 {
            const f32 v = f32(i) / rings;
            const f32 phi = v * Math::Pi<f32>();
            const f32 pc = Math::Cosine(phi);
            const f32 ps = Math::Sine(phi);
            const f32 tx = Math::Map(ps, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(pc, 0.f, -1.f, -0.5f, -1.f);
            const f32 tz = Math::Map(ps, 0.f, 1.f, mnz, mxz);
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();
            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            const f32 x = Math::Map(ps * tc, ps, 0.f, tx, sx * 0.5f);
            const f32 z = Math::Map(ps * ts, ps, 0.f, tz, sz * 0.5f);
            return f32v3{x, y, z};
        };

        // Find the ring index where Y-dominance ends.
        // At ring i, the normal's Y component ~ cos(phi) - 0.5, and the
        // horizontal component ~ sin(phi). Y-dominant when cos(phi)-0.5 > sin(phi)*factor.
        // For simplicity, use halfRings/2 as the split — at the midpoint between pole
        // and equator. Or better, compute it:
        u32 ySplitRing = 1;
        for (u32 i = 1; i < halfRings + 1; ++i)
        {
            const f32v3 pos = computeUpperVertex(i, halfQuart); // sample at theta midpoint
            const u32 face = chooseFace_02(pos, sx, sz);
            if (face != yFaceTop)
            {
                ySplitRing = i;
                break;
            }
            ySplitRing = i + 1;
        }

        // Similarly for lower hemisphere
        u32 ySplitRingLower = rings - 2;
        for (u32 i = rings - 2; i >= halfRings; --i)
        {
            const f32v3 pos = computeLowerVertex(i, halfQuart);
            const u32 face = chooseFace_02(pos, sx, sz);
            if (face != yFaceBot)
            {
                ySplitRingLower = i;
                break;
            }
            ySplitRingLower = i - 1;
        }

        // ------------------------------------------------------------------
        // Emit sub-patches. Each is a self-contained vertex+index block.
        // We use a helper lambda to emit a rectangular patch of the octant.
        // ------------------------------------------------------------------

        // Emits a patch covering rings [ringMin, ringMax] and sectors [secMin, secMax]
        // with a forced face assignment. Handles pole rings (ring 0 and ring rings-1)
        // specially by emitting per-sector pole vertices.
        const auto emitPatch = [&](const u32 ringMin, const u32 ringMax, const u32 secMin, const u32 secMax,
                                   const u32 face, const bool upper) {
            const u32 patchOffset = data.Vertices.GetSize();
            const u32 secCount = secMax - secMin;

            const bool hasTopPole = (ringMin == 0);
            // Actually poles are only at ring 0 (top) and the absolute bottom

            // For indexing within this patch:
            // If hasTopPole: ring 0 has secCount pole vertices (indices 0..secCount-1)
            //   then ring 1 starts at secCount, each ring has (secCount+1) vertices
            // If !hasTopPole: ring ringMin has (secCount+1) vertices starting at 0
            //   subsequent rings follow

            // Emit vertices
            // Ring 0 (pole) if applicable
            if (hasTopPole)
                for (u32 j = 0; j < secCount; ++j)
                    addSphereVertex(mnx, 1.f, mnz, face);

            // Body rings
            const u32 bodyStart = hasTopPole ? 1 : ringMin;
            const u32 bodyEnd = ringMax; // inclusive

            for (u32 i = bodyStart; i <= bodyEnd; ++i)
                for (u32 j = secMin; j <= secMax; ++j)
                {
                    f32v3 pos;
                    if (upper || i < halfRings)
                        pos = computeUpperVertex(i, j);
                    else
                        pos = computeLowerVertex(i, j);
                    addSphereVertex(pos[0], pos[1], pos[2], face);
                }

            // Bottom pole if applicable
            const bool isAbsoluteBottom = (!upper && ringMax >= rings - 1);
            if (isAbsoluteBottom)
                for (u32 j = 0; j < secCount; ++j)
                    addSphereVertex(mnx, -1.f, mnz, face);

            // Emit indices
            const u32 ringWidth = secCount + 1;

            // Local index helper
            const auto localIdx = [&](const u32 ring, const u32 sec) -> u32 {
                // ring is in patch-local coords (0 = first ring in patch)
                // sec is in [0, secCount]
                if (hasTopPole && ring == 0)
                    return patchOffset + sec; // pole vertex for this sector
                const u32 bodyRingIdx = ring - (hasTopPole ? 1 : 0);
                const u32 base = hasTopPole ? secCount : 0;
                return patchOffset + base + bodyRingIdx * ringWidth + sec;
            };

            const u32 totalBodyRings = bodyEnd - bodyStart + 1;
            // Top pole fan
            if (hasTopPole && totalBodyRings > 0)
            {
                for (u32 j = 0; j < secCount; ++j)
                {
                    const u32 jj = j + 1;
                    if (sx * sz > 0.f)
                    {
                        data.Indices.Append(Index(localIdx(1, jj)));
                        data.Indices.Append(Index(localIdx(1, j)));
                        data.Indices.Append(Index(localIdx(0, j)));
                    }
                    else
                    {
                        data.Indices.Append(Index(localIdx(1, jj)));
                        data.Indices.Append(Index(localIdx(0, j)));
                        data.Indices.Append(Index(localIdx(1, j)));
                    }
                }
            }

            // Body quads
            const u32 firstBodyLocal = hasTopPole ? 1 : 0;
            for (u32 ri = 0; ri < totalBodyRings - 1; ++ri)
            {
                const u32 r = firstBodyLocal + ri;
                const u32 rn = r + 1;
                for (u32 j = 0; j < secCount; ++j)
                {
                    const u32 jj = j + 1;

                    // Determine winding based on hemisphere
                    const u32 actualRing = bodyStart + ri;
                    if (actualRing < halfRings) // upper hemisphere
                    {
                        if (sx * sz > 0.f)
                        {
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(rn, j)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(r, jj)));
                        }
                        else
                        {
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(rn, j)));
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                        }
                    }
                    else // lower hemisphere
                    {
                        if (sx * sz > 0.f)
                        {
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(rn, j)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(r, jj)));
                        }
                        else
                        {
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(rn, j)));
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                        }
                    }
                }
            }

            // Bottom pole fan
            if (isAbsoluteBottom)
            {
                const u32 lastBodyRing = firstBodyLocal + totalBodyRings - 1;
                const u32 poleRing = lastBodyRing + 1;
                for (u32 j = 0; j < secCount; ++j)
                {
                    if (sx * sz > 0.f)
                    {
                        data.Indices.Append(Index(localIdx(lastBodyRing, j)));
                        data.Indices.Append(Index(localIdx(lastBodyRing, j + 1)));
                        data.Indices.Append(Index(localIdx(poleRing, j)));
                    }
                    else
                    {
                        data.Indices.Append(Index(localIdx(lastBodyRing, j)));
                        data.Indices.Append(Index(localIdx(poleRing, j)));
                        data.Indices.Append(Index(localIdx(lastBodyRing, j + 1)));
                    }
                }
            }
        };

        // ------------------------------------------------------------------
        // Emit the sub-patches
        // ------------------------------------------------------------------

        // Upper hemisphere:
        //   Y-face patch:  rings [0, ySplitRing],  sectors [0, quartSectors]
        //   Z-face patch:  rings [ySplitRing, halfRings], sectors [0, halfQuart]
        //   X-face patch:  rings [ySplitRing, halfRings], sectors [halfQuart, quartSectors]

        if (ySplitRing > 1)
            emitPatch(0, ySplitRing, 0, quartSectors, yFaceTop, true);
        else
            emitPatch(0, 1, 0, quartSectors, yFaceTop, true); // at least the pole fan

        if (ySplitRing <= halfRings)
        {
            emitPatch(ySplitRing, halfRings, 0, halfQuart, zFace, true);
            emitPatch(ySplitRing, halfRings, halfQuart, quartSectors, xFace, true);
        }

        // Lower hemisphere:
        //   Z-face patch:  rings [halfRings, ySplitRingLower], sectors [0, halfQuart]
        //   X-face patch:  rings [halfRings, ySplitRingLower], sectors [halfQuart, quartSectors]
        //   Y-face patch:  rings [ySplitRingLower, rings-1],   sectors [0, quartSectors]

        if (ySplitRingLower >= halfRings)
        {
            emitPatch(halfRings, ySplitRingLower, 0, halfQuart, zFace, false);
            emitPatch(halfRings, ySplitRingLower, halfQuart, quartSectors, xFace, false);
        }

        if (ySplitRingLower < rings - 1)
            emitPatch(ySplitRingLower, rings - 1, 0, quartSectors, yFaceBot, false);

        offset = data.Vertices.GetSize();
        {
            const f32 angle = 2.f * Math::Pi<f32>() / sectors;
            const u32 halfQuart = quartSectors / 2;

            // Determine which faces this edge bridges
            // addEdge02 sweeps from the Z-aligned face toward the X-aligned face
            // At j=0 the normal points along Z, at j=quartSectors it points along X
            const u32 faceA = (sz > 0.f) ? 4u : 5u; // +Z or -Z
            const u32 faceB = (sx > 0.f) ? 0u : 1u; // +X or -X

            // First half: j = 0 .. halfQuart, assigned to faceA
            for (u32 j = 0; j <= halfQuart; ++j)
            {
                const f32 cc = Math::Cosine(j * angle);
                const f32 ss = Math::Sine(j * angle);
                const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
                const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

                addCylinderVertex02(x, -0.5f, z, faceA);
                addCylinderVertex02(x, 0.5f, z, faceA);
            }

            // Duplicate the midpoint vertex pair for faceB (seam)
            {
                const f32 cc = Math::Cosine(halfQuart * angle);
                const f32 ss = Math::Sine(halfQuart * angle);
                const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
                const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

                addCylinderVertex02(x, -0.5f, z, faceB);
                addCylinderVertex02(x, 0.5f, z, faceB);
            }

            // Second half: j = halfQuart+1 .. quartSectors, assigned to faceB
            for (u32 j = halfQuart + 1; j <= quartSectors; ++j)
            {
                const f32 cc = Math::Cosine(j * angle);
                const f32 ss = Math::Sine(j * angle);
                const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
                const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

                addCylinderVertex02(x, -0.5f, z, faceB);
                addCylinderVertex02(x, 0.5f, z, faceB);
            }

            // Indexing: total vertex pairs = (halfQuart+1) + 1 + (quartSectors - halfQuart)
            //                               = quartSectors + 2
            // The midpoint duplication inserts an extra pair at index (halfQuart+1)
            // So vertex pair indices:
            //   j=0..halfQuart  -> pair index 0..halfQuart          (faceA)
            //   duplicate       -> pair index halfQuart+1            (faceB)
            //   j=halfQuart+1.. -> pair index halfQuart+2..          (faceB)

            const u32 totalPairs = quartSectors + 2; // +1 for closing, +1 for duplicate
            for (u32 p = 0; p < totalPairs - 1; ++p)
            {
                // Skip the quad that straddles faceA's last and faceB's first
                // Actually we don't skip — the duplicate means pair halfQuart is faceA
                // and pair halfQuart+1 is faceB at the same position, forming a degenerate
                // zero-area quad. But we do want to skip indexing across the seam.
                if (p == halfQuart)
                    continue; // this pair's "next" is the duplicate, skip

                const u32 ii = 2 * p;
                if (sx * sz > 0.f)
                {
                    addCylinderIndex(ii);
                    addCylinderIndex(ii + 1);
                    addCylinderIndex(ii + 2);

                    addCylinderIndex(ii + 1);
                    addCylinderIndex(ii + 3);
                    addCylinderIndex(ii + 2);
                }
                else
                {
                    addCylinderIndex(ii);
                    addCylinderIndex(ii + 2);
                    addCylinderIndex(ii + 1);

                    addCylinderIndex(ii + 1);
                    addCylinderIndex(ii + 2);
                    addCylinderIndex(ii + 3);
                }
            }
        }
        offset = data.Vertices.GetSize();
    };

    const u32 whole = 2 * rings;
    const u32 part = halfRings;
    const auto addEdge12 = [&](const f32 sy, const f32 sz) {
        const f32 mny = 0.5f * sy;
        const f32 mxy = sy;

        const f32 mnz = 0.5f * sz;
        const f32 mxz = sz;
        const f32 angle = 2.f * Math::Pi<f32>() / whole;

        const u32 halfPart = part / 2;

        // At j=0 the normal points along Y, at j=part it points along Z
        const u32 faceA = (sy > 0.f) ? 2u : 3u; // +Y or -Y
        const u32 faceB = (sz > 0.f) ? 4u : 5u; // +Z or -Z

        // First half: j = 0 .. halfPart, assigned to faceA
        for (u32 j = 0; j <= halfPart; ++j)
        {
            const f32 cc = Math::Cosine(j * angle);
            const f32 ss = Math::Sine(j * angle);
            const f32 y = Math::Map(cc, 0.f, 1.f, mny, mxy);
            const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

            addCylinderVertex12(-0.5f, y, z, faceA);
            addCylinderVertex12(0.5f, y, z, faceA);
        }

        // Duplicate midpoint for faceB
        {
            const f32 cc = Math::Cosine(halfPart * angle);
            const f32 ss = Math::Sine(halfPart * angle);
            const f32 y = Math::Map(cc, 0.f, 1.f, mny, mxy);
            const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

            addCylinderVertex12(-0.5f, y, z, faceB);
            addCylinderVertex12(0.5f, y, z, faceB);
        }

        // Second half: j = halfPart+1 .. part, assigned to faceB
        for (u32 j = halfPart + 1; j <= part; ++j)
        {
            const f32 cc = Math::Cosine(j * angle);
            const f32 ss = Math::Sine(j * angle);
            const f32 y = Math::Map(cc, 0.f, 1.f, mny, mxy);
            const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

            addCylinderVertex12(-0.5f, y, z, faceB);
            addCylinderVertex12(0.5f, y, z, faceB);
        }

        // Indexing
        const u32 totalPairs = part + 2;
        for (u32 p = 0; p < totalPairs - 1; ++p)
        {
            if (p == halfPart)
                continue;

            const u32 ii = 2 * p;
            if (sy * sz < 0.f)
            {
                addCylinderIndex(ii);
                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 2);

                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 3);
                addCylinderIndex(ii + 2);
            }
            else
            {
                addCylinderIndex(ii);
                addCylinderIndex(ii + 2);
                addCylinderIndex(ii + 1);

                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 2);
                addCylinderIndex(ii + 3);
            }
        }

        offset = data.Vertices.GetSize();
    };

    const auto addEdge01 = [&](const f32 sx, const f32 sy) {
        const f32 mnx = 0.5f * sx;
        const f32 mxx = sx;

        const f32 mny = 0.5f * sy;
        const f32 mxy = sy;

        const f32 angle = 2.f * Math::Pi<f32>() / whole;

        const u32 halfPart = part / 2;

        // At j=0 the normal points along X, at j=part it points along Y
        const u32 faceA = (sx > 0.f) ? 0u : 1u; // +X or -X
        const u32 faceB = (sy > 0.f) ? 2u : 3u; // +Y or -Y

        // First half
        for (u32 j = 0; j <= halfPart; ++j)
        {
            const f32 cc = Math::Cosine(j * angle);
            const f32 ss = Math::Sine(j * angle);
            const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(ss, 0.f, 1.f, mny, mxy);

            addCylinderVertex01(x, y, -0.5f, faceA);
            addCylinderVertex01(x, y, 0.5f, faceA);
        }

        // Duplicate midpoint for faceB
        {
            const f32 cc = Math::Cosine(halfPart * angle);
            const f32 ss = Math::Sine(halfPart * angle);
            const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(ss, 0.f, 1.f, mny, mxy);

            addCylinderVertex01(x, y, -0.5f, faceB);
            addCylinderVertex01(x, y, 0.5f, faceB);
        }

        // Second half
        for (u32 j = halfPart + 1; j <= part; ++j)
        {
            const f32 cc = Math::Cosine(j * angle);
            const f32 ss = Math::Sine(j * angle);
            const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(ss, 0.f, 1.f, mny, mxy);

            addCylinderVertex01(x, y, -0.5f, faceB);
            addCylinderVertex01(x, y, 0.5f, faceB);
        }

        // Indexing
        const u32 totalPairs = part + 2;
        for (u32 p = 0; p < totalPairs - 1; ++p)
        {
            if (p == halfPart)
                continue;

            const u32 ii = 2 * p;
            if (sx * sy < 0.f)
            {
                addCylinderIndex(ii);
                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 2);

                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 3);
                addCylinderIndex(ii + 2);
            }
            else
            {
                addCylinderIndex(ii);
                addCylinderIndex(ii + 2);
                addCylinderIndex(ii + 1);

                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 2);
                addCylinderIndex(ii + 3);
            }
        }

        offset = data.Vertices.GetSize();
    };
    addEdge02(1.f, 1.f);
    addEdge02(-1.f, 1.f);
    addEdge02(1.f, -1.f);
    addEdge02(-1.f, -1.f);

    addEdge12(1.f, 1.f);
    addEdge12(-1.f, 1.f);
    addEdge12(1.f, -1.f);
    addEdge12(-1.f, -1.f);

    addEdge01(1.f, 1.f);
    addEdge01(-1.f, 1.f);
    addEdge01(1.f, -1.f);
    addEdge01(-1.f, -1.f);

    const StatMeshData<D3> cdata = createBoxMeshData(offset, 1.f);
    for (const StatVertex<D3> &vx : cdata.Vertices)
    {
        const f32v2 uv = computeRoundedBoxTexCoords(vx.Position, vx.Normal);
        data.Vertices.Append(ParaVertex<D3>{vx.Position, uv, vx.Normal, vx.Tangent, 0});
    }
    data.Indices.Insert(data.Indices.end(), cdata.Indices.begin(), cdata.Indices.end());

    VALIDATE_MESH_DATA(data);
    return data;
}

ParaMeshData<D3> CreateTorusMeshData(const u32 rings, const u32 sectors)
{
    ParaMeshData<D3> data{};
    data.Shape = ParametricShape_Torus;

    // Major radius: center of tube to center of torus
    // Minor radius: tube radius
    // Total diameter = 2 * (major + minor) = 1  →  major + minor = 0.5
    // Convention: minor = major / 2  →  major = 1/3, minor = 1/6
    // (adjust ratio to taste)
    constexpr f32 R = 1.f / 3.f;
    constexpr f32 r = 1.f / 6.f;

    // Vertices: (rings + 1) x (sectors + 1) grid with wrapped UVs
    for (u32 i = 0; i <= rings; ++i)
    {
        const f32 u = f32(i) / rings;
        const f32 theta = 2.f * u * Math::Pi<f32>(); // major angle
        const f32 ct = Math::Cosine(theta);
        const f32 st = Math::Sine(theta);

        for (u32 j = 0; j <= sectors; ++j)
        {
            const f32 v = f32(j) / sectors;
            const f32 phi = 2.f * v * Math::Pi<f32>(); // minor angle
            const f32 cp = Math::Cosine(phi);
            const f32 sp = Math::Sine(phi);

            // Position
            const f32 x = (R + r * cp) * ct;
            const f32 y = r * sp;
            const f32 z = (R + r * cp) * st;

            // Normal: points from tube center toward surface
            const f32v3 normal = Math::Normalize(f32v3{cp * ct, sp, cp * st});

            // Tangent: direction of increasing U (along major circle)
            const f32v4 tangent = f32v4{-st, 0.f, ct, 1.f};

            data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, normal, tangent, 0});
        }
    }

    // Indices: quads between adjacent rings/sectors
    for (u32 i = 0; i < rings; ++i)
    {
        for (u32 j = 0; j < sectors; ++j)
        {
            const u32 a = i * (sectors + 1) + j;
            const u32 b = a + sectors + 1;

            data.Indices.Append(Index(a));
            data.Indices.Append(Index(a + 1));
            data.Indices.Append(Index(b));

            data.Indices.Append(Index(a + 1));
            data.Indices.Append(Index(b + 1));
            data.Indices.Append(Index(b));
        }
    }

    VALIDATE_MESH_DATA(data);
    return data;
}
template StatMeshData<D2> CreateTriangleMeshData<D2>();
template StatMeshData<D3> CreateTriangleMeshData<D3>();

template StatMeshData<D2> CreateQuadMeshData<D2>();
template StatMeshData<D3> CreateQuadMeshData<D3>();

template StatMeshData<D2> CreateRegularPolygonMeshData<D2>(u32);
template StatMeshData<D3> CreateRegularPolygonMeshData<D3>(u32);

template StatMeshData<D2> CreatePolygonMeshData<D2>(TKit::Span<const f32v2>);
template StatMeshData<D3> CreatePolygonMeshData<D3>(TKit::Span<const f32v2>);

template ParaMeshData<D2> CreateStadiumMeshData<D2>();
template ParaMeshData<D3> CreateStadiumMeshData<D3>();

template ParaMeshData<D2> CreateRoundedQuadMeshData<D2>();
template ParaMeshData<D3> CreateRoundedQuadMeshData<D3>();

#ifdef ONYX_ENABLE_OBJ_LOAD
template Result<StatMeshData<D2>> LoadStaticMeshDataFromObjFile<D2>(const char *path, LoadObjDataFlags flags);
template Result<StatMeshData<D3>> LoadStaticMeshDataFromObjFile<D3>(const char *path, LoadObjDataFlags flags);
#endif
} // namespace Onyx
