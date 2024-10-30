#include "core/pch.hpp"
#include "onyx/draw/vertex.hpp"

namespace ONYX
{
template <u32 N>
    requires(IsDim<N>())
static const std::array<VkVertexInputBindingDescription, Vertex<N>::BINDINGS> &bindingDescriptions() noexcept
{
    static std::array<VkVertexInputBindingDescription, Vertex<N>::BINDINGS> bindingDescriptions{};
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(Vertex<N>);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescriptions[0] = description;

    return bindingDescriptions;
}

template <u32 N>
    requires(IsDim<N>())
static const std::array<VkVertexInputAttributeDescription, Vertex<N>::ATTRIBUTES> &attributeDescriptions() noexcept
{
    static std::array<VkVertexInputAttributeDescription, Vertex<N>::ATTRIBUTES> attributeDescriptions{};

    VkVertexInputAttributeDescription position{};
    position.binding = 0;
    position.location = 0;
    position.offset = offsetof(Vertex<N>, Position);

    if constexpr (N == 2)
        position.format = VK_FORMAT_R32G32_SFLOAT;
    else
    {
        position.format = VK_FORMAT_R32G32B32_SFLOAT;
        VkVertexInputAttributeDescription normal{};
        normal.binding = 0;
        normal.location = 1;
        normal.offset = offsetof(Vertex<N>, Normal);
        normal.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1] = normal;
    }

    attributeDescriptions[0] = position;
    return attributeDescriptions;
}

const std::array<VkVertexInputBindingDescription, Vertex2D::BINDINGS> &Vertex<2>::GetBindingDescriptions() noexcept
{
    return bindingDescriptions<2>();
}
const std::array<VkVertexInputAttributeDescription, Vertex2D::ATTRIBUTES> &Vertex<
    2>::GetAttributeDescriptions() noexcept
{
    return attributeDescriptions<2>();
}

const std::array<VkVertexInputBindingDescription, Vertex3D::BINDINGS> &Vertex<3>::GetBindingDescriptions() noexcept
{
    return bindingDescriptions<3>();
}
const std::array<VkVertexInputAttributeDescription, Vertex3D::ATTRIBUTES> &Vertex<
    3>::GetAttributeDescriptions() noexcept
{
    return attributeDescriptions<3>();
}
} // namespace ONYX