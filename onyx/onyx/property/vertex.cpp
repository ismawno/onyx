#include "onyx/core/pch.hpp"
#include "onyx/property/vertex.hpp"

namespace Onyx
{
template <Dimension D>
static const TKit::FixedArray<VkVertexInputBindingDescription, StatVertex<D>::Bindings> &bindingDescriptions()
{
    static TKit::FixedArray<VkVertexInputBindingDescription, StatVertex<D>::Bindings> bindingDescriptions{};
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(StatVertex<D>);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescriptions[0] = description;

    return bindingDescriptions;
}

template <Dimension D>
static const TKit::FixedArray<VkVertexInputAttributeDescription, StatVertex<D>::Attributes> &attributeDescriptions()
{
    static TKit::FixedArray<VkVertexInputAttributeDescription, StatVertex<D>::Attributes> attributeDescriptions{};

    VkVertexInputAttributeDescription position{};
    position.binding = 0;
    position.location = 0;
    position.offset = offsetof(StatVertex<D>, Position);

    if constexpr (D == D2)
        position.format = VK_FORMAT_R32G32_SFLOAT;
    else
    {
        position.format = VK_FORMAT_R32G32B32_SFLOAT;
        VkVertexInputAttributeDescription normal{};
        normal.binding = 0;
        normal.location = 1;
        normal.offset = offsetof(StatVertex<D>, Normal);
        normal.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1] = normal;
    }

    attributeDescriptions[0] = position;
    return attributeDescriptions;
}

const TKit::FixedArray<VkVertexInputBindingDescription, StatVertex<D2>::Bindings> &StatVertex<
    D2>::GetBindingDescriptions()
{
    return bindingDescriptions<D2>();
}
const TKit::FixedArray<VkVertexInputAttributeDescription, StatVertex<D2>::Attributes> &StatVertex<
    D2>::GetAttributeDescriptions()
{
    return attributeDescriptions<D2>();
}

const TKit::FixedArray<VkVertexInputBindingDescription, StatVertex<D3>::Bindings> &StatVertex<
    D3>::GetBindingDescriptions()
{
    return bindingDescriptions<D3>();
}
const TKit::FixedArray<VkVertexInputAttributeDescription, StatVertex<D3>::Attributes> &StatVertex<
    D3>::GetAttributeDescriptions()
{
    return attributeDescriptions<D3>();
}
} // namespace Onyx
