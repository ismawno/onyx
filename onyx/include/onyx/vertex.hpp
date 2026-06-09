#pragma once

#include "onyx/math.hpp"
#include "onyx/dimension.hpp"
#include "onyx/handle.hpp"
#include "onyx/instance.hpp"
#include "tkit/utils/hash.hpp"
#include "tkit/math/hash.hpp"

#define ONYX_INDEX_TYPE TKit::Alias::u32

namespace Onyx
{
using Index = ONYX_INDEX_TYPE;

template <Dimension D> struct DynamicVertex;
template <> struct DynamicVertex<D2>
{
    static constexpr Geometry Geo = Geometry_Dynamic;
    static constexpr ResourceType Resource = Resource_DynamicMesh;
    static constexpr Dimension Dim = D2;

    f32v2 Position;
    f32v2 TexCoord;
    u32 Color;

    friend bool operator==(const DynamicVertex<D2> &left, const DynamicVertex<D2> &right)
    {
        return left.Position == right.Position && left.TexCoord == right.TexCoord && left.Color == right.Color;
    }
};
template <> struct DynamicVertex<D3>
{
    static constexpr Geometry Geo = Geometry_Dynamic;
    static constexpr ResourceType Resource = Resource_DynamicMesh;
    static constexpr Dimension Dim = D3;

    f32v3 Position;
    f32v2 TexCoord;
    f32v3 Normal;
    f32v4 Tangent;
    u32 Color;

    friend bool operator==(const DynamicVertex<D3> &left, const DynamicVertex<D3> &right)
    {
        return left.Position == right.Position && left.TexCoord == right.TexCoord && left.Normal == right.Normal &&
               left.Tangent == right.Tangent && left.Color == right.Color;
    }
};

template <Dimension D> struct StaticVertex;
template <> struct StaticVertex<D2>
{
    static constexpr Geometry Geo = Geometry_Static;
    static constexpr ResourceType Resource = Resource_StaticMesh;
    static constexpr Dimension Dim = D2;

    f32v2 Position;
    f32v2 TexCoord;

    friend bool operator==(const StaticVertex<D2> &left, const StaticVertex<D2> &right)
    {
        return left.Position == right.Position && left.TexCoord == right.TexCoord;
    }
};

template <> struct StaticVertex<D3>
{
    static constexpr Geometry Geo = Geometry_Static;
    static constexpr ResourceType Resource = Resource_StaticMesh;
    static constexpr Dimension Dim = D3;

    f32v3 Position;
    f32v2 TexCoord;
    f32v3 Normal;
    f32v4 Tangent;

    friend bool operator==(const StaticVertex<D3> &left, const StaticVertex<D3> &right)
    {
        return left.Position == right.Position && left.TexCoord == right.TexCoord && left.Normal == right.Normal &&
               left.Tangent == right.Tangent;
    }
};

using ParametricRegionFlags = u32;
enum StadiumRegion : ParametricRegionFlags
{
    StadiumRegion_Edge = 1U << 0,
    StadiumRegion_Moon = 1U << 1,
};

enum RoundedRectRegion : ParametricRegionFlags
{
    RoundedRectRegion_HorizontalEdge = 1U << 0,
    RoundedRectRegion_VerticalEdge = 1U << 1,
    RoundedRectRegion_Moon = 1U << 2,
};

enum CapsuleRegion : ParametricRegionFlags
{
    CapsuleRegion_Body = 1U << 0,
};

template <Dimension D> struct ParametricVertex;
template <> struct ParametricVertex<D2>
{
    static constexpr Geometry Geo = Geometry_Parametric;
    static constexpr ResourceType Resource = Resource_ParametricMesh;
    static constexpr Dimension Dim = D2;

    f32v2 Position;
    f32v2 TexCoord;
    ParametricRegionFlags Region;
};

template <> struct ParametricVertex<D3>
{
    static constexpr Geometry Geo = Geometry_Parametric;
    static constexpr ResourceType Resource = Resource_ParametricMesh;
    static constexpr Dimension Dim = D3;

    f32v3 Position;
    f32v2 TexCoord;
    f32v3 Normal;
    f32v4 Tangent;
    ParametricRegionFlags Region;
};

struct GlyphVertex
{
    static constexpr Geometry Geo = Geometry_Glyph;
    static constexpr ResourceType Resource = Resource_Font;

    f32v2 Position;
    f32v2 AtlasCoord;
    f32v2 TexCoord;
};

} // namespace Onyx

template <> struct std::hash<Onyx::StaticVertex<Onyx::D2>>
{
    std::size_t operator()(const Onyx::StaticVertex<Onyx::D2> &vertex) const
    {
        return TKit::Hash(vertex.Position, vertex.TexCoord);
    }
};

template <> struct std::hash<Onyx::StaticVertex<Onyx::D3>>
{
    std::size_t operator()(const Onyx::StaticVertex<Onyx::D3> &vertex) const
    {
        return TKit::Hash(vertex.Position, vertex.TexCoord, vertex.Normal, vertex.Tangent);
    }
};
