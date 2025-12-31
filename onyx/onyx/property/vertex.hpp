#pragma once

#include "onyx/core/math.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/core/api.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/serialization/yaml/serialize.hpp"
#include "tkit/utils/hash.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/math/hash.hpp"

#include <vulkan/vulkan.h>

namespace Onyx
{

// Vertices have no color because they limit my ability to re use a mesh. I plan to have a single color per rendered
// object, so I dont need to store it in the vertex
template <Dimension D> struct StatVertex;

template <> struct ONYX_API StatVertex<D2>
{
    TKIT_REFLECT_DECLARE(StatVertex)
    TKIT_YAML_SERIALIZE_DECLARE(StatVertex)

    TKIT_REFLECT_IGNORE_BEGIN()
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    static constexpr u32 Bindings = 1;
    static constexpr u32 Attributes = 1;
    TKIT_YAML_SERIALIZE_IGNORE_END()
    TKIT_REFLECT_IGNORE_END()

    static const TKit::FixedArray<VkVertexInputBindingDescription, Bindings> &GetBindingDescriptions();
    static const TKit::FixedArray<VkVertexInputAttributeDescription, Attributes> &GetAttributeDescriptions();

    f32v2 Position;

    friend bool operator==(const StatVertex<D2> &p_Left, const StatVertex<D2> &p_Right)
    {
        return p_Left.Position == p_Right.Position;
    }
};

template <> struct ONYX_API StatVertex<D3>
{
    TKIT_REFLECT_DECLARE(StatVertex)
    TKIT_YAML_SERIALIZE_DECLARE(StatVertex)

    TKIT_REFLECT_IGNORE_BEGIN()
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    static constexpr u32 Bindings = 1;
    static constexpr u32 Attributes = 2;
    TKIT_YAML_SERIALIZE_IGNORE_END()
    TKIT_REFLECT_IGNORE_END()

    static const TKit::FixedArray<VkVertexInputBindingDescription, Bindings> &GetBindingDescriptions();
    static const TKit::FixedArray<VkVertexInputAttributeDescription, Attributes> &GetAttributeDescriptions();

    f32v3 Position;
    f32v3 Normal;

    friend bool operator==(const StatVertex<D3> &p_Left, const StatVertex<D3> &p_Right)
    {
        return p_Left.Position == p_Right.Position && p_Left.Normal == p_Right.Normal;
    }
};

} // namespace Onyx

template <> struct ONYX_API std::hash<Onyx::StatVertex<Onyx::D2>>
{
    std::size_t operator()(const Onyx::StatVertex<Onyx::D2> &p_Vertex) const
    {
        return TKit::Hash(p_Vertex.Position);
    }
};

template <> struct ONYX_API std::hash<Onyx::StatVertex<Onyx::D3>>
{
    std::size_t operator()(const Onyx::StatVertex<Onyx::D3> &p_Vertex) const
    {
        return TKit::Hash(p_Vertex.Position, p_Vertex.Normal);
    }
};
