#pragma once

#include "onyx/core/core.hpp"
#include "onyx/resource/buffer.hpp"
#include "onyx/property/vertex.hpp"
#include "tkit/container/dynamic_array.hpp"

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
template <typename Vertex> struct MeshData
{
    TKit::DynamicArray<Vertex> Vertices{};
    TKit::DynamicArray<Index> Indices{};
};

template <Dimension D> struct MeshData<ParaVertex<D>>
{
    TKit::DynamicArray<ParaVertex<D>> Vertices{};
    TKit::DynamicArray<Index> Indices{};
    ParametricShape Shape{};
};

template <Dimension D> using StatMeshData = MeshData<StatVertex<D>>;
template <Dimension D> using ParaMeshData = MeshData<ParaVertex<D>>;

struct MeshDataLayout
{
    u32 VertexStart = 0;
    u32 VertexCount = 0;
    u32 IndexStart = 0;
    u32 IndexCount = 0;
};

#ifdef ONYX_ENABLE_OBJ_LOAD
template <Dimension D> ONYX_NO_DISCARD Result<StatMeshData<D>> LoadStaticMeshDataFromObjFile(const char *path);
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMeshData();
template <Dimension D> StatMeshData<D> CreateQuadMeshData();
template <Dimension D> StatMeshData<D> CreateRegularPolygonMeshData(u32 sides);
template <Dimension D> StatMeshData<D> CreatePolygonMeshData(TKit::Span<const f32v2> vertices);

// rings and sectors should be even

StatMeshData<D3> CreateBoxMeshData();
StatMeshData<D3> CreateSphereMeshData(u32 rings = 16, u32 sectors = 32);
StatMeshData<D3> CreateCylinderMeshData(u32 sides = 32);

template <Dimension D> ParaMeshData<D> CreateStadiumMeshData();
template <Dimension D> ParaMeshData<D> CreateRoundedQuadMeshData();

ParaMeshData<D3> CreateCapsuleMeshData(u32 rings = 16, u32 sectors = 32);
ParaMeshData<D3> CreateRoundedBoxMeshData(u32 rings = 16, u32 sectors = 32);
ParaMeshData<D3> CreateTorusMeshData(u32 rings = 32, u32 sectors = 32);

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
