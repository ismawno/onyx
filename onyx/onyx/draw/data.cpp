#include "onyx/core/pch.hpp"
#include "onyx/draw/data.hpp"
#include <tiny_obj_loader.h>

namespace Onyx
{
template <Dimension D> VKit::FormattedResult<IndexVertexData<D>> Load(const std::string_view p_Path) noexcept
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, p_Path.data()))
        return VKit::FormattedResult<IndexVertexData<D>>::Error(
            VKIT_FORMAT_ERROR(VK_ERROR_INITIALIZATION_FAILED, "Failed to load model: {}", err + warn));

    TKit::HashMap<Vertex<D>, Index> uniqueVertices;
    IndexVertexData<D> buffers;

    buffers.Vertices.reserve(static_cast<u32>(attrib.vertices.size() / 3));
    buffers.Indices.reserve(static_cast<u32>(shapes.size() * 3));

    for (const auto &shape : shapes)
        for (const auto &index : shape.mesh.indices)
        {
            Vertex<D> vertex{};
            for (Index i = 0; i < D; ++i)
                vertex.Position[i] = attrib.vertices[3 * index.vertex_index + i];
            if constexpr (D == D3)
                for (Index i = 0; i < 3; ++i)
                    vertex.Normal[i] = attrib.normals[3 * index.normal_index + i];

            if (!uniqueVertices.contains(vertex))
            {
                uniqueVertices[vertex] = static_cast<Index>(uniqueVertices.size());
                buffers.Vertices.push_back(vertex);
            }
            buffers.Indices.push_back(uniqueVertices[vertex]);
        }
    return VKit::FormattedResult<IndexVertexData<D>>::Ok(buffers);
}

template <Dimension D>
DeviceLocalVertexBuffer<D> CreateDeviceLocalVertexBuffer(const TKit::Span<const Vertex<D>> p_Vertices) noexcept
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::VertexSpecs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Vertices;
    specs.CommandPool = &Core::GetCommandPool();
    specs.Queue = Core::GetGraphicsQueue();
    const auto result = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
DeviceLocalIndexBuffer CreateDeviceLocalIndexBuffer(const TKit::Span<const Index> p_Indices) noexcept
{
    typename VKit::DeviceLocalBuffer<Index>::IndexSpecs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Indices;
    specs.CommandPool = &Core::GetCommandPool();
    specs.Queue = Core::GetGraphicsQueue();
    const auto result = VKit::DeviceLocalBuffer<Index>::CreateIndexBuffer(specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> HostVisibleVertexBuffer<D> CreateHostVisibleVertexBuffer(const VkDeviceSize p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<Vertex<D>>::VertexSpecs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = p_Capacity;
    const auto result = VKit::HostVisibleBuffer<Vertex<D>>::CreateVertexBuffer(specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
HostVisibleIndexBuffer CreateHostVisibleIndexBuffer(const VkDeviceSize p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<Index>::IndexSpecs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = p_Capacity;
    const auto result = VKit::HostVisibleBuffer<Index>::CreateIndexBuffer(specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template ONYX_API VKit::FormattedResult<IndexVertexData<D2>> Load(const std::string_view p_Path) noexcept;
template ONYX_API VKit::FormattedResult<IndexVertexData<D3>> Load(const std::string_view p_Path) noexcept;

template struct ONYX_API IndexVertexData<D2>;
template struct ONYX_API IndexVertexData<D3>;

template ONYX_API DeviceLocalVertexBuffer<D2> CreateDeviceLocalVertexBuffer<D2>(TKit::Span<const Vertex<D2>>) noexcept;
template ONYX_API DeviceLocalVertexBuffer<D3> CreateDeviceLocalVertexBuffer<D3>(TKit::Span<const Vertex<D3>>) noexcept;

template ONYX_API HostVisibleVertexBuffer<D2> CreateHostVisibleVertexBuffer<D2>(VkDeviceSize) noexcept;
template ONYX_API HostVisibleVertexBuffer<D3> CreateHostVisibleVertexBuffer<D3>(VkDeviceSize) noexcept;

} // namespace Onyx