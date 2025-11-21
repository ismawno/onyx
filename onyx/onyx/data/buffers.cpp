#include "onyx/core/pch.hpp"
#include "onyx/property/vertex.hpp"
#include "onyx/data/buffers.hpp"
#include <tiny_obj_loader.h>

namespace Onyx
{
namespace Detail
{
void RecordCopy(const VkCommandBuffer p_CommandBuffer, const VkBuffer p_DeviceLocalBuffer,
                const VkBuffer p_DeviceStagingBuffer, const u32 p_Size)
{
    VkBufferCopy copy{};
    copy.dstOffset = 0;
    copy.srcOffset = 0;
    copy.size = p_Size;

    Core::GetDeviceTable().CmdCopyBuffer(p_CommandBuffer, p_DeviceStagingBuffer, p_DeviceLocalBuffer, 1, &copy);
}

VkBufferMemoryBarrier CreateAcquireBarrier(const VkBuffer p_DeviceLocalBuffer, const u32 p_Size,
                                           const VkAccessFlags p_DstFlags)
{
    const u32 qsrc = Core::GetTransferIndex();
    const u32 qdst = Core::GetGraphicsIndex();

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = qsrc != qdst ? 0 : VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = p_DstFlags;
    barrier.srcQueueFamilyIndex = qsrc != qdst ? qsrc : VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = qsrc != qdst ? qdst : VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = p_DeviceLocalBuffer;
    barrier.offset = 0;
    barrier.size = p_Size;

    return barrier;
}
VkBufferMemoryBarrier CreateReleaseBarrier(const VkBuffer p_DeviceLocalBuffer, const u32 p_Size)
{
    const u32 qsrc = Core::GetTransferIndex();
    const u32 qdst = Core::GetGraphicsIndex();
    TKIT_ASSERT(qsrc != qdst,
                "[ONYX] Cannot create a release barrier if the graphics and transfer queues belong to the same family");

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    barrier.srcQueueFamilyIndex = qsrc;
    barrier.dstQueueFamilyIndex = qdst;
    barrier.buffer = p_DeviceLocalBuffer;
    barrier.offset = 0;
    barrier.size = p_Size;

    return barrier;
}

void ApplyAcquireBarrier(const VkCommandBuffer p_CommandBuffer,
                         const TKit::Span<const VkBufferMemoryBarrier> p_Barriers,
                         const VkPipelineStageFlags p_DstFlags)
{
    const TransferMode mode = Core::GetTransferMode();
    const VkPipelineStageFlags src =
        mode == TransferMode::Separate ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;

    Core::GetDeviceTable().CmdPipelineBarrier(p_CommandBuffer, src, p_DstFlags, 0, 0, nullptr, p_Barriers.GetSize(),
                                              p_Barriers.GetData(), 0, nullptr);
}

void ApplyReleaseBarrier(const VkCommandBuffer p_CommandBuffer,
                         const TKit::Span<const VkBufferMemoryBarrier> p_Barriers)
{
    TKIT_ASSERT(
        Core::GetTransferMode() == TransferMode::Separate,
        "[ONYX] Can only apply release barrier if the graphics and transfer queues dont belong to the same family");

    Core::GetDeviceTable().CmdPipelineBarrier(p_CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, p_Barriers.GetSize(),
                                              p_Barriers.GetData(), 0, nullptr);
}
} // namespace Detail
template <Dimension D>
VKit::FormattedResult<IndexVertexHostData<D>> Load(const std::string_view p_Path, const f32m<D> *p_Transform)
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
        const f32m3 normalMatrix = Math::Transpose(Math::Inverse(f32m3(*p_Transform)));
        for (Vertex<D> &vertex : buffers.Vertices)
        {
            vertex.Position = f32v3{*p_Transform * f32v4{vertex.Position, 1.f}};
            vertex.Normal = normalMatrix * vertex.Normal;
        }
    }
    else
        for (Vertex<D> &vertex : buffers.Vertices)
            if constexpr (D == D3)
                vertex.Position = f32v2{*p_Transform * f32v3{vertex.Position, 1.f}};
    return VKit::FormattedResult<IndexVertexHostData<D>>::Ok(buffers);
}

template <Dimension D>
DeviceLocalVertexBuffer<D> CreateDeviceLocalVertexBuffer(const HostVertexBuffer<D> &p_Vertices)
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Vertices;
    specs.CommandPool = &Core::GetTransferPool();
    specs.Queue = Core::GetTransferQueue();
    const auto result = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
DeviceLocalIndexBuffer CreateDeviceLocalIndexBuffer(const HostIndexBuffer &p_Indices)
{
    typename VKit::DeviceLocalBuffer<Index>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = p_Indices;
    specs.CommandPool = &Core::GetTransferPool();
    specs.Queue = Core::GetTransferQueue();
    const auto result = VKit::DeviceLocalBuffer<Index>::CreateIndexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
template <Dimension D> DeviceLocalVertexBuffer<D> CreateDeviceLocalVertexBuffer(const u32 p_Capacity)
{
    typename VKit::DeviceLocalBuffer<Vertex<D>>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = TKit::Span<const Vertex<D>>{nullptr, p_Capacity};
    specs.CommandPool = &Core::GetTransferPool();
    specs.Queue = Core::GetTransferQueue();
    const auto result = VKit::DeviceLocalBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
DeviceLocalIndexBuffer CreateDeviceLocalIndexBuffer(const u32 p_Capacity)
{
    typename VKit::DeviceLocalBuffer<Index>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Data = TKit::Span<const Index>{nullptr, p_Capacity};
    specs.CommandPool = &Core::GetTransferPool();
    specs.Queue = Core::GetTransferQueue();
    const auto result = VKit::DeviceLocalBuffer<Index>::CreateIndexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template <Dimension D> HostVisibleVertexBuffer<D> CreateHostVisibleVertexBuffer(const u32 p_Capacity)
{
    typename VKit::HostVisibleBuffer<Vertex<D>>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = static_cast<VkDeviceSize>(p_Capacity);
    specs.AllocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    specs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto result = VKit::HostVisibleBuffer<Vertex<D>>::CreateVertexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
HostVisibleIndexBuffer CreateHostVisibleIndexBuffer(const u32 p_Capacity)
{
    typename VKit::HostVisibleBuffer<Index>::Specs specs{};
    specs.Allocator = Core::GetVulkanAllocator();
    specs.Capacity = static_cast<VkDeviceSize>(p_Capacity);
    specs.AllocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    specs.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto result = VKit::HostVisibleBuffer<Index>::CreateIndexBuffer(Core::GetDevice(), specs);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

template ONYX_API VKit::FormattedResult<IndexVertexHostData<D2>> Load(const std::string_view p_Path,
                                                                      const f32m<D2> *p_Transform);
template ONYX_API VKit::FormattedResult<IndexVertexHostData<D3>> Load(const std::string_view p_Path,
                                                                      const f32m<D3> *p_Transform);

template struct ONYX_API IndexVertexHostData<D2>;
template struct ONYX_API IndexVertexHostData<D3>;

template ONYX_API DeviceLocalVertexBuffer<D2> CreateDeviceLocalVertexBuffer<D2>(const HostVertexBuffer<D2> &);
template ONYX_API DeviceLocalVertexBuffer<D3> CreateDeviceLocalVertexBuffer<D3>(const HostVertexBuffer<D3> &);

template ONYX_API DeviceLocalVertexBuffer<D2> CreateDeviceLocalVertexBuffer<D2>(u32);
template ONYX_API DeviceLocalVertexBuffer<D3> CreateDeviceLocalVertexBuffer<D3>(u32);

template ONYX_API HostVisibleVertexBuffer<D2> CreateHostVisibleVertexBuffer<D2>(u32);
template ONYX_API HostVisibleVertexBuffer<D3> CreateHostVisibleVertexBuffer<D3>(u32);

} // namespace Onyx
