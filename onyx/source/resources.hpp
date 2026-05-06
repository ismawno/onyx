#pragma once

#include "onyx/resource/resources.hpp"
#include "vkit/resource/device_image.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "vkit/resource/sampler.hpp"

namespace Onyx::Resources
{
void Initialize(const Specs &specs);
void Terminate();

const VKit::DeviceBuffer &GetBuffer(Resource buffer);
const VKit::Sampler &GetSampler(Resource sampler);
const VKit::DeviceImage &GetImage(const Resource image);

struct MeshBuffers
{
    const VKit::DeviceBuffer *VertexBuffer = nullptr;
    const VKit::DeviceBuffer *IndexBuffer = nullptr;
};

template <Dimension D> MeshBuffers GetMeshBuffers(ResourcePool pool);
MeshBuffers GetFontBuffers(ResourcePool pool);
MeshBuffers GetGlyphBuffers(ResourcePool pool);

} // namespace Onyx::Resources
