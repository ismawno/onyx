#include "onyx/core/pch.hpp"
#include "onyx/property/vertex.hpp"

namespace Onyx
{
template <Dimension D>
static const TKit::Array<VkVertexInputBindingDescription, Vertex<D>::Bindings> &bindingDescriptions()
{
    static TKit::Array<VkVertexInputBindingDescription, Vertex<D>::Bindings> bindingDescriptions{};
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(Vertex<D>);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescriptions[0] = description;

    return bindingDescriptions;
}

template <Dimension D>
static const TKit::Array<VkVertexInputAttributeDescription, Vertex<D>::Attributes> &attributeDescriptions()
{
    static TKit::Array<VkVertexInputAttributeDescription, Vertex<D>::Attributes> attributeDescriptions{};

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

const TKit::Array<VkVertexInputBindingDescription, Vertex<D2>::Bindings> &Vertex<D2>::GetBindingDescriptions()
{
    return bindingDescriptions<D2>();
}
const TKit::Array<VkVertexInputAttributeDescription, Vertex<D2>::Attributes> &Vertex<D2>::GetAttributeDescriptions()
{
    return attributeDescriptions<D2>();
}

const TKit::Array<VkVertexInputBindingDescription, Vertex<D3>::Bindings> &Vertex<D3>::GetBindingDescriptions()
{
    return bindingDescriptions<D3>();
}
const TKit::Array<VkVertexInputAttributeDescription, Vertex<D3>::Attributes> &Vertex<D3>::GetAttributeDescriptions()
{
    return attributeDescriptions<D3>();
}
} // namespace Onyx
