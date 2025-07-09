#include "onyx/core/pch.hpp"
#include "onyx/property/vertex.hpp"
#include "onyx/data/buffers.hpp"
#include <tiny_obj_loader.h>

namespace Onyx
{
template <Dimension D>
VKit::FormattedResult<IndexVertexHostData<D>> Load(const std::string_view p_Path, const fmat<D> *p_Transform) noexcept
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, nullptr, &warn, &err, p_Path.data()))
        return VKit::FormattedResult<IndexVertexHostData<D>>::Error(
            VKIT_FORMAT_ERROR(VK_ERROR_INITIALIZATION_FAILED, "Failed to load mesh: {}", err + warn));

    TKit::HashMap<Vertex<D>, Index> uniqueVertices;
    IndexVertexHostData<D> buffers;

    const u32 vcount = static_cast<u32>(attrib.vertices.size());

    buffers.Vertices.Reserve(vcount);
    buffers.Indices.Reserve(vcount);
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
                buffers.Vertices.Append(vertex);
            }
            buffers.Indices.Append(uniqueVertices[vertex]);
        }
    if (!p_Transform)
        return VKit::FormattedResult<IndexVertexHostData<D>>::Ok(buffers);

    if constexpr (D == D3)
    {
        const fmat3 normalMatrix = glm::transpose(glm::inverse(fmat3(*p_Transform)));
        for (Vertex<D> &vertex : buffers.Vertices)
        {
            vertex.Position = fvec3{*p_Transform * fvec4{vertex.Position, 1.f}};
            vertex.Normal = normalMatrix * vertex.Normal;
        }
    }
    else
        for (Vertex<D> &vertex : buffers.Vertices)
            if constexpr (D == D3)
                vertex.Position = fvec2{*p_Transform * fvec3{vertex.Position, 1.f}};
    return VKit::FormattedResult<IndexVertexHostData<D>>::Ok(buffers);
}

template <Dimension D>
DeviceLocalVertexBuffer<D> CreateDeviceLocalVertexBuffer(const HostVertexBuffer<D> &p_Vertices) noexcept
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Vertices;
    specs.CommandPool = &Core::GetCommandPool();
    specs.Queue = Core::GetGraphicsQueue();
    const auto result = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
DeviceLocalIndexBuffer CreateDeviceLocalIndexBuffer(const HostIndexBuffer &p_Indices) noexcept
{
    typename VKit::DeviceLocalBuffer<Index>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Indices;
    specs.CommandPool = &Core::GetCommandPool();
    specs.Queue = Core::GetGraphicsQueue();
    const auto result = VKit::DeviceLocalBuffer<Index>::CreateIndexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> HostVisibleVertexBuffer<D> CreateHostVisibleVertexBuffer(const VkDeviceSize p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<Vertex<D>>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = p_Capacity;
    specs.AllocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    const auto result = VKit::HostVisibleBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
HostVisibleIndexBuffer CreateHostVisibleIndexBuffer(const VkDeviceSize p_Capacity) noexcept
{
    typename VKit::HostVisibleBuffer<Index>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = p_Capacity;
    specs.AllocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    const auto result = VKit::HostVisibleBuffer<Index>::CreateIndexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template ONYX_API VKit::FormattedResult<IndexVertexHostData<D2>> Load(const std::string_view p_Path,
                                                                      const fmat<D2> *p_Transform) noexcept;
template ONYX_API VKit::FormattedResult<IndexVertexHostData<D3>> Load(const std::string_view p_Path,
                                                                      const fmat<D3> *p_Transform) noexcept;

template struct ONYX_API IndexVertexHostData<D2>;
template struct ONYX_API IndexVertexHostData<D3>;

template ONYX_API DeviceLocalVertexBuffer<D2> CreateDeviceLocalVertexBuffer<D2>(const HostVertexBuffer<D2> &) noexcept;
template ONYX_API DeviceLocalVertexBuffer<D3> CreateDeviceLocalVertexBuffer<D3>(const HostVertexBuffer<D3> &) noexcept;

template ONYX_API HostVisibleVertexBuffer<D2> CreateHostVisibleVertexBuffer<D2>(VkDeviceSize) noexcept;
template ONYX_API HostVisibleVertexBuffer<D3> CreateHostVisibleVertexBuffer<D3>(VkDeviceSize) noexcept;

} // namespace Onyx
