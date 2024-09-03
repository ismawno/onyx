#include "core/pch.hpp"
#include "onyx/model/vertex.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Vertex<N>::Vertex(const vec<N> &p_Position, const ONYX::Color &p_Color) noexcept
    : Position(p_Position), Color(p_Color)
{
}

ONYX_DIMENSION_TEMPLATE KIT::StaticArray<VkVertexInputBindingDescription, 1> Vertex<N>::BindingDescriptions() noexcept
{
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(Vertex);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return {description};
}

ONYX_DIMENSION_TEMPLATE KIT::StaticArray<VkVertexInputAttributeDescription, 2> Vertex<
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

    VkVertexInputAttributeDescription color{};
    color.binding = 0;
    color.location = 1;
    color.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    color.offset = offsetof(Vertex, Color);

    return {position, color};
}

template struct Vertex<2>;
template struct Vertex<3>;
} // namespace ONYX