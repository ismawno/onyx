#include "onyx/core/pch.hpp"
#include "onyx/draw/vertex.hpp"

namespace Onyx
{
template <Dimension D>
static const TKit::Array<VkVertexInputBindingDescription, Vertex<D>::BINDINGS> &bindingDescriptions() noexcept
{
    static TKit::Array<VkVertexInputBindingDescription, Vertex<D>::BINDINGS> bindingDescriptions{};
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(Vertex<D>);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescriptions[0] = description;

    return bindingDescriptions;
}

template <Dimension D>
static const TKit::Array<VkVertexInputAttributeDescription, Vertex<D>::ATTRIBUTES> &attributeDescriptions() noexcept
{
    static TKit::Array<VkVertexInputAttributeDescription, Vertex<D>::ATTRIBUTES> attributeDescriptions{};

    VkVertexInputAttributeDescription position{};
    position.binding = 0;
    position.location = 0;
    position.offset = offsetof(Vertex<D>, Position);

    if constexpr (D == D2)
        position.format = VK_FORMAT_R32G32_SFLOAT;
    else
    {
        position.format = VK_FORMAT_R32G32B32_SFLOAT;
        VkVertexInputAttributeDescription normal{};
        normal.binding = 0;
        normal.location = 1;
        normal.offset = offsetof(Vertex<D>, Normal);
        normal.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1] = normal;
    }

    attributeDescriptions[0] = position;
    return attributeDescriptions;
}

const TKit::Array<VkVertexInputBindingDescription, Vertex<D2>::BINDINGS> &Vertex<D2>::GetBindingDescriptions() noexcept
{
    return bindingDescriptions<D2>();
}
const TKit::Array<VkVertexInputAttributeDescription, Vertex<D2>::ATTRIBUTES> &Vertex<
    D2>::GetAttributeDescriptions() noexcept
{
    return attributeDescriptions<D2>();
}

const TKit::Array<VkVertexInputBindingDescription, Vertex<D3>::BINDINGS> &Vertex<D3>::GetBindingDescriptions() noexcept
{
    return bindingDescriptions<D3>();
}
const TKit::Array<VkVertexInputAttributeDescription, Vertex<D3>::ATTRIBUTES> &Vertex<
    D3>::GetAttributeDescriptions() noexcept
{
    return attributeDescriptions<D3>();
}
} // namespace Onyx