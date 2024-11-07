#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/draw/color.hpp"
#include "kit/container/static_array.hpp"
#include "kit/container/hashable_tuple.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{

// Vertices have no color because they limit my ability to re use a model. I plan to have a single color per rendered
// object, so I dont need to store it in the vertex
template <Dimension D> struct Vertex;

template <> struct ONYX_API Vertex<D2>
{
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 1;
    static const std::array<VkVertexInputBindingDescription, BINDINGS> &GetBindingDescriptions() noexcept;
    static const std::array<VkVertexInputAttributeDescription, ATTRIBUTES> &GetAttributeDescriptions() noexcept;

    vec2 Position;

    friend bool operator==(const Vertex<D2> &p_Left, const Vertex<D2> &p_Right) noexcept
    {
        return p_Left.Position == p_Right.Position;
    }
};

template <> struct ONYX_API Vertex<D3>
{
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 2;
    static const std::array<VkVertexInputBindingDescription, BINDINGS> &GetBindingDescriptions() noexcept;
    static const std::array<VkVertexInputAttributeDescription, ATTRIBUTES> &GetAttributeDescriptions() noexcept;

    vec3 Position;
    vec3 Normal;

    friend bool operator==(const Vertex<D3> &p_Left, const Vertex<D3> &p_Right) noexcept
    {
        return p_Left.Position == p_Right.Position && p_Left.Normal == p_Right.Normal;
    }
};

} // namespace ONYX

template <> struct std::hash<ONYX::Vertex<ONYX::D2>>
{
    std::size_t operator()(const ONYX::Vertex<ONYX::D2> &p_Vertex) const
    {
        return std::hash<glm::vec2>()(p_Vertex.Position);
    }
};

template <> struct std::hash<ONYX::Vertex<ONYX::D3>>
{
    std::size_t operator()(const ONYX::Vertex<ONYX::D3> &p_Vertex) const
    {
        KIT::HashableTuple<glm::vec3, glm::vec3> tuple{p_Vertex.Position, p_Vertex.Normal};
        return tuple();
    }
};