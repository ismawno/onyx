#include "core/pch.hpp"
#include "onyx/model/vertex.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE KIT::StaticArray<VkVertexInputBindingDescription, 1> Vertex<N>::BindingDescriptions() noexcept
{
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(Vertex);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return {description};
}

ONYX_DIMENSION_TEMPLATE KIT::StaticArray<VkVertexInputAttributeDescription, 1> Vertex<
    N>::AttributeDescriptions() noexcept
{
    VkVertexInputAttributeDescription position{};
    position.binding = 0;
    position.location = 0;

    if constexpr (N == 2)
        position.format = VK_FORMAT_R32G32_SFLOAT;
    else
        position.format = VK_FORMAT_R32G32B32_SFLOAT;

    position.offset = offsetof(Vertex, Position);
    return {position};
}

template struct Vertex<2>;
template struct Vertex<3>;
} // namespace ONYX