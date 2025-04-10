#pragma once

#include "onyx/draw/vertex.hpp"
#include "vkit/buffer/device_local_buffer.hpp"
#include "vkit/buffer/host_visible_buffer.hpp"

#ifndef ONYX_INDEX_TYPE
#    define ONYX_INDEX_TYPE TKit::Alias::u32
#endif

namespace Onyx
{
using Index = ONYX_INDEX_TYPE;

template <Dimension D> struct IndexVertexData
{
    TKit::DynamicArray<Vertex<D>> Vertices;
    TKit::DynamicArray<Index> Indices;
};

template <Dimension D> VKit::FormattedResult<IndexVertexData<D>> Load(std::string_view p_Path) noexcept;

template <Dimension D> using VertexBuffer = VKit::DeviceLocalBuffer<Vertex<D>>;
using IndexBuffer = VKit::DeviceLocalBuffer<Index>;
template <typename T> using StorageBuffer = VKit::DeviceLocalBuffer<T>;

template <Dimension D> using MutableVertexBuffer = VKit::HostVisibleBuffer<Vertex<D>>;
using MutableIndexBuffer = VKit::HostVisibleBuffer<Index>;
template <typename T> using MutableStorageBuffer = VKit::HostVisibleBuffer<T>;

} // namespace Onyx