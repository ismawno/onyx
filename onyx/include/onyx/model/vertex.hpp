#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/model/color.hpp"
#include "kit/container/static_array.hpp"

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
    static std::array<VkVertexInputBindingDescription, BINDINGS> BindingDescriptions() noexcept;
    static std::array<VkVertexInputAttributeDescription, ATTRIBUTES> AttributeDescriptions() noexcept;

    vec2 Position;
};

template <> struct ONYX_API Vertex<3>
{
    static constexpr u32 BINDINGS = 1;
    static constexpr u32 ATTRIBUTES = 1;
    static std::array<VkVertexInputBindingDescription, BINDINGS> BindingDescriptions() noexcept;
    static std::array<VkVertexInputAttributeDescription, ATTRIBUTES> AttributeDescriptions() noexcept;

    vec3 Position;
};

using Vertex2D = Vertex<2>;
using Vertex3D = Vertex<3>;

} // namespace ONYX