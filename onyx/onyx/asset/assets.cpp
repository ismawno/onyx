#include "onyx/core/pch.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/state/descriptors.hpp"
#include "vkit/state/descriptor_set.hpp"
#include "vkit/resource/sampler.hpp"
#include "vkit/resource/device_image.hpp"
#include "tkit/container/arena_hive.hpp"
#include "tkit/container/stack_array.hpp"
#ifdef ONYX_ENABLE_OBJ_LOAD
#    define TINYOBJLOADER_IMPLEMENTATION
#    include <tiny_obj_loader.h>
#endif
#ifdef ONYX_ENABLE_GLTF_LOAD
#    define TINYGLTF_IMPLEMENTATION
#    define STB_IMAGE_IMPLEMENTATION
#    define STB_IMAGE_WRITE_IMPLEMENTATION
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_CLANG_WARNING_IGNORE("-Wdeprecated-literal-operator")
TKIT_CLANG_WARNING_IGNORE("-Wmissing-field-initializers")
TKIT_GCC_WARNING_IGNORE("-Wdeprecated-literal-operator")
TKIT_GCC_WARNING_IGNORE("-Wmissing-field-initializers")
#    include <tiny_gltf.h>
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

struct DataLayout
{
    u32 VertexStart;
    u32 VertexCount;
    u32 IndexStart;
    u32 IndexCount;
    StatusFlags Flags;
};

struct BatchRange
{
    u32 BatchStart;
    u32 BatchCount;
};

AssetsFlags s_Flags = 0;
TKit::FixedArray<BatchRange, Geometry_Count> s_BatchRanges{};

template <typename Vertex> struct MeshAssetData
{
    VKit::DeviceBuffer VertexBuffer{};
    VKit::DeviceBuffer IndexBuffer{};
    TKit::ArenaArray<DataLayout> Layouts{};
    MeshData<Vertex> Meshes{};

    u32 GetVertexCount(u32 size = TKIT_U32_MAX) const
    {
        if (Layouts.IsEmpty() || size == 0)
            return 0;
        if (size == TKIT_U32_MAX)
            size = Layouts.GetSize();
        return Layouts[size - 1].VertexStart + Layouts[size - 1].VertexCount;
    }
    u32 GetIndexCount(u32 size = TKIT_U32_MAX) const
    {
        if (Layouts.IsEmpty() || size == 0)
            return 0;
        if (size == TKIT_U32_MAX)
            size = Layouts.GetSize();
        return Layouts[size - 1].IndexStart + Layouts[size - 1].IndexCount;
    }
};

template <Dimension D> using StatMeshAssetData = MeshAssetData<StatVertex<D>>;

template <Dimension D> struct MaterialAssetData
{
    VKit::DeviceBuffer Buffer{};
    TKit::ArenaArray<MaterialData<D>> Materials{};
    TKit::ArenaArray<StatusFlags> Flags{};
};

struct SamplerInfo
{
    VKit::Sampler Sampler{};
    Onyx::Sampler Id = NullSampler;
    SamplerData Data{};
    AssetsFlags Flags = 0;
};

struct SamplerAssetData
{
    TKit::ArenaHive<SamplerInfo> Samplers{};
    TKit::ArenaArray<Sampler> ToRemove{};
};

struct TextureInfo
{
    VKit::DeviceImage Image{};
    TextureData Data{};
    Texture Id = NullTexture;
    StatusFlags Flags{};
};

struct TextureAssetData
{
    TKit::ArenaHive<TextureInfo> Textures{};
    TKit::ArenaArray<Texture> ToRemove{};
};

template <Dimension D> struct AssetData
{
    StatMeshAssetData<D> StaticMeshes{};
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

template <typename Vertex> ONYX_NO_DISCARD static Result<> checkSize(MeshAssetData<Vertex> &meshData)
{
    StatusFlags flags = 0;

    const u32 vcount = meshData.GetVertexCount();
    auto result = Resources::GrowBufferIfNeeded<Vertex>(meshData.VertexBuffer, vcount);
    TKIT_RETURN_ON_ERROR(result);

    if (result.GetValue())
    {
        flags = StatusFlag_UpdateVertex;
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Insufficient vertex buffer memory. Resized from {:L} to {:L} bytes",
                       vcount * sizeof(Vertex), meshData.VertexBuffer.GetInfo().Size);
    }

    const u32 icount = meshData.GetIndexCount();
    result = Resources::GrowBufferIfNeeded<Index>(meshData.IndexBuffer, icount);
    TKIT_RETURN_ON_ERROR(result);

    if (result.GetValue())
    {
        flags |= StatusFlag_UpdateIndex;
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Insufficient index buffer memory. Resized from {:L} to {:L} bytes",
                       icount * sizeof(Index), meshData.IndexBuffer.GetInfo().Size);
    }
    if (flags)
        for (DataLayout &layout : meshData.Layouts)
            layout.Flags |= flags;
    return Result<>::Ok();
}

template <Dimension D> void updateMaterialDescriptorSet()
{
    MaterialAssetData<D> &matData = getData<D>().Materials;
    const VkDescriptorBufferInfo binfo = matData.Buffer.CreateDescriptorInfo();
    const VKit::DescriptorSetLayout &layout = Descriptors::GetDescriptorSetLayout<D>(Shading_Lit);

    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &layout};
    writer.WriteBuffer(1, binfo);
    for (const VkDescriptorSet set : Renderer::GetDescriptorSets<D>(Shading_Lit))
        writer.Overwrite(set);
}

template <Dimension D> ONYX_NO_DISCARD static Result<> checkMaterialBufferSize()
{
    MaterialAssetData<D> &matData = getData<D>().Materials;
    const u32 mcount = matData.Materials.GetSize();
    const auto result = Resources::GrowBufferIfNeeded<MaterialData<D>>(matData.Buffer, mcount);
    TKIT_RETURN_ON_ERROR(result);
    if (result.GetValue())
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Insufficient material buffer memory. Resized from {:L} to {:L} bytes",
                       mcount * sizeof(MaterialData<D>), matData.Buffer.GetInfo().Size);
        for (StatusFlags &flags : matData.Flags)
            flags = StatusFlag_UpdateMaterial;
        updateMaterialDescriptorSet<D>();
    }
    return Result<>::Ok();
}

template <typename Vertex>
ONYX_NO_DISCARD static Result<> uploadVertexRange(MeshAssetData<Vertex> &meshData, const u32 start, const u32 end)
{
    constexpr VkDeviceSize size = sizeof(Vertex);
    const VkDeviceSize voffset = meshData.GetVertexCount(start) * size;
    const VkDeviceSize vsize = meshData.GetVertexCount(end) * size - voffset;

    TKIT_LOG_DEBUG("[ONYX][ASSETS] Uploading vertex range: {} - {} ({:L} bytes)", meshData.GetVertexCount(start),
                   meshData.GetVertexCount(end), vsize);

    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    return meshData.VertexBuffer.UploadFromHost(pool, queue->GetHandle(), meshData.Meshes.Vertices.GetData(),
                                                {.srcOffset = voffset, .dstOffset = voffset, .size = vsize});
}
template <typename Vertex>
ONYX_NO_DISCARD static Result<> uploadIndexRange(MeshAssetData<Vertex> &meshData, const u32 start, const u32 end)
{
    constexpr VkDeviceSize size = sizeof(Index);
    const VkDeviceSize ioffset = meshData.GetIndexCount(start) * size;
    const VkDeviceSize isize = meshData.GetIndexCount(end) * size - ioffset;

    TKIT_LOG_DEBUG("[ONYX][ASSETS] Uploading index range: {} - {} ({:L} bytes)", meshData.GetIndexCount(start),
                   meshData.GetIndexCount(end), isize);

    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    return meshData.IndexBuffer.UploadFromHost(pool, queue->GetHandle(), meshData.Meshes.Indices.GetData(),
                                               {.srcOffset = ioffset, .dstOffset = ioffset, .size = isize});
}

template <typename Vertex> ONYX_NO_DISCARD static Result<> uploadMeshData(MeshAssetData<Vertex> &meshData)
{
    if (meshData.Layouts.IsEmpty())
        return Result<>::Ok();
    const auto checkFlags = [&meshData](const u32 index, const StatusFlags flags) {
        return flags & meshData.Layouts[index].Flags;
    };

    TKIT_RETURN_IF_FAILED(checkSize(meshData));

    auto &layouts = meshData.Layouts;
    for (u32 i = 0; i < layouts.GetSize(); ++i)
        if (checkFlags(i, StatusFlag_UpdateVertex))
            for (u32 j = i + 1; j <= layouts.GetSize(); ++j)
                if (j == layouts.GetSize() || !checkFlags(j, StatusFlag_UpdateVertex))
                {
                    TKIT_RETURN_IF_FAILED(uploadVertexRange(meshData, i, j));
                    i = j;
                    break;
                }
    for (u32 i = 0; i < layouts.GetSize(); ++i)
        if (checkFlags(i, StatusFlag_UpdateIndex))
            for (u32 j = i + 1; j <= layouts.GetSize(); ++j)
                if (j == layouts.GetSize() || !checkFlags(j, StatusFlag_UpdateIndex))
                {
                    TKIT_RETURN_IF_FAILED(uploadIndexRange(meshData, i, j));
                    i = j;
                    break;
                }
    for (DataLayout &layout : layouts)
        layout.Flags = 0;
    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> uploadMaterialRange(const u32 start, const u32 end)
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    const VkDeviceSize offset = start * sizeof(MaterialData<D>);
    const VkDeviceSize size = end * sizeof(MaterialData<D>) - offset;

    TKIT_LOG_DEBUG("[ONYX][ASSETS] Uploading material range: {} - {} ({:L} bytes)", start, end, size);

    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    return materials.Buffer.UploadFromHost(pool, queue->GetHandle(), materials.Materials.GetData(),
                                           {.srcOffset = offset, .dstOffset = offset, .size = size});
}

template <Dimension D> ONYX_NO_DISCARD static Result<> uploadMaterialData()
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    if (materials.Materials.IsEmpty())
        return Result<>::Ok();

    TKIT_ASSERT(materials.Materials.GetSize() == materials.Flags.GetSize(),
                "[ONYX][ASSETS] Material data size ({}) and flags size ({}) mismatch", materials.Materials.GetSize(),
                materials.Flags.GetSize());

    const auto mustUpdate = [&materials](const u32 index) {
        return StatusFlag_UpdateMaterial & materials.Flags[index];
    };

    TKIT_RETURN_IF_FAILED(checkMaterialBufferSize<D>());

    const u32 size = materials.Materials.GetSize();
    for (u32 i = 0; i < size; ++i)
        if (mustUpdate(i))
            for (u32 j = i + 1; j <= size; ++j)
                if (j == size || !mustUpdate(j))
                {
                    TKIT_RETURN_IF_FAILED(uploadMaterialRange<D>(i, j));
                    i = j;
                    break;
                }

    for (StatusFlags &flags : materials.Flags)
        flags = 0;

    return Result<>::Ok();
}

template <typename Vertex>
ONYX_NO_DISCARD static Result<> initialize(MeshAssetData<Vertex> &meshData, const u32 maxLayouts)
{
    auto result = Resources::CreateBuffer<Vertex>(Buffer_DeviceVertex);
    TKIT_RETURN_ON_ERROR(result);
    meshData.VertexBuffer = result.GetValue();

    result = Resources::CreateBuffer<Index>(Buffer_DeviceIndex);
    TKIT_RETURN_ON_ERROR(result);
    meshData.IndexBuffer = result.GetValue();
    meshData.Layouts.Reserve(maxLayouts);

    if (Core::CanNameObjects())
    {
        TKIT_RETURN_IF_FAILED(meshData.VertexBuffer.SetName(Vertex::Dim == D2 ? "onyx-assets-2D-vertex-buffer"
                                                                              : "onyx-assets-3D-vertex-buffer"));
        return meshData.IndexBuffer.SetName(Vertex::Dim == D2 ? "onyx-assets-2D-index-buffer"
                                                              : "onyx-assets-3D-index-buffer");
    }

    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> initializeMaterials(const u32 maxMaterials)
{
    MaterialAssetData<D> &matData = getData<D>().Materials;
    const auto result = Resources::CreateBuffer<MaterialData<D>>(Buffer_DeviceStorage);
    TKIT_RETURN_ON_ERROR(result);
    matData.Buffer = result.GetValue();
    matData.Materials.Reserve(maxMaterials);
    matData.Flags.Reserve(maxMaterials);

    updateMaterialDescriptorSet<D>();

    if (Core::CanNameObjects())
        return matData.Buffer.SetName(D == D2 ? "onyx-assets-2D-material-buffer" : "onyx-assets-3D-material-buffer");

    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> initialize(const Specs &specs)
{
    AssetData<D> &data = getData<D>();
    TKIT_RETURN_IF_FAILED(initialize(data.StaticMeshes, specs.MaxStaticMeshes));
    return initializeMaterials<D>(specs.MaxMaterials);
}

template <typename Vertex> static void terminate(MeshAssetData<Vertex> &meshData)
{
    meshData.VertexBuffer.Destroy();
    meshData.IndexBuffer.Destroy();
}

template <Dimension D> static void terminateMaterials()
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    materials.Buffer.Destroy();
}

template <Dimension D> static void terminate()
{
    AssetData<D> &data = getData<D>();
    terminateMaterials<D>();
    terminate(data.StaticMeshes);
}

Result<> Initialize(const Specs &specs)
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

    s_BatchRanges[Geometry_Circle] = {.BatchStart = 0, .BatchCount = 1};
    s_BatchRanges[Geometry_StaticMesh] = {.BatchStart = 1, .BatchCount = specs.MaxStaticMeshes};

    TKIT_RETURN_IF_FAILED(initialize<D2>(specs));
    return initialize<D3>(specs);
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

u32 GetStaticMeshBatchIndex(const Mesh handle)
{
    TKIT_ASSERT(
        handle < s_BatchRanges[Geometry_StaticMesh].BatchCount,
        "[ONYX][ASSETS] Mesh index overflow. The mesh index {} does not have a valid assigned batch. This may have "
        "happened because the used mesh does not point to a valid static mesh, or the amount of static mesh "
        "types exceeds the maximum of {}. Consider increasing such maximum from the Onyx initialization specs",
        handle, s_BatchRanges[Geometry_StaticMesh].BatchCount);
    return handle + s_BatchRanges[Geometry_StaticMesh].BatchStart;
}
u32 GetStaticMeshIndexFromBatch(const u32 batch)
{
    TKIT_ASSERT(
        batch < GetBatchEnd(Geometry_StaticMesh),
        "[ONYX][ASSETS] Batch index overflow. The batch index {} does not have a valid assigned mesh. This may have "
        "happened because the used batch does not point to a valid static mesh batch",
        batch, GetBatchEnd(Geometry_StaticMesh));
    return batch - s_BatchRanges[Geometry_StaticMesh].BatchStart;
}
u32 GetCircleBatchIndex()
{
    return s_BatchRanges[Geometry_Circle].BatchStart;
}

u32 GetBatchStart(const Geometry geo)
{
    return s_BatchRanges[geo].BatchStart;
}
u32 GetBatchEnd(const Geometry geo)
{
    return s_BatchRanges[geo].BatchStart + s_BatchRanges[geo].BatchCount;
}
u32 GetBatchCount(const Geometry geo)
{
    return s_BatchRanges[geo].BatchCount;
}
u32 GetBatchCount()
{
    u32 count = 0;
    for (u32 i = 0; i < Geometry_Count; ++i)
        count += s_BatchRanges[i].BatchCount;
    return count;
}

Sampler AddSampler(const SamplerData &data)
{
    const Sampler handle = s_SamplerData->Samplers.Insert();
    SamplerInfo &sinfo = s_SamplerData->Samplers[handle];
    sinfo.Id = handle;
    sinfo.Data = data;

    sinfo.Flags |= StatusFlag_UpdateSampler;
    return handle;
}

void UpdateSampler(const Sampler handle, const SamplerData &data)
{
    SamplerInfo &sinfo = s_SamplerData->Samplers[handle];
    sinfo.Data = data;
    sinfo.Flags |= StatusFlag_UpdateSampler;
}

template <Dimension D> static void removeSamplerReferences(const Sampler handle)
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    const auto updateRef = [handle](Sampler &toUpdate) -> StatusFlags {
        if (toUpdate == handle)
        {
            toUpdate = NullSampler;
            return StatusFlag_UpdateMaterial;
        }
        return 0;
    };
    auto &matData = materials.Materials;
    auto &flags = materials.Flags;

    TKIT_ASSERT(matData.GetSize() == flags.GetSize(),
                "[ONYX][ASSETS] Material data size ({}) and flags size ({}) mismatch", matData.GetSize(),
                flags.GetSize());

    for (u32 i = 0; i < matData.GetSize(); ++i)
        if constexpr (D == D2)
            flags[i] |= updateRef(matData[i].Sampler);
        else
            for (Sampler &handle : matData[i].Samplers)
                flags[i] |= updateRef(handle);
}

void RemoveSampler(const Sampler handle)
{
    s_SamplerData->ToRemove.Append(handle);
    removeSamplerReferences<D2>(handle);
    removeSamplerReferences<D3>(handle);
}

template <Dimension D> GltfHandles AddGltfAssets(GltfAssets<D> &assets)
{
    GltfHandles handles;
    handles.StaticMeshes.Reserve(assets.StaticMeshes.GetSize());
    handles.Materials.Reserve(assets.Materials.GetSize());
    handles.Samplers.Reserve(assets.Samplers.GetSize());
    handles.Textures.Reserve(assets.Textures.GetSize());

    for (const StatMeshData<D> &smesh : assets.StaticMeshes)
        handles.StaticMeshes.Append(AddMesh(smesh));

    for (const SamplerData &sdata : assets.Samplers)
        handles.Samplers.Append(AddSampler(sdata));

    for (const TextureData &tdata : assets.Textures)
        handles.Textures.Append(AddTexture(tdata));

    for (MaterialData<D> &mdata : assets.Materials)
    {
        if constexpr (D == D2)
        {
            if (mdata.Sampler != NullSampler)
                mdata.Sampler = handles.Samplers[mdata.Sampler];
            if (mdata.Texture != NullTexture)
                mdata.Texture = handles.Textures[mdata.Texture];
        }
        else
        {
            for (Sampler &sampler : mdata.Samplers)
                if (sampler != NullSampler)
                    sampler = handles.Samplers[sampler];
            for (Texture &tex : mdata.Textures)
                if (tex != NullSampler)
                    tex = handles.Textures[tex];
        }
        handles.Materials.Append(AddMaterial(mdata));
    }

    return handles;
}

Texture AddTexture(const TextureData &data, const AddTextureFlags flags)
{
#ifndef ONYX_ENABLE_GLTF_LOAD
    TKIT_ASSERT(flags & AddTextureFlag_ManuallyHandledMemory,
                "[ONYX][ASSETS] If GLTF load is disabled, all texture data CPU memory must be handled by the user. "
                "This must be reflected by passing the AddTextureFlag_ManuallyHandledMemory flag");
#endif
    const Texture tex = s_TextureData->Textures.Insert();
    TextureInfo &tinfo = s_TextureData->Textures[tex];
    tinfo.Data = data;
    tinfo.Id = tex;

    StatusFlags sflags = StatusFlag_UpdateTexture | StatusFlag_CreateTexture;
#ifdef ONYX_ENABLE_GLTF_LOAD
    if (!(flags & AddTextureFlag_ManuallyHandledMemory))
        sflags |= StatusFlag_MustFreeTexture;
#endif
    tinfo.Flags = sflags;
    return tex;
}
void UpdateTexture(const Texture tex, const TextureData &data, const AddTextureFlags flags)
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

template <Dimension D> static void removeTextureReferences(const Texture handle)
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    const auto updateRef = [handle](Texture &toUpdate) -> StatusFlags {
        if (toUpdate == handle)
        {
            toUpdate = NullTexture;
            return StatusFlag_UpdateMaterial;
        }
        return 0;
    };
    auto &matData = materials.Materials;
    auto &flags = materials.Flags;

    TKIT_ASSERT(matData.GetSize() == flags.GetSize(),
                "[ONYX][ASSETS] Material data size ({}) and flags size ({}) mismatch", matData.GetSize(),
                flags.GetSize());

    for (u32 i = 0; i < matData.GetSize(); ++i)
        if constexpr (D == D2)
            flags[i] |= updateRef(matData[i].Texture);
        else
            for (Texture &handle : matData[i].Textures)
                flags[i] |= updateRef(handle);
}

void RemoveTexture(const Texture tex)
{
    s_TextureData->ToRemove.Append(tex);
    removeTextureReferences<D2>(tex);
    removeTextureReferences<D3>(tex);
}

template <Dimension D> Mesh AddMesh(const StatMeshData<D> &data)
{
    StatMeshAssetData<D> &meshes = getData<D>().StaticMeshes;

    const Mesh handle = meshes.Layouts.GetSize();
    DataLayout layout;
    layout.VertexStart = meshes.GetVertexCount();
    layout.VertexCount = data.Vertices.GetSize();
    layout.IndexStart = meshes.GetIndexCount();
    layout.IndexCount = data.Indices.GetSize();
    layout.Flags = StatusFlag_UpdateVertex | StatusFlag_UpdateIndex;

    meshes.Layouts.Append(layout);

    auto &vertices = meshes.Meshes.Vertices;
    auto &indices = meshes.Meshes.Indices;
    vertices.Insert(vertices.end(), data.Vertices.begin(), data.Vertices.end());
    indices.Insert(indices.end(), data.Indices.begin(), data.Indices.end());

    return handle;
}

template <Dimension D> void UpdateMesh(const Mesh handle, const StatMeshData<D> &data)
{
    StatMeshAssetData<D> &meshes = getData<D>().StaticMeshes;

    DataLayout &layout = meshes.Layouts[handle];
    TKIT_ASSERT(
        data.Vertices.GetSize() == layout.VertexCount && data.Indices.GetSize() == layout.IndexCount,
        "[ONYX][ASSETS] When updating a mesh, the vertex and index count of the previous and updated mesh must be the "
        "same. If they are not, you must create a new mesh");

    TKit::ForwardCopy(meshes.Meshes.Vertices.begin() + layout.VertexStart, data.Vertices.begin(), data.Vertices.end());
    TKit::ForwardCopy(meshes.Meshes.Indices.begin() + layout.IndexStart, data.Indices.begin(), data.Indices.end());

    layout.Flags |= StatusFlag_UpdateVertex | StatusFlag_UpdateIndex;
}

template <Dimension D> Material AddMaterial(const MaterialData<D> &data)
{
    MaterialAssetData<D> &materials = getData<D>().Materials;

    const Material handle = materials.Materials.GetSize();
    materials.Materials.Append(data);
    materials.Flags.Append(StatusFlag_UpdateMaterial);

    return handle;
}

template <Dimension D> void UpdateMaterial(const Material handle, const MaterialData<D> &data)
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    materials.Materials[handle] = data;
    materials.Flags[handle] = StatusFlag_UpdateMaterial;
}

template <Dimension D> StatMeshData<D> GetStaticMeshData(const Mesh handle)
{
    StatMeshAssetData<D> &meshes = getData<D>().StaticMeshes;
    StatMeshData<D> data{};
    const DataLayout &layout = meshes.Layouts[handle];

    const u32 vstart = layout.VertexStart;
    const u32 vend = vstart + layout.VertexCount;

    const u32 istart = layout.IndexStart;
    const u32 iend = vstart + layout.IndexCount;

    const auto &m = meshes.Meshes;
    data.Vertices.Insert(data.Vertices.end(), m.Vertices.begin() + vstart, m.Vertices.begin() + vend);
    data.Indices.Insert(data.Indices.end(), m.Indices.begin() + istart, m.Indices.begin() + iend);

    return data;
}
template <Dimension D> const MaterialData<D> &GetMaterialData(const Material handle)
{
    MaterialAssetData<D> &materials = getData<D>().Materials;
    return materials.Materials[handle];
}
const TextureData &GetTextureData(const Texture texture)
{
    return s_TextureData->Textures[texture].Data;
}

template <Dimension D> u32 GetStaticMeshCount()
{
    return getData<D>().StaticMeshes.Layouts.GetSize();
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
    TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());

    AssetData<D> &data = getData<D>();
    TKIT_RETURN_IF_FAILED(uploadMeshData(data.StaticMeshes));
    return uploadMaterialData<D>();
}

ONYX_NO_DISCARD static Result<> uploadTextures()
{
    for (const Texture handle : s_TextureData->ToRemove)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying texture with handle {}", handle);
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
                               tinfo.Id, tdata.Width, tdata.Height, tdata.Components, tdata.ComputeSize());

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
                    const std::string name = TKit::Format("onyx-assets-texture-image-{}", tinfo.Id);
                    TKIT_RETURN_IF_FAILED(img.SetName(name.c_str()));
                }
                const VkDescriptorImageInfo &info =
                    imageInfos.Append(img.CreateDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
                writer2.WriteImage(3, info, tinfo.Id);
                writer3.WriteImage(3, info, tinfo.Id);
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
    for (const Sampler handle : s_SamplerData->ToRemove)
    {
        TKIT_LOG_DEBUG("[ONYX][ASSETS] Destroying sampler with handle {}", handle);
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
            TKIT_LOG_DEBUG("[ONYX][ASSETS] Updating sampler with handle {}", sinfo.Id);
            const VkDescriptorImageInfo &info = imageInfos.Append(VkDescriptorImageInfo{
                .sampler = sinfo.Sampler, .imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED});
            writer2.WriteImage(2, info, sinfo.Id);
            writer3.WriteImage(2, info, sinfo.Id);
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
    TKIT_RETURN_IF_FAILED(upload<D2>());
    TKIT_RETURN_IF_FAILED(upload<D3>());
    TKIT_RETURN_IF_FAILED(uploadTextures());
    TKIT_RETURN_IF_FAILED(uploadSamplers());
    s_Flags &= ~AssetsFlag_MustUpload;
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

template <Dimension D> MeshDataLayout GetStaticMeshLayout(const Mesh handle)
{
    const DataLayout &layout = getData<D>().StaticMeshes.Layouts[handle];
    return MeshDataLayout{.VertexStart = layout.VertexStart,
                          .VertexCount = layout.VertexCount,
                          .IndexStart = layout.IndexStart,
                          .IndexCount = layout.IndexCount};
}

template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshVertexBuffer()
{
    return getData<D>().StaticMeshes.VertexBuffer;
}
template <Dimension D> const VKit::DeviceBuffer &GetStaticMeshIndexBuffer()
{
    return getData<D>().StaticMeshes.IndexBuffer;
}

#ifdef ONYX_ENABLE_OBJ_LOAD
template <Dimension D> Result<StatMeshData<D>> LoadStaticMeshFromObjFile(const char *path)
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
            if (!meshData.Vertices.IsEmpty() && (flags & LoadGltfDataFlag_CenterVerticesAroundOrigin))
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
                matData.Sampler = Sampler(model.textures[texIdx].sampler);
                matData.Texture = Texture(model.textures[texIdx].source);
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
                matData.Samplers[TextureSlot_Albedo] = Sampler(model.textures[albedoIdx].sampler);
                matData.Textures[TextureSlot_Albedo] = Texture(model.textures[albedoIdx].source);
            }

            const i32 mrIdx = pbr.metallicRoughnessTexture.index;
            if (mrIdx >= 0)
            {
                matData.Samplers[TextureSlot_MetallicRoughness] = Sampler(model.textures[mrIdx].sampler);
                matData.Textures[TextureSlot_MetallicRoughness] = Texture(model.textures[mrIdx].source);
            }

            const i32 normalIdx = mat.normalTexture.index;
            if (normalIdx >= 0)
            {
                matData.Samplers[TextureSlot_Normal] = Sampler(model.textures[normalIdx].sampler);
                matData.Textures[TextureSlot_Normal] = Texture(model.textures[normalIdx].source);
            }

            const i32 occlusionIdx = mat.occlusionTexture.index;
            if (occlusionIdx >= 0)
            {
                matData.Samplers[TextureSlot_Occlusion] = Sampler(model.textures[occlusionIdx].sampler);
                matData.Textures[TextureSlot_Occlusion] = Texture(model.textures[occlusionIdx].source);
            }

            const i32 emissiveIdx = mat.emissiveTexture.index;
            if (emissiveIdx >= 0)
            {
                matData.Samplers[TextureSlot_Emissive] = Sampler(model.textures[emissiveIdx].sampler);
                matData.Textures[TextureSlot_Emissive] = Texture(model.textures[emissiveIdx].source);
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

#ifdef TKIT_ENABLE_ASSERTS
template <Dimension D> static void validateMesh(const StatMeshData<D> &data, const u32 offset = 0)
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
}
#    define VALIDATE_MESH(...) validateMesh(__VA_ARGS__)
#else
#    define VALIDATE_MESH(...)
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
template <Dimension D> StatMeshData<D> CreateSquareMesh()
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
            addVertex(f32v3{0.f, c, s}, uv);
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

StatMeshData<D3> CreateCubeMesh()
{
    StatMeshData<D3> data{};
    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 n0, const f32 n1, const f32 n2,
                                   const f32 u, const f32 v, const f32v4 &tangent) {
        data.Vertices.Append(StatVertex<D3>{f32v3{x, y, z}, f32v2{u, v}, f32v3{n0, n1, n2}, tangent});
    };
    const auto addQuad = [&data](const Index a, const Index b, const Index c, const Index d) {
        data.Indices.Append(a);
        data.Indices.Append(b);
        data.Indices.Append(c);
        data.Indices.Append(a);
        data.Indices.Append(c);
        data.Indices.Append(d);
    };
    const f32v4 tPosZ{0.f, 0.f, 1.f, 1.f};  // -X face: U goes along +Z
    const f32v4 tPosX{1.f, 0.f, 0.f, 1.f};  // +Z face: U goes along +X
    const f32v4 tNegZ{0.f, 0.f, -1.f, 1.f}; // +X face: U goes along -Z
    const f32v4 tNegX{-1.f, 0.f, 0.f, 1.f}; // -Z face: U goes along -X
    const f32v4 tTopX{1.f, 0.f, 0.f, 1.f};  // +Y face: U goes along +X
    const f32v4 tBotX{-1.f, 0.f, 0.f, 1.f}; // -Y face: U goes along -X

    Index base = 0;
    addVertex(-0.5f, 0.5f, -0.5f, -1.f, 0.f, 0.f, 0.f, 0.f, tPosZ);
    addVertex(-0.5f, -0.5f, -0.5f, -1.f, 0.f, 0.f, 0.f, 1.f, tPosZ);
    addVertex(-0.5f, -0.5f, 0.5f, -1.f, 0.f, 0.f, 1.f, 1.f, tPosZ);
    addVertex(-0.5f, 0.5f, 0.5f, -1.f, 0.f, 0.f, 1.f, 0.f, tPosZ);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(-0.5f, 0.5f, 0.5f, 0.f, 0.f, 1.f, 0.f, 0.f, tPosX);
    addVertex(-0.5f, -0.5f, 0.5f, 0.f, 0.f, 1.f, 0.f, 1.f, tPosX);
    addVertex(0.5f, -0.5f, 0.5f, 0.f, 0.f, 1.f, 1.f, 1.f, tPosX);
    addVertex(0.5f, 0.5f, 0.5f, 0.f, 0.f, 1.f, 1.f, 0.f, tPosX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(0.5f, 0.5f, 0.5f, 1.f, 0.f, 0.f, 0.f, 0.f, tNegZ);
    addVertex(0.5f, -0.5f, 0.5f, 1.f, 0.f, 0.f, 0.f, 1.f, tNegZ);
    addVertex(0.5f, -0.5f, -0.5f, 1.f, 0.f, 0.f, 1.f, 1.f, tNegZ);
    addVertex(0.5f, 0.5f, -0.5f, 1.f, 0.f, 0.f, 1.f, 0.f, tNegZ);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(0.5f, 0.5f, -0.5f, 0.f, 0.f, -1.f, 0.f, 0.f, tNegX);
    addVertex(0.5f, -0.5f, -0.5f, 0.f, 0.f, -1.f, 0.f, 1.f, tNegX);
    addVertex(-0.5f, -0.5f, -0.5f, 0.f, 0.f, -1.f, 1.f, 1.f, tNegX);
    addVertex(-0.5f, 0.5f, -0.5f, 0.f, 0.f, -1.f, 1.f, 0.f, tNegX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(-0.5f, 0.5f, 0.5f, 0.f, 1.f, 0.f, 0.f, 0.f, tTopX);
    addVertex(0.5f, 0.5f, 0.5f, 0.f, 1.f, 0.f, 1.f, 0.f, tTopX);
    addVertex(0.5f, 0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f, 1.f, tTopX);
    addVertex(-0.5f, 0.5f, -0.5f, 0.f, 1.f, 0.f, 0.f, 1.f, tTopX);
    addQuad(base + 0, base + 1, base + 2, base + 3);
    base += 4;
    addVertex(0.5f, -0.5f, 0.5f, 0.f, -1.f, 0.f, 0.f, 0.f, tBotX);
    addVertex(-0.5f, -0.5f, 0.5f, 0.f, -1.f, 0.f, 1.f, 0.f, tBotX);
    addVertex(-0.5f, -0.5f, -0.5f, 0.f, -1.f, 0.f, 1.f, 1.f, tBotX);
    addVertex(0.5f, -0.5f, -0.5f, 0.f, -1.f, 0.f, 0.f, 1.f, tBotX);
    addQuad(base + 0, base + 1, base + 2, base + 3);

    VALIDATE_MESH(data);
    return data;
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
            idx = 0;
        else if (ring == rings - 1)
            idx = 1 + (rings - 2) * sectors;
        else
            idx = 1 + sector + (ring - 1) * sectors;
        data.Indices.Append(Index(idx));
    };

    addVertex(0.f, 0.5f, 0.f, 0.5f, 0.f, f32v4{1.f, 0.f, 0.f, 1.f});
    for (u32 i = 1; i < rings - 1; ++i)
    {
        const f32 v = f32(i) / rings;
        const f32 phi = v * Math::Pi<f32>();

        const f32 pc = Math::Cosine(phi);
        const f32 ps = Math::Sine(phi);

        for (u32 j = 0; j < sectors; ++j)
        {
            const f32 u = f32(j) / sectors;
            const f32 th = 2.f * u * Math::Pi<f32>();

            const f32 tc = Math::Cosine(th);
            const f32 ts = Math::Sine(th);
            addVertex(0.5f * ps * tc, 0.5f * pc, 0.5f * ps * ts, u, v, f32v4{-ts, 0.f, tc, 1.f});

            const u32 ii = i - 1;
            const u32 jj = (j + 1) % sectors;
            addIndex(i, jj);
            addIndex(i, j);
            addIndex(ii, j);
            if (i != 1 && i != rings - 1)
            {
                addIndex(i, jj);
                addIndex(ii, j);
                addIndex(ii, jj);
            }
        }
    }
    addVertex(0.f, -0.5f, 0.f, 0.5f, 1.f, f32v4{1.f, 0.f, 0.f, 1.f});

    for (u32 j = 0; j < sectors; ++j)
    {
        addIndex(rings - 2, j);
        addIndex(rings - 2, (j + 1) % sectors);
        addIndex(rings - 1, j);
    }

    VALIDATE_MESH(data);
    return data;
}
StatMeshData<D3> CreateCylinderMesh(const u32 sides)
{
    const StatMeshData<D3> left = createRegularPolygon<D3, true, false>(
        sides, f32v3{-0.5f, 0.f, 0.f}, 0, f32v3{-1.f, 0.f, 0.f}, f32v4{0.f, 0.f, 1.f, 1.f});

    const StatMeshData<D3> right = createRegularPolygon<D3, true, true>(
        sides, f32v3{0.5f, 0.f, 0.f}, left.Vertices.GetSize(), f32v3{1.f, 0.f, 0.f}, f32v4{0.f, 0.f, 1.f, 1.f});

    StatMeshData<D3> data{};
    data.Indices.Insert(data.Indices.end(), left.Indices.begin(), left.Indices.end());
    data.Indices.Insert(data.Indices.end(), right.Indices.begin(), right.Indices.end());

    data.Vertices.Insert(data.Vertices.end(), left.Vertices.begin(), left.Vertices.end());
    data.Vertices.Insert(data.Vertices.end(), right.Vertices.begin(), right.Vertices.end());

    const auto addVertex = [&data](const f32 x, const f32 y, const f32 z, const f32 u, const f32 v,
                                   const f32v4 &tangent) {
        const f32v3 vertex = f32v3{x, y, z};
        data.Vertices.Append(StatVertex<D3>{vertex, f32v2{u, v}, Math::Normalize(f32v3{0.f, y, z}), tangent});
    };
    const u32 offset = left.Vertices.GetSize() + right.Vertices.GetSize();
    const auto addIndex = [&data, offset](const u32 index) { data.Indices.Append(Index(index + offset)); };

    const f32 angle = 2.f * Math::Pi<f32>() / sides;
    for (u32 i = 0; i < sides; ++i)
    {
        const f32 cc = Math::Cosine(i * angle);
        const f32 ss = Math::Sine(i * angle);
        const f32v4 tangent = f32v4{0.f, -ss, cc, 1.f};

        const f32 c = 0.5f * cc;
        const f32 s = 0.5f * ss;

        const f32 u = f32(i) / sides;
        addVertex(-0.5f, c, s, u, 0.f, tangent);
        addVertex(0.5f, c, s, u, 1.f, tangent);

        const u32 ii = 2 * i;
        addIndex(ii);
        addIndex((ii + 2) % (2 * sides));
        addIndex(ii + 1);

        addIndex(ii + 1);
        addIndex((ii + 2) % (2 * sides));
        addIndex((ii + 3) % (2 * sides));
    }

    VALIDATE_MESH(data);
    return data;
}

template Material AddMaterial(const MaterialData<D2> &data);
template Material AddMaterial(const MaterialData<D3> &data);

template void UpdateMaterial(Material mesh, const MaterialData<D2> &data);
template void UpdateMaterial(Material mesh, const MaterialData<D3> &data);

template Mesh AddMesh(const StatMeshData<D2> &data);
template Mesh AddMesh(const StatMeshData<D3> &data);

template void UpdateMesh(Mesh handle, const StatMeshData<D2> &data);
template void UpdateMesh(Mesh handle, const StatMeshData<D3> &data);

template u32 GetStaticMeshCount<D2>();
template u32 GetStaticMeshCount<D3>();

#ifdef ONYX_ENABLE_OBJ_LOAD
template Result<StatMeshData<D2>> LoadStaticMeshFromObjFile<D2>(const char *path);
template Result<StatMeshData<D3>> LoadStaticMeshFromObjFile<D3>(const char *path);
#endif

template StatMeshData<D2> GetStaticMeshData(Mesh handle);
template StatMeshData<D3> GetStaticMeshData(Mesh handle);

template const MaterialData<D2> &GetMaterialData(Material handle);
template const MaterialData<D3> &GetMaterialData(Material handle);

#ifdef ONYX_ENABLE_GLTF_LOAD
template GltfHandles AddGltfAssets(GltfAssets<D2> &assets);
template GltfHandles AddGltfAssets(GltfAssets<D3> &assets);

template Result<GltfAssets<D2>> LoadGltfAssetsFromFile(const std::string &path, AssetsFlags flags);
template Result<GltfAssets<D3>> LoadGltfAssetsFromFile(const std::string &path, AssetsFlags flags);
#endif

template const VKit::DeviceBuffer &GetStaticMeshVertexBuffer<D2>();
template const VKit::DeviceBuffer &GetStaticMeshVertexBuffer<D3>();

template const VKit::DeviceBuffer &GetStaticMeshIndexBuffer<D2>();
template const VKit::DeviceBuffer &GetStaticMeshIndexBuffer<D3>();

template MeshDataLayout GetStaticMeshLayout<D2>(Mesh handle);
template MeshDataLayout GetStaticMeshLayout<D3>(Mesh handle);

template StatMeshData<D2> CreateTriangleMesh<D2>();
template StatMeshData<D3> CreateTriangleMesh<D3>();

template StatMeshData<D2> CreateSquareMesh<D2>();
template StatMeshData<D3> CreateSquareMesh<D3>();

template StatMeshData<D2> CreateRegularPolygonMesh<D2>(u32);
template StatMeshData<D3> CreateRegularPolygonMesh<D3>(u32);

template StatMeshData<D2> CreatePolygonMesh<D2>(TKit::Span<const f32v2>);
template StatMeshData<D3> CreatePolygonMesh<D3>(TKit::Span<const f32v2>);

} // namespace Onyx::Assets
