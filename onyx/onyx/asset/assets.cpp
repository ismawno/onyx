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
    TKit::StaticArray<AssetPool, ONYX_MAX_ASSET_POOLS> ToDestroy{};
};

template <Dimension D> using StatMeshPoolData = MeshPoolData<StatVertex<D>>;
template <Dimension D> using StatMeshAssetData = MeshAssetData<StatVertex<D>>;

template <Dimension D> using ParaMeshPoolData = MeshPoolData<ParaVertex<D>>;
template <Dimension D> using ParaMeshAssetData = MeshAssetData<ParaVertex<D>>;

struct FontDataInfo
{
    FontData Data{};
    Asset Atlas = NullHandle;
    u32 VertexStart;
    u32 VertexCount;
    u32 IndexStart;
    u32 IndexCount;
    StatusFlags Flags;
};

struct FontPoolData
{
    VKit::DeviceBuffer VertexBuffer{};
    VKit::DeviceBuffer IndexBuffer{};
    TKit::DynamicArray<GlyphVertex> Vertices{};
    TKit::DynamicArray<Index> Indices{};
    TKit::TierArray<FontDataInfo> FontData{};
};

struct FontAssetData
{
    TKit::StaticHive<FontPoolData, ONYX_MAX_TEXTURES> Pools{};
    TKit::StaticArray<AssetPool, ONYX_MAX_ASSET_POOLS> ToDestroy{};
};

template <Dimension D> struct MaterialPoolData
{
    VKit::DeviceBuffer Buffer{};
    TKit::TierArray<MaterialData<D>> Materials{};
    TKit::TierArray<StatusFlags> Flags{};
};

template <Dimension D> struct MaterialAssetData
{
    TKit::StaticHive<MaterialPoolData<D>, ONYX_MAX_ASSET_POOLS> Pools{};
    TKit::StaticArray<AssetPool, ONYX_MAX_ASSET_POOLS> ToDestroy{};
};

struct SamplerInfo
{
    VKit::Sampler Sampler{};
    SamplerData Data{};
    Asset Handle = NullAsset;
    AssetsFlags Flags = 0;
};

struct SamplerAssetData
{
    TKit::ArenaHive<SamplerInfo> Samplers{};
    TKit::ArenaArray<Asset> ToDestroy{};
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
    TKit::ArenaArray<Asset> ToDestroy{};
};

template <Dimension D> struct AssetData
{
    StatMeshAssetData<D> StaticMeshes{};
    ParaMeshAssetData<D> ParametricMeshes{};
    MaterialAssetData<D> Materials{};
};

static TKit::Storage<AssetData<D2>> s_AssetData2{};
static TKit::Storage<AssetData<D3>> s_AssetData3{};
static TKit::Storage<FontAssetData> s_FontData{};
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

template <Dimension D> void updateMaterialDescriptorSet(const u32 pid)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pid];

    const VkDescriptorBufferInfo binfo = mpool.Buffer.CreateDescriptorInfo();
    const VKit::DescriptorSetLayout &layout = Descriptors::GetDescriptorSetLayout<D>(Shading_Lit);

    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &layout};
    writer.WriteBuffer(3, binfo, pid);

    for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D>(Shading_Lit))
        writer.Overwrite(set);
}

template <Dimension D> ONYX_NO_DISCARD static Result<> checkMaterialBufferSize(const u32 pid)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pid];

    const u32 mcount = mpool.Materials.GetSize();
    const auto result = Resources::GrowBufferIfNeeded<MaterialData<D>>(mpool.Buffer, mcount);
    TKIT_RETURN_ON_ERROR(result);
    if (result.GetValue())
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Insufficient material buffer memory. Resized from {:L} to {:L} bytes",
                       mcount * sizeof(MaterialData<D>), mpool.Buffer.GetInfo().Size);
        for (StatusFlags &flags : mpool.Flags)
            flags = StatusFlag_UpdateMaterial;
        updateMaterialDescriptorSet<D>(pid);
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
    for (const AssetPool pool : meshes.ToDestroy)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying mesh pool with handle {:#010x}", pool);
        const u32 pid = GetAssetPoolId(pool);
        MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
        mpool.VertexBuffer.Destroy();
        mpool.IndexBuffer.Destroy();

        meshes.Pools.Remove(pid);
    }
    meshes.ToDestroy.Clear();

    for (MeshPoolData<Vertex> &mpool : meshes.Pools)
    {
        TKIT_RETURN_IF_FAILED(uploadMeshPool(mpool));
    }

    return Result<>::Ok();
}

template <Dimension D>
ONYX_NO_DISCARD static Result<> uploadMaterialRange(const u32 pid, const u32 start, const u32 end)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pid];
    const VkDeviceSize offset = start * sizeof(MaterialData<D>);
    const VkDeviceSize size = end * sizeof(MaterialData<D>) - offset;

    TKIT_LOG_DEBUG("[ONYX][ASSETS] Uploading material range {} - {} ({:L} bytes)", start, end, size);

    VKit::CommandPool &cpool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    return mpool.Buffer.UploadFromHost(cpool, queue->GetHandle(), mpool.Materials.GetData(),
                                       {.srcOffset = offset, .dstOffset = offset, .size = size});
}

template <Dimension D> ONYX_NO_DISCARD static Result<> uploadMaterialPool(const u32 pid)
{
    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pid];
    if (mpool.Materials.IsEmpty())
        return Result<>::Ok();

    TKIT_ASSERT(mpool.Materials.GetSize() == mpool.Flags.GetSize(),
                "[ONYX][ASSETS] Material data size ({}) and flags size ({}) mismatch", mpool.Materials.GetSize(),
                mpool.Flags.GetSize());

    const auto mustUpdate = [&mpool](const u32 index) { return StatusFlag_UpdateMaterial & mpool.Flags[index]; };

    TKIT_RETURN_IF_FAILED(checkMaterialBufferSize<D>(pid));

    const u32 size = mpool.Materials.GetSize();
    for (u32 i = 0; i < size; ++i)
        if (mustUpdate(i))
            for (u32 j = i + 1; j <= size; ++j)
                if (j == size || !mustUpdate(j))
                {
                    TKIT_RETURN_IF_FAILED(uploadMaterialRange<D>(pid, i, j));
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
    for (const AssetPool pool : materials.ToDestroy)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying material pool with handle {:#010x}", pool);

        const u32 pid = GetAssetPoolId(pool);
        MaterialPoolData<D> &mpool = materials.Pools[pid];
        mpool.Buffer.Destroy();
        materials.Pools.Remove(pid);
    }
    materials.ToDestroy.Clear();

    for (const u32 pid : materials.Pools.GetValidIds())
    {
        TKIT_RETURN_IF_FAILED(uploadMaterialPool<D>(pid));
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

Result<> Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][ASSETS] Initializing");
    s_AssetData2.Construct();
    s_AssetData3.Construct();
    s_FontData.Construct();
    s_SamplerData.Construct();
    s_TextureData.Construct();

    s_SamplerData->Samplers.Reserve(specs.MaxSamplers);
    s_SamplerData->ToDestroy.Reserve(specs.MaxSamplers);

    s_TextureData->Textures.Reserve(specs.MaxTextures);
    s_TextureData->ToDestroy.Reserve(specs.MaxTextures);
    return Result<>::Ok();
}

void Terminate()
{
    terminate<D2>();
    terminate<D3>();

    for (TextureInfo &tinfo : s_TextureData->Textures)
    {
        tinfo.Image.Destroy();
        if (tinfo.Flags & StatusFlag_MustFreeTexture)
            TKit::Deallocate(tinfo.Data.Data);
    }

    for (SamplerInfo &sinfo : s_SamplerData->Samplers)
        sinfo.Sampler.Destroy();

    s_SamplerData.Construct();
    s_TextureData.Destruct();
    s_FontData.Destruct();
    s_AssetData2.Destruct();
    s_AssetData3.Destruct();
}

#define CHECK_POOL_HANDLE(pool, atype)                                                                                 \
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);                                                                           \
    ONYX_CHECK_HANDLE_HAS_ASSET_POOL_TYPE(pool, atype);                                                                \
    ONYX_CHECK_ASSET_POOL_IS_VALID(pool, atype)

#define CHECK_POOL_HANDLE_WITH_DIM(pool, atype, dim)                                                                   \
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);                                                                           \
    ONYX_CHECK_HANDLE_HAS_ASSET_POOL_TYPE(pool, atype);                                                                \
    ONYX_CHECK_ASSET_POOL_IS_VALID_WITH_DIM(pool, atype, dim)

#define CHECK_ASSET_HANDLE(handle, atype)                                                                              \
    ONYX_CHECK_ASSET_IS_NOT_NULL(handle);                                                                              \
    ONYX_CHECK_HANDLE_HAS_ASSET_TYPE(handle, atype);                                                                   \
    ONYX_CHECK_ASSET_IS_VALID(handle, atype);

#define CHECK_ASSET_AND_POOL_HANDLES(handle, atype)                                                                    \
    ONYX_CHECK_ASSET_IS_NOT_NULL(handle);                                                                              \
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(handle);                                                                         \
    ONYX_CHECK_ASSET_POOL_IS_VALID(handle, atype);                                                                     \
    ONYX_CHECK_ASSET_IS_VALID(handle, atype);

#define CHECK_ASSET_AND_POOL_HANDLES_WITH_DIM(handle, atype, dim)                                                      \
    ONYX_CHECK_ASSET_IS_NOT_NULL(handle);                                                                              \
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(handle);                                                                         \
    ONYX_CHECK_ASSET_POOL_IS_VALID_WITH_DIM(handle, atype, dim);                                                       \
    ONYX_CHECK_ASSET_IS_VALID_WITH_DIM(handle, atype, dim);

Asset CreateSampler(const SamplerData &data)
{
    const u32 sid = s_SamplerData->Samplers.Insert();
    SamplerInfo &sinfo = s_SamplerData->Samplers[sid];

    const Asset handle = CreateAssetHandle(Asset_Sampler, sid);
    sinfo.Handle = handle;
    sinfo.Data = data;

    sinfo.Flags |= StatusFlag_UpdateSampler;
    return handle;
}

void UpdateSampler(const Asset handle, const SamplerData &data)
{
    CHECK_ASSET_HANDLE(handle, Asset_Sampler);

    const u32 sid = GetAssetId(handle);

    SamplerInfo &sinfo = s_SamplerData->Samplers[sid];
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

void DestroySampler(const Asset handle)
{
    CHECK_ASSET_HANDLE(handle, Asset_Sampler);

    s_SamplerData->ToDestroy.Append(handle);
    removeSamplerReferences<D2>(handle);
    removeSamplerReferences<D3>(handle);
}

template <Dimension D>
GltfHandles CreateGltfAssets(const AssetPool meshPool, const AssetPool materialPool, GltfData<D> &data)
{
    GltfHandles handles;
    handles.StaticMeshes.Reserve(data.StaticMeshes.GetSize());
    handles.Materials.Reserve(data.Materials.GetSize());
    handles.Samplers.Reserve(data.Samplers.GetSize());
    handles.Textures.Reserve(data.Textures.GetSize());

    for (const StatMeshData<D> &smesh : data.StaticMeshes)
        handles.StaticMeshes.Append(CreateMesh(meshPool, smesh));

    for (const SamplerData &sdata : data.Samplers)
        handles.Samplers.Append(CreateSampler(sdata));

    for (const TextureData &tdata : data.Textures)
        handles.Textures.Append(CreateTexture(tdata));

    for (MaterialData<D> &mdata : data.Materials)
    {
        if constexpr (D == D2)
        {
            if (mdata.Sampler != TKIT_U32_MAX)
                mdata.Sampler = handles.Samplers[mdata.Sampler];
            if (mdata.Texture != TKIT_U32_MAX)
                mdata.Texture = handles.Textures[mdata.Texture];
        }
        else
        {
            for (Asset &sampler : mdata.Samplers)
                if (sampler != TKIT_U32_MAX)
                    sampler = handles.Samplers[sampler];
            for (Asset &texture : mdata.Textures)
                if (texture != TKIT_U32_MAX)
                    texture = handles.Textures[texture];
        }
        handles.Materials.Append(CreateMaterial(materialPool, mdata));
    }

    return handles;
}

Asset CreateTexture(const TextureData &data, const CreateTextureFlags flags)
{
    const u32 tid = s_TextureData->Textures.Insert();
    const Asset handle = CreateAssetHandle(Asset_Texture, tid);

    TextureInfo &tinfo = s_TextureData->Textures[tid];
    tinfo.Data = data;
    tinfo.Handle = handle;

    StatusFlags sflags = StatusFlag_UpdateTexture | StatusFlag_CreateTexture;
    if (!(flags & CreateTextureFlag_ManuallyHandledMemory))
        sflags |= StatusFlag_MustFreeTexture;
    tinfo.Flags = sflags;
    return handle;
}
void UpdateTexture(const Asset handle, const TextureData &data, const CreateTextureFlags flags)
{
    CHECK_ASSET_HANDLE(handle, Asset_Texture);

    const u32 tid = GetAssetId(handle);

    TextureInfo &tinfo = s_TextureData->Textures[tid];
    TKIT_ASSERT(
        data.ComputeSize() == tinfo.Data.ComputeSize(),
        "[ONYX][ASSETS] When updating a texture, the size of the data of the previous and updated texture must be the "
        "same. If they are not, you must create a new texture");

    if (tinfo.Flags & StatusFlag_MustFreeTexture)
        TKit::Deallocate(tinfo.Data.Data);

    tinfo.Data = data;
    tinfo.Flags |= StatusFlag_UpdateTexture;
#ifdef ONYX_ENABLE_GLTF_LOAD
    if (!(flags & CreateTextureFlag_ManuallyHandledMemory))
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

void DestroyTexture(const Asset handle)
{
    CHECK_ASSET_HANDLE(handle, Asset_Texture);

    s_TextureData->ToDestroy.Append(handle);
    removeTextureReferences<D2>(handle);
    removeTextureReferences<D3>(handle);
}

template <typename Vertex>
ONYX_NO_DISCARD static Result<AssetPool> createMeshPool(const AssetType atype, MeshAssetData<Vertex> &data)
{
    auto result = Resources::CreateBuffer<Vertex>(Buffer_DeviceVertex);
    TKIT_RETURN_ON_ERROR(result);
    VKit::DeviceBuffer vbuffer = result.GetValue();

    result = Resources::CreateBuffer<Index>(Buffer_DeviceIndex);
    TKIT_RETURN_ON_ERROR(result, vbuffer.Destroy());
    VKit::DeviceBuffer ibuffer = result.GetValue();

    const u32 pid = data.Pools.Insert();
    MeshPoolData<Vertex> &mpool = data.Pools[pid];
    mpool.VertexBuffer = vbuffer;
    mpool.IndexBuffer = ibuffer;

    const AssetPool pool = CreateAssetPoolHandle(atype, pid);
    if (Core::CanNameObjects())
    {
        const std::string vb = TKit::Format("onyx-assets-{}D-vertex-buffer-{:#010x}", u8(Vertex::Dim), pool);
        const std::string ib = TKit::Format("onyx-assets-{}D-index-buffer-{:#010x}", u8(Vertex::Dim), pool);

        TKIT_RETURN_IF_FAILED(vbuffer.SetName(vb.c_str()), vbuffer.Destroy(); data.Pools.Remove(pid));
        TKIT_RETURN_IF_FAILED(ibuffer.SetName(ib.c_str()), ibuffer.Destroy(); data.Pools.Remove(pid));
    }

    return pool;
}

template <Dimension D> ONYX_NO_DISCARD static Result<AssetPool> createMaterialPool()
{
    const auto result = Resources::CreateBuffer<MaterialData<D>>(Buffer_DeviceStorage);
    TKIT_RETURN_ON_ERROR(result);

    MaterialAssetData<D> &materials = getData<D>().Materials;
    const u32 pid = materials.Pools.Insert();

    MaterialPoolData<D> &mpool = materials.Pools[pid];
    mpool.Buffer = result.GetValue();

    const AssetPool pool = CreateAssetPoolHandle(Asset_Material, pid);
    if (Core::CanNameObjects())
    {
        const std::string name = TKit::Format("onyx-assets-{}D-material-pool-buffer-{:#010x}", u8(D), pool);
        TKIT_RETURN_IF_FAILED(mpool.Buffer.SetName(name.c_str()), mpool.Buffer.Destroy(); materials.Pools.Remove(pid));
    }

    updateMaterialDescriptorSet<D>(pid);
    return pool;
}

Result<AssetPool> CreateFontPool()
{
    auto result = Resources::CreateBuffer<GlyphVertex>(Buffer_DeviceVertex);
    TKIT_RETURN_ON_ERROR(result);
    VKit::DeviceBuffer vbuffer = result.GetValue();

    result = Resources::CreateBuffer<Index>(Buffer_DeviceIndex);
    TKIT_RETURN_ON_ERROR(result, vbuffer.Destroy());
    VKit::DeviceBuffer ibuffer = result.GetValue();

    const u32 pid = s_FontData->Pools.Insert();
    FontPoolData &fpool = s_FontData->Pools[pid];

    fpool.VertexBuffer = vbuffer;
    fpool.IndexBuffer = ibuffer;

    const AssetPool pool = CreateAssetPoolHandle(Asset_Font, pid);
    if (Core::CanNameObjects())
    {
        const std::string vb = TKit::Format("onyx-assets-font-vertex-buffer-{:#010x}", pool);
        const std::string ib = TKit::Format("onyx-assets-font-index-buffer-{:#010x}", pool);

        TKIT_RETURN_IF_FAILED(vbuffer.SetName(vb.c_str()), vbuffer.Destroy(); s_FontData->Pools.Remove(pid));
        TKIT_RETURN_IF_FAILED(ibuffer.SetName(ib.c_str()), ibuffer.Destroy(); s_FontData->Pools.Remove(pid));
    }

    return pool;
}

template <Dimension D> Result<AssetPool> CreateAssetPool(const AssetType atype)
{
    switch (atype)
    {
    case Asset_StaticMesh:
        return createMeshPool(Asset_StaticMesh, getData<D>().StaticMeshes);
    case Asset_ParametricMesh:
        return createMeshPool(Asset_ParametricMesh, getData<D>().ParametricMeshes);
    case Asset_Material:
        return createMaterialPool<D>();
    case Asset_Font:
    case Asset_GlyphMesh:
        return CreateFontPool();
    default:
        return Result<AssetPool>::Error(
            Error_BadInput,
            TKit::Format("[ONYX][ASSETS] An asset pool cannot be created for assets of type '{}'", ToString(atype)));
    }
}

void DestroyFontPool(const AssetPool pool)
{
    const u32 pid = GetAssetPoolId(pool);
    for (const FontDataInfo &info : s_FontData->Pools[pid].FontData)
        DestroyTexture(info.Atlas);
    s_FontData->ToDestroy.Append(pool);
}

template <Dimension D> void DestroyAssetPool(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);

    const AssetType atype = GetAssetType(pool);
    switch (atype)
    {
    case Asset_StaticMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_StaticMesh);
        getData<D>().StaticMeshes.ToDestroy.Append(pool);
        return;
    case Asset_ParametricMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_ParametricMesh);
        getData<D>().ParametricMeshes.ToDestroy.Append(pool);
        return;
    case Asset_Material:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Material);
        getData<D>().Materials.ToDestroy.Append(pool);
        return;
    case Asset_Font:
    case Asset_GlyphMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Font);
        DestroyFontPool(pool);
        return;
    default:
        TKIT_FATAL("[ONYX][ASSETS] An asset pool of type '{}' cannot exist", ToString(atype));
    }
}

template <typename Vertex>
static Asset addMesh(const AssetPool pool, MeshAssetData<Vertex> &meshes, const MeshData<Vertex> &data)
{
    constexpr AssetType atype = Vertex::Asset;

    CHECK_POOL_HANDLE_WITH_DIM(pool, atype, Vertex::Dim);

    const u32 pid = GetAssetPoolId(pool);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
    const u32 mid = mpool.Info.GetSize();
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
    return CreateAssetHandle(atype, mid, pid);
}

template <Dimension D> Asset CreateMesh(const AssetPool pool, const StatMeshData<D> &data)
{
    return addMesh(pool, getData<D>().StaticMeshes, data);
}
template <Dimension D> Asset CreateMesh(const AssetPool pool, const ParaMeshData<D> &data)
{
    return addMesh(pool, getData<D>().ParametricMeshes, data);
}

template <typename F1, typename F2>
static void buildFontBuffers(const FontData &data, const F1 addVertex, const F2 addIndex)
{
    for (u32 i = 0; i < data.Glyphs.GetSize(); ++i)
    {
        const GlyphData &glyph = data.Glyphs[i];
        const f32v2 min = f32v2{glyph.Bearing[0], glyph.Bearing[1] - glyph.Size[1]};
        const f32v2 max = f32v2{glyph.Bearing[0] + glyph.Size[0], glyph.Bearing[1]};

        const f32v2 mnuv = glyph.MinTexCoord;
        const f32v2 mxuv = glyph.MaxTexCoord;

        addVertex(min[0], min[1], mnuv[0], mxuv[1]);
        addVertex(max[0], min[1], mxuv[0], mxuv[1]);
        addVertex(min[0], max[1], mnuv[0], mnuv[1]);
        addVertex(max[0], max[1], mxuv[0], mnuv[1]);

        addIndex(i);
        addIndex(i + 1);
        addIndex(i + 2);
        addIndex(i + 1);
        addIndex(i + 3);
        addIndex(i + 2);
    }
}

Asset CreateFont(const AssetPool pool, const FontData &data)
{
    CHECK_POOL_HANDLE(pool, Asset_Font);

    const u32 pid = GetAssetPoolId(pool);
    FontPoolData &fpool = s_FontData->Pools[pid];
    FontDataInfo info;
    info.Data = data;
    info.Atlas = CreateTexture(data.AtlasData);

    const u32 gsize = data.Glyphs.GetSize();
    info.VertexStart = fpool.Vertices.GetSize();
    info.VertexCount = gsize * 4;
    info.IndexStart = fpool.Indices.GetSize();
    info.IndexCount = gsize * 6;
    info.Flags = StatusFlag_UpdateVertex | StatusFlag_UpdateIndex;

    const u32 fid = fpool.FontData.GetSize();

    const auto addVertex = [&fpool](const f32 x, const f32 y, const f32 u, const f32 v) {
        fpool.Vertices.Append(GlyphVertex{f32v2{x, y}, f32v2{u, v}});
    };

    const auto addIndex = [&fpool, &info](const u32 idx) { fpool.Indices.Append(Index(idx + info.VertexStart)); };
    buildFontBuffers(data, addVertex, addIndex);
    return CreateAssetHandle(Asset_Font, fid, pid);
}

void UpdateFont(const Asset handle, const FontData &data)
{
    CHECK_ASSET_AND_POOL_HANDLES(handle, Asset_Font);

    const u32 pid = GetAssetPoolId(handle);
    const u32 fid = GetAssetId(handle);

    FontPoolData &fpool = s_FontData->Pools[pid];
    FontDataInfo &info = fpool.FontData[fid];
    info.Flags |= StatusFlag_UpdateVertex | StatusFlag_UpdateIndex;

    TKIT_ASSERT(info.VertexCount == 4 * data.Glyphs.GetSize() && info.IndexCount == 6 * data.Glyphs.GetSize(),
                "[ONYX][ASSETS] When updating a font, the amount of glyphs must match with the old one. If they do "
                "not, you must create a new font");

    u32 vidx = info.VertexStart;
    u32 iidx = info.IndexStart;
    const auto addVertex = [&fpool, &vidx](const f32 x, const f32 y, const f32 u, const f32 v) {
        fpool.Vertices[vidx++] = GlyphVertex{f32v2{x, y}, f32v2{u, v}};
    };
    const auto addIndex = [&fpool, &info, &iidx](const u32 idx) {
        fpool.Indices[iidx++] = Index(idx + info.VertexStart);
    };
    buildFontBuffers(data, addVertex, addIndex);
}

template <typename Vertex>
static void updateMesh(const Asset handle, MeshAssetData<Vertex> &meshes, const MeshData<Vertex> &data)
{
    CHECK_ASSET_AND_POOL_HANDLES_WITH_DIM(handle, Vertex::Asset, Vertex::Dim);

    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
    MeshDataInfo<Vertex> &info = mpool.Info[mid];
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

template <Dimension D> Asset CreateMaterial(const AssetPool pool, const MaterialData<D> &data)
{
    CHECK_POOL_HANDLE_WITH_DIM(pool, Asset_Material, D);

    const u32 pid = GetAssetPoolId(pool);

    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pid];

    const u32 mid = mpool.Materials.GetSize();
    mpool.Materials.Append(data);
    mpool.Flags.Append(StatusFlag_UpdateMaterial);

    return CreateAssetHandle(Asset_Material, mid, pid);
}

template <Dimension D> void UpdateMaterial(const Asset handle, const MaterialData<D> &data)
{
    CHECK_ASSET_AND_POOL_HANDLES_WITH_DIM(handle, Asset_Material, D);

    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pid];
    mpool.Materials[mid] = data;
    mpool.Flags[mid] = StatusFlag_UpdateMaterial;
}

template <typename Vertex> static MeshData<Vertex> getMeshData(const Asset handle, MeshAssetData<Vertex> &meshes)
{
    CHECK_ASSET_AND_POOL_HANDLES_WITH_DIM(handle, Vertex::Asset, Vertex::Dim);

    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];

    MeshData<Vertex> data{};
    const MeshDataInfo<Vertex> &info = mpool.Info[mid];

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
    CHECK_ASSET_AND_POOL_HANDLES_WITH_DIM(handle, Asset_ParametricMesh, D);

    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    ParaMeshPoolData<D> &mpool = getData<D>().ParametricMeshes.Pools[pid];
    return mpool.Info[mid].Shape;
}
template <Dimension D> const MaterialData<D> &GetMaterialData(const Asset handle)
{
    CHECK_ASSET_AND_POOL_HANDLES_WITH_DIM(handle, Asset_Material, D);

    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    MaterialPoolData<D> &mpool = getData<D>().Materials.Pools[pid];
    return mpool.Materials[mid];
}

const SamplerData &GetSamplerData(const Asset handle)
{
    CHECK_ASSET_HANDLE(handle, Asset_Sampler);
    const u32 sid = GetAssetId(handle);
    return s_SamplerData->Samplers[sid].Data;
}
const TextureData &GetTextureData(const Asset handle)
{
    CHECK_ASSET_HANDLE(handle, Asset_Texture);
    const u32 tid = GetAssetId(handle);
    return s_TextureData->Textures[tid].Data;
}
const FontData &GetFontData(const Asset handle)
{
    CHECK_ASSET_AND_POOL_HANDLES(handle, Asset_Font);
    const u32 pid = GetAssetPoolId(handle);
    const u32 fid = GetAssetId(handle);

    return s_FontData->Pools[pid].FontData[fid].Data;
}
const GlyphData *GetGlyphData(const Asset font, const u32 codePoint)
{
    CHECK_ASSET_AND_POOL_HANDLES(font, Asset_Font);

    const u32 pid = GetAssetPoolId(font);
    const u32 fid = GetAssetId(font);

    const FontDataInfo &info = s_FontData->Pools[pid].FontData[fid];
    const TKit::TierArray<CodePointRange> &ranges = info.Data.CodePoints;
    for (const CodePointRange &range : ranges)
        if (codePoint < range.Last)
            return &info.Data.Glyphs[codePoint - range.First];

    TKIT_FATAL("[ONYX][ASSETS] The code point {} was not found", codePoint);
    return nullptr;
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
    for (const Asset handle : s_TextureData->ToDestroy)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying texture with handle {:#010x}", handle);
        const u32 tid = GetAssetId(handle);

        TextureInfo &tinfo = s_TextureData->Textures[tid];
        tinfo.Image.Destroy();
        if (tinfo.Flags & StatusFlag_MustFreeTexture)
            TKit::Deallocate(tinfo.Data.Data);
        s_TextureData->Textures.Remove(tid);
    }
    s_TextureData->ToDestroy.Clear();

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

                TKIT_LOG_DEBUG(
                    "[ONYX][ASSETS] Creating new texture (handle={:#010x}) with dimensions {}x{} and {} channels, "
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
                    const std::string name = TKit::Format("onyx-assets-texture-image-{:#010x}", tinfo.Handle);
                    TKIT_RETURN_IF_FAILED(img.SetName(name.c_str()), img.Destroy());
                }
                const VkDescriptorImageInfo &info =
                    imageInfos.Append(img.CreateDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

                const u32 tid = GetAssetId(tinfo.Handle);
                writer2.WriteImage(2, info, tid);
                writer3.WriteImage(2, info, tid);
            }

        for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D2>(Shading_Unlit))
            writer2.Overwrite(set);
        for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D3>(Shading_Unlit))
            writer3.Overwrite(set);
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
                TKIT_RETURN_IF_FAILED(
                    uploadBuffer.SetName(
                        TKit::Format("onyx-assets-texture-upload-buffer-{:#010x}", tinfo.Handle).c_str()),
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
    for (const Asset handle : s_SamplerData->ToDestroy)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying sampler with handle {:#010x}", handle);
        const u32 sid = GetAssetId(handle);

        SamplerInfo &sinfo = s_SamplerData->Samplers[sid];
        sinfo.Sampler.Destroy();
        s_SamplerData->Samplers.Remove(sid);
    }
    s_SamplerData->ToDestroy.Clear();

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
            TKIT_LOG_DEBUG("[ONYX][ASSETS] Updating sampler with handle {:#010x}", sinfo.Handle);

            if (Core::CanNameObjects())
            {
                TKIT_RETURN_IF_FAILED(
                    sinfo.Sampler.SetName(TKit::Format("onyx-assets-sampler-{:#010x}", sinfo.Handle).c_str()),
                    sinfo.Sampler.Destroy());
            }

            const VkDescriptorImageInfo &info = imageInfos.Append(VkDescriptorImageInfo{
                .sampler = sinfo.Sampler, .imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED});

            const u32 sid = GetAssetId(sinfo.Handle);
            writer2.WriteImage(1, info, sid);
            writer3.WriteImage(1, info, sid);
        }

    for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D2>(Shading_Unlit))
        writer2.Overwrite(set);
    for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D3>(Shading_Unlit))
        writer3.Overwrite(set);
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
    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    const MeshDataInfo<Vertex> &info = meshes.Pools[pid].Info[mid];
    return MeshDataLayout{.VertexStart = info.VertexStart,
                          .VertexCount = info.VertexCount,
                          .IndexStart = info.IndexStart,
                          .IndexCount = info.IndexCount};
}

MeshDataLayout GetFontLayout(const Asset handle)
{
    const u32 pid = GetAssetPoolId(handle);
    const u32 fid = GetAssetId(handle);

    const FontDataInfo &info = s_FontData->Pools[pid].FontData[fid];
    return MeshDataLayout{.VertexStart = info.VertexStart,
                          .VertexCount = info.VertexCount,
                          .IndexStart = info.IndexStart,
                          .IndexCount = info.IndexCount};
}

MeshDataLayout GetGlyphLayout(const Asset handle)
{
    const u32 gid = GetAssetId(handle);
    return MeshDataLayout{.VertexStart = 4 * gid, .VertexCount = 4, .IndexStart = 6 * gid, .IndexCount = 6};
}

template <Dimension D> MeshDataLayout GetMeshLayout(const Asset handle)
{
    ONYX_CHECK_ASSET_IS_NOT_NULL(handle);
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(handle);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(handle);

    const AssetType atype = GetAssetType(handle);

    switch (atype)
    {
    case Asset_StaticMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(handle, Asset_StaticMesh);
        return getMeshLayout(handle, getData<D>().StaticMeshes);
    case Asset_ParametricMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(handle, Asset_ParametricMesh);
        return getMeshLayout(handle, getData<D>().ParametricMeshes);
    case Asset_Font:
        ONYX_CHECK_ASSET_POOL_IS_VALID(handle, Asset_Font);
        return GetFontLayout(handle);
    case Asset_GlyphMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(handle, Asset_GlyphMesh);
        return GetGlyphLayout(handle);
    default:
        TKIT_FATAL("[ONYX][ASSETS] An asset of type '{}' does not have a mesh layout", ToString(atype));
        return MeshDataLayout{};
    }
}

TKit::Span<const u32> GetFontPoolIds()
{
    return s_FontData->Pools.GetValidIds();
}

template <Dimension D> TKit::Span<const u32> GetAssetPoolIds(const AssetType atype)
{
    switch (atype)
    {
    case Asset_StaticMesh:
        return getData<D>().StaticMeshes.Pools.GetValidIds();
    case Asset_ParametricMesh:
        return getData<D>().ParametricMeshes.Pools.GetValidIds();
    case Asset_Material:
        return getData<D>().Materials.Pools.GetValidIds();
    case Asset_Font:
    case Asset_GlyphMesh:
        return GetFontPoolIds();
    default:
        TKIT_FATAL("[ONYX][ASSETS] An asset of type '{}' cannot have an asset pool", ToString(atype));
        return TKit::Span<const u32>{};
    }
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

u32 GetFontCount(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);
    ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Font);

    const u32 pid = GetAssetPoolId(pool);
    return s_FontData->Pools[pid].FontData.GetSize();
}

u32 GetGlyphCount(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);
    ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_GlyphMesh);

    const u32 pid = GetAssetPoolId(pool);
    return s_FontData->Pools[pid].Vertices.GetSize() / 4;
}

template <Dimension D> u32 GetAssetCount(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);

    const AssetType atype = GetAssetType(pool);
    const u32 pid = GetAssetPoolId(pool);

    switch (atype)
    {
    case Asset_StaticMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_StaticMesh);
        return getData<D>().StaticMeshes.Pools[pid].Info.GetSize();
    case Asset_ParametricMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_ParametricMesh);
        return getData<D>().ParametricMeshes.Pools[pid].Info.GetSize();
    case Asset_Material:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Material);
        return getData<D>().Materials.Pools[pid].Materials.GetSize();
    case Asset_Font:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Font);
        return GetFontCount(pool);
    case Asset_GlyphMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_GlyphMesh);
        return GetGlyphCount(pool);
    default:
        TKIT_FATAL("[ONYX][ASSETS] An asset of type '{}' cannot have an asset pool", ToString(atype));
        return 0;
    }
}

MeshBuffers GetFontBuffers(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);
    ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Font);

    const u32 pid = GetAssetPoolId(pool);

    return {&s_FontData->Pools[pid].VertexBuffer, &s_FontData->Pools[pid].IndexBuffer};
}

MeshBuffers GetGlyphBuffers(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);
    ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_GlyphMesh);

    const u32 pid = GetAssetPoolId(pool);

    return {&s_FontData->Pools[pid].VertexBuffer, &s_FontData->Pools[pid].IndexBuffer};
}

template <Dimension D> MeshBuffers GetMeshBuffers(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);

    const AssetType atype = GetAssetType(pool);

    const u32 pid = GetAssetPoolId(pool);
    switch (atype)
    {
    case Asset_StaticMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_StaticMesh);
        return {&getData<D>().StaticMeshes.Pools[pid].VertexBuffer, &getData<D>().StaticMeshes.Pools[pid].IndexBuffer};
    case Asset_ParametricMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_ParametricMesh);
        return {&getData<D>().ParametricMeshes.Pools[pid].VertexBuffer,
                &getData<D>().ParametricMeshes.Pools[pid].IndexBuffer};
    case Asset_Font:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Font);
        return {&s_FontData->Pools[pid].VertexBuffer, &s_FontData->Pools[pid].IndexBuffer};
    case Asset_GlyphMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_GlyphMesh);
        return {&s_FontData->Pools[pid].VertexBuffer, &s_FontData->Pools[pid].IndexBuffer};
    default:
        TKIT_FATAL("[ONYX][ASSETS] An asset of type '{}' does not have a vertex buffer", ToString(atype));
        return {};
    }
}

template <Dimension D> bool IsAssetValid(const Asset handle, const AssetType atype)
{
    const u32 itype = GetAssetTypeAsInteger(handle);
    if (itype >= Asset_Count || itype != atype)
        return false;

    const u32 aid = GetAssetId(handle);
    const u32 pid = GetAssetPoolId(handle);
    switch (atype)
    {
    case Asset_StaticMesh:
        return IsAssetPoolValid<D>(handle, Asset_StaticMesh) &&
               aid < getData<D>().StaticMeshes.Pools[pid].Info.GetSize();
    case Asset_ParametricMesh:
        return IsAssetPoolValid<D>(handle, Asset_ParametricMesh) &&
               aid < getData<D>().ParametricMeshes.Pools[pid].Info.GetSize();
    case Asset_Material:
        return IsAssetPoolValid<D>(handle, Asset_Material) &&
               aid < getData<D>().Materials.Pools[pid].Materials.GetSize();
    case Asset_Font:
        return IsAssetPoolValid<D>(handle, Asset_Font) && aid < s_FontData->Pools[pid].FontData.GetSize();
    case Asset_GlyphMesh:
        return IsAssetPoolValid<D>(handle, Asset_GlyphMesh) && aid < 4 * s_FontData->Pools[pid].Vertices.GetSize() &&
               aid < 6 * s_FontData->Pools[pid].Indices.GetSize();
    case Asset_Sampler:
        return IsAssetPoolNull(handle) && s_SamplerData->Samplers.Contains(aid);
    case Asset_Texture:
        return IsAssetPoolNull(handle) && s_TextureData->Textures.Contains(aid);
    default:
        return false;
    }
}
template <Dimension D> bool IsAssetPoolValid(const Handle handle, const AssetType atype)
{
    const u32 itype = GetAssetTypeAsInteger(handle);
    if (itype >= Asset_PoolCount || itype != atype)
        return false;

    const u32 pid = GetAssetPoolId(handle);
    switch (atype)
    {
    case Asset_StaticMesh:
        return getData<D>().StaticMeshes.Pools.Contains(pid);
    case Asset_ParametricMesh:
        return getData<D>().ParametricMeshes.Pools.Contains(pid);
    case Asset_Material:
        return getData<D>().Materials.Pools.Contains(pid);
    case Asset_Font:
    case Asset_GlyphMesh:
        return s_FontData->Pools.Contains(pid);
    default:
        return false;
    }
}

bool IsAssetValid(const Asset handle, const AssetType atype)
{
    return IsAssetValid<D2>(handle, atype) || IsAssetValid<D3>(handle, atype);
}
bool IsAssetPoolValid(const Handle handle, const AssetType atype)
{
    return IsAssetPoolValid<D2>(handle, atype) || IsAssetPoolValid<D3>(handle, atype);
}

template Asset CreateMaterial(AssetPool pool, const MaterialData<D2> &data);
template Asset CreateMaterial(AssetPool pool, const MaterialData<D3> &data);

template void UpdateMaterial(Asset mesh, const MaterialData<D2> &data);
template void UpdateMaterial(Asset mesh, const MaterialData<D3> &data);

template Asset CreateMesh(AssetPool pool, const StatMeshData<D2> &data);
template Asset CreateMesh(AssetPool pool, const StatMeshData<D3> &data);

template void UpdateMesh(Asset handle, const StatMeshData<D2> &data);
template void UpdateMesh(Asset handle, const StatMeshData<D3> &data);

template Asset CreateMesh(AssetPool pool, const ParaMeshData<D2> &data);
template Asset CreateMesh(AssetPool pool, const ParaMeshData<D3> &data);

template void UpdateMesh(Asset handle, const ParaMeshData<D2> &data);
template void UpdateMesh(Asset handle, const ParaMeshData<D3> &data);

template Result<AssetPool> CreateAssetPool<D2>(AssetType atype);
template Result<AssetPool> CreateAssetPool<D3>(AssetType atype);

template void DestroyAssetPool<D2>(AssetPool pool);
template void DestroyAssetPool<D3>(AssetPool pool);

template StatMeshData<D2> GetStaticMeshData(Asset handle);
template StatMeshData<D3> GetStaticMeshData(Asset handle);

template ParaMeshData<D2> GetParametricMeshData(Asset handle);
template ParaMeshData<D3> GetParametricMeshData(Asset handle);

template ParametricShape GetParametricShape<D2>(Asset handle);
template ParametricShape GetParametricShape<D3>(Asset handle);

template const MaterialData<D2> &GetMaterialData(Asset handle);
template const MaterialData<D3> &GetMaterialData(Asset handle);

template GltfHandles CreateGltfAssets(AssetPool meshPool, AssetPool materialPool, GltfData<D2> &assets);
template GltfHandles CreateGltfAssets(AssetPool meshPool, AssetPool materialPool, GltfData<D3> &assets);

template TKit::Span<const u32> GetAssetPoolIds<D2>(AssetType atype);
template TKit::Span<const u32> GetAssetPoolIds<D3>(AssetType atype);

template u32 GetAssetCount<D2>(AssetPool pool);
template u32 GetAssetCount<D3>(AssetPool pool);

template MeshBuffers GetMeshBuffers<D2>(AssetPool pool);
template MeshBuffers GetMeshBuffers<D3>(AssetPool pool);

template bool IsAssetValid<D2>(Asset handle, AssetType atype);
template bool IsAssetValid<D3>(Asset handle, AssetType atype);

template bool IsAssetPoolValid<D2>(Handle handle, AssetType atype);
template bool IsAssetPoolValid<D3>(Handle handle, AssetType atype);

template MeshDataLayout GetMeshLayout<D2>(Asset handle);
template MeshDataLayout GetMeshLayout<D3>(Asset handle);

} // namespace Onyx::Assets
