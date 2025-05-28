#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/core/api.hpp"
#include "tkit/utils/hash.hpp"

#include <vulkan/vulkan.h>

namespace Onyx
{

// Vertices have no color because they limit my ability to re use a model. I plan to have a single color per rendered
// object, so I dont need to store it in the vertex
template <Dimension D> struct Vertex;

template <> struct ONYX_API Vertex<D2>
{
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 1;
    static const TKit::Array<VkVertexInputBindingDescription, BINDINGS> &GetBindingDescriptions() noexcept;
    static const TKit::Array<VkVertexInputAttributeDescription, ATTRIBUTES> &GetAttributeDescriptions() noexcept;

    fvec2 Position;

    friend bool operator==(const Vertex<D2> &p_Left, const Vertex<D2> &p_Right) noexcept
    {
        return p_Left.Position == p_Right.Position;
    }
};

template <> struct ONYX_API Vertex<D3>
{
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 2;
    static const TKit::Array<VkVertexInputBindingDescription, BINDINGS> &GetBindingDescriptions() noexcept;
    static const TKit::Array<VkVertexInputAttributeDescription, ATTRIBUTES> &GetAttributeDescriptions() noexcept;

    fvec3 Position;
    fvec3 Normal;

    friend bool operator==(const Vertex<D3> &p_Left, const Vertex<D3> &p_Right) noexcept
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
