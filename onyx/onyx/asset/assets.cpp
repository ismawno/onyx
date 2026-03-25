#include "onyx/core/pch.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/state/descriptors.hpp"
#include "vkit/state/descriptor_set.hpp"
#include "vkit/resource/sampler.hpp"
#include "vkit/resource/device_image.hpp"
#include "tkit/container/arena_hive.hpp"
#include "tkit/container/static_hive.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/container/dynamic_array.hpp"
#ifdef ONYX_ENABLE_OBJ_LOAD
#    include <tiny_obj_loader.h>
#endif
#ifdef ONYX_ENABLE_GLTF_LOAD
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_CLANG_WARNING_IGNORE("-Wdeprecated-literal-operator")
TKIT_CLANG_WARNING_IGNORE("-Wmissing-field-initializers")
TKIT_GCC_WARNING_IGNORE("-Wdeprecated-literal-operator")
TKIT_GCC_WARNING_IGNORE("-Wmissing-field-initializers")
#    include <tiny_gltf.h>
#    include <stb_image.h>
#    include <stb_image_write.h>
TKIT_COMPILER_WARNING_IGNORE_POP()
#endif

namespace Onyx::Assets
{
using AssetsFlags = u8;
enum AssetsFlagBit : AssetsFlags
{
    AssetsFlag_Locked = 1 << 0,     // internal
    AssetsFlag_MustUpload = 1 << 1, // internal
};
using StatusFlags = u8;
enum StatusFlagBit : StatusFlags
{
    StatusFlag_UpdateVertex = 1 << 0,
    StatusFlag_UpdateIndex = 1 << 1,
    StatusFlag_UpdateMaterial = 1 << 2,
    StatusFlag_UpdateTexture = 1 << 3,
    StatusFlag_CreateTexture = 1 << 4,
    StatusFlag_UpdateSampler = 1 << 5,
    StatusFlag_MustFreeTexture = 1 << 6,
};

template <typename Vertex> struct MeshDataInfo
{
    u32 VertexStart;
    u32 VertexCount;
    u32 IndexStart;
    u32 IndexCount;
    StatusFlags Flags;
};

template <Dimension D> using StatMeshDataInfo = MeshDataInfo<StatVertex<D>>;
template <Dimension D> using ParaMeshDataInfo = MeshDataInfo<ParaVertex<D>>;

template <Dimension D> struct MeshDataInfo<ParaVertex<D>>
{
    u32 VertexStart;
    u32 VertexCount;
    u32 IndexStart;
    u32 IndexCount;
    StatusFlags Flags;
    ParametricShape Shape;
};

struct BatchRange
{
    u32 BatchStart;
    u32 BatchCount;
};

AssetsFlags s_Flags = 0;

template <typename Vertex> struct MeshPoolData
{
    VKit::DeviceBuffer VertexBuffer{};
    VKit::DeviceBuffer IndexBuffer{};
    TKit::TierArray<MeshDataInfo<Vertex>> Info{};
    TKit::DynamicArray<Vertex> Vertices{};
    TKit::DynamicArray<Index> Indices{};

    u32 GetVertexCount(u32 size = TKIT_U32_MAX) const
    {
        if (Info.IsEmpty() || size == 0)
            return 0;
        if (size == TKIT_U32_MAX)
            size = Info.GetSize();
        return Info[size - 1].VertexStart + Info[size - 1].VertexCount;
    }
    u32 GetIndexCount(u32 size = TKIT_U32_MAX) const
    {
        if (Info.IsEmpty() || size == 0)
            return 0;
        if (size == TKIT_U32_MAX)
            size = Info.GetSize();
        return Info[size - 1].IndexStart + Info[size - 1].IndexCount;
    }
};

template <typename Vertex> struct MeshAssetData
{
    TKit::StaticHive<MeshPoolData<Vertex>, ONYX_MAX_ASSET_POOLS> Pools{};
    TKit::StaticArray<AssetPool, ONYX_MAX_ASSET_POOLS> ToRemove{};
};

template <Dimension D> using StatMeshPoolData = MeshPoolData<StatVertex<D>>;
template <Dimension D> using StatMeshAssetData = MeshAssetData<StatVertex<D>>;

template <Dimension D> using ParaMeshPoolData = MeshPoolData<ParaVertex<D>>;
template <Dimension D> using ParaMeshAssetData = MeshAssetData<ParaVertex<D>>;

template <Dimension D> struct MaterialPoolData
{
    VKit::DeviceBuffer Buffer{};
    TKit::TierArray<MaterialData<D>> Materials{};
    TKit::TierArray<StatusFlags> Flags{};
};

template <Dimension D> struct MaterialAssetData
{
    TKit::StaticHive<MaterialPoolData<D>, ONYX_MAX_ASSET_POOLS> Pools{};
    TKit::StaticArray<AssetPool, ONYX_MAX_ASSET_POOLS> ToRemove{};
};

struct SamplerInfo
{
    VKit::Sampler Sampler{};
    Asset Handle = NullAsset;
    SamplerData Data{};
    AssetsFlags Flags = 0;
};

struct SamplerAssetData
{
    TKit::ArenaHive<SamplerInfo> Samplers{};
    TKit::ArenaArray<Asset> ToRemove{};
};

struct TextureInfo
{
    VKit::DeviceImage Image{};
    TextureData Data{};
    Asset Handle = NullAsset;
    StatusFlags Flags{};
};

struct TextureAssetData
{
    TKit::ArenaHive<TextureInfo> Textures{};
    TKit::ArenaArray<Asset> ToRemove{};
};

template <Dimension D> struct AssetData
{
    StatMeshAssetData<D> StaticMeshes{};
    ParaMeshAssetData<D> ParametricMeshes{};
    MaterialAssetData<D> Materials{};
};

static TKit::Storage<AssetData<D2>> s_AssetData2{};
static TKit::Storage<AssetData<D3>> s_AssetData3{};
static TKit::Storage<SamplerAssetData> s_SamplerData{};
static TKit::Storage<TextureAssetData> s_TextureData{};

template <Dimension D> static AssetData<D> &getData()
{
    if constexpr (D == D2)
        return *s_AssetData2;
    else
        return *s_AssetData3;
}

template <typename Vertex> ONYX_NO_DISCARD static Result<> checkSize(MeshPoolData<Vertex> &mpool)
{
    StatusFlags flags = 0;

    const u32 vcount = mpool.GetVertexCount();
    auto result = Resources::GrowBufferIfNeeded<Vertex>(mpool.VertexBuffer, vcount);
    TKIT_RETURN_ON_ERROR(result);

    if (result.GetValue())
    {
        flags = StatusFlag_UpdateVertex;
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Insufficient vertex buffer memory. Resized from {:L} to {:L} bytes",
                       vcount * sizeof(Vertex), mpool.VertexBuffer.GetInfo().Size);
    }

    const u32 icount = mpool.GetIndexCount();
    result = Resources::GrowBufferIfNeeded<Index>(mpool.IndexBuffer, icount);
    TKIT_RETURN_ON_ERROR(result);

    if (result.GetValue())
    {
        flags |= StatusFlag_UpdateIndex;
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Insufficient index buffer memory. Resized from {:L} to {:L} bytes",
                       icount * sizeof(Index), mpool.IndexBuffer.GetInfo().Size);
    }
    if (flags)
        for (MeshDataInfo<Vertex> &info : mpool.Info)
            info.Flags |= flags;
    return Result<>::Ok();
}

template <Dimension D> void updateMaterialDescriptorSet(const AssetPool pool)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pool];

    const VkDescriptorBufferInfo binfo = mpool.Buffer.CreateDescriptorInfo();
    const VKit::DescriptorSetLayout &layout = Descriptors::GetDescriptorSetLayout<D>(Shading_Lit);

    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &layout};
    writer.WriteBuffer(1, binfo, pool);

    for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D>(Shading_Lit))
        writer.Overwrite(set);
}

template <Dimension D> ONYX_NO_DISCARD static Result<> checkMaterialBufferSize(const AssetPool pool)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pool];

    const u32 mcount = mpool.Materials.GetSize();
    const auto result = Resources::GrowBufferIfNeeded<MaterialData<D>>(mpool.Buffer, mcount);
    TKIT_RETURN_ON_ERROR(result);
    if (result.GetValue())
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Insufficient material buffer memory. Resized from {:L} to {:L} bytes",
                       mcount * sizeof(MaterialData<D>), mpool.Buffer.GetInfo().Size);
        for (StatusFlags &flags : mpool.Flags)
            flags = StatusFlag_UpdateMaterial;
        updateMaterialDescriptorSet<D>(pool);
    }
    return Result<>::Ok();
}

template <typename Vertex>
ONYX_NO_DISCARD static Result<> uploadVertexRange(MeshPoolData<Vertex> &mpool, const u32 start, const u32 end)
{
    constexpr VkDeviceSize size = sizeof(Vertex);
    const VkDeviceSize voffset = mpool.GetVertexCount(start) * size;
    const VkDeviceSize vsize = mpool.GetVertexCount(end) * size - voffset;

    TKIT_LOG_DEBUG("[ONYX][ASSETS] Uploading vertex range: {} - {} ({:L} bytes)", mpool.GetVertexCount(start),
                   mpool.GetVertexCount(end), vsize);

    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    return mpool.VertexBuffer.UploadFromHost(pool, queue->GetHandle(), mpool.Vertices.GetData(),
                                             {.srcOffset = voffset, .dstOffset = voffset, .size = vsize});
}
template <typename Vertex>
ONYX_NO_DISCARD static Result<> uploadIndexRange(MeshPoolData<Vertex> &mpool, const u32 start, const u32 end)
{
    constexpr VkDeviceSize size = sizeof(Index);
    const VkDeviceSize ioffset = mpool.GetIndexCount(start) * size;
    const VkDeviceSize isize = mpool.GetIndexCount(end) * size - ioffset;

    TKIT_LOG_DEBUG("[ONYX][ASSETS] Uploading index range: {} - {} ({:L} bytes)", mpool.GetIndexCount(start),
                   mpool.GetIndexCount(end), isize);

    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    return mpool.IndexBuffer.UploadFromHost(pool, queue->GetHandle(), mpool.Indices.GetData(),
                                            {.srcOffset = ioffset, .dstOffset = ioffset, .size = isize});
}

template <typename Vertex> ONYX_NO_DISCARD static Result<> uploadMeshPool(MeshPoolData<Vertex> &mpool)
{
    if (mpool.Info.IsEmpty())
        return Result<>::Ok();
    const auto checkFlags = [&mpool](const u32 index, const StatusFlags flags) {
        return flags & mpool.Info[index].Flags;
    };

    TKIT_RETURN_IF_FAILED(checkSize(mpool));

    auto &layouts = mpool.Info;
    for (u32 i = 0; i < layouts.GetSize(); ++i)
        if (checkFlags(i, StatusFlag_UpdateVertex))
            for (u32 j = i + 1; j <= layouts.GetSize(); ++j)
                if (j == layouts.GetSize() || !checkFlags(j, StatusFlag_UpdateVertex))
                {
                    TKIT_RETURN_IF_FAILED(uploadVertexRange(mpool, i, j));
                    i = j;
                    break;
                }
    for (u32 i = 0; i < layouts.GetSize(); ++i)
        if (checkFlags(i, StatusFlag_UpdateIndex))
            for (u32 j = i + 1; j <= layouts.GetSize(); ++j)
                if (j == layouts.GetSize() || !checkFlags(j, StatusFlag_UpdateIndex))
                {
                    TKIT_RETURN_IF_FAILED(uploadIndexRange(mpool, i, j));
                    i = j;
                    break;
                }
    for (MeshDataInfo<Vertex> &info : layouts)
        info.Flags = 0;
    return Result<>::Ok();
}

template <typename Vertex> ONYX_NO_DISCARD static Result<> uploadMeshData(MeshAssetData<Vertex> &meshes)
{
    for (const AssetPool pool : meshes.ToRemove)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying mesh pool with id {}", pool);
        MeshPoolData<Vertex> &mpool = meshes.Pools[pool];
        mpool.VertexBuffer.Destroy();
        mpool.IndexBuffer.Destroy();

        meshes.Pools.Remove(pool);
    }
    meshes.ToRemove.Clear();

    for (MeshPoolData<Vertex> &mpool : meshes.Pools)
    {
        TKIT_RETURN_IF_FAILED(uploadMeshPool(mpool));
    }

    return Result<>::Ok();
}

template <Dimension D>
ONYX_NO_DISCARD static Result<> uploadMaterialRange(const AssetPool pool, const u32 start, const u32 end)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pool];
    const VkDeviceSize offset = start * sizeof(MaterialData<D>);
    const VkDeviceSize size = end * sizeof(MaterialData<D>) - offset;

    TKIT_LOG_DEBUG("[ONYX][ASSETS] Uploading material range for pool {}: {} - {} ({:L} bytes)", pool, start, end, size);

    VKit::CommandPool &cpool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    return mpool.Buffer.UploadFromHost(cpool, queue->GetHandle(), mpool.Materials.GetData(),
                                       {.srcOffset = offset, .dstOffset = offset, .size = size});
}

template <Dimension D> ONYX_NO_DISCARD static Result<> uploadMaterialPool(const AssetPool pool)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pool];
    if (mpool.Materials.IsEmpty())
        return Result<>::Ok();

    TKIT_ASSERT(mpool.Materials.GetSize() == mpool.Flags.GetSize(),
                "[ONYX][ASSETS] Material data size ({}) and flags size ({}) mismatch", mpool.Materials.GetSize(),
                mpool.Flags.GetSize());

    const auto mustUpdate = [&mpool](const u32 index) { return StatusFlag_UpdateMaterial & mpool.Flags[index]; };

    TKIT_RETURN_IF_FAILED(checkMaterialBufferSize<D>(pool));

    const u32 size = mpool.Materials.GetSize();
    for (u32 i = 0; i < size; ++i)
        if (mustUpdate(i))
            for (u32 j = i + 1; j <= size; ++j)
                if (j == size || !mustUpdate(j))
                {
                    TKIT_RETURN_IF_FAILED(uploadMaterialRange<D>(pool, i, j));
                    i = j;
                    break;
                }

    for (StatusFlags &flags : mpool.Flags)
        flags = 0;

    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> uploadMaterialData()
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    for (const AssetPool pool : materials.ToRemove)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying material pool with id {}", pool);
        MaterialPoolData<D> &mpool = materials.Pools[pool];
        mpool.Buffer.Destroy();
        materials.Pools.Remove(pool);
    }
    materials.ToRemove.Clear();

    for (const AssetPool pool : materials.Pools.GetValidIds())
    {
        TKIT_RETURN_IF_FAILED(uploadMaterialPool<D>(pool));
    }

    return Result<>::Ok();
}

template <typename Vertex> static void terminate(MeshAssetData<Vertex> &meshData)
{
    for (MeshPoolData<Vertex> &pool : meshData.Pools)
    {
        pool.VertexBuffer.Destroy();
        pool.IndexBuffer.Destroy();
    }
}

template <Dimension D> static void terminateMaterials()
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    for (MaterialPoolData<D> &mpool : materials.Pools)
        mpool.Buffer.Destroy();
}

template <Dimension D> static void terminate()
{
    AssetData<D> &data = getData<D>();
    terminateMaterials<D>();
    terminate(data.ParametricMeshes);
    terminate(data.StaticMeshes);
}

void Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][ASSETS] Initializing");
    s_AssetData2.Construct();
    s_AssetData3.Construct();
    s_SamplerData.Construct();
    s_TextureData.Construct();

    s_SamplerData->Samplers.Reserve(specs.MaxSamplers);
    s_SamplerData->ToRemove.Reserve(specs.MaxSamplers);

    s_TextureData->Textures.Reserve(specs.MaxTextures);
    s_TextureData->ToRemove.Reserve(specs.MaxTextures);
}

void Terminate()
{
    terminate<D2>();
    terminate<D3>();

    for (TextureInfo &tinfo : s_TextureData->Textures)
    {
        tinfo.Image.Destroy();
#ifdef ONYX_ENABLE_GLTF_LOAD
        if (tinfo.Flags & StatusFlag_MustFreeTexture)
            TKit::Deallocate(tinfo.Data.Data);
#endif
    }

    for (SamplerInfo &sinfo : s_SamplerData->Samplers)
        sinfo.Sampler.Destroy();

    s_SamplerData.Construct();
    s_TextureData.Destruct();
    s_AssetData2.Destruct();
    s_AssetData3.Destruct();
}

Asset AddSampler(const SamplerData &data)
{
    const Asset handle = s_SamplerData->Samplers.Insert();
    SamplerInfo &sinfo = s_SamplerData->Samplers[handle];
    sinfo.Handle = handle;
    sinfo.Data = data;

    sinfo.Flags |= StatusFlag_UpdateSampler;
    return handle;
}

void UpdateSampler(const Asset handle, const SamplerData &data)
{
    SamplerInfo &sinfo = s_SamplerData->Samplers[handle];
    sinfo.Data = data;
    sinfo.Flags |= StatusFlag_UpdateSampler;
}

template <Dimension D> static void removeSamplerReferences(const Asset handle)
{
    const auto updateRef = [handle](Asset &toUpdate) -> StatusFlags {
        if (toUpdate == handle)
        {
            toUpdate = NullAsset;
            return StatusFlag_UpdateMaterial;
        }
        return 0;
    };

    MaterialAssetData<D> &materials = getData<D>().Materials;
    for (MaterialPoolData<D> &mpool : materials.Pools)
    {
        auto &matData = mpool.Materials;
        auto &flags = mpool.Flags;

        TKIT_ASSERT(matData.GetSize() == flags.GetSize(),
                    "[ONYX][ASSETS] Material data size ({}) and flags size ({}) mismatch", matData.GetSize(),
                    flags.GetSize());

        for (u32 i = 0; i < matData.GetSize(); ++i)
            if constexpr (D == D2)
                flags[i] |= updateRef(matData[i].Sampler);
            else
                for (Asset &handle : matData[i].Samplers)
                    flags[i] |= updateRef(handle);
    }
}

void RemoveSampler(const Asset handle)
{
    s_SamplerData->ToRemove.Append(handle);
    removeSamplerReferences<D2>(handle);
    removeSamplerReferences<D3>(handle);
}

template <Dimension D>
GltfHandles AddGltfAssets(const AssetPool meshPool, const AssetPool materialPool, GltfAssets<D> &assets)
{
    GltfHandles handles;
    handles.StaticMeshes.Reserve(assets.StaticMeshes.GetSize());
    handles.Materials.Reserve(assets.Materials.GetSize());
    handles.Samplers.Reserve(assets.Samplers.GetSize());
    handles.Textures.Reserve(assets.Textures.GetSize());

    for (const StatMeshData<D> &smesh : assets.StaticMeshes)
        handles.StaticMeshes.Append(AddMesh(meshPool, smesh));

    for (const SamplerData &sdata : assets.Samplers)
        handles.Samplers.Append(AddSampler(sdata));

    for (const TextureData &tdata : assets.Textures)
        handles.Textures.Append(AddTexture(tdata));

    for (MaterialData<D> &mdata : assets.Materials)
    {
        if constexpr (D == D2)
        {
            if (mdata.Sampler != NullAsset)
                mdata.Sampler = handles.Samplers[mdata.Sampler];
            if (mdata.Texture != NullAsset)
                mdata.Texture = handles.Textures[mdata.Texture];
        }
        else
        {
            for (Asset &sampler : mdata.Samplers)
                if (sampler != NullAsset)
                    sampler = handles.Samplers[sampler];
            for (Asset &tex : mdata.Textures)
                if (tex != NullAsset)
                    tex = handles.Textures[tex];
        }
        handles.Materials.Append(AddMaterial(materialPool, mdata));
    }

    return handles;
}

Asset AddTexture(const TextureData &data, const AddTextureFlags flags)
{
#ifndef ONYX_ENABLE_GLTF_LOAD
    TKIT_ASSERT(flags & AddTextureFlag_ManuallyHandledMemory,
                "[ONYX][ASSETS] If GLTF load is disabled, all texture data CPU memory must be handled by the user. "
                "This must be reflected by passing the AddTextureFlag_ManuallyHandledMemory flag");
#endif
    const Asset tex = s_TextureData->Textures.Insert();
    TextureInfo &tinfo = s_TextureData->Textures[tex];
    tinfo.Data = data;
    tinfo.Handle = tex;

    StatusFlags sflags = StatusFlag_UpdateTexture | StatusFlag_CreateTexture;
#ifdef ONYX_ENABLE_GLTF_LOAD
    if (!(flags & AddTextureFlag_ManuallyHandledMemory))
        sflags |= StatusFlag_MustFreeTexture;
#endif
    tinfo.Flags = sflags;
    return tex;
}
void UpdateTexture(const Asset tex, const TextureData &data, const AddTextureFlags flags)
{
#ifndef ONYX_ENABLE_GLTF_LOAD
    TKIT_ASSERT(flags & AddTextureFlag_ManuallyHandledMemory,
                "[ONYX][ASSETS] If GLTF load is disabled, all texture data CPU memory must be handled by the user. "
                "This must be reflected by passing the AddTextureFlag_ManuallyHandledMemory flag");
#endif

    TextureInfo &tinfo = s_TextureData->Textures[tex];
    TKIT_ASSERT(
        data.ComputeSize() == tinfo.Data.ComputeSize(),
        "[ONYX][ASSETS] When updating a texture, the size of the data of the previous and updated texture must be the "
        "same. If they are not, you must create a new texture");

#ifdef ONYX_ENABLE_GLTF_LOAD
    if (tinfo.Flags & StatusFlag_MustFreeTexture)
        stbi_image_free(data.Data);
#endif

    tinfo.Data = data;
    tinfo.Flags |= StatusFlag_UpdateTexture;
#ifdef ONYX_ENABLE_GLTF_LOAD
    if (!(flags & AddTextureFlag_ManuallyHandledMemory))
        tinfo.Flags |= StatusFlag_MustFreeTexture;
#endif
}

template <Dimension D> static void removeTextureReferences(const Asset handle)
{
    const auto updateRef = [handle](Asset &toUpdate) -> StatusFlags {
        if (toUpdate == handle)
        {
            toUpdate = NullAsset;
            return StatusFlag_UpdateMaterial;
        }
        return 0;
    };
    MaterialAssetData<D> &materials = getData<D>().Materials;
    for (MaterialPoolData<D> &mpool : materials.Pools)
    {
        auto &matData = mpool.Materials;
        auto &flags = mpool.Flags;

        TKIT_ASSERT(matData.GetSize() == flags.GetSize(),
                    "[ONYX][ASSETS] Material data size ({}) and flags size ({}) mismatch", matData.GetSize(),
                    flags.GetSize());

        for (u32 i = 0; i < matData.GetSize(); ++i)
            if constexpr (D == D2)
                flags[i] |= updateRef(matData[i].Texture);
            else
                for (Asset &handle : matData[i].Textures)
                    flags[i] |= updateRef(handle);
    }
}

void RemoveTexture(const Asset tex)
{
    s_TextureData->ToRemove.Append(tex);
    removeTextureReferences<D2>(tex);
    removeTextureReferences<D3>(tex);
}

template <Dimension D> bool IsMeshPoolHandleValid(const Geometry geo, const AssetPool handle)
{
    TKIT_ASSERT(geo != Geometry_Circle, "[ONYX][ASSETS] Circles are not considered meshes");
    if (geo == Geometry_Static)
        return getData<D>().StaticMeshes.Pools.Contains(handle);
    else if (geo == Geometry_Parametric)
        return getData<D>().ParametricMeshes.Pools.Contains(handle);
    return false;
}

template <Dimension D> bool IsMeshHandleValid(const Geometry geo, const Asset handle)
{
    const AssetPool pool = GetPoolHandle(handle);
    if (!IsMeshPoolHandleValid<D>(geo, pool))
        return false;
    const u32 idx = GetAssetIndex(handle);
    if (geo == Geometry_Static)
        return idx < getData<D>().StaticMeshes.Pools[pool].Info.GetSize();
    else if (geo == Geometry_Parametric)
        return idx < getData<D>().ParametricMeshes.Pools[pool].Info.GetSize();
    return false;
}

template <Dimension D> bool IsMaterialPoolHandleValid(const AssetPool handle)
{
    return getData<D>().Materials.Pools.Contains(handle);
}

template <Dimension D> bool IsMaterialHandleValid(const Asset handle)
{
    const AssetPool pool = GetPoolHandle(handle);
    const u32 idx = GetAssetIndex(handle);
    return IsMaterialPoolHandleValid<D>(pool) && idx < getData<D>().Materials.Pools[pool].Materials.GetSize();
}

bool IsSamplerHandleValid(const Asset handle)
{
    return s_SamplerData->Samplers.Contains(handle);
}
bool IsTextureHandleValid(const Asset handle)
{
    return s_TextureData->Textures.Contains(handle);
}

template <typename Vertex> ONYX_NO_DISCARD static Result<AssetPool> createMeshPool(MeshAssetData<Vertex> &data)
{
    auto result = Resources::CreateBuffer<Vertex>(Buffer_DeviceVertex);
    TKIT_RETURN_ON_ERROR(result);
    VKit::DeviceBuffer vbuffer = result.GetValue();

    result = Resources::CreateBuffer<Index>(Buffer_DeviceIndex);
    TKIT_RETURN_ON_ERROR(result, vbuffer.Destroy());
    VKit::DeviceBuffer ibuffer = result.GetValue();

    AssetPool pool = data.Pools.Insert();
    MeshPoolData<Vertex> &mpool = data.Pools[pool];
    mpool.VertexBuffer = vbuffer;
    mpool.IndexBuffer = ibuffer;

    if (Core::CanNameObjects())
    {
        const std::string vb = TKit::Format("onyx-assets-{}D-vertex-buffer-{}", u8(Vertex::Dim), pool);
        const std::string ib = TKit::Format("onyx-assets-{}D-index-buffer-{}", u8(Vertex::Dim), pool);

        TKIT_RETURN_IF_FAILED(vbuffer.SetName(vb.c_str()), vbuffer.Destroy(); data.Pools.Remove(pool));
        TKIT_RETURN_IF_FAILED(ibuffer.SetName(ib.c_str()), ibuffer.Destroy(); data.Pools.Remove(pool));
    }

    return pool;
}

template <typename Vertex> static void destroyMeshPool(const AssetPool pool, MeshAssetData<Vertex> &meshes)
{
    meshes.ToRemove.Append(pool);
}

template <Dimension D> Result<AssetPool> CreateMeshPool(const Geometry geo)
{
    if (geo == Geometry_Circle)
        return Result<AssetPool>::Error(Error_BadInput, "[ONYX][ASSETS] Circles are not considered meshes");
    if (geo == Geometry_Static)
        return createMeshPool(getData<D>().StaticMeshes);
    else if (geo == Geometry_Parametric)
        return createMeshPool(getData<D>().ParametricMeshes);
    return NullAssetPool;
}

template <Dimension D> void DestroyMeshPool(const Geometry geo, const AssetPool pool)
{
    TKIT_ASSERT(geo != Geometry_Circle, "[ONYX][ASSETS] Circles are not considered meshes");
    if (geo == Geometry_Static)
        getData<D>().StaticMeshes.ToRemove.Append(pool);
    else if (geo == Geometry_Parametric)
        getData<D>().ParametricMeshes.ToRemove.Append(pool);
}

template <typename Vertex>
static Asset addMesh(const AssetPool pool, MeshAssetData<Vertex> &meshes, const MeshData<Vertex> &data)
{
    MeshPoolData<Vertex> &mpool = meshes.Pools[pool];
    const u32 idx = mpool.Info.GetSize();
    MeshDataInfo<Vertex> info;
    info.VertexStart = mpool.GetVertexCount();
    info.VertexCount = data.Vertices.GetSize();
    info.IndexStart = mpool.GetIndexCount();
    info.IndexCount = data.Indices.GetSize();
    info.Flags = StatusFlag_UpdateVertex | StatusFlag_UpdateIndex;
    if constexpr (Vertex::Geo == Geometry_Parametric)
        info.Shape = data.Shape;

    mpool.Info.Append(info);

    auto &vertices = mpool.Vertices;
    auto &indices = mpool.Indices;
    vertices.Insert(vertices.end(), data.Vertices.begin(), data.Vertices.end());
    indices.Insert(indices.end(), data.Indices.begin(), data.Indices.end());

    return GetAssetHandle(pool, idx);
}

template <Dimension D> Asset AddMesh(const AssetPool pool, const StatMeshData<D> &data)
{
    return addMesh(pool, getData<D>().StaticMeshes, data);
}
template <Dimension D> Asset AddMesh(const AssetPool pool, const ParaMeshData<D> &data)
{
    return addMesh(pool, getData<D>().ParametricMeshes, data);
}

template <typename Vertex>
static void updateMesh(const Asset handle, MeshAssetData<Vertex> &meshes, const MeshData<Vertex> &data)
{
    const u32 idx = GetAssetIndex(handle);
    const AssetPool pool = GetPoolHandle(handle);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pool];

    MeshDataInfo<Vertex> &info = mpool.Info[idx];
    TKIT_ASSERT(
        data.Vertices.GetSize() == info.VertexCount && data.Indices.GetSize() == info.IndexCount,
        "[ONYX][ASSETS] When updating a mesh, the vertex and index count of the previous and updated mesh must be the "
        "same. If they are not, you must create a new mesh");

    TKit::ForwardCopy(mpool.Vertices.begin() + info.VertexStart, data.Vertices.begin(), data.Vertices.end());
    TKit::ForwardCopy(mpool.Indices.begin() + info.IndexStart, data.Indices.begin(), data.Indices.end());

    if constexpr (Vertex::Geo == Geometry_Parametric)
        info.Shape = data.Shape;

    info.Flags |= StatusFlag_UpdateVertex | StatusFlag_UpdateIndex;
}

template <Dimension D> void UpdateMesh(const Asset handle, const StatMeshData<D> &data)
{
    return updateMesh(handle, getData<D>().StaticMeshes, data);
}
template <Dimension D> void UpdateMesh(const Asset handle, const ParaMeshData<D> &data)
{
    return updateMesh(handle, getData<D>().ParametricMeshes, data);
}

template <Dimension D> Result<AssetPool> CreateMaterialPool()
{
    const auto result = Resources::CreateBuffer<MaterialData<D>>(Buffer_DeviceStorage);
    TKIT_RETURN_ON_ERROR(result);

    MaterialAssetData<D> &materials = getData<D>().Materials;
    const AssetPool pool = AssetPool(materials.Pools.Insert());

    MaterialPoolData<D> &mpool = materials.Pools[pool];
    mpool.Buffer = result.GetValue();

    if (Core::CanNameObjects())
    {
        const std::string name = TKit::Format("onyx-assets-{}D-material-pool-buffer-{}", u8(D), pool);
        TKIT_RETURN_IF_FAILED(mpool.Buffer.SetName(name.c_str()), mpool.Buffer.Destroy(); materials.Pools.Remove(pool));
    }

    updateMaterialDescriptorSet<D>(pool);

    return pool;
}

template <Dimension D> void DestroyMaterialPool(const AssetPool handle)
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    materials.ToRemove.Append(handle);
}

template <Dimension D> Asset AddMaterial(const AssetPool pool, const MaterialData<D> &data)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pool];

    const u32 idx = mpool.Materials.GetSize();
    mpool.Materials.Append(data);
    mpool.Flags.Append(StatusFlag_UpdateMaterial);

    return GetAssetHandle(pool, idx);
}

template <Dimension D> void UpdateMaterial(const Asset handle, const MaterialData<D> &data)
{
    const u32 idx = GetAssetIndex(handle);
    const AssetPool pool = GetPoolHandle(handle);

    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pool];
    mpool.Materials[idx] = data;
    mpool.Flags[idx] = StatusFlag_UpdateMaterial;
}

template <typename Vertex> static MeshData<Vertex> getMeshData(const Asset handle, MeshAssetData<Vertex> &meshes)
{
    const u32 idx = GetAssetIndex(handle);
    const AssetPool pool = GetPoolHandle(handle);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pool];

    MeshData<Vertex> data{};
    const MeshDataInfo<Vertex> &info = mpool.Info[idx];

    const u32 vstart = info.VertexStart;
    const u32 vend = vstart + info.VertexCount;

    const u32 istart = info.IndexStart;
    const u32 iend = vstart + info.IndexCount;

    data.Vertices.Insert(data.Vertices.end(), mpool.Vertices.begin() + vstart, mpool.Vertices.begin() + vend);
    data.Indices.Insert(data.Indices.end(), mpool.Indices.begin() + istart, mpool.Indices.begin() + iend);
    if constexpr (Vertex::Geo == Geometry_Parametric)
        data.Shape = info.Shape;

    return data;
}

template <Dimension D> StatMeshData<D> GetStaticMeshData(const Asset handle)
{
    return getMeshData(handle, getData<D>().StaticMeshes);
}
template <Dimension D> ParaMeshData<D> GetParametricMeshData(const Asset handle)
{
    return getMeshData(handle, getData<D>().ParametricMeshes);
}
template <Dimension D> ParametricShape GetParametricShape(const Asset handle)
{
    const u32 idx = GetAssetIndex(handle);
    const AssetPool pool = GetPoolHandle(handle);

    ParaMeshPoolData<D> &mpool = getData<D>().ParametricMeshes.Pools[pool];
    return mpool.Info[idx].Shape;
}
template <Dimension D> const MaterialData<D> &GetMaterialData(const Asset handle)
{
    const u32 idx = GetAssetIndex(handle);
    const AssetPool pool = GetPoolHandle(handle);

    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pool];
    return mpool.Materials[idx];
}
const TextureData &GetTextureData(const Asset texture)
{
    return s_TextureData->Textures[texture].Data;
}

void Lock()
{
    s_Flags |= AssetsFlag_Locked;
}
Result<> Unlock()
{
    s_Flags &= ~AssetsFlag_Locked;
    if (s_Flags & AssetsFlag_MustUpload)
        return Upload();

    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> upload()
{
    TKIT_RETURN_IF_FAILED(uploadMeshData(getData<D>().StaticMeshes));
    TKIT_RETURN_IF_FAILED(uploadMeshData(getData<D>().ParametricMeshes));
    return uploadMaterialData<D>();
}

ONYX_NO_DISCARD static Result<> uploadTextures()
{
    for (const Asset handle : s_TextureData->ToRemove)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying texture with id {}", handle);
        TextureInfo &tinfo = s_TextureData->Textures[handle];
        tinfo.Image.Destroy();
#ifdef ONYX_ENABLE_GLTF_LOAD
        if (tinfo.Flags & StatusFlag_MustFreeTexture)
            TKit::Deallocate(tinfo.Data.Data);
#endif
        s_TextureData->Textures.Remove(handle);
    }
    s_TextureData->ToRemove.Clear();

    StatusFlags sflags = 0;
    for (const TextureInfo &tinfo : s_TextureData->Textures)
        sflags |= tinfo.Flags;

    TKIT_ASSERT((sflags & StatusFlag_UpdateTexture) || !(sflags & StatusFlag_CreateTexture),
                "[ONYX][ASSETS] If a texture needs to be created, it also needs to be updated");

    if (!(sflags & StatusFlag_UpdateTexture))
        return Result<>::Ok();

    if (sflags & StatusFlag_CreateTexture)
    {
        const VKit::DescriptorSetLayout &layout2 = Descriptors::GetDescriptorSetLayout<D2>(Shading_Lit);
        VKit::DescriptorSet::Writer writer2{Core::GetDevice(), &layout2};

        const VKit::DescriptorSetLayout &layout3 = Descriptors::GetDescriptorSetLayout<D3>(Shading_Lit);
        VKit::DescriptorSet::Writer writer3{Core::GetDevice(), &layout3};

        TKit::StackArray<VkDescriptorImageInfo> imageInfos{};
        imageInfos.Reserve(s_TextureData->Textures.GetSize());

        for (TextureInfo &tinfo : s_TextureData->Textures)
            if (tinfo.Flags & StatusFlag_CreateTexture)
            {
                tinfo.Flags &= ~StatusFlag_CreateTexture;
                const TextureData &tdata = tinfo.Data;
                TKIT_LOG_DEBUG("[ONYX][ASSETS] Creating new texture (handle={}) with dimensions {}x{} and {} channels, "
                               "with size {:L}",
                               tinfo.Handle, tdata.Width, tdata.Height, tdata.Components, tdata.ComputeSize());

                TKIT_ASSERT(!tinfo.Image,
                            "[ONYX][ASSETS] To create a texture, it is underlying image must not exist yet");

                auto result = VKit::DeviceImage::Builder(Core::GetDevice(), Core::GetVulkanAllocator(),
                                                         VkExtent2D{tdata.Width, tdata.Height}, tdata.Format,
                                                         VKit::DeviceImageFlag_Color | VKit::DeviceImageFlag_Sampled |
                                                             VKit::DeviceImageFlag_Destination)
                                  .WithImageView()
                                  .Build();
                TKIT_RETURN_ON_ERROR(result);
                VKit::DeviceImage &img = result.GetValue();
                tinfo.Image = img;
                if (Core::CanNameObjects())
                {
                    const std::string name = TKit::Format("onyx-assets-texture-image-{}", tinfo.Handle);
                    TKIT_RETURN_IF_FAILED(img.SetName(name.c_str()));
                }
                const VkDescriptorImageInfo &info =
                    imageInfos.Append(img.CreateDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
                writer2.WriteImage(3, info, tinfo.Handle);
                writer3.WriteImage(3, info, tinfo.Handle);
            }

        for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D2>(Shading_Lit))
            writer2.Overwrite(set);
        for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D3>(Shading_Lit))
            writer3.Overwrite(set);
    }

    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    for (TextureInfo &tinfo : s_TextureData->Textures)
        if (tinfo.Flags & StatusFlag_UpdateTexture)
        {
            tinfo.Flags &= ~StatusFlag_UpdateTexture;
            const TextureData &tdata = tinfo.Data;
            VKit::DeviceImage &img = tinfo.Image;

            const VkDeviceSize size = img.ComputeSize();
            TKIT_ASSERT(
                size == tdata.ComputeSize(),
                "[ONYX][ASSETS] Size mismatch. Device image reports {:L} bytes while texture data reports {:L} bytes",
                size, tdata.ComputeSize());

            TKIT_LOG_DEBUG("[ONYX][ASSETS] Uploading new texture of size {:L} bytes", size);

            auto result =
                Resources::CreateBuffer(VKit::DeviceBufferFlag_Staging | VKit::DeviceBufferFlag_HostMapped, size);
            TKIT_RETURN_ON_ERROR(result);

            VKit::DeviceBuffer &uploadBuffer = result.GetValue();
            if (Core::CanNameObjects())
            {
                TKIT_RETURN_IF_FAILED(uploadBuffer.SetName("onyx-assets-texture-upload-buffer"),
                                      uploadBuffer.Destroy());
            }
            uploadBuffer.Write(tdata.Data, {.srcOffset = 0, .dstOffset = 0, .size = size});

            TKIT_RETURN_IF_FAILED(uploadBuffer.Flush(), uploadBuffer.Destroy());
            const auto cmdres = pool.BeginSingleTimeCommands();
            TKIT_RETURN_ON_ERROR(cmdres, uploadBuffer.Destroy());

            const VkCommandBuffer cmd = cmdres.GetValue();
            img.TransitionLayout2(
                cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                {.DstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, .DstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR});

            VkBufferImageCopy2KHR copy{};
            copy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR;
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.layerCount = 1;
            copy.imageExtent.width = tdata.Width;
            copy.imageExtent.height = tdata.Height;
            copy.imageExtent.depth = 1;

            img.CopyFromBuffer2(cmd, uploadBuffer, copy);

            img.TransitionLayout2(
                cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                {.SrcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, .SrcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR});

            TKIT_RETURN_IF_FAILED(pool.EndSingleTimeCommands(cmd, queue->GetHandle()), uploadBuffer.Destroy());
            uploadBuffer.Destroy();
        }
    return Result<>::Ok();
}
static VkSamplerMipmapMode toVulkan(const SamplerMode mode)
{
    switch (mode)
    {
    case SamplerMode_Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    case SamplerMode_Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
}
static VkFilter toVulkan(const SamplerFilter filter)
{
    switch (filter)
    {
    case SamplerFilter_Linear:
        return VK_FILTER_LINEAR;
    case SamplerFilter_Nearest:
        return VK_FILTER_NEAREST;
    case SamplerFilter_Cubic:
        return VK_FILTER_CUBIC_EXT;
    }
}
static VkSamplerAddressMode toVulkan(const SamplerWrap wrap)
{
    switch (wrap)
    {
    case SamplerWrap_Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case SamplerWrap_ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case SamplerWrap_MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
}

ONYX_NO_DISCARD static Result<> uploadSamplers()
{
    for (const Asset handle : s_SamplerData->ToRemove)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying sampler with id {}", handle);
        SamplerInfo &sinfo = s_SamplerData->Samplers[handle];
        sinfo.Sampler.Destroy();
        s_SamplerData->Samplers.Remove(handle);
    }
    s_SamplerData->ToRemove.Clear();

    StatusFlags sflags = 0;
    for (const SamplerInfo &sinfo : s_SamplerData->Samplers)
        sflags |= sinfo.Flags;

    if (!(sflags & StatusFlag_UpdateSampler))
        return Result<>::Ok();

    const VKit::DescriptorSetLayout &layout2 = Descriptors::GetDescriptorSetLayout<D2>(Shading_Lit);
    VKit::DescriptorSet::Writer writer2{Core::GetDevice(), &layout2};

    const VKit::DescriptorSetLayout &layout3 = Descriptors::GetDescriptorSetLayout<D3>(Shading_Lit);
    VKit::DescriptorSet::Writer writer3{Core::GetDevice(), &layout3};

    TKit::StackArray<VkDescriptorImageInfo> imageInfos{};
    imageInfos.Reserve(s_SamplerData->Samplers.GetSize());
    for (SamplerInfo &sinfo : s_SamplerData->Samplers)
        if (sinfo.Flags & StatusFlag_UpdateSampler)
        {
            sinfo.Flags &= ~StatusFlag_UpdateSampler;
            const auto result = VKit::Sampler::Builder(Core::GetDevice())
                                    .SetMipmapMode(toVulkan(sinfo.Data.Mode))
                                    .SetMinFilter(toVulkan(sinfo.Data.MinFilter))
                                    .SetMagFilter(toVulkan(sinfo.Data.MagFilter))
                                    .SetAddressModeU(toVulkan(sinfo.Data.WrapU))
                                    .SetAddressModeV(toVulkan(sinfo.Data.WrapV))
                                    .SetAddressModeW(toVulkan(sinfo.Data.WrapW))
                                    .Build();
            TKIT_RETURN_ON_ERROR(result);

            sinfo.Sampler = result.GetValue();
            TKIT_LOG_DEBUG("[ONYX][ASSETS] Updating sampler with id {}", sinfo.Handle);
            const VkDescriptorImageInfo &info = imageInfos.Append(VkDescriptorImageInfo{
                .sampler = sinfo.Sampler, .imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED});
            writer2.WriteImage(2, info, sinfo.Handle);
            writer3.WriteImage(2, info, sinfo.Handle);
        }

    for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D2>(Shading_Lit))
        writer2.Overwrite(set);
    for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D3>(Shading_Lit))
        writer3.Overwrite(set);

    return Result<>::Ok();
}

Result<> Upload()
{
    if (s_Flags & AssetsFlag_Locked)
        return Result<>::Error(Error_LockedAssets,
                               "[ONYX][ASSETS] Cannot upload/mutate asset data because it is locked, either by the "
                               "user or by Onyx. If using the application class, this happens automatically in-between "
                               "frames to avoid having dangling references in command buffers");
    TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());
    TKIT_RETURN_IF_FAILED(upload<D2>());
    TKIT_RETURN_IF_FAILED(upload<D3>());
    TKIT_RETURN_IF_FAILED(uploadTextures());
    TKIT_RETURN_IF_FAILED(uploadSamplers());
    s_Flags &= ~AssetsFlag_MustUpload;
    Renderer::FlushAllContexts();
    return Result<>::Ok();
}

Result<bool> RequestUpload()
{
    if (s_Flags & AssetsFlag_Locked)
    {
        s_Flags |= AssetsFlag_MustUpload;
        return false;
    }
    TKIT_RETURN_IF_FAILED(Upload());
    return true;
}

template <typename Vertex> static MeshDataLayout getMeshLayout(const Asset handle, MeshAssetData<Vertex> &meshes)
{
    const u32 idx = GetAssetIndex(handle);
    const AssetPool pool = GetPoolHandle(handle);

    const MeshDataInfo<Vertex> &info = meshes.Pools[pool].Info[idx];
    return MeshDataLayout{.VertexStart = info.VertexStart,
                          .VertexCount = info.VertexCount,
                          .IndexStart = info.IndexStart,
                          .IndexCount = info.IndexCount};
}

template <Dimension D> MeshDataLayout GetMeshLayout(const Geometry geo, const Asset handle)
{
    TKIT_ASSERT(geo != Geometry_Circle, "[ONYX][ASSETS] Circles are not considered meshes");

    if (geo == Geometry_Static)
        return getMeshLayout(handle, getData<D>().StaticMeshes);
    else if (geo == Geometry_Parametric)
        return getMeshLayout(handle, getData<D>().ParametricMeshes);
    return MeshDataLayout{};
}

template <Dimension D> TKit::Span<const u32> GetMeshAssetPools(const Geometry geo)
{
    TKIT_ASSERT(geo != Geometry_Circle, "[ONYX][ASSETS] Circles are not considered meshes");

    if (geo == Geometry_Static)
        return getData<D>().StaticMeshes.Pools.GetValidIds();
    else if (geo == Geometry_Parametric)
        return getData<D>().ParametricMeshes.Pools.GetValidIds();
    return TKit::Span<const u32>{};
}

template <typename Vertex> static u32 getMeshBatchCount(const MeshAssetData<Vertex> &meshes)
{
    u32 count = 0;
    for (const MeshPoolData<Vertex> &mpool : meshes.Pools)
        count += mpool.Info.GetSize();
    return count;
}

u32 GetBatchCount()
{
    u32 count = 1; // circles
    count += getMeshBatchCount(getData<D2>().StaticMeshes);
    count += getMeshBatchCount(getData<D3>().StaticMeshes);

    count += getMeshBatchCount(getData<D2>().ParametricMeshes);
    count += getMeshBatchCount(getData<D3>().ParametricMeshes);
    return count;
}
template <Dimension D> u32 GetMeshCount(const Geometry geo, const AssetPool pool)
{
    TKIT_ASSERT(geo != Geometry_Circle, "[ONYX][ASSETS] Circles are not considered meshes");
    if (geo == Geometry_Static)
        return getData<D>().StaticMeshes.Pools[pool].Info.GetSize();
    else if (geo == Geometry_Parametric)
        return getData<D>().ParametricMeshes.Pools[pool].Info.GetSize();
    return 0;
}
template <Dimension D> const VKit::DeviceBuffer *GetMeshVertexBuffer(const Geometry geo, const AssetPool pool)
{
    TKIT_ASSERT(geo != Geometry_Circle, "[ONYX][ASSETS] Circles are not considered meshes");
    if (geo == Geometry_Static)
        return &getData<D>().StaticMeshes.Pools[pool].VertexBuffer;
    else if (geo == Geometry_Parametric)
        return &getData<D>().ParametricMeshes.Pools[pool].VertexBuffer;
    return nullptr;
}
template <Dimension D> const VKit::DeviceBuffer *GetMeshIndexBuffer(const Geometry geo, const AssetPool pool)
{
    TKIT_ASSERT(geo != Geometry_Circle, "[ONYX][ASSETS] Circles are not considered meshes");
    if (geo == Geometry_Static)
        return &getData<D>().StaticMeshes.Pools[pool].IndexBuffer;
    else if (geo == Geometry_Parametric)
        return &getData<D>().ParametricMeshes.Pools[pool].IndexBuffer;
    return nullptr;
}

#ifdef ONYX_ENABLE_OBJ_LOAD
template <Dimension D> Result<StatMeshData<D>> LoadStaticMeshFromObjFile(const char *path, const LoadObjDataFlags flags)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, nullptr, &warn, &err, path))
        return Result<StatMeshData<D>>::Error(Error_FileNotFound,
                                              TKit::Format("[ONYX][ASSETS] Failed to load mesh: {}", err + warn));
    TKIT_LOG_WARNING_IF(!warn.empty(), "[ONYX][ASSETS] {}", warn);
    if (!err.empty())
        return Result<StatMeshData<D>>::Error(Error_LoadFailed,
                                              TKit::Format("[ONYX][ASSETS] Failed to load mesh: {}", err));
    if (shapes.empty())
        return Result<StatMeshData<D>>::Error(Error_LoadFailed,
                                              "[ONYX][ASSETS] Failed to load mesh. Shapes container was empty");

    std::unordered_map<StatVertex<D>, Index> uniqueVertices;
    StatMeshData<D> data;

    const u32 vcount = u32(attrib.vertices.size());

    data.Vertices.Reserve(vcount);
    data.Indices.Reserve(vcount);
    for (const auto &shape : shapes)
        for (const auto &index : shape.mesh.indices)
        {
            StatVertex<D> vertex{};
            for (Index i = 0; i < D; ++i)
                vertex.Position[i] = attrib.vertices[3 * index.vertex_index + i];
            if constexpr (D == D3)
                for (Index i = 0; i < 3; ++i)
                    vertex.Normal[i] = attrib.normals[3 * index.normal_index + i];

            if (!uniqueVertices.contains(vertex))
            {
                uniqueVertices[vertex] = Index(uniqueVertices.size());
                data.Vertices.Append(vertex);
            }
            data.Indices.Append(uniqueVertices[vertex]);
        }
    if (flags & LoadObjDataFlag_CenterVerticesAroundOrigin)
    {
        f32v<D> center{0.f};
        for (const StatVertex<D> &vx : data.Vertices)
            center += vx.Position;
        center /= data.Vertices.GetSize();
        for (StatVertex<D> &vx : data.Vertices)
            vx.Position -= center;
    }
    return data;
}
#endif

#ifdef ONYX_ENABLE_GLTF_LOAD
static VkFormat getFormat(const i32 components, const i32 pixelType, const bool isSrgb)
{
    switch (pixelType)
    {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        switch (components)
        {
        case 1:
            return isSrgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
        case 2:
            return isSrgb ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
        case 3:
            return isSrgb ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
        case 4:
            return isSrgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        switch (components)
        {
        case 1:
            return VK_FORMAT_R16_UNORM;
        case 2:
            return VK_FORMAT_R16G16_UNORM;
        case 3:
            return VK_FORMAT_R16G16B16_UNORM;
        case 4:
            return VK_FORMAT_R16G16B16A16_UNORM;
        }
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        switch (components)
        {
        case 1:
            return VK_FORMAT_R32_SFLOAT;
        case 2:
            return VK_FORMAT_R32G32_SFLOAT;
        case 3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        }
    default:
        return VK_FORMAT_UNDEFINED;
    }
}
template <Dimension D>
Result<GltfAssets<D>> LoadGltfAssetsFromFile(const std::string &path, const LoadGltfDataFlags flags)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string warn, err;

    loader.SetPreserveImageChannels(!(flags & LoadGltfDataFlag_ForceRGBA));
    const bool ok = path.ends_with(".bin") || path.ends_with(".glb")
                        ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
                        : loader.LoadASCIIFromFile(&model, &err, &warn, path);
    TKIT_LOG_WARNING_IF(!warn.empty(), "[ONYX][ASSETS] {}", warn);
    if (!ok)
        return Result<GltfAssets<D>>::Error(Error_LoadFailed,
                                            TKit::Format("[ONYX][ASSETS] Failed to load gltf: {}", err));
    GltfAssets<D> assets{};
    for (const auto &mesh : model.meshes)
    {
        std::unordered_map<i32, u32> primToMeshIndex;
        for (const auto &prim : mesh.primitives)
        {
            const i32 posAccessorIdx = prim.attributes.at("POSITION");
            if (primToMeshIndex.contains(posAccessorIdx))
                continue;

            StatMeshData<D> meshData{};

            u32 posStride;
            u32 normStride;
            u32 uvStride;
            u32 tanStride;

            const auto &posAccessor = model.accessors[posAccessorIdx];
            const auto &posView = model.bufferViews[posAccessor.bufferView];
            const auto &posBuf = model.buffers[posView.buffer];
            const f32 *positions = rcast<const f32 *>(posBuf.data.data() + posView.byteOffset + posAccessor.byteOffset);

            posStride = posView.byteStride ? posView.byteStride / sizeof(f32) : 3;
            const f32 *normals = nullptr;
            if (prim.attributes.contains("NORMAL"))
            {
                const auto &normAccessor = model.accessors[prim.attributes.at("NORMAL")];
                const auto &normView = model.bufferViews[normAccessor.bufferView];
                normals = rcast<const f32 *>(model.buffers[normView.buffer].data.data() + normView.byteOffset +
                                             normAccessor.byteOffset);
                normStride = normView.byteStride ? normView.byteStride / sizeof(f32) : 3;
            }

            const f32 *texcoords = nullptr;
            if (prim.attributes.contains("TEXCOORD_0"))
            {
                const auto &uvAccessor = model.accessors[prim.attributes.at("TEXCOORD_0")];
                const auto &uvView = model.bufferViews[uvAccessor.bufferView];
                texcoords = rcast<const f32 *>(model.buffers[uvView.buffer].data.data() + uvView.byteOffset +
                                               uvAccessor.byteOffset);
                uvStride = uvView.byteStride ? uvView.byteStride / sizeof(f32) : 2;
            }

            const f32 *tangents = nullptr;
            if constexpr (D == D3)
            {
                if (prim.attributes.contains("TANGENT"))
                {
                    const auto &tanAccessor = model.accessors[prim.attributes.at("TANGENT")];
                    const auto &tanView = model.bufferViews[tanAccessor.bufferView];
                    tangents = rcast<const f32 *>(model.buffers[tanView.buffer].data.data() + tanView.byteOffset +
                                                  tanAccessor.byteOffset);
                    tanStride = tanView.byteStride ? tanView.byteStride / sizeof(f32) : 4;
                }
            }

            const u32 vertexCount = u32(posAccessor.count);
            for (u32 i = 0; i < vertexCount; ++i)
            {
                StatVertex<D> vertex{};
                for (u32 j = 0; j < D; ++j)
                    vertex.Position[j] = positions[posStride * i + j];
                if constexpr (D == D3)
                {
                    if (normals)
                        for (u32 j = 0; j < 3; ++j)
                            vertex.Normal[j] = normals[normStride * i + j];
                    if (tangents)
                        for (u32 j = 0; j < 4; ++j)
                            vertex.Tangent[j] = tangents[tanStride * i + j];
                }
                if (texcoords)
                    for (u32 j = 0; j < 2; ++j)
                        vertex.TexCoord[j] = texcoords[uvStride * i + j];

                meshData.Vertices.Append(vertex);
            }
            if (flags & LoadGltfDataFlag_CenterVerticesAroundOrigin)
            {
                f32v<D> center{0.f};
                for (const StatVertex<D> &vx : meshData.Vertices)
                    center += vx.Position;
                center /= meshData.Vertices.GetSize();
                for (StatVertex<D> &vx : meshData.Vertices)
                    vx.Position -= center;
            }

            const auto &idxAccessor = model.accessors[prim.indices];
            const auto &idxView = model.bufferViews[idxAccessor.bufferView];
            const u8 *idxBuf = model.buffers[idxView.buffer].data.data() + idxView.byteOffset + idxAccessor.byteOffset;
            const u32 indexCount = u32(idxAccessor.count);
            for (u32 i = 0; i < indexCount; ++i)
            {
                switch (idxAccessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    meshData.Indices.Append(Index(idxBuf[i]));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    meshData.Indices.Append(Index(rcast<const u16 *>(idxBuf)[i]));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    meshData.Indices.Append(Index(rcast<const u32 *>(idxBuf)[i]));
                    break;
                }
            }
            assets.StaticMeshes.Append(meshData);
        }
    }

    std::unordered_set<u32> srgbTexs;
    for (const auto &mat : model.materials)
    {
        const i32 albedoIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
        const i32 emissiveIdx = mat.emissiveTexture.index;
        if (albedoIdx >= 0)
            srgbTexs.insert(u32(model.textures[albedoIdx].source));
        if (emissiveIdx >= 0)
            srgbTexs.insert(u32(model.textures[emissiveIdx].source));

        if constexpr (D == D2)
        {
            MaterialData<D2> matData{};
            const auto &pbr = mat.pbrMetallicRoughness;
            const auto &color = pbr.baseColorFactor;
            const u8 r = u8(color[0] * 255.f);
            const u8 g = u8(color[1] * 255.f);
            const u8 b = u8(color[2] * 255.f);
            const u8 a = u8(color[3] * 255.f);
            matData.ColorFactor = (a << 24) | (b << 16) | (g << 8) | r; // ABGR packed

            const i32 texIdx = pbr.baseColorTexture.index;
            if (texIdx >= 0)
            {
                matData.Sampler = Asset(model.textures[texIdx].sampler);
                matData.Texture = Asset(model.textures[texIdx].source);
            }

            assets.Materials.Append(matData);
        }
        else
        {
            MaterialData<D3> matData{};
            const auto &pbr = mat.pbrMetallicRoughness;

            const auto &color = pbr.baseColorFactor;
            const u8 r = u8(color[0] * 255.f);
            const u8 g = u8(color[1] * 255.f);
            const u8 b = u8(color[2] * 255.f);
            const u8 a = u8(color[3] * 255.f);
            matData.AlbedoFactor = (a << 24) | (b << 16) | (g << 8) | r;

            matData.MetallicFactor = f32(pbr.metallicFactor);
            matData.RoughnessFactor = f32(pbr.roughnessFactor);
            matData.NormalScale = f32(mat.normalTexture.scale);
            matData.OcclusionStrength = f32(mat.occlusionTexture.strength);

            const auto &emissive = mat.emissiveFactor;
            matData.EmissiveFactor = f32v3{f32(emissive[0]), f32(emissive[1]), f32(emissive[2])};

            const i32 albedoIdx = pbr.baseColorTexture.index;
            if (albedoIdx >= 0)
            {
                matData.Samplers[TextureSlot_Albedo] = Asset(model.textures[albedoIdx].sampler);
                matData.Textures[TextureSlot_Albedo] = Asset(model.textures[albedoIdx].source);
            }

            const i32 mrIdx = pbr.metallicRoughnessTexture.index;
            if (mrIdx >= 0)
            {
                matData.Samplers[TextureSlot_MetallicRoughness] = Asset(model.textures[mrIdx].sampler);
                matData.Textures[TextureSlot_MetallicRoughness] = Asset(model.textures[mrIdx].source);
            }

            const i32 normalIdx = mat.normalTexture.index;
            if (normalIdx >= 0)
            {
                matData.Samplers[TextureSlot_Normal] = Asset(model.textures[normalIdx].sampler);
                matData.Textures[TextureSlot_Normal] = Asset(model.textures[normalIdx].source);
            }

            const i32 occlusionIdx = mat.occlusionTexture.index;
            if (occlusionIdx >= 0)
            {
                matData.Samplers[TextureSlot_Occlusion] = Asset(model.textures[occlusionIdx].sampler);
                matData.Textures[TextureSlot_Occlusion] = Asset(model.textures[occlusionIdx].source);
            }

            const i32 emissiveIdx = mat.emissiveTexture.index;
            if (emissiveIdx >= 0)
            {
                matData.Samplers[TextureSlot_Emissive] = Asset(model.textures[emissiveIdx].sampler);
                matData.Textures[TextureSlot_Emissive] = Asset(model.textures[emissiveIdx].source);
            }

            assets.Materials.Append(matData);
        }
    }

    for (const auto &sampler : model.samplers)
    {
        SamplerData samplerData{};

        switch (sampler.minFilter)
        {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            samplerData.MinFilter = SamplerFilter_Nearest;
            samplerData.Mode = SamplerMode_Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            samplerData.MinFilter = SamplerFilter_Linear;
            samplerData.Mode = SamplerMode_Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            samplerData.MinFilter = SamplerFilter_Nearest;
            samplerData.Mode = SamplerMode_Linear;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        default:
            samplerData.MinFilter = SamplerFilter_Linear;
            samplerData.Mode = SamplerMode_Linear;
            break;
        }

        switch (sampler.magFilter)
        {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            samplerData.MagFilter = SamplerFilter_Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        default:
            samplerData.MagFilter = SamplerFilter_Linear;
            break;
        }

        const auto toWrap = [](const i32 wrap) -> SamplerWrap {
            switch (wrap)
            {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                return SamplerWrap_ClampToEdge;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                return SamplerWrap_MirroredRepeat;
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
            default:
                return SamplerWrap_Repeat;
            }
        };

        samplerData.WrapU = toWrap(sampler.wrapS);
        samplerData.WrapV = toWrap(sampler.wrapT);
        assets.Samplers.Append(samplerData);
    }

    for (u32 i = 0; i < u32(model.images.size()); ++i)
    {
        const auto &image = model.images[i];
        TextureData tex{};
        tex.Width = u32(image.width);
        tex.Height = u32(image.height);
        tex.Components = u32(image.component);
        tex.Format = getFormat(image.component, image.pixel_type, srgbTexs.contains(i));

        const usz size = tex.ComputeSize();
        tex.Data = scast<std::byte *>(TKit::Allocate(size));
        TKIT_ASSERT(
            size == image.image.size(),
            "[ONYX][ASSETS] Texture size mismatch between gltf ({:L}) and computed size based on components and "
            "format ({:L})",
            image.image.size(), size);
        TKIT_ASSERT(tex.Data, "[ONYX][ASSETS] Failed to allocate texture data of size {:L} bytes", tex.ComputeSize());

        TKit::ForwardCopy(tex.Data, image.image.data(), size);
        assets.Textures.Append(tex);
    }

    return assets;
}
Result<TextureData> LoadTextureDataFromImageFile(const char *path, const ImageComponent requiredComponents,
                                                 const LoadTextureDataFlags flags)
{
    TextureData data{};
    i32 w;
    i32 h;
    i32 c;

    i32 pixelType;
    void *img = nullptr;

    if (stbi_is_hdr(path))
    {
        pixelType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        img = stbi_loadf(path, &w, &h, &c, i32(requiredComponents));
    }
    else if (stbi_is_16_bit(path))
    {
        pixelType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        img = stbi_load_16(path, &w, &h, &c, i32(requiredComponents));
    }
    else
    {
        pixelType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
        img = stbi_load(path, &w, &h, &c, i32(requiredComponents));
    }

    if (!img)
        return Result<TextureData>::Error(
            Error_LoadFailed, TKit::Format("[ONYX][ASSETS] Failed to load image data: {}", stbi_failure_reason()));

    data.Width = u32(w);
    data.Height = u32(h);
    data.Components = requiredComponents > 0 ? requiredComponents : u32(c);
    data.Format = getFormat(i32(data.Components), pixelType, !(flags & LoadTextureDataFlag_AsLinearImage));
    const usz size = data.ComputeSize();

    data.Data = scast<std::byte *>(TKit::Allocate(size));
    TKIT_ASSERT(data.Data, "[ONYX][ASSETS] Failed to allocate image data with size {:L} bytes", size);
    TKit::ForwardCopy(data.Data, img, size);
    stbi_image_free(img);
    return data;
}
#endif

template <typename Vertex> static void removeUnusedVertices(MeshData<Vertex> &data, const u32 offset = 0)
{
    TKit::StackArray<u32> counts{};
    counts.Resize(data.Vertices.GetSize(), 0);

    for (const Index i : data.Indices)
        counts[i - Index(offset)]++;
    for (u32 i = counts.GetSize() - 1; i < counts.GetSize(); --i)
        if (counts[i] == 0)
        {
            data.Vertices.RemoveOrdered(data.Vertices.begin() + i);
            for (Index &j : data.Indices)
                if (j > i)
                    --j;
        }
}

#ifdef TKIT_ENABLE_ASSERTS
template <typename Vertex> static void validateMesh(MeshData<Vertex> &data, const u32 offset = 0)
{
    Index mx = 0;
    for (const Index i : data.Indices)
        if (i > mx)
            mx = i;

    mx -= Index(offset);
    TKIT_ASSERT(mx < data.Vertices.GetSize(),
                "[ONYX][ASSETS] Index and vertex host data creation is invalid. An index exceeds vertex bounds. Index: "
                "{}, size: {}",
                mx, data.Vertices.GetSize());
    removeUnusedVertices(data, offset);
    TKit::StackArray<u32> counts{};
    counts.Resize(data.Vertices.GetSize(), 0);

    for (const Index i : data.Indices)
        counts[i - Index(offset)]++;
    for (const u32 c : counts)
    {
        TKIT_ASSERT(c != 0, "[ONYX][ASSETS] Found unused vertices in a mesh");
    }
}
#    define VALIDATE_MESH(...) validateMesh(__VA_ARGS__)
#else
#    define VALIDATE_MESH(...) removeUnusedVertices(__VA_ARGS__)
#endif

template <Dimension D> StatMeshData<D> CreateTriangleMesh()
{
    StatMeshData<D> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 u, const f32 v) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{f32v2{x, y}, f32v2{u, v}});
        else
            data.Vertices.Append(
                StatVertex<D3>{f32v3{x, y, 0.f}, f32v2{u, v}, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };

    addVertex(0.f, 0.5f, 0.5f, 1.f);
    addVertex(-0.433013f, -0.25f, 0.f, 0.f);
    addVertex(0.433013f, -0.25f, 1.f, 0.f);

    addIndex(0);
    addIndex(1);
    addIndex(2);
    VALIDATE_MESH(data);
    return data;
}
template <Dimension D> StatMeshData<D> CreateQuadMesh()
{
    StatMeshData<D> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 u, const f32 v) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{f32v2{x, y}, f32v2{u, v}});
        else
            data.Vertices.Append(
                StatVertex<D3>{f32v3{x, y, 0.f}, f32v2{u, v}, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };

    addVertex(-0.5f, -0.5f, 0.f, 1.f);
    addVertex(0.5f, -0.5f, 1.f, 1.f);
    addVertex(-0.5f, 0.5f, 0.f, 0.f);
    addVertex(0.5f, 0.5f, 1.f, 0.f);

    addIndex(0);
    addIndex(1);
    addIndex(2);
    addIndex(1);
    addIndex(3);
    addIndex(2);

    VALIDATE_MESH(data);
    return data;
}

template <Dimension D, bool Inverted = false, bool Counter = true>
static StatMeshData<D> createRegularPolygon(const u32 sides, const f32v<D> &vertexOffset = f32v<D>{0.f},
                                            const u32 indexOffset = 0, const f32v3 &normal = f32v3{0.f, 0.f, 1.f},
                                            const f32v4 &tangent = f32v4{1.f, 0.f, 0.f, 1.f})
{
    TKIT_ASSERT(sides >= 3, "[ONYX][ASSETS] A regular polygon requires at least 3 sides");
    StatMeshData<D> data{};
    const auto addVertex = [&](const f32v<D> &vertex, const f32v2 &uv) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{vertex + vertexOffset, uv});
        else
            data.Vertices.Append(StatVertex<D3>{vertex + vertexOffset, uv, normal, tangent});
    };
    const auto addIndex = [&data, indexOffset](const u32 index) { data.Indices.Append(Index(index + indexOffset)); };

    addVertex(f32v<D>{0.f}, f32v2{0.5f});
    const f32 angle = 2.f * Math::Pi<f32>() / sides;
    for (u32 i = 0; i < sides; ++i)
    {
        const f32 c = 0.5f * Math::Cosine(i * angle);
        const f32 s = 0.5f * Math::Sine(i * angle);
        const f32v2 uv{c + 0.5f, s + 0.5f};
        if constexpr (D == D2)
            addVertex(f32v2{c, s}, uv);
        else if constexpr (Inverted)
            addVertex(f32v3{c, 0.f, s}, uv);
        else
            addVertex(f32v3{c, s, 0.f}, uv);
        if constexpr (Counter)
        {
            addIndex(0);
            addIndex(i + 1);

            const u32 idx = i + 2;
            addIndex(idx > sides ? 1 : idx);
        }
        else
        {
            addIndex(0);
            const u32 idx = i + 2;
            addIndex(idx > sides ? 1 : idx);
            addIndex(i + 1);
        }
    }
    VALIDATE_MESH(data, indexOffset);
    return data;
}
template <Dimension D> StatMeshData<D> CreateRegularPolygonMesh(const u32 sides)
{
    return createRegularPolygon<D>(sides);
}

template <Dimension D> StatMeshData<D> CreatePolygonMesh(const TKit::Span<const f32v2> vertices)
{
    TKIT_ASSERT(vertices.GetSize() >= 3, "[ONYX][ASSETS] A polygon must have at least 3 vertices");
    StatMeshData<D> data{};

    const auto addVertex = [&data](const f32v2 &vertex, const f32v2 &uv) {
        if constexpr (D == D2)
            data.Vertices.Append(StatVertex<D2>{vertex, uv});
        else
            data.Vertices.Append(
                StatVertex<D3>{f32v3{vertex, 0.f}, uv, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };

    f32v2 minv = vertices[0], maxv = vertices[0];
    for (const f32v2 &v : vertices)
    {
        minv = Math::Min(minv, v);
        maxv = Math::Max(maxv, v);
    }
    const f32v2 range = maxv - minv;
    const auto toTex = [&](const f32v2 &v) -> f32v2 {
        return (range[0] > 0.f && range[1] > 0.f) ? (v - minv) / range : f32v2{0.5f};
    };

    addVertex(f32v<D>{0.f}, f32v2{0.5f});
    const u32 size = vertices.GetSize();
    for (u32 i = 0; i < size; ++i)
    {
        const f32v2 &vertex = vertices[i];
        addVertex(vertex, toTex(vertex));
        addIndex(0);
        addIndex(i + 1);

        const u32 idx = i + 2;
        addIndex(idx > size ? 1 : idx);
    }
    return data;
}

static StatMeshData<D3> createBoxMesh(const u32 offset = 0, const f32 push = 0.5f)
{
    StatMeshData<D3> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 n0, const f32 n1, const f32 n2,
                                   const f32 u, const f32 v, const f32v4 &tangent) {
        data.Vertices.Append(StatVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, f32v3{n0, n1, n2}, tangent});
    };
    const auto addQuad = [&data, offset](const Index a, const Index b, const Index c, const Index d) {
        data.Indices.Append(a + offset);
        data.Indices.Append(b + offset);
        data.Indices.Append(c + offset);
        data.Indices.Append(a + offset);
        data.Indices.Append(c + offset);
        data.Indices.Append(d + offset);
    };
    const f32v4 tPosZ{0.f, 0.f, 1.f, 1.f};  // -X face: U goes along +Z
    const f32v4 tPosX{1.f, 0.f, 0.f, 1.f};  // +Z face: U goes along +X
    const f32v4 tNegZ{0.f, 0.f, -1.f, 1.f}; // +X face: U goes along -Z
    const f32v4 tNegX{-1.f, 0.f, 0.f, 1.f}; // -Z face: U goes along -X
    const f32v4 tTopX{1.f, 0.f, 0.f, 1.f};  // +Y face: U goes along +X
    const f32v4 tBotX{-1.f, 0.f, 0.f, 1.f}; // -Y face: U goes along -X

    Index base = 0;
    addVertex(-push, 0.5f, -0.5f, -1.f, 0.f, 0.f, 0.f, 0.f, tPosZ);
    addVertex(-push, -0.5f, -0.5f, -1.f, 0.f, 0.f, 0.f, 1.f, tPosZ);
    addVertex(-push, -0.5f, 0.5f, -1.f, 0.f, 0.f, 1.f, 1.f, tPosZ);
    addVertex(-push, 0.5f, 0.5f, -1.f, 0.f, 0.f, 1.f, 0.f, tPosZ);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(-0.5f, 0.5f, push, 0.f, 0.f, 1.f, 0.f, 0.f, tPosX);
    addVertex(-0.5f, -0.5f, push, 0.f, 0.f, 1.f, 0.f, 1.f, tPosX);
    addVertex(0.5f, -0.5f, push, 0.f, 0.f, 1.f, 1.f, 1.f, tPosX);
    addVertex(0.5f, 0.5f, push, 0.f, 0.f, 1.f, 1.f, 0.f, tPosX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(push, 0.5f, 0.5f, 1.f, 0.f, 0.f, 0.f, 0.f, tNegZ);
    addVertex(push, -0.5f, 0.5f, 1.f, 0.f, 0.f, 0.f, 1.f, tNegZ);
    addVertex(push, -0.5f, -0.5f, 1.f, 0.f, 0.f, 1.f, 1.f, tNegZ);
    addVertex(push, 0.5f, -0.5f, 1.f, 0.f, 0.f, 1.f, 0.f, tNegZ);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(0.5f, 0.5f, -push, 0.f, 0.f, -1.f, 0.f, 0.f, tNegX);
    addVertex(0.5f, -0.5f, -push, 0.f, 0.f, -1.f, 0.f, 1.f, tNegX);
    addVertex(-0.5f, -0.5f, -push, 0.f, 0.f, -1.f, 1.f, 1.f, tNegX);
    addVertex(-0.5f, 0.5f, -push, 0.f, 0.f, -1.f, 1.f, 0.f, tNegX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(-0.5f, push, 0.5f, 0.f, 1.f, 0.f, 0.f, 0.f, tTopX);
    addVertex(0.5f, push, 0.5f, 0.f, 1.f, 0.f, 1.f, 0.f, tTopX);
    addVertex(0.5f, push, -0.5f, 0.f, 1.f, 0.f, 1.f, 1.f, tTopX);
    addVertex(-0.5f, push, -0.5f, 0.f, 1.f, 0.f, 0.f, 1.f, tTopX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(0.5f, -push, 0.5f, 0.f, -1.f, 0.f, 0.f, 0.f, tBotX);
    addVertex(-0.5f, -push, 0.5f, 0.f, -1.f, 0.f, 1.f, 0.f, tBotX);
    addVertex(-0.5f, -push, -0.5f, 0.f, -1.f, 0.f, 1.f, 1.f, tBotX);
    addVertex(0.5f, -push, -0.5f, 0.f, -1.f, 0.f, 0.f, 1.f, tBotX);
    addQuad(base + 0, base + 1, base + 2, base + 3);

    VALIDATE_MESH(data, offset);
    return data;
}

StatMeshData<D3> CreateBoxMesh()
{
    return createBoxMesh();
}
StatMeshData<D3> CreateSphereMesh(u32 rings, const u32 sectors)
{
    rings += 2;
    StatMeshData<D3> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                   const f32v4 &tangent) {
        const f32v3 vertex = f32v3{x, y, z};
        data.Vertices.Append(StatVertex<D3>{vertex, f32v2{u, v}, Math::Normalize(vertex), tangent});
    };
    const auto addIndex = [&data, sectors, rings](const u32 ring, const u32 sector) {
        u32 idx;
        if (ring == 0)
            idx = sector;
        else if (ring == rings - 1)
            idx = sectors + (rings - 2) * (sectors + 1) + sector;
        else
            idx = sectors + sector + (ring - 1) * (sectors + 1);
        data.Indices.Append(Index(idx));
    };

    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 u = (f32(j) + 0.5f) / sectors;
        addVertex(0.f, 0.5f, 0.f, u, 0.f, f32v4{1.f, 0.f, 0.f, 1.f});
    }
    for (u32 i = 1; i < rings - 1; ++i)
    {
        const f32 v = f32(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        const f32 y = 0.5f * pc;
        for (u32 j = 0; j < sectors; ++j)
        {
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            addVertex(0.5f * ps * tc, y, 0.5f * ps * ts, u, v, f32v4{-ts, 0.f, tc, 1.f});

            const u32 ii = i - 1;
            const u32 jj = j + 1;
            addIndex(i, jj);
            addIndex(i, j);
            addIndex(ii, j);
            if (i != 1)
            {
                addIndex(i, jj);
                addIndex(ii, j);
                addIndex(ii, jj);
            }
        }
        addVertex(0.5f * ps, 0.5f * pc, 0.f, 1.0f, v, f32v4{0.f, 0.f, 1.f, 1.f});
    }
    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 u = (f32(j) + 0.5f) / sectors;
        addVertex(0.f, -0.5f, 0.f, u, 1.f, f32v4{1.f, 0.f, 0.f, 1.f});
    }

    for (u32 j = 0; j < sectors; ++j)
    {
        addIndex(rings - 2, j);
        addIndex(rings - 2, j + 1);
        addIndex(rings - 1, j);
    }

    VALIDATE_MESH(data);
    return data;
}
StatMeshData<D3> CreateCylinderMesh(const u32 sides)
{
    const StatMeshData<D3> left = createRegularPolygon<D3, true, true>(
        sides, f32v3{0.f, -0.5f, 0.f}, 0, f32v3{0.f, -1.f, 0.f}, f32v4{0.f, 0.f, 1.f, 1.f});

    const StatMeshData<D3> right = createRegularPolygon<D3, true, false>(
        sides, f32v3{0.f, 0.5f, 0.f}, left.Vertices.GetSize(), f32v3{0.f, 1.f, 0.f}, f32v4{0.f, 0.f, 1.f, 1.f});

    StatMeshData<D3> data{};
    data.Indices.Insert(data.Indices.end(), left.Indices.begin(), left.Indices.end());
    data.Indices.Insert(data.Indices.end(), right.Indices.begin(), right.Indices.end());

    data.Vertices.Insert(data.Vertices.end(), left.Vertices.begin(), left.Vertices.end());
    data.Vertices.Insert(data.Vertices.end(), right.Vertices.begin(), right.Vertices.end());

    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                   const f32v4 &tangent) {
        data.Vertices.Append(StatVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, Math::Normalize(f32v3{x, 0.f, z}), tangent});
    };
    const u32 offset = left.Vertices.GetSize() + right.Vertices.GetSize();
    const auto addIndex = [&data, offset](const u32 index) { data.Indices.Append(Index(index + offset)); };

    const f32 angle = 2.f * Math::Pi<f32>() / sides;
    for (u32 i = 0; i < sides; ++i)
    {
        const f32 cc = Math::Cosine(i * angle);
        const f32 ss = Math::Sine(i * angle);

        const f32v4 tangent = f32v4{-ss, 0.f, cc, 1.f};

        const f32 x = 0.5f * cc;
        const f32 z = 0.5f * ss;

        const f32 u = f32(i) / sides;
        addVertex(x, -0.5f, z, u, 1.f, tangent);
        addVertex(x, 0.5f, z, u, 0.f, tangent);

        const u32 ii = 2 * i;
        addIndex(ii);
        addIndex(ii + 1);
        addIndex(ii + 2);

        addIndex(ii + 1);
        addIndex(ii + 3);
        addIndex(ii + 2);
    }
    addVertex(0.5f, -0.5f, 0.f, 1.f, 1.f, f32v4{0.f, 0.f, 1.f, 1.f});
    addVertex(0.5f, 0.5f, 0.f, 1.f, 0.f, f32v4{0.f, 0.f, 1.f, 1.f});

    VALIDATE_MESH(data);
    return data;
}

template <Dimension D> ParaMeshData<D> CreateStadiumMesh()
{
    ParaMeshData<D> data{};
    data.Shape = ParametricShape_Stadium;
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 u, const f32 v,
                                   const ParametricRegionFlags region = 0) {
        if constexpr (D == D2)
            data.Vertices.Append(ParaVertex<D2>{f32v2{x, y}, f32v2{u, v}, region});
        else
            data.Vertices.Append(
                ParaVertex<D3>{f32v3{x, y, 0.f}, f32v2{u, v}, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}, region});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };
    constexpr u32 edge = StadiumRegion_Edge;
    constexpr u32 moon = StadiumRegion_Moon;

    addVertex(-0.5f, -0.5f, 0.f, 0.75f);
    addVertex(0.5f, -0.5f, 1.f, 0.75f);
    addVertex(-0.5f, 0.5f, 0.f, 0.25f);
    addVertex(0.5f, 0.5f, 1.f, 0.25f);

    addVertex(-0.5f, -1.f, 0.f, 1.f, edge | moon);
    addVertex(0.5f, -1.f, 1.f, 1.f, edge | moon);
    addVertex(-0.5f, -0.5f, 0.f, 0.75f, moon);
    addVertex(0.5f, -0.5f, 1.f, 0.75f, moon);

    addVertex(-0.5f, 0.5f, 0.f, 0.25f, moon);
    addVertex(0.5f, 0.5f, 1.f, 0.25f, moon);
    addVertex(-0.5f, 1.f, 0.f, 0.f, edge | moon);
    addVertex(0.5f, 1.f, 1.f, 0.f, edge | moon);

    for (u32 i = 0; i < 3; ++i)
    {
        addIndex(0 + i * 4);
        addIndex(1 + i * 4);
        addIndex(2 + i * 4);
        addIndex(1 + i * 4);
        addIndex(3 + i * 4);
        addIndex(2 + i * 4);
    }
    VALIDATE_MESH(data);
    return data;
}

template <Dimension D> ParaMeshData<D> CreateRoundedQuadMesh()
{
    ParaMeshData<D> data{};
    data.Shape = ParametricShape_RoundedQuad;
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 u, const f32 v,
                                   const ParametricRegionFlags region = 0) {
        if constexpr (D == D2)
            data.Vertices.Append(ParaVertex<D2>{f32v2{x, y}, f32v2{u, v}, region});
        else
            data.Vertices.Append(
                ParaVertex<D3>{f32v3{x, y, 0.f}, f32v2{u, v}, f32v3{0.f, 0.f, 1.f}, f32v4{1.f, 0.f, 0.f, 1.f}, region});
    };
    const auto addIndex = [&data](const u32 index) { data.Indices.Append(Index(index)); };

    constexpr u32 hedge = RoundedQuadRegion_HorizontalEdge;
    constexpr u32 vedge = RoundedQuadRegion_VerticalEdge;
    constexpr u32 moon = RoundedQuadRegion_Moon;

    addVertex(-0.5f, -0.5f, 0.25f, 0.75f);
    addVertex(0.5f, -0.5f, 0.75f, 0.75f);
    addVertex(-0.5f, 0.5f, 0.25f, 0.25f);
    addVertex(0.5f, 0.5f, 0.75f, 0.25f);

    addVertex(-1.f, -0.5f, 0.f, 0.75f, hedge);
    addVertex(-0.5f, -0.5f, 0.25f, 0.75f);
    addVertex(-1.f, 0.5f, 0.f, 0.25f, hedge);
    addVertex(-0.5f, 0.5f, 0.25f, 0.25f);

    addVertex(0.5f, -0.5f, 0.75f, 0.75f);
    addVertex(1.f, -0.5f, 1.f, 0.75f, hedge);
    addVertex(0.5f, 0.5f, 0.75f, 0.25f);
    addVertex(1.f, 0.5f, 1.f, 0.25f, hedge);

    addVertex(-0.5f, -0.5f, 0.25f, 0.75f);
    addVertex(-0.5f, -1.f, 0.25f, 1.f, vedge);
    addVertex(0.5f, -0.5f, 0.75f, 0.75f);
    addVertex(0.5f, -1.f, 0.75f, 1.f, vedge);

    addVertex(-0.5f, 1.f, 0.25f, 0.f, vedge);
    addVertex(-0.5f, 0.5f, 0.25f, 0.25f);
    addVertex(0.5f, 1.f, 0.75f, 0.f, vedge);
    addVertex(0.5f, 0.5f, 0.75f, 0.25f);

    addVertex(-1.f, -0.5f, 0.f, 0.75f, moon | hedge);
    addVertex(-1.f, -1.f, 0.f, 1.f, moon | hedge | vedge);
    addVertex(-0.5f, -0.5f, 0.25f, 0.75f, moon);
    addVertex(-0.5f, -1.f, 0.25f, 1.f, moon | vedge);

    addVertex(0.5f, -1.f, 0.75f, 1.f, moon | vedge);
    addVertex(1.f, -1.f, 1.f, 1.f, moon | hedge | vedge);
    addVertex(0.5f, -0.5f, 0.75f, 0.75f, moon);
    addVertex(1.f, -0.5f, 1.f, 0.75f, moon | hedge);

    addVertex(-0.5f, 1.f, 0.25f, 0.f, moon | vedge);
    addVertex(-1.f, 1.f, 0.f, 0.f, moon | hedge | vedge);
    addVertex(-0.5f, 0.5f, 0.25f, 0.25f, moon);
    addVertex(-1.f, 0.5f, 0.f, 0.25f, moon | hedge);

    addVertex(1.f, 0.5f, 1.f, 0.25f, moon | hedge);
    addVertex(1.f, 1.f, 1.f, 0.f, moon | hedge | vedge);
    addVertex(0.5f, 0.5f, 0.75f, 0.25f, moon);
    addVertex(0.5f, 1.f, 0.75f, 0.f, moon | vedge);

    for (u32 i = 0; i < 9; ++i)
    {
        addIndex(0 + i * 4);
        addIndex(1 + i * 4);
        addIndex(2 + i * 4);
        addIndex(1 + i * 4);
        addIndex(3 + i * 4);
        addIndex(2 + i * 4);
    }

    VALIDATE_MESH(data);
    return data;
}

ParaMeshData<D3> CreateCapsuleMesh(u32 rings, const u32 sectors)
{
    rings += 2;
    ParaMeshData<D3> data{};
    data.Shape = ParametricShape_Capsule;

    const auto addCylinderVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                           const f32v4 &tangent) {
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, Math::Normalize(f32v3{x, 0.f, z}), tangent,
                                            CapsuleRegion_Body});
    };
    u32 coffset = 0;
    const auto addSphereVertex = [&data, &coffset](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                                   const f32v4 &tangent) {
        f32v3 normal = f32v3{x, y, z};
        if (y < 0.f)
            normal[1] += 0.5f;
        else
            normal[1] -= 0.5f;
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, Math::Normalize(normal), tangent, 0});
        ++coffset;
    };

    const auto addSphereIndex = [&data, sectors, rings](const u32 ring, const u32 sector) {
        u32 idx;
        if (ring == 0)
            idx = sector;
        else if (ring == rings)
            idx = sectors + (rings - 1) * (sectors + 1) + sector;
        else
            idx = sectors + sector + (ring - 1) * (sectors + 1);
        data.Indices.Append(Index(idx));
    };
    const auto addCylinderIndex = [&data, &coffset](const u32 index) { data.Indices.Append(Index(index + coffset)); };

    const u32 halfRings = rings / 2;

    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 u = (f32(j) + 0.5f) / sectors;
        addSphereVertex(0.f, 1.f, 0.f, u, 0.f, f32v4{1.f, 0.f, 0.f, 1.f});
    }

    for (u32 i = 1; i < halfRings + 1; ++i)
    {
        const f32 v = f32(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        const f32 y = Math::Map(pc, 0.f, 1.f, 0.5f, 1.f);
        const f32 vv = Math::Map(v, 0.f, 0.5f, 0.f, 0.25f);
        for (u32 j = 0; j < sectors; ++j)
        {
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);

            addSphereVertex(0.5f * ps * tc, y, 0.5f * ps * ts, u, vv, f32v4{-ts, 0.f, tc, 1.f});

            const u32 ii = i - 1;
            const u32 jj = j + 1;
            addSphereIndex(i, jj);
            addSphereIndex(i, j);
            addSphereIndex(ii, j);
            if (i != 1)
            {
                addSphereIndex(i, jj);
                addSphereIndex(ii, j);
                addSphereIndex(ii, jj);
            }
        }
        addSphereVertex(0.5f * ps, y, 0.f, 1.f, vv, f32v4{0.f, 0.f, 1.f, 1.f});
    }

    for (u32 i = halfRings; i < rings - 1; ++i)
    {
        const f32 v = f32(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        const f32 y = Math::Map(pc, 0.f, -1.f, -0.5f, -1.f);
        const f32 vv = Math::Map(v, 0.5f, 1.f, 0.75f, 1.f);
        for (u32 j = 0; j < sectors; ++j)
        {
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);

            addSphereVertex(0.5f * ps * tc, y, 0.5f * ps * ts, u, vv, f32v4{-ts, 0.f, tc, 1.f});

            if (i != halfRings)
            {
                const u32 ii = i + 1;
                const u32 jj = j + 1;

                addSphereIndex(ii, jj);
                addSphereIndex(ii, j);
                addSphereIndex(i, j);
                addSphereIndex(ii, jj);
                addSphereIndex(i, j);
                addSphereIndex(i, jj);
            }
        }
        addSphereVertex(0.5f * ps, y, 0.f, 1.f, vv, f32v4{0.f, 0.f, 1.f, 1.f});
    }

    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 u = (f32(j) + 0.5f) / sectors;
        addSphereVertex(0.f, -1.f, 0.f, u, 1.f, f32v4{0.f, 1.f, 0.f, 1.f});
    }
    for (u32 j = 0; j < sectors; ++j)
    {
        addSphereIndex(rings - 1, j);
        addSphereIndex(rings - 1, j + 1);
        addSphereIndex(rings, j);
    }

    const f32 angle = 2.f * Math::Pi<f32>() / sectors;
    for (u32 j = 0; j < sectors; ++j)
    {
        const f32 cc = Math::Cosine(j * angle);
        const f32 ss = Math::Sine(j * angle);

        const f32v4 tangent = f32v4{-ss, 0.f, cc, 1.f};

        const f32 x = 0.5f * cc;
        const f32 z = 0.5f * ss;

        const f32 u = f32(j) / sectors;
        addCylinderVertex(x, -0.5f, z, u, 0.75f, tangent);
        addCylinderVertex(x, 0.5f, z, u, 0.25f, tangent);

        const u32 ii = 2 * j;
        addCylinderIndex(ii);
        addCylinderIndex(ii + 1);
        addCylinderIndex(ii + 2);

        addCylinderIndex(ii + 1);
        addCylinderIndex(ii + 3);
        addCylinderIndex(ii + 2);
    }
    addCylinderVertex(0.5f, -0.5f, 0.f, 1.f, 0.75f, f32v4{0.f, 0.f, 1.f, 1.f});
    addCylinderVertex(0.5f, 0.5f, 0.f, 1.f, 0.25f, f32v4{0.f, 0.f, 1.f, 1.f});

    VALIDATE_MESH(data);
    return data;
}

// from this point on, up to the explicit instantiations, the code is pretty much ia generated with minor modifications
// on my part. i got tired of creating shape code. seeing i had spent more than one day creating the stadium, rounded
// quad and the capsule, i got a bit fed up. in fact, i did do the rounded box geometry, but didnt take into account
// uvs. when i tried to tackle it, i would have rather jumped out of my window. so i asked claude to do it
//
// idk how i feel about it. i want onyx to be mine, and this kind of feels dishonest. i dont know why i feel this way
// for this particular piece of unimportant code. but i guess it is the first time something has been so tedious, i
// decided to have it automatically generated
static f32v2 computeRoundedBoxTexCoords(const f32v3 &pos, const f32v3 &normal) noexcept
{
    const f32 ax = Math::Absolute(normal[0]);
    const f32 ay = Math::Absolute(normal[1]);
    const f32 az = Math::Absolute(normal[2]);

    f32 lu, lv;

    if (ay >= ax && ay >= az)
    {
        // Y-dominant face
        lu = pos[0];
        lv = (normal[1] > 0.f) ? pos[2] : -pos[2];
    }
    else if (ax >= ay && ax >= az)
    {
        // X-dominant face
        lu = (normal[0] > 0.f) ? -pos[2] : pos[2];
        lv = -pos[1];
    }
    else
    {
        // Z-dominant face
        lu = (normal[2] > 0.f) ? pos[0] : -pos[0];
        lv = -pos[1];
    }

    // Position range is [-1, 1], map to [0, 1]
    return f32v2{(lu + 1.f) * 0.5f, (lv + 1.f) * 0.5f};
}

static f32v2 computeRoundedBoxTexCoords_Forced(const f32v3 &pos, const u32 axis) noexcept
{
    f32 lu, lv;
    switch (axis)
    {
    case 0:
        lu = -pos[2];
        lv = -pos[1];
        break; // +X
    case 1:
        lu = pos[2];
        lv = -pos[1];
        break; // -X
    case 2:
        lu = pos[0];
        lv = pos[2];
        break; // +Y
    case 3:
        lu = pos[0];
        lv = -pos[2];
        break; // -Y
    case 4:
        lu = pos[0];
        lv = -pos[1];
        break; // +Z
    case 5:
        lu = -pos[0];
        lv = -pos[1];
        break; // -Z
    default:
        lu = lv = 0.f;
        break;
    }
    return f32v2{(lu + 1.f) * 0.5f, (lv + 1.f) * 0.5f};
}

static u32 chooseFace_02(const f32v3 &pos, const f32 sx, const f32 sz) noexcept
{
    f32v3 n = pos;
    for (u32 i = 0; i < 3; ++i)
        if (n[i] > 0.f)
            n[i] -= 0.5f;
        else
            n[i] += 0.5f;

    const f32 ax = Math::Absolute(n[0]);
    const f32 ay = Math::Absolute(n[1]);
    const f32 az = Math::Absolute(n[2]);

    if (ay > ax && ay > az)
        return (n[1] > 0.f) ? 2u : 3u; // +Y / -Y
    if (az >= ax)
        return (sz > 0.f) ? 4u : 5u; // +Z / -Z
    return (sx > 0.f) ? 0u : 1u;     // +X / -X
}

static f32v4 getFaceTangent(const u32 p_axis) noexcept
{
    switch (p_axis)
    {
    case 0:
        return f32v4{0.f, 0.f, -1.f, 1.f}; // +X
    case 1:
        return f32v4{0.f, 0.f, 1.f, 1.f}; // -X
    case 2:
        return f32v4{1.f, 0.f, 0.f, 1.f}; // +Y
    case 3:
        return f32v4{1.f, 0.f, 0.f, 1.f}; // -Y
    case 4:
        return f32v4{1.f, 0.f, 0.f, 1.f}; // +Z
    case 5:
        return f32v4{-1.f, 0.f, 0.f, 1.f}; // -Z
    default:
        return f32v4{1.f, 0.f, 0.f, 1.f};
    }
}

ParaMeshData<D3> CreateRoundedBoxMesh(u32 rings, const u32 sectors)
{
    rings += 2;
    ParaMeshData<D3> data{};
    data.Shape = ParametricShape_RoundedBox;

    const auto addCylinderVertex02 = [&data](const f32 x, const f32 y, const f32 z, const u32 face) {
        f32v3 normal = f32v3{x, 0.f, z};
        if (x > 0.f)
            normal[0] -= 0.5f;
        else
            normal[0] += 0.5f;
        if (z > 0.f)
            normal[2] -= 0.5f;
        else
            normal[2] += 0.5f;
        const f32v3 n = Math::Normalize(normal);
        const f32v2 uv = computeRoundedBoxTexCoords_Forced(f32v3{x, y, z}, face);
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, uv, n, getFaceTangent(face), 0});
    };

    const auto addCylinderVertex12 = [&data](const f32 x, const f32 y, const f32 z, const u32 face) {
        f32v3 normal = f32v3{0, y, z};
        if (y > 0.f)
            normal[1] -= 0.5f;
        else
            normal[1] += 0.5f;
        if (z > 0.f)
            normal[2] -= 0.5f;
        else
            normal[2] += 0.5f;
        const f32v3 n = Math::Normalize(normal);
        const f32v2 uv = computeRoundedBoxTexCoords_Forced(f32v3{x, y, z}, face);
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, uv, n, getFaceTangent(face), 0});
    };

    const auto addCylinderVertex01 = [&data](const f32 x, const f32 y, const f32 z, const u32 face) {
        f32v3 normal = f32v3{x, y, 0.f};
        if (x > 0.f)
            normal[0] -= 0.5f;
        else
            normal[0] += 0.5f;
        if (y > 0.f)
            normal[1] -= 0.5f;
        else
            normal[1] += 0.5f;
        const f32v3 n = Math::Normalize(normal);
        const f32v2 uv = computeRoundedBoxTexCoords_Forced(f32v3{x, y, z}, face);
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, uv, n, getFaceTangent(face), 0});
    };

    const auto addSphereVertex = [&data](const f32 x, const f32 y, const f32 z, const u32 face) {
        f32v3 normal = f32v3{x, y, z};
        for (u32 i = 0; i < 3; ++i)
            if (normal[i] > 0.f)
                normal[i] -= 0.5f;
            else
                normal[i] += 0.5f;
        const f32v3 n = Math::Normalize(normal);
        const f32v2 uv = computeRoundedBoxTexCoords_Forced(f32v3{x, y, z}, face);
        data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, uv, n, getFaceTangent(face), 0});
    };

    const u32 quartSectors = sectors / 4;
    u32 offset = 0;
    const auto addCylinderIndex = [&data, &offset](const u32 index) { data.Indices.Append(Index(index + offset)); };

    const u32 halfRings = rings / 2;

    const auto addEdge02 = [&](const f32 sx, const f32 sz) {
        const f32 mnx = 0.5f * sx;
        const f32 mxx = sx;
        const f32 mnz = 0.5f * sz;
        const f32 mxz = sz;

        const u32 yFaceTop = 2u; // +Y
        const u32 yFaceBot = 3u; // -Y
        const u32 zFace = (sz > 0.f) ? 4u : 5u;
        const u32 xFace = (sx > 0.f) ? 0u : 1u;

        const u32 halfQuart = quartSectors / 2;

        const auto computeUpperVertex = [&](const u32 i, const u32 j) -> f32v3 {
            const f32 v = f32(i) / rings;
            const f32 phi = v * Math::Pi<f32>();
            const f32 pc = Math::Cosine(phi);
            const f32 ps = Math::Sine(phi);
            const f32 tx = Math::Map(ps, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(pc, 0.f, 1.f, 0.5f, 1.f);
            const f32 tz = Math::Map(ps, 0.f, 1.f, mnz, mxz);
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();
            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            const f32 x = Math::Map(ps * tc, ps, 0.f, tx, sx * 0.5f);
            const f32 z = Math::Map(ps * ts, ps, 0.f, tz, sz * 0.5f);
            return f32v3{x, y, z};
        };

        const auto computeLowerVertex = [&](const u32 i, const u32 j) -> f32v3 {
            const f32 v = f32(i) / rings;
            const f32 phi = v * Math::Pi<f32>();
            const f32 pc = Math::Cosine(phi);
            const f32 ps = Math::Sine(phi);
            const f32 tx = Math::Map(ps, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(pc, 0.f, -1.f, -0.5f, -1.f);
            const f32 tz = Math::Map(ps, 0.f, 1.f, mnz, mxz);
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();
            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            const f32 x = Math::Map(ps * tc, ps, 0.f, tx, sx * 0.5f);
            const f32 z = Math::Map(ps * ts, ps, 0.f, tz, sz * 0.5f);
            return f32v3{x, y, z};
        };

        // Find the ring index where Y-dominance ends.
        // At ring i, the normal's Y component ~ cos(phi) - 0.5, and the
        // horizontal component ~ sin(phi). Y-dominant when cos(phi)-0.5 > sin(phi)*factor.
        // For simplicity, use halfRings/2 as the split — at the midpoint between pole
        // and equator. Or better, compute it:
        u32 ySplitRing = 1;
        for (u32 i = 1; i < halfRings + 1; ++i)
        {
            const f32v3 pos = computeUpperVertex(i, halfQuart); // sample at theta midpoint
            const u32 face = chooseFace_02(pos, sx, sz);
            if (face != yFaceTop)
            {
                ySplitRing = i;
                break;
            }
            ySplitRing = i + 1;
        }

        // Similarly for lower hemisphere
        u32 ySplitRingLower = rings - 2;
        for (u32 i = rings - 2; i >= halfRings; --i)
        {
            const f32v3 pos = computeLowerVertex(i, halfQuart);
            const u32 face = chooseFace_02(pos, sx, sz);
            if (face != yFaceBot)
            {
                ySplitRingLower = i;
                break;
            }
            ySplitRingLower = i - 1;
        }

        // ------------------------------------------------------------------
        // Emit sub-patches. Each is a self-contained vertex+index block.
        // We use a helper lambda to emit a rectangular patch of the octant.
        // ------------------------------------------------------------------

        // Emits a patch covering rings [ringMin, ringMax] and sectors [secMin, secMax]
        // with a forced face assignment. Handles pole rings (ring 0 and ring rings-1)
        // specially by emitting per-sector pole vertices.
        const auto emitPatch = [&](const u32 ringMin, const u32 ringMax, const u32 secMin, const u32 secMax,
                                   const u32 face, const bool upper) {
            const u32 patchOffset = data.Vertices.GetSize();
            const u32 secCount = secMax - secMin;

            const bool hasTopPole = (ringMin == 0);
            // Actually poles are only at ring 0 (top) and the absolute bottom

            // For indexing within this patch:
            // If hasTopPole: ring 0 has secCount pole vertices (indices 0..secCount-1)
            //   then ring 1 starts at secCount, each ring has (secCount+1) vertices
            // If !hasTopPole: ring ringMin has (secCount+1) vertices starting at 0
            //   subsequent rings follow

            // Emit vertices
            // Ring 0 (pole) if applicable
            if (hasTopPole)
                for (u32 j = 0; j < secCount; ++j)
                    addSphereVertex(mnx, 1.f, mnz, face);

            // Body rings
            const u32 bodyStart = hasTopPole ? 1 : ringMin;
            const u32 bodyEnd = ringMax; // inclusive

            for (u32 i = bodyStart; i <= bodyEnd; ++i)
                for (u32 j = secMin; j <= secMax; ++j)
                {
                    f32v3 pos;
                    if (upper || i < halfRings)
                        pos = computeUpperVertex(i, j);
                    else
                        pos = computeLowerVertex(i, j);
                    addSphereVertex(pos[0], pos[1], pos[2], face);
                }

            // Bottom pole if applicable
            const bool isAbsoluteBottom = (!upper && ringMax >= rings - 1);
            if (isAbsoluteBottom)
                for (u32 j = 0; j < secCount; ++j)
                    addSphereVertex(mnx, -1.f, mnz, face);

            // Emit indices
            const u32 ringWidth = secCount + 1;

            // Local index helper
            const auto localIdx = [&](const u32 ring, const u32 sec) -> u32 {
                // ring is in patch-local coords (0 = first ring in patch)
                // sec is in [0, secCount]
                if (hasTopPole && ring == 0)
                    return patchOffset + sec; // pole vertex for this sector
                const u32 bodyRingIdx = ring - (hasTopPole ? 1 : 0);
                const u32 base = hasTopPole ? secCount : 0;
                return patchOffset + base + bodyRingIdx * ringWidth + sec;
            };

            const u32 totalBodyRings = bodyEnd - bodyStart + 1;
            // Top pole fan
            if (hasTopPole && totalBodyRings > 0)
            {
                for (u32 j = 0; j < secCount; ++j)
                {
                    const u32 jj = j + 1;
                    if (sx * sz > 0.f)
                    {
                        data.Indices.Append(Index(localIdx(1, jj)));
                        data.Indices.Append(Index(localIdx(1, j)));
                        data.Indices.Append(Index(localIdx(0, j)));
                    }
                    else
                    {
                        data.Indices.Append(Index(localIdx(1, jj)));
                        data.Indices.Append(Index(localIdx(0, j)));
                        data.Indices.Append(Index(localIdx(1, j)));
                    }
                }
            }

            // Body quads
            const u32 firstBodyLocal = hasTopPole ? 1 : 0;
            for (u32 ri = 0; ri < totalBodyRings - 1; ++ri)
            {
                const u32 r = firstBodyLocal + ri;
                const u32 rn = r + 1;
                for (u32 j = 0; j < secCount; ++j)
                {
                    const u32 jj = j + 1;

                    // Determine winding based on hemisphere
                    const u32 actualRing = bodyStart + ri;
                    if (actualRing < halfRings) // upper hemisphere
                    {
                        if (sx * sz > 0.f)
                        {
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(rn, j)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(r, jj)));
                        }
                        else
                        {
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(rn, j)));
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                        }
                    }
                    else // lower hemisphere
                    {
                        if (sx * sz > 0.f)
                        {
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(rn, j)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(r, jj)));
                        }
                        else
                        {
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                            data.Indices.Append(Index(localIdx(rn, j)));
                            data.Indices.Append(Index(localIdx(rn, jj)));
                            data.Indices.Append(Index(localIdx(r, jj)));
                            data.Indices.Append(Index(localIdx(r, j)));
                        }
                    }
                }
            }

            // Bottom pole fan
            if (isAbsoluteBottom)
            {
                const u32 lastBodyRing = firstBodyLocal + totalBodyRings - 1;
                const u32 poleRing = lastBodyRing + 1;
                for (u32 j = 0; j < secCount; ++j)
                {
                    if (sx * sz > 0.f)
                    {
                        data.Indices.Append(Index(localIdx(lastBodyRing, j)));
                        data.Indices.Append(Index(localIdx(lastBodyRing, j + 1)));
                        data.Indices.Append(Index(localIdx(poleRing, j)));
                    }
                    else
                    {
                        data.Indices.Append(Index(localIdx(lastBodyRing, j)));
                        data.Indices.Append(Index(localIdx(poleRing, j)));
                        data.Indices.Append(Index(localIdx(lastBodyRing, j + 1)));
                    }
                }
            }
        };

        // ------------------------------------------------------------------
        // Emit the sub-patches
        // ------------------------------------------------------------------

        // Upper hemisphere:
        //   Y-face patch:  rings [0, ySplitRing],  sectors [0, quartSectors]
        //   Z-face patch:  rings [ySplitRing, halfRings], sectors [0, halfQuart]
        //   X-face patch:  rings [ySplitRing, halfRings], sectors [halfQuart, quartSectors]

        if (ySplitRing > 1)
            emitPatch(0, ySplitRing, 0, quartSectors, yFaceTop, true);
        else
            emitPatch(0, 1, 0, quartSectors, yFaceTop, true); // at least the pole fan

        if (ySplitRing <= halfRings)
        {
            emitPatch(ySplitRing, halfRings, 0, halfQuart, zFace, true);
            emitPatch(ySplitRing, halfRings, halfQuart, quartSectors, xFace, true);
        }

        // Lower hemisphere:
        //   Z-face patch:  rings [halfRings, ySplitRingLower], sectors [0, halfQuart]
        //   X-face patch:  rings [halfRings, ySplitRingLower], sectors [halfQuart, quartSectors]
        //   Y-face patch:  rings [ySplitRingLower, rings-1],   sectors [0, quartSectors]

        if (ySplitRingLower >= halfRings)
        {
            emitPatch(halfRings, ySplitRingLower, 0, halfQuart, zFace, false);
            emitPatch(halfRings, ySplitRingLower, halfQuart, quartSectors, xFace, false);
        }

        if (ySplitRingLower < rings - 1)
            emitPatch(ySplitRingLower, rings - 1, 0, quartSectors, yFaceBot, false);

        offset = data.Vertices.GetSize();
        {
            const f32 angle = 2.f * Math::Pi<f32>() / sectors;
            const u32 halfQuart = quartSectors / 2;

            // Determine which faces this edge bridges
            // addEdge02 sweeps from the Z-aligned face toward the X-aligned face
            // At j=0 the normal points along Z, at j=quartSectors it points along X
            const u32 faceA = (sz > 0.f) ? 4u : 5u; // +Z or -Z
            const u32 faceB = (sx > 0.f) ? 0u : 1u; // +X or -X

            // First half: j = 0 .. halfQuart, assigned to faceA
            for (u32 j = 0; j <= halfQuart; ++j)
            {
                const f32 cc = Math::Cosine(j * angle);
                const f32 ss = Math::Sine(j * angle);
                const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
                const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

                addCylinderVertex02(x, -0.5f, z, faceA);
                addCylinderVertex02(x, 0.5f, z, faceA);
            }

            // Duplicate the midpoint vertex pair for faceB (seam)
            {
                const f32 cc = Math::Cosine(halfQuart * angle);
                const f32 ss = Math::Sine(halfQuart * angle);
                const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
                const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

                addCylinderVertex02(x, -0.5f, z, faceB);
                addCylinderVertex02(x, 0.5f, z, faceB);
            }

            // Second half: j = halfQuart+1 .. quartSectors, assigned to faceB
            for (u32 j = halfQuart + 1; j <= quartSectors; ++j)
            {
                const f32 cc = Math::Cosine(j * angle);
                const f32 ss = Math::Sine(j * angle);
                const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
                const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

                addCylinderVertex02(x, -0.5f, z, faceB);
                addCylinderVertex02(x, 0.5f, z, faceB);
            }

            // Indexing: total vertex pairs = (halfQuart+1) + 1 + (quartSectors - halfQuart)
            //                               = quartSectors + 2
            // The midpoint duplication inserts an extra pair at index (halfQuart+1)
            // So vertex pair indices:
            //   j=0..halfQuart  -> pair index 0..halfQuart          (faceA)
            //   duplicate       -> pair index halfQuart+1            (faceB)
            //   j=halfQuart+1.. -> pair index halfQuart+2..          (faceB)

            const u32 totalPairs = quartSectors + 2; // +1 for closing, +1 for duplicate
            for (u32 p = 0; p < totalPairs - 1; ++p)
            {
                // Skip the quad that straddles faceA's last and faceB's first
                // Actually we don't skip — the duplicate means pair halfQuart is faceA
                // and pair halfQuart+1 is faceB at the same position, forming a degenerate
                // zero-area quad. But we do want to skip indexing across the seam.
                if (p == halfQuart)
                    continue; // this pair's "next" is the duplicate, skip

                const u32 ii = 2 * p;
                if (sx * sz > 0.f)
                {
                    addCylinderIndex(ii);
                    addCylinderIndex(ii + 1);
                    addCylinderIndex(ii + 2);

                    addCylinderIndex(ii + 1);
                    addCylinderIndex(ii + 3);
                    addCylinderIndex(ii + 2);
                }
                else
                {
                    addCylinderIndex(ii);
                    addCylinderIndex(ii + 2);
                    addCylinderIndex(ii + 1);

                    addCylinderIndex(ii + 1);
                    addCylinderIndex(ii + 2);
                    addCylinderIndex(ii + 3);
                }
            }
        }
        offset = data.Vertices.GetSize();
    };

    const u32 whole = 2 * rings;
    const u32 part = halfRings;
    const auto addEdge12 = [&](const f32 sy, const f32 sz) {
        const f32 mny = 0.5f * sy;
        const f32 mxy = sy;

        const f32 mnz = 0.5f * sz;
        const f32 mxz = sz;
        const f32 angle = 2.f * Math::Pi<f32>() / whole;

        const u32 halfPart = part / 2;

        // At j=0 the normal points along Y, at j=part it points along Z
        const u32 faceA = (sy > 0.f) ? 2u : 3u; // +Y or -Y
        const u32 faceB = (sz > 0.f) ? 4u : 5u; // +Z or -Z

        // First half: j = 0 .. halfPart, assigned to faceA
        for (u32 j = 0; j <= halfPart; ++j)
        {
            const f32 cc = Math::Cosine(j * angle);
            const f32 ss = Math::Sine(j * angle);
            const f32 y = Math::Map(cc, 0.f, 1.f, mny, mxy);
            const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

            addCylinderVertex12(-0.5f, y, z, faceA);
            addCylinderVertex12(0.5f, y, z, faceA);
        }

        // Duplicate midpoint for faceB
        {
            const f32 cc = Math::Cosine(halfPart * angle);
            const f32 ss = Math::Sine(halfPart * angle);
            const f32 y = Math::Map(cc, 0.f, 1.f, mny, mxy);
            const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

            addCylinderVertex12(-0.5f, y, z, faceB);
            addCylinderVertex12(0.5f, y, z, faceB);
        }

        // Second half: j = halfPart+1 .. part, assigned to faceB
        for (u32 j = halfPart + 1; j <= part; ++j)
        {
            const f32 cc = Math::Cosine(j * angle);
            const f32 ss = Math::Sine(j * angle);
            const f32 y = Math::Map(cc, 0.f, 1.f, mny, mxy);
            const f32 z = Math::Map(ss, 0.f, 1.f, mnz, mxz);

            addCylinderVertex12(-0.5f, y, z, faceB);
            addCylinderVertex12(0.5f, y, z, faceB);
        }

        // Indexing
        const u32 totalPairs = part + 2;
        for (u32 p = 0; p < totalPairs - 1; ++p)
        {
            if (p == halfPart)
                continue;

            const u32 ii = 2 * p;
            if (sy * sz < 0.f)
            {
                addCylinderIndex(ii);
                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 2);

                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 3);
                addCylinderIndex(ii + 2);
            }
            else
            {
                addCylinderIndex(ii);
                addCylinderIndex(ii + 2);
                addCylinderIndex(ii + 1);

                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 2);
                addCylinderIndex(ii + 3);
            }
        }

        offset = data.Vertices.GetSize();
    };

    const auto addEdge01 = [&](const f32 sx, const f32 sy) {
        const f32 mnx = 0.5f * sx;
        const f32 mxx = sx;

        const f32 mny = 0.5f * sy;
        const f32 mxy = sy;

        const f32 angle = 2.f * Math::Pi<f32>() / whole;

        const u32 halfPart = part / 2;

        // At j=0 the normal points along X, at j=part it points along Y
        const u32 faceA = (sx > 0.f) ? 0u : 1u; // +X or -X
        const u32 faceB = (sy > 0.f) ? 2u : 3u; // +Y or -Y

        // First half
        for (u32 j = 0; j <= halfPart; ++j)
        {
            const f32 cc = Math::Cosine(j * angle);
            const f32 ss = Math::Sine(j * angle);
            const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(ss, 0.f, 1.f, mny, mxy);

            addCylinderVertex01(x, y, -0.5f, faceA);
            addCylinderVertex01(x, y, 0.5f, faceA);
        }

        // Duplicate midpoint for faceB
        {
            const f32 cc = Math::Cosine(halfPart * angle);
            const f32 ss = Math::Sine(halfPart * angle);
            const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(ss, 0.f, 1.f, mny, mxy);

            addCylinderVertex01(x, y, -0.5f, faceB);
            addCylinderVertex01(x, y, 0.5f, faceB);
        }

        // Second half
        for (u32 j = halfPart + 1; j <= part; ++j)
        {
            const f32 cc = Math::Cosine(j * angle);
            const f32 ss = Math::Sine(j * angle);
            const f32 x = Math::Map(cc, 0.f, 1.f, mnx, mxx);
            const f32 y = Math::Map(ss, 0.f, 1.f, mny, mxy);

            addCylinderVertex01(x, y, -0.5f, faceB);
            addCylinderVertex01(x, y, 0.5f, faceB);
        }

        // Indexing
        const u32 totalPairs = part + 2;
        for (u32 p = 0; p < totalPairs - 1; ++p)
        {
            if (p == halfPart)
                continue;

            const u32 ii = 2 * p;
            if (sx * sy < 0.f)
            {
                addCylinderIndex(ii);
                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 2);

                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 3);
                addCylinderIndex(ii + 2);
            }
            else
            {
                addCylinderIndex(ii);
                addCylinderIndex(ii + 2);
                addCylinderIndex(ii + 1);

                addCylinderIndex(ii + 1);
                addCylinderIndex(ii + 2);
                addCylinderIndex(ii + 3);
            }
        }

        offset = data.Vertices.GetSize();
    };
    addEdge02(1.f, 1.f);
    addEdge02(-1.f, 1.f);
    addEdge02(1.f, -1.f);
    addEdge02(-1.f, -1.f);

    addEdge12(1.f, 1.f);
    addEdge12(-1.f, 1.f);
    addEdge12(1.f, -1.f);
    addEdge12(-1.f, -1.f);

    addEdge01(1.f, 1.f);
    addEdge01(-1.f, 1.f);
    addEdge01(1.f, -1.f);
    addEdge01(-1.f, -1.f);

    const StatMeshData<D3> cdata = createBoxMesh(offset, 1.f);
    for (const StatVertex<D3> &vx : cdata.Vertices)
    {
        const f32v2 uv = computeRoundedBoxTexCoords(vx.Position, vx.Normal);
        data.Vertices.Append(ParaVertex<D3>{vx.Position, uv, vx.Normal, vx.Tangent, 0});
    }
    data.Indices.Insert(data.Indices.end(), cdata.Indices.begin(), cdata.Indices.end());

    VALIDATE_MESH(data);
    return data;
}

ParaMeshData<D3> CreateTorusMesh(const u32 rings, const u32 sectors)
{
    ParaMeshData<D3> data{};
    data.Shape = ParametricShape_Torus;

    // Major radius: center of tube to center of torus
    // Minor radius: tube radius
    // Total diameter = 2 * (major + minor) = 1  →  major + minor = 0.5
    // Convention: minor = major / 2  →  major = 1/3, minor = 1/6
    // (adjust ratio to taste)
    constexpr f32 R = 1.f / 3.f;
    constexpr f32 r = 1.f / 6.f;

    // Vertices: (rings + 1) x (sectors + 1) grid with wrapped UVs
    for (u32 i = 0; i <= rings; ++i)
    {
        const f32 u = f32(i) / rings;
        const f32 theta = 2.f * u * Math::Pi<f32>(); // major angle
        const f32 ct = Math::Cosine(theta);
        const f32 st = Math::Sine(theta);

        for (u32 j = 0; j <= sectors; ++j)
        {
            const f32 v = f32(j) / sectors;
            const f32 phi = 2.f * v * Math::Pi<f32>(); // minor angle
            const f32 cp = Math::Cosine(phi);
            const f32 sp = Math::Sine(phi);

            // Position
            const f32 x = (R + r * cp) * ct;
            const f32 y = r * sp;
            const f32 z = (R + r * cp) * st;

            // Normal: points from tube center toward surface
            const f32v3 normal = Math::Normalize(f32v3{cp * ct, sp, cp * st});

            // Tangent: direction of increasing U (along major circle)
            const f32v4 tangent = f32v4{-st, 0.f, ct, 1.f};

            data.Vertices.Append(ParaVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, normal, tangent, 0});
        }
    }

    // Indices: quads between adjacent rings/sectors
    for (u32 i = 0; i < rings; ++i)
    {
        for (u32 j = 0; j < sectors; ++j)
        {
            const u32 a = i * (sectors + 1) + j;
            const u32 b = a + sectors + 1;

            data.Indices.Append(Index(a));
            data.Indices.Append(Index(a + 1));
            data.Indices.Append(Index(b));

            data.Indices.Append(Index(a + 1));
            data.Indices.Append(Index(b + 1));
            data.Indices.Append(Index(b));
        }
    }

    VALIDATE_MESH(data);
    return data;
}

template Asset AddMaterial(AssetPool pool, const MaterialData<D2> &data);
template Asset AddMaterial(AssetPool pool, const MaterialData<D3> &data);

template Result<AssetPool> CreateMaterialPool<D2>();
template Result<AssetPool> CreateMaterialPool<D3>();

template void DestroyMaterialPool<D2>(AssetPool pool);
template void DestroyMaterialPool<D3>(AssetPool pool);

template void UpdateMaterial(Asset mesh, const MaterialData<D2> &data);
template void UpdateMaterial(Asset mesh, const MaterialData<D3> &data);

template Asset AddMesh(AssetPool pool, const StatMeshData<D2> &data);
template Asset AddMesh(AssetPool pool, const StatMeshData<D3> &data);

template void UpdateMesh(Asset handle, const StatMeshData<D2> &data);
template void UpdateMesh(Asset handle, const StatMeshData<D3> &data);

template Asset AddMesh(AssetPool pool, const ParaMeshData<D2> &data);
template Asset AddMesh(AssetPool pool, const ParaMeshData<D3> &data);

template void UpdateMesh(Asset handle, const ParaMeshData<D2> &data);
template void UpdateMesh(Asset handle, const ParaMeshData<D3> &data);

template Result<AssetPool> CreateMeshPool<D2>(Geometry geo);
template Result<AssetPool> CreateMeshPool<D3>(Geometry geo);

template void DestroyMeshPool<D2>(Geometry geo, AssetPool pool);
template void DestroyMeshPool<D3>(Geometry geo, AssetPool pool);

#ifdef ONYX_ENABLE_OBJ_LOAD
template Result<StatMeshData<D2>> LoadStaticMeshFromObjFile<D2>(const char *path, LoadObjDataFlags flags);
template Result<StatMeshData<D3>> LoadStaticMeshFromObjFile<D3>(const char *path, LoadObjDataFlags flags);
#endif

template bool IsMeshPoolHandleValid<D2>(Geometry geo, AssetPool handle);
template bool IsMeshHandleValid<D2>(Geometry geo, Asset handle);
template bool IsMaterialPoolHandleValid<D2>(AssetPool handle);
template bool IsMaterialHandleValid<D2>(Asset handle);

template bool IsMeshPoolHandleValid<D3>(Geometry geo, AssetPool handle);
template bool IsMeshHandleValid<D3>(Geometry geo, Asset handle);
template bool IsMaterialPoolHandleValid<D3>(AssetPool handle);
template bool IsMaterialHandleValid<D3>(Asset handle);

template StatMeshData<D2> GetStaticMeshData(Asset handle);
template StatMeshData<D3> GetStaticMeshData(Asset handle);

template ParaMeshData<D2> GetParametricMeshData(Asset handle);
template ParaMeshData<D3> GetParametricMeshData(Asset handle);

template ParametricShape GetParametricShape<D2>(Asset handle);
template ParametricShape GetParametricShape<D3>(Asset handle);

template const MaterialData<D2> &GetMaterialData(Asset handle);
template const MaterialData<D3> &GetMaterialData(Asset handle);

#ifdef ONYX_ENABLE_GLTF_LOAD
template GltfHandles AddGltfAssets(AssetPool meshPool, AssetPool materialPool, GltfAssets<D2> &assets);
template GltfHandles AddGltfAssets(AssetPool meshPool, AssetPool materialPool, GltfAssets<D3> &assets);

template Result<GltfAssets<D2>> LoadGltfAssetsFromFile(const std::string &path, AssetsFlags flags);
template Result<GltfAssets<D3>> LoadGltfAssetsFromFile(const std::string &path, AssetsFlags flags);
#endif

template TKit::Span<const u32> GetMeshAssetPools<D2>(Geometry geo);
template TKit::Span<const u32> GetMeshAssetPools<D3>(Geometry geo);

template u32 GetMeshCount<D2>(Geometry geo, AssetPool pool);
template u32 GetMeshCount<D3>(Geometry geo, AssetPool pool);

template const VKit::DeviceBuffer *GetMeshVertexBuffer<D2>(Geometry geo, AssetPool pool);
template const VKit::DeviceBuffer *GetMeshVertexBuffer<D3>(Geometry geo, AssetPool pool);

template const VKit::DeviceBuffer *GetMeshIndexBuffer<D2>(Geometry geo, AssetPool pool);
template const VKit::DeviceBuffer *GetMeshIndexBuffer<D3>(Geometry geo, AssetPool pool);

template MeshDataLayout GetMeshLayout<D2>(Geometry geo, Asset handle);
template MeshDataLayout GetMeshLayout<D3>(Geometry geo, Asset handle);

template StatMeshData<D2> CreateTriangleMesh<D2>();
template StatMeshData<D3> CreateTriangleMesh<D3>();

template StatMeshData<D2> CreateQuadMesh<D2>();
template StatMeshData<D3> CreateQuadMesh<D3>();

template StatMeshData<D2> CreateRegularPolygonMesh<D2>(u32);
template StatMeshData<D3> CreateRegularPolygonMesh<D3>(u32);

template StatMeshData<D2> CreatePolygonMesh<D2>(TKit::Span<const f32v2>);
template StatMeshData<D3> CreatePolygonMesh<D3>(TKit::Span<const f32v2>);

template ParaMeshData<D2> CreateStadiumMesh<D2>();
template ParaMeshData<D3> CreateStadiumMesh<D3>();

template ParaMeshData<D2> CreateRoundedQuadMesh<D2>();
template ParaMeshData<D3> CreateRoundedQuadMesh<D3>();

} // namespace Onyx::Assets
