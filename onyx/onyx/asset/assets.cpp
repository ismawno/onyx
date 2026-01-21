#include "onyx/core/pch.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/core/limits.hpp"
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

    u32 GetVertexCount(u32 p_Size = TKIT_U32_MAX) const
    {
        if (Layouts.IsEmpty() || p_Size == 0)
            return 0;
        if (p_Size == TKIT_U32_MAX)
            p_Size = Layouts.GetSize();
        return Layouts[p_Size - 1].VertexStart + Layouts[p_Size - 1].VertexCount;
    }
    u32 GetIndexCount(u32 p_Size = TKIT_U32_MAX) const
    {
        if (Layouts.IsEmpty() || p_Size == 0)
            return 0;
        if (p_Size == TKIT_U32_MAX)
            p_Size = Layouts.GetSize();
        return Layouts[p_Size - 1].IndexStart + Layouts[p_Size - 1].IndexCount;
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

template <typename Vertex> static void checkSize(MeshInfo<Vertex> &p_Info)
{
    LayoutFlags flags = 0;

    const u32 vcount = p_Info.GetVertexCount();
    if (Resources::GrowBufferIfNeeded<Vertex>(p_Info.VertexBuffer, vcount, Buffer_DeviceVertex))
        flags = LayoutFlag_UpdateVertex;

    const u32 icount = p_Info.GetIndexCount();
    if (Resources::GrowBufferIfNeeded<Index>(p_Info.IndexBuffer, icount, Buffer_DeviceIndex))
        flags |= LayoutFlag_UpdateIndex;
    if (flags)
        for (DataLayout &layout : p_Info.Layouts)
            layout.Flags |= flags;
}

template <typename Vertex> static void uploadVertexData(MeshInfo<Vertex> &p_Info, const u32 p_Start, const u32 p_End)
{
    constexpr VkDeviceSize size = sizeof(Vertex);
    const VkDeviceSize voffset = p_Info.GetVertexCount(p_Start) * size;
    const VkDeviceSize vsize = p_Info.GetVertexCount(p_End) * size - voffset;

    VKit::CommandPool &pool = Execution::GetTransientTransferPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
    VKIT_CHECK_EXPRESSION(
        p_Info.VertexBuffer.UploadFromHost(pool, queue->GetHandle(), p_Info.Meshes.Vertices.GetData(),
                                           {.srcOffset = voffset, .dstOffset = voffset, .size = vsize}));
}
template <typename Vertex> static void uploadIndexData(MeshInfo<Vertex> &p_Info, const u32 p_Start, const u32 p_End)
{
    constexpr VkDeviceSize size = sizeof(Index);
    const VkDeviceSize ioffset = p_Info.GetIndexCount(p_Start) * size;
    const VkDeviceSize isize = p_Info.GetIndexCount(p_End) * size - ioffset;

    VKit::CommandPool &pool = Execution::GetTransientTransferPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
    VKIT_CHECK_EXPRESSION(
        p_Info.IndexBuffer.UploadFromHost(pool, queue->GetHandle(), p_Info.Meshes.Indices.GetData(),
                                          {.srcOffset = ioffset, .dstOffset = ioffset, .size = isize}));
}

template <typename Vertex> static void uploadMeshData(MeshInfo<Vertex> &p_Info)
{
    TKIT_ASSERT(!p_Info.Layouts.IsEmpty(), "[ONYX][ASSETS] Cannot upload assets. Layouts is empty");
    const auto checkFlags = [&p_Info](const u32 p_Index, const LayoutFlags p_Flags) {
        return p_Flags & p_Info.Layouts[p_Index].Flags;
    };

    checkSize(p_Info);
    auto &layouts = p_Info.Layouts;
    for (u32 i = 0; i < layouts.GetSize(); ++i)
        if (checkFlags(i, LayoutFlag_UpdateVertex))
            for (u32 j = i + 1; j <= layouts.GetSize(); ++j)
                if (j == layouts.GetSize() || !checkFlags(j, LayoutFlag_UpdateVertex))
                {
                    uploadVertexData(p_Info, i, j);
                    i = j;
                    break;
                }
    for (u32 i = 0; i < layouts.GetSize(); ++i)
        if (checkFlags(i, LayoutFlag_UpdateIndex))
            for (u32 j = i + 1; j <= layouts.GetSize(); ++j)
                if (j == layouts.GetSize() || !checkFlags(j, LayoutFlag_UpdateIndex))
                {
                    uploadIndexData(p_Info, i, j);
                    i = j;
                    break;
                }
    for (DataLayout &layout : layouts)
        layout.Flags = 0;
}

template <typename Vertex> static void initialize(MeshInfo<Vertex> &p_Info, const u32 p_MaxLayouts)
{
    p_Info.VertexBuffer = Resources::CreateBuffer<Vertex>(Buffer_DeviceVertex);
    p_Info.IndexBuffer = Resources::CreateBuffer<Index>(Buffer_DeviceIndex);
    p_Info.Layouts.Reserve(p_MaxLayouts);
}

template <Dimension D> static void initialize(const Specs &p_Specs)
{
    AssetData<D> &data = getData<D>();
    initialize(data.StaticMeshes, p_Specs.MaxStaticMeshes);
}

template <typename Vertex> static void terminate(MeshInfo<Vertex> &p_Info)
{
    p_Info.VertexBuffer.Destroy();
    p_Info.IndexBuffer.Destroy();
    p_Info.Layouts.Clear();
    p_Info.Meshes.Indices.Clear();
    p_Info.Meshes.Vertices.Clear();
}

template <Dimension D> static void terminate()
{
    AssetData<D> &data = getData<D>();
    terminate(data.StaticMeshes);
}

void Initialize(const Specs &p_Specs)
{
    s_BatchRanges[Geometry_Circle] = {.BatchStart = 0, .BatchCount = 1};
    s_BatchRanges[Geometry_StaticMesh] = {.BatchStart = 1, .BatchCount = p_Specs.MaxStaticMeshes};

    initialize<D2>(p_Specs);
    initialize<D3>(p_Specs);
}

void Terminate()
{
    terminate<D2>();
    terminate<D3>();
}

u32 GetStaticMeshBatchIndex(const Mesh p_Mesh)
{
    TKIT_ASSERT(p_Mesh < s_BatchRanges[Geometry_StaticMesh].BatchCount,
                "[ONYX] Mesh index overflow. The mesh index {} does not have a valid assigned batch. This may have "
                "happened because the used mesh does not point to a valid static mesh, or the amount of static mesh "
                "types exceeds the maximum of {}. Consider increasing such maximum from the Onyx initialization specs",
                p_Mesh, s_BatchRanges[Geometry_StaticMesh].BatchCount);
    return p_Mesh + s_BatchRanges[Geometry_StaticMesh].BatchStart;
}
u32 GetStaticMeshIndexFromBatch(const u32 p_Batch)
{
    TKIT_ASSERT(p_Batch < GetBatchEnd(Geometry_StaticMesh),
                "[ONYX] Batch index overflow. The batch index {} does not have a valid assigned mesh. This may have "
                "happened because the used batch does not point to a valid static mesh batch",
                p_Batch, GetBatchEnd(Geometry_StaticMesh));
    return p_Batch - s_BatchRanges[Geometry_StaticMesh].BatchStart;
}
u32 GetCircleBatchIndex()
{
    return s_BatchRanges[Geometry_Circle].BatchStart;
}

u32 GetBatchStart(const GeometryType p_Geo)
{
    return s_BatchRanges[p_Geo].BatchStart;
}
u32 GetBatchEnd(const GeometryType p_Geo)
{
    return s_BatchRanges[p_Geo].BatchStart + s_BatchRanges[p_Geo].BatchCount;
}
u32 GetBatchCount(const GeometryType p_Geo)
{
    return s_BatchRanges[p_Geo].BatchCount;
}
u32 GetBatchCount()
{
    u32 count = 0;
    for (u32 i = 0; i < Geometry_Count; ++i)
        count += s_BatchRanges[i].BatchCount;
    return count;
}

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &p_Data)
{
    StatMeshInfo<D> &data = getData<D>().StaticMeshes;

    const Mesh mesh = data.Layouts.GetSize();
    DataLayout layout;
    layout.VertexStart = data.GetVertexCount();
    layout.VertexCount = p_Data.Vertices.GetSize();
    layout.IndexStart = data.GetIndexCount();
    layout.IndexCount = p_Data.Indices.GetSize();
    layout.Flags = LayoutFlag_UpdateVertex | LayoutFlag_UpdateIndex;

    data.Layouts.Append(layout);

    auto &vertices = data.Meshes.Vertices;
    auto &indices = data.Meshes.Indices;
    vertices.Insert(vertices.end(), p_Data.Vertices.begin(), p_Data.Vertices.end());
    indices.Insert(indices.end(), p_Data.Indices.begin(), p_Data.Indices.end());

    return mesh;
}

template <Dimension D> void UpdateMesh(const Mesh p_Mesh, const StatMeshData<D> &p_Data)
{
    StatMeshInfo<D> &data = getData<D>().StaticMeshes;

    DataLayout &layout = data.Layouts[p_Mesh];
    TKIT_ASSERT(p_Data.Vertices.GetSize() == layout.VertexCount && p_Data.Indices.GetSize() == layout.IndexCount,
                "[ONYX] When updating a mesh, the vertex and index count of the previous and updated mesh must be the "
                "same. If they are not, you must create a new mesh");

    TKit::Memory::ForwardCopy(data.Meshes.Vertices.begin() + layout.VertexStart, p_Data.Vertices.begin(),
                              p_Data.Vertices.end());
    TKit::Memory::ForwardCopy(data.Meshes.Indices.begin() + layout.IndexStart, p_Data.Indices.begin(),
                              p_Data.Indices.end());

    layout.Flags |= LayoutFlag_UpdateVertex | LayoutFlag_UpdateIndex;
}

template <Dimension D> u32 GetStaticMeshCount()
{
    return getData<D>().StaticMeshes.Layouts.GetSize();
}

template <Dimension D> void Upload()
{
    Core::DeviceWaitIdle();
    AssetData<D> &data = getData<D>();
    uploadMeshData(data.StaticMeshes);
}

template <Dimension D> MeshDataLayout GetStaticMeshLayout(const Mesh p_Mesh)
{
    const DataLayout &layout = getData<D>().StaticMeshes.Layouts[p_Mesh];
    return MeshDataLayout{
        .VertexStart = layout.VertexStart, .IndexStart = layout.IndexStart, .IndexCount = layout.IndexCount};
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
template <Dimension D> VKit::Result<StatMeshData<D>> LoadStaticMesh(const char *p_Path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, nullptr, &warn, &err, p_Path))
        return VKit::Result<StatMeshData<D>>::Error(VK_ERROR_INITIALIZATION_FAILED,
                                                    TKit::Format("Failed to load mesh: {}", err + warn));

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
template <Dimension D> static void validateMesh(const StatMeshData<D> &p_Data, const u32 p_Offset = 0)
{
    Index mx = 0;
    for (const Index i : p_Data.Indices)
        if (i > mx)
            mx = i;

    mx -= static_cast<Index>(p_Offset);
    TKIT_ASSERT(
        mx < p_Data.Vertices.GetSize(),
        "[ONYX] Index and vertex host data creation is invalid. An index exceeds vertex bounds. Index: {}, size: {}",
        mx, p_Data.Vertices.GetSize());
}
#    define VALIDATE_MESH(...) validateMesh(__VA_ARGS__)
#else
#    define VALIDATE_MESH(...)
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh()
{
    StatMeshData<D> data{};
    const auto addVertex = [&data](const f32 p_X, const f32 p_Y) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{f32v2{p_X, p_Y}});
        else
            data.Vertices.Append(StatVertex<D3>{f32v3{p_X, p_Y, 0.f}, f32v3{0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 p_Index) { data.Indices.Append(static_cast<Index>(p_Index)); };

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
    const auto addVertex = [&data](const f32 p_X, const f32 p_Y) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{f32v2{p_X, p_Y}});
        else
            data.Vertices.Append(StatVertex<D3>{f32v3{p_X, p_Y, 0.f}, f32v3{0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 p_Index) { data.Indices.Append(static_cast<Index>(p_Index)); };

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
static StatMeshData<D> createRegularPolygon(const u32 p_Sides, const f32v<D> &p_VertexOffset = f32v<D>{0.f},
                                            const u32 p_IndexOffset = 0, const f32v3 &p_Normal = f32v3{0.f, 0.f, 1.f})
{
    TKIT_ASSERT(p_Sides >= 3, "[ONYX] A regular polygon requires at least 3 sides");
    StatMeshData<D> data{};
    const auto addVertex = [&](const f32v<D> &p_Vertex) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{p_Vertex + p_VertexOffset});
        else
            data.Vertices.Append(StatVertex<D3>{p_Vertex + p_VertexOffset, p_Normal});
    };
    const auto addIndex = [&data, p_IndexOffset](const u32 p_Index) {
        data.Indices.Append(static_cast<Index>(p_Index + p_IndexOffset));
    };

    addVertex(f32v<D>{0.f});
    const f32 angle = 2.f * Math::Pi<f32>() / p_Sides;
    for (u32 i = 0; i < p_Sides; ++i)
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
            addIndex(idx > p_Sides ? 1 : idx);
        }
        else
        {
            addIndex(0);
            const u32 idx = i + 2;
            addIndex(idx > p_Sides ? 1 : idx);
            addIndex(i + 1);
        }
    }
    VALIDATE_MESH(data, p_IndexOffset);
    return data;
}
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(const u32 p_Sides)
{
    return createRegularPolygon<D>(p_Sides);
}

template <Dimension D> StatMeshData<D> CreatePolygonMesh(const TKit::Span<const f32v2> p_Vertices)
{
    TKIT_ASSERT(p_Vertices.GetSize() >= 3, "[ONYX] A polygon must have at least 3 vertices");
    StatMeshData<D> data{};

    const auto addVertex = [&data](const f32v2 &p_Vertex) {
        if constexpr (D == D3)
            data.Vertices.Append(StatVertex<D3>{f32v3{p_Vertex, 0.f}, f32v3{0.f, 0.f, 1.f}});
        else
            data.Vertices.Append(StatVertex<D2>{p_Vertex});
    };
    const auto addIndex = [&data](const u32 p_Index) { data.Indices.Append(static_cast<Index>(p_Index)); };

    addVertex(f32v<D>{0.f});
    const u32 size = p_Vertices.GetSize();
    for (u32 i = 0; i < size; ++i)
    {
        const f32v2 &vertex = p_Vertices[i];
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
    const auto addVertex = [&data](const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_N0, const f32 p_N1,
                                   const f32 p_N2) {
        data.Vertices.Append(StatVertex<D3>{f32v3{p_X, p_Y, p_Z}, f32v3{p_N0, p_N1, p_N2}});
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

    VALIDATE_MESH(data);
    return data;
}
StatMeshData<D3> CreateSphereMesh(const u32 p_Rings, const u32 p_Sectors)
{
    const Index rings = p_Rings + 2;
    StatMeshData<D3> data{};
    const auto addVertex = [&data](const f32 p_X, const f32 p_Y, const f32 p_Z) {
        const f32v3 vertex = f32v3{p_X, p_Y, p_Z};
        data.Vertices.Append(StatVertex<D3>{vertex, Math::Normalize(vertex)});
    };
    const auto addIndex = [&data, p_Sectors, rings](const u32 p_Ring, const u32 p_Sector) {
        u32 idx;
        if (p_Ring == 0)
            idx = 0;
        else if (p_Ring == rings - 1)
            idx = 1 + (rings - 2) * p_Sectors;
        else
            idx = 1 + p_Sector + (p_Ring - 1) * p_Sectors;
        data.Indices.Append(static_cast<Index>(idx));
    };

    addVertex(0.f, 0.5f, 0.f);
    for (u32 i = 1; i < rings - 1; ++i)
    {
        const f32 v = static_cast<f32>(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        for (u32 j = 0; j < p_Sectors; ++j)
        {
            const f32 u = static_cast<f32>(j) / p_Sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            addVertex(0.5f * ps * tc, 0.5f * pc, 0.5f * ps * ts);

            const u32 ii = i - 1;
            const u32 jj = (j + 1) % p_Sectors;
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
    for (u32 j = 0; j < p_Sectors; ++j)
    {
        addIndex(rings - 2, j);
        addIndex(rings - 2, (j + 1) % p_Sectors);
        addIndex(rings - 1, j);
    }

    VALIDATE_MESH(data);
    return data;
}
StatMeshData<D3> CreateCylinderMesh(const u32 p_Sides)
{
    const StatMeshData<D3> left =
        createRegularPolygon<D3, true, false>(p_Sides, f32v3{-0.5f, 0.f, 0.f}, 0, f32v3{-1.f, 0.f, 0.f});

    const StatMeshData<D3> right = createRegularPolygon<D3, true, true>(p_Sides, f32v3{0.5f, 0.f, 0.f},
                                                                        left.Vertices.GetSize(), f32v3{1.f, 0.f, 0.f});

    StatMeshData<D3> data{};
    data.Indices.Insert(data.Indices.end(), left.Indices.begin(), left.Indices.end());
    data.Indices.Insert(data.Indices.end(), right.Indices.begin(), right.Indices.end());

    data.Vertices.Insert(data.Vertices.end(), left.Vertices.begin(), left.Vertices.end());
    data.Vertices.Insert(data.Vertices.end(), right.Vertices.begin(), right.Vertices.end());

    const auto addVertex = [&data](const f32 p_X, const f32 p_Y, const f32 p_Z) {
        const f32v3 vertex = f32v3{p_X, p_Y, p_Z};
        data.Vertices.Append(StatVertex<D3>{vertex, Math::Normalize(f32v3{0.f, p_Y, p_Z})});
    };

    const u32 offset = left.Vertices.GetSize() + right.Vertices.GetSize();
    const auto addIndex = [&data, offset](const u32 p_Index) {
        data.Indices.Append(static_cast<Index>(p_Index + offset));
    };

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

    VALIDATE_MESH(data);
    return data;
}

template Mesh AddMesh(const StatMeshData<D2> &p_Data);
template Mesh AddMesh(const StatMeshData<D3> &p_Data);

template void UpdateMesh(Mesh p_Mesh, const StatMeshData<D2> &p_Data);
template void UpdateMesh(Mesh p_Mesh, const StatMeshData<D3> &p_Data);

template u32 GetStaticMeshCount<D2>();
template u32 GetStaticMeshCount<D3>();

template void Upload<D2>();
template void Upload<D3>();

#ifdef ONYX_ENABLE_OBJ
template VKit::Result<StatMeshData<D2>> LoadStaticMesh<D2>(const char *p_Path);
template VKit::Result<StatMeshData<D3>> LoadStaticMesh<D3>(const char *p_Path);
#endif

template const VKit::DeviceBuffer &GetStaticMeshVertexBuffer<D2>();
template const VKit::DeviceBuffer &GetStaticMeshVertexBuffer<D3>();

template const VKit::DeviceBuffer &GetStaticMeshIndexBuffer<D2>();
template const VKit::DeviceBuffer &GetStaticMeshIndexBuffer<D3>();

template MeshDataLayout GetStaticMeshLayout<D2>(Mesh p_Mesh);
template MeshDataLayout GetStaticMeshLayout<D3>(Mesh p_Mesh);

template StatMeshData<D2> CreateTriangleMesh<D2>();
template StatMeshData<D3> CreateTriangleMesh<D3>();

template StatMeshData<D2> CreateSquareMesh<D2>();
template StatMeshData<D3> CreateSquareMesh<D3>();

template StatMeshData<D2> CreateRegularPolygonMesh<D2>(u32);
template StatMeshData<D3> CreateRegularPolygonMesh<D3>(u32);

template StatMeshData<D2> CreatePolygonMesh<D2>(TKit::Span<const f32v2>);
template StatMeshData<D3> CreatePolygonMesh<D3>(TKit::Span<const f32v2>);

} // namespace Onyx::Assets
