#pragma once

#include "onyx/mesh.hpp"
#include "onyx/material.hpp"
#include "onyx/sampler.hpp"
#include "onyx/image.hpp"
#include "tkit/container/tier_array.hpp"

namespace Onyx
{
struct GltfHandles
{
    TKit::TierArray<Resource> StaticMeshes{};
    TKit::TierArray<Resource> Materials{};
    TKit::TierArray<Resource> Samplers{};
    TKit::TierArray<Resource> Textures{};
};

template <Dimension D> struct GltfData
{
    TKit::TierArray<StaticMeshData<D>> StaticMeshes{};

    // here texture handles inside materials refer to the Images attribute in GltfData, not to any real Resource
    // handle!! AddGltfResouces modifies this material data so that it actually points to real textures (thats why it is
    // taken as a non-const lvalue)
    TKit::TierArray<MaterialData<D>> Materials{};
    TKit::TierArray<SamplerData> Samplers{};
    TKit::TierArray<ImageData> Images{};
};

#ifdef ONYX_ENABLE_GLTF_LOAD
using LoadGltfDataFlags = u8;
enum LoadGltfDataFlagBit : LoadGltfDataFlags
{
    LoadGltfDataFlag_ForceRGBA = 1U << 0,
};
template <Dimension D>
ONYX_NO_DISCARD Result<GltfData<D>> LoadGltfDataFromFile(const std::string &path, LoadGltfDataFlags flags = 0);
#endif

} // namespace Onyx
