#pragma once

#include "onyx/resources.hpp"
#include "vkit/resource/device_buffer.hpp"

namespace Onyx::Resources
{
void Initialize(const Specs &specs);
void Terminate();

struct MeshBuffers
{
    const VKit::DeviceBuffer *VertexBuffer = nullptr;
    const VKit::DeviceBuffer *IndexBuffer = nullptr;
};

template <Dimension D> MeshBuffers GetMeshBuffers(ResourcePool pool);
MeshBuffers GetFontBuffers(ResourcePool pool);
MeshBuffers GetGlyphBuffers(ResourcePool pool);

} // namespace Onyx::Resources
