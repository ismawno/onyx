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
ONYX_DIMENSION_TEMPLATE struct ONYX_API Vertex
{
    static KIT::StaticArray<VkVertexInputBindingDescription, 1> BindingDescriptions() noexcept;
    static KIT::StaticArray<VkVertexInputAttributeDescription, 1> AttributeDescriptions() noexcept;

    vec<N> Position;
};

using Vertex2D = Vertex<2>;
using Vertex3D = Vertex<3>;

} // namespace ONYX