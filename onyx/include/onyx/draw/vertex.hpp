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
ONYX_DIMENSION_TEMPLATE struct Vertex;

template <> struct ONYX_API Vertex<2>
{
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 1;
    static std::array<VkVertexInputBindingDescription, BINDINGS> GetBindingDescriptions() noexcept;
    static std::array<VkVertexInputAttributeDescription, ATTRIBUTES> GetAttributeDescriptions() noexcept;

    vec2 Position;

    friend bool operator==(const Vertex<2> &p_Left, const Vertex<2> &p_Right) noexcept
    {
        return p_Left.Position == p_Right.Position;
    }
};

template <> struct ONYX_API Vertex<3>
{
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 2;
    static std::array<VkVertexInputBindingDescription, BINDINGS> GetBindingDescriptions() noexcept;
    static std::array<VkVertexInputAttributeDescription, ATTRIBUTES> GetAttributeDescriptions() noexcept;

    vec3 Position;
    vec3 Normal;

    friend bool operator==(const Vertex<3> &p_Left, const Vertex<3> &p_Right) noexcept
    {
        return p_Left.Position == p_Right.Position && p_Left.Normal == p_Right.Normal;
    }
};

using Vertex2D = Vertex<2>;
using Vertex3D = Vertex<3>;

} // namespace ONYX

template <> struct std::hash<ONYX::Vertex<2>>
{
    std::size_t operator()(const ONYX::Vertex<2> &p_Vertex) const
    {
        return std::hash<glm::vec2>()(p_Vertex.Position);
    }
};

template <> struct std::hash<ONYX::Vertex<3>>
{
    std::size_t operator()(const ONYX::Vertex<3> &p_Vertex) const
    {
        KIT::HashableTuple<glm::vec3, glm::vec3> tuple{p_Vertex.Position, p_Vertex.Normal};
        return tuple();
    }
};