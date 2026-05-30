#include "pch.hpp"
#include "onyx/specs.hpp"
#include "resources.hpp"
#include "conversion.hpp"
#include "renderer.hpp"
#include "core.hpp"
#include "buffer.hpp"
#include "vkit/state/descriptor_set.hpp"
#include "vkit/resource/sampler.hpp"
#include "vkit/resource/device_image.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "tkit/container/hive.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/container/dynamic_array.hpp"
#include "tkit/profiling/clock.hpp"

namespace Onyx::Resources
{
#define CHECK_POOL_HANDLE(pool, rtype)                                                                                 \
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);                                                                        \
    ONYX_CHECK_HANDLE_HAS_RESOURCE_POOL_TYPE(pool, rtype);                                                             \
    ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, rtype)

#define CHECK_POOL_HANDLE_WITH_DIM(pool, rtype, dim)                                                                   \
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);                                                                        \
    ONYX_CHECK_HANDLE_HAS_RESOURCE_POOL_TYPE(pool, rtype);                                                             \
    ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(pool, rtype, dim)

#define CHECK_RESOURCE_HANDLE(handle, rtype)                                                                           \
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);                                                                           \
    ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(handle, rtype);                                                                \
    ONYX_CHECK_RESOURCE_IS_VALID(handle, rtype);

#define CHECK_RESOURCE_HANDLE_WITH_DIM(handle, rtype, dim)                                                             \
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);                                                                           \
    ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(handle, rtype);                                                                \
    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(handle, rtype, dim);

#define CHECK_RESOURCE_AND_POOL_HANDLES(handle, rtype)                                                                 \
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);                                                                           \
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);                                                                      \
    ONYX_CHECK_RESOURCE_POOL_IS_VALID(handle, rtype);                                                                  \
    ONYX_CHECK_RESOURCE_IS_VALID(handle, rtype);

#define CHECK_RESOURCE_AND_POOL_HANDLES_WITH_DIM(handle, rtype, dim)                                                   \
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);                                                                           \
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);                                                                      \
    ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(handle, rtype, dim);                                                    \
    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(handle, rtype, dim);

using StatusFlags = u8;
enum StatusFlagBit : StatusFlags
{
    StatusFlag_NeedsSync = 1U << 0,
};

// NOTE(Isma): Messy. too many specializations, specially for the back culled bool
template <typename Vertex> struct MeshDataInfo
{
    MeshDataLayout Layout;
    Resource Bounds;
};

template <> struct MeshDataInfo<StatVertex<D3>>
{
    MeshDataLayout Layout;
    Resource Bounds;
    bool BackCulled;
};

template <Dimension D> struct MeshDataInfo<ParaVertex<D>>
{
    MeshDataLayout Layout;
    Resource Bounds;
    ParametricShape Shape;
};
template <> struct MeshDataInfo<ParaVertex<D3>>
{
    MeshDataLayout Layout;
    Resource Bounds;
    ParametricShape Shape;
    bool BackCulled;
};

template <> struct MeshDataInfo<GlyphVertex>
{
    MeshDataLayout Layout;
    FontData Data{};
    Resource AtlasImage = NullHandle;
    Resource AtlasTexture = NullHandle;
};

template <typename Vertex> struct MeshPoolData
{
    VKit::DeviceBuffer VertexBuffer{};
    VKit::DeviceBuffer IndexBuffer{};
    TKit::DynamicArray<Vertex> Vertices{};
    TKit::DynamicArray<Index> Indices{};
    TKit::TierArray<MeshDataInfo<Vertex>> Meshes{};
    StatusFlags Flags = 0;
};

template <typename Vertex> struct MeshResourceData
{
    TKit::StaticHive<MeshPoolData<Vertex>, ONYX_MAX_RESOURCE_POOLS> Pools{};
    TKit::StaticArray<ResourcePool, ONYX_MAX_RESOURCE_POOLS> ToDestroy{};
};

template <> struct MeshPoolData<GlyphVertex>
{
    VKit::DeviceBuffer VertexBuffer{};
    VKit::DeviceBuffer IndexBuffer{};
    TKit::TierArray<GlyphVertex> Vertices{};
    TKit::TierArray<Index> Indices{};
    TKit::TierArray<u32> GlyphToFontId{};
    TKit::TierArray<MeshDataInfo<GlyphVertex>> Meshes{};
    StatusFlags Flags = 0;
};

template <Dimension D> using StatMeshResourceData = MeshResourceData<StatVertex<D>>;
template <Dimension D> using StatMeshPoolData = MeshPoolData<StatVertex<D>>;
template <Dimension D> using StatMeshDataInfo = MeshDataInfo<StatVertex<D>>;

template <Dimension D> using ParaMeshResourceData = MeshResourceData<ParaVertex<D>>;
template <Dimension D> using ParaMeshPoolData = MeshPoolData<ParaVertex<D>>;
template <Dimension D> using ParaMeshDataInfo = MeshDataInfo<ParaVertex<D>>;

using FontResourceData = MeshResourceData<GlyphVertex>;
using FontPoolData = MeshPoolData<GlyphVertex>;
using FontDataInfo = MeshDataInfo<GlyphVertex>;

template <typename T> struct HiveResourceData
{
    VKit::DeviceBuffer Buffer{};
    TKit::ArenaHive<T> Elements{};
    StatusFlags Flags = 0;
};

template <Dimension D> using MaterialResourceData = HiveResourceData<MaterialData<D>>;
template <Dimension D> using BoundsResourceData = HiveResourceData<BoundsData<D>>;

struct ImageInfo
{
    VKit::DeviceImage Image{};
    TKit::TierArray<Resource> Textures{};
};

template <typename T> struct ArenaResourceData
{
    TKit::ArenaHive<T> Resources{};
    TKit::ArenaArray<Resource> ToDestroy{};
};

using BufferResourceData = ArenaResourceData<VKit::DeviceBuffer>;
using SamplerResourceData = ArenaResourceData<VKit::Sampler>;
using ImageResourceData = ArenaResourceData<ImageInfo>;

struct Texture
{
    Resource Image;
    VkImageView View;
};

template <Dimension D> struct ResourceData
{
    StatMeshResourceData<D> StaticMeshes{};
    ParaMeshResourceData<D> ParametricMeshes{};
    MaterialResourceData<D> Materials{};
    BoundsResourceData<D> BoundingBoxes{};
};

static TKit::Storage<ResourceData<D2>> s_ResourceData2{};
static TKit::Storage<ResourceData<D3>> s_ResourceData3{};
static TKit::Storage<BufferResourceData> s_Buffers{};
static TKit::Storage<SamplerResourceData> s_Samplers{};
static TKit::Storage<ImageResourceData> s_Images{};
static TKit::Storage<TKit::ArenaHive<Texture>> s_Textures{};
static TKit::Storage<FontResourceData> s_FontData{};
static DefaultResources s_DefaultResources{};

template <Dimension D> static ResourceData<D> &getData()
{
    if constexpr (D == D2)
        return *s_ResourceData2;
    else
        return *s_ResourceData3;
}

template <Dimension D> static void updateMaterialDescriptorSet()
{
    MaterialResourceData<D> &materials = getData<D>().Materials;

    const VkDescriptorBufferInfo binfo = materials.Buffer.CreateDescriptorInfo();
    Renderer::BindBuffer<D>(ONYX_MATERIALS_BINDING_POINT, binfo, RenderPass_Shaded);
    if constexpr (D == D2)
        Renderer::BindBuffer<D>(ONYX_MATERIALS_BINDING_POINT, binfo, RenderPass_Shadow);
}

template <Dimension D> static void updateBoundsDescriptorSet()
{
    BoundsResourceData<D> &bounds = getData<D>().BoundingBoxes;

    const VkDescriptorBufferInfo binfo = bounds.Buffer.CreateDescriptorInfo();
    Renderer::BindBuffer<D>(ONYX_BOUNDS_BINDING_POINT, binfo, RenderPass_Shaded);
    Renderer::BindBuffer<D>(ONYX_BOUNDS_BINDING_POINT, binfo, RenderPass_Flat);
    Renderer::BindBuffer<D>(ONYX_BOUNDS_BINDING_POINT, binfo, RenderPass_Shadow);
}

template <typename T> static void initializeHiveResources(const u32 capacity, HiveResourceData<T> &hive)
{
    hive.Elements.Reserve(capacity);
    hive.Buffer = Onyx::CreateBuffer<T>(Buffer_DeviceStorage);
}

template <Dimension D> static void initializeMaterials(const u32 capacity)
{
    initializeHiveResources(capacity, getData<D>().Materials);
    updateMaterialDescriptorSet<D>();
}

template <Dimension D> static void initializeBounds(const u32 capacity)
{
    initializeHiveResources(capacity, getData<D>().BoundingBoxes);
    updateBoundsDescriptorSet<D>();
}

void Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][RESOURCES] Initializing");
    s_ResourceData2.Construct();
    s_ResourceData3.Construct();
    s_Buffers.Construct();
    s_Samplers.Construct();
    s_Images.Construct();
    s_Textures.Construct();
    s_FontData.Construct();

    s_DefaultResources = {};
    s_Buffers->Resources.Reserve(specs.MaxBuffers);
    s_Samplers->Resources.Reserve(specs.MaxSamplers);
    s_Images->Resources.Reserve(specs.MaxImages);
    s_Textures->Reserve(specs.MaxTextures);

    initializeMaterials<D2>(specs.MaxMaterials);
    initializeMaterials<D3>(specs.MaxMaterials);
    initializeBounds<D2>(specs.MaxBounds);
    return initializeBounds<D3>(specs.MaxBounds);
}

template <typename Vertex> static void terminateMeshes(MeshResourceData<Vertex> &meshData)
{
    for (MeshPoolData<Vertex> &pool : meshData.Pools)
    {
        pool.VertexBuffer.Destroy();
        pool.IndexBuffer.Destroy();
    }
}
template <Dimension D> static void terminateMaterials()
{
    MaterialResourceData<D> &materials = getData<D>().Materials;
    materials.Buffer.Destroy();
}

template <Dimension D> static void terminateBounds()
{
    BoundsResourceData<D> &data = getData<D>().BoundingBoxes;
    data.Buffer.Destroy();
}

template <Dimension D> static void terminate()
{
    terminateMaterials<D>();
    ResourceData<D> &data = getData<D>();
    terminateMeshes(data.ParametricMeshes);
    terminateMeshes(data.StaticMeshes);
    terminateBounds<D>();
}

void Terminate()
{
    terminate<D2>();
    terminate<D3>();
    terminateMeshes(*s_FontData);

    for (VKit::DeviceBuffer &buffer : s_Buffers->Resources)
        buffer.Destroy();
    for (VKit::Sampler &sampler : s_Samplers->Resources)
        sampler.Destroy();
    for (ImageInfo &img : s_Images->Resources)
        img.Image.Destroy();

    s_Buffers.Destruct();
    s_Samplers.Destruct();
    s_Images.Destruct();
    s_FontData.Destruct();
    s_ResourceData2.Destruct();
    s_ResourceData3.Destruct();
}

Resource CreateBuffer(const VKit::DeviceBuffer::Builder &builder)
{
    const u32 bid = s_Buffers->Resources.Insert(ONYX_CHECK_VKIT_RESULT(builder.Build()));
    return CreateResourceHandle(Resource_Buffer, bid);
}
// static const VKit::DeviceBuffer &getBuffer(const Resource handle)
// {
//     CHECK_RESOURCE_HANDLE(handle, Resource_Buffer);
//
//     const u32 bid = GetResourceId(handle);
//     return s_Buffers->Resources[bid];
// }
void DestroyBuffer(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Buffer);

    const u32 bid = GetResourceId(handle);
    s_Buffers->Resources[bid].Destroy();
    s_Buffers->Resources.Remove(bid);
}
void ReleaseBuffer(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Buffer);
    s_Buffers->ToDestroy.Append(handle);
}

static VkSamplerMipmapMode asVulkanMipmapMode(const SamplerMode mode)
{
    switch (mode)
    {
    case SamplerMode_Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    case SamplerMode_Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    default:
        return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}
static VkFilter asVulkanFilter(const SamplerFilter filter)
{
    switch (filter)
    {
    case SamplerFilter_Linear:
        return VK_FILTER_LINEAR;
    case SamplerFilter_Nearest:
        return VK_FILTER_NEAREST;
    case SamplerFilter_Cubic:
        return VK_FILTER_CUBIC_EXT;
    default:
        return VK_FILTER_MAX_ENUM;
    }
}
static VkSamplerAddressMode asVulkanAddressMode(const SamplerWrap wrap)
{
    switch (wrap)
    {
    case SamplerWrap_Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case SamplerWrap_ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case SamplerWrap_MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    default:
        return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }
}

static Resource createSampler(TKit::ArenaHive<VKit::Sampler> &samplers, const VKit::Sampler::Builder &builder)
{
    const u32 sid = samplers.Insert(ONYX_CHECK_VKIT_RESULT(builder.Build()));
    return CreateResourceHandle(Resource_Sampler, sid);
}

static Resource createSampler(TKit::ArenaHive<VKit::Sampler> &samplers, const SamplerData &data)
{
    return createSampler(samplers, VKit::Sampler::Builder(GetDevice())
                                       .SetMipmapMode(asVulkanMipmapMode(data.Mode))
                                       .SetMinFilter(asVulkanFilter(data.MinFilter))
                                       .SetMagFilter(asVulkanFilter(data.MagFilter))
                                       .SetAddressModeU(asVulkanAddressMode(data.WrapU))
                                       .SetAddressModeV(asVulkanAddressMode(data.WrapV))
                                       .SetAddressModeW(asVulkanAddressMode(data.WrapW)));
}

static VKit::Sampler &getSampler(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Sampler);

    const u32 sid = GetResourceId(handle);
    return s_Samplers->Resources[sid];
}

static void bindSampler(const Resource handle)
{
    VKit::Sampler &sampler = getSampler(handle);
    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_VKIT_RESULT(
            sampler.SetName(TKit::StackString::Format("onyx-resources-sampler-{:#010x}", handle).CString()));
    }

    const VkDescriptorImageInfo info = VkDescriptorImageInfo{
        .sampler = sampler, .imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};

    const u32 sid = GetResourceId(handle);
    Renderer::BindImage<D2>(ONYX_SAMPLERS_BINDING_POINT, info, RenderPass_Shaded, sid);
    Renderer::BindImage<D2>(ONYX_SAMPLERS_BINDING_POINT, info, RenderPass_Flat, sid);
    Renderer::BindImage<D2>(ONYX_SAMPLERS_BINDING_POINT, info, RenderPass_Shadow, sid);
    Renderer::BindImage<D3>(ONYX_SAMPLERS_BINDING_POINT, info, RenderPass_Shaded, sid);
    Renderer::BindImage<D3>(ONYX_SAMPLERS_BINDING_POINT, info, RenderPass_Flat, sid);
    Renderer::BindImage<D3>(ONYX_SAMPLERS_BINDING_POINT, info, RenderPass_Shadow, sid);
}

Resource CreateSampler(const VKit::Sampler::Builder &builder)
{
    const Resource handle = createSampler(s_Samplers->Resources, builder);
    bindSampler(handle);
    return handle;
}
Resource CreateSampler(const SamplerData &data)
{
    const Resource handle = createSampler(s_Samplers->Resources, data);
    bindSampler(handle);
    return handle;
}
template <Dimension D> static void removeSamplerReferences(const Resource handle)
{
    const auto updateRef = [handle](Resource &toUpdate) -> StatusFlags {
        if (toUpdate == handle)
        {
            toUpdate = NullHandle;
            return StatusFlag_NeedsSync;
        }
        return 0;
    };

    MaterialResourceData<D> &materials = getData<D>().Materials;
    for (MaterialData<D> &mat : materials.Elements)
        if constexpr (D == D2)
            materials.Flags |= updateRef(mat.Sampler);
        else
            for (Resource &h : mat.Samplers)
                materials.Flags |= updateRef(h);
}

void DestroySampler(const Resource handle)
{
    TKIT_LOG_DEBUG("[ONYX][RESOURCES]    Destroying sampler with handle {:#010x}", handle);
    CHECK_RESOURCE_HANDLE(handle, Resource_Sampler);

    removeSamplerReferences<D2>(handle);
    removeSamplerReferences<D3>(handle);

    const u32 sid = GetResourceId(handle);
    s_Samplers->Resources[sid].Destroy();
    s_Samplers->Resources.Remove(sid);
}

void ReleaseSampler(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Sampler);
    s_Samplers->ToDestroy.Append(handle);
}

static ImageInfo &getImage(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Image);

    const u32 iid = GetResourceId(handle);
    return s_Images->Resources[iid];
}

Resource CreateImage(const VKit::DeviceImage::Builder &builder)
{
    const u32 iid = s_Images->Resources.Insert();
    ImageInfo &img = s_Images->Resources[iid];
    img.Image = ONYX_CHECK_VKIT_RESULT(builder.Build());
    return CreateResourceHandle(Resource_Image, iid);
}
Resource CreateImage(const ImageData &data)
{
    const VkFormat fmt = AsVulkanFormat(data.Format);
    const Resource handle = CreateImage(VKit::DeviceImage::Builder(
        GetDevice(), GetVulkanAllocator(), VkExtent2D{data.Width, data.Height}, fmt,
        VKit::DeviceImageFlag_Color | VKit::DeviceImageFlag_Sampled | VKit::DeviceImageFlag_Destination));

    ImageInfo &img = getImage(handle);
    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    const VkDeviceSize size = img.Image.ComputeSize();
    TKIT_ASSERT(
        size == data.ComputeSize(),
        "[ONYX][RESOURCES] Size mismatch. Device image reports {:L} bytes while texture data reports {:L} bytes", size,
        data.ComputeSize());

    VKit::DeviceBuffer uploadBuffer =
        ONYX_CHECK_VKIT_RESULT(VKit::DeviceBuffer::Builder(GetDevice(), GetVulkanAllocator(),
                                                           DeviceBufferFlag_Staging | DeviceBufferFlag_HostMapped)
                                   .SetSize(size)
                                   .Build());

    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-resources-image-{:#010x}", handle);
        ONYX_CHECK_VKIT_RESULT(img.Image.SetName(name.CString()));
        ONYX_CHECK_VKIT_RESULT(uploadBuffer.SetName(
            TKit::StackString::Format("onyx-resources-texture-upload-buffer-{:#010x}", handle).CString()));
    }

    uploadBuffer.Write(data.Data, {.srcOffset = 0, .dstOffset = 0, .size = size});
    ONYX_CHECK_VKIT_RESULT(uploadBuffer.Flush());

    const VkCommandBuffer cmd = ONYX_CHECK_VKIT_RESULT(pool.BeginSingleTimeCommands());
    img.Image.TransitionLayout2(
        cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        {.DstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, .DstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR});

    VkBufferImageCopy2KHR copy{};
    copy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = data.Width;
    copy.imageExtent.height = data.Height;
    copy.imageExtent.depth = 1;

    img.Image.CopyFromBuffer2(cmd, uploadBuffer, copy);

    img.Image.TransitionLayout2(
        cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        {.SrcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, .SrcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR});

    ONYX_CHECK_VKIT_RESULT(pool.EndSingleTimeCommands(cmd, queue->GetHandle()));
    uploadBuffer.Destroy();
    return handle;
}

void DestroyImage(const Resource handle)
{
    TKIT_LOG_DEBUG("[ONYX][RESOURCES]    Destroying image with handle {:#010x}", handle);
    CHECK_RESOURCE_HANDLE(handle, Resource_Image);

    ImageInfo &img = getImage(handle);
    for (const Resource tex : img.Textures)
        DestroyTexture(tex);

    img.Image.Destroy();
    const u32 iid = GetResourceId(handle);
    s_Images->Resources.Remove(iid);
}

void ReleaseImage(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Image);
    s_Images->ToDestroy.Append(handle);
}

static Resource createTexture(const Resource handle, const VkImageView imageView)
{
    const u32 tid = s_Textures->Insert();
    Texture &tex = s_Textures->At(tid);
    tex.Image = handle;
    tex.View = imageView;

    VkDescriptorImageInfo info;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = imageView;
    info.sampler = VK_NULL_HANDLE;

    Renderer::BindImage<D2>(ONYX_TEXTURES_BINDING_POINT, info, RenderPass_Shaded, tid);
    Renderer::BindImage<D2>(ONYX_TEXTURES_BINDING_POINT, info, RenderPass_Flat, tid);
    Renderer::BindImage<D2>(ONYX_TEXTURES_BINDING_POINT, info, RenderPass_Shadow, tid);
    Renderer::BindImage<D3>(ONYX_TEXTURES_BINDING_POINT, info, RenderPass_Shaded, tid);
    Renderer::BindImage<D3>(ONYX_TEXTURES_BINDING_POINT, info, RenderPass_Flat, tid);
    Renderer::BindImage<D3>(ONYX_TEXTURES_BINDING_POINT, info, RenderPass_Shadow, tid);

    const Resource thandle = CreateResourceHandle(Resource_Texture, tid);
    ImageInfo &img = getImage(handle);
    img.Textures.Append(thandle);
    return thandle;
}

Resource CreateTexture(const Resource handle, const u32 viewIndex)
{
    VKit::DeviceImage &img = getImage(handle).Image;
    const VkImageView view =
        viewIndex == TKIT_U32_MAX ? ONYX_CHECK_VKIT_RESULT(img.AddImageView()) : img.GetView(viewIndex);
    return createTexture(handle, view);
}

template <Dimension D> static void removeTextureReferences(const Resource handle)
{
    const auto updateRef = [handle](Resource &toUpdate) -> StatusFlags {
        if (toUpdate == handle)
        {
            toUpdate = NullHandle;
            return StatusFlag_NeedsSync;
        }
        return 0;
    };

    MaterialResourceData<D> &materials = getData<D>().Materials;
    for (MaterialData<D> &mat : materials.Elements)
        if constexpr (D == D2)
            materials.Flags |= updateRef(mat.Texture);
        else
            for (Resource &h : mat.Textures)
                materials.Flags |= updateRef(h);
}

static void destroyTexture(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Texture);

    removeTextureReferences<D2>(handle);
    removeTextureReferences<D3>(handle);

    const u32 tid = GetResourceId(handle);
    s_Textures->Remove(tid);
}

void DestroyTexture(const Resource handle)
{
#ifdef TKIT_ENABLE_ASSERTS
    for (const FontPoolData &fpool : s_FontData->Pools)
        for (const FontDataInfo &finfo : fpool.Meshes)
        {
            TKIT_ASSERT(finfo.AtlasTexture != handle,
                        "[ONYX][RESOURCES] Attempting to destroy texture with handle {:#010x}, which is a font atlas "
                        "managed by an active font!",
                        handle);
        }
#endif
    destroyTexture(handle);
}

template <typename T>
static Resource createHiveResource(const ResourceType rtype, const T &data, HiveResourceData<T> &hive)
{
    hive.Flags = StatusFlag_NeedsSync;
    return CreateResourceHandle(rtype, hive.Elements.Insert(data));
}

template <typename T> static void updateHiveResource(const Resource handle, const T &data, HiveResourceData<T> &hive)
{
    const u32 aid = GetResourceId(handle);
    hive.Elements[aid] = data;
    hive.Flags = StatusFlag_NeedsSync;
}

template <typename T> static void destroyHiveResource(const Resource handle, HiveResourceData<T> &hive)
{
    const u32 aid = GetResourceId(handle);
    hive.Elements.Remove(aid);
}

template <Dimension D> static u32 createBounds(const BoundsData<D> &data)
{
    return createHiveResource(Resource_Bounds, data, getData<D>().BoundingBoxes);
}

template <Dimension D> static void updateBounds(const Resource handle, const BoundsData<D> &data)
{
    CHECK_RESOURCE_HANDLE_WITH_DIM(handle, Resource_Bounds, D);
    updateHiveResource(handle, data, getData<D>().BoundingBoxes);
}

template <Dimension D> static void destroyBounds(const Resource handle)
{
    CHECK_RESOURCE_HANDLE_WITH_DIM(handle, Resource_Bounds, D);
    destroyHiveResource(handle, getData<D>().BoundingBoxes);
}

template <typename Vertex>
static Resource createMesh(const ResourcePool pool, MeshResourceData<Vertex> &meshes, const MeshData<Vertex> &data)
{
    constexpr ResourceType rtype = Vertex::Resource;
    CHECK_POOL_HANDLE_WITH_DIM(pool, rtype, Vertex::Dim);

    const u32 pid = GetResourcePoolId(pool);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
    mpool.Flags = StatusFlag_NeedsSync;

    const u32 mid = mpool.Meshes.GetSize();
    const u32 vcount = mpool.Vertices.GetSize();
    const u32 icount = mpool.Indices.GetSize();

    MeshDataInfo<Vertex> &minfo = mpool.Meshes.Append();
    minfo.Layout.VertexStart = vcount;
    minfo.Layout.VertexCount = data.Vertices.GetSize();
    minfo.Layout.IndexStart = icount;
    minfo.Layout.IndexCount = data.Indices.GetSize();
    minfo.Bounds = createBounds(CreateBoundsData(data));
    if constexpr (Vertex::Dim == D3)
        minfo.BackCulled = data.BackCulled;

    if constexpr (Vertex::Geo == Geometry_Parametric)
        minfo.Shape = data.Shape;

    auto &vertices = mpool.Vertices;
    auto &indices = mpool.Indices;
    vertices.Insert(vertices.end(), data.Vertices.begin(), data.Vertices.end());
    indices.Insert(indices.end(), data.Indices.begin(), data.Indices.end());
    return CreateResourceHandle(rtype, mid, pid);
}

template <typename Vertex>
static void updateMesh(const Resource handle, MeshResourceData<Vertex> &meshes, const MeshData<Vertex> &data)
{
    CHECK_RESOURCE_AND_POOL_HANDLES_WITH_DIM(handle, Vertex::Resource, Vertex::Dim);

    const u32 pid = GetResourcePoolId(handle);
    const u32 mid = GetResourceId(handle);

    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
    mpool.Flags = StatusFlag_NeedsSync;

    MeshDataInfo<Vertex> &minfo = mpool.Meshes[mid];
    const MeshDataLayout &layout = minfo.Layout;
    TKIT_ASSERT(data.Vertices.GetSize() == layout.VertexCount && data.Indices.GetSize() == layout.IndexCount,
                "[ONYX][RESOURCES] When updating a mesh, the vertex and index count of the previous and updated mesh "
                "must be the "
                "same. If they are not, you must create a new mesh");

    updateBounds(minfo.Bounds, CreateBoundsData(data));

    TKit::ForwardCopy(mpool.Vertices.begin() + layout.VertexStart, data.Vertices.begin(), data.Vertices.end());
    TKit::ForwardCopy(mpool.Indices.begin() + layout.IndexStart, data.Indices.begin(), data.Indices.end());

    if constexpr (Vertex::Geo == Geometry_Parametric)
        minfo.Shape = data.Shape;
}

const DefaultResources &GetDefaultResources()
{
    return s_DefaultResources;
}

template <Dimension D>
static void createDefaultPool(ResourcePool &pool, const ResourcePool fallback, const ResourceType rtype)
{
    if (fallback == NullHandle)
        pool = CreateResourcePool<D>(rtype);
    else
        pool = fallback;
}

const DefaultResources &CreateDefaultResources(const DefaultResourcesOptions &opts)
{
    // NOTE(Isma): Resource checks are weak for the moment, meaning that if the user passes a bad resource, it will be
    // used

    DefaultResources &def = s_DefaultResources;
    createDefaultPool<D2>(def.StaticPool2, opts.StaticPool2, Resource_StaticMesh);
    createDefaultPool<D3>(def.StaticPool3, opts.StaticPool3, Resource_StaticMesh);

    createDefaultPool<D2>(def.ParametricPool2, opts.ParametricPool2, Resource_ParametricMesh);
    createDefaultPool<D3>(def.ParametricPool3, opts.ParametricPool3, Resource_ParametricMesh);

    if (opts.FontPool == NullHandle)
        def.FontPool = CreateFontPool();
    else
        def.FontPool = opts.FontPool;

    def.Font = opts.DefaultFont;
#ifdef ONYX_INCLUDE_DEFAULT_FONT
    if (def.Font == NullHandle)
    {
        const auto fres = LoadDefaultFont(opts.FontOpts);
        ONYX_LOG_RESULT_ERROR(fres);
        if (fres)
            def.Font = RegisterFont(def.FontPool, *fres);
    }
#endif
    def.FontSampler = CreateSampler(opts.FontSamplerData);

    def.Triangle2 = RegisterMesh(def.StaticPool2, opts.TriangleData2);
    def.Triangle3 = RegisterMesh(def.StaticPool3, opts.TriangleData3);

    def.Quad2 = RegisterMesh(def.StaticPool2, opts.QuadData2);
    def.Quad3 = RegisterMesh(def.StaticPool3, opts.QuadData3);

    def.Box = RegisterMesh(def.StaticPool3, opts.BoxData);
    def.Sphere = RegisterMesh(def.StaticPool3, opts.SphereData);
    def.Cylinder = RegisterMesh(def.StaticPool3, opts.CylinderData);

    def.Stadium2 = RegisterMesh(def.ParametricPool2, opts.StadiumData2);
    def.Stadium3 = RegisterMesh(def.ParametricPool3, opts.StadiumData3);

    def.RoundedRect2 = RegisterMesh(def.ParametricPool2, opts.RoundedRectData2);
    def.RoundedRect3 = RegisterMesh(def.ParametricPool3, opts.RoundedRectData3);

    def.Capsule = RegisterMesh(def.ParametricPool3, opts.CapsuleData);
    def.RoundedBox = RegisterMesh(def.ParametricPool3, opts.RoundedBoxData);
    def.Torus = RegisterMesh(def.ParametricPool3, opts.TorusData);

    SyncFlags flags = SyncFlag_StaticMeshes | SyncFlag_ParametricMeshes;
#ifdef ONYX_INCLUDE_DEFAULT_FONT
    flags |= SyncFlag_Fonts;
#endif

    Sync(flags);
    return def;
}

template <Dimension D> Resource RegisterMesh(const ResourcePool pool, const StatMeshData<D> &data)
{
    return createMesh(pool, getData<D>().StaticMeshes, data);
}
template <Dimension D> Resource RegisterMesh(const ResourcePool pool, const ParaMeshData<D> &data)
{
    return createMesh(pool, getData<D>().ParametricMeshes, data);
}

template <Dimension D> Resource RegisterMaterial(const MaterialData<D> &data)
{
    return createHiveResource(Resource_Material, data, getData<D>().Materials);
}

template <Dimension D> void UpdateMaterial(const Resource handle, const MaterialData<D> &data)
{
    CHECK_RESOURCE_HANDLE_WITH_DIM(handle, Resource_Material, D);
    updateHiveResource(handle, data, getData<D>().Materials);
}

template <Dimension D> void UpdateMesh(const Resource handle, const StatMeshData<D> &data)
{
    return updateMesh(handle, getData<D>().StaticMeshes, data);
}
template <Dimension D> void UpdateMesh(const Resource handle, const ParaMeshData<D> &data)
{
    return updateMesh(handle, getData<D>().ParametricMeshes, data);
}

template <typename Vertex> static ResourcePool createMeshPool(const ResourceType rtype, MeshResourceData<Vertex> &data)
{
    VKit::DeviceBuffer vbuffer = Onyx::CreateBuffer<Vertex>(Buffer_DeviceVertex);
    VKit::DeviceBuffer ibuffer = Onyx::CreateBuffer<Index>(Buffer_DeviceIndex);

    const u32 pid = data.Pools.Insert();
    MeshPoolData<Vertex> &mpool = data.Pools[pid];
    mpool.VertexBuffer = vbuffer;
    mpool.IndexBuffer = ibuffer;

    const ResourcePool pool = CreateResourcePoolHandle(rtype, pid);
    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString vb = TKit::StackString::Format("onyx-resources-vertex-buffer-{:#010x}", pool);
        const TKit::StackString ib = TKit::StackString::Format("onyx-resources-index-buffer-{:#010x}", pool);

        ONYX_CHECK_VKIT_RESULT(vbuffer.SetName(vb.CString()));
        ONYX_CHECK_VKIT_RESULT(ibuffer.SetName(ib.CString()));
    }

    return pool;
}

ResourcePool CreateFontPool()
{
    return createMeshPool(Resource_Font, *s_FontData);
}

template <Dimension D> ResourcePool CreateResourcePool(const ResourceType rtype)
{
    switch (rtype)
    {
    case Resource_StaticMesh:
        return createMeshPool(Resource_StaticMesh, getData<D>().StaticMeshes);
    case Resource_ParametricMesh:
        return createMeshPool(Resource_ParametricMesh, getData<D>().ParametricMeshes);
    case Resource_Font:
    case Resource_GlyphMesh:
        return CreateFontPool();
    default:
        TKIT_FATAL("[ONYX][RESOURCES] A resource pool cannot be created for resources of type '{}'", ToString(rtype));
        return NullHandle;
    }
}

template <typename Vertex> static void destroyMeshPool(const ResourcePool pool, MeshResourceData<Vertex> &meshes)
{
    TKIT_LOG_DEBUG("[ONYX][RESOURCES]    Destroying mesh pool with handle {:#010x}", pool);
    const u32 pid = GetResourcePoolId(pool);
    MeshPoolData<Vertex> &mpool = meshes.Pools[pid];
    if constexpr (!std::is_same_v<Vertex, GlyphVertex>)
        for (const MeshDataInfo<Vertex> &minfo : mpool.Meshes)
            destroyBounds<Vertex::Dim>(minfo.Bounds);
    mpool.VertexBuffer.Destroy();
    mpool.IndexBuffer.Destroy();

    meshes.Pools.Remove(pid);
}

template <Dimension D> void DestroyResourcePool(const ResourcePool pool)
{
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(pool);
    const ResourceType rtype = GetResourceType(pool);
    switch (rtype)
    {
    case Resource_StaticMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_StaticMesh);
        destroyMeshPool(pool, getData<D>().StaticMeshes);
        return;
    case Resource_ParametricMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_ParametricMesh);
        destroyMeshPool(pool, getData<D>().ParametricMeshes);
        return;
    case Resource_Font:
    case Resource_GlyphMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_Font);
        DestroyFontPool(pool);
        return;
    default:
        TKIT_FATAL("[ONYX][RESOURCES] A resource pool of type '{}' cannot exist", ToString(rtype));
    }
}

template <Dimension D> void ReleaseResourcePool(const ResourcePool pool)
{
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(pool);

    const ResourceType rtype = GetResourceType(pool);
    switch (rtype)
    {
    case Resource_StaticMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_StaticMesh);
        getData<D>().StaticMeshes.ToDestroy.Append(pool);
        return;
    case Resource_ParametricMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_ParametricMesh);
        getData<D>().ParametricMeshes.ToDestroy.Append(pool);
        return;
    case Resource_Font:
    case Resource_GlyphMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_Font);
        ReleaseFontPool(pool);
        return;
    default:
        TKIT_FATAL("[ONYX][RESOURCES] A resource pool of type '{}' cannot exist", ToString(rtype));
    }
}

void ReleaseFontPool(const ResourcePool pool)
{
    s_FontData->ToDestroy.Append(pool);
}
void DestroyFontPool(const ResourcePool pool)
{
    const u32 pid = GetResourcePoolId(pool);
    for (const FontDataInfo &finfo : s_FontData->Pools[pid].Meshes)
    {
        ReleaseImage(finfo.AtlasImage);
        TKit::Deallocate(finfo.Data.AtlasData.Data);
    }
    destroyMeshPool(pool, *s_FontData);
}

Resource RegisterFont(const ResourcePool pool, const FontData &data)
{
    CHECK_POOL_HANDLE(pool, Resource_Font);

    const u32 pid = GetResourcePoolId(pool);
    FontPoolData &fpool = s_FontData->Pools[pid];

    const u32 fid = fpool.Meshes.GetSize();
    FontDataInfo &finfo = fpool.Meshes.Append();
    finfo.Data = data;

    const ImageData &adata = finfo.Data.AtlasData;
    TKIT_LOG_WARNING_IF(
        adata.Width != adata.Height,
        "[ONYX][RESOURCES] Atlas dimensions are not uniform (w = {} != {} = h). May cause a slight decrease in "
        "quality, as the unit range factor is computed taking only one dimension into account",
        adata.Width, adata.Height);

    finfo.AtlasImage = CreateImage(adata);
    finfo.AtlasTexture = CreateTexture(finfo.AtlasImage);

    const u32 gsize = data.Glyphs.GetSize();
    finfo.Layout.VertexStart = fpool.Vertices.GetSize();
    finfo.Layout.VertexCount = gsize * 4;
    finfo.Layout.IndexStart = fpool.Indices.GetSize();
    finfo.Layout.IndexCount = gsize * 6;
    fpool.Flags = StatusFlag_NeedsSync;

    const auto addVertex = [&fpool](const f32 x, const f32 y, const f32 au, const f32 av, const f32 tu, const f32 tv) {
        fpool.Vertices.Append(GlyphVertex{f32v2{x, y}, f32v2{au, av}, f32v2{tu, tv}});
    };
    const auto addIndex = [&fpool, &finfo](const u32 idx) {
        fpool.Indices.Append(Index(idx + finfo.Layout.VertexStart));
    };

    for (u32 i = 0; i < data.Glyphs.GetSize(); ++i)
    {
        const GlyphData &glyph = data.Glyphs[i];
        const f32v2 &min = glyph.Min;
        const f32v2 &max = glyph.Max;

        const f32v2 &mnuv = glyph.MinTexCoord;
        const f32v2 &mxuv = glyph.MaxTexCoord;

        fpool.GlyphToFontId.Append(fid);
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

    return CreateResourceHandle(Resource_Font, fid, pid);
}

template <Dimension D> GltfHandles RegisterGltfResources(const ResourcePool meshPool, GltfData<D> &data)
{
    GltfHandles handles;
    handles.StaticMeshes.Reserve(data.StaticMeshes.GetSize());
    handles.Materials.Reserve(data.Materials.GetSize());
    handles.Samplers.Reserve(data.Samplers.GetSize());
    handles.Textures.Reserve(data.Images.GetSize());

    for (const StatMeshData<D> &smesh : data.StaticMeshes)
        handles.StaticMeshes.Append(RegisterMesh(meshPool, smesh));

    for (const SamplerData &sdata : data.Samplers)
        handles.Samplers.Append(CreateSampler(sdata));

    for (const ImageData &idata : data.Images)
    {
        const Resource img = CreateImage(idata);
        handles.Textures.Append(CreateTexture(img));
    }

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
            for (Resource &sampler : mdata.Samplers)
                if (sampler != TKIT_U32_MAX)
                    sampler = handles.Samplers[sampler];
            for (Resource &texture : mdata.Textures)
                if (texture != TKIT_U32_MAX)
                    texture = handles.Textures[texture];
        }
        handles.Materials.Append(RegisterMaterial(mdata));
    }

    return handles;
}

template <typename Vertex> static MeshData<Vertex> getMeshData(const Resource handle, MeshResourceData<Vertex> &meshes)
{
    CHECK_RESOURCE_AND_POOL_HANDLES_WITH_DIM(handle, Vertex::Resource, Vertex::Dim);

    const u32 pid = GetResourcePoolId(handle);
    const u32 mid = GetResourceId(handle);

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

template <Dimension D> StatMeshData<D> GetStaticMeshData(const Resource handle)
{
    return getMeshData(handle, getData<D>().StaticMeshes);
}
template <Dimension D> ParaMeshData<D> GetParametricMeshData(const Resource handle)
{
    return getMeshData(handle, getData<D>().ParametricMeshes);
}
template <Dimension D> ParametricShape GetParametricShape(const Resource handle)
{
    CHECK_RESOURCE_AND_POOL_HANDLES_WITH_DIM(handle, Resource_ParametricMesh, D);

    const u32 pid = GetResourcePoolId(handle);
    const u32 mid = GetResourceId(handle);

    ParaMeshPoolData<D> &mpool = getData<D>().ParametricMeshes.Pools[pid];
    return mpool.Meshes[mid].Shape;
}
template <Dimension D> const MaterialData<D> &GetMaterialData(const Resource handle)
{
    CHECK_RESOURCE_HANDLE_WITH_DIM(handle, Resource_Material, D);

    const u32 mid = GetResourceId(handle);

    return getData<D>().Materials.Elements[mid];
}

template <Dimension D> void DestroyMaterial(const Resource handle)
{
    CHECK_RESOURCE_HANDLE_WITH_DIM(handle, Resource_Material, D);
    destroyHiveResource(handle, getData<D>().Materials);
}

const FontData &GetFontData(const Resource handle)
{
    CHECK_RESOURCE_AND_POOL_HANDLES(handle, Resource_Font);
    const u32 pid = GetResourcePoolId(handle);
    const u32 fid = GetResourceId(handle);

    return s_FontData->Pools[pid].Meshes[fid].Data;
}
Resource GetFontAtlas(const Resource handle)
{
    CHECK_RESOURCE_AND_POOL_HANDLES(handle, Resource_Font);
    const u32 pid = GetResourcePoolId(handle);
    const u32 fid = GetResourceId(handle);

    return s_FontData->Pools[pid].Meshes[fid].AtlasTexture;
}
Resource GetFont(const Resource handle)
{
    CHECK_RESOURCE_AND_POOL_HANDLES(handle, Resource_GlyphMesh);
    const u32 pid = GetResourcePoolId(handle);
    const u32 gid = GetResourceId(handle);

    const FontPoolData &fpool = s_FontData->Pools[pid];
    const u32 fid = fpool.GlyphToFontId[gid];
    return CreateResourceHandle(Resource_Font, fid, pid);
}
Resource GetGlyph(const Resource handle, const u32 codePoint)
{
    CHECK_RESOURCE_AND_POOL_HANDLES(handle, Resource_Font);

    const u32 pid = GetResourcePoolId(handle);
    const u32 fid = GetResourceId(handle);

    const FontPoolData &fpool = s_FontData->Pools[pid];
    const FontDataInfo &finfo = fpool.Meshes[fid];
    const u32 gstart = finfo.Layout.VertexStart / 4;

    const auto it = finfo.Data.GlyphMap.Find(codePoint);
    if (it == finfo.Data.GlyphMap.end())
        return NullHandle;

    return CreateResourceHandle(Resource_GlyphMesh, gstart + it->Value, pid);
}

const GlyphData &GetGlyphData(const Resource handle)
{
    CHECK_RESOURCE_AND_POOL_HANDLES(handle, Resource_GlyphMesh);

    const u32 pid = GetResourcePoolId(handle);
    const u32 gid = GetResourceId(handle);

    const FontPoolData &fpool = s_FontData->Pools[pid];
    const FontDataInfo &finfo = fpool.Meshes[fpool.GlyphToFontId[gid]];
    const u32 gstart = finfo.Layout.VertexStart / 4;
    return finfo.Data.Glyphs[gid - gstart];
}

template <typename Vertex> static MeshDataLayout getMeshLayout(const Resource handle, MeshResourceData<Vertex> &meshes)
{
    const u32 pid = GetResourcePoolId(handle);
    const u32 mid = GetResourceId(handle);

    return meshes.Pools[pid].Meshes[mid].Layout;
}

MeshDataLayout GetFontLayout(const Resource handle)
{
    return getMeshLayout(handle, *s_FontData);
}

MeshDataLayout GetGlyphLayout(const Resource handle)
{
    const u32 gid = GetResourceId(handle);
    return MeshDataLayout{.VertexStart = 4 * gid, .VertexCount = 4, .IndexStart = 0, .IndexCount = 6};
}

template <Dimension D> MeshDataLayout GetMeshLayout(const Resource handle)
{
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(handle);

    const ResourceType rtype = GetResourceType(handle);

    switch (rtype)
    {
    case Resource_StaticMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(handle, Resource_StaticMesh);
        return getMeshLayout(handle, getData<D>().StaticMeshes);
    case Resource_ParametricMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(handle, Resource_ParametricMesh);
        return getMeshLayout(handle, getData<D>().ParametricMeshes);
    case Resource_Font:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(handle, Resource_Font);
        return GetFontLayout(handle);
    case Resource_GlyphMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(handle, Resource_GlyphMesh);
        return GetGlyphLayout(handle);
    default:
        TKIT_FATAL("[ONYX][RESOURCES] A resource of type '{}' does not have a mesh layout", ToString(rtype));
        return MeshDataLayout{};
    }
}

template <typename Vertex> static Resource getMeshBounds(const Resource handle, MeshResourceData<Vertex> &meshes)
{
    const u32 pid = GetResourcePoolId(handle);
    const u32 mid = GetResourceId(handle);

    return meshes.Pools[pid].Meshes[mid].Bounds;
}

template <Dimension D> Resource GetMeshBounds(const Resource handle)
{
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(handle);

    const ResourceType rtype = GetResourceType(handle);
    switch (rtype)
    {
    case Resource_StaticMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(handle, Resource_StaticMesh);
        return getMeshBounds(handle, getData<D>().StaticMeshes);
    case Resource_ParametricMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(handle, Resource_ParametricMesh);
        return getMeshBounds(handle, getData<D>().ParametricMeshes);
    default:
        TKIT_FATAL(
            "[ONYX][RESOURCES] A resource of type '{}' does not have well defined bounds. To access glyph bounds, "
            "use GetBoundsData<D2>() with the appropiate bounds handle",
            ToString(rtype));
        return TKIT_U32_MAX;
    }
}
template <Dimension D> const BoundsData<D> &GetBoundsData(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Bounds);

    const u32 bid = GetResourceId(handle);
    return getData<D>().BoundingBoxes[bid].Data;
}

TKit::Span<const u32> GetFontPoolIds()
{
    return s_FontData->Pools.GetValidIds();
}

template <Dimension D> TKit::Span<const u32> GetResourcePoolIds(const ResourceType rtype)
{
    switch (rtype)
    {
    case Resource_StaticMesh:
        return getData<D>().StaticMeshes.Pools.GetValidIds();
    case Resource_ParametricMesh:
        return getData<D>().ParametricMeshes.Pools.GetValidIds();
    case Resource_Font:
    case Resource_GlyphMesh:
        return GetFontPoolIds();
    default:
        TKIT_FATAL("[ONYX][RESOURCES] A resource of type '{}' cannot have a resource pool", ToString(rtype));
        return TKit::Span<const u32>{};
    }
}

template <typename Vertex> static u32 getMeshBatchCount(const MeshResourceData<Vertex> &meshes)
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
        count += fpool.GlyphToFontId.GetSize();

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

u32 GetFontCount(const ResourcePool pool)
{
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(pool);
    ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_Font);

    const u32 pid = GetResourcePoolId(pool);
    return s_FontData->Pools[pid].Meshes.GetSize();
}

u32 GetGlyphCount(const ResourcePool pool)
{
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(pool);
    ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_GlyphMesh);

    const u32 pid = GetResourcePoolId(pool);
    return s_FontData->Pools[pid].GlyphToFontId.GetSize();
}

template <Dimension D> u32 GetResourceCount(const ResourcePool pool)
{
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(pool);

    const ResourceType rtype = GetResourceType(pool);
    const u32 pid = GetResourcePoolId(pool);

    switch (rtype)
    {
    case Resource_StaticMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_StaticMesh);
        return getData<D>().StaticMeshes.Pools[pid].Meshes.GetSize();
    case Resource_ParametricMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_ParametricMesh);
        return getData<D>().ParametricMeshes.Pools[pid].Meshes.GetSize();
    case Resource_Font:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_Font);
        return GetFontCount(pool);
    case Resource_GlyphMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_GlyphMesh);
        return GetGlyphCount(pool);
    default:
        TKIT_FATAL("[ONYX][RESOURCES] A resource of type '{}' cannot have a resource pool", ToString(rtype));
        return 0;
    }
}

MeshBuffers GetFontBuffers(const ResourcePool pool)
{
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(pool);
    ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_Font);

    const u32 pid = GetResourcePoolId(pool);

    return {&s_FontData->Pools[pid].VertexBuffer, &s_FontData->Pools[pid].IndexBuffer};
}

MeshBuffers GetGlyphBuffers(const ResourcePool pool)
{
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(pool);
    ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_GlyphMesh);

    const u32 pid = GetResourcePoolId(pool);

    return {&s_FontData->Pools[pid].VertexBuffer, &s_FontData->Pools[pid].IndexBuffer};
}

bool IsBackCulled(const Resource handle)
{
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);

    const ResourceType rtype = GetResourceType(handle);
    if (rtype == Resource_GlyphMesh)
        return false;

    const u32 pid = GetResourcePoolId(handle);
    const u32 rid = GetResourceId(handle);
    if (rtype == Resource_StaticMesh)
        return s_ResourceData3->StaticMeshes.Pools[pid].Meshes[rid].BackCulled;

    return s_ResourceData3->ParametricMeshes.Pools[pid].Meshes[rid].BackCulled;
}

template <Dimension D> MeshBuffers GetMeshBuffers(const ResourcePool pool)
{
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(pool);
    ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(pool);

    const ResourceType rtype = GetResourceType(pool);

    const u32 pid = GetResourcePoolId(pool);
    switch (rtype)
    {
    case Resource_StaticMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_StaticMesh);
        return {&getData<D>().StaticMeshes.Pools[pid].VertexBuffer, &getData<D>().StaticMeshes.Pools[pid].IndexBuffer};
    case Resource_ParametricMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_ParametricMesh);
        return {&getData<D>().ParametricMeshes.Pools[pid].VertexBuffer,
                &getData<D>().ParametricMeshes.Pools[pid].IndexBuffer};
    case Resource_Font:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_Font);
        return {&s_FontData->Pools[pid].VertexBuffer, &s_FontData->Pools[pid].IndexBuffer};
    case Resource_GlyphMesh:
        ONYX_CHECK_RESOURCE_POOL_IS_VALID(pool, Resource_GlyphMesh);
        return {&s_FontData->Pools[pid].VertexBuffer, &s_FontData->Pools[pid].IndexBuffer};
    default:
        TKIT_FATAL("[ONYX][RESOURCES] A resource of type '{}' does not have a vertex buffer", ToString(rtype));
        return {};
    }
}

template <Dimension D> bool IsResourceValid(const Resource handle, const ResourceType rtype)
{
    const u32 itype = GetResourceTypeAsInteger(handle);
    if (itype >= Resource_Count || (itype != rtype && rtype != Resource_None))
        return false;

    const u32 aid = GetResourceId(handle);
    const u32 pid = GetResourcePoolId(handle);
    switch (itype)
    {
    case Resource_StaticMesh:
        return IsResourcePoolValid<D>(handle, Resource_StaticMesh) &&
               aid < getData<D>().StaticMeshes.Pools[pid].Meshes.GetSize();
    case Resource_ParametricMesh:
        return IsResourcePoolValid<D>(handle, Resource_ParametricMesh) &&
               aid < getData<D>().ParametricMeshes.Pools[pid].Meshes.GetSize();
    case Resource_Material:
        return IsResourcePoolNull(handle) && getData<D>().Materials.Elements.Contains(aid);
    case Resource_Font:
        return IsResourcePoolValid<D>(handle, Resource_Font) && aid < s_FontData->Pools[pid].Meshes.GetSize();
    case Resource_GlyphMesh:
        return IsResourcePoolValid<D>(handle, Resource_GlyphMesh) &&
               aid < 4 * s_FontData->Pools[pid].Vertices.GetSize() &&
               aid < 6 * s_FontData->Pools[pid].Indices.GetSize();
    case Resource_Sampler:
        return IsResourcePoolNull(handle) && s_Samplers->Resources.Contains(aid);
    case Resource_Texture:
        return IsResourcePoolNull(handle) && s_Textures->Contains(aid);
    case Resource_Bounds:
        return IsResourcePoolNull(handle) && getData<D>().BoundingBoxes.Elements.Contains(aid);
    case Resource_Buffer:
        return IsResourcePoolNull(handle) && s_Buffers->Resources.Contains(aid);
    case Resource_Image:
        return IsResourcePoolNull(handle) && s_Images->Resources.Contains(aid);
    default:
        return false;
    }
}
template <Dimension D> bool IsResourcePoolValid(const Handle handle, const ResourceType rtype)
{
    const u32 itype = GetResourceTypeAsInteger(handle);
    if (itype >= Resource_PoolCount || (itype != rtype && rtype != Resource_None))
        return false;

    const u32 pid = GetResourcePoolId(handle);
    switch (itype)
    {
    case Resource_StaticMesh:
        return getData<D>().StaticMeshes.Pools.Contains(pid);
    case Resource_ParametricMesh:
        return getData<D>().ParametricMeshes.Pools.Contains(pid);
    case Resource_Font:
    case Resource_GlyphMesh:
        return s_FontData->Pools.Contains(pid);
    default:
        return false;
    }
}

bool IsResourceValid(const Resource handle, const ResourceType rtype)
{
    return IsResourceValid<D2>(handle, rtype) || IsResourceValid<D3>(handle, rtype);
}
bool IsResourcePoolValid(const Handle handle, const ResourceType rtype)
{
    return IsResourcePoolValid<D2>(handle, rtype) || IsResourcePoolValid<D3>(handle, rtype);
}

template <typename T> static bool uploadFromHost(VKit::DeviceBuffer &buffer, const TKit::Span<const T> data)
{
    TKIT_LOG_DEBUG("[ONYX][RESOURCES]    Uploading buffer of {:L} bytes to device", data.GetBytes());
    VKit::CommandPool &pool = Execution::GetTransientGraphicsPool();
    const VKit::Queue *queue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

    const bool grow = GrowBufferIfNeeded<T>(buffer, data.GetSize());

    ONYX_CHECK_VKIT_RESULT(
        buffer.UploadFromHost(pool, *queue, data.GetData(), {.srcOffset = 0, .dstOffset = 0, .size = data.GetBytes()}));
    return grow;
}

template <typename Vertex> static void uploadMeshPool(MeshPoolData<Vertex> &mpool)
{
    uploadFromHost<Vertex>(mpool.VertexBuffer, mpool.Vertices);
    uploadFromHost<Index>(mpool.IndexBuffer, mpool.Indices);

    mpool.Flags = 0;
}

template <typename Vertex> static void uploadMeshes(MeshResourceData<Vertex> &meshes)
{
    for (const ResourcePool pool : meshes.ToDestroy)
        if constexpr (std::is_same_v<Vertex, GlyphVertex>)
            DestroyFontPool(pool);
        else
            destroyMeshPool(pool, meshes);

    meshes.ToDestroy.Clear();

    for (MeshPoolData<Vertex> &mpool : meshes.Pools)
        if (mpool.Flags & StatusFlag_NeedsSync)
            uploadMeshPool(mpool);
}

template <typename T> static bool uploadHiveResources(HiveResourceData<T> &hive)
{
    if (!(hive.Flags & StatusFlag_NeedsSync))
        return false;

    TKit::StackArray<T> sparse{};
    sparse.Resize(hive.Elements.GetIds().GetSize());

    for (const u32 id : hive.Elements.GetValidIds())
        sparse[id] = hive.Elements[id];

    hive.Flags = 0;
    return uploadFromHost<T>(hive.Buffer, sparse);
}

template <Dimension D> static void uploadMaterials()
{
    TKIT_LOG_DEBUG_IF(getData<D>().Materials.Flags & StatusFlag_NeedsSync, "[ONYX][RESOURCES] Uploading {}D materials",
                      u8(D));

    if (uploadHiveResources(getData<D>().Materials))
        updateMaterialDescriptorSet<D>();
}

template <Dimension D> static void uploadBounds()
{
    TKIT_LOG_DEBUG_IF(getData<D>().BoundingBoxes.Flags & StatusFlag_NeedsSync, "[ONYX][RESOURCES] Uploading {}D bounds",
                      u8(D));

    if (uploadHiveResources(getData<D>().BoundingBoxes))
        updateBoundsDescriptorSet<D>();
}

#ifdef TKIT_ENABLE_DEBUG_LOGS
template <typename Vertex> static bool anyMeshUploads(const MeshResourceData<Vertex> &data)
{
    for (const MeshPoolData<Vertex> &mpool : data.Pools)
        if (mpool.Flags & StatusFlag_NeedsSync)
            return true;
    return false;
}
#endif

template <Dimension D> static void upload(const SyncFlags flags)
{
    if (flags & SyncFlag_StaticMeshes)
    {
        TKIT_LOG_DEBUG_IF(anyMeshUploads(getData<D>().StaticMeshes), "[ONYX][RESOURCES] Uploading {}D static meshes",
                          u8(D));
        uploadMeshes(getData<D>().StaticMeshes);
    }

    if (flags & SyncFlag_ParametricMeshes)
    {
        TKIT_LOG_DEBUG_IF(anyMeshUploads(getData<D>().ParametricMeshes),
                          "[ONYX][RESOURCES] Uploading {}D parametric meshes", u8(D));
        uploadMeshes(getData<D>().ParametricMeshes);
    }

    if (flags & SyncFlag_Materials)
        uploadMaterials<D>();
}

void Sync(const SyncFlags flags)
{
    TKIT_BEGIN_INFO_CLOCK();
    TKIT_ASSERT(flags, "[ONYX][RESOURCES] Sync flags must not be zero");
    DeviceWaitIdle();
    upload<D2>(flags);
    upload<D3>(flags);
    if (flags & SyncFlag_Fonts)
    {
        TKIT_LOG_DEBUG_IF(anyMeshUploads(*s_FontData), "[ONYX][RESOURCES] Uploading fonts");
        uploadMeshes(*s_FontData);
    }

    // these here bc D3 may also request D2 bounds to be removed
    if (flags & (SyncFlag_StaticMeshes | SyncFlag_ParametricMeshes))
    {
        uploadBounds<D2>();
        uploadBounds<D3>();
    }

    if (flags & SyncFlag_Images)
    {
        TKIT_LOG_DEBUG_IF(!s_Images->ToDestroy.IsEmpty(), "[ONYX][RESOURCES] Destroying images");
        for (const Resource handle : s_Images->ToDestroy)
            DestroyImage(handle);
        s_Images->ToDestroy.Clear();
    }
    if (flags & SyncFlag_Samplers)
    {
        TKIT_LOG_DEBUG_IF(!s_Samplers->ToDestroy.IsEmpty(), "[ONYX][RESOURCES] Destroying samplers");
        for (const Resource handle : s_Samplers->ToDestroy)
            DestroySampler(handle);
        s_Samplers->ToDestroy.Clear();
    }

    Renderer::FlushAllContexts();
    TKIT_END_INFO_CLOCK(Milliseconds, "[ONYX][RESOURCES] Uploaded resources in {:.2f} milliseconds");
}

template Resource RegisterMaterial(const MaterialData<D2> &data);
template Resource RegisterMaterial(const MaterialData<D3> &data);

template void DestroyMaterial<D2>(Resource handle);
template void DestroyMaterial<D3>(Resource handle);

template void UpdateMaterial(Resource mesh, const MaterialData<D2> &data);
template void UpdateMaterial(Resource mesh, const MaterialData<D3> &data);

template Resource RegisterMesh(ResourcePool pool, const StatMeshData<D2> &data);
template Resource RegisterMesh(ResourcePool pool, const StatMeshData<D3> &data);

template void UpdateMesh(Resource handle, const StatMeshData<D2> &data);
template void UpdateMesh(Resource handle, const StatMeshData<D3> &data);

template Resource RegisterMesh(ResourcePool pool, const ParaMeshData<D2> &data);
template Resource RegisterMesh(ResourcePool pool, const ParaMeshData<D3> &data);

template void UpdateMesh(Resource handle, const ParaMeshData<D2> &data);
template void UpdateMesh(Resource handle, const ParaMeshData<D3> &data);

template ResourcePool CreateResourcePool<D2>(ResourceType rtype);
template ResourcePool CreateResourcePool<D3>(ResourceType rtype);

template void DestroyResourcePool<D2>(ResourcePool pool);
template void DestroyResourcePool<D3>(ResourcePool pool);

template StatMeshData<D2> GetStaticMeshData(Resource handle);
template StatMeshData<D3> GetStaticMeshData(Resource handle);

template ParaMeshData<D2> GetParametricMeshData(Resource handle);
template ParaMeshData<D3> GetParametricMeshData(Resource handle);

template ParametricShape GetParametricShape<D2>(Resource handle);
template ParametricShape GetParametricShape<D3>(Resource handle);

template const MaterialData<D2> &GetMaterialData(Resource handle);
template const MaterialData<D3> &GetMaterialData(Resource handle);

template GltfHandles RegisterGltfResources(ResourcePool meshPool, GltfData<D2> &resources);
template GltfHandles RegisterGltfResources(ResourcePool meshPool, GltfData<D3> &resources);

template TKit::Span<const u32> GetResourcePoolIds<D2>(ResourceType rtype);
template TKit::Span<const u32> GetResourcePoolIds<D3>(ResourceType rtype);

template u32 GetResourceCount<D2>(ResourcePool pool);
template u32 GetResourceCount<D3>(ResourcePool pool);

template MeshBuffers GetMeshBuffers<D2>(ResourcePool pool);
template MeshBuffers GetMeshBuffers<D3>(ResourcePool pool);

template bool IsResourceValid<D2>(Resource handle, ResourceType rtype);
template bool IsResourceValid<D3>(Resource handle, ResourceType rtype);

template bool IsResourcePoolValid<D2>(Handle handle, ResourceType rtype);
template bool IsResourcePoolValid<D3>(Handle handle, ResourceType rtype);

template u32 GetDistinctBatchDrawCount<D2>();
template u32 GetDistinctBatchDrawCount<D3>();

template MeshDataLayout GetMeshLayout<D2>(Resource handle);
template MeshDataLayout GetMeshLayout<D3>(Resource handle);

template Resource GetMeshBounds<D2>(Resource mesh);
template Resource GetMeshBounds<D3>(Resource mesh);

} // namespace Onyx::Resources
