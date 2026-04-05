#pragma once

#include "onyx/asset/mesh.hpp"
#include "onyx/asset/material.hpp"
#include "onyx/asset/sampler.hpp"
#include "onyx/asset/image.hpp"

namespace Onyx
{
struct GltfHandles
{
    TKit::TierArray<Asset> StaticMeshes{};
    TKit::TierArray<Asset> Materials{};
    TKit::TierArray<Asset> Samplers{};
    TKit::TierArray<Asset> Textures{};
};

template <Dimension D> struct GltfData
{
    TKit::TierArray<StatMeshData<D>> StaticMeshes{};

    // here texture handles inside materials refer to the Images attribute in GltfData, not to any real Asset
    // handle!! AddGltfAssets modifies this material data so that it actually points to real textures (thats why it is
    // taken as a non-const lvalue)
    TKit::TierArray<MaterialData<D>> Materials{};
    TKit::TierArray<SamplerData> Samplers{};
    TKit::TierArray<ImageData> Images{};
};

#ifdef ONYX_ENABLE_GLTF_LOAD
using LoadGltfDataFlags = u8;
enum LoadGltfDataFlagBit : LoadGltfDataFlags
{
    LoadGltfDataFlag_ForceRGBA = 1 << 0,
};
template <Dimension D>
ONYX_NO_DISCARD Result<GltfData<D>> LoadGltfDataFromFile(const std::string &path, LoadGltfDataFlags flags = 0);
#endif

} // namespace Onyx
