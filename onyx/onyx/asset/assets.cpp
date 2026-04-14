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
#include "tkit/profiling/clock.hpp"

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
    StatusFlag_Create = 1 << 0,
    StatusFlag_Update = 1 << 1,
    StatusFlag_Destroy = 1 << 2,
    StatusFlag_MustFree = 1 << 3,
};

template <typename Vertex> struct MeshDataInfo
{
    MeshDataLayout Layout;
    Asset Bounds;
};

template <Dimension D> struct MeshDataInfo<ParaVertex<D>>
{
    MeshDataLayout Layout;
    Asset Bounds;
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
    TKit::DynamicArray<Vertex> Vertices{};
    TKit::DynamicArray<Index> Indices{};
    TKit::TierArray<MeshDataInfo<Vertex>> Meshes{};
    StatusFlags Flags = 0;
};

template <typename Vertex> struct MeshAssetData
{
    TKit::StaticHive<MeshPoolData<Vertex>, ONYX_MAX_ASSET_POOLS> Pools{};
    TKit::StaticArray<AssetPool, ONYX_MAX_ASSET_POOLS> ToDestroy{};
};

template <> struct MeshDataInfo<GlyphVertex>
{
    MeshDataLayout Layout;
    FontData Data{};
    Asset Atlas = NullHandle;
};

template <> struct MeshPoolData<GlyphVertex>
{
    VKit::DeviceBuffer VertexBuffer{};
    VKit::DeviceBuffer IndexBuffer{};
    TKit::TierArray<GlyphVertex> Vertices{};
    TKit::TierArray<Index> Indices{};
    TKit::TierArray<Glyph> Glyphs{};
    TKit::TierArray<MeshDataInfo<GlyphVertex>> Meshes{};
    StatusFlags Flags = 0;
};

template <Dimension D> using StatMeshAssetData = MeshAssetData<StatVertex<D>>;
template <Dimension D> using StatMeshPoolData = MeshPoolData<StatVertex<D>>;
template <Dimension D> using StatMeshDataInfo = MeshDataInfo<StatVertex<D>>;

template <Dimension D> using ParaMeshAssetData = MeshAssetData<ParaVertex<D>>;
template <Dimension D> using ParaMeshPoolData = MeshPoolData<ParaVertex<D>>;
template <Dimension D> using ParaMeshDataInfo = MeshDataInfo<ParaVertex<D>>;

using FontAssetData = MeshAssetData<GlyphVertex>;
using FontPoolData = MeshPoolData<GlyphVertex>;
using FontDataInfo = MeshDataInfo<GlyphVertex>;

template <typename T> struct HiveAssetData
{
    VKit::DeviceBuffer Buffer{};
    TKit::ArenaHive<T> Elements{};
    StatusFlags Flags = 0;
};

template <Dimension D> using MaterialAssetData = HiveAssetData<MaterialData<D>>;
template <Dimension D> using BoundsAssetData = HiveAssetData<BoundsData<D>>;

struct SamplerInfo
{
    VKit::Sampler Sampler{};
    SamplerData Data{};
    Asset Handle = NullHandle;
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
    ImageData Data{};
    Asset Handle = NullHandle;
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
    BoundsAssetData<D> BoundingBoxes{};
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

template <typename Vertex> static void terminateMeshes(MeshAssetData<Vertex> &meshData)
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
    materials.Buffer.Destroy();
}

template <Dimension D> static void terminateBounds()
{
    BoundsAssetData<D> &data = getData<D>().BoundingBoxes;
    data.Buffer.Destroy();
}

template <Dimension D> static void terminate()
{
    terminateMaterials<D>();
    AssetData<D> &data = getData<D>();
    terminateMeshes(data.ParametricMeshes);
    terminateMeshes(data.StaticMeshes);
    terminateBounds<D>();
}

template <Dimension D> static void updateMaterialDescriptorSet()
{
    MaterialAssetData<D> &materials = getData<D>().Materials;

    const VkDescriptorBufferInfo binfo = materials.Buffer.CreateDescriptorInfo();
    Renderer::WriteBuffer<D>(Descriptors::GetMaterialsBindingPoint(), binfo, RenderPass_Fill);
    if constexpr (D == D2)
        Renderer::WriteBuffer<D>(Descriptors::GetMaterialsBindingPoint(), binfo, RenderPass_Shadow);
}

template <Dimension D> static void updateBoundsDescriptorSet()
{
    BoundsAssetData<D> &bounds = getData<D>().BoundingBoxes;

    const VkDescriptorBufferInfo binfo = bounds.Buffer.CreateDescriptorInfo();
    Renderer::WriteBuffer<D>(Descriptors::GetBoundsBindingPoint<D>(RenderPass_Fill), binfo, RenderPass_Fill);
    Renderer::WriteBuffer<D>(Descriptors::GetBoundsBindingPoint<D>(RenderPass_Stencil), binfo, RenderPass_Stencil);
    Renderer::WriteBuffer<D>(Descriptors::GetBoundsBindingPoint<D>(RenderPass_Shadow), binfo, RenderPass_Shadow);

    if constexpr (D == D2)
        Renderer::WriteBuffer<D3>(Descriptors::GetBoundsBindingPoint<D2>(RenderPass_Stencil), binfo,
                                  RenderPass_Stencil);
}

template <typename T> ONYX_NO_DISCARD static Result<> initializeHiveAssets(const u32 capacity, HiveAssetData<T> &hive)
{
    hive.Elements.Reserve(capacity);

    const auto result = Resources::CreateBuffer<T>(Buffer_DeviceStorage);
    TKIT_RETURN_ON_ERROR(result);
    hive.Buffer = result.GetValue();
    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> initializeMaterials(const u32 capacity)
{
    TKIT_RETURN_IF_FAILED(initializeHiveAssets(capacity, getData<D>().Materials));
    updateMaterialDescriptorSet<D>();
    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> initializeBounds(const u32 capacity)
{
    TKIT_RETURN_IF_FAILED(initializeHiveAssets(capacity, getData<D>().BoundingBoxes));
    updateBoundsDescriptorSet<D>();
    return Result<>::Ok();
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

    TKIT_RETURN_IF_FAILED(initializeMaterials<D2>(specs.MaxMaterials));
    TKIT_RETURN_IF_FAILED(initializeMaterials<D3>(specs.MaxMaterials));
    TKIT_RETURN_IF_FAILED(initializeBounds<D2>(specs.MaxBounds));
    return initializeBounds<D3>(specs.MaxBounds);
}

void Terminate()
{
    terminate<D2>();
    terminate<D3>();
    terminateMeshes(*s_FontData);

    for (TextureInfo &tinfo : s_TextureData->Textures)
    {
        tinfo.Image.Destroy();
        if (tinfo.Flags & StatusFlag_MustFree)
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

#define CHECK_ASSET_HANDLE_WITH_DIM(handle, atype, dim)                                                                \
    ONYX_CHECK_ASSET_IS_NOT_NULL(handle);                                                                              \
    ONYX_CHECK_HANDLE_HAS_ASSET_TYPE(handle, atype);                                                                   \
    ONYX_CHECK_ASSET_IS_VALID_WITH_DIM(handle, atype, dim);

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

    sinfo.Flags |= StatusFlag_Update;
    return handle;
}

void UpdateSampler(const Asset handle, const SamplerData &data)
{
    CHECK_ASSET_HANDLE(handle, Asset_Sampler);

    const u32 sid = GetAssetId(handle);

    SamplerInfo &sinfo = s_SamplerData->Samplers[sid];
    sinfo.Data = data;
    sinfo.Flags |= StatusFlag_Update;
}

template <Dimension D> static void removeSamplerReferences(const Asset handle)
{
    const auto updateRef = [handle](Asset &toUpdate) -> StatusFlags {
        if (toUpdate == handle)
        {
            toUpdate = NullHandle;
            return StatusFlag_Update;
        }
        return 0;
    };

    MaterialAssetData<D> &materials = getData<D>().Materials;
    for (MaterialData<D> &mat : materials.Elements)
        if constexpr (D == D2)
            materials.Flags |= updateRef(mat.Sampler);
        else
            for (Asset &handle : mat.Samplers)
                materials.Flags |= updateRef(handle);
}

void DestroySampler(const Asset handle)
{
    CHECK_ASSET_HANDLE(handle, Asset_Sampler);

    s_SamplerData->ToDestroy.Append(handle);
    removeSamplerReferences<D2>(handle);
    removeSamplerReferences<D3>(handle);
}

template <Dimension D> GltfHandles CreateGltfAssets(const AssetPool meshPool, GltfData<D> &data)
{
    GltfHandles handles;
    handles.StaticMeshes.Reserve(data.StaticMeshes.GetSize());
    handles.Materials.Reserve(data.Materials.GetSize());
    handles.Samplers.Reserve(data.Samplers.GetSize());
    handles.Textures.Reserve(data.Images.GetSize());

    for (const StatMeshData<D> &smesh : data.StaticMeshes)
        handles.StaticMeshes.Append(CreateMesh(meshPool, smesh));

    for (const SamplerData &sdata : data.Samplers)
        handles.Samplers.Append(CreateSampler(sdata));

    for (const ImageData &idata : data.Images)
        handles.Textures.Append(CreateTexture(idata));

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
        handles.Materials.Append(CreateMaterial(mdata));
    }

    return handles;
}

Asset CreateTexture(const ImageData &data, const CreateTextureFlags flags)
{
    const u32 tid = s_TextureData->Textures.Insert();
    const Asset handle = CreateAssetHandle(Asset_Texture, tid);

    TextureInfo &tinfo = s_TextureData->Textures[tid];
    tinfo.Data = data;
    tinfo.Handle = handle;

    StatusFlags sflags = StatusFlag_Update | StatusFlag_Create;
    if (!(flags & CreateTextureFlag_ManuallyHandledMemory))
        sflags |= StatusFlag_MustFree;
    tinfo.Flags = sflags;
    return handle;
}
void UpdateTexture(const Asset handle, const ImageData &data, const CreateTextureFlags flags)
{
    CHECK_ASSET_HANDLE(handle, Asset_Texture);

    const u32 tid = GetAssetId(handle);

    TextureInfo &tinfo = s_TextureData->Textures[tid];
    TKIT_ASSERT(
        data.ComputeSize() == tinfo.Data.ComputeSize(),
        "[ONYX][ASSETS] When updating a texture, the size of the data of the previous and updated texture must be the "
        "same. If they are not, you must create a new texture");

    if (tinfo.Flags & StatusFlag_MustFree)
        TKit::Deallocate(tinfo.Data.Data);

    tinfo.Data = data;
    tinfo.Flags |= StatusFlag_Update;
#ifdef ONYX_ENABLE_GLTF_LOAD
    if (!(flags & CreateTextureFlag_ManuallyHandledMemory))
        tinfo.Flags |= StatusFlag_MustFree;
#endif
}

template <Dimension D> static void removeTextureReferences(const Asset handle)
{
    const auto updateRef = [handle](Asset &toUpdate) -> StatusFlags {
        if (toUpdate == handle)
        {
            toUpdate = NullHandle;
            return StatusFlag_Update;
        }
        return 0;
    };

    MaterialAssetData<D> &materials = getData<D>().Materials;
    for (MaterialData<D> &mat : materials.Elements)
        if constexpr (D == D2)
            materials.Flags |= updateRef(mat.Texture);
        else
            for (Asset &handle : mat.Textures)
                materials.Flags |= updateRef(handle);
}

static void destroyTexture(const Asset handle)
{
    CHECK_ASSET_HANDLE(handle, Asset_Texture);

    s_TextureData->ToDestroy.Append(handle);
    removeTextureReferences<D2>(handle);
    removeTextureReferences<D3>(handle);
}

void DestroyTexture(const Asset handle)
{
#ifdef TKIT_ENABLE_ASSERTS
    for (const FontPoolData &fpool : s_FontData->Pools)
        for (const FontDataInfo &finfo : fpool.Meshes)
        {
            TKIT_ASSERT(finfo.Atlas != handle,
                        "[ONYX][ASSETS] Attempting to destroy texture with handle {:#010x}, which is a font atlas "
                        "managed by an active font!",
                        handle);
        }
#endif
    destroyTexture(handle);
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
    if (IsDebugUtilsEnabled())
    {
        const std::string vb = TKit::Format("onyx-assets-vertex-buffer-{:#010x}", pool);
        const std::string ib = TKit::Format("onyx-assets-index-buffer-{:#010x}", pool);

        TKIT_RETURN_IF_FAILED(vbuffer.SetName(vb.c_str()), vbuffer.Destroy(); data.Pools.Remove(pid));
        TKIT_RETURN_IF_FAILED(ibuffer.SetName(ib.c_str()), ibuffer.Destroy(); data.Pools.Remove(pid));
    }

    return pool;
}

Result<AssetPool> CreateFontPool()
{
    return createMeshPool(Asset_Font, *s_FontData);
}

template <Dimension D> Result<AssetPool> CreateAssetPool(const AssetType atype)
{
    switch (atype)
    {
    case Asset_StaticMesh:
        return createMeshPool(Asset_StaticMesh, getData<D>().StaticMeshes);
    case Asset_ParametricMesh:
        return createMeshPool(Asset_ParametricMesh, getData<D>().ParametricMeshes);
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
    for (const FontDataInfo &finfo : s_FontData->Pools[pid].Meshes)
        destroyTexture(finfo.Atlas);
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
    case Asset_Font:
    case Asset_GlyphMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Font);
        DestroyFontPool(pool);
        return;
    default:
        TKIT_FATAL("[ONYX][ASSETS] An asset pool of type '{}' cannot exist", ToString(atype));
    }
}

template <typename T> static Asset createHiveAsset(const AssetType atype, const T &data, HiveAssetData<T> &hive)
{
    hive.Flags = StatusFlag_Update;
    return CreateAssetHandle(atype, hive.Elements.Insert(data));
}

template <typename T>
static void updateHiveAsset(const AssetType atype, const Asset handle, const T &data, HiveAssetData<T> &hive)
{
    CHECK_ASSET_HANDLE_WITH_DIM(handle, atype, T::Dim);
    const u32 aid = GetAssetId(handle);

    hive.Elements[aid] = data;
    hive.Flags = StatusFlag_Update;
}

template <typename T> static void destroyHiveAsset(const AssetType atype, const Asset handle, HiveAssetData<T> &hive)
{
    CHECK_ASSET_HANDLE_WITH_DIM(handle, atype, T::Dim);
    const u32 aid = GetAssetId(handle);

    hive.Elements.Remove(aid);
}

template <Dimension D> static u32 createBounds(const BoundsData<D> &data)
{
    return createHiveAsset(Asset_Bounds, data, getData<D>().BoundingBoxes);
}

template <Dimension D> static void updateBounds(const Asset handle, const BoundsData<D> &data)
{
    updateHiveAsset(Asset_Bounds, handle, data, getData<D>().BoundingBoxes);
}

template <Dimension D> static void destroyBounds(const Asset handle)
{
    destroyHiveAsset(Asset_Bounds, handle, getData<D>().BoundingBoxes);
}

template <typename Vertex>
static Asset createMesh(const AssetPool pool, MeshAssetData<Vertex> &meshes, const MeshData<Vertex> &data)
{
    constexpr AssetType atype = Vertex::Asset;
    CHECK_POOL_HANDLE_WITH_DIM(pool, atype, Vertex::Dim);

    const u32 pid = GetAssetPoolId(pool);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
    mpool.Flags = StatusFlag_Update;

    const u32 mid = mpool.Meshes.GetSize();
    const u32 vcount = mpool.Vertices.GetSize();
    const u32 icount = mpool.Indices.GetSize();

    MeshDataInfo<Vertex> &minfo = mpool.Meshes.Append();
    minfo.Layout.VertexStart = vcount;
    minfo.Layout.VertexCount = data.Vertices.GetSize();
    minfo.Layout.IndexStart = icount;
    minfo.Layout.IndexCount = data.Indices.GetSize();
    minfo.Bounds = createBounds(CreateBoundsData(data));

    if constexpr (Vertex::Geo == Geometry_Parametric)
        minfo.Shape = data.Shape;

    auto &vertices = mpool.Vertices;
    auto &indices = mpool.Indices;
    vertices.Insert(vertices.end(), data.Vertices.begin(), data.Vertices.end());
    indices.Insert(indices.end(), data.Indices.begin(), data.Indices.end());
    return CreateAssetHandle(atype, mid, pid);
}

template <Dimension D> Asset CreateMesh(const AssetPool pool, const StatMeshData<D> &data)
{
    return createMesh(pool, getData<D>().StaticMeshes, data);
}
template <Dimension D> Asset CreateMesh(const AssetPool pool, const ParaMeshData<D> &data)
{
    return createMesh(pool, getData<D>().ParametricMeshes, data);
}

template <typename F1, typename F2, typename F3>
static void buildFontBuffers(const FontData &data, const F1 addGlyph, const F2 addVertex, const F3 addIndex)
{
    for (u32 i = 0; i < data.Glyphs.GetSize(); ++i)
    {
        const GlyphData &glyph = data.Glyphs[i];
        const f32v2 &min = glyph.Bounds.Min;
        const f32v2 &max = glyph.Bounds.Max;

        const f32v2 &mnuv = glyph.MinTexCoord;
        const f32v2 &mxuv = glyph.MaxTexCoord;

        addGlyph(i, glyph);

        addVertex(min[0], min[1], mnuv[0], mnuv[1], 0.f, 1.f);
        addVertex(max[0], min[1], mxuv[0], mnuv[1], 1.f, 1.f);
        addVertex(min[0], max[1], mnuv[0], mxuv[1], 0.f, 0.f);
        addVertex(max[0], max[1], mxuv[0], mxuv[1], 1.f, 0.f);

        const u32 base = i * 4;
        addIndex(base);
        addIndex(base + 1);
        addIndex(base + 2);
        addIndex(base + 1);
        addIndex(base + 3);
        addIndex(base + 2);
    }
}

Asset CreateFont(const AssetPool pool, const FontData &data)
{
    CHECK_POOL_HANDLE(pool, Asset_Font);

    const u32 pid = GetAssetPoolId(pool);
    FontPoolData &fpool = s_FontData->Pools[pid];

    const u32 fid = fpool.Meshes.GetSize();
    FontDataInfo &finfo = fpool.Meshes.Append();
    finfo.Data = data;
    finfo.Atlas = CreateTexture(data.AtlasData);

    const u32 gsize = data.Glyphs.GetSize();
    finfo.Layout.VertexStart = fpool.Vertices.GetSize();
    finfo.Layout.VertexCount = gsize * 4;
    finfo.Layout.IndexStart = fpool.Indices.GetSize();
    finfo.Layout.IndexCount = gsize * 6;
    fpool.Flags = StatusFlag_Update;

    const u32 goffset = fpool.Glyphs.GetSize();
    const auto addGlyph = [&fpool, goffset](const u32 id, const GlyphData &data) {
        fpool.Glyphs.Append(id + goffset, createBounds(data.Bounds), data.Advance);
    };
    const auto addVertex = [&fpool](const f32 x, const f32 y, const f32 au, const f32 av, const f32 tu, const f32 tv) {
        fpool.Vertices.Append(GlyphVertex{f32v2{x, y}, f32v2{au, av}, f32v2{tu, tv}});
    };

    const auto addIndex = [&fpool, &finfo](const u32 idx) {
        fpool.Indices.Append(Index(idx + finfo.Layout.VertexStart));
    };
    buildFontBuffers(data, addGlyph, addVertex, addIndex);
    return CreateAssetHandle(Asset_Font, fid, pid);
}

void UpdateFont(const Asset handle, const FontData &data)
{
    CHECK_ASSET_AND_POOL_HANDLES(handle, Asset_Font);

    const u32 pid = GetAssetPoolId(handle);
    const u32 fid = GetAssetId(handle);

    FontPoolData &fpool = s_FontData->Pools[pid];
    fpool.Flags = StatusFlag_Update;

    FontDataInfo &finfo = fpool.Meshes[fid];
    TKIT_ASSERT(finfo.Layout.VertexCount == 4 * data.Glyphs.GetSize() &&
                    finfo.Layout.IndexCount == 6 * data.Glyphs.GetSize(),
                "[ONYX][ASSETS] When updating a font, the amount of glyphs must match with the old one. If they do "
                "not, you must create a new font");

    u32 vidx = finfo.Layout.VertexStart;
    u32 iidx = finfo.Layout.IndexStart;
    u32 gidx = finfo.Layout.VertexStart / 4;

    const auto addGlyph = [&fpool, &gidx](const u32, const GlyphData &data) {
        Glyph &glyph = fpool.Glyphs[gidx++];
        glyph.Advance = data.Advance;
        updateBounds(glyph.Bounds, data.Bounds);
    };

    const auto addVertex = [&fpool, &vidx](const f32 x, const f32 y, const f32 u, const f32 v, const f32 tu,
                                           const f32 tv) {
        fpool.Vertices[vidx++] = GlyphVertex{f32v2{x, y}, f32v2{u, v}, f32v2{tu, tv}};
    };
    const auto addIndex = [&fpool, &finfo, &iidx](const u32 idx) {
        fpool.Indices[iidx++] = Index(idx + finfo.Layout.VertexStart);
    };
    buildFontBuffers(data, addGlyph, addVertex, addIndex);
}

template <typename Vertex>
static void updateMesh(const Asset handle, MeshAssetData<Vertex> &meshes, const MeshData<Vertex> &data)
{
    CHECK_ASSET_AND_POOL_HANDLES_WITH_DIM(handle, Vertex::Asset, Vertex::Dim);

    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
    mpool.Flags = StatusFlag_Update;

    MeshDataInfo<Vertex> &minfo = mpool.Meshes[mid];
    const MeshDataLayout &layout = minfo.Layout;
    TKIT_ASSERT(data.Vertices.GetSize() == layout.VertexCount && data.Indices.GetSize() == layout.IndexCount,
                "[ONYX][ASSETS] When updating a mesh, the vertex and index count of the previous and updated mesh "
                "must be the "
                "same. If they are not, you must create a new mesh");

    updateBounds(minfo.Bounds, CreateBoundsData(data));

    TKit::ForwardCopy(mpool.Vertices.begin() + layout.VertexStart, data.Vertices.begin(), data.Vertices.end());
    TKit::ForwardCopy(mpool.Indices.begin() + layout.IndexStart, data.Indices.begin(), data.Indices.end());

    if constexpr (Vertex::Geo == Geometry_Parametric)
        minfo.Shape = data.Shape;
}

template <Dimension D> void UpdateMesh(const Asset handle, const StatMeshData<D> &data)
{
    return updateMesh(handle, getData<D>().StaticMeshes, data);
}
template <Dimension D> void UpdateMesh(const Asset handle, const ParaMeshData<D> &data)
{
    return updateMesh(handle, getData<D>().ParametricMeshes, data);
}

template <Dimension D> Asset CreateMaterial(const MaterialData<D> &data)
{
    return createHiveAsset(Asset_Material, data, getData<D>().Materials);
}

template <Dimension D> void UpdateMaterial(const Asset handle, const MaterialData<D> &data)
{
    updateHiveAsset(Asset_Material, handle, data, getData<D>().Materials);
}

template <Dimension D> void DestroyMaterial(const Asset handle)
{
    destroyHiveAsset(Asset_Material, handle, getData<D>().Materials);
}

template <typename Vertex> static MeshData<Vertex> getMeshData(const Asset handle, MeshAssetData<Vertex> &meshes)
{
    CHECK_ASSET_AND_POOL_HANDLES_WITH_DIM(handle, Vertex::Asset, Vertex::Dim);

    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];

    MeshData<Vertex> data{};
    const MeshDataInfo<Vertex> &minfo = mpool.Meshes[mid];

    const u32 vstart = minfo.Layout.VertexStart;
    const u32 vend = vstart + minfo.Layout.VertexCount;

    const u32 istart = minfo.Layout.IndexStart;
    const u32 iend = istart + minfo.Layout.IndexCount;

    data.Vertices.Insert(data.Vertices.end(), mpool.Vertices.begin() + vstart, mpool.Vertices.begin() + vend);
    data.Indices.Insert(data.Indices.end(), mpool.Indices.begin() + istart, mpool.Indices.begin() + iend);
    if constexpr (Vertex::Geo == Geometry_Parametric)
        data.Shape = minfo.Shape;

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
    return mpool.Meshes[mid].Shape;
}
template <Dimension D> const MaterialData<D> &GetMaterialData(const Asset handle)
{
    CHECK_ASSET_HANDLE_WITH_DIM(handle, Asset_Material, D);

    const u32 mid = GetAssetId(handle);

    return getData<D>().Materials.Elements[mid];
}

const SamplerData &GetSamplerData(const Asset handle)
{
    CHECK_ASSET_HANDLE(handle, Asset_Sampler);
    const u32 sid = GetAssetId(handle);
    return s_SamplerData->Samplers[sid].Data;
}
const ImageData &GetTextureData(const Asset handle)
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

    return s_FontData->Pools[pid].Meshes[fid].Data;
}
Asset GetFontAtlas(const Asset handle)
{
    CHECK_ASSET_AND_POOL_HANDLES(handle, Asset_Font);
    const u32 pid = GetAssetPoolId(handle);
    const u32 fid = GetAssetId(handle);

    return s_FontData->Pools[pid].Meshes[fid].Atlas;
}
const Glyph *GetGlyph(const Asset handle, const u32 codePoint)
{
    CHECK_ASSET_AND_POOL_HANDLES(handle, Asset_Font);

    const u32 pid = GetAssetPoolId(handle);
    const u32 fid = GetAssetId(handle);

    const FontPoolData &fpool = s_FontData->Pools[pid];
    const FontDataInfo &finfo = fpool.Meshes[fid];
    const u32 gstart = finfo.Layout.VertexStart / 4;

    const TKit::TierArray<CodePointRange> &ranges = finfo.Data.CodePoints;
    u32 csize = 0;
    for (const CodePointRange &range : ranges)
    {
        if (codePoint >= range.First && codePoint <= range.Last)
            return &fpool.Glyphs[gstart + codePoint - range.First + csize];
        csize += range.Last - range.First + 1;
    }

    TKIT_FATAL("[ONYX][ASSETS] The code point '{}' was not found", codePoint);
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

template <typename T>
ONYX_NO_DISCARD static Result<bool> uploadFromHost(VKit::DeviceBuffer &buffer, const TKit::Span<const T> data)
{
    TKIT_LOG_DEBUG("[ONYX][ASSETS]    Uploading buffer of {:L} bytes to device", data.GetBytes());
    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    const auto result = Resources::GrowBufferIfNeeded<T>(buffer, data.GetSize());
    TKIT_RETURN_ON_ERROR(result);

    TKIT_RETURN_IF_FAILED(
        buffer.UploadFromHost(pool, *queue, data.GetData(), {.srcOffset = 0, .dstOffset = 0, .size = data.GetBytes()}));
    return result.GetValue();
}

template <typename Vertex> ONYX_NO_DISCARD static Result<> uploadMeshPool(MeshPoolData<Vertex> &mpool)
{
    TKIT_RETURN_IF_FAILED(uploadFromHost<Vertex>(mpool.VertexBuffer, mpool.Vertices));
    TKIT_RETURN_IF_FAILED(uploadFromHost<Index>(mpool.IndexBuffer, mpool.Indices));

    mpool.Flags = 0;
    return Result<>::Ok();
}

template <typename Vertex> ONYX_NO_DISCARD static Result<> uploadMeshes(MeshAssetData<Vertex> &meshes)
{
    for (const AssetPool pool : meshes.ToDestroy)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS]    Destroying mesh pool with handle {:#010x}", pool);
        const u32 pid = GetAssetPoolId(pool);
        MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
        if constexpr (std::is_same_v<Vertex, GlyphVertex>)
            for (const Glyph &glyph : mpool.Glyphs)
                destroyBounds<D2>(glyph.Bounds);
        else
            for (const MeshDataInfo<Vertex> &minfo : mpool.Meshes)
                destroyBounds<Vertex::Dim>(minfo.Bounds);

        mpool.VertexBuffer.Destroy();
        mpool.IndexBuffer.Destroy();

        meshes.Pools.Remove(pid);
    }
    meshes.ToDestroy.Clear();

    for (MeshPoolData<Vertex> &mpool : meshes.Pools)
        if (mpool.Flags & StatusFlag_Update)
        {
            TKIT_RETURN_IF_FAILED(uploadMeshPool(mpool));
        }

    return Result<>::Ok();
}

template <typename T> ONYX_NO_DISCARD static Result<bool> uploadHiveAssets(HiveAssetData<T> &hive)
{
    if (!(hive.Flags & StatusFlag_Update))
        return false;

    TKit::StackArray<T> sparse{};
    sparse.Resize(hive.Elements.GetIds().GetSize());

    for (const u32 id : hive.Elements.GetValidIds())
        sparse[id] = hive.Elements[id];

    const auto result = uploadFromHost<T>(hive.Buffer, sparse);
    TKIT_RETURN_ON_ERROR(result);
    hive.Flags = 0;
    return result;
}

template <Dimension D> ONYX_NO_DISCARD static Result<> uploadMaterials()
{
    TKIT_LOG_DEBUG_IF(getData<D>().Materials.Flags & StatusFlag_Update, "[ONYX][ASSETS] Uploading {}D materials",
                      u8(D));
    const auto result = uploadHiveAssets(getData<D>().Materials);
    TKIT_RETURN_ON_ERROR(result);
    if (result.GetValue())
        updateMaterialDescriptorSet<D>();
    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> uploadBounds()
{
    TKIT_LOG_DEBUG_IF(getData<D>().BoundingBoxes.Flags & StatusFlag_Update, "[ONYX][ASSETS] Uploading {}D bounds",
                      u8(D));
    const auto result = uploadHiveAssets(getData<D>().BoundingBoxes);
    TKIT_RETURN_ON_ERROR(result);
    if (result.GetValue())
        updateBoundsDescriptorSet<D>();

    return Result<>::Ok();
}

#ifdef TKIT_ENABLE_DEBUG_LOGS
template <typename Vertex> static bool anyMeshUploads(const MeshAssetData<Vertex> &data)
{
    for (const MeshPoolData<Vertex> &mpool : data.Pools)
        if (mpool.Flags & StatusFlag_Update)
            return true;
    return false;
}
static bool anyTextureUploads()
{
    if (!s_TextureData->ToDestroy.IsEmpty())
        return true;
    for (const TextureInfo &tinfo : s_TextureData->Textures)
        if (tinfo.Flags & StatusFlag_Update)
            return true;
    return false;
}
static bool anySamplerUploads()
{
    if (!s_SamplerData->ToDestroy.IsEmpty())
        return true;
    for (const SamplerInfo &sinfo : s_SamplerData->Samplers)
        if (sinfo.Flags & StatusFlag_Update)
            return true;
    return false;
}
#endif

template <Dimension D> ONYX_NO_DISCARD static Result<> upload()
{
    TKIT_LOG_DEBUG_IF(anyMeshUploads(getData<D>().StaticMeshes), "[ONYX][ASSETS] Uploading {}D static meshes", u8(D));
    TKIT_RETURN_IF_FAILED(uploadMeshes(getData<D>().StaticMeshes));

    TKIT_LOG_DEBUG_IF(anyMeshUploads(getData<D>().ParametricMeshes), "[ONYX][ASSETS] Uploading {}D parametric meshes",
                      u8(D));
    TKIT_RETURN_IF_FAILED(uploadMeshes(getData<D>().ParametricMeshes));

    return uploadMaterials<D>();
}

ONYX_NO_DISCARD static Result<> uploadTextures()
{
    TKIT_LOG_DEBUG_IF(anyTextureUploads(), "[ONYX][ASSETS] Uploading textures");
    for (const Asset handle : s_TextureData->ToDestroy)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS]    Destroying texture with handle {:#010x}", handle);
        const u32 tid = GetAssetId(handle);

        TextureInfo &tinfo = s_TextureData->Textures[tid];
        tinfo.Image.Destroy();
        if (tinfo.Flags & StatusFlag_MustFree)
            TKit::Deallocate(tinfo.Data.Data);
        s_TextureData->Textures.Remove(tid);
    }
    s_TextureData->ToDestroy.Clear();

    StatusFlags sflags = 0;
    for (const TextureInfo &tinfo : s_TextureData->Textures)
        sflags |= tinfo.Flags;

    TKIT_ASSERT((sflags & StatusFlag_Update) || !(sflags & StatusFlag_Create),
                "[ONYX][ASSETS] If a texture needs to be created, it also needs to be updated");

    if (!(sflags & StatusFlag_Update))
        return Result<>::Ok();

    if (sflags & StatusFlag_Create)
    {
        for (TextureInfo &tinfo : s_TextureData->Textures)
            if (tinfo.Flags & StatusFlag_Create)
            {
                tinfo.Flags &= ~StatusFlag_Create;
                const ImageData &idata = tinfo.Data;

                TKIT_LOG_DEBUG(
                    "[ONYX][ASSETS]    Creating new texture (handle={:#010x}) with dimensions {}x{} and {} channels, "
                    "with size {:L} bytes",
                    tinfo.Handle, idata.Width, idata.Height, idata.Components, idata.ComputeSize());

                TKIT_ASSERT(!tinfo.Image,
                            "[ONYX][ASSETS] To create a texture, it is underlying image must not exist yet");

                auto result = VKit::DeviceImage::Builder(GetDevice(), GetVulkanAllocator(),
                                                         VkExtent2D{idata.Width, idata.Height}, idata.Format,
                                                         VKit::DeviceImageFlag_Color | VKit::DeviceImageFlag_Sampled |
                                                             VKit::DeviceImageFlag_Destination)
                                  .AddImageView()
                                  .Build();
                TKIT_RETURN_ON_ERROR(result);
                VKit::DeviceImage &img = result.GetValue();
                tinfo.Image = img;
                if (IsDebugUtilsEnabled())
                {
                    const std::string name = TKit::Format("onyx-assets-texture-image-{:#010x}", tinfo.Handle);
                    TKIT_RETURN_IF_FAILED(img.SetName(name.c_str()), img.Destroy());
                }
                const u32 tid = GetAssetId(tinfo.Handle);

                const VkDescriptorImageInfo info = img.CreateDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                // TODO(Isma): loop please
                Renderer::WriteImage<D2>(Descriptors::GetTexturesBindingPoint(), info, RenderPass_Fill, tid);
                Renderer::WriteImage<D2>(Descriptors::GetTexturesBindingPoint(), info, RenderPass_Stencil, tid);
                Renderer::WriteImage<D2>(Descriptors::GetTexturesBindingPoint(), info, RenderPass_Shadow, tid);
                Renderer::WriteImage<D3>(Descriptors::GetTexturesBindingPoint(), info, RenderPass_Fill, tid);
                Renderer::WriteImage<D3>(Descriptors::GetTexturesBindingPoint(), info, RenderPass_Stencil, tid);
                Renderer::WriteImage<D3>(Descriptors::GetTexturesBindingPoint(), info, RenderPass_Shadow, tid);
            }
    }

    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    for (TextureInfo &tinfo : s_TextureData->Textures)
        if (tinfo.Flags & StatusFlag_Update)
        {
            tinfo.Flags &= ~StatusFlag_Update;
            const ImageData &idata = tinfo.Data;
            VKit::DeviceImage &img = tinfo.Image;

            const VkDeviceSize size = img.ComputeSize();
            TKIT_ASSERT(
                size == idata.ComputeSize(),
                "[ONYX][ASSETS] Size mismatch. Device image reports {:L} bytes while texture data reports {:L} bytes",
                size, idata.ComputeSize());

            TKIT_LOG_DEBUG("[ONYX][ASSETS]    Uploading texture of size {:L} bytes", size);

            auto result =
                Resources::CreateBuffer(VKit::DeviceBufferFlag_Staging | VKit::DeviceBufferFlag_HostMapped, size);
            TKIT_RETURN_ON_ERROR(result);

            VKit::DeviceBuffer &uploadBuffer = result.GetValue();
            if (IsDebugUtilsEnabled())
            {
                TKIT_RETURN_IF_FAILED(
                    uploadBuffer.SetName(
                        TKit::Format("onyx-assets-texture-upload-buffer-{:#010x}", tinfo.Handle).c_str()),
                    uploadBuffer.Destroy());
            }
            uploadBuffer.Write(idata.Data, {.srcOffset = 0, .dstOffset = 0, .size = size});

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
            copy.imageExtent.width = idata.Width;
            copy.imageExtent.height = idata.Height;
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
    TKIT_LOG_DEBUG_IF(anySamplerUploads(), "[ONYX][ASSETS] Uploading samplers");
    for (const Asset handle : s_SamplerData->ToDestroy)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS]    Destroying sampler with handle {:#010x}", handle);
        const u32 sid = GetAssetId(handle);

        SamplerInfo &sinfo = s_SamplerData->Samplers[sid];
        sinfo.Sampler.Destroy();
        s_SamplerData->Samplers.Remove(sid);
    }
    s_SamplerData->ToDestroy.Clear();

    StatusFlags sflags = 0;
    for (const SamplerInfo &sinfo : s_SamplerData->Samplers)
        sflags |= sinfo.Flags;

    if (!(sflags & StatusFlag_Update))
        return Result<>::Ok();

    for (SamplerInfo &sinfo : s_SamplerData->Samplers)
        if (sinfo.Flags & StatusFlag_Update)
        {
            sinfo.Flags &= ~StatusFlag_Update;
            const auto result = VKit::Sampler::Builder(GetDevice())
                                    .SetMipmapMode(toVulkan(sinfo.Data.Mode))
                                    .SetMinFilter(toVulkan(sinfo.Data.MinFilter))
                                    .SetMagFilter(toVulkan(sinfo.Data.MagFilter))
                                    .SetAddressModeU(toVulkan(sinfo.Data.WrapU))
                                    .SetAddressModeV(toVulkan(sinfo.Data.WrapV))
                                    .SetAddressModeW(toVulkan(sinfo.Data.WrapW))
                                    .Build();
            TKIT_RETURN_ON_ERROR(result);

            sinfo.Sampler = result.GetValue();
            TKIT_LOG_DEBUG("[ONYX][ASSETS]    Updating sampler with handle {:#010x}", sinfo.Handle);

            if (IsDebugUtilsEnabled())
            {
                TKIT_RETURN_IF_FAILED(
                    sinfo.Sampler.SetName(TKit::Format("onyx-assets-sampler-{:#010x}", sinfo.Handle).c_str()),
                    sinfo.Sampler.Destroy());
            }

            const VkDescriptorImageInfo info = VkDescriptorImageInfo{
                .sampler = sinfo.Sampler, .imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};

            const u32 sid = GetAssetId(sinfo.Handle);
            // TODO(Isma): loop please
            Renderer::WriteImage<D2>(Descriptors::GetSamplersBindingPoint(), info, RenderPass_Fill, sid);
            Renderer::WriteImage<D2>(Descriptors::GetSamplersBindingPoint(), info, RenderPass_Stencil, sid);
            Renderer::WriteImage<D2>(Descriptors::GetSamplersBindingPoint(), info, RenderPass_Shadow, sid);
            Renderer::WriteImage<D3>(Descriptors::GetSamplersBindingPoint(), info, RenderPass_Fill, sid);
            Renderer::WriteImage<D3>(Descriptors::GetSamplersBindingPoint(), info, RenderPass_Stencil, sid);
            Renderer::WriteImage<D3>(Descriptors::GetSamplersBindingPoint(), info, RenderPass_Shadow, sid);
        }
    return Result<>::Ok();
}

Result<> Upload()
{
    TKIT_BEGIN_INFO_CLOCK();
    if (s_Flags & AssetsFlag_Locked)
        return Result<>::Error(Error_LockedAssets,
                               "[ONYX][ASSETS] Cannot upload/mutate asset data because it is locked, either by the "
                               "user or by Onyx. If using the application class, this happens automatically in-between "
                               "frames to avoid having dangling references in command buffers");
    TKIT_RETURN_IF_FAILED(DeviceWaitIdle());
    TKIT_RETURN_IF_FAILED(upload<D2>());
    TKIT_RETURN_IF_FAILED(upload<D3>());
    TKIT_RETURN_IF_FAILED(uploadTextures());
    TKIT_RETURN_IF_FAILED(uploadSamplers());
    TKIT_LOG_DEBUG_IF(anyMeshUploads(*s_FontData), "[ONYX][ASSETS] Uploading fonts");
    TKIT_RETURN_IF_FAILED(uploadMeshes(*s_FontData));

    // these here bc D3 may also request D2 bounds to be removed
    TKIT_RETURN_IF_FAILED(uploadBounds<D2>());
    TKIT_RETURN_IF_FAILED(uploadBounds<D3>());
    s_Flags &= ~AssetsFlag_MustUpload;
    Renderer::FlushAllContexts();
    TKIT_END_INFO_CLOCK(Milliseconds, "[ONYX][ASSETS] Uploaded assets in {:.2f} milliseconds");
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

    return meshes.Pools[pid].Meshes[mid].Layout;
}

MeshDataLayout GetFontLayout(const Asset handle)
{
    return getMeshLayout(handle, *s_FontData);
}

MeshDataLayout GetGlyphLayout(const Asset handle)
{
    const u32 gid = GetAssetId(handle);
    return MeshDataLayout{.VertexStart = 4 * gid, .VertexCount = 4, .IndexStart = 0, .IndexCount = 6};
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

template <typename Vertex> static Asset getMeshBounds(const Asset handle, MeshAssetData<Vertex> &meshes)
{
    const u32 pid = GetAssetPoolId(handle);
    const u32 mid = GetAssetId(handle);

    return meshes.Pools[pid].Meshes[mid].Bounds;
}

template <Dimension D> Asset GetMeshBounds(const Asset handle)
{
    ONYX_CHECK_ASSET_IS_NOT_NULL(handle);
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(handle);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(handle);

    const AssetType atype = GetAssetType(handle);
    switch (atype)
    {
    case Asset_StaticMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(handle, Asset_StaticMesh);
        return getMeshBounds(handle, getData<D>().StaticMeshes);
    case Asset_ParametricMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(handle, Asset_ParametricMesh);
        return getMeshBounds(handle, getData<D>().ParametricMeshes);
    default:
        TKIT_FATAL("[ONYX][ASSETS] An asset of type '{}' does not have well defined bounds. To access glyph bounds, "
                   "use GetBounds<D2>() with the appropiate bounds id",
                   ToString(atype));
        return TKIT_U32_MAX;
    }
}

template <Dimension D> const BoundsData<D> &GetBounds(const u32 id)
{
    return getData<D>().BoundingBoxes[id].Data;
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
        count += mpool.Meshes.GetSize();
    return count;
}

static u32 getGlyphBatchCount()
{
    u32 count = 0;
    for (const FontPoolData &fpool : s_FontData->Pools)
        count += fpool.Glyphs.GetSize();

    return count;
}

template <Dimension D> u32 GetDistinctBatchDrawCount()
{
    u32 count = 1; // circles
    count += getMeshBatchCount(getData<D>().StaticMeshes);
    count += getMeshBatchCount(getData<D>().ParametricMeshes);
    count += getGlyphBatchCount();
    return count;
}

u32 GetFontCount(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);
    ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_Font);

    const u32 pid = GetAssetPoolId(pool);
    return s_FontData->Pools[pid].Meshes.GetSize();
}

u32 GetGlyphCount(const AssetPool pool)
{
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(pool);
    ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_GlyphMesh);

    const u32 pid = GetAssetPoolId(pool);
    return s_FontData->Pools[pid].Glyphs.GetSize();
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
        return getData<D>().StaticMeshes.Pools[pid].Meshes.GetSize();
    case Asset_ParametricMesh:
        ONYX_CHECK_ASSET_POOL_IS_VALID(pool, Asset_ParametricMesh);
        return getData<D>().ParametricMeshes.Pools[pid].Meshes.GetSize();
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
               aid < getData<D>().StaticMeshes.Pools[pid].Meshes.GetSize();
    case Asset_ParametricMesh:
        return IsAssetPoolValid<D>(handle, Asset_ParametricMesh) &&
               aid < getData<D>().ParametricMeshes.Pools[pid].Meshes.GetSize();
    case Asset_Material:
        return IsAssetPoolNull(handle) && getData<D>().Materials.Elements.Contains(aid);
    case Asset_Font:
        return IsAssetPoolValid<D>(handle, Asset_Font) && aid < s_FontData->Pools[pid].Meshes.GetSize();
    case Asset_GlyphMesh:
        return IsAssetPoolValid<D>(handle, Asset_GlyphMesh) && aid < 4 * s_FontData->Pools[pid].Vertices.GetSize() &&
               aid < 6 * s_FontData->Pools[pid].Indices.GetSize();
    case Asset_Sampler:
        return IsAssetPoolNull(handle) && s_SamplerData->Samplers.Contains(aid);
    case Asset_Texture:
        return IsAssetPoolNull(handle) && s_TextureData->Textures.Contains(aid);
    case Asset_Bounds:
        return IsAssetPoolNull(handle) && getData<D>().BoundingBoxes.Elements.Contains(aid);
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

template Asset CreateMaterial(const MaterialData<D2> &data);
template Asset CreateMaterial(const MaterialData<D3> &data);

template void DestroyMaterial<D2>(Asset handle);
template void DestroyMaterial<D3>(Asset handle);

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

template GltfHandles CreateGltfAssets(AssetPool meshPool, GltfData<D2> &assets);
template GltfHandles CreateGltfAssets(AssetPool meshPool, GltfData<D3> &assets);

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

template u32 GetDistinctBatchDrawCount<D2>();
template u32 GetDistinctBatchDrawCount<D3>();

template MeshDataLayout GetMeshLayout<D2>(Asset handle);
template MeshDataLayout GetMeshLayout<D3>(Asset handle);

template Asset GetMeshBounds<D2>(Asset mesh);
template Asset GetMeshBounds<D3>(Asset mesh);

} // namespace Onyx::Assets
