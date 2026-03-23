#pragma once

#include "onyx/core/math.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/property/instance.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/serialization/yaml/serialize.hpp"
#include "tkit/utils/hash.hpp"
#include "tkit/math/hash.hpp"

#include <vulkan/vulkan.h>

namespace Onyx
{
template <Dimension D> struct StatVertex;

template <> struct StatVertex<D2>
{
    TKIT_REFLECT_DECLARE(StatVertex)
    TKIT_YAML_SERIALIZE_DECLARE(StatVertex)

    TKIT_REFLECT_IGNORE_BEGIN()
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    static constexpr Geometry Geo = Geometry_Static;
    static constexpr Dimension Dim = D2;
    TKIT_YAML_SERIALIZE_IGNORE_END()
    TKIT_REFLECT_IGNORE_END()

    f32v2 Position;
    f32v2 TexCoord;

    friend bool operator==(const StatVertex<D2> &left, const StatVertex<D2> &right)
    {
        return left.Position == right.Position && left.TexCoord == right.TexCoord;
    }
};

template <> struct StatVertex<D3>
{
    TKIT_REFLECT_DECLARE(StatVertex)
    TKIT_YAML_SERIALIZE_DECLARE(StatVertex)

    TKIT_REFLECT_IGNORE_BEGIN()
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    static constexpr Geometry Geo = Geometry_Static;
    static constexpr Dimension Dim = D3;
    TKIT_YAML_SERIALIZE_IGNORE_END()
    TKIT_REFLECT_IGNORE_END()

    f32v3 Position;
    f32v2 TexCoord;
    f32v3 Normal;
    f32v4 Tangent;

    friend bool operator==(const StatVertex<D3> &left, const StatVertex<D3> &right)
    {
        return left.Position == right.Position && left.TexCoord == right.TexCoord && left.Normal == right.Normal &&
               left.Tangent == right.Tangent;
    }
};

using ParametricRegionFlags = u32;
enum StadiumRegion : ParametricRegionFlags
{
    StadiumRegion_Body = 1 << 0,
    StadiumRegion_Edge = 1 << 1,
    StadiumRegion_Moon = 1 << 2,
};

enum RoundedQuadRegion : ParametricRegionFlags
{
    RoundedQuadRegion_Body = 1 << 0,
    RoundedQuadRegion_HorizontalEdge = 1 << 1,
    RoundedQuadRegion_VerticalEdge = 1 << 2,
    RoundedQuadRegion_Moon = 1 << 3,
};

enum CapsuleRegion : ParametricRegionFlags
{
    CapsuleRegion_Body = 1 << 0,
    CapsuleRegion_Cap = 1 << 1,
};

template <Dimension D> struct ParaVertex;
template <> struct ParaVertex<D2>
{
    TKIT_REFLECT_DECLARE(ParaVertex)
    TKIT_YAML_SERIALIZE_DECLARE(ParaVertex)

    TKIT_REFLECT_IGNORE_BEGIN()
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    static constexpr Geometry Geo = Geometry_Parametric;
    static constexpr Dimension Dim = D2;
    TKIT_YAML_SERIALIZE_IGNORE_END()
    TKIT_REFLECT_IGNORE_END()

    f32v2 Position;
    f32v2 TexCoord;
    ParametricRegionFlags Region;
};

template <> struct ParaVertex<D3>
{
    TKIT_REFLECT_DECLARE(ParaVertex)
    TKIT_YAML_SERIALIZE_DECLARE(ParaVertex)

    TKIT_REFLECT_IGNORE_BEGIN()
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    static constexpr Geometry Geo = Geometry_Parametric;
    static constexpr Dimension Dim = D3;
    TKIT_YAML_SERIALIZE_IGNORE_END()
    TKIT_REFLECT_IGNORE_END()

    f32v3 Position;
    f32v2 TexCoord;
    f32v3 Normal;
    f32v4 Tangent;
    ParametricRegionFlags Region;
};

} // namespace Onyx

template <> struct std::hash<Onyx::StatVertex<Onyx::D2>>
{
    std::size_t operator()(const Onyx::StatVertex<Onyx::D2> &vertex) const
    {
        return TKit::Hash(vertex.Position, vertex.TexCoord);
    }
};

template <> struct std::hash<Onyx::StatVertex<Onyx::D3>>
{
    std::size_t operator()(const Onyx::StatVertex<Onyx::D3> &vertex) const
    {
        return TKit::Hash(vertex.Position, vertex.TexCoord, vertex.Normal, vertex.Tangent);
    }
};
