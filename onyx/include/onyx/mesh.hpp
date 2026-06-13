#pragma once

#include "onyx/core.hpp"
#include "onyx/vertex.hpp"
#include "onyx/color.hpp"
#include "tkit/container/dynamic_array.hpp"
#include "tkit/container/span.hpp"

namespace Onyx
{
template <Dimension D> struct BoundsData
{
    static constexpr Dimension Dim = D;
    f32v<D> Min{0.f};
    f32v<D> Center{0.f};
    f32v<D> Max{0.f};

    const f32 *GetData() const
    {
        return Center.GetData();
    }
};

using MeshDataFlags = u8;
enum MeshDataFlagBit : MeshDataFlags
{
    MeshDataFlag_BackCulled = 1U << 0, // no effect for 2D
};

template <typename Vertex> struct MeshData
{
    TKit::DynamicArray<Vertex> Vertices{};
    TKit::DynamicArray<Index> Indices{};
    MeshDataFlags Flags = 0;
};

template <Dimension D> struct MeshData<ParametricVertex<D>>
{
    TKit::DynamicArray<ParametricVertex<D>> Vertices{};
    TKit::DynamicArray<Index> Indices{};
    ParametricShape Shape{};
    MeshDataFlags Flags = 0;
};

template <Dimension D> using StaticMeshData = MeshData<StaticVertex<D>>;
template <Dimension D> using DynamicMeshData = MeshData<DynamicVertex<D>>;
template <Dimension D> using ParametricMeshData = MeshData<ParametricVertex<D>>;

template <Dimension D> struct DynamicMeshInfo
{
    DynamicMeshData<D> *Data;
    Resource Handle;
};

struct MeshDataLayout
{
    u32 VertexStart = 0;
    u32 VertexCount = 0;
    u32 IndexStart = 0;
    u32 IndexCount = 0;
};

enum Topology : u8
{
    Topology_TriangleList,
    Topology_TriangleStrip,
    Topology_TriangleFan,
};

#ifdef ONYX_ENABLE_OBJ_LOAD
template <Dimension D>
ONYX_NO_DISCARD Result<StaticMeshData<D>> LoadStaticMeshDataFromObjFile(const char *path, u32 maxVertices = 2048);
#endif

// TODO(Isma): Triangle is a bit up-shifted. bring it down
template <Dimension D>
StaticMeshData<D> CreateTriangleMeshData(const f32v2 &left = f32v2{-0.433013f, -0.25f},
                                         const f32v2 &right = f32v2{0.433013f, -0.25f},
                                         const f32v2 &top = f32v2{0.f, 0.5f});

template <Dimension D>
StaticMeshData<D> CreateQuadMeshData(const f32v2 &bl = f32v2{-0.5f}, const f32v2 &br = f32v2{0.5f, -0.5f},
                                     const f32v2 &tl = f32v2{-0.5f, 0.5f}, const f32v2 &tr = f32v2{0.5f});
template <Dimension D> StaticMeshData<D> CreateRegularPolygonMeshData(u32 sides);
template <Dimension D> StaticMeshData<D> CreatePolygonMeshData(TKit::Span<const f32v2> vertices);

// rings and sectors should be even

StaticMeshData<D3> CreateBoxMeshData();
StaticMeshData<D3> CreateSphereMeshData(u32 rings = 16, u32 sectors = 32);
StaticMeshData<D3> CreateCylinderMeshData(u32 sides = 32);

template <Dimension D> ParametricMeshData<D> CreateStadiumMeshData();
template <Dimension D> ParametricMeshData<D> CreateRoundedRectMeshData();

ParametricMeshData<D3> CreateCapsuleMeshData(u32 rings = 16, u32 sectors = 32);
ParametricMeshData<D3> CreateRoundedBoxMeshData(u32 rings = 16, u32 sectors = 32);
ParametricMeshData<D3> CreateTorusMeshData(u32 rings = 32, u32 sectors = 32);

template <Dimension D>
DynamicMeshData<D> CreateDynamicMeshData(TKit::Span<const DynamicVertex<D>> vertices, TKit::Span<const Index> indices,
                                         MeshDataFlags flags = 0);
template <Dimension D>
DynamicMeshData<D> CreateDynamicMeshData(TKit::Span<const DynamicVertex<D>> vertices,
                                         Topology topology = Topology_TriangleList, MeshDataFlags flags = 0);
template <Dimension D>
DynamicMeshData<D> CreateDynamicMeshData(TKit::Span<const f32v<D>> vertices, TKit::Span<const Index> indices,
                                         const Color &color = Color_White, MeshDataFlags flags = 0);
template <Dimension D>
DynamicMeshData<D> CreateDynamicMeshData(TKit::Span<const f32v<D>> vertices, Topology topology = Topology_TriangleList,
                                         const Color &color = Color_White, MeshDataFlags flags = 0);

template <typename Vertex> BoundsData<Vertex::Dim> CreateBoundsData(const MeshData<Vertex> &data)
{
    constexpr Dimension D = Vertex::Dim;
    BoundsData<D> bounds;
    bounds.Center = f32v<D>{0.f};
    for (const Vertex &vx : data.Vertices)
    {
        bounds.Center += vx.Position;
        for (u32 i = 0; i < D; ++i)
        {
            if (bounds.Min[i] > vx.Position[i])
                bounds.Min[i] = vx.Position[i];
            if (bounds.Max[i] < vx.Position[i])
                bounds.Max[i] = vx.Position[i];
        }
    }
    bounds.Center /= data.Vertices.GetSize();
    return bounds;
}

} // namespace Onyx
