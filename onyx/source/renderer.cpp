#include "pch.hpp"
#include "onyx/specs.hpp"
#include "onyx/context.hpp"
#include "core.hpp"
#include "instance.hpp"
#include "buffer.hpp"
#include "resources.hpp"
#include "renderer.hpp"
#include "conversion.hpp"
#include "pipelines.hpp"
#include "descriptors.hpp"
#include "execution.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "vkit/resource/device_image.hpp"
#include "vkit/resource/sampler.hpp"
#include "vkit/state/compute_pipeline.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/profiling/macros.hpp"

#ifdef ONYX_ENABLE_IMGUI
#    include <imgui.h>
#    include "imgui_backend.hpp"
#    ifdef ONYX_ENABLE_IMPLOT
#        include <implot.h>
#    endif
#endif

namespace Onyx::Renderer
{
using namespace Detail;
struct Range
{
    u32 Offset = 0;
    u32 Count = 0;
};
struct ContextInstanceRange
{
    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
    ViewMask ViewMask = 0;
    u64 Generation = 0;
    u32 ContextIndex = TKIT_U32_MAX;
};

struct TransferRange
{
    Execution::Tracker Tracker{};

    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
};

// note that graphics ranges, even tho purely gpu resources (not accessed/written by cpu directly) still have gpu usage
// trackers. originally i thought this was needed (dumb, i know) and then i realized i dont need trackers for gpu only
// resources as the sync is already laid out with gpu barriers and semaphores, so i was about to remove them. but ive
// decided to keep them only bc they serve as a good heuristic when tryin to choose resources. if i choose resources not
// in use by the gpu, i may minimize waits on the gpu/semaphore barriers. and because marking these resources as used
// pretty much costs nothing, ive decided to keep them at least for now
struct GraphicsRange
{
    Execution::Tracker TransferTracker{};
    Execution::Tracker GraphicsTracker{};

    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;

    // the generation is used to identify ranges. in the case of light, which range is the current one in use for
    // lights. in the case of vertex/index, to map graphics instance ranges with vertex/index graphics ranges
    u64 Generation = 0;

    bool InUseByTransfer() const
    {
        return TransferTracker.InUse();
    }
    bool InUseByGraphics() const
    {
        return GraphicsTracker.InUse();
    }
    bool InUse() const
    {
        return InUseByTransfer() || InUseByGraphics();
    }
};

template <typename T> struct Pool
{
    VKit::DeviceBuffer Buffer{};
    TKit::TierArray<T> Ranges{};
};

using TransferPool = Pool<TransferRange>;
using GraphicsPool = Pool<GraphicsRange>;

struct Arena
{
    TransferPool Transfer{};
    GraphicsPool Graphics{};
    // starting at one to honor Active*Range starting as well as one, so that value will never get assigned to any range
    // (++prefix will prevent this value showing up)
    u64 LatestGeneration = 1;
};

using TransferInstanceRange = TransferRange;
struct GraphicsInstanceRange
{
    Execution::Tracker TransferTracker{};
    Execution::Tracker GraphicsTracker{};

    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
    ViewMask ViewMask = 0;

    Resource MeshHandle = NullHandle;

    // only used for dynamic meshes
    // start at one to signal that at the beginning, no vertex/index range are associated with these
    u64 ActiveVertexGeneration = 1;
    u64 ActiveIndexGeneration = 1;

    GraphicsRange *ActiveVertexRange = nullptr;
    GraphicsRange *ActiveIndexRange = nullptr;
    //

    RenderModeFlags RenderFlags = 0;
    BlendPass Blend = BlendPass_None;
    TKit::TierArray<ContextInstanceRange> ContextRanges{};

    bool InUseByTransfer() const
    {
        return TransferTracker.InUse();
    }
    bool InUseByGraphics() const
    {
        return GraphicsTracker.InUse();
    }
    bool InUse() const
    {
        return InUseByTransfer() || InUseByGraphics();
    }
};

using TransferInstancePool = Pool<TransferInstanceRange>;
using GraphicsInstancePool = Pool<GraphicsInstanceRange>;

struct InstanceArena
{
    TransferInstancePool Transfer{};
    GraphicsInstancePool Graphics{};
};

using TransferLightRange = TransferRange;

using TransferLightPool = Pool<TransferLightRange>;
using GraphicsLightPool = Pool<GraphicsRange>;

struct DrawInfo
{
    u32 FirstInstance = 0;
    u32 InstanceCount = 0;
};

using InstanceDrawInfo = DrawInfo;

struct LightArena
{
    TransferLightPool Transfer{};
    GraphicsLightPool Graphics{};

    GraphicsRange *ActiveRange = nullptr;
    // starting at one, while ranges start at 0. meaning no active range will be able to be found
    // start at one to signal that at the beginning, no light range is active
    u64 ActiveGeneration = 1;

    u32 LightCount = 0;
};

struct GeometryData
{
    TKit::FixedArray<InstanceArena, Geometry_Count> Arenas{};
    Arena VertexArena{};
    Arena IndexArena{};
    TKit::FixedArray<TKit::FixedArray<TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count>, PipelinePass_Count>,
                     BlendPass_Count>
        Pipelines{};
};

template <typename LightParams> struct ContextLights
{
    TKit::TierArray<LightParams> Lights{};
    TKit::TierArray<typename LightParams::InstanceData> Data{};
};

template <Dimension D> struct LightInstances
{
    ContextLights<PointLightParameters<D>> Points{};
    ContextLights<DirectionalLightParameters<D>> Directionals{};

    void ClearLights()
    {
        Points.Lights.Clear();
        Directionals.Lights.Clear();
    }
};

template <> struct LightInstances<D3>
{
    ContextLights<PointLightParameters<D3>> Points{};
    ContextLights<DirectionalLightParameters<D3>> Directionals{};
    ContextLights<SpotLightParameters> Spots{};

    void ClearLights()
    {
        Points.Lights.Clear();
        Directionals.Lights.Clear();
        Spots.Lights.Clear();
    }
};

template <Dimension D> struct LightData
{
    LightInstances<D> Instances{};
    TKit::FixedArray<LightArena, LightTypeCount<D>> Arenas{};
    TKit::FixedArray<Range, LightTypeCount<D>> Ranges{};
};

struct TextureMap
{
    Execution::Tracker Tracker{};
    VKit::DeviceImage Image{};
};

using TextureMapArray = TKit::StaticArray<TextureMap, ONYX_MAX_TEXTURE_MAPS>;

template <Dimension D> struct ShadowData;

template <> struct ShadowData<D2>
{
    TKit::FixedArray<TextureMapArray, LightTypeCount<D2>> ShadowMaps{};
    TKit::FixedArray<u32, LightTypeCount<D2>> ShadowResolutions{};

    TextureMapArray OcclusionMaps{};
    u32 OcclusionResolution = 0;

    TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count> Pipelines{}; // occlusion pipelines
    VKit::ComputePipeline RayMarchPipeline{};
    VkDescriptorSet RayMarchSet = VK_NULL_HANDLE;
    VkFormat OcclusionFormat = VK_FORMAT_UNDEFINED;
    VkFormat ShadowFormat = VK_FORMAT_UNDEFINED;
    ViewMask DirtyShadowViews = 0;
    bool UsesFallback = false;
};

using ShadowMapId = u32;

template <> struct ShadowData<D3>
{
    TKit::FixedArray<TextureMapArray, LightTypeCount<D2>> ShadowMaps{};
    TKit::FixedArray<u32, LightTypeCount<D3>> ShadowResolutions{};

    TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count> Pipelines{};
    VkFormat ShadowFormat = VK_FORMAT_UNDEFINED;
    ViewMask DirtyShadowViews = 0;
};

template <Dimension D> struct ContextInfo
{
    RenderContext<D> *Context = nullptr;
    u64 Generation = 0;

    bool IsDirty() const
    {
        return Context->IsDirty(Generation);
    }
};

template <Dimension D> struct RendererData
{
    TKit::TierArray<ContextInfo<D>> Contexts{};
    TKit::TierArray<VkBufferMemoryBarrier2KHR> AcquireBarriers{};
    TKit::FixedArray<TKit::FixedArray<VkDescriptorSet, Geometry_Count>, RenderPass_Count> Descriptors{};

    GeometryData Geometry{};
    LightData<D> Lights{};
    ShadowData<D> Shadows{};

    bool IsContextRangeClean(const ContextInstanceRange &crange) const
    {
        return crange.ContextIndex != TKIT_U32_MAX &&
               !Contexts[crange.ContextIndex].Context->IsDirty(crange.Generation);
    }
    bool IsContextRangeClean(const ViewMask viewBit, const ContextInstanceRange &crange) const
    {
        return (crange.ViewMask & viewBit) && crange.ContextIndex != TKIT_U32_MAX &&
               !Contexts[crange.ContextIndex].Context->IsDirty(crange.Generation);
    }
    bool IsAnyContextRangeClean(const GraphicsInstanceRange &grange) const
    {
        for (const ContextInstanceRange &crange : grange.ContextRanges)
            if (IsContextRangeClean(crange))
                return true;
        return false;
    }
    bool IsAnyContextRangeDirty(const GraphicsInstanceRange &grange) const
    {
        for (const ContextInstanceRange &crange : grange.ContextRanges)
            if (!IsContextRangeClean(crange))
                return true;
        return false;
    }
    bool AreAllContextRangesClean(const GraphicsInstanceRange &grange) const
    {
        for (const ContextInstanceRange &crange : grange.ContextRanges)
            if (!IsContextRangeClean(crange))
                return false;
        return true;
    }
    bool AreAllContextRangesDirty(const GraphicsInstanceRange &grange) const
    {
        for (const ContextInstanceRange &crange : grange.ContextRanges)
            if (IsContextRangeClean(crange))
                return false;
        return true;
    }
};

struct DrawBuffer
{
    Execution::Tracker Tracker{};
    VKit::DeviceBuffer Buffer{};
};

static TKit::Storage<TKit::TierArray<DrawBuffer>> s_DrawBuffers{};
static TKit::Storage<TKit::TierArray<DrawBuffer>> s_IndexedDrawBuffers{};

static TKit::Storage<RendererData<D2>> s_RendererData2{};
static TKit::Storage<RendererData<D3>> s_RendererData3{};
static VKit::GraphicsPipeline s_BlendPipeline{};
static VKit::GraphicsPipeline s_PostProcessPipeline{};
static VKit::GraphicsPipeline s_CompositorPipeline{};

static VKit::Sampler s_LinearSampler{};
static VKit::Sampler s_CompareSampler{};
static VKit::Sampler s_NearSampler{};

static u64 s_SyncPointCount = 0;

template <Dimension D> static RendererData<D> &getRendererData()
{
    if constexpr (D == D2)
        return *s_RendererData2;
    else
        return *s_RendererData3;
}

template <Dimension D> static u32 getLightSize(const LightType light)
{
    switch (light)
    {
    case Light_Point:
        return sizeof(PointLightData<D>);
    case Light_Directional:
        return sizeof(DirectionalLightData<D>);
    case Light_Spot:
        return sizeof(SpotLightData);
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized light type");
        return 0;
    }
}

template <Dimension D> static void updateInstanceDescriptorSets(const Geometry geo)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < RenderPass_Count; ++i)
    {
        const RenderPass rpass = RenderPass(i);
        const VkDescriptorSet set = rdata.Descriptors[i][geo];
        const VkDescriptorBufferInfo info = rdata.Geometry.Arenas[geo].Graphics.Buffer.CreateDescriptorInfo();

        Descriptors::BindBuffer<D>(ONYX_INSTANCES_BINDING_POINT, set, info, rpass);
    }
}

static u32 lightToBinding(const LightType light)
{
    switch (light)
    {
    case Light_Point:
        return ONYX_POINT_LIGHTS_BINDING_POINT;
    case Light_Directional:
        return ONYX_DIRECTIONAL_LIGHTS_BINDING_POINT;
    case Light_Spot:
        return ONYX_SPOT_LIGHTS_BINDING_POINT;
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized light type: {}", u8(light));
        return 0;
    }
}

template <Dimension D> static void updateLightDescriptorSets(const LightType light)
{
    RendererData<D> &rdata = getRendererData<D>();
    const VkDescriptorBufferInfo info = rdata.Lights.Arenas[light].Graphics.Buffer.CreateDescriptorInfo();
    Descriptors::BindBuffer<D>(lightToBinding(light), rdata.Descriptors[RenderPass_Shaded], info, RenderPass_Shaded);
}

template <Dimension D>
static VKit::DeviceBuffer createTransferInstanceBuffer(const Geometry geo,
                                                       const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const VKit::DeviceBufferFlags flags = VKit::DeviceBufferFlags(Buffer_Staging) | DeviceBufferFlag_Destination;
    VKit::DeviceBuffer buffer = Onyx::CreateBuffer(flags, GetInstanceSize<D>(geo) * instances);

    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-transfer-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
        ONYX_CHECK_VKIT_RESULT(buffer.SetName(name.CString()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createTransferLightBuffer(const LightType light,
                                                    const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    VKit::DeviceBuffer buffer = Onyx::CreateBuffer(Buffer_Staging, instances * getLightSize<D>(light));
    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-transfer-light-buffer-{}D-type-{}", u8(D), ToString(light));
        ONYX_CHECK_VKIT_RESULT(buffer.SetName(name.CString()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createTransferVertexBuffer(const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const VKit::DeviceBufferFlags flags = DeviceBufferFlag_Vertex | DeviceBufferFlag_HostMapped |
                                          DeviceBufferFlag_HostRandomAccess | DeviceBufferFlag_Staging |
                                          DeviceBufferFlag_Destination;
    VKit::DeviceBuffer buffer = Onyx::CreateBuffer(flags, instances * sizeof(DynamicVertex<D>));
    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-renderer-transfer-vertex-buffer-{}D", u8(D));
        ONYX_CHECK_VKIT_RESULT(buffer.SetName(name.CString()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createTransferIndexBuffer(const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const VKit::DeviceBufferFlags flags = DeviceBufferFlag_Index | DeviceBufferFlag_HostMapped |
                                          DeviceBufferFlag_HostRandomAccess | DeviceBufferFlag_Staging |
                                          DeviceBufferFlag_Destination;
    VKit::DeviceBuffer buffer = Onyx::CreateBuffer(flags, instances * sizeof(Index));
    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-renderer-transfer-index-buffer-{}D", u8(D));
        ONYX_CHECK_VKIT_RESULT(buffer.SetName(name.CString()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createGraphicsInstanceBuffer(const Geometry geo,
                                                       const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const VKit::DeviceBufferFlags flags = VKit::DeviceBufferFlags(Buffer_DeviceStorage) | DeviceBufferFlag_Source;
    VKit::DeviceBuffer buffer = Onyx::CreateBuffer(flags, instances * GetInstanceSize<D>(geo));
    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-graphics-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
        ONYX_CHECK_VKIT_RESULT(buffer.SetName(name.CString()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createGraphicsLightBuffer(const LightType light,
                                                    const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    VKit::DeviceBuffer buffer = Onyx::CreateBuffer(Buffer_DeviceStorage, instances * getLightSize<D>(light));
    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name =
            TKit::StackString::Format("onyx-renderer-graphics-light-buffer-{}D-type-{}", u8(D), ToString(light));
        ONYX_CHECK_VKIT_RESULT(buffer.SetName(name.CString()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createGraphicsVertexBuffer(const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const VKit::DeviceBufferFlags flags =
        DeviceBufferFlag_Vertex | DeviceBufferFlag_DeviceLocal | DeviceBufferFlag_Source;

    VKit::DeviceBuffer buffer = Onyx::CreateBuffer(flags, instances * sizeof(DynamicVertex<D>));
    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-renderer-graphics-vertex-buffer-{}D", u8(D));
        ONYX_CHECK_VKIT_RESULT(buffer.SetName(name.CString()));
    }
    return buffer;
}
template <Dimension D>
static VKit::DeviceBuffer createGraphicsIndexBuffer(const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    const VKit::DeviceBufferFlags flags =
        DeviceBufferFlag_Index | DeviceBufferFlag_DeviceLocal | DeviceBufferFlag_Source;

    VKit::DeviceBuffer buffer = Onyx::CreateBuffer(flags, instances * sizeof(Index));
    if (IsDebugUtilsEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-renderer-graphics-index-buffer-{}D", u8(D));
        ONYX_CHECK_VKIT_RESULT(buffer.SetName(name.CString()));
    }
    return buffer;
}

template <Dimension D> static void createPipelines()
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    for (u32 j = 0; j < Geometry_Count; ++j)
    {
        const Geometry geo = Geometry(j);
        for (u32 i = 0; i < PipelinePass_Count; ++i)
        {
            for (u32 k = 0; k < BlendPass_Count; ++k)
            {
                const BlendPass bpass = BlendPass(k);
                const PipelinePass pass = PipelinePass(i);
                rdata.Geometry.Pipelines[bpass][pass][geo] = Pipelines::CreateGeometryPipeline<D>(pass, bpass, geo);

                if (IsDebugUtilsEnabled())
                {
                    const TKit::StackString name =
                        TKit::StackString::Format("onyx-renderer-geometry-pipeline-{}D-{}-pass-{}-geometry-'{}'", u8(D),
                                                  ToString(bpass), ToString(pass), ToString(geo));
                    ONYX_CHECK_VKIT_RESULT(rdata.Geometry.Pipelines[bpass][pass][geo].SetName(name.CString()));
                }
            }
        }
        VkFormat format;
        if constexpr (D == D2)
            format = sdata.OcclusionFormat;
        else
            format = sdata.ShadowFormat;

        sdata.Pipelines[geo] = Pipelines::CreateShadowPipeline<D>(geo, format);
        if (IsDebugUtilsEnabled())
        {
            const TKit::StackString name =
                TKit::StackString::Format("onyx-renderer-shadow-pipeline-{}D-geometry-'{}'", u8(D), ToString(geo));
            ONYX_CHECK_VKIT_RESULT(sdata.Pipelines[geo].SetName(name.CString()));
        }
    }

    if constexpr (D == D2)
    {
        sdata.RayMarchPipeline = Pipelines::CreateRayMarchPipeline();
        if (IsDebugUtilsEnabled())
        {
            ONYX_CHECK_VKIT_RESULT(sdata.RayMarchPipeline.SetName("onyx-renderer-ray-march-pipeline"));
        }
    }
}

static void createPipelines()
{
    s_BlendPipeline = Pipelines::CreateBlendPipeline();
    s_PostProcessPipeline = Pipelines::CreatePostProcessPipeline();
    s_CompositorPipeline = Pipelines::CreateCompositorPipeline();

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_VKIT_RESULT(s_BlendPipeline.SetName("onyx-blend-pipeline"));
        ONYX_CHECK_VKIT_RESULT(s_PostProcessPipeline.SetName("onyx-post-process-pipeline"));
        ONYX_CHECK_VKIT_RESULT(s_CompositorPipeline.SetName("onyx-compositor-pipeline"));
    }

    createPipelines<D2>();
    createPipelines<D3>();
}

template <Dimension D> static void destroyPipelines()
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;
    for (u32 geo = 0; geo < Geometry_Count; ++geo)
    {
        for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
            for (u32 pass = 0; pass < PipelinePass_Count; ++pass)
                rdata.Geometry.Pipelines[bpass][pass][geo].Destroy();
        sdata.Pipelines[geo].Destroy();
    }
    if constexpr (D == D2)
        sdata.RayMarchPipeline.Destroy();
}

static void destroyPipelines()
{
    destroyPipelines<D2>();
    destroyPipelines<D3>();
    s_BlendPipeline.Destroy();
    s_PostProcessPipeline.Destroy();
    s_CompositorPipeline.Destroy();
}

template <Dimension D> static void initializeLights()
{
    LightData<D> &ldata = getRendererData<D>().Lights;
    for (u32 i = 0; i < LightTypeCount<D>; ++i)
    {
        const LightType light = LightType(i);
        TransferLightPool &tpool = ldata.Arenas[light].Transfer;

        tpool.Buffer = createTransferLightBuffer<D>(light);
        tpool.Ranges.Append(TransferLightRange{.Size = tpool.Buffer.GetInfo().Size});

        GraphicsLightPool &gpool = ldata.Arenas[light].Graphics;
        gpool.Buffer = createGraphicsLightBuffer<D>(light);

        gpool.Ranges.Append(GraphicsRange{.Size = gpool.Buffer.GetInfo().Size});
        updateLightDescriptorSets<D>(light);
    }
}

template <Dimension D> static void initializeShadows(const ShadowSpecs<D> &specs)
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;

    VkDescriptorImageInfo info{};
    info.sampler = s_LinearSampler;
    BindImage<D>(ONYX_SHADOW_SAMPLER_BINDING_POINT, info, RenderPass_Shaded);

    info.sampler = s_CompareSampler;
    BindImage<D>(ONYX_SHADOW_COMPARE_SAMPLER_BINDING_POINT, info, RenderPass_Shaded);

    sdata.ShadowFormat = AsVulkanFormat(specs.ShadowFormat);
    if constexpr (D == D2)
    {
        sdata.OcclusionFormat = AsVulkanFormat(specs.OcclusionFormat);
        sdata.OcclusionResolution = specs.OcclusionResolution;
        sdata.RayMarchSet = ONYX_CHECK_VKIT_RESULT(
            Descriptors::GetDescriptorPool().Allocate(Descriptors::GetDescriptorLayout(StandalonePass_RayMarch)));

        VkPhysicalDeviceImageFormatInfo2KHR finfo{};
        finfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR;
        finfo.format = sdata.ShadowFormat;
        finfo.type = VK_IMAGE_TYPE_1D;
        finfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        finfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

        VkImageFormatProperties2KHR props{};
        props.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR;

        const auto table = GetInstanceTable();
        const VkResult result = table->GetPhysicalDeviceImageFormatProperties2KHR(GetPhysicalDevice(), &finfo, &props);
        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            sdata.ShadowFormat = AsVulkanFormat(specs.FallbackShadowFormat);
            sdata.UsesFallback = true;
        }
    }

    sdata.ShadowResolutions = specs.ShadowResolutions;
}

template <Dimension D> static void initialize(const ShadowSpecs<D> &shadowSpecs)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);

        TransferInstancePool &tpool = rdata.Geometry.Arenas[geo].Transfer;
        tpool.Buffer = createTransferInstanceBuffer<D>(geo);
        tpool.Ranges.Append(TransferInstanceRange{.Size = tpool.Buffer.GetInfo().Size});

        GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;
        gpool.Buffer = createGraphicsInstanceBuffer<D>(geo);
        gpool.Ranges.Append(GraphicsInstanceRange{.Size = gpool.Buffer.GetInfo().Size});

        for (u32 j = 0; j < RenderPass_Count; ++j)
        {
            const RenderPass rpass = RenderPass(j);
            const VKit::DescriptorSetLayout &layout = Descriptors::GetDescriptorLayout<D>(rpass);
            const VkDescriptorSet set = ONYX_CHECK_VKIT_RESULT(Descriptors::GetDescriptorPool().Allocate(layout));
            if (IsDebugUtilsEnabled())
            {
                const auto &device = GetDevice();
                const TKit::StackString name = TKit::StackString::Format(
                    "onyx-renderer-descriptor-set-{}D-geometry-{}-pass-{}", u8(D), ToString(geo), ToString(rpass));
                ONYX_CHECK_VKIT_RESULT(device.SetObjectName(set, VK_OBJECT_TYPE_DESCRIPTOR_SET, name.CString()));
            }

            const VkDescriptorBufferInfo info = gpool.Buffer.CreateDescriptorInfo();
            Descriptors::BindBuffer<D>(ONYX_INSTANCES_BINDING_POINT, set, info, rpass);
            rdata.Descriptors[j][i] = set;
        }
    }

    TransferPool &vtpool = rdata.Geometry.VertexArena.Transfer;
    vtpool.Buffer = createTransferVertexBuffer<D>();
    vtpool.Ranges.Append(TransferRange{.Size = vtpool.Buffer.GetInfo().Size});

    TransferPool &itpool = rdata.Geometry.IndexArena.Transfer;
    itpool.Buffer = createTransferIndexBuffer<D>();
    itpool.Ranges.Append(TransferRange{.Size = itpool.Buffer.GetInfo().Size});

    GraphicsPool &vgpool = rdata.Geometry.VertexArena.Graphics;
    vgpool.Buffer = createGraphicsVertexBuffer<D>();
    vgpool.Ranges.Append(GraphicsRange{.Size = vgpool.Buffer.GetInfo().Size});

    GraphicsPool &igpool = rdata.Geometry.IndexArena.Graphics;
    igpool.Buffer = createGraphicsIndexBuffer<D>();
    igpool.Ranges.Append(GraphicsRange{.Size = igpool.Buffer.GetInfo().Size});

    initializeLights<D>();
    return initializeShadows(shadowSpecs);
}

template <Dimension D> static void terminateShadows()
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;
    for (auto &maps : sdata.ShadowMaps)
        for (auto &map : maps)
            map.Image.Destroy();
    if constexpr (D == D2)
        for (auto &map : sdata.OcclusionMaps)
            map.Image.Destroy();
}

template <Dimension D> static void terminate()
{
    RendererData<D> &rdata = getRendererData<D>();

    DeviceWaitIdle();
    for (InstanceArena &arena : rdata.Geometry.Arenas)
    {
        arena.Transfer.Buffer.Destroy();
        arena.Graphics.Buffer.Destroy();
    }
    for (LightArena &arena : rdata.Lights.Arenas)
    {
        arena.Transfer.Buffer.Destroy();
        arena.Graphics.Buffer.Destroy();
    }

    rdata.Geometry.VertexArena.Graphics.Buffer.Destroy();
    rdata.Geometry.VertexArena.Transfer.Buffer.Destroy();
    rdata.Geometry.IndexArena.Graphics.Buffer.Destroy();
    rdata.Geometry.IndexArena.Transfer.Buffer.Destroy();

    terminateShadows<D>();

    TKit::TierAllocator *tier = TKit::GetTier();
    for (const ContextInfo<D> &info : rdata.Contexts)
        tier->Destroy(info.Context);
}

void Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][RENDERER] Initializing");
    s_DrawBuffers.Construct();
    s_IndexedDrawBuffers.Construct();
    s_RendererData2.Construct();
    s_RendererData3.Construct();

    VKit::Sampler::Builder builder{GetDevice()};

    s_LinearSampler = ONYX_CHECK_VKIT_RESULT(VKit::Sampler::Builder(GetDevice())
                                                 .SetMinFilter(VK_FILTER_LINEAR)
                                                 .SetMagFilter(VK_FILTER_LINEAR)
                                                 .SetAddressModes(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
                                                 .SetBorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
                                                 .Build());

    s_NearSampler = ONYX_CHECK_VKIT_RESULT(
        VKit::Sampler::Builder(GetDevice()).SetMinFilter(VK_FILTER_NEAREST).SetMagFilter(VK_FILTER_NEAREST).Build());

    s_CompareSampler = ONYX_CHECK_VKIT_RESULT(VKit::Sampler::Builder(GetDevice())
                                                  .SetMinFilter(VK_FILTER_LINEAR)
                                                  .SetMagFilter(VK_FILTER_LINEAR)
                                                  .EnableCompare()
                                                  .SetCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
                                                  .SetAddressModes(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
                                                  .SetBorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
                                                  .Build());

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_VKIT_RESULT(s_LinearSampler.SetName("onyx-linear-sampler"));
        ONYX_CHECK_VKIT_RESULT(s_NearSampler.SetName("onyx-near-sampler"));
        ONYX_CHECK_VKIT_RESULT(s_CompareSampler.SetName("onyx-compare-sampler"));
    }

    initialize<D2>(specs.Shadows2);
    initialize<D3>(specs.Shadows3);
    return createPipelines();
}
void Terminate()
{
    terminate<D2>();
    terminate<D3>();

    destroyPipelines();

    for (DrawBuffer &buffer : *s_DrawBuffers)
        buffer.Buffer.Destroy();
    for (DrawBuffer &buffer : *s_IndexedDrawBuffers)
        buffer.Buffer.Destroy();
    s_LinearSampler.Destroy();
    s_NearSampler.Destroy();
    s_CompareSampler.Destroy();
    s_RendererData2.Destruct();
    s_RendererData3.Destruct();
    s_IndexedDrawBuffers.Destruct();
    s_DrawBuffers.Destruct();
}

template <Dimension D> RenderContext<D> *CreateContext(const u32 immediateDynamicMeshCapacity)
{
    RendererData<D> &rdata = getRendererData<D>();
    ContextInfo<D> info;

    TKit::TierAllocator *tier = TKit::GetTier();
    info.Context = tier->Create<RenderContext<D>>(immediateDynamicMeshCapacity);
    info.Generation = info.Context->GetGeneration();

    rdata.Contexts.Append(info);
    return info.Context;
}

template <Dimension D> static u32 getContextIndex(const RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < rdata.Contexts.GetSize(); ++i)
        if (rdata.Contexts[i].Context == context)
            return i;
    TKIT_FATAL("[ONYX][RENDERER] Render context ({}) index not found", scast<const void *>(context));
    return TKIT_U32_MAX;
}

template <Dimension D> void DestroyContext(RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u32 index = getContextIndex(context);
    for (InstanceArena &arena : rdata.Geometry.Arenas)
        for (GraphicsInstanceRange &grange : arena.Graphics.Ranges)
        {
            ViewMask vmask = 0;
            for (ContextInstanceRange &crange : grange.ContextRanges)
            {
                if (crange.ContextIndex != TKIT_U32_MAX && crange.ContextIndex > index)
                    --crange.ContextIndex;
                else if (crange.ContextIndex == index)
                {
                    crange.ViewMask = 0;
                    crange.ContextIndex = TKIT_U32_MAX;
                }
                vmask |= crange.ViewMask;
            }
            grange.ViewMask = vmask;
        }

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(context);
    rdata.Contexts.RemoveOrdered(rdata.Contexts.begin() + index);
}

template <Dimension D> void flushAllContexts()
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const ContextInfo<D> &info : rdata.Contexts)
        info.Context->Flush();
}

void FlushAllContexts()
{
    flushAllContexts<D2>();
    flushAllContexts<D3>();
}

void ReloadPipelines()
{
    DeviceWaitIdle();
    destroyPipelines();
    return createPipelines();
}
bool IsDepthSupportedFor2D()
{
    return !s_RendererData2->Shadows.UsesFallback;
}

template <Dimension D> static void addTarget(const ViewMask vmask)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const ContextInfo<D> &info : rdata.Contexts)
        info.Context->AddTarget(vmask);
}
template <Dimension D> static void removeTarget(const ViewMask vmask)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (const ContextInfo<D> &info : rdata.Contexts)
        info.Context->RemoveTarget(vmask);
}

void AddTarget(const ViewMask vmask)
{
    addTarget<D2>(vmask);
    addTarget<D2>(vmask);
}
void RemoveTarget(const ViewMask vmask)
{
    removeTarget<D2>(vmask);
    removeTarget<D3>(vmask);
}

template <Dimension D>
void BindBuffer(const u32 binding, TKit::Span<const VkDescriptorBufferInfo> info, const RenderPass pass,
                const u32 dstElement)
{
    RendererData<D> &rdata = getRendererData<D>();
    Descriptors::BindBuffer<D>(binding, rdata.Descriptors[pass], info, pass, dstElement);
}
template <Dimension D>
void BindImage(const u32 binding, TKit::Span<const VkDescriptorImageInfo> info, const RenderPass pass,
               const u32 dstElement)
{
    RendererData<D> &rdata = getRendererData<D>();
    Descriptors::BindImage<D>(binding, rdata.Descriptors[pass], info, pass, dstElement);
}

const VKit::Sampler &GetNearSampler()
{
    return s_NearSampler;
}

template <Dimension D> const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets(const RenderPass pass)
{
    return getRendererData<D>().Descriptors[pass];
}

#ifdef TKIT_ENABLE_ASSERTS
template <typename Range> static void validateRanges(const char *name, const Pool<Range> &pool)
{
    const auto &ranges = pool.Ranges;
    const VKit::DeviceBuffer::Info &info = pool.Buffer.GetInfo();
    VkDeviceSize size = 0;
    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        const Range &range = ranges[i];
        TKIT_ASSERT(info.Size >= range.Offset + range.Size,
                    "[ONYX][RENDERER] A {} memory range with index {} ({} total) exceeds buffer "
                    "size. Buffer size is {} bytes, which is smaller than offset + size = {} + {} = {}",
                    name, i, ranges.GetSize(), info.Size, range.Offset, range.Size, range.Offset + range.Size);
        if (i != 0)
        {
            const Range &prange = ranges[i - 1];
            TKIT_ASSERT(prange.Offset + prange.Size == range.Offset,
                        "[ONYX][RENDERER] A {} memory range pair with indices {} and {} ({} total) are not perfectly "
                        "next to each other, meaning offset{} + size{} != offset{} -> {} + {} = {} != {}",
                        name, i, i - 1, ranges.GetSize(), i - 1, i - 1, i, prange.Offset, prange.Size,
                        prange.Offset + prange.Size, range.Offset);
        }
        size += range.Size;
    }
    TKIT_ASSERT(size == info.Size,
                "[ONYX][RENDERER] The sum of the {} memory range sizes ({:L}) does not equal the one of the "
                "buffer ({:L})",
                name, size, info.Size);
}

static void validateGraphicsInstanceRanges(const GraphicsInstancePool &gpool)
{
    const auto &granges = gpool.Ranges;
    const VKit::DeviceBuffer::Info &ginfo = gpool.Buffer.GetInfo();
    VkDeviceSize gsize = 0;
    for (u32 i = 0; i < granges.GetSize(); ++i)
    {
        const GraphicsInstanceRange &grange = granges[i];
        TKIT_ASSERT(ginfo.Size >= grange.Offset + grange.Size,
                    "[ONYX][RENDERER] A graphics instance memory range with index {} ({} total) exceeds buffer size"
                    ". Buffer size is {:L} bytes, which is smaller than offset + size = {:L} + {:L} = {:L}",
                    i, granges.GetSize(), ginfo.Size, grange.Offset, grange.Size, grange.Offset + grange.Size);
        if (i != 0)
        {
            const GraphicsInstanceRange &pgrange = granges[i - 1];
            TKIT_ASSERT(pgrange.Offset + pgrange.Size == grange.Offset,
                        "[ONYX][RENDERER] A graphics instance memory range pair with indices {} and {} ({} total) are "
                        "not perfectly "
                        "next to each other, meaning offset{} + size{} != offset{} -> {:L} + {:L} = {:L} != {:L}",
                        i, i - 1, granges.GetSize(), i - 1, i - 1, i, pgrange.Offset, pgrange.Size,
                        pgrange.Offset + pgrange.Size, grange.Offset);
        }
        const auto &cranges = grange.ContextRanges;
        VkDeviceSize csize = 0;
        ViewMask vmask = 0;
        for (u32 j = 0; j < cranges.GetSize(); ++j)
        {
            const ContextInstanceRange &crange = cranges[j];
            TKIT_ASSERT(
                (crange.ViewMask & grange.ViewMask) || crange.ViewMask == grange.ViewMask || crange.ViewMask == 0,
                "[ONYX][RENDERER] A context memory range with index {} ({} total) has one or more view bits not "
                "present in graphics range view mask (context range has {:032b} while graphics range has {:032b})",
                i, cranges.GetSize(), crange.ViewMask, grange.ViewMask);

            vmask |= crange.ViewMask;
            TKIT_ASSERT(
                j != 0 || crange.Offset == 0,
                "[ONYX][RENDERER] First context range offset of a graphics range offset must be zero, but is {:L}",
                crange.Offset);

            TKIT_ASSERT(j != cranges.GetSize() - 1 || crange.Offset + crange.Size == grange.Size,
                        "[ONYX][RENDERER] Last context expression: offset + size = {} + {} = {} must be equal to "
                        "graphics context size of {}",
                        crange.Offset, crange.Size, crange.Offset + crange.Size, grange.Size);

            TKIT_ASSERT(grange.Size >= crange.Offset + crange.Size,
                        "[ONYX][RENDERER] A context memory range with index {} ({} total) exceeds graphics range size. "
                        "Range size is {:L} bytes, which is smaller than offset + size = {:L} + {:L} = {:L}",
                        j, cranges.GetSize(), grange.Size, crange.Offset, crange.Size, crange.Offset + crange.Size);
            TKIT_ASSERT(ginfo.Size >= crange.Offset + crange.Size,
                        "[ONYX][RENDERER] A context memory range with index {} ({} total) exceeds buffer size. "
                        "Buffer size is {:L} bytes, which is smaller than offset + size = {:L} + {:L} = {:L}",
                        j, cranges.GetSize(), ginfo.Size, crange.Offset, crange.Size, crange.Offset + crange.Size);
            if (j != 0)
            {
                const ContextInstanceRange &pcrange = cranges[j - 1];
                TKIT_ASSERT(pcrange.Offset + pcrange.Size == crange.Offset,
                            "[ONYX][RENDERER] A context memory range pair with indices {} and {} ({} total) are not "
                            "perfectly "
                            "next to each other, meaning offset{} + size{} != offset{} -> {:L} + {:L} = {:L} != {:L}",
                            j, j - 1, cranges.GetSize(), j - 1, j - 1, j, pcrange.Offset, pcrange.Size,
                            pcrange.Offset + pcrange.Size, crange.Offset);
            }
            csize += crange.Size;
        }
        TKIT_ASSERT(vmask == grange.ViewMask,
                    "[ONYX][RENDERER] Combined context range viewmasks ({:032b}) does not match the view mask of "
                    "the graphics range ({:032b})",
                    vmask, grange.ViewMask);

        TKIT_ASSERT(csize <= grange.Size,
                    "[ONYX][RENDERER] The sum of the context memory range sizes ({:L}) exceeds the size of its "
                    "parent range ({:L})",
                    csize, grange.Size);
        gsize += grange.Size;
    }
    TKIT_ASSERT(
        gsize == ginfo.Size,
        "[ONYX][RENDERER] The sum of the graphics instance memory range sizes ({:L}) does not equal the one of the "
        "buffer ({:L})",
        gsize, ginfo.Size);
}

template <Dimension D> static void validateRanges()
{
    const RendererData<D> &rdata = getRendererData<D>();
    for (const InstanceArena &arena : rdata.Geometry.Arenas)
    {
        validateRanges("transfer instance", arena.Transfer);
        validateGraphicsInstanceRanges(arena.Graphics);
    }

    validateRanges("transfer vertex", rdata.Geometry.VertexArena.Transfer);
    validateRanges("graphics vertex", rdata.Geometry.VertexArena.Graphics);

    validateRanges("transfer index", rdata.Geometry.IndexArena.Transfer);
    validateRanges("graphics index", rdata.Geometry.IndexArena.Graphics);

    const LightData<D> &ldata = rdata.Lights;
    for (const LightArena &arena : ldata.Arenas)
    {
        validateRanges("transfer light", arena.Transfer);
        validateRanges("graphics light", arena.Graphics);
        for (u32 i = 0; i < arena.Graphics.Ranges.GetSize(); ++i)
        {
            const GraphicsRange &grange = arena.Graphics.Ranges[i];
            TKIT_ASSERT(grange.Generation <= arena.ActiveGeneration,
                        "[ONYX][RENDERER] Found graphics light memory range of index {} whose generation {} exceeds "
                        "the one of the arena {}",
                        i, grange.Generation, arena.ActiveGeneration);
        }
    }
}
#endif

template <Dimension D, typename Range>
static Range *handlePoolResize(const VkDeviceSize requiredMem, VKit::DeviceBuffer &nbuffer, VKit::DeviceBuffer &buffer,
                               TKit::TierArray<Range> &ranges, TKit::StackArray<Task> *tasks = nullptr,
                               VKit::Queue *transfer = nullptr)
{
    // this is a proxy that tells *this function* if we need to copy old contents. we do this for instance, vertex and
    // index ranges, but not for light ranges. we also happen to end all copy tasks on the three former, so thats how we
    // know;
    const bool copyOldContents = bool(tasks);
    if (tasks)
    {
        for (const Task &task : *tasks)
            task.WaitUntilFinished();
        tasks->Clear();
    }

    TKit::StackArray<VkSemaphore> semaphores{};
    semaphores.Reserve(2 * ranges.GetSize());
    TKit::StackArray<u64> values{};
    values.Reserve(2 * ranges.GetSize());

    for (const Range &range : ranges)
        if constexpr (std::is_same_v<Range, GraphicsInstanceRange> || std::is_same_v<Range, GraphicsRange>)
        {
            if (range.TransferTracker.InFlight())
            {
                semaphores.Append(range.TransferTracker.Queue->GetTimelineSempahore());
                values.Append(range.TransferTracker.InFlightValue);
            }

            if (range.GraphicsTracker.InFlight())
            {
                semaphores.Append(range.GraphicsTracker.Queue->GetTimelineSempahore());
                values.Append(range.GraphicsTracker.InFlightValue);
            }
        }
        else if (range.Tracker.InFlight())
        {
            semaphores.Append(range.Tracker.Queue->GetTimelineSempahore());
            values.Append(range.Tracker.InFlightValue);
        }
    if (!semaphores.IsEmpty())
    {
        VkSemaphoreWaitInfoKHR waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
        waitInfo.semaphoreCount = semaphores.GetSize();
        waitInfo.pSemaphores = semaphores.GetData();
        waitInfo.pValues = values.GetData();

        const auto &device = GetDevice();
        const auto table = GetDeviceTable();
        ONYX_CHECK_VKIT_RESULT(table->WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX));
    }

    const VkDeviceSize size = buffer.GetInfo().Size;
    const VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = size};

    if (copyOldContents)
    {
        if constexpr (std::is_same_v<Range, GraphicsInstanceRange> || std::is_same_v<Range, GraphicsRange>)
        {
            TKIT_ASSERT(transfer);
            ONYX_CHECK_VKIT_RESULT(
                nbuffer.CopyFromBuffer(Execution::GetTransientTransferPool(), *transfer, buffer, copy));
        }
        else if constexpr (std::is_same_v<Range, TransferInstanceRange> || std::is_same_v<Range, TransferRange>)
            nbuffer.Write(buffer.GetData(), copy);
    }

    buffer.Destroy();
    buffer = nbuffer;

    Range smallRange{};
    smallRange.Offset = size;
    smallRange.Size = requiredMem;

    ranges.Append(smallRange);
    const VkDeviceSize nsize = nbuffer.GetInfo().Size;
    const VkDeviceSize offset = size + requiredMem;
    if (nsize == offset)
        return &ranges[ranges.GetSize() - 1];

    Range bigRange{};
    bigRange.Offset = offset;
    bigRange.Size = nsize - offset;

    ranges.Append(bigRange);

    return &ranges[ranges.GetSize() - 2];
}

template <typename Range>
static Range *splitRange(const u32 index, TKit::TierArray<Range> &ranges, const VkDeviceSize requiredMem)
{
    Range &range = ranges[index];
    if (range.Size == requiredMem)
        return &range;

    Range child;
    child.Size = requiredMem;
    child.Offset = range.Offset;

    range.Offset += requiredMem;
    range.Size -= requiredMem;
    if constexpr (std::is_same_v<Range, GraphicsInstanceRange>)
    {
        range.ViewMask = 0;
        range.MeshHandle = NullHandle;
        range.RenderFlags = 0;
        range.ContextRanges.Clear();
    }

    ranges.Insert(ranges.begin() + index, child);
    return &ranges[index];
}

static u32 computeNewInstanceCount(const u32 instanceSize, VKit::DeviceBuffer &buffer, const VkDeviceSize requiredMem)
{
    const VkDeviceSize size = buffer.GetInfo().Size;
    const u32 icount = 2 * u32(Math::Max(requiredMem, size) / instanceSize);

    TKIT_LOG_DEBUG("[ONYX][RENDERER] Failed to find an available range with {:L} bytes of memory. A new buffer "
                   "will be created with more memory (from {:L} to {:L} bytes)",
                   requiredMem, size, icount * instanceSize);
    return icount;
}

template <Dimension D, typename Range, typename Pool, typename F>
static Range *findTransferRange(Pool &pool, const VkDeviceSize requiredMem, const F createBuffer,
                                TKit::StackArray<Task> *tasks = nullptr)
{
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    for (u32 i = 0; i < ranges.GetSize(); ++i)
        if (ranges[i].Size >= requiredMem && !ranges[i].Tracker.InUse())
            return splitRange(i, ranges, requiredMem);

    VKit::DeviceBuffer nbuffer = createBuffer();
    return handlePoolResize<D>(requiredMem, nbuffer, pool.Buffer, ranges, tasks);
}

template <Dimension D>
static TransferInstanceRange *findTransferInstanceRange(const Geometry geo, TransferInstancePool &pool,
                                                        const VkDeviceSize requiredMem, TKit::StackArray<Task> &tasks)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindTransferInstanceRange");
    return findTransferRange<D, TransferInstanceRange>(
        pool, requiredMem,
        [&] {
            return createTransferInstanceBuffer<D>(
                geo, computeNewInstanceCount(GetInstanceSize<D>(geo), pool.Buffer, requiredMem));
        },
        &tasks);
}

template <Dimension D>
static TransferLightRange *findTransferLightRange(const LightType light, TransferLightPool &pool,
                                                  const VkDeviceSize requiredMem)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindTransferLightRange");
    return findTransferRange<D, TransferLightRange>(pool, requiredMem, [&] {
        return createTransferLightBuffer<D>(light,
                                            computeNewInstanceCount(getLightSize<D>(light), pool.Buffer, requiredMem));
    });
}

template <Dimension D>
static TransferRange *findTransferVertexRange(TransferLightPool &pool, const VkDeviceSize requiredMem,
                                              TKit::StackArray<Task> &tasks)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindVertexTransferRange");
    return findTransferRange<D, TransferRange>(
        pool, requiredMem,
        [&] {
            return createTransferVertexBuffer<D>(
                computeNewInstanceCount(sizeof(DynamicVertex<D>), pool.Buffer, requiredMem));
        },
        &tasks);
}
template <Dimension D>
static TransferRange *findTransferIndexRange(TransferLightPool &pool, const VkDeviceSize requiredMem,
                                             TKit::StackArray<Task> &tasks)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindIndexTransferRange");
    return findTransferRange<D, TransferRange>(
        pool, requiredMem,
        [&] { return createTransferIndexBuffer<D>(computeNewInstanceCount(sizeof(Index), pool.Buffer, requiredMem)); },
        &tasks);
}

template <Dimension D, typename Range, typename Pool, typename F1, typename F2>
static Range *findGraphicsRange(Pool &pool, const VkDeviceSize requiredMem, const F1 createBuffer,
                                const F2 canRangeSplit, bool *resized = nullptr,
                                TKit::StackArray<Task> *tasks = nullptr, VKit::Queue *transfer = nullptr)
{
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        Range &range = ranges[i];
        if (range.Size >= requiredMem && !range.InUse() && canRangeSplit(range))
            return splitRange(i, ranges, requiredMem);
    }
    if (resized)
        *resized = true;

    VKit::DeviceBuffer &buffer = pool.Buffer;
    for (u32 i = rdata.AcquireBarriers.GetSize() - 1; i < rdata.AcquireBarriers.GetSize(); --i)
    {
        const VkBufferMemoryBarrier2KHR &barrier = rdata.AcquireBarriers[i];
        if (barrier.buffer == buffer.GetHandle())
            rdata.AcquireBarriers.RemoveUnordered(rdata.AcquireBarriers.begin() + i);
    }

    VKit::DeviceBuffer nbuffer = createBuffer();
    return handlePoolResize<D>(requiredMem, nbuffer, buffer, ranges, tasks, transfer);
}
template <Dimension D, typename Range, typename Pool, typename F>
static Range *findGraphicsRange(Pool &pool, const VkDeviceSize requiredMem, const F createBuffer,
                                bool *resized = nullptr, TKit::StackArray<Task> *tasks = nullptr,
                                VKit::Queue *transfer = nullptr)
{
    return findGraphicsRange<D, Range>(
        pool, requiredMem, createBuffer, [](const auto &) { return true; }, resized, tasks, transfer);
}

template <Dimension D>
static GraphicsInstanceRange *findGraphicsInstanceRange(const Geometry geo, GraphicsInstancePool &pool,
                                                        const VkDeviceSize requiredMem, VKit::Queue *transfer,
                                                        TKit::StackArray<Task> &tasks)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindGraphicsInstanceRange");
    RendererData<D> &rdata = getRendererData<D>();
    bool resized = false;
    GraphicsInstanceRange *range = findGraphicsRange<D, GraphicsInstanceRange>(
        pool, requiredMem,
        [&] {
            return createGraphicsInstanceBuffer<D>(
                geo, computeNewInstanceCount(GetInstanceSize<D>(geo), pool.Buffer, requiredMem));
        },
        [&](const GraphicsInstanceRange &range) { return rdata.AreAllContextRangesDirty(range); }, &resized, &tasks,
        transfer);
    if (resized)
        updateInstanceDescriptorSets<D>(geo);
    return range;
}

template <Dimension D>
static GraphicsRange *findGraphicsLightRange(const LightType light, GraphicsLightPool &pool,
                                             const VkDeviceSize requiredMem)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindGraphicsLightRange");
    bool resized = false;
    GraphicsRange *range = findGraphicsRange<D, GraphicsRange>(
        pool, requiredMem,
        [&] {
            return createGraphicsLightBuffer<D>(
                light, computeNewInstanceCount(getLightSize<D>(light), pool.Buffer, requiredMem));
        },
        &resized);
    if (resized)
        updateLightDescriptorSets<D>(light);
    return range;
}

template <Dimension D>
GraphicsRange *findGraphicsVertexRange(GraphicsPool &pool, const VkDeviceSize requiredMem, VKit::Queue *transfer,
                                       TKit::StackArray<Task> &tasks)
{
    return findGraphicsRange<D, GraphicsRange>(
        pool, requiredMem,
        [&] {
            return createGraphicsVertexBuffer<D>(
                computeNewInstanceCount(sizeof(DynamicVertex<D>), pool.Buffer, requiredMem));
        },
        nullptr, &tasks, transfer);
}
template <Dimension D>
GraphicsRange *findGraphicsIndexRange(GraphicsPool &pool, const VkDeviceSize requiredMem, VKit::Queue *transfer,
                                      TKit::StackArray<Task> &tasks)
{
    return findGraphicsRange<D, GraphicsRange>(
        pool, requiredMem,
        [&] { return createGraphicsIndexBuffer<D>(computeNewInstanceCount(sizeof(Index), pool.Buffer, requiredMem)); },
        nullptr, &tasks, transfer);
}

static VkBufferMemoryBarrier2KHR createAcquireBarrier(const VkBuffer deviceLocalBuffer, const VkDeviceSize offset,
                                                      const VkDeviceSize size)
{
    const u32 qsrc = Execution::GetFamilyIndex(VKit::Queue_Transfer);
    const u32 qdst = Execution::GetFamilyIndex(VKit::Queue_Graphics);
    const bool needsTransfer = qsrc != qdst;

    VkBufferMemoryBarrier2KHR barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
    barrier.srcAccessMask = needsTransfer ? VK_ACCESS_2_NONE_KHR : VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR;
    barrier.srcStageMask = needsTransfer ? VK_PIPELINE_STAGE_2_NONE_KHR : VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;

    barrier.srcQueueFamilyIndex = needsTransfer ? qsrc : VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = needsTransfer ? qdst : VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = deviceLocalBuffer;
    barrier.offset = offset;
    barrier.size = size;

    return barrier;
}
static VkBufferMemoryBarrier2KHR createReleaseBarrier(const VkBuffer deviceLocalBuffer, const VkDeviceSize offset,
                                                      const VkDeviceSize size)
{
    const u32 qsrc = Execution::GetFamilyIndex(VKit::Queue_Transfer);
    const u32 qdst = Execution::GetFamilyIndex(VKit::Queue_Graphics);
    TKIT_ASSERT(qsrc != qdst, "[ONYX][RENDERER] Cannot create a release barrier if the graphics and transfer Execution "
                              "belong to the same family");

    VkBufferMemoryBarrier2KHR barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_2_NONE_KHR;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
    barrier.srcQueueFamilyIndex = qsrc;
    barrier.dstQueueFamilyIndex = qdst;
    barrier.buffer = deviceLocalBuffer;
    barrier.offset = offset;
    barrier.size = size;

    return barrier;
}

static ResourceType getResourceType(const Geometry geo)
{
    switch (geo)
    {
    case Geometry_Static:
        return Resource_StaticMesh;
    case Geometry_Parametric:
        return Resource_ParametricMesh;
    case Geometry_Glyph:
        return Resource_GlyphMesh;
    case Geometry_Dynamic:
        return Resource_DynamicMesh;
    case Geometry_Circle:
        return Resource_MeshPoolCount;
    default:
        TKIT_FATAL("[ONYX][RENDERER] The geometry '{}' does not have a resource type associated", ToString(geo));
        return Resource_Count;
    }
}

static u32 findAvailableOcclusionMap()
{
    ShadowData<D2> &sdata = s_RendererData2->Shadows;
    const u32 size = sdata.OcclusionMaps.GetSize();
    for (u32 i = 0; i < size; ++i)
        if (!sdata.OcclusionMaps[i].Tracker.InUse())
            return i;

    TextureMap &map = sdata.OcclusionMaps.Append();
    map.Image = ONYX_CHECK_VKIT_RESULT(
        VKit::DeviceImage::Builder(
            GetDevice(), GetVulkanAllocator(), VkExtent2D{sdata.OcclusionResolution, sdata.OcclusionResolution},
            sdata.OcclusionFormat, VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Storage)
            .AddImageView()
            .Build());

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_VKIT_RESULT(map.Image.SetName(TKit::StackString::Format("onyx-occlusion-map-{}", size).CString()));
        ONYX_CHECK_VKIT_RESULT(
            map.Image.SetViewNames(TKit::StackString::Format("onyx-occlusion-map-view-{}", size).CString()));
    }

    VKit::DescriptorSet::Writer writer{GetDevice(), &Descriptors::GetDescriptorLayout(StandalonePass_RayMarch)};
    VkDescriptorImageInfo info{};
    info.imageView = map.Image.GetView();
    info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    writer.WriteImage(ONYX_OCCLUSION_MAP_BINDING_POINT, info, size);
    writer.Overwrite(sdata.RayMarchSet);
    return size;
}

// these two are here bc 2D cpu shadow map textures are NOT shared, but they are in the descriptor array. so sometimes
// we have to hop in and out of desc index or pool index space
template <Dimension D> u32 computeShadowMapDescriptorIndex(const LightType ltype, const u32 idx)
{
    if constexpr (D == D3)
        return idx;
    else
        return idx + ltype * ONYX_MAX_TEXTURE_MAPS;
}
template <Dimension D> u32 computeShadowMapPoolIndex(const LightType ltype, const u32 idx)
{
    if constexpr (D == D3)
        return idx;
    else
        return idx - ltype * ONYX_MAX_TEXTURE_MAPS;
}

template <Dimension D>
static Range findAvailableTextureMapRange(const LightType light, const VkFormat format, const u32 count)
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;
    const u32 resolution = sdata.ShadowResolutions[light];
    TextureMapArray &maps = sdata.ShadowMaps[light];

    Range range{};
    for (u32 i = 0; i < maps.GetSize(); ++i)
    {
        const TextureMap &map = maps[i];
        ++range.Count;
        if (map.Tracker.InUse())
        {
            range.Offset = i + 1;
            range.Count = 0;
        }
        if (range.Count == count)
            return range;
    }

    VKit::DeviceImageFlags flags = VKit::DeviceImageFlag_Sampled;
    if constexpr (D == D3)
        flags |= VKit::DeviceImageFlag_DepthAttachment;
    else
    {
        flags |= VKit::DeviceImageFlag_Storage;
        if (sdata.UsesFallback)
            flags |= VKit::DeviceImageFlag_Color;
        else
            flags |= VKit::DeviceImageFlag_Depth;
    }

    for (u32 i = range.Count; i < count; ++i)
    {
        TextureMap &map = maps.Append();
        VKit::DeviceImage::Builder builder{GetDevice(), GetVulkanAllocator(),
                                           VkExtent2D{resolution, D == D3 ? resolution : 1}, format, flags};
        if constexpr (D == D2)
        {
            builder.SetImageType(VK_IMAGE_TYPE_1D);
            builder.AddImageView();
        }
        else
        {
            constexpr TKit::FixedArray<u32, Light_Count> lcounts = {6, ONYX_MAX_CASCADES, 1};
            const u32 lcount = lcounts[light];

            builder.SetArrayLayers(lcount);
            for (u32 j = 0; j < lcount; ++j)
            {
                VkImageSubresourceRange rg{};
                rg.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                rg.baseArrayLayer = j;
                rg.layerCount = 1;
                rg.levelCount = 1;
                builder.AddImageView(rg);
            }
            if (light == Light_Point)
            {
                builder.SetFlags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
                builder.AddImageView(VK_IMAGE_VIEW_TYPE_CUBE);
            }
            else if (light == Light_Directional)
                builder.AddImageView(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
            else
                builder.AddImageView();
        }

        map.Image = ONYX_CHECK_VKIT_RESULT(builder.Build());
        if (IsDebugUtilsEnabled())
        {
            ONYX_CHECK_VKIT_RESULT(map.Image.SetName(
                TKit::StackString::Format("onyx-texture-map-{}D-'{}'-{}", u8(D), ToString(light), range.Offset + i)
                    .CString()));
            ONYX_CHECK_VKIT_RESULT(map.Image.SetViewNames(
                TKit::StackString::Format("onyx-texture-map-view-{}D-'{}'-{}", u8(D), ToString(light), range.Offset + i)
                    .CString()));
        }

        VkDescriptorImageInfo info{};
        info.imageView = map.Image.GetViews().GetBack();
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        const u32 dstElement = computeShadowMapDescriptorIndex<D>(light, range.Offset + i);
        if constexpr (D == D2)
        {
            BindImage<D>(ONYX_SHADOW_MAPS_BINDING_POINT, info, RenderPass_Shaded, dstElement);
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VKit::DescriptorSet::Writer writer{GetDevice(), &Descriptors::GetDescriptorLayout(StandalonePass_RayMarch)};
            writer.WriteImage(ONYX_RAY_MARCH_MAP_BINDING_POINT, info, dstElement);
            writer.Overwrite(getRendererData<D2>().Shadows.RayMarchSet);
        }
        else
        {
            constexpr TKit::FixedArray<u32, Light_Count> bindings = {
                ONYX_POINT_MAPS_BINDING_POINT, ONYX_DIRECTIONAL_MAPS_BINDING_POINT, ONYX_SPOT_MAPS_BINDING_POINT};
            BindImage<D>(bindings[light], info, RenderPass_Shaded, dstElement);
        }
    }
    range.Count = count;
    return range;
}

template <Dimension D>
PointLightData<D> createLightData(const ViewMask vmask, const u32 shadowMapOffset,
                                  const PointLightParameters<D> &params)
{
    PointLightData<D> data;
    if constexpr (D == D2)
    {
        data.Direction = f32v2{Math::Cosine(params.Angle), Math::Sine(params.Angle)};
        data.Angle = params.Angle;
        data.Decay = params.Decay;
        data.Extent = params.Extent;
    }
    data.LightSize = params.LightSize;
    data.Position = params.Position;
    data.Intensity = params.Intensity;
    data.LightRadius = params.LightRadius;
    data.ShadowRadius = params.ShadowRadius;
    data.Color = params.Tint.ToLinear().Pack();
    data.ViewMask = vmask;
    data.ShadowMapOffset = shadowMapOffset;
    data.Flags = params.Flags;
    return data;
}

template <typename T> static T cascadeLerp(const T mn, const T mx, const f32 t, const f32 lambda)
{
    const T ln = Math::LinearLerp(mn, mx, t);
    const T lg = Math::LogLerp(mn, mx, t);
    return Math::LinearLerp(ln, lg, lambda);
}

static TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> createCascades(const f32v3 &dir, const u32 count,
                                                                       const f32 lambda, const f32 overlap,
                                                                       const FixedCascadeParameters &params)
{
    TKIT_ASSERT(count <= ONYX_MAX_CASCADES, "[ONYX] The maximum amount of cascades is {}", ONYX_MAX_CASCADES);
    TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> cascades;

    for (u32 i = 0; i < count; ++i)
    {
        CascadeData &c = cascades[i];
        const f32 t = f32(i + 1) / f32(count);

        const f32 width = cascadeLerp(params.MinSize, params.MaxSize, t, lambda);

        const f32m4 proj = Transform<D3>::Orthographic(-width, width, -width, width, params.Near, params.Far);
        const f32m4 view = Transform<D3>::LookTowards(params.ViewPosition, dir);
        c.ProjectionView = CreateTransformData<D3>(proj * view);

        c.InvSize = 0.5f * f32v2{proj[0][0], proj[1][1]};
        c.DepthRange = params.Far - params.Near;
        c.Split = width * (1.f + overlap);
    }
    return cascades;
}

static TKit::FixedArray<f32v4, 8> getCameraCorners(const f32m4 &pv)
{
    const f32m4 ipv = Math::Inverse(pv);
    TKit::FixedArray<f32v4, 8> corners;
    u32 idx = 0;
    for (u32 i = 0; i < 2; ++i)
        for (u32 j = 0; j < 2; ++j)
            for (u32 k = 0; k < 2; ++k)
            {
                const f32v4 norm = f32v4{2.f * i - 1.f, 2.f * j - 1.f, f32(k), 1.f};
                const f32v4 wpos = ipv * norm;
                corners[idx++] = wpos / wpos[3];
            }
    return corners;
}

static TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> createCascades(const f32v3 &dir, const RenderView<D3> *rview,
                                                                       const f32v3 &offset, const f32v2 &extent,
                                                                       const u32 count, const f32 zmul,
                                                                       const f32 lambda, const f32 overlap,
                                                                       u32 &enableFlags)
{
    const TKit::FixedArray<f32v4, 8> globalCorners = getCameraCorners(rview->GetProjectionView());

    TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> cascades;
    f32 cnear;
    f32 cfar;
    const Camera<D3> *camera = rview->GetCamera();
    const CameraMode mode = camera->Mode;
    if (mode == CameraMode_Perspective)
    {
        cnear = camera->PerspParameters.Near;
        cfar = camera->PerspParameters.Far;
    }
    else
    {
        cnear = camera->OrthoParameters.Near;
        cfar = camera->OrthoParameters.Far;
    }
    const f32 range = cfar - cnear;
    f32 split0 = cnear;
    enableFlags = 0;
    for (u32 i = 0; i < count; ++i)
    {
        CascadeData &c = cascades[i];

        const f32 t = f32(i + 1) / count;
        const f32 split1 = cascadeLerp(cnear, cfar, t, lambda);

        const f32 t0 = (split0 - cnear) / range;
        const f32 t1 = (split1 - cnear) / range;

        TKit::FixedArray<f32v4, 8> corners;
        for (u32 j = 0; j < 4; ++j)
        {
            corners[2 * j] = Math::LinearLerp(globalCorners[2 * j], globalCorners[2 * j + 1], t0);
            corners[2 * j + 1] = Math::LinearLerp(globalCorners[2 * j], globalCorners[2 * j + 1], t1);
        }

        f32v3 center{0.f};
        for (u32 j = 0; j < 8; ++j)
            center += f32v3{corners[j]};
        center /= 8.f;

        const f32m4 view = Transform<D3>::LookTowards(center, dir);
        f32v3 min{TKIT_F32_MAX};
        f32v3 max{TKIT_F32_MIN};
        for (const f32v4 &cr : corners)
        {
            const f32v3 lc = f32v3{view * cr};
            min = Math::Min(min, lc);
            max = Math::Max(max, lc);
        }

        const f32v2 ofview = f32v2{view * f32v4{offset, 1.f}};
        const f32v2 cmin = ofview - extent;
        const f32v2 cmax = ofview + extent;

        min[0] = Math::Max(min[0], cmin[0]);
        min[1] = Math::Max(min[1], cmin[1]);

        max[0] = Math::Min(max[0], cmax[0]);
        max[1] = Math::Min(max[1], cmax[1]);

        if (min[0] >= max[0] || min[1] >= max[1])
            continue;

        enableFlags |= 1U << i;

        if (min[2] < 0.f)
            min[2] *= zmul;
        else
            min[2] /= zmul;

        if (max[2] < 0.f)
            max[2] /= zmul;
        else
            max[2] *= zmul;

        const f32m4 proj = Transform<D3>::Orthographic(min, max);
        c.ProjectionView = CreateTransformData<D3>(proj * view);

        c.InvSize = 0.5f * f32v2{proj[0][0], proj[1][1]};
        c.DepthRange = max[2] - min[2];
        c.Split = split1 * (1.f + overlap);

        split0 = split1;
    }
    return cascades;
}

template <Dimension D>
DirectionalLightData<D> createLightData(const ViewMask vmask, const u32 shadowMapOffset,
                                        const DirectionalLightParameters<D> &params)
{
    DirectionalLightData<D> data;

    if constexpr (D == D3)
    {
        const f32v3 dir = Math::Normalize(params.Direction);
        const f32m2x3 orth = Transform<D3>::CreateOrthogonalBase(dir);

        data.Offset = params.Offset[0] * orth[0] + params.Offset[1] * orth[1];
        data.Extent = params.Extent;
        data.PlaneVec1 = orth[0];
        data.PlaneVec2 = orth[1];

        if (params.Flags & LightFlag_CastShadows)
        {
            const ShadowCascadeParameters &c = params.Cascades;
            TKIT_ASSERT(
                c.Count != 0,
                "[ONYX][RENDERER] If a directional light casts shadows, its cascade count must be greater than 0");

            data.CascadeEnable = (1U << ONYX_MAX_CASCADES) - 1;
            const TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> cascades =
                c.View ? createCascades(dir, c.View, data.Offset, params.Extent, c.Count, c.FittedParameters.ZMul,
                                        c.Lambda, c.Overlap, data.CascadeEnable)
                       : createCascades(dir, c.Count, c.Lambda, c.Overlap, c.FixedParameters);

            data.Cascades = cascades;
            data.CascadeCount = c.Count;
        }
        data.Direction = dir;
    }
    else
    {
        const f32v2 dir = f32v2{Math::Cosine(params.Angle), Math::Sine(params.Angle)};
        const f32v2 normal = f32v2{-dir[1], dir[0]};
        const f32v2 pos = params.ShadowOffset * dir + params.LightOffset * normal;

        const f32 le = params.LightExtent;
        const f32 se = params.ShadowExtent;

        const f32m3 orth = Transform<D2>::Orthographic(-se, se, -le, le);
        const f32m3 view = Transform<D2>::ComputeInverseTransform(pos, f32v2{1.f}, params.Angle);

        data.ProjectionView = CreateTransformData<D2>(orth * view);
        data.Direction = dir;
        data.LightOffset = params.LightOffset;
        data.LightExtent = le;
        data.ShadowExtent = se;
    }

    data.Decay = params.Decay;
    data.TanAngleSize = Math::Tangent(params.AngleSize);
    data.Intensity = params.Intensity;
    data.Color = params.Tint.ToLinear().Pack();
    data.ViewMask = vmask;
    data.ShadowMapOffset = shadowMapOffset;
    data.Flags = params.Flags;
    return data;
}

SpotLightData createLightData(const ViewMask vmask, const u32 shadowMapOffset, const SpotLightParameters &params)
{
    SpotLightData data;
    data.Direction = Math::Normalize(params.Direction);

    const f32 near = ONYX_SPOT_LIGHT_NEAR_RANGE_FACTOR * params.ShadowRange;
    const f32 far = params.ShadowRange;

    data.Near = near;
    data.Far = far;

    const f32m4 proj = Transform<D3>::Perspective(params.FieldOfView, near, far);
    const f32m4 view = Transform<D3>::LookTowards(params.Position, data.Direction);

    data.ProjectionView = proj * view;
    data.ShadowInvSize = 0.5f * Math::Absolute(f32v2{proj[0][0], proj[1][1]});

    data.Position = params.Position;
    data.LightSize = params.LightSize;
    data.CosHalfPov = Math::Cosine(0.5f * params.FieldOfView);
    data.LightRange = params.LightRange;
    data.Decay = params.Decay;
    data.Intensity = params.Intensity;
    data.Color = params.Tint.ToLinear().Pack();
    data.ShadowMapOffset = shadowMapOffset;
    data.ViewMask = vmask;
    data.Flags = params.Flags;
    return data;
}

template <Dimension D>
static void transfer(VKit::Queue *transfer, const VkCommandBuffer command, TransferSubmitInfo &info,
                     TKit::StackArray<VkBufferMemoryBarrier2KHR> *release, const u64 transferFlightValue,
                     const u32 maxLights)
{

    RendererData<D> &rdata = getRendererData<D>();
    TKit::TierArray<ContextInfo<D>> &contexts = rdata.Contexts;
    if (contexts.IsEmpty())
        return;

    TKit::StackArray<u32> dirtyContexts{};
    dirtyContexts.Reserve(rdata.Contexts.GetSize());

    TKit::FixedArray<TKit::StackArray<ViewMask>, LightTypeCount<D>> lightViews{};
    for (u32 i = 0; i < LightTypeCount<D>; ++i)
        lightViews[i].Reserve(maxLights);

    using LightUpdateFlags = u8;
    enum LightUpdateFlagBit : LightUpdateFlags
    {
        LightUpdateFlag_Point = 1U << 0,
        LightUpdateFlag_Directional = 1U << 1,
        LightUpdateFlag_Spot = 1U << 2,
    };

    LightUpdateFlags toUpdate = 0;

    LightData<D> &ldata = rdata.Lights;
    ldata.Instances.ClearLights();

    ShadowData<D> &sdata = rdata.Shadows;

    InstanceDataGrouping<MeshInstanceGrouping<LocalResourceRegistry>> meshRegistry{};
    InstanceDataGrouping<LocalResourceRegistry> dynMeshRegistry{};
    for (u32 i = 0; i < contexts.GetSize(); ++i)
    {
        ContextInfo<D> &cinfo = contexts[i];
        RenderContext<D> *ctx = cinfo.Context;

        const ViewMask vmask = ctx->GetViewMask();
        if (!vmask)
            continue;

        const bool isDirty = cinfo.IsDirty();
        if (isDirty)
        {
            dirtyContexts.Append(i);
            const InstanceDataGrouping<InstanceDataArrays *> &idata = ctx->GetInstanceData();
            ForEachResourceGroup<D>([&](const u32 bpass, const u32 rmode, const u32 mtype, const u32 pid) {
                InstanceResourceGroup &group = idata[bpass][rmode]->Meshes[mtype][pid];

                for (const u32 rid : group.Registry.ResourceIds)
                    meshRegistry[bpass][rmode][mtype][pid].RegisterResourceId(rid);
            });
            ForEachDynamicMeshResourceGroup<D>([&](const u32 bpass, const u32 rmode) {
                InstanceResourceGroup &group = idata[bpass][rmode]->DynamicMeshes;
                for (const u32 rid : group.Registry.ResourceIds)
                    dynMeshRegistry[bpass][rmode].RegisterResourceId(rid);
            });
            cinfo.Generation = ctx->GetGeneration();
        }
        const auto gatherLights =
            [&]<typename LightParams>(const LightType ltype, const TKit::TierArray<LightParams> &src,
                                      TKit::TierArray<LightParams> &dst, const LightUpdateFlags update) {
                TKit::StackArray<ViewMask> &vmasks = lightViews[ltype];
                LightFlags flags = 0;

                for (u32 i = 0; i < src.GetSize(); ++i)
                {
                    const LightParams &params = src[i];
                    flags |= params.Flags;

                    dst.Append(params);
                    vmasks.Append(vmask);
                }
                toUpdate |= (update * isDirty * !src.IsEmpty()) |
                            ((flags & LightFlag_CastShadows) *
                             (LightUpdateFlag_Point | LightUpdateFlag_Directional | LightUpdateFlag_Spot));

                sdata.DirtyShadowViews |= vmask * (flags & LightFlag_CastShadows);
            };

        gatherLights(Light_Point, ctx->GetPointLightData(), ldata.Instances.Points.Lights, LightUpdateFlag_Point);
        gatherLights(Light_Directional, ctx->GetDirectionalLightData(), ldata.Instances.Directionals.Lights,
                     LightUpdateFlag_Directional);
        if constexpr (D == D3)
            gatherLights(Light_Spot, ctx->GetSpotLightData(), ldata.Instances.Spots.Lights, LightUpdateFlag_Spot);
    }

    const auto checkLightCountChange =
        [&]<typename LightParams>(const LightType ltype, const LightUpdateFlags flags,
                                  ContextLights<LightParams> &clights) -> LightUpdateFlags {
        const u32 ncount = clights.Lights.GetSize();
        u32 &count = ldata.Arenas[ltype].LightCount;
        if (ncount != count)
        {
            count = ncount;
            clights.Data.Clear();
            return flags;
        }
        return 0;
    };
    toUpdate |= checkLightCountChange(Light_Point, LightUpdateFlag_Point, ldata.Instances.Points);
    toUpdate |= checkLightCountChange(Light_Directional, LightUpdateFlag_Directional, ldata.Instances.Directionals);
    if constexpr (D == D3)
        toUpdate |= checkLightCountChange(Light_Spot, LightUpdateFlag_Spot, ldata.Instances.Spots);

    const auto copyLightRanges = [&]<typename LightParams>(const LightType ltype, ContextLights<LightParams> &clights) {
        info.Command = command;
        clights.Data.Clear();

        LightArena &arena = ldata.Arenas[ltype];
        TransferLightPool &tpool = arena.Transfer;
        GraphicsLightPool &gpool = arena.Graphics;

        using LightData = typename LightParams::InstanceData;
        const VkDeviceSize requiredMem = sizeof(LightData) * clights.Lights.GetSize();

        const TKit::StackArray<ViewMask> &vmasks = lightViews[ltype];
        for (u32 i = 0; i < clights.Lights.GetSize(); ++i)
        {
            const LightParams &light = clights.Lights[i];

            const ViewMask vm = vmasks[i];
            const u32 count = std::popcount(vm);
            u32 shadowOffset = TKIT_U32_MAX;

            if (light.Flags & LightFlag_CastShadows)
            {
                const Range range = findAvailableTextureMapRange<D>(ltype, sdata.ShadowFormat, count);
                // this is a small hack: shadow maps wont ever be used by the transfer queue, but this way we prevent
                // lights from taking the same shadow map this run
                for (u32 j = 0; j < range.Count; ++j)
                    sdata.ShadowMaps[ltype][j + range.Offset].Tracker.MarkInUse(transfer, transferFlightValue);

                shadowOffset = computeShadowMapDescriptorIndex<D>(ltype, range.Offset);
            }
            clights.Data.Append(createLightData(vm, shadowOffset, light));
        }

        TransferLightRange *trange = findTransferLightRange<D>(ltype, tpool, requiredMem);
        trange->Tracker.MarkInUse(transfer, transferFlightValue);
        tpool.Buffer.Write(clights.Data.GetData(), {.srcOffset = 0, .dstOffset = trange->Offset, .size = trange->Size});

        GraphicsRange *grange = findGraphicsLightRange<D>(ltype, gpool, requiredMem);
        grange->TransferTracker.MarkInUse(transfer, transferFlightValue);
        grange->GraphicsTracker = {};
        grange->Generation = ++arena.ActiveGeneration;

        VkBufferCopy2KHR copy{};
        copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
        copy.pNext = nullptr;
        copy.srcOffset = trange->Offset;
        copy.dstOffset = grange->Offset;
        copy.size = requiredMem;

        gpool.Buffer.CopyFromBuffer2(command, tpool.Buffer, copy);
        rdata.AcquireBarriers.Append(createAcquireBarrier(gpool.Buffer, grange->Offset, requiredMem));
        if (release)
            release->Append(createReleaseBarrier(gpool.Buffer, grange->Offset, requiredMem));
    };

    if ((toUpdate & LightUpdateFlag_Point) && !ldata.Instances.Points.Lights.IsEmpty())
        copyLightRanges(Light_Point, ldata.Instances.Points);
    if ((toUpdate & LightUpdateFlag_Directional) && !ldata.Instances.Directionals.Lights.IsEmpty())
        copyLightRanges(Light_Directional, ldata.Instances.Directionals);

    if constexpr (D == D3)
        if ((toUpdate & LightUpdateFlag_Spot) && !ldata.Instances.Spots.Lights.IsEmpty())
            copyLightRanges(Light_Spot, ldata.Instances.Spots);

    if (dirtyContexts.IsEmpty())
        return;

    u32 sindex = 0;

    // this struct is used to create the appropiate memory barriers in the copies ranges. takes a buffer so this works
    // for both instance and v/i buffers
    struct RangePair
    {
        VKit::DeviceBuffer *GraphicsBuffer;
        VkDeviceSize GraphicsOffset;
        VkDeviceSize RequiredMemory;
    };

    // the actual copy commands for each data type (instance, vertex and index). can belong to any buffer (in the case
    // of vertex/index there is only one buffer for each so its easy enough)
    struct BufferCopies
    {
        BufferCopies(const u32 incount, const u32 dynCount)
        {
            Instance.Reserve(incount);
            Vertex.Reserve(dynCount);
            Index.Reserve(dynCount);
        }
        TKit::StackArray<VkBufferCopy2KHR> Instance{};
        TKit::StackArray<VkBufferCopy2KHR> Vertex{};
        TKit::StackArray<VkBufferCopy2KHR> Index{};
    };

    // a "copy struct slice" to identify what chunks of the global copy array belongs to which instance buffer, as there
    // are many (one pair transfer-graphics per geometry type). this is only needed for instances. for v/i, there is
    // only one buffer so all copies go to those
    struct BufferCopySlice
    {
        VKit::DeviceBuffer *Transfer;
        VKit::DeviceBuffer *Graphics;
        u32 Offset;
        u32 Size;
    };

    const u32 bcount = Resources::GetDistinctBatchDrawCount<D>();
    const u32 dynCount = Resources::GetDynamicMeshCount<D>();
    BufferCopies copies{bcount, dynCount};

    TKit::StackArray<RangePair> ranges{};
    ranges.Reserve(RenderMode_Count * bcount);

    TKit::StackArray<BufferCopySlice> instanceCopyCmds{};
    instanceCopyCmds.Reserve(RenderMode_Count * u32(Geometry_Count));

    TKit::StackArray<ContextInstanceRange> contextRanges{};
    contextRanges.Reserve(dirtyContexts.GetSize());

    TKit::ITaskManager *tm = GetTaskManager();

    TKit::StackArray<Task> tasks{};
    tasks.Reserve(dirtyContexts.GetSize() * bcount);

    const auto finishTasks = [&] {
        for (const Task &task : tasks)
            tm->WaitUntilFinished(task);
    };

    const auto findInstanceRanges = [&](const u32 rmode, const u32 bpass, const u32 geo, const Resource handle,
                                        const auto getInstanceData) {
        u64 vgen = 0;
        u64 igen = 0;
        if (geo == Geometry_Dynamic)
        {
            const DynamicMeshData<D> *data = Resources::GetDynamicMeshData<D>(handle);
            if (data->Vertices.IsEmpty() || data->Indices.IsEmpty())
                return;

            Arena &varena = rdata.Geometry.VertexArena;
            Arena &iarena = rdata.Geometry.IndexArena;

            vgen = ++varena.LatestGeneration;
            igen = ++iarena.LatestGeneration;

            const auto processTransferArena = [&](TransferPool &pool, const auto &data, const bool vertices) {
                TransferRange *trng = vertices ? findTransferVertexRange<D>(pool, data.GetBytes(), tasks)
                                               : findTransferIndexRange<D>(pool, data.GetBytes(), tasks);
                trng->Tracker.MarkInUse(transfer, transferFlightValue);
                const auto vcopy = [&, trng = *trng] {
                    TKIT_PROFILE_NSCOPE("Onyx::Renderer::HostCopy");
                    pool.Buffer.Write(data.GetData(), {.srcOffset = 0, .dstOffset = trng.Offset, .size = trng.Size});
                };
                Task &task = tasks.Append(vcopy);
                sindex = tm->SubmitTask(&task, sindex);
                return trng;
            };

            const TransferRange *vtrng = processTransferArena(varena.Transfer, data->Vertices, true);
            const TransferRange *itrng = processTransferArena(iarena.Transfer, data->Indices, false);

            const auto processGraphicsArena = [&](GraphicsPool &pool, const VkDeviceSize reqMem,
                                                  const TransferRange *trng, const bool vertices) {
                GraphicsRange *grng = vertices ? findGraphicsVertexRange<D>(pool, reqMem, transfer, tasks)
                                               : findGraphicsIndexRange<D>(pool, reqMem, transfer, tasks);
                grng->TransferTracker.MarkInUse(transfer, transferFlightValue);

                VkBufferCopy2KHR &copy = vertices ? copies.Vertex.Append() : copies.Index.Append();
                copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
                copy.pNext = nullptr;
                copy.srcOffset = trng->Offset;
                copy.dstOffset = grng->Offset;
                copy.size = reqMem;
                ranges.Append(&pool.Buffer, grng->Offset, reqMem);
                return grng;
            };

            GraphicsRange *vgrng = processGraphicsArena(varena.Graphics, data->Vertices.GetBytes(), vtrng, true);
            GraphicsRange *igrng = processGraphicsArena(iarena.Graphics, data->Indices.GetBytes(), itrng, false);

            vgrng->Generation = vgen;
            igrng->Generation = igen;
        }

        TransferInstancePool &tpool = rdata.Geometry.Arenas[geo].Transfer;
        GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;

        contextRanges.Clear();
        VkDeviceSize requiredMem = 0;
        ViewMask viewMask = 0;
        for (const u32 idx : dirtyContexts)
        {
            const RenderContext<D> *ctx = rdata.Contexts[idx].Context;
            const auto &idata = getInstanceData(ctx);
            if (idata.Instances == 0)
                continue;

            ContextInstanceRange &crange = contextRanges.Append();
            crange.ContextIndex = idx;
            crange.Offset = requiredMem;
            crange.Size = idata.Instances * idata.InstanceSize;
            crange.Generation = ctx->GetGeneration();

            const ViewMask vm = ctx->GetViewMask();
            viewMask |= vm;
            crange.ViewMask = vm;

            requiredMem += crange.Size;
        }
        if (requiredMem == 0)
            return;

        TransferInstanceRange *trange = findTransferInstanceRange<D>(Geometry(geo), tpool, requiredMem, tasks);
        trange->Tracker.MarkInUse(transfer, transferFlightValue);

        for (const ContextInstanceRange &crange : contextRanges)
        {
            const RenderContext<D> *ctx = contexts[crange.ContextIndex].Context;

            const auto &idata = getInstanceData(ctx);
            const auto copy = [&, crange, trange = *trange] {
                TKIT_PROFILE_NSCOPE("Onyx::Renderer::HostCopy");
                tpool.Buffer.Write(idata.Data.GetData(),
                                   {.srcOffset = 0, .dstOffset = trange.Offset + crange.Offset, .size = crange.Size});
            };

            Task &task = tasks.Append(copy);
            sindex = tm->SubmitTask(&task, sindex);
        }

        GraphicsInstanceRange *grange =
            findGraphicsInstanceRange<D>(Geometry(geo), gpool, requiredMem, transfer, tasks);
        grange->Blend = BlendPass(bpass);
        grange->MeshHandle = handle;
        grange->ContextRanges = contextRanges;
        grange->ViewMask = viewMask;
        grange->RenderFlags = GetRenderModeFlags(RenderMode(rmode));
        grange->TransferTracker.MarkInUse(transfer, transferFlightValue);
        grange->GraphicsTracker = {};
        if (geo == Geometry_Dynamic)
        {
            grange->ActiveVertexGeneration = vgen;
            grange->ActiveIndexGeneration = igen;
        }

        VkBufferCopy2KHR &copy = copies.Instance.Append();
        copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
        copy.pNext = nullptr;
        copy.srcOffset = trange->Offset;
        copy.dstOffset = grange->Offset;
        copy.size = requiredMem;

        ranges.Append(&gpool.Buffer, grange->Offset, requiredMem);
    };

    const auto gatherInstanceRanges = [&](const u32 rmode, const u32 geo) {
        TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindRanges");
        TransferInstancePool &tpool = rdata.Geometry.Arenas[geo].Transfer;
        GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;

        BufferCopySlice copyCmdSlice{};
        copyCmdSlice.Transfer = &tpool.Buffer;
        copyCmdSlice.Graphics = &gpool.Buffer;
        copyCmdSlice.Offset = copies.Instance.GetSize();

        if (geo == Geometry_Circle)
            for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
                findInstanceRanges(rmode, bpass, Geometry_Circle, NullHandle,
                                   [rmode, bpass](const RenderContext<D> *ctx) -> const auto & {
                                       return ctx->GetInstanceData()[BlendPass(bpass)][rmode]->Circles;
                                   });
        else if (geo == Geometry_Dynamic)
            for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
            {
                const TKit::TierArray<Resource> &resources = dynMeshRegistry[bpass][rmode].ResourceIds;
                for (const u32 rid : resources)
                    findInstanceRanges(rmode, bpass, geo, CreateResourceHandle(Resource_DynamicMesh, rid),
                                       [rmode, bpass, rid](const RenderContext<D> *ctx) -> const auto & {
                                           return ctx->GetInstanceData()[bpass][rmode]->DynamicMeshes.Instances[rid];
                                       });
            }
        else
        {
            const ResourceType rtype = getResourceType(Geometry(geo));
            const TKit::Span<const u32> poolIds = Resources::GetResourcePoolIds<D>(rtype);
            for (const u32 pid : poolIds)
            {
                for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
                {
                    const TKit::TierArray<Resource> &resources = meshRegistry[bpass][rmode][rtype][pid].ResourceIds;
                    for (const u32 rid : resources)
                        findInstanceRanges(
                            rmode, bpass, geo, CreateResourceHandle(rtype, rid, pid),
                            [rtype, rmode, bpass, pid, rid](const RenderContext<D> *ctx) -> const auto & {
                                return ctx->GetInstanceData()[bpass][rmode]->Meshes[rtype][pid].Instances[rid];
                            });
                }
            }
        }

        copyCmdSlice.Size = copies.Instance.GetSize() - copyCmdSlice.Offset;
        if (copyCmdSlice.Size != 0)
            instanceCopyCmds.Append(copyCmdSlice);
    };

    for (u32 rmode = 0; rmode < RenderMode_Count; ++rmode)
        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            gatherInstanceRanges(rmode, geo);

    for (const RangePair &range : ranges)
    {
        rdata.AcquireBarriers.Append(
            createAcquireBarrier(*range.GraphicsBuffer, range.GraphicsOffset, range.RequiredMemory));

        if (release)
            release->Append(createReleaseBarrier(*range.GraphicsBuffer, range.GraphicsOffset, range.RequiredMemory));
    }
    for (const BufferCopySlice &cmdSlice : instanceCopyCmds)
    {
        const TKit::Span<const VkBufferCopy2KHR> cpspan{copies.Instance.GetData() + cmdSlice.Offset, cmdSlice.Size};
        cmdSlice.Graphics->CopyFromBuffer2(command, *cmdSlice.Transfer, cpspan);
    }
    if (!copies.Vertex.IsEmpty())
        rdata.Geometry.VertexArena.Graphics.Buffer.CopyFromBuffer2(command, rdata.Geometry.VertexArena.Transfer.Buffer,
                                                                   copies.Vertex);

    if (!copies.Index.IsEmpty())
        rdata.Geometry.IndexArena.Graphics.Buffer.CopyFromBuffer2(command, rdata.Geometry.IndexArena.Transfer.Buffer,
                                                                  copies.Index);

    info.Command = command;
    finishTasks();
}

TransferSubmitInfo Transfer(VKit::Queue *tqueue, const VkCommandBuffer command, const u32 maxLights,
                            const u32 maxReleaseBarriers)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Transfer");
    TransferSubmitInfo submitInfo{};
    const bool separate = Execution::IsSeparateTransferMode();
    TKit::StackArray<VkBufferMemoryBarrier2KHR> release{};
    if (separate)
        release.Reserve(maxReleaseBarriers);

    const u64 transferFlight = tqueue->NextTimelineValue();

    transfer<D2>(tqueue, command, submitInfo, separate ? &release : nullptr, transferFlight, maxLights);
    transfer<D3>(tqueue, command, submitInfo, separate ? &release : nullptr, transferFlight, maxLights);

#ifdef TKIT_ENABLE_ASSERTS
    validateRanges<D2>();
    validateRanges<D3>();
#endif

    if (separate)
    {
        VkDependencyInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        info.bufferMemoryBarrierCount = release.GetSize();
        info.pBufferMemoryBarriers = release.GetData();
        info.dependencyFlags = 0;
        const auto table = GetDeviceTable();
        table->CmdPipelineBarrier2KHR(command, &info);
    }
    if (submitInfo)
    {
        VkSemaphoreSubmitInfoKHR semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
        semInfo.semaphore = tqueue->GetTimelineSempahore();
        semInfo.value = transferFlight;
        semInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
        submitInfo.SignalSemaphore = semInfo;
        submitInfo.InFlightValue = transferFlight;
    }

    s_SyncPointCount = 0;
    for (const InstanceArena &arena : getRendererData<D2>().Geometry.Arenas)
        s_SyncPointCount += arena.Graphics.Ranges.GetSize();
    for (const InstanceArena &arena : getRendererData<D3>().Geometry.Arenas)
        s_SyncPointCount += arena.Graphics.Ranges.GetSize();
    return submitInfo;
}

void SubmitTransfer(VKit::Queue *transfer, CommandPool *pool, TKit::Span<const TransferSubmitInfo> info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::SubmitTransfer");
    TKit::StackArray<VkSubmitInfo2KHR> submits{};
    submits.Reserve(info.GetSize());

    TKit::StackArray<VkCommandBufferSubmitInfoKHR> cmds{};
    cmds.Reserve(info.GetSize());

    u64 maxFlight = 0;
    for (const TransferSubmitInfo &inf : info)
    {
        TKIT_ASSERT(inf.Command,
                    "[ONYX][RENDERER] A submission must have a valid transfer command buffer to be submitted");
        if (inf.InFlightValue > maxFlight)
            maxFlight = inf.InFlightValue;

        VkSubmitInfo2KHR &sinfo = submits.Append();
        sinfo = {};
        sinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;

        sinfo.signalSemaphoreInfoCount = 1;
        sinfo.pSignalSemaphoreInfos = &inf.SignalSemaphore;

        VkCommandBufferSubmitInfoKHR &cmd = cmds.Append();
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
        cmd.pNext = nullptr;
        cmd.commandBuffer = inf.Command;
        cmd.deviceMask = 0;

        sinfo.commandBufferInfoCount = 1;
        sinfo.pCommandBufferInfos = &cmd;
        sinfo.flags = 0;
    }

    Execution::MarkInUse(pool, transfer, maxFlight);
    ONYX_CHECK_VKIT_RESULT(transfer->Submit2(submits));
}

template <Dimension D> void gatherAcquireBarriers(TKit::StackArray<VkBufferMemoryBarrier2KHR> &barriers)
{
    RendererData<D> &rdata = getRendererData<D>();
    barriers.Insert(barriers.end(), rdata.AcquireBarriers.begin(), rdata.AcquireBarriers.end());
    rdata.AcquireBarriers.Clear();
}

template <Dimension D> void prepareRender()
{
    RendererData<D> &rdata = getRendererData<D>();
    LightData<D> &ldata = rdata.Lights;
    for (u32 i = 0; i < ldata.Arenas.GetSize(); ++i)
    {
        ldata.Ranges[i] = Range{};

        LightArena &arena = ldata.Arenas[i];
        arena.ActiveRange = nullptr;
        if (arena.LightCount != 0)
            for (GraphicsRange &grange : arena.Graphics.Ranges)
                if (grange.Generation == arena.ActiveGeneration)
                {
                    arena.ActiveRange = &grange;
                    const u32 isize = getLightSize<D>(LightType(i));
                    TKIT_ASSERT(arena.LightCount == grange.Size / isize,
                                "[ONYX][RENDERER] Light count mismatch between cached arena value and graphics range");
                    ldata.Ranges[i].Count = arena.LightCount;
                    ldata.Ranges[i].Offset = grange.Offset / isize;
                    break;
                }
    }

    for (GraphicsInstanceRange &grange : rdata.Geometry.Arenas[Geometry_Dynamic].Graphics.Ranges)
    {
        grange.ActiveVertexRange = nullptr;
        grange.ActiveIndexRange = nullptr;
        for (GraphicsRange &vgrange : rdata.Geometry.VertexArena.Graphics.Ranges)
            if (grange.ActiveVertexGeneration == vgrange.Generation)
            {
                grange.ActiveVertexRange = &vgrange;
                break;
            }
        for (GraphicsRange &igrange : rdata.Geometry.IndexArena.Graphics.Ranges)
            if (grange.ActiveIndexGeneration == igrange.Generation)
            {
                grange.ActiveIndexRange = &igrange;
                break;
            }
        // TKIT_ASSERT(grange.ActiveVertexRange,
        //             "[ONYX][RENDERING] Graphics instance range failed to find a vertex graphics range");
        // TKIT_ASSERT(grange.ActiveIndexRange,
        //             "[ONYX][RENDERING] Graphics instance range failed to find an index graphics range");
    }
}

void PrepareRender()
{
    prepareRender<D2>();
    prepareRender<D3>();
}
void ApplyAcquireBarriers(const VkCommandBuffer cmd)
{
    TKit::StackArray<VkBufferMemoryBarrier2KHR> barriers{};
    barriers.Reserve(s_RendererData2->AcquireBarriers.GetSize() + s_RendererData3->AcquireBarriers.GetSize());
    gatherAcquireBarriers<D2>(barriers);
    gatherAcquireBarriers<D3>(barriers);
    if (!barriers.IsEmpty())
    {
        const auto table = GetDeviceTable();
        VkDependencyInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        info.bufferMemoryBarrierCount = barriers.GetSize();
        info.pBufferMemoryBarriers = barriers.GetData();
        info.dependencyFlags = 0;
        table->CmdPipelineBarrier2KHR(cmd, &info);
    }
}

template <typename T>
ONYX_NO_DISCARD static VKit::DeviceBuffer *findAvailableDrawBuffer(TKit::TierArray<DrawBuffer> &buffers,
                                                                   const u32 drawCount, const VKit::Queue *graphics,
                                                                   const u64 inFlightValue)
{
    for (DrawBuffer &db : buffers)
        if (!db.Tracker.InUse())
        {
            GrowBufferIfNeeded<T>(db.Buffer, drawCount);
            db.Tracker.MarkInUse(graphics, inFlightValue);
            return &db.Buffer;
        }

    DrawBuffer &db = buffers.Append();

    db.Buffer = Onyx::CreateBuffer<T>(
        DeviceBufferFlag_HostMapped | DeviceBufferFlag_HostRandomAccess | DeviceBufferFlag_Indirect, drawCount);

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_VKIT_RESULT(db.Buffer.SetName("onyx-draw-buffer"));
    }

    db.Tracker.MarkInUse(graphics, inFlightValue);
    return &db.Buffer;
}

static VKit::DeviceBuffer *findAvailableDrawBuffer(const u32 drawCount, const VKit::Queue *graphics,
                                                   const u64 inFlightValue)
{
    return findAvailableDrawBuffer<VkDrawIndirectCommand>(*s_DrawBuffers, drawCount, graphics, inFlightValue);
}
static VKit::DeviceBuffer *findAvailableIndexedDrawBuffer(const u32 drawCount, const VKit::Queue *graphics,
                                                          const u64 inFlightValue)
{
    return findAvailableDrawBuffer<VkDrawIndexedIndirectCommand>(*s_IndexedDrawBuffers, drawCount, graphics,
                                                                 inFlightValue);
}

template <Dimension D> static void bindMeshBuffers(const ResourcePool pool, const VkCommandBuffer command)
{
    const Resources::MeshBuffers buffers = Resources::GetMeshBuffers<D>(pool);

    buffers.VertexBuffer->BindAsVertexBuffer(command);
    buffers.IndexBuffer->BindAsIndexBuffer<Index>(command);
}

static VkDrawIndirectCommand createCircleCommand(const u32 firstInstance, const u32 instanceCount)
{
    VkDrawIndirectCommand cmd;
    cmd.firstInstance = firstInstance;
    cmd.instanceCount = instanceCount;
    cmd.firstVertex = 0;
    cmd.vertexCount = 6;
    return cmd;
}

template <Dimension D>
static VkDrawIndexedIndirectCommand createMeshCommand(const Resource mesh, const u32 firstInstance,
                                                      const u32 instanceCount)
{
    const MeshDataLayout layout = Resources::GetMeshLayout<D>(mesh);
    VkDrawIndexedIndirectCommand cmd;
    cmd.firstInstance = firstInstance;
    cmd.instanceCount = instanceCount;
    cmd.firstIndex = layout.IndexStart;
    cmd.indexCount = layout.IndexCount;
    cmd.vertexOffset = layout.VertexStart;
    return cmd;
}

template <Dimension D>
static VkDrawIndexedIndirectCommand createDynamicMeshCommand(const GraphicsInstanceRange &grange,
                                                             const u32 firstInstance, const u32 instanceCount)
{
    TKIT_ASSERT(grange.ActiveVertexRange && grange.ActiveIndexRange,
                "[ONYX][RENDERER] Cannot create a dynamic mesh command when any of the active vertex/index memory "
                "ranges are null");

    VkDrawIndexedIndirectCommand cmd;
    cmd.firstInstance = firstInstance;
    cmd.instanceCount = instanceCount;
    cmd.firstIndex = grange.ActiveIndexRange->Offset / sizeof(Index);
    cmd.indexCount = grange.ActiveIndexRange->Size / sizeof(Index);
    cmd.vertexOffset = grange.ActiveVertexRange->Offset / sizeof(DynamicVertex<D>);

    return cmd;
}

template <Dimension D> static void beginShadowTransitionLayout(const VkCommandBuffer cmd, TextureMap &map)
{
    const VkImageLayout attLayout =
        D == D3 ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    map.Image.TransitionLayout2(
        cmd, attLayout,
        {.SrcAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
         .DstAccess =
             D == D3 ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
         .SrcStage = D == D3 ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
         .DstStage = D == D3 ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR
                             : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});
}

template <Dimension D> static void endShadowTransitionLayout(const VkCommandBuffer cmd, TextureMap &map)
{
    map.Image.TransitionLayout2(cmd, D == D3 ? VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
                                {.SrcAccess = D == D3 ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR
                                                      : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                 .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                 .SrcStage = D == D3 ? VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR
                                                     : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                 .DstStage = D == D3 ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR
                                                     : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR});
}

template <Dimension D>
static void beginShadowPass(const VkCommandBuffer cmd, TextureMap &map, const u32 imageViewIndex = 0)
{
    const VkImageLayout attLayout =
        D == D3 ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    const VKit::DeviceImage::Info &info = map.Image.GetInfo();
    const VkExtent2D extent = VkExtent2D{info.Width, info.Height};

    VkRenderingAttachmentInfo att{};
    att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    att.imageView = map.Image.GetView(imageViewIndex);
    att.imageLayout = attLayout;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfoKHR renderInfo{};

    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    if constexpr (D == D3)
    {
        att.clearValue.depthStencil = {1.f, 0};
        renderInfo.pDepthAttachment = &att;
    }
    else
    {
        att.clearValue.color = {{0.f, 0.f, 0.f, 0.f}};
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &att;
    }

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = f32(extent.width);
    viewport.height = f32(extent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    const auto table = GetDeviceTable();
    table->CmdSetViewport(cmd, 0, 1, &viewport);
    table->CmdSetScissor(cmd, 0, 1, &scissor);

    table->CmdBeginRenderingKHR(cmd, &renderInfo);
}

static void endShadowPass(const VkCommandBuffer cmd)
{
    const auto table = GetDeviceTable();
    table->CmdEndRenderingKHR(cmd);
}

static void addTransferTrackerIfNeeded(TKit::StackArray<Execution::Tracker> &transferTrackers,
                                       const Execution::Tracker &tracker)
{
    bool found = false;
    for (Execution::Tracker &tr : transferTrackers)
    {
        if (tr.Queue == tracker.Queue)
        {
            found = true;
            if (tr.InFlightValue < tracker.InFlightValue)
                tr.InFlightValue = tracker.InFlightValue;
            break;
        }
    }
    if (!found)
        transferTrackers.Append(tracker);
}

template <Dimension D, typename F>
static void collectDrawInfo(const VKit::Queue *graphics, const Geometry geo, const ViewMask viewBit,
                            const u64 inFlightValue, const F &insertCommand, const RenderModeFlags flags,
                            TKit::StackArray<Execution::Tracker> *transferTrackers = nullptr,
                            const BlendPass bpass = BlendPass_All)
{
    RendererData<D> &rdata = getRendererData<D>();
    GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;
    const u32 instanceSize = GetInstanceSize<D>(geo);
    const ResourceType rtype = getResourceType(geo);

    for (GraphicsInstanceRange &grange : gpool.Ranges)
    {
        if (!(grange.ViewMask & viewBit) || !(grange.RenderFlags & flags) ||
            (bpass != BlendPass_All && grange.Blend != bpass))
            continue;

        TKIT_ASSERT(!grange.ContextRanges.IsEmpty(),
                    "[ONYX][RENDERER] Context ranges cannot be empty for a graphics memory range");

        TKIT_ASSERT(geo != Geometry_Circle || grange.MeshHandle == NullHandle,
                    "[ONYX][RENDERER] Circle geometry is required to have a null mesh handle");

        VkDeviceSize offset = grange.Offset;
        VkDeviceSize size = 0;

        bool found = false;
        for (const ContextInstanceRange &crange : grange.ContextRanges)
        {
            if (rdata.IsContextRangeClean(viewBit, crange))
                size += crange.Size;
            else if (size != 0)
            {
                const u32 fi = u32(offset / instanceSize);
                const u32 ic = u32(size / instanceSize);

                offset += size + crange.Size;
                size = 0;
                insertCommand(rtype, grange, fi, ic);
                found = true;
            }
            else
                offset += crange.Size;
        }
        // TKIT_ASSERT(size != 0 || offset > grange.Offset,
        //             "[ONYX][RENDERER] Found labeled graphics memory pool range for window with view bit {} with
        //             " "no context ranges targetting said view", viewBit);
        if (size != 0)
        {
            const u32 fi = u32(offset / instanceSize);
            const u32 ic = u32(size / instanceSize);
            insertCommand(rtype, grange, fi, ic);
        }
        else if (!found)
            continue;

        grange.GraphicsTracker.MarkInUse(graphics, inFlightValue);
        if (transferTrackers && grange.InUseByTransfer())
            addTransferTrackerIfNeeded(*transferTrackers, grange.TransferTracker);

        if (grange.ActiveVertexRange)
        {
            grange.ActiveVertexRange->GraphicsTracker.MarkInUse(graphics, inFlightValue);
            if (transferTrackers && grange.ActiveVertexRange->InUseByTransfer())
                addTransferTrackerIfNeeded(*transferTrackers, grange.ActiveVertexRange->TransferTracker);
        }
        if (grange.ActiveIndexRange)
        {
            grange.ActiveIndexRange->GraphicsTracker.MarkInUse(graphics, inFlightValue);
            if (transferTrackers && grange.ActiveIndexRange->InUseByTransfer())
                addTransferTrackerIfNeeded(*transferTrackers, grange.ActiveIndexRange->TransferTracker);
        }
    }
}

template <Dimension D>
static void setupState(const VkCommandBuffer cmd, const RenderPass rpass, const Geometry geo,
                       const VKit::PipelineLayout &playout, const VKit::GraphicsPipeline &pipeline)
{
    const RendererData<D> &rdata = getRendererData<D>();
    const VkDescriptorSet set = rdata.Descriptors[rpass][geo];

    pipeline.Bind(cmd);
    VKit::DescriptorSet::Bind(GetDevice(), cmd, set, VK_PIPELINE_BIND_POINT_GRAPHICS, playout);
}

enum CullMode : u8
{
    CullMode_None,
    CullMode_Back,
    CullMode_Count,
};

// TODO(Isma): These could be stack arrays
//  per draw cmd
using CircleDrawCommands = TKit::TierArray<VkDrawIndirectCommand>;

using PerCullPerCmd = TKit::FixedArray<TKit::TierArray<VkDrawIndexedIndirectCommand>, CullMode_Count>;

// per mesh type per resource pool per cull mode per draw cmd
using MeshDrawCommands =
    TKit::FixedArray<TKit::FixedArray<PerCullPerCmd, ONYX_MAX_RESOURCE_POOLS>, Resource_MeshPoolCount>;

// per cull mode per draw cmd
using DynMeshDrawCommands = PerCullPerCmd;

template <Dimension D>
static void submitDrawCommands(const VKit::Queue *graphics, const u64 inFlightValue, const VkCommandBuffer cmd,
                               const RenderPass rpass, const VKit::PipelineLayout &playout,
                               const TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count> &pipelines,
                               const CircleDrawCommands &circleCmds, const MeshDrawCommands &meshCmds,
                               const DynMeshDrawCommands &dynMeshCmds)
{
    const auto table = GetDeviceTable();
    const u32 drawCount = circleCmds.GetSize();
    const usz size = circleCmds.GetBytes();
    if (drawCount != 0)
    {
        setupState<D>(cmd, rpass, Geometry_Circle, playout, pipelines[Geometry_Circle]);
        VKit::DeviceBuffer *dbuffer = findAvailableDrawBuffer(drawCount, graphics, inFlightValue);

        dbuffer->Write(circleCmds.GetData(), {.srcOffset = 0, .dstOffset = 0, .size = size});
        ONYX_CHECK_VKIT_RESULT(dbuffer->Flush());
        table->CmdDrawIndirect(cmd, *dbuffer, 0, drawCount, sizeof(VkDrawIndirectCommand));
    }

    // NOTE(Isma): Bit of a mess, should consider cleaning this up

    const auto hasCommands = [](const PerCullPerCmd &drawCmds) {
        return !drawCmds[CullMode_None].IsEmpty() || !drawCmds[CullMode_Back].IsEmpty();
    };

    const auto drawCulledMeshes = [&]([[maybe_unused]] const Geometry geo, const PerCullPerCmd &drawCmds) {
        const TKit::TierArray<VkDrawIndexedIndirectCommand> &cmds1 = drawCmds[CullMode_None];
        const TKit::TierArray<VkDrawIndexedIndirectCommand> &cmds2 = drawCmds[CullMode_Back];

        const u32 dc1 = cmds1.GetSize();
        const u32 dc2 = cmds2.GetSize();
        const u32 drawCount = dc1 + dc2;

        const usz size1 = cmds1.GetBytes();
        const usz size2 = cmds2.GetBytes();

        TKIT_ASSERT((D == D3 && geo != Geometry_Glyph) || size2 == 0,
                    "[ONYX][RENDERER] No back culling draw commands must be submitted for flat geometry");

        VKit::DeviceBuffer *dbuffer = findAvailableIndexedDrawBuffer(drawCount, graphics, inFlightValue);

        if (D == D2 || size1 != 0)
            dbuffer->Write(cmds1.GetData(), {.srcOffset = 0, .dstOffset = 0, .size = size1});
        if constexpr (D == D3)
            if (size2 != 0)
                dbuffer->Write(cmds2.GetData(), {.srcOffset = 0, .dstOffset = size1, .size = size2});

        ONYX_CHECK_VKIT_RESULT(dbuffer->Flush());

        if constexpr (D == D3)
            if (geo != Geometry_Glyph)
                table->CmdSetCullModeEXT(cmd, VK_CULL_MODE_NONE);

        if (D == D2 || size1 != 0)
            table->CmdDrawIndexedIndirect(cmd, *dbuffer, 0, dc1, sizeof(VkDrawIndexedIndirectCommand));
        if constexpr (D == D3)
            if (size2 != 0)
            {
                table->CmdSetCullModeEXT(cmd, VK_CULL_MODE_BACK_BIT);
                table->CmdDrawIndexedIndirect(cmd, *dbuffer, size1, dc2, sizeof(VkDrawIndexedIndirectCommand));
            }
    };

    const auto renderMeshGeometry = [&](const Geometry geo) {
        setupState<D>(cmd, rpass, geo, playout, pipelines[geo]);

        const ResourceType rtype = getResourceType(geo);
        const TKit::Span<const u32> poolIds = Resources::GetResourcePoolIds<D>(rtype);

        for (const ResourcePool pid : poolIds)
            if (hasCommands(meshCmds[rtype][pid]))
            {
                bindMeshBuffers<D>(CreateResourcePoolHandle(rtype, pid), cmd);
                drawCulledMeshes(geo, meshCmds[rtype][pid]);
            }
    };

    renderMeshGeometry(Geometry_Static);
    renderMeshGeometry(Geometry_Parametric);
    renderMeshGeometry(Geometry_Glyph);

    if (hasCommands(dynMeshCmds))
    {
        RendererData<D> &rdata = getRendererData<D>();
        setupState<D>(cmd, rpass, Geometry_Dynamic, playout, pipelines[Geometry_Dynamic]);
        rdata.Geometry.VertexArena.Graphics.Buffer.BindAsVertexBuffer(cmd);
        rdata.Geometry.IndexArena.Graphics.Buffer.template BindAsIndexBuffer<Index>(cmd);

        drawCulledMeshes(Geometry_Dynamic, dynMeshCmds);
    }
}

template <Dimension D> static f32m4 createTransform(const TransformData<D> &transform)
{
    if constexpr (D == D2)
        return f32m4{f32v4{transform.Column0, 0.f, 0.f}, f32v4{transform.Column1, 0.f, 0.f}, f32v4{0.f, 0.f, 0.f, 0.f},
                     f32v4{transform.Column3, 0.f, 1.f}};
    else
        return f32m4{f32v4{transform.Row0[0], transform.Row1[0], transform.Row2[0], 0.f},
                     f32v4{transform.Row0[1], transform.Row1[1], transform.Row2[1], 0.f},
                     f32v4{transform.Row0[2], transform.Row1[2], transform.Row2[2], 0.f},
                     f32v4{transform.Row0[3], transform.Row1[3], transform.Row2[3], 1.f}

        };
}

static CullMode getCullMode(const Resource handle)
{
    return CullMode(Resources::IsBackCulled(handle));
}

template <Dimension D>
static void renderShadows(const VKit::Queue *graphics, const VkCommandBuffer cmd, const ViewMask viewBit,
                          const u64 inFlightValue)
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    const ViewMask dirtyViews = sdata.DirtyShadowViews;
    sdata.DirtyShadowViews &= ~viewBit;

    const auto table = GetDeviceTable();
    const auto processLight =
        [&]<typename LightParams>(const LightType ltype, const TKit::TierArray<LightParams> &lights,
                                  const TKit::TierArray<typename LightParams::InstanceData> lightData) {
            using LightData = LightParams::InstanceData;
            TKIT_ASSERT(
                lights.GetSize() == lightData.GetSize(),
                "[ONYX][RENDERER] Light count and light instance data count must match, but former has {} element(s) "
                "while latter has {}",
                lights.GetSize(), lightData.GetSize());

            for (u32 i = 0; i < lights.GetSize(); ++i)
            {
                const LightData &data = lightData[i];

                constexpr bool isPoint = std::is_same_v<LightParams, PointLightParameters<D>>;
                constexpr bool isDir = std::is_same_v<LightParams, DirectionalLightParameters<D>>;
                constexpr bool isSpot = std::is_same_v<LightParams, SpotLightParameters>;

                if (!(data.ViewMask & viewBit) || !(data.Flags & LightFlag_CastShadows))
                    continue;
                if constexpr (D == D3 && isDir)
                    if (data.CascadeEnable == 0)
                        continue;

                const LightParams &params = lights[i];

                const u32 viewIndex = std::popcount(data.ViewMask & (viewBit - 1));
                const u32 shindex = data.ShadowMapOffset + viewIndex;
                const u32 shPoolIndex = computeShadowMapPoolIndex<D>(ltype, shindex);

                TextureMapArray &shadowMaps = sdata.ShadowMaps[ltype];
                TextureMap &smap = shadowMaps[shPoolIndex];

                smap.Tracker.MarkInUse(graphics, inFlightValue);

                if (!(dirtyViews & viewBit))
                    continue;

                CircleDrawCommands circleCmds{};
                DynMeshDrawCommands dynMeshCmds{};
                MeshDrawCommands meshCmds{};
                const auto insertCommand = [&](const ResourceType rtype, const GraphicsInstanceRange &grange,
                                               const u32 fi, const u32 ic) {
                    if (grange.MeshHandle == NullHandle) // circles sentry
                        circleCmds.Append(createCircleCommand(fi, ic));
                    else
                    {
                        ONYX_CHECK_RESOURCE_IS_NOT_NULL(grange.MeshHandle);
#ifdef TKIT_ENABLE_ASSERTS
                        if (rtype != Resource_DynamicMesh)
                        {
                            ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(grange.MeshHandle);
                            ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(grange.MeshHandle, rtype, D);
                        }
#endif
                        ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(grange.MeshHandle, rtype, D);

                        const u32 pid = GetResourcePoolId(grange.MeshHandle);
                        PerCullPerCmd &cmds = rtype == Resource_DynamicMesh ? dynMeshCmds : meshCmds[rtype][pid];
                        const VkDrawIndexedIndirectCommand cmd = rtype == Resource_DynamicMesh
                                                                     ? createDynamicMeshCommand<D>(grange, fi, ic)
                                                                     : createMeshCommand<D>(grange.MeshHandle, fi, ic);

                        if constexpr (D == D2)
                            cmds[CullMode_None].Append(cmd);
                        else
                            cmds[getCullMode(grange.MeshHandle)].Append(cmd);
                    }
                };
                for (u32 j = 0; j < Geometry_Count; ++j)
                {
                    const Geometry geo = Geometry(j);
                    // filtering by _Shaded saves us from a lot of computation (obviously) but most importantly avoids
                    // misinterpretation of the MatOrSamplerTex field in the instance data with flat render modes
                    collectDrawInfo<D>(graphics, geo, viewBit, inFlightValue, insertCommand, RenderModeFlag_Shaded);
                }

                const auto processMap = [&](TextureMap &map, const f32m4 &projView, const u32 viewIndex = 0) {
                    beginShadowPass<D>(cmd, map, viewIndex);
                    const VKit::PipelineLayout &playout = Pipelines::GetPipelineLayout<D>(RenderPass_Shadow);

                    ShadowPushConstantData<D> pdata;
                    pdata.LightProjection = projView;

                    VkShaderStageFlags flags = VK_SHADER_STAGE_VERTEX_BIT;
                    if constexpr (D == D3)
                    {
                        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
                        if constexpr (isSpot || isPoint)
                            pdata.DepthBias = params.DepthBias;
                        else
                            pdata.DepthBias = params.Cascades.DepthBias[viewIndex];

                        if constexpr (isPoint)
                        {
                            pdata.LightPos = data.Position;
                            pdata.Far = data.ShadowRadius;
                        }
                        else
                            pdata.Far = 0.f;
                    }

                    table->CmdPushConstants(cmd, playout, flags, 0, sizeof(ShadowPushConstantData<D>), &pdata);
                    submitDrawCommands<D>(graphics, inFlightValue, cmd, RenderPass_Shadow, playout, sdata.Pipelines,
                                          circleCmds, meshCmds, dynMeshCmds);

                    endShadowPass(cmd);
                };

                if constexpr (D == D2)
                {
                    const u32 ocindex = findAvailableOcclusionMap();
                    TextureMap &ocmap = sdata.OcclusionMaps[ocindex];
                    ocmap.Tracker.MarkInUse(graphics, inFlightValue);

                    beginShadowTransitionLayout<D>(cmd, ocmap);
                    if constexpr (isPoint)
                    {
                        const f32 r = data.ShadowRadius;
                        f32m3 pv = Transform<D2>::Orthographic(-r, r, -r, r);
                        pv[2][0] -= data.Position[0] * pv[0][0];
                        pv[2][1] -= data.Position[1] * pv[1][1];
                        processMap(ocmap, Transform<D2>::Promote(pv));
                    }
                    else
                        processMap(ocmap, createTransform(data.ProjectionView));

                    endShadowTransitionLayout<D>(cmd, ocmap);

                    smap.Image.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_GENERAL,
                                                 {.SrcAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                                  .DstAccess = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
                                                  .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                                  .DstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR});

                    const VkDescriptorSet set = sdata.RayMarchSet;

                    sdata.RayMarchPipeline.Bind(cmd);
                    const VKit::PipelineLayout &playout = Pipelines::GetPipelineLayout(StandalonePass_RayMarch);
                    VKit::DescriptorSet::Bind(GetDevice(), cmd, set, VK_PIPELINE_BIND_POINT_COMPUTE, playout);

                    RayMarchPushConstantData pdata;
                    pdata.OcclusionMapIndex = ocindex;
                    pdata.OcclusionResolution = sdata.OcclusionResolution;
                    pdata.ShadowMapIndex = shindex;
                    pdata.ShadowResolution = sdata.ShadowResolutions[ltype];
                    pdata.DistanceBias = params.DepthBias;

                    if constexpr (isPoint)
                    {
                        const f32 ext = Math::Pi() * data.Extent;
                        const f32 angle = params.Angle;
                        pdata.StartAngle = angle - ext;
                        pdata.EndAngle = angle + ext;
                        pdata.Mode = ONYX_MARCH_MODE_POINT_LIGHT;
                    }
                    else
                        pdata.Mode = ONYX_MARCH_MODE_DIRECTIONAL_LIGHT;

                    table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                            sizeof(RayMarchPushConstantData), &pdata);

                    constexpr u32 groupSize = 64;
                    table->CmdDispatch(cmd, (pdata.ShadowResolution + groupSize - 1) / groupSize, 1, 1);

                    smap.Image.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                 {.SrcAccess = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
                                                  .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                                  .SrcStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
                                                  .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR});
                }
                else
                {
                    beginShadowTransitionLayout<D>(cmd, smap);
                    // TODO(Isma): Try to pseudo optimize this by working out the views?
                    if constexpr (isPoint)
                    {
                        const f32 near = ONYX_POINT_LIGHT_NEAR_RADIUS_FACTOR * data.ShadowRadius;
                        const f32 far = data.ShadowRadius;
                        const f32m4 proj = Transform<D3>::Perspective(0.5f * Math::Pi(), near, far);

                        constexpr f32v3 faceDir[6] = {
                            f32v3{1.f, 0.f, 0.f},  f32v3{-1.f, 0.f, 0.f}, f32v3{0.f, 1.f, 0.f},
                            f32v3{0.f, -1.f, 0.f}, f32v3{0.f, 0.f, 1.f},  f32v3{0.f, 0.f, -1.f},
                        };
                        constexpr f32v3 faceUp[6] = {
                            f32v3{0.f, -1.f, 0.f}, f32v3{0.f, -1.f, 0.f}, f32v3{0.f, 0.f, 1.f},
                            f32v3{0.f, 0.f, -1.f}, f32v3{0.f, -1.f, 0.f}, f32v3{0.f, -1.f, 0.f},
                        };
                        for (u32 j = 0; j < 6; ++j)
                        {
                            const f32m4 view = Transform<D3>::LookTowards(data.Position, faceDir[j], faceUp[j]);
                            processMap(smap, proj * view, j);
                        }
                    }
                    else if constexpr (isDir)
                    {
                        for (u32 j = 0; j < data.CascadeCount; ++j)
                            if ((1U << j) & data.CascadeEnable)
                                processMap(smap, createTransform(data.Cascades[j].ProjectionView), j);
                    }
                    else
                        processMap(smap, data.ProjectionView);
                    endShadowTransitionLayout<D>(cmd, smap);
                }
            }
        };

    const LightData<D> &ldata = rdata.Lights;
    processLight(Light_Point, ldata.Instances.Points.Lights, ldata.Instances.Points.Data);
    processLight(Light_Directional, ldata.Instances.Directionals.Lights, ldata.Instances.Directionals.Data);
    if constexpr (D == D3)
        processLight(Light_Spot, ldata.Instances.Spots.Lights, ldata.Instances.Spots.Data);
}

template <Dimension D>
static void renderGeometry(const VKit::Queue *graphics, const VkCommandBuffer cmd, const ViewInfo<D> &vinfo,
                           const BlendPass bpass, const u64 inFlightValue,
                           TKit::StackArray<Execution::Tracker> &transferTrackers, const bool shadows)
{
    const ViewMask viewBit = vinfo.ViewBit;

    RendererData<D> &rdata = getRendererData<D>();

    // TODO(Isma): At some point would be good letting the user decide what strategy to use to aggregate ambient
    Color ambient{0.f, 0.f};
    for (const ContextInfo<D> &info : rdata.Contexts)
        if (info.Context->GetViewMask() & viewBit)
        {
            const Color &a = info.Context->GetAmbientLight();
            ambient.rgba = Math::Max(ambient.rgba, a.rgba);
        }
    const u32 ambientColor = ambient.ToLinear().Pack();

    TKit::FixedArray<CircleDrawCommands, PipelinePass_Count> circleCmds{};
    TKit::FixedArray<DynMeshDrawCommands, PipelinePass_Count> dynMeshCmds{};
    TKit::FixedArray<MeshDrawCommands, PipelinePass_Count> meshCmds{};
    TKit::FixedArray<u32, PipelinePass_Count> cmdCount{0, 0, 0};

    const auto insertCommand = [&](const ResourceType rtype, const GraphicsInstanceRange &grange, const u32 fi,
                                   const u32 ic) {
        u32 pcount = 0;
        TKit::FixedArray<PipelinePass, 3> passes;
        const RenderModeFlags flags = grange.RenderFlags;
        if (flags & RenderModeFlag_Shaded)
            passes[pcount++] = PipelinePass_Shaded;
        if (flags & RenderModeFlag_Flat)
            passes[pcount++] = PipelinePass_Flat;
        if (flags & RenderModeFlag_Outlined)
            passes[pcount++] = PipelinePass_Outlined;
        TKIT_ASSERT(pcount != 0, "[ONYX][RENDERER] Pass count should not be zero");

        if (grange.MeshHandle == NullHandle) // circles sentry
            for (u32 i = 0; i < pcount; ++i)
            {
                const PipelinePass p = passes[i];
                circleCmds[p].Append(createCircleCommand(fi, ic));
                ++cmdCount[p];
            }
        else
        {
            ONYX_CHECK_RESOURCE_IS_NOT_NULL(grange.MeshHandle);
#ifdef TKIT_ENABLE_ASSERTS
            if (rtype != Resource_DynamicMesh)
            {
                ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(grange.MeshHandle);
                ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(grange.MeshHandle, rtype, D);
            }
#endif
            ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(grange.MeshHandle, rtype, D);

            const u32 pid = GetResourcePoolId(grange.MeshHandle);

            CullMode cull;
            if constexpr (D == D2)
                cull = CullMode_None;
            else
                cull = getCullMode(grange.MeshHandle);

            const VkDrawIndexedIndirectCommand cmd = rtype == Resource_DynamicMesh
                                                         ? createDynamicMeshCommand<D>(grange, fi, ic)
                                                         : createMeshCommand<D>(grange.MeshHandle, fi, ic);
            if (rtype == Resource_DynamicMesh)
                for (u32 i = 0; i < pcount; ++i)
                {
                    const PipelinePass p = passes[i];
                    dynMeshCmds[p][cull].Append(cmd);
                    ++cmdCount[p];
                }
            else
                for (u32 i = 0; i < pcount; ++i)
                {
                    const PipelinePass p = passes[i];
                    meshCmds[p][rtype][pid][cull].Append(cmd);
                    ++cmdCount[p];
                }
        }
    };

    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        collectDrawInfo<D>(graphics, geo, viewBit, inFlightValue, insertCommand,
                           RenderModeFlag_Shaded | RenderModeFlag_Outlined | RenderModeFlag_Flat, &transferTrackers,
                           bpass);
    }

    LightData<D> &ldata = rdata.Lights;
    for (u32 i = 0; i < ldata.Arenas.GetSize(); ++i)
    {
        LightArena &arena = ldata.Arenas[i];
        if (arena.LightCount == 0)
            continue;

        TKIT_ASSERT(arena.ActiveRange && arena.ActiveRange->Generation == arena.ActiveGeneration,
                    "[ONYX][RENDERER] Active light graphics range arena does not have a valid generation or is just "
                    "null (forgot to call Renderer::PrepareRender()?)");

        arena.ActiveRange->GraphicsTracker.MarkInUse(graphics, inFlightValue);
        if (arena.ActiveRange->InUseByTransfer())
            addTransferTrackerIfNeeded(transferTrackers, arena.ActiveRange->TransferTracker);
    }

    const auto table = GetDeviceTable();

    ShadowData<D> &sdata = rdata.Shadows;
    for (u32 i = 0; i < PipelinePass_Count; ++i)
    {
        if (cmdCount[i] == 0)
            continue;
        const PipelinePass pass = PipelinePass(i);
        const RenderPass rpass = GetRenderPass(pass);
        const VKit::PipelineLayout &playout = Pipelines::GetPipelineLayout<D>(rpass);
        if (rpass == RenderPass_Flat)
            table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(FlatPushConstantData),
                                    &vinfo.ProjectionView);
        else
        {
            ShadedPushConstantData<D> pdata;
            pdata.ProjectionView = vinfo.ProjectionView;
            pdata.Flags = ShadedFlag_Shadows * shadows;

            for (u32 j = 0; j < LightTypeCount<D>; ++j)
            {
                pdata.TexelSizes[j] = 1.f / f32(sdata.ShadowResolutions[j]);
                pdata.LightRanges[j] = (ldata.Ranges[j].Offset << 16) | ldata.Ranges[j].Count;
            }

            if constexpr (D == D3)
            {
                pdata.ViewPosition = vinfo.ViewPosition;
                pdata.ViewForward = vinfo.ViewForward;
            }

            pdata.ViewBit = viewBit;
            pdata.AmbientColor = ambientColor;
            table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                    sizeof(ShadedPushConstantData<D>), &pdata);
        }

        const u32 idx = bpass == BlendPass_All ? BlendPass_Opaque : bpass;
        submitDrawCommands<D>(graphics, inFlightValue, cmd, rpass, playout, rdata.Geometry.Pipelines[idx][pass],
                              circleCmds[pass], meshCmds[pass], dynMeshCmds[pass]);
    }
}

static RenderSubmitInfo createRenderSubmitInfo(VKit::Queue *graphics, const VkCommandBuffer command,
                                               const u64 graphicsFlight, const RenderTargetInfo &tinfo,
                                               TKit::StackArray<Execution::Tracker> &transferTrackers)
{
    RenderSubmitInfo submitInfo{};
    submitInfo.Command = command;
    submitInfo.InFlightValue = graphicsFlight;

    if (tinfo.ImageAvailableSemaphore)
    {
        VkSemaphoreSubmitInfoKHR &imgInfo = submitInfo.WaitSemaphores.Append();
        imgInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
        imgInfo.pNext = nullptr;
        imgInfo.semaphore = tinfo.ImageAvailableSemaphore;
        imgInfo.deviceIndex = 0;
        imgInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    }

    VkSemaphoreSubmitInfoKHR &gtimSemInfo = submitInfo.SignalSemaphores.Append();
    gtimSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    gtimSemInfo.pNext = nullptr;
    gtimSemInfo.value = graphicsFlight;
    gtimSemInfo.deviceIndex = 0;
    gtimSemInfo.semaphore = graphics->GetTimelineSempahore();
    gtimSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

    if (tinfo.RenderFinishedSemaphore)
    {
        VkSemaphoreSubmitInfoKHR &rendFinInfo = submitInfo.SignalSemaphores.Append();
        rendFinInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
        rendFinInfo.pNext = nullptr;
        rendFinInfo.value = 0;
        rendFinInfo.semaphore = tinfo.RenderFinishedSemaphore;
        rendFinInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
        rendFinInfo.deviceIndex = 0;
    }

    for (const Execution::Tracker &ttracker : transferTrackers)
    {
        VkSemaphoreSubmitInfoKHR &ttimSemInfo = submitInfo.WaitSemaphores.Append();
        ttimSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
        ttimSemInfo.pNext = nullptr;
        ttimSemInfo.semaphore = ttracker.Queue->GetTimelineSempahore();
        ttimSemInfo.deviceIndex = 0;
        ttimSemInfo.value = ttracker.InFlightValue;
        ttimSemInfo.stageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
    }
    return submitInfo;
}

template <Dimension D>
static void renderViews(const TKit::TierArray<RenderView<D> *> &views, VKit::Queue *graphics, const VkCommandBuffer cmd,
                        const u64 graphicsFlight, TKit::StackArray<Execution::Tracker> &transferTrackers)
{
    Execution::Tracker tracker;
    tracker.Queue = graphics;
    tracker.InFlightValue = graphicsFlight;

    const auto &device = GetDevice();
    const auto table = GetDeviceTable();

    TKit::StackArray<RenderView<D> *> ppViews{};
    ppViews.Reserve(views.GetSize());

    for (RenderView<D> *rv : views)
    {
        const RenderViewFlags flags = rv->GetFlags();
        if (flags & RenderViewFlag_Hidden)
            continue;
        if (flags & RenderViewFlag_PostProcess)
            ppViews.Append(rv);

        const bool shadows = flags & RenderViewFlag_Shadows;
        if (shadows)
            renderShadows<D>(graphics, cmd, rv->GetViewBit(), graphicsFlight);

        rv->CacheMatrices();
        const ViewInfo<D> vinfo = rv->CreateViewInfo();

        rv->MarkCurrentAttachmentsInUse(tracker);

        const bool transparency = flags & RenderViewFlag_Transparency;
        const BlendPass opaquePass = transparency ? BlendPass_Opaque : BlendPass_All;

        rv->BeginOpaquePass(cmd);
        renderGeometry<D>(graphics, cmd, vinfo, opaquePass, graphicsFlight, transferTrackers, shadows);
        rv->EndOpaquePass(cmd);
        if (transparency)
        {
            rv->BeginTransparentPass(cmd);
            renderGeometry<D>(graphics, cmd, vinfo, BlendPass_Transparent, graphicsFlight, transferTrackers, shadows);
            rv->EndTransparentPass(cmd);
            rv->BeginBlendPass(cmd);
            s_BlendPipeline.Bind(cmd);

            const VKit::PipelineLayout &playout = Pipelines::GetPipelineLayout(StandalonePass_Blend);
            const VkDescriptorSet set = rv->GetBlendSet();
            VKit::DescriptorSet::Bind(device, cmd, set, VK_PIPELINE_BIND_POINT_GRAPHICS, playout);

            const u32 idx = rv->GetAttachmentIndex();
            TKIT_ASSERT(idx < ONYX_MAX_ATTACHMENTS,
                        "[ONYX][RENDERER] The maximum amount of attachments has been exceeded ({} >= {})", idx,
                        ONYX_MAX_ATTACHMENTS);

            table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlendPushConstantData), &idx);
            table->CmdDraw(cmd, 6, 1, 0, 0);

            rv->EndBlendPass(cmd);
        }
    }

    if (!ppViews.IsEmpty())
    {
        const VKit::PipelineLayout &playout = Pipelines::GetPipelineLayout(StandalonePass_PostProcess);
        for (RenderView<D> *rv : ppViews)
        {
            rv->BeginPostProcess(cmd);
            s_PostProcessPipeline.Bind(cmd);

            const VkDescriptorSet set = rv->GetPostProcessSet();
            VKit::DescriptorSet::Bind(device, cmd, set, VK_PIPELINE_BIND_POINT_GRAPHICS, playout);

            PostProcessPushConstantData pdata;
            pdata.AttachmentIndex = rv->GetAttachmentIndex();

            TKIT_ASSERT(pdata.AttachmentIndex < ONYX_MAX_ATTACHMENTS,
                        "[ONYX][RENDERER] The maximum amount of attachments has been exceeded ({} >= {})",
                        pdata.AttachmentIndex, ONYX_MAX_ATTACHMENTS);

            pdata.Extent = rv->GetRenderExtent();
            pdata.MaxOutlineWidth = rv->MaxOutlineWidth;
            pdata.Flags = rv->GetFlags();

            table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostProcessPushConstantData),
                                    &pdata);
            table->CmdDraw(cmd, 6, 1, 0, 0);

            rv->EndPostProcess(cmd);
        }
    }
}

template <Dimension D>
static void renderCompositor(const TKit::TierArray<RenderView<D> *> &views, const VkCommandBuffer cmd,
                             const VKit::PipelineLayout &playout)
{
    const auto &device = GetDevice();
    const auto table = GetDeviceTable();
    for (const RenderView<D> *rv : views)
    {
        if (rv->GetFlags() & RenderViewFlag_Hidden)
            continue;

        const VkDescriptorSet set = rv->GetCompositorSet();
        VKit::DescriptorSet::Bind(device, cmd, set, VK_PIPELINE_BIND_POINT_GRAPHICS, playout);

        // TODO(Isma): Here, you must get the scissor normalized wrt viewport, re-normalize to screen with the viewport
        // coordinates, and multiply by the parent extent
        const Viewport vp = rv->GetAbsoluteViewport();
        const Viewport nvp = rv->GetNormalizedViewport();
        const f32v2 pext = f32v2{rv->GetParentExtent()};

        // the +2 here adds a very small offset that prevents some (rounding??) error that causes a bit of undefined
        // texture to be rendered, clipped from the view's attachment
        const f32 padding = 2.f;
        Scissor sc = rv->GetNormalizedScissor();
        sc.Position = (sc.Position * nvp.Extent + nvp.Position) * (pext + padding);
        sc.Extent *= nvp.Extent * (pext - padding);

        const VkRect2D scissor = AsVulkanScissor(sc);

        const VkViewport viewport = AsVulkanViewport(vp);
        const u32 idx = rv->GetAttachmentIndex();

        table->CmdSetViewport(cmd, 0, 1, &viewport);
        table->CmdSetScissor(cmd, 0, 1, &scissor);
        table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(u32), &idx);
        table->CmdDraw(cmd, 6, 1, 0, 0);
    }
}

template <typename Target>
RenderSubmitInfo render(VKit::Queue *graphics, const VkCommandBuffer cmd, Target *target, const RenderFlags flags)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Render");

    const u64 graphicsFlight = graphics->NextTimelineValue();
    const RenderTargetInfo tinfo = target->CreateRenderTargetInfo();

    TKit::StackArray<Execution::Tracker> transferTrackers{};
    transferTrackers.Reserve(s_SyncPointCount);

    renderViews(tinfo.Views2, graphics, cmd, graphicsFlight, transferTrackers);
    renderViews(tinfo.Views3, graphics, cmd, graphicsFlight, transferTrackers);

    Execution::Tracker tracker;
    tracker.Queue = graphics;
    tracker.InFlightValue = graphicsFlight;

    if constexpr (std::is_same_v<Target, Window>)
        target->MarkImageSemaphoreInUse(tracker);
    else
        target->MarkCurrentImageInUse(tracker);

    target->BeginRendering(cmd);

    s_CompositorPipeline.Bind(cmd);

    const VKit::PipelineLayout &playout = Pipelines::GetPipelineLayout(StandalonePass_Compositor);
    renderCompositor(tinfo.Views2, cmd, playout);
    renderCompositor(tinfo.Views3, cmd, playout);

#ifdef ONYX_ENABLE_IMGUI
    if constexpr (std::is_same_v<Target, Window>)
        if (flags & RenderFlag_ImGui)
        {
            ImGui::Render();
            ImGuiBackend::RenderData(ImGui::GetDrawData(), cmd);
            ImGuiBackend::UpdatePlatformWindows();
        }
#else
    TKIT_UNUSED(flags);
#endif

    target->EndRendering(cmd);
    return createRenderSubmitInfo(graphics, cmd, graphicsFlight, tinfo, transferTrackers);
}

RenderSubmitInfo Render(VKit::Queue *graphics, const VkCommandBuffer cmd, Window *window, const RenderFlags flags)
{
    return render(graphics, cmd, window, flags);
}
RenderSubmitInfo Render(VKit::Queue *graphics, const VkCommandBuffer cmd, RenderTexture *rtex)
{
    return render(graphics, cmd, rtex, 0);
}

void SubmitRender(VKit::Queue *graphics, CommandPool *pool, const TKit::Span<const RenderSubmitInfo> info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::SubmitRender");
    TKIT_ASSERT(!info.IsEmpty(), "[ONYX][RENDERER] Must at least provide one submission");
    TKit::StackArray<VkSubmitInfo2KHR> submits{};
    submits.Reserve(info.GetSize());

    TKit::StackArray<VkCommandBufferSubmitInfoKHR> cmds{};
    cmds.Reserve(info.GetSize());

    u64 maxFlight = 0;
    for (const RenderSubmitInfo &rinfo : info)
    {
        TKIT_ASSERT(rinfo.Command,
                    "[ONYX][RENDERER] A submission must have a valid graphics command buffer to be submitted");
        if (rinfo.InFlightValue > maxFlight)
            maxFlight = rinfo.InFlightValue;

        VkSubmitInfo2KHR &sinfo = submits.Append();
        sinfo = {};
        sinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
        if (!rinfo.WaitSemaphores.IsEmpty())
        {
            sinfo.waitSemaphoreInfoCount = rinfo.WaitSemaphores.GetSize();
            sinfo.pWaitSemaphoreInfos = rinfo.WaitSemaphores.GetData();
        }

        if (!rinfo.SignalSemaphores.IsEmpty())
        {
            sinfo.signalSemaphoreInfoCount = rinfo.SignalSemaphores.GetSize();
            sinfo.pSignalSemaphoreInfos = rinfo.SignalSemaphores.GetData();
        }

        VkCommandBufferSubmitInfoKHR &cmd = cmds.Append();
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
        cmd.pNext = nullptr;
        cmd.commandBuffer = rinfo.Command;
        cmd.deviceMask = 0;

        sinfo.commandBufferInfoCount = 1;
        sinfo.pCommandBufferInfos = &cmd;
        sinfo.flags = 0;
    }

    Execution::MarkInUse(pool, graphics, maxFlight);
    ONYX_CHECK_VKIT_RESULT(graphics->Submit2(submits));
}

template <typename Range, typename F> static void coalesceRanges(Pool<Range> &pool, F &&isInUse)
{
    Range mergeRange{};
    TKit::StackArray<Range> ranges{};
    ranges.Reserve(pool.Ranges.GetSize());

    for (const Range &range : pool.Ranges)
    {
        if (isInUse(range))
        {
            if (mergeRange.Size != 0)
            {
                ranges.Append(mergeRange);
                mergeRange.Offset += mergeRange.Size + range.Size;
                mergeRange.Size = 0;
            }
            else
                mergeRange.Offset += range.Size;
            ranges.Append(range);
        }
        else
            mergeRange.Size += range.Size;
    }
    if (mergeRange.Size != 0)
        ranges.Append(mergeRange);

    TKIT_ASSERT(!ranges.IsEmpty(),
                "[ONYX][RENDERER] All memory ranges for a pool have been removed after coalesce operation!");
    pool.Ranges = ranges;
}

template <typename Range> static void coalesceRanges(Pool<Range> &pool)
{
    coalesceRanges(pool, [](const Range &range) { return range.Tracker.InUse(); });
}

template <Dimension D> static void coalesceGraphicsInstanceRanges(GraphicsInstancePool &gpool, const u32 maxRanges)
{
    const RendererData<D> &rdata = getRendererData<D>();
    GraphicsInstanceRange gmergeRange{};
    TKit::StackArray<GraphicsInstanceRange> granges{};
    granges.Reserve(maxRanges);

    for (const GraphicsInstanceRange &grange : gpool.Ranges)
    {
        if (grange.InUse())
        {
            if (gmergeRange.Size != 0)
            {
                granges.Append(gmergeRange);
                gmergeRange.Offset += gmergeRange.Size + grange.Size;
                gmergeRange.Size = 0;
            }
            else
                gmergeRange.Offset += grange.Size;
            granges.Append(grange);
        }
        else if (!grange.ContextRanges.IsEmpty())
        {
            TKit::StackArray<ContextInstanceRange> cranges{};
            cranges.Reserve(grange.ContextRanges.GetSize());

            TKIT_ASSERT(grange.Size != 0, "[ONYX][RENDERER] Graphics memory range should not have reached a zero "
                                          "size if there are context ranges left");
            GraphicsInstanceRange ngrange{};
            ngrange.Offset = grange.Offset;
            ngrange.RenderFlags = grange.RenderFlags;
            ngrange.MeshHandle = grange.MeshHandle;

            // VkDeviceSize leftover = grange.Size;
            for (const ContextInstanceRange &crange : grange.ContextRanges)
            {
                // leftover -= crange.Size;
                if (rdata.IsContextRangeClean(crange))
                {
                    if (gmergeRange.Size != 0)
                    {
                        granges.Append(gmergeRange);
                        gmergeRange.Offset += gmergeRange.Size + crange.Size;
                        gmergeRange.Size = 0;
                    }
                    else
                        gmergeRange.Offset += crange.Size;

                    ContextInstanceRange &ncrange = cranges.Append(crange);
                    ncrange.Offset = ngrange.Size;
                    ngrange.Size += crange.Size;
                    ngrange.ViewMask |= crange.ViewMask;
                }
                else
                {
                    if (ngrange.Size != 0)
                    {
                        ngrange.ContextRanges = cranges;
                        granges.Append(ngrange);
                        ngrange.Offset += ngrange.Size + crange.Size;
                        ngrange.Size = 0;
                        ngrange.ViewMask = 0;
                        cranges.Clear();
                    }
                    else
                        ngrange.Offset += crange.Size;
                    gmergeRange.Size += crange.Size;
                }
            }
            if (ngrange.Size != 0)
            {
                ngrange.ContextRanges = cranges;
                granges.Append(ngrange);
            }
            // gmergeRange.Size += leftover;
        }
        else
            gmergeRange.Size += grange.Size;
    }
    if (gmergeRange.Size != 0)
        granges.Append(gmergeRange);

    TKIT_ASSERT(!granges.IsEmpty(),
                "[ONYX][RENDERER] All memory ranges for the graphics pool have been removed after coalesce operation!");
    gpool.Ranges = granges;
}

template <Dimension D> void coalesce(const u32 maxRanges)
{
#ifdef TKIT_ENABLE_ASSERTS
    validateRanges<D>();
#endif
    RendererData<D> &rdata = getRendererData<D>();
    for (InstanceArena &arena : rdata.Geometry.Arenas)
    {
        coalesceRanges(arena.Transfer);
        coalesceGraphicsInstanceRanges<D>(arena.Graphics, maxRanges);
    }
    LightData<D> &ldata = rdata.Lights;
    for (LightArena &arena : ldata.Arenas)
    {
        coalesceRanges(arena.Transfer);
        coalesceRanges(arena.Graphics, [&arena](const GraphicsRange &grange) {
            return grange.Generation == arena.ActiveGeneration || grange.InUse();
        });
    }

    coalesceRanges(rdata.Geometry.VertexArena.Transfer);
    coalesceRanges(rdata.Geometry.IndexArena.Transfer);

    // NOTE(Isma): This is NOT ideal at all
    coalesceRanges(rdata.Geometry.VertexArena.Graphics, [&](const GraphicsRange &vgrange) {
        if (vgrange.InUse())
            return true;
        for (const GraphicsInstanceRange &grange : rdata.Geometry.Arenas[Geometry_Dynamic].Graphics.Ranges)
            if (grange.ActiveVertexGeneration == vgrange.Generation)
                return true;
        return false;
    });
    coalesceRanges(rdata.Geometry.IndexArena.Graphics, [&](const GraphicsRange &vgrange) {
        if (vgrange.InUse())
            return true;
        for (const GraphicsInstanceRange &grange : rdata.Geometry.Arenas[Geometry_Dynamic].Graphics.Ranges)
            if (grange.ActiveIndexGeneration == vgrange.Generation)
                return true;
        return false;
    });

#ifdef TKIT_ENABLE_ASSERTS
    validateRanges<D>();
#endif
}

void Coalesce(const u32 maxRanges)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Coalesce");
    coalesce<D2>(maxRanges);
    coalesce<D3>(maxRanges);
}

#ifdef ONYX_ENABLE_IMGUI
template <Dimension D, typename Range>
static void displayRanges(const char *name, const Pool<Range> &pool, const u64 generation = 0)
{
    const RendererData<D> &rdata = getRendererData<D>();
    // NOTE(Isma): Using stack strings as return values may be a bit dangerous. RVO may save us here, but its an
    // unreliable solution
    const auto fmts = [](const VkDeviceSize bytes) -> TKit::StackString {
        if (bytes > 1_gib)
            return TKit::StackString::Format("{:.2f} gib", f32(bytes) / f32(1_gib));
        if (bytes > 1_mib)
            return TKit::StackString::Format("{:.2f} mib", f32(bytes) / f32(1_mib));
        if (bytes > 1_kib)
            return TKit::StackString::Format("{:.2f} kib", f32(bytes) / f32(1_kib));
        return TKit::StackString::Format("{:L} b", bytes);
    };

    const auto fmtb = [](const VkDeviceSize bytes) -> TKit::StackString {
        return TKit::StackString::Format("{:L} b", bytes);
    };

    if (ImGui::TreeNode(&pool, "%s pool ranges (%u)", name, pool.Ranges.GetSize()))
    {
        ImGui::Text("Buffer size: %s", fmts(pool.Buffer.GetInfo().Size).CString());
        for (const Range &range : pool.Ranges)
            if constexpr (std::is_same_v<Range, TransferInstanceRange> || std::is_same_v<Range, TransferLightRange>)
                ImGui::Text("%s (%s): %s - %s", range.Tracker.InUse() ? "IN-USE" : "FREE", fmts(range.Size).CString(),
                            fmtb(range.Offset).CString(), fmtb(range.Offset + range.Size).CString());
            else if constexpr (std::is_same_v<Range, GraphicsInstanceRange>)
            {
                if (ImGui::TreeNode(&range, "%s (%s): %s - %s",
                                    range.InUse()
                                        ? "IN-USE"
                                        : (rdata.AreAllContextRangesDirty(range)
                                               ? "FREE"
                                               : (rdata.AreAllContextRangesClean(range) ? "CLEAN" : "FRAGMENTED")),
                                    fmts(range.Size).CString(), fmtb(range.Offset).CString(),
                                    fmtb(range.Offset + range.Size).CString()))
                {
                    ImGui::Text("In use by transfer queue: %s", range.TransferTracker.InUse() ? "YES" : "NO");
                    ImGui::Text("In use by graphics queue: %s", range.GraphicsTracker.InUse() ? "YES" : "NO");
                    if (range.MeshHandle != NullHandle)
                        ImGui::Text("Mesh handle: %u", range.MeshHandle);

                    if (range.RenderFlags != 0)
                        ImGui::Text("Render mode: %s", ToString(GetRenderMode(range.RenderFlags)));
                    const TKit::StackString vmask = TKit::StackString::Format("{:032b}", range.ViewMask);
                    ImGui::Text("View mask: %s", vmask.CString());
                    if (ImGui::TreeNode(&range.ContextRanges, "Context ranges (%u)", range.ContextRanges.GetSize()))
                    {
                        for (const ContextInstanceRange &crange : range.ContextRanges)
                            if (ImGui::TreeNode(&crange, "%s (%s): %s - %s",
                                                rdata.IsContextRangeClean(crange) ? "CLEAN" : "DIRTY",
                                                fmts(crange.Size).CString(), fmtb(crange.Offset).CString(),
                                                fmtb(crange.Offset + crange.Size).CString()))
                            {
                                if (crange.ContextIndex != TKIT_U32_MAX)
                                {
                                    ImGui::Text("Context index: %u", crange.ContextIndex);
#    ifndef TKIT_OS_LINUX
                                    ImGui::Text("Context generation: %llu", crange.Generation);
#    else
                                    ImGui::Text("Context generation: %lu", crange.Generation);
#    endif
                                }
                                else
                                    ImGui::Text("Context index: None");

                                const TKit::StackString cvmask = TKit::StackString::Format("{:032b}", crange.ViewMask);
                                ImGui::Text("View mask: %s", cvmask.CString());
                                ImGui::TreePop();
                                ImGui::Spacing();
                            }
                        ImGui::TreePop();
                        ImGui::Spacing();
                    }
                    ImGui::TreePop();
                    ImGui::Spacing();
                }
            }
            else
                ImGui::Text("%s (%s): %s - %s",
                            range.InUse() ? "IN-USE" : (range.Generation == generation ? "CLEAN" : "FREE"),
                            fmts(range.Size).CString(), fmtb(range.Offset).CString(),
                            fmtb(range.Offset + range.Size).CString());
        ImGui::TreePop();
        ImGui::Spacing();
    }
}

#    ifdef ONYX_ENABLE_IMPLOT
template <Dimension D, typename TRange, typename GRange>
static void plotRanges(const Pool<TRange> &tpool, const Pool<GRange> &gpool, const u64 generation = 0)
{
    const RendererData<D> &rdata = getRendererData<D>();
    const auto fmts = [](const VkDeviceSize bytes) -> TKit::StackString {
        if (bytes > 1_gib)
            return TKit::StackString::Format("{:.2f} gib", f32(bytes) / f32(1_gib));
        if (bytes > 1_mib)
            return TKit::StackString::Format("{:.2f} mib", f32(bytes) / f32(1_mib));
        if (bytes > 1_kib)
            return TKit::StackString::Format("{:.2f} kib", f32(bytes) / f32(1_kib));
        return TKit::StackString::Format("{:L} b", bytes);
    };

    const auto fmtb = [](const VkDeviceSize bytes) -> TKit::StackString {
        return TKit::StackString::Format("{:L} b", bytes);
    };
    const VkDeviceSize maxSize = Math::Max(tpool.Buffer.GetInfo().Size, gpool.Buffer.GetInfo().Size);
    constexpr u32 top = 2 + std::is_same_v<GRange, GraphicsInstanceRange>;
    ImPlot::SetNextAxesLimits(0.0, f64(maxSize), -1, top, ImGuiCond_Always);

    if (ImPlot::BeginPlot("Memory ranges", ImVec2(-1, -1)))
    {
        constexpr TKit::FixedArray<const char *, 5> status = {"FREE", "IN-USE", "CLEAN", "DIRTY", "FRAGMENTED"};
        const TKit::FixedArray<u32, 5> colors = {
            Color::FromHexadecimal(0x6B7280B3).Pack(), Color::FromHexadecimal(0x22C55EB3).Pack(),
            Color::FromHexadecimal(0x3B82F6B3).Pack(), Color::FromHexadecimal(0xF59E0BB3).Pack(),
            Color::FromHexadecimal(0xF97316B3).Pack()};

        ImPlot::SetupAxes("Offset", nullptr, 0, ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, f64(top), ImGuiCond_Always);
        ImDrawList *dl = ImPlot::GetPlotDrawList();

        const f32 height = 1.f;
        const f32 separation = 0.1f;
        const auto drawPlot = [&](const u32 bindex, const VkDeviceSize offset, const VkDeviceSize size, const u32 idx,
                                  const Resource meshHandle = NullHandle, const RenderMode rmode = RenderMode_None) {
            const ImVec2 mnpix = ImPlot::PlotToPixels(f64(offset), f64(bindex * height + separation));
            const ImVec2 mxpix = ImPlot::PlotToPixels(f64(offset + size), f64((bindex + 1) * height - separation));

            dl->AddRectFilled(mnpix, mxpix, colors[idx]);
            dl->AddRect(mnpix, mxpix, IM_COL32(50, 50, 50, 180));

            const char *lbl = status[idx];
            if (ImPlot::IsPlotHovered())
            {
                const ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                if (mouse.x >= offset && mouse.x <= offset + size && mouse.y >= bindex && mouse.y <= bindex + 1.0)
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s - Offset: %s - Size: %s", lbl, fmtb(offset).CString(), fmts(size).CString());
                    if (rmode != RenderMode_None)
                    {
                        ImGui::SameLine();
                        ImGui::Text("- Mesh handle: %s - Render mode: %s",
                                    TKit::StackString::Format("{:#010x}", meshHandle).CString(), ToString(rmode));
                    }
                    ImGui::EndTooltip();
                }
            }
        };

        const auto drawLabel = [&dl](const char *name, const u32 bindex) {
            const ImVec2 labelPos = ImPlot::PlotToPixels(0, (bindex + 0.5));
            dl->AddText(ImVec2(labelPos.x + 4, labelPos.y - ImGui::GetTextLineHeight() * 0.5f),
                        IM_COL32(255, 255, 255, 255), name);
        };

        for (const TRange &trange : tpool.Ranges)
            drawPlot(top - 1, trange.Offset, trange.Size, trange.Tracker.InUse() ? 1 : 0);

        for (const GRange &range : gpool.Ranges)
            if constexpr (std::is_same_v<GRange, GraphicsInstanceRange>)
            {
                const u32 idx =
                    range.InUse()
                        ? 1
                        : (rdata.AreAllContextRangesDirty(range) ? 0 : (rdata.AreAllContextRangesClean(range) ? 2 : 4));
                drawPlot(1, range.Offset, range.Size, idx, range.MeshHandle,
                         range.RenderFlags != 0 ? GetRenderMode(range.RenderFlags) : RenderMode_None);
                for (const ContextInstanceRange &crange : range.ContextRanges)
                    drawPlot(0, range.Offset + crange.Offset, crange.Size, rdata.IsContextRangeClean(crange) ? 2 : 3);
            }
            else
            {
                const u32 idx = range.InUse() ? 1 : (range.Generation == generation ? 2 : 0);
                drawPlot(0, range.Offset, range.Size, idx);
            }

        drawLabel("Transfer", top - 1);
        drawLabel("Graphics", top - 2);
        if constexpr (std::is_same_v<GRange, GraphicsInstanceRange>)
            drawLabel("Context", 0);

        if (ImPlot::IsPlotHovered())
        {
            const ImVec2 plotPos = ImPlot::GetPlotPos();
            const ImVec2 plotSize = ImPlot::GetPlotSize();

            constexpr f32 legendPadding = 8.f;
            constexpr f32 swatchSize = 12.f;
            constexpr f32 swatchSpacing = 4.f;

            f32 totalWidth = legendPadding;
            const u32 stsize = status.GetSize() - 2 * std::is_same_v<GRange, GraphicsRange>;
            for (u32 j = 0; j < stsize; ++j)
                totalWidth += swatchSize + swatchSpacing + ImGui::CalcTextSize(status[j]).x + legendPadding;

            const f32 legendHeight = swatchSize + legendPadding * 2.f;

            const ImVec2 legendMin = ImVec2(plotPos.x + (plotSize.x - totalWidth) * 0.5f,
                                            plotPos.y + plotSize.y - legendHeight - legendPadding);
            const ImVec2 legendMax = ImVec2(legendMin.x + totalWidth, legendMin.y + legendHeight);

            dl->AddRectFilled(legendMin, legendMax, IM_COL32(30, 30, 30, 200));
            dl->AddRect(legendMin, legendMax, IM_COL32(255, 255, 255, 80));

            f32 cursorX = legendMin.x + legendPadding;
            const f32 itemY = legendMin.y + legendPadding;

            for (u32 j = 0; j < stsize; ++j)
            {
                const ImVec2 swatchMin = ImVec2(cursorX, itemY);
                const ImVec2 swatchMax = ImVec2(cursorX + swatchSize, itemY + swatchSize);
                dl->AddRectFilled(swatchMin, swatchMax, colors[j]);
                dl->AddRect(swatchMin, swatchMax, IM_COL32(0, 0, 0, 255));
                cursorX += swatchSize + swatchSpacing;

                dl->AddText(ImVec2(cursorX, itemY + swatchSize * 0.5f - ImGui::GetTextLineHeight() * 0.5f),
                            IM_COL32(255, 255, 255, 255), status[j]);
                cursorX += ImGui::CalcTextSize(status[j]).x + legendPadding;
            }
        }
        ImPlot::EndPlot();
    }
}
#    endif

template <Dimension D> void DisplayMemoryLayout()
{
    const RendererData<D> &rdata = getRendererData<D>();
    const LightData<D> &ldata = rdata.Lights;
    ImGui::PushID(&rdata);
    if (ImGui::Button("Coalesce##Button"))
        coalesce<D>(512);

    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        const InstanceArena &arena = rdata.Geometry.Arenas[geo];
        if (ImGui::TreeNode(&arena, "%s", ToString(geo)))
        {
            displayRanges<D>("Transfer", arena.Transfer);
            displayRanges<D>("Graphics", arena.Graphics);
#    ifdef ONYX_ENABLE_IMPLOT
            plotRanges<D>(arena.Transfer, arena.Graphics);
#    endif
            ImGui::TreePop();
            ImGui::Spacing();
        }
    }
    for (u32 i = 0; i < LightTypeCount<D>; ++i)
    {
        const LightType light = LightType(i);
        const LightArena &arena = ldata.Arenas[light];
        if (ImGui::TreeNode(&arena, "%s", ToString(light)))
        {
            displayRanges<D>("Transfer", arena.Transfer);
            displayRanges<D>("Graphics", arena.Graphics, arena.ActiveGeneration);
#    ifdef ONYX_ENABLE_IMPLOT
            plotRanges<D>(arena.Transfer, arena.Graphics, arena.ActiveGeneration);
#    endif
            ImGui::TreePop();
            ImGui::Spacing();
        }
    }
    if (ImGui::TreeNode("Vertex buffer"))
    {
        const Arena &arena = rdata.Geometry.VertexArena;
        displayRanges<D>("Transfer", arena.Transfer);
        displayRanges<D>("Graphics", arena.Graphics);
#    ifdef ONYX_ENABLE_IMPLOT
        plotRanges<D>(arena.Transfer, arena.Graphics);
#    endif
    }
    if (ImGui::TreeNode("Index buffer"))
    {
        const Arena &arena = rdata.Geometry.IndexArena;
        displayRanges<D>("Transfer", arena.Transfer);
        displayRanges<D>("Graphics", arena.Graphics);
#    ifdef ONYX_ENABLE_IMPLOT
        plotRanges<D>(arena.Transfer, arena.Graphics);
#    endif
    }
    ImGui::PopID();
}

template void DisplayMemoryLayout<D2>();
template void DisplayMemoryLayout<D3>();
#endif

template const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets<D2>(RenderPass rpass);
template const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets<D3>(RenderPass rpass);

template RenderContext<D2> *CreateContext(u32 immediateDynamicMeshCapacity);
template RenderContext<D3> *CreateContext(u32 immediateDynamicMeshCapacity);

template void DestroyContext(RenderContext<D2> *context);
template void DestroyContext(RenderContext<D3> *context);

template void BindBuffer<D2>(u32 binding, TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass,
                             u32 dstElement);
template void BindBuffer<D3>(u32 binding, TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass,
                             u32 dstElement);

template void BindImage<D2>(u32 binding, TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement);
template void BindImage<D3>(u32 binding, TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement);

} // namespace Onyx::Renderer
