#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/core/api.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/serialization/yaml/serialize.hpp"
#include "tkit/utils/hash.hpp"
#include "tkit/container/array.hpp"

#include <vulkan/vulkan.h>

namespace Onyx
{

// Vertices have no color because they limit my ability to re use a mesh. I plan to have a single color per rendered
// object, so I dont need to store it in the vertex
template <Dimension D> struct Vertex;

template <> struct ONYX_API Vertex<D2>
{
    TKIT_REFLECT_DECLARE(Vertex)
    TKIT_YAML_SERIALIZE_DECLARE(Vertex)

    TKIT_REFLECT_IGNORE_BEGIN()
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 1;
    TKIT_REFLECT_IGNORE_END()
    TKIT_YAML_SERIALIZE_IGNORE_END()

    static const TKit::Array<VkVertexInputBindingDescription, BINDINGS> &GetBindingDescriptions();
    static const TKit::Array<VkVertexInputAttributeDescription, ATTRIBUTES> &GetAttributeDescriptions();

    fvec2 Position;

    friend bool operator==(const Vertex<D2> &p_Left, const Vertex<D2> &p_Right)
    {
        return p_Left.Position == p_Right.Position;
    }
};

template <> struct ONYX_API Vertex<D3>
{
    TKIT_REFLECT_DECLARE(Vertex)
    TKIT_YAML_SERIALIZE_DECLARE(Vertex)

    TKIT_REFLECT_IGNORE_BEGIN()
    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 2;
    TKIT_REFLECT_IGNORE_END()
    TKIT_YAML_SERIALIZE_IGNORE_END()

    static const TKit::Array<VkVertexInputBindingDescription, BINDINGS> &GetBindingDescriptions();
    static const TKit::Array<VkVertexInputAttributeDescription, ATTRIBUTES> &GetAttributeDescriptions();

    fvec3 Position;
    fvec3 Normal;

    friend bool operator==(const Vertex<D3> &p_Left, const Vertex<D3> &p_Right)
    {
        return p_Left.Position == p_Right.Position && p_Left.Normal == p_Right.Normal;
    }
};

} // namespace Onyx

template <> struct ONYX_API std::hash<Onyx::Vertex<Onyx::D2>>
{
    std::size_t operator()(const Onyx::Vertex<Onyx::D2> &p_Vertex) const
    {
        return TKit::Hash(p_Vertex.Position);
    }
};

template <> struct ONYX_API std::hash<Onyx::Vertex<Onyx::D3>>
{
    std::size_t operator()(const Onyx::Vertex<Onyx::D3> &p_Vertex) const
    {
        return TKit::Hash(p_Vertex.Position, p_Vertex.Normal);
    }
};
