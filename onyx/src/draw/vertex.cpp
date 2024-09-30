#include "core/pch.hpp"
#include "onyx/draw/vertex.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE static std::array<VkVertexInputBindingDescription, Vertex<N>::BINDINGS>
bindingDescriptions() noexcept
{
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(Vertex<N>);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return {description};
}

ONYX_DIMENSION_TEMPLATE static std::array<VkVertexInputAttributeDescription, Vertex<N>::ATTRIBUTES>
attributeDescriptions() noexcept
{
    VkVertexInputAttributeDescription position{};
    position.binding = 0;
    position.location = 0;
    position.offset = offsetof(Vertex<N>, Position);

    if constexpr (N == 2)
    {
        position.format = VK_FORMAT_R32G32_SFLOAT;
        return {position};
    }
    else
    {
        position.format = VK_FORMAT_R32G32B32_SFLOAT;
        VkVertexInputAttributeDescription normal{};
        normal.binding = 0;
        normal.location = 1;
        normal.offset = offsetof(Vertex<N>, Normal);
        normal.format = VK_FORMAT_R32G32B32_SFLOAT;
        return {position, normal};
    }
}

std::array<VkVertexInputBindingDescription, Vertex2D::BINDINGS> Vertex<2>::GetBindingDescriptions() noexcept
{
    return bindingDescriptions<2>();
}
std::array<VkVertexInputAttributeDescription, Vertex2D::ATTRIBUTES> Vertex<2>::GetAttributeDescriptions() noexcept
{
    return attributeDescriptions<2>();
}

std::array<VkVertexInputBindingDescription, Vertex3D::BINDINGS> Vertex<3>::GetBindingDescriptions() noexcept
{
    return bindingDescriptions<3>();
}
std::array<VkVertexInputAttributeDescription, Vertex3D::ATTRIBUTES> Vertex<3>::GetAttributeDescriptions() noexcept
{
    return attributeDescriptions<3>();
}
} // namespace ONYX