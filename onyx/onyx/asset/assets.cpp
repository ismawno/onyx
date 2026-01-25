#include "onyx/core/pch.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/resource/resources.hpp"
#ifdef ONYX_ENABLE_OBJ
#    include <tiny_obj_loader.h>
#endif

namespace Onyx::Assets
{
using LayoutFlags = u8;
enum LayoutFlagBit : LayoutFlags
{
    LayoutFlag_UpdateVertex = 1 << 0,
    LayoutFlag_UpdateIndex = 1 << 1,
};

struct DataLayout
{
    u32 VertexStart;
    u32 VertexCount;
    u32 IndexStart;
    u32 IndexCount;
    LayoutFlags Flags;
};

struct BatchRange
{
    u32 BatchStart;
    u32 BatchCount;
};

TKit::FixedArray<BatchRange, Geometry_Count> s_BatchRanges{};

template <typename Vertex> struct MeshInfo
{
    VKit::DeviceBuffer VertexBuffer{};
    VKit::DeviceBuffer IndexBuffer{};
    TKit::ArenaArray<DataLayout> Layouts{};
    MeshData<Vertex> Meshes{};

    u32 GetVertexCount(u32 size = TKIT_U32_MAX) const
    {
        if (Layouts.IsEmpty() || size == 0)
            return 0;
        if (size == TKIT_U32_MAX)
            size = Layouts.GetSize();
        return Layouts[size - 1].VertexStart + Layouts[size - 1].VertexCount;
    }
    u32 GetIndexCount(u32 size = TKIT_U32_MAX) const
    {
        if (Layouts.IsEmpty() || size == 0)
            return 0;
        if (size == TKIT_U32_MAX)
            size = Layouts.GetSize();
        return Layouts[size - 1].IndexStart + Layouts[size - 1].IndexCount;
    }
};

template <Dimension D> using StatMeshInfo = MeshInfo<StatVertex<D>>;

template <Dimension D> struct AssetData
{
    StatMeshInfo<D> StaticMeshes;
};

static AssetData<D2> s_AssetData2{};
static AssetData<D3> s_AssetData3{};

template <Dimension D> static AssetData<D> &getData()
{
    if constexpr (D == D2)
        return s_AssetData2;
    else
        return s_AssetData3;
}

template <typename Vertex> ONYX_NO_DISCARD static Result<> checkSize(MeshInfo<Vertex> &info)
{
    LayoutFlags flags = 0;

    const u32 vcount = info.GetVertexCount();
    auto result = Resources::GrowBufferIfNeeded<Vertex>(info.VertexBuffer, vcount, Buffer_DeviceVertex);
    TKIT_RETURN_ON_ERROR(result);

    if (result.GetValue())
        flags = LayoutFlag_UpdateVertex;

    const u32 icount = info.GetIndexCount();
    result = Resources::GrowBufferIfNeeded<Index>(info.IndexBuffer, icount, Buffer_DeviceIndex);
    TKIT_RETURN_ON_ERROR(result);

    if (result.GetValue())
        flags |= LayoutFlag_UpdateIndex;
    if (flags)
        for (DataLayout &layout : info.Layouts)
            layout.Flags |= flags;
    return Result<>::Ok();
}

template <typename Vertex>
ONYX_NO_DISCARD static Result<> uploadVertexData(MeshInfo<Vertex> &info, const u32 start, const u32 end)
{
    constexpr VkDeviceSize size = sizeof(Vertex);
    const VkDeviceSize voffset = info.GetVertexCount(start) * size;
    const VkDeviceSize vsize = info.GetVertexCount(end) * size - voffset;

    VKit::CommandPool &pool = Execution::GetTransientTransferPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Transfer);

    return info.VertexBuffer.UploadFromHost(pool, queue->GetHandle(), info.Meshes.Vertices.GetData(),
                                            {.srcOffset = voffset, .dstOffset = voffset, .size = vsize});
}
template <typename Vertex>
ONYX_NO_DISCARD static Result<> uploadIndexData(MeshInfo<Vertex> &info, const u32 start, const u32 end)
{
    constexpr VkDeviceSize size = sizeof(Index);
    const VkDeviceSize ioffset = info.GetIndexCount(start) * size;
    const VkDeviceSize isize = info.GetIndexCount(end) * size - ioffset;

    VKit::CommandPool &pool = Execution::GetTransientTransferPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Transfer);

    return info.IndexBuffer.UploadFromHost(pool, queue->GetHandle(), info.Meshes.Indices.GetData(),
                                           {.srcOffset = ioffset, .dstOffset = ioffset, .size = isize});
}

template <typename Vertex> ONYX_NO_DISCARD static Result<> uploadMeshData(MeshInfo<Vertex> &info)
{
    TKIT_ASSERT(!info.Layouts.IsEmpty(), "[ONYX][ASSETS] Cannot upload assets. Layouts is empty");
    const auto checkFlags = [&info](const u32 index, const LayoutFlags flags) {
        return flags & info.Layouts[index].Flags;
    };

    TKIT_RETURN_IF_FAILED(checkSize(info));

    auto &layouts = info.Layouts;
    for (u32 i = 0; i < layouts.GetSize(); ++i)
        if (checkFlags(i, LayoutFlag_UpdateVertex))
            for (u32 j = i + 1; j <= layouts.GetSize(); ++j)
                if (j == layouts.GetSize() || !checkFlags(j, LayoutFlag_UpdateVertex))
                {
                    TKIT_RETURN_IF_FAILED(uploadVertexData(info, i, j));
                    i = j;
                    break;
                }
    for (u32 i = 0; i < layouts.GetSize(); ++i)
        if (checkFlags(i, LayoutFlag_UpdateIndex))
            for (u32 j = i + 1; j <= layouts.GetSize(); ++j)
                if (j == layouts.GetSize() || !checkFlags(j, LayoutFlag_UpdateIndex))
                {
                    TKIT_RETURN_IF_FAILED(uploadIndexData(info, i, j));
                    i = j;
                    break;
                }
    for (DataLayout &layout : layouts)
        layout.Flags = 0;
    return Result<>::Ok();
}

// terminate must be called if this fails (handled automatically when calling Core::Initialize())
template <typename Vertex> ONYX_NO_DISCARD static Result<> initialize(MeshInfo<Vertex> &info, const u32 maxLayouts)
{
    auto result = Resources::CreateBuffer<Vertex>(Buffer_DeviceVertex);
    TKIT_RETURN_ON_ERROR(result);
    info.VertexBuffer = result.GetValue();

    result = Resources::CreateBuffer<Index>(Buffer_DeviceIndex);
    TKIT_RETURN_ON_ERROR(result);
    info.IndexBuffer = result.GetValue();

    info.Layouts.Reserve(maxLayouts);
    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> initialize(const Specs &specs)
{
    AssetData<D> &data = getData<D>();
    return initialize(data.StaticMeshes, specs.MaxStaticMeshes);
}

template <typename Vertex> static void terminate(MeshInfo<Vertex> &info)
{
    info.VertexBuffer.Destroy();
    info.IndexBuffer.Destroy();
    info.Layouts.Clear();
    info.Meshes.Indices.Clear();
    info.Meshes.Vertices.Clear();
}

template <Dimension D> static void terminate()
{
    AssetData<D> &data = getData<D>();
    terminate(data.StaticMeshes);
}

Result<> Initialize(const Specs &specs)
{
    s_BatchRanges[Geometry_Circle] = {.BatchStart = 0, .BatchCount = 1};
    s_BatchRanges[Geometry_StaticMesh] = {.BatchStart = 1, .BatchCount = specs.MaxStaticMeshes};

    TKIT_RETURN_IF_FAILED(initialize<D2>(specs));
    return initialize<D3>(specs);
}

void Terminate()
{
    terminate<D2>();
    terminate<D3>();
}

u32 GetStaticMeshBatchIndex(const Mesh mesh)
{
    TKIT_ASSERT(
        mesh < s_BatchRanges[Geometry_StaticMesh].BatchCount,
        "[ONYX][ASSETS] Mesh index overflow. The mesh index {} does not have a valid assigned batch. This may have "
        "happened because the used mesh does not point to a valid static mesh, or the amount of static mesh "
        "types exceeds the maximum of {}. Consider increasing such maximum from the Onyx initialization specs",
        mesh, s_BatchRanges[Geometry_StaticMesh].BatchCount);
    return mesh + s_BatchRanges[Geometry_StaticMesh].BatchStart;
}
u32 GetStaticMeshIndexFromBatch(const u32 batch)
{
    TKIT_ASSERT(
        batch < GetBatchEnd(Geometry_StaticMesh),
        "[ONYX][ASSETS] Batch index overflow. The batch index {} does not have a valid assigned mesh. This may have "
        "happened because the used batch does not point to a valid static mesh batch",
        batch, GetBatchEnd(Geometry_StaticMesh));
    return batch - s_BatchRanges[Geometry_StaticMesh].BatchStart;
}
u32 GetCircleBatchIndex()
{
    return s_BatchRanges[Geometry_Circle].BatchStart;
}

u32 GetBatchStart(const GeometryType geo)
{
    return s_BatchRanges[geo].BatchStart;
}
u32 GetBatchEnd(const GeometryType geo)
{
    return s_BatchRanges[geo].BatchStart + s_BatchRanges[geo].BatchCount;
}
u32 GetBatchCount(const GeometryType geo)
{
    return s_BatchRanges[geo].BatchCount;
}
u32 GetBatchCount()
{
    u32 count = 0;
    for (u32 i = 0; i < Geometry_Count; ++i)
        count += s_BatchRanges[i].BatchCount;
    return count;
}

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &data)
{
    StatMeshInfo<D> &meshes = getData<D>().StaticMeshes;

    const Mesh mesh = meshes.Layouts.GetSize();
    DataLayout layout;
    layout.VertexStart = meshes.GetVertexCount();
    layout.VertexCount = data.Vertices.GetSize();
    layout.IndexStart = meshes.GetIndexCount();
    layout.IndexCount = data.Indices.GetSize();
    layout.Flags = LayoutFlag_UpdateVertex | LayoutFlag_UpdateIndex;

    meshes.Layouts.Append(layout);

    auto &vertices = meshes.Meshes.Vertices;
    auto &indices = meshes.Meshes.Indices;
    vertices.Insert(vertices.end(), data.Vertices.begin(), data.Vertices.end());
    indices.Insert(indices.end(), data.Indices.begin(), data.Indices.end());

    return mesh;
}

template <Dimension D> void UpdateMesh(const Mesh mesh, const StatMeshData<D> &data)
{
    StatMeshInfo<D> &meshes = getData<D>().StaticMeshes;

    DataLayout &layout = meshes.Layouts[mesh];
    TKIT_ASSERT(
        data.Vertices.GetSize() == layout.VertexCount && data.Indices.GetSize() == layout.IndexCount,
        "[ONYX][ASSETS] When updating a mesh, the vertex and index count of the previous and updated mesh must be the "
        "same. If they are not, you must create a new mesh");

    TKit::Memory::ForwardCopy(meshes.Meshes.Vertices.begin() + layout.VertexStart, data.Vertices.begin(),
                              data.Vertices.end());
    TKit::Memory::ForwardCopy(meshes.Meshes.Indices.begin() + layout.IndexStart, data.Indices.begin(),
                              data.Indices.end());

    layout.Flags |= LayoutFlag_UpdateVertex | LayoutFlag_UpdateIndex;
}

template <Dimension D> u32 GetStaticMeshCount()
{
    return getData<D>().StaticMeshes.Layouts.GetSize();
}

template <Dimension D> Result<> Upload()
{
    TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());

    AssetData<D> &data = getData<D>();
    return uploadMeshData(data.StaticMeshes);
}

template <Dimension D> MeshDataLayout GetStaticMeshLayout(const Mesh mesh)
{
    const DataLayout &layout = getData<D>().StaticMeshes.Layouts[mesh];
    return MeshDataLayout{.VertexStart = layout.VertexStart,
                          .VertexCount = layout.VertexCount,
                          .IndexStart = layout.IndexStart,
                          .IndexCount = layout.IndexCount};
}

template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshVertexBuffer()
{
    return getData<D>().StaticMeshes.VertexBuffer;
}
template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshIndexBuffer()
{
    return getData<D>().StaticMeshes.IndexBuffer;
}

#ifdef ONYX_ENABLE_OBJ
template <Dimension D> Result<StatMeshData<D>> LoadStaticMesh(const char *path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, nullptr, &warn, &err, path))
        return Result<StatMeshData<D>>::Error(Error_FileNotFound,
                                              TKit::Format("[ONYX][ASSETS] Failed to load mesh: {}", err + warn));

    std::unordered_map<StatVertex<D>, Index> uniqueVertices;
    StatMeshData<D> data;

    const u32 vcount = static_cast<u32>(attrib.vertices.size());

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
                uniqueVertices[vertex] = static_cast<Index>(uniqueVertices.size());
                data.Vertices.Append(vertex);
            }
            data.Indices.Append(uniqueVertices[vertex]);
        }
    return data;
}
#endif

#ifdef TKIT_ENABLE_ASSERTS
template <Dimension D> static void validateMesh(const StatMeshData<D> &data, const u32 offset = 0)
{
    Index mx = 0;
    for (const Index i : data.Indices)
        if (i > mx)
            mx = i;

    mx -= static_cast<Index>(offset);
    TKIT_ASSERT(mx < data.Vertices.GetSize(),
                "[ONYX][ASSETS] Index and vertex host data creation is invalid. An index exceeds vertex bounds. Index: "
                "{}, size: {}",
                mx, data.Vertices.GetSize());
}
#    define VALIDATE_MESH(...) validateMesh(__VA_ARGS__)
#else
#    define VALIDATE_MESH(...)
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh()
{
    StatMeshData<D> data{};
    const auto addVertex = [&data](const f32 x, const f32 y) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{f32v2{x, y}});
        else
            data.Vertices.Append(StatVertex<D3>{f32v3{x, y, 0.f}, f32v3{0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(static_cast<Index>(index)); };

    addVertex(0.f, 0.5f);
    addVertex(-0.433013f, -0.25f);
    addVertex(0.433013f, -0.25f);

    addIndex(0);
    addIndex(1);
    addIndex(2);
    VALIDATE_MESH(data);
    return data;
}
template <Dimension D> StatMeshData<D> CreateSquareMesh()
{
    StatMeshData<D> data{};
    const auto addVertex = [&data](const f32 x, const f32 y) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{f32v2{x, y}});
        else
            data.Vertices.Append(StatVertex<D3>{f32v3{x, y, 0.f}, f32v3{0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(static_cast<Index>(index)); };

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

    VALIDATE_MESH(data);
    return data;
}

template <Dimension D, bool Inverted = false, bool Counter = true>
static StatMeshData<D> createRegularPolygon(const u32 sides, const f32v<D> &vertexOffset = f32v<D>{0.f},
                                            const u32 indexOffset = 0, const f32v3 &normal = f32v3{0.f, 0.f, 1.f})
{
    TKIT_ASSERT(sides >= 3, "[ONYX][ASSETS] A regular polygon requires at least 3 sides");
    StatMeshData<D> data{};
    const auto addVertex = [&](const f32v<D> &vertex) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{vertex + vertexOffset});
        else
            data.Vertices.Append(StatVertex<D3>{vertex + vertexOffset, normal});
    };
    const auto addIndex = [&data, indexOffset](const u32 index) {
        data.Indices.Append(static_cast<Index>(index + indexOffset));
    };

    addVertex(f32v<D>{0.f});
    const f32 angle = 2.f * Math::Pi<f32>() / sides;
    for (u32 i = 0; i < sides; ++i)
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
    VALIDATE_MESH(data, indexOffset);
    return data;
}
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(const u32 sides)
{
    return createRegularPolygon<D>(sides);
}

template <Dimension D> StatMeshData<D> CreatePolygonMesh(const TKit::Span<const f32v2> vertices)
{
    TKIT_ASSERT(vertices.GetSize() >= 3, "[ONYX][ASSETS] A polygon must have at least 3 vertices");
    StatMeshData<D> data{};

    const auto addVertex = [&data](const f32v2 &vertex) {
        if constexpr (D == D3)
            data.Vertices.Append(StatVertex<D3>{f32v3{vertex, 0.f}, f32v3{0.f, 0.f, 1.f}});
        else
            data.Vertices.Append(StatVertex<D2>{vertex});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(static_cast<Index>(index)); };

    addVertex(f32v<D>{0.f});
    const u32 size = vertices.GetSize();
    for (u32 i = 0; i < size; ++i)
    {
        const f32v2 &vertex = vertices[i];
        addVertex(vertex);
        addIndex(0);
        addIndex(i + 1);

        const u32 idx = i + 2;
        addIndex(idx > size ? 1 : idx);
    }
    return data;
}

StatMeshData<D3> CreateCubeMesh()
{
    StatMeshData<D3> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 n0, const f32 n1, const f32 n2) {
        data.Vertices.Append(StatVertex<D3>{f32v3{x, y, z}, f32v3{n0, n1, n2}});
    };

    const auto addQuad = [&data](const Index a, const Index b, const Index c, const Index d) {
        data.Indices.Append(a);
        data.Indices.Append(b);
        data.Indices.Append(c);

        data.Indices.Append(a);
        data.Indices.Append(c);
        data.Indices.Append(d);
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

    VALIDATE_MESH(data);
    return data;
}
StatMeshData<D3> CreateSphereMesh(u32 rings, const u32 sectors)
{
    rings += 2;
    StatMeshData<D3> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z) {
        const f32v3 vertex = f32v3{x, y, z};
        data.Vertices.Append(StatVertex<D3>{vertex, Math::Normalize(vertex)});
    };
    const auto addIndex = [&data, sectors, rings](const u32 ring, const u32 sector) {
        u32 idx;
        if (ring == 0)
            idx = 0;
        else if (ring == rings - 1)
            idx = 1 + (rings - 2) * sectors;
        else
            idx = 1 + sector + (ring - 1) * sectors;
        data.Indices.Append(static_cast<Index>(idx));
    };

    addVertex(0.f, 0.5f, 0.f);
    for (u32 i = 1; i < rings - 1; ++i)
    {
        const f32 v = static_cast<f32>(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        for (u32 j = 0; j < sectors; ++j)
        {
            const f32 u = static_cast<f32>(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            addVertex(0.5f * ps * tc, 0.5f * pc, 0.5f * ps * ts);

            const u32 ii = i - 1;
            const u32 jj = (j + 1) % sectors;
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
    for (u32 j = 0; j < sectors; ++j)
    {
        addIndex(rings - 2, j);
        addIndex(rings - 2, (j + 1) % sectors);
        addIndex(rings - 1, j);
    }

    VALIDATE_MESH(data);
    return data;
}
StatMeshData<D3> CreateCylinderMesh(const u32 sides)
{
    const StatMeshData<D3> left =
        createRegularPolygon<D3, true, false>(sides, f32v3{-0.5f, 0.f, 0.f}, 0, f32v3{-1.f, 0.f, 0.f});

    const StatMeshData<D3> right = createRegularPolygon<D3, true, true>(sides, f32v3{0.5f, 0.f, 0.f},
                                                                        left.Vertices.GetSize(), f32v3{1.f, 0.f, 0.f});

    StatMeshData<D3> data{};
    data.Indices.Insert(data.Indices.end(), left.Indices.begin(), left.Indices.end());
    data.Indices.Insert(data.Indices.end(), right.Indices.begin(), right.Indices.end());

    data.Vertices.Insert(data.Vertices.end(), left.Vertices.begin(), left.Vertices.end());
    data.Vertices.Insert(data.Vertices.end(), right.Vertices.begin(), right.Vertices.end());

    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z) {
        const f32v3 vertex = f32v3{x, y, z};
        data.Vertices.Append(StatVertex<D3>{vertex, Math::Normalize(f32v3{0.f, y, z})});
    };

    const u32 offset = left.Vertices.GetSize() + right.Vertices.GetSize();
    const auto addIndex = [&data, offset](const u32 index) { data.Indices.Append(static_cast<Index>(index + offset)); };

    const f32 angle = 2.f * Math::Pi<f32>() / sides;
    for (u32 i = 0; i < sides; ++i)
    {
        const f32 c = 0.5f * Math::Cosine(i * angle);
        const f32 s = 0.5f * Math::Sine(i * angle);

        addVertex(-0.5f, c, s);
        addVertex(0.5f, c, s);
        const u32 ii = 2 * i;
        addIndex(ii);
        addIndex((ii + 2) % (2 * sides));
        addIndex(ii + 1);

        addIndex(ii + 1);
        addIndex((ii + 2) % (2 * sides));
        addIndex((ii + 3) % (2 * sides));
    }

    VALIDATE_MESH(data);
    return data;
}

template Mesh AddMesh(const StatMeshData<D2> &data);
template Mesh AddMesh(const StatMeshData<D3> &data);

template void UpdateMesh(Mesh mesh, const StatMeshData<D2> &data);
template void UpdateMesh(Mesh mesh, const StatMeshData<D3> &data);

template u32 GetStaticMeshCount<D2>();
template u32 GetStaticMeshCount<D3>();

template Result<> Upload<D2>();
template Result<> Upload<D3>();

#ifdef ONYX_ENABLE_OBJ
template Result<StatMeshData<D2>> LoadStaticMesh<D2>(const char *path);
template Result<StatMeshData<D3>> LoadStaticMesh<D3>(const char *path);
#endif

template const VKit::DeviceBuffer &GetStaticMeshVertexBuffer<D2>();
template const VKit::DeviceBuffer &GetStaticMeshVertexBuffer<D3>();

template const VKit::DeviceBuffer &GetStaticMeshIndexBuffer<D2>();
template const VKit::DeviceBuffer &GetStaticMeshIndexBuffer<D3>();

template MeshDataLayout GetStaticMeshLayout<D2>(Mesh mesh);
template MeshDataLayout GetStaticMeshLayout<D3>(Mesh mesh);

template StatMeshData<D2> CreateTriangleMesh<D2>();
template StatMeshData<D3> CreateTriangleMesh<D3>();

template StatMeshData<D2> CreateSquareMesh<D2>();
template StatMeshData<D3> CreateSquareMesh<D3>();

template StatMeshData<D2> CreateRegularPolygonMesh<D2>(u32);
template StatMeshData<D3> CreateRegularPolygonMesh<D3>(u32);

template StatMeshData<D2> CreatePolygonMesh<D2>(TKit::Span<const f32v2>);
template StatMeshData<D3> CreatePolygonMesh<D3>(TKit::Span<const f32v2>);

} // namespace Onyx::Assets
