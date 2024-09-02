#pragma once

#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/drawing/color.hpp"
#include "kit/container/static_array.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct Vertex
{
    Vertex() noexcept = default;
    Vertex(const vec<N> &p_Position, const Color &p_Color) noexcept;

    static KIT::StaticArray<VkVertexInputBindingDescription, 1> BindingDescriptions() noexcept;
    static KIT::StaticArray<VkVertexInputAttributeDescription, 2> AttributeDescriptions() noexcept;

    vec<N> Position;
    ONYX::Color Color;
};
} // namespace ONYX