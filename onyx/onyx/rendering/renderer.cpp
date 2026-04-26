#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/platform/window.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "vkit/resource/sampler.hpp"
#include "vkit/state/compute_pipeline.hpp"
#include "tkit/container/tier_hive.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/profiling/macros.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include <imgui.h>
#    include "onyx/imgui/backend.hpp"
#    ifdef ONYX_ENABLE_IMPLOT
#        include <implot.h>
#    endif
#endif

namespace Onyx::Renderer
{
using namespace Detail;
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

using TransferInstanceRange = TransferRange;

struct GraphicsInstanceRange
{
    Execution::Tracker TransferTracker{};
    Execution::Tracker GraphicsTracker{};

    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
    ViewMask ViewMask = 0;
    Asset MeshHandle = NullHandle;
    RenderModeFlags RenderFlags = 0;
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

template <typename T> struct Pool
{
    VKit::DeviceBuffer Buffer{};
    TKit::TierArray<T> Ranges{};
};

using TransferInstancePool = Pool<TransferInstanceRange>;
using GraphicsInstancePool = Pool<GraphicsInstanceRange>;

struct InstanceArena
{
    TransferInstancePool Transfer{};
    GraphicsInstancePool Graphics{};
};

using TransferLightRange = TransferRange;

struct GraphicsLightRange
{
    Execution::Tracker TransferTracker{};
    Execution::Tracker GraphicsTracker{};

    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;

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

using TransferLightPool = Pool<TransferLightRange>;
using GraphicsLightPool = Pool<GraphicsLightRange>;

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
    GraphicsLightRange *ActiveRange = nullptr;
    u64 Generation = 1;
    u32 LightCount = 0;
};

struct GeometryData
{
    TKit::FixedArray<InstanceArena, Geometry_Count> Arenas{};
    TKit::FixedArray<TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count>, PipelinePass_Count> Pipelines{};
};

template <typename LightParams> struct ContextLights
{
    TKit::TierArray<LightParams> Lights{};
    TKit::TierArray<typename LightParams::InstanceData> Data{};
};

template <Dimension D> struct LightInstances
{
    ContextLights<PointLightParameters<D>> Points{};
    void ClearLights()
    {
        Points.Lights.Clear();
    }
};

template <> struct LightInstances<D3>
{
    ContextLights<PointLightParameters<D3>> Points{};
    ContextLights<DirectionalLightParameters> Directionals{};

    void ClearLights()
    {
        Points.Lights.Clear();
        Directionals.Lights.Clear();
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
    u32 Resolution = TKIT_U32_MAX;
    bool Grabbed = false;
};

using TextureMapArray = TKit::StaticArray<TextureMap, ONYX_MAX_TEXTURE_MAPS>;

template <Dimension D> struct ShadowData;

template <> struct ShadowData<D2>
{
    VKit::Sampler Sampler{};
    TKit::TierHive<Range> ShadowMapSlots{};

    TextureMapArray OcclusionMaps{};
    TextureMapArray ShadowMaps{};
    u32 OcclusionResolution = 0;
    u32 ShadowResolution = 0;

    TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count> Pipelines{}; // occlusion pipelines
    VKit::ComputePipeline DistancePipeline{};
    VkDescriptorSet DistanceSet = VK_NULL_HANDLE;
    VkFormat OcclusionFormat = VK_FORMAT_UNDEFINED;
    VkFormat ShadowFormat = VK_FORMAT_UNDEFINED;
    ViewMask DirtyViews = 0;
};

using ShadowMapId = u32;

template <> struct ShadowData<D3>
{
    VKit::Sampler Sampler{};
    TKit::TierHive<Range> ShadowMapSlots{};

    TextureMapArray PointMaps{};
    TextureMapArray DirectionalMaps{};
    TKit::FixedArray<u32, LightTypeCount<D3>> ShadowResolutions{};

    TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count> Pipelines{};
    VkFormat ShadowFormat = VK_FORMAT_UNDEFINED;
    ViewMask DirtyViews = 0;
};

template <Dimension D> struct ContextInfo
{
    RenderContext<D> *Context = nullptr;
    TKit::FixedArray<TKit::TierArray<ShadowMapId>, LightTypeCount<D>> ShadowMapRanges{};
    u64 Generation = 0;
    u32 Index = 0; // unstable: dont use except for in transfer

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
static VKit::GraphicsPipeline s_PostProcessPipeline{};
static VKit::GraphicsPipeline s_CompositorPipeline{};
static VKit::Sampler s_GeneralSampler{};

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
        return sizeof(DirectionalLightData);
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

        Descriptors::BindBuffer<D>(Descriptors::GetInstancesBindingPoint(), set, info, rpass);
    }
}

template <Dimension D> static u32 lightToBinding(const LightType light)
{
    return light == Light_Point ? Descriptors::GetPointLightsBindingPoint()
                                : Descriptors::GetDirectionalLightsBindingPoint();
}

template <Dimension D> static void updateLightDescriptorSets(const LightType light)
{
    RendererData<D> &rdata = getRendererData<D>();
    const VkDescriptorBufferInfo info = rdata.Lights.Arenas[light].Graphics.Buffer.CreateDescriptorInfo();
    Descriptors::BindBuffer<D>(lightToBinding<D>(light), rdata.Descriptors[RenderPass_Shaded], info, RenderPass_Shaded);
}

static constexpr VKit::DeviceBufferFlags getStageFlags()
{
    return VKit::DeviceBufferFlags(Buffer_Staging) | VKit::DeviceBufferFlag_Destination;
}
static constexpr VKit::DeviceBufferFlags getDeviceLocalFlags()
{
    return VKit::DeviceBufferFlags(Buffer_DeviceStorage) | VKit::DeviceBufferFlag_Source;
}

template <Dimension D>
static VKit::DeviceBuffer createTransferInstanceBuffer(const Geometry geo,
                                                       const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    VKit::DeviceBuffer buffer = Resources::CreateBuffer(getStageFlags(), GetInstanceSize<D>(geo) * instances);

    if (IsDebugUtilsEnabled())
    {
        const std::string name =
            TKit::Format("onyx-renderer-transfer-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
        ONYX_CHECK_EXPRESSION(buffer.SetName(name.c_str()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createGraphicsInstanceBuffer(const Geometry geo,
                                                       const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    VKit::DeviceBuffer buffer = Resources::CreateBuffer(getDeviceLocalFlags(), instances * GetInstanceSize<D>(geo));

    if (IsDebugUtilsEnabled())
    {
        const std::string name =
            TKit::Format("onyx-renderer-graphics-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
        ONYX_CHECK_EXPRESSION(buffer.SetName(name.c_str()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createTransferLightBuffer(const LightType light,
                                                    const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    VKit::DeviceBuffer buffer = Resources::CreateBuffer(Buffer_Staging, instances * getLightSize<D>(light));

    if (IsDebugUtilsEnabled())
    {
        const std::string name =
            TKit::Format("onyx-renderer-transfer-light-buffer-{}D-type-{}", u8(D), ToString(light));
        ONYX_CHECK_EXPRESSION(buffer.SetName(name.c_str()));
    }
    return buffer;
}

template <Dimension D>
static VKit::DeviceBuffer createGraphicsLightBuffer(const LightType light,
                                                    const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    VKit::DeviceBuffer buffer = Resources::CreateBuffer(Buffer_DeviceStorage, instances * getLightSize<D>(light));
    if (IsDebugUtilsEnabled())
    {
        const std::string name =
            TKit::Format("onyx-renderer-graphics-light-buffer-{}D-type-{}", u8(D), ToString(light));
        ONYX_CHECK_EXPRESSION(buffer.SetName(name.c_str()));
    }
    return buffer;
}

template <Dimension D> static void createPipelines()
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    const VkFormat cformat = Platform::GetColorFormat();
    VkPipelineRenderingCreateInfoKHR rinfo{};
    rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachmentFormats = &cformat;
    rinfo.depthAttachmentFormat = Platform::GetDepthStencilFormat();
    rinfo.stencilAttachmentFormat = Platform::GetDepthStencilFormat();

    for (u32 j = 0; j < Geometry_Count; ++j)
    {
        const Geometry geo = Geometry(j);
        for (u32 i = 0; i < PipelinePass_Count; ++i)
        {
            const PipelinePass pass = PipelinePass(i);
            rdata.Geometry.Pipelines[pass][geo] = Pipelines::CreateGeometryPipeline<D>(pass, geo);

            if (IsDebugUtilsEnabled())
            {
                const std::string name = TKit::Format("onyx-renderer-geometry-pipeline-{}D-pass-{}-geometry-'{}'",
                                                      u8(D), ToString(pass), ToString(geo));
                ONYX_CHECK_EXPRESSION(rdata.Geometry.Pipelines[pass][geo].SetName(name.c_str()));
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
            const std::string name =
                TKit::Format("onyx-renderer-shadow-pipeline-{}D-geometry-'{}'", u8(D), ToString(geo));
            ONYX_CHECK_EXPRESSION(sdata.Pipelines[geo].SetName(name.c_str()));
        }
    }

    if constexpr (D == D2)
    {
        sdata.DistancePipeline = Pipelines::CreateDistancePipeline();
        if (IsDebugUtilsEnabled())
        {
            ONYX_CHECK_EXPRESSION(sdata.DistancePipeline.SetName("onyx-renderer-distance-pipeline"));
        }
    }
}

static void createPipelines()
{
    s_PostProcessPipeline = Pipelines::CreatePostProcessPipeline();
    s_CompositorPipeline = Pipelines::CreateCompositorPipeline();

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_EXPRESSION(s_PostProcessPipeline.SetName("onyx-post-process-pipeline"));
        ONYX_CHECK_EXPRESSION(s_CompositorPipeline.SetName("onyx-compositor-pipeline"));
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
        for (u32 pass = 0; pass < PipelinePass_Count; ++pass)
            if (rdata.Geometry.Pipelines[pass][geo])
                rdata.Geometry.Pipelines[pass][geo].Destroy();
        sdata.Pipelines[geo].Destroy();
    }
    if constexpr (D == D2)
        sdata.DistancePipeline.Destroy();
}

static void destroyPipelines()
{
    destroyPipelines<D2>();
    destroyPipelines<D3>();
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

        gpool.Ranges.Append(GraphicsLightRange{.Size = gpool.Buffer.GetInfo().Size});
        updateLightDescriptorSets<D>(light);
    }
}

template <Dimension D> static void initializeShadows(const ShadowSpecs<D> &specs)
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;
    VKit::Sampler::Builder builder{GetDevice()};

    if constexpr (D == D3)
        builder.SetCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
            .SetAddressModes(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
            .SetBorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

    sdata.Sampler = ONYX_CHECK_EXPRESSION(builder.Build());

    VkDescriptorImageInfo info{};
    info.sampler = sdata.Sampler;

    BindImage<D>(Descriptors::GetShadowSamplerBindingPoint(), info, RenderPass_Shaded);

    sdata.ShadowFormat = specs.ShadowFormat;
    if constexpr (D == D2)
    {
        sdata.OcclusionFormat = specs.OcclusionFormat;
        sdata.ShadowResolution = specs.ShadowResolution;
        sdata.OcclusionResolution = specs.OcclusionResolution;
        sdata.DistanceSet = ONYX_CHECK_EXPRESSION(
            Descriptors::GetDescriptorPool().Allocate(Descriptors::GetDistanceDescriptorLayout()));
    }
    else
        sdata.ShadowResolutions = specs.ShadowResolutions;

    if (IsDebugUtilsEnabled())
    {
        const std::string name = TKit::Format("onyx-renderer-shadow-sampler-{}D", u8(D));
        ONYX_CHECK_EXPRESSION(sdata.Sampler.SetName(name.c_str()));
    }
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
            const VkDescriptorSet set = ONYX_CHECK_EXPRESSION(Descriptors::GetDescriptorPool().Allocate(layout));
            if (IsDebugUtilsEnabled())
            {
                const auto &device = GetDevice();
                const std::string name = TKit::Format("onyx-renderer-descriptor-set-{}D-geometry-{}-pass-{}", u8(D),
                                                      ToString(geo), ToString(rpass));
                ONYX_CHECK_EXPRESSION(device.SetObjectName(set, VK_OBJECT_TYPE_DESCRIPTOR_SET, name.c_str()));
            }

            const VkDescriptorBufferInfo info = gpool.Buffer.CreateDescriptorInfo();
            Descriptors::BindBuffer<D>(Descriptors::GetInstancesBindingPoint(), set, info, rpass);
            rdata.Descriptors[j][i] = set;
        }
    }
    initializeLights<D>();
    return initializeShadows(shadowSpecs);
}

template <Dimension D> static void terminateShadows()
{
    ShadowData<D> &sdata = getRendererData<D>().Shadows;
    sdata.Sampler.Destroy();
    if constexpr (D == D2)
    {
        for (auto &map : sdata.OcclusionMaps)
            map.Image.Destroy();
        for (auto &map : sdata.ShadowMaps)
            map.Image.Destroy();
    }
    else
    {
        for (auto &map : sdata.PointMaps)
            map.Image.Destroy();
        for (auto &map : sdata.DirectionalMaps)
            map.Image.Destroy();
    }
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

    s_GeneralSampler = ONYX_CHECK_EXPRESSION(VKit::Sampler::Builder(GetDevice()).Build());

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_EXPRESSION(s_GeneralSampler.SetName("onyx-general-sampler"));
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
    s_GeneralSampler.Destroy();
    s_RendererData2.Destruct();
    s_RendererData3.Destruct();
    s_IndexedDrawBuffers.Destruct();
    s_DrawBuffers.Destruct();
}

template <Dimension D> RenderContext<D> *CreateContext()
{
    RendererData<D> &rdata = getRendererData<D>();
    ContextInfo<D> info;

    TKit::TierAllocator *tier = TKit::GetTier();
    info.Context = tier->Create<RenderContext<D>>();
    info.Generation = info.Context->GetGeneration();
    info.Index = rdata.Contexts.GetSize();

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

    ContextInfo<D> &cinfo = rdata.Contexts[index];
    ShadowData<D> &sdata = rdata.Shadows;
    for (const TKit::TierArray<ShadowMapId> &ranges : cinfo.ShadowMapRanges)
        for (const ShadowMapId id : ranges)
            sdata.ShadowMapSlots.Remove(id);

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

const VKit::Sampler &GetGeneralPurposeSampler()
{
    return s_GeneralSampler;
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
    const LightData<D> &ldata = rdata.Lights;
    for (const LightArena &arena : ldata.Arenas)
    {
        validateRanges("transfer light", arena.Transfer);
        validateRanges("graphics light", arena.Graphics);
        for (u32 i = 0; i < arena.Graphics.Ranges.GetSize(); ++i)
        {
            const GraphicsLightRange &grange = arena.Graphics.Ranges[i];
            TKIT_ASSERT(grange.Generation <= arena.Generation,
                        "[ONYX][RENDERER] Found graphics light memory range of index {} whose generation {} exceeds "
                        "the one of the arena ({})",
                        i, grange.Generation, arena.Generation);
        }
    }
}
#endif

template <Dimension D, typename Range>
static Range *handlePoolResize(const VkDeviceSize requiredMem, VKit::DeviceBuffer &nbuffer, VKit::DeviceBuffer &buffer,
                               TKit::TierArray<Range> &ranges, TKit::StackArray<Task> *tasks = nullptr,
                               VKit::Queue *transfer = nullptr)
{
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
        if constexpr (std::is_same_v<Range, GraphicsInstanceRange> || std::is_same_v<Range, GraphicsLightRange>)
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
        ONYX_CHECK_EXPRESSION(table->WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX));
    }

    const VkDeviceSize size = buffer.GetInfo().Size;
    const VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = size};

    if constexpr (std::is_same_v<Range, GraphicsInstanceRange>)
    {
        TKIT_ASSERT(transfer);
        ONYX_CHECK_EXPRESSION(nbuffer.CopyFromBuffer(Execution::GetTransientTransferPool(), *transfer, buffer, copy));
    }
    else if constexpr (std::is_same_v<Range, TransferInstanceRange>)
        nbuffer.Write(buffer.GetData(), copy);

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

    TKIT_LOG_DEBUG("[ONYX][RENDERER] Failed to find a suitable range with {:L} bytes of memory. A new buffer "
                   "will be created with more memory (from {:L} to {:L} bytes)",
                   requiredMem, size, icount * instanceSize);
    return icount;
}

template <Dimension D>
static TransferInstanceRange *findTransferInstanceRange(const Geometry geo, TransferInstancePool &pool,
                                                        const VkDeviceSize requiredMem, TKit::StackArray<Task> &tasks)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindTransferInstanceRange");
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    for (u32 i = 0; i < ranges.GetSize(); ++i)
        if (ranges[i].Size >= requiredMem && !ranges[i].Tracker.InUse())
            return splitRange(i, ranges, requiredMem);

    VKit::DeviceBuffer &buffer = pool.Buffer;
    const u32 icount = computeNewInstanceCount(GetInstanceSize<D>(geo), buffer, requiredMem);

    auto bresult = createTransferInstanceBuffer<D>(geo, icount);
    return handlePoolResize<D>(requiredMem, bresult, buffer, ranges, &tasks);
}

template <Dimension D>
static GraphicsInstanceRange *findGraphicsInstanceRange(const Geometry geo, GraphicsInstancePool &pool,
                                                        const VkDeviceSize requiredMem, VKit::Queue *transfer,
                                                        TKit::StackArray<Task> &tasks)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindGraphicsInstanceRange");
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        GraphicsInstanceRange &range = ranges[i];
        if (range.Size >= requiredMem && !range.InUse() && rdata.AreAllContextRangesDirty(range))
            return splitRange(i, ranges, requiredMem);
    }

    VKit::DeviceBuffer &buffer = pool.Buffer;
    const u32 icount = computeNewInstanceCount(GetInstanceSize<D>(geo), buffer, requiredMem);
    for (u32 i = rdata.AcquireBarriers.GetSize() - 1; i < rdata.AcquireBarriers.GetSize(); --i)
    {
        const VkBufferMemoryBarrier2KHR &barrier = rdata.AcquireBarriers[i];
        if (barrier.buffer == buffer.GetHandle())
            rdata.AcquireBarriers.RemoveUnordered(rdata.AcquireBarriers.begin() + i);
    }

    VKit::DeviceBuffer nbuffer = createGraphicsInstanceBuffer<D>(geo, icount);
    GraphicsInstanceRange *range = handlePoolResize<D>(requiredMem, nbuffer, buffer, ranges, &tasks, transfer);
    updateInstanceDescriptorSets<D>(geo);
    return range;
}

template <Dimension D>
static TransferLightRange *findTransferLightRange(const LightType light, TransferLightPool &pool,
                                                  const VkDeviceSize requiredMem)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindTransferLightRange");
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    for (u32 i = 0; i < ranges.GetSize(); ++i)
        if (ranges[i].Size >= requiredMem && !ranges[i].Tracker.InUse())
            return splitRange(i, ranges, requiredMem);

    VKit::DeviceBuffer &buffer = pool.Buffer;
    const u32 icount = computeNewInstanceCount(getLightSize<D>(light), buffer, requiredMem);

    auto bresult = createTransferLightBuffer<D>(light, icount);
    return handlePoolResize<D>(requiredMem, bresult, buffer, ranges);
}

template <Dimension D>
static GraphicsLightRange *findGraphicsLightRange(const LightType light, GraphicsLightPool &pool,
                                                  const VkDeviceSize requiredMem)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindGraphicsLightRange");
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    for (u32 i = 0; i < ranges.GetSize(); ++i)
        if (ranges[i].Size >= requiredMem && !ranges[i].InUse())
            return splitRange(i, ranges, requiredMem);

    VKit::DeviceBuffer &buffer = pool.Buffer;
    const u32 icount = computeNewInstanceCount(getLightSize<D>(light), buffer, requiredMem);

    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = rdata.AcquireBarriers.GetSize() - 1; i < rdata.AcquireBarriers.GetSize(); --i)
    {
        const VkBufferMemoryBarrier2KHR &barrier = rdata.AcquireBarriers[i];
        if (barrier.buffer == buffer.GetHandle())
            rdata.AcquireBarriers.RemoveUnordered(rdata.AcquireBarriers.begin() + i);
    }

    VKit::DeviceBuffer nbuffer = createGraphicsLightBuffer<D>(light, icount);
    GraphicsLightRange *range = handlePoolResize<D>(requiredMem, nbuffer, buffer, ranges);
    updateLightDescriptorSets<D>(light);
    return range;
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

static AssetType getAssetType(const Geometry geo)
{
    switch (geo)
    {
    case Geometry_Static:
        return Asset_StaticMesh;
    case Geometry_Parametric:
        return Asset_ParametricMesh;
    case Geometry_Glyph:
        return Asset_GlyphMesh;
    default:
        return Asset_Count;
        TKIT_FATAL("[ONYX][RENDERER] The geometry '{}' does not have an asset type associated", ToString(geo));
    }
}

static u32 findSuitableOcclusionMap()
{
    ShadowData<D2> &sdata = s_RendererData2->Shadows;
    const u32 size = sdata.OcclusionMaps.GetSize();
    for (u32 i = 0; i < size; ++i)
        if (!sdata.OcclusionMaps[i].Tracker.InUse())
            return i;

    TextureMap &map = sdata.OcclusionMaps.Append();
    map.Image = ONYX_CHECK_EXPRESSION(
        VKit::DeviceImage::Builder(
            GetDevice(), GetVulkanAllocator(), VkExtent2D{sdata.OcclusionResolution, sdata.OcclusionResolution},
            sdata.OcclusionFormat, VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Storage)
            .AddImageView()
            .Build());
    map.Resolution = sdata.OcclusionResolution;

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_EXPRESSION(map.Image.SetName(TKit::Format("onyx-occlusion-map-{}", size).c_str()));
        ONYX_CHECK_EXPRESSION(map.Image.SetViewNames(TKit::Format("onyx-occlusion-map-view-{}", size).c_str()));
    }

    VKit::DescriptorSet::Writer writer{GetDevice(), &Descriptors::GetDistanceDescriptorLayout()};
    VkDescriptorImageInfo info{};
    info.imageView = map.Image.GetView();
    info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    writer.WriteImage(Descriptors::GetOcclusionMapBindingPoint(), info, size);
    writer.Overwrite(sdata.DistanceSet);
    return size;
}

template <Dimension D>
static Range findSuitableTextureMapRange(const LightType light, const VkFormat format, const u32 count,
                                         const u32 resolution, TextureMapArray &maps)
{
    Range range{};
    const auto grabMaps = [&] {
        for (u32 i = 0; i < range.Count; ++i)
            maps[i + range.Offset].Grabbed = true;
        return range;
    };
    for (u32 i = 0; i < maps.GetSize(); ++i)
    {
        const TextureMap &map = maps[i];
        TKIT_ASSERT(map.Resolution == resolution, "[ONYX][RENDERER] Resolution mismatch between texture maps");
        ++range.Count;
        if (map.Tracker.InUse() || map.Grabbed)
        {
            range.Offset = i + 1;
            range.Count = 0;
        }
        if (range.Count == count)
            return grabMaps();
    }

    const VKit::DeviceImageFlags flags = (D == D3 ? VKit::DeviceImageFlag_DepthAttachment
                                                  : (VKit::DeviceImageFlag_Storage | VKit::DeviceImageFlag_Color)) |
                                         VKit::DeviceImageFlag_Sampled;

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
            const u32 lcount = light == Light_Point ? 6 : ONYX_MAX_CASCADES;
            builder.SetArrayLayers(lcount);
            for (u32 j = 0; j < lcount; ++j)
            {
                VkImageSubresourceRange range{};
                range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                range.baseArrayLayer = j;
                range.layerCount = 1;
                range.levelCount = 1;
                builder.AddImageView(range);
            }
            if (light == Light_Point)
            {
                builder.SetFlags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
                builder.AddImageView(VK_IMAGE_VIEW_TYPE_CUBE);
            }
            else
                builder.AddImageView(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        }

        map.Image = ONYX_CHECK_EXPRESSION(builder.Build());
        map.Resolution = resolution;

        if (IsDebugUtilsEnabled())
        {
            ONYX_CHECK_EXPRESSION(map.Image.SetName(
                TKit::Format("onyx-texture-map-{}D-'{}'-{}", u8(D), ToString(light), range.Offset + i).c_str()));
            ONYX_CHECK_EXPRESSION(map.Image.SetViewNames(
                TKit::Format("onyx-texture-map-view-{}D-'{}'-{}", u8(D), ToString(light), range.Offset + i).c_str()));
        }

        VkDescriptorImageInfo info{};
        info.imageView = map.Image.GetViews().GetBack();
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const u32 dstElement = range.Offset + i;
        if constexpr (D == D2)
        {
            BindImage<D>(Descriptors::GetShadowMapsBindingPoint(), info, RenderPass_Shaded, dstElement);
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VKit::DescriptorSet::Writer writer{GetDevice(), &Descriptors::GetDistanceDescriptorLayout()};
            writer.WriteImage(Descriptors::GetDistanceMapBindingPoint(), info, dstElement);
            writer.Overwrite(getRendererData<D2>().Shadows.DistanceSet);
        }
        else if (light == Light_Point)
            BindImage<D>(Descriptors::GetPointMapsBindingPoint(), info, RenderPass_Shaded, dstElement);
        else
            BindImage<D>(Descriptors::GetDirectionalMapsBindingPoint(), info, RenderPass_Shaded, dstElement);
    }
    range.Count = count;
    return grabMaps();
}

template <Dimension D>
PointLightData<D> createLightData(const ViewMask vmask, const u32 shadowMapOffset,
                                  const PointLightParameters<D> &params)
{
    PointLightData<D> data;
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

struct ShadowCascades
{
    TKit::FixedArray<f32m4, ONYX_MAX_CASCADES> ProjectionViews;
    TKit::FixedArray<f32, ONYX_MAX_CASCADES> Splits;
};

template <typename T> static T cascadeLerp(const T mn, const T mx, const f32 t, const f32 lambda)
{
    const T ln = Math::LinearLerp(mn, mx, t);
    const T lg = Math::LogLerp(mn, mx, t);
    return Math::LinearLerp(ln, lg, lambda);
}

static ShadowCascades createCascades(const f32v3 &dir, const u32 count, const f32 lambda, const f32 overlap,
                                     const FixedCascadeParameters &params)
{
    TKIT_ASSERT(count <= ONYX_MAX_CASCADES, "[ONYX] The maximum amount of cascades is {}", ONYX_MAX_CASCADES);
    ShadowCascades cascades;

    for (u32 i = 0; i < count; ++i)
    {
        const f32 t = f32(i + 1) / f32(count);

        const f32 width = cascadeLerp(params.MinSize, params.MaxSize, t, lambda);
        cascades.ProjectionViews[i] =
            Transform<D3>::Orthographic(-width, width, -width, width, params.Near, params.Far) *
            Transform<D3>::LookTowardsMatrix(params.ViewPosition, dir);
        cascades.Splits[i] = width * (1.f + overlap);
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

static ShadowCascades createCascades(const f32v3 &dir, const RenderView<D3> *rview, const u32 count, const f32 zmul,
                                     const f32 lambda, const f32 overlap)
{
    const TKit::FixedArray<f32v4, 8> globalCorners = getCameraCorners(rview->GetProjectionView());

    ShadowCascades cascades;
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
    for (u32 i = 0; i < count; ++i)
    {
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

        f32m4 view = Transform<D3>::LookTowardsMatrix(center, dir);
        f32v3 min{TKIT_F32_MAX};
        f32v3 max{TKIT_F32_LOWEST};
        for (const f32v4 &c : corners)
        {
            const f32v3 lc = f32v3{view * c};
            min = Math::Min(min, lc);
            max = Math::Max(max, lc);
        }

        if (min[2] < 0.f)
            min[2] *= zmul;
        else
            min[2] /= zmul;

        if (max[2] < 0.f)
            max[2] /= zmul;
        else
            max[2] *= zmul;

        cascades.ProjectionViews[i] = Transform<D3>::Orthographic(min, max) * view;
        cascades.Splits[i] = split1 * (1.f + overlap);
        split0 = split1;
    }
    return cascades;
}

DirectionalLightData createLightData(const ViewMask vmask, const u32 shadowMapOffset,
                                     const DirectionalLightParameters &params)
{
    DirectionalLightData data;
    const f32v3 dir = Math::Normalize(params.Direction);
    if (params.Flags & LightFlag_CastsShadows)
    {
        const ShadowCascadeParameters &c = params.Cascades;
        TKIT_ASSERT(c.Count != 0,
                    "[ONYX][RENDERER] If a directional light casts shadows, its cascade count must be greater than 0");

        const ShadowCascades cascades =
            c.View ? createCascades(dir, c.View, c.Count, c.FittedParameters.ZMul, c.Lambda, c.Overlap)
                   : createCascades(dir, c.Count, c.Lambda, c.Overlap, c.FixedParameters);

        for (u32 i = 0; i < c.Count; ++i)
        {
            data.Cascades[i] = CreateTransformData<D3>(cascades.ProjectionViews[i]);
            data.Splits[i] = cascades.Splits[i];
        }
        data.CascadeCount = c.Count;
    }

    data.Direction = dir;
    data.Intensity = params.Intensity;
    data.Color = params.Tint.ToLinear().Pack();
    data.ViewMask = vmask;
    data.ShadowMapOffset = shadowMapOffset;
    data.Flags = params.Flags;
    return data;
}

template <Dimension D>
static void transfer(VKit::Queue *transfer, const VkCommandBuffer command, TransferSubmitInfo &info,
                     TKit::StackArray<VkBufferMemoryBarrier2KHR> *release, const u64 transferFlightValue)
{

    RendererData<D> &rdata = getRendererData<D>();
    TKit::TierArray<ContextInfo<D>> &contexts = rdata.Contexts;
    if (contexts.IsEmpty())
        return;

    TKit::StackArray<const ContextInfo<D> *> dirtyContexts{};
    dirtyContexts.Reserve(rdata.Contexts.GetSize());

    struct LightInfo
    {
        TKit::StackArray<ViewMask> Views{};
        TKit::StackArray<ShadowMapId> ShadowMapRanges{};

        void Reserve(const u32 count)
        {
            Views.Reserve(count);
            ShadowMapRanges.Reserve(count);
        }
    };

    TKit::FixedArray<LightInfo, LightTypeCount<D>> linfo{};

    // TODO(Isma): Improve this. a bit sloppy
    const u32 count = 100 + 10 * contexts.GetSize();
    linfo[Light_Point].Reserve(count);
    if constexpr (D == D3)
        linfo[Light_Directional].Reserve(count);

    using LightUpdateFlags = u8;
    enum LightUpdateFlagBit : LightUpdateFlags
    {
        LightUpdateFlag_Point = 1 << 0,
        LightUpdateFlag_Directional = 1 << 1,
    };

    LightUpdateFlags toUpdate = 0;

    LightData<D> &ldata = rdata.Lights;
    ldata.Instances.ClearLights();

    ViewMask shadowedViews = 0;

    ShadowData<D> &sdata = rdata.Shadows;
    for (u32 i = 0; i < contexts.GetSize(); ++i)
    {
        ContextInfo<D> &cinfo = contexts[i];
        cinfo.Index = i;

        RenderContext<D> *ctx = cinfo.Context;

        const ViewMask vmask = ctx->GetViewMask();

        const bool isDirty = cinfo.IsDirty();
        if (isDirty)
        {
            dirtyContexts.Append(&cinfo);
            sdata.DirtyViews |= vmask;
            cinfo.Generation = ctx->GetGeneration();
            for (TKit::TierArray<ShadowMapId> &ranges : cinfo.ShadowMapRanges)
            {
                for (const ShadowMapId rangeId : ranges)
                    sdata.ShadowMapSlots.Remove(rangeId);
                ranges.Clear();
            }
        }
        const auto gatherLights =
            [&]<typename LightParams>(const LightType ltype, const TKit::TierArray<LightParams> &src,
                                      TKit::TierArray<LightParams> &dst, const LightUpdateFlags update) {
                LightInfo &info = linfo[ltype];
                TKit::TierArray<ShadowMapId> &ranges = cinfo.ShadowMapRanges[ltype];

                LightFlags flags = 0;
                for (u32 i = 0; i < src.GetSize(); ++i)
                {
                    const LightParams &params = src[i];
                    flags |= params.Flags;

                    dst.Append(params);
                    info.Views.Append(vmask);
                    if (isDirty)
                    {
                        const ShadowMapId id = ranges.Append(sdata.ShadowMapSlots.Insert(Range{}));
                        info.ShadowMapRanges.Append(id);
                    }
                    else
                        info.ShadowMapRanges.Append(ranges[i]);
                }
                shadowedViews |= vmask * !src.IsEmpty() * (flags & LightFlag_CastsShadows);
                toUpdate |= update * isDirty * !src.IsEmpty();
            };

        gatherLights(Light_Point, ctx->GetPointLightData(), ldata.Instances.Points.Lights, LightUpdateFlag_Point);
        if constexpr (D == D3)
            gatherLights(Light_Directional, ctx->GetDirectionalLightData(), ldata.Instances.Directionals.Lights,
                         LightUpdateFlag_Directional);
    }

    if (sdata.DirtyViews & shadowedViews)
        toUpdate |= LightUpdateFlag_Point | LightUpdateFlag_Directional;

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
    if constexpr (D == D3)
        toUpdate |= checkLightCountChange(Light_Directional, LightUpdateFlag_Directional, ldata.Instances.Directionals);

    const auto copyLightRanges = [&]<typename LightParams>(const LightType ltype, ContextLights<LightParams> &clights) {
        info.Command = command;
        clights.Data.Clear();

        LightArena &arena = ldata.Arenas[ltype];
        TransferLightPool &tpool = arena.Transfer;
        GraphicsLightPool &gpool = arena.Graphics;

        using LightData = typename LightParams::InstanceData;
        const VkDeviceSize requiredMem = sizeof(LightData) * clights.Lights.GetSize();

        TextureMapArray *maps;
        u32 resolution;
        if constexpr (D == D2)
        {
            maps = &sdata.ShadowMaps;
            resolution = sdata.ShadowResolution;
        }
        else
        {
            resolution = sdata.ShadowResolutions[ltype];
            if constexpr (std::is_same_v<LightParams, DirectionalLightParameters>)
                maps = &sdata.DirectionalMaps;
            else
                maps = &sdata.PointMaps;
        }

        const LightInfo &info = linfo[ltype];
        for (u32 i = 0; i < clights.Lights.GetSize(); ++i)
        {
            const LightParams &light = clights.Lights[i];

            const ViewMask vm = info.Views[i];
            const ShadowMapId shadowId = info.ShadowMapRanges[i];
            Range &range = sdata.ShadowMapSlots[shadowId];

            const u32 count = std::popcount(vm);
            if (count != 0 && (light.Flags & LightFlag_CastsShadows) && ((vm & sdata.DirtyViews) || range.Count == 0))
                range = findSuitableTextureMapRange<D>(ltype, sdata.ShadowFormat, count, resolution, *maps);

            const u32 shadowOffset = range.Offset;
            clights.Data.Append(createLightData(vm, shadowOffset, light));
        }

        TransferLightRange *trange = findTransferLightRange<D>(ltype, tpool, requiredMem);
        trange->Tracker.MarkInUse(transfer, transferFlightValue);
        tpool.Buffer.Write(clights.Data.GetData(), {.srcOffset = 0, .dstOffset = trange->Offset, .size = trange->Size});

        GraphicsLightRange *grange = findGraphicsLightRange<D>(ltype, gpool, requiredMem);
        grange->TransferTracker.MarkInUse(transfer, transferFlightValue);
        grange->GraphicsTracker = {};
        grange->Generation = ++arena.Generation;

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

    if constexpr (D == D3)
        if ((toUpdate & LightUpdateFlag_Directional) && !ldata.Instances.Directionals.Lights.IsEmpty())
            copyLightRanges(Light_Directional, ldata.Instances.Directionals);

    if (dirtyContexts.IsEmpty())
        return;

    TKit::StackArray<VkBufferCopy2KHR> copies{};
    const u32 bcount = Assets::GetDistinctBatchDrawCount<D>();
    copies.Reserve(bcount);

    TKit::StackArray<ContextInstanceRange> contextRanges{};
    contextRanges.Reserve(dirtyContexts.GetSize());

    TKit::ITaskManager *tm = GetTaskManager();

    TKit::StackArray<Task> tasks{};
    tasks.Reserve(dirtyContexts.GetSize() * bcount);

    u32 sindex = 0;

    struct RangePair
    {
        VKit::DeviceBuffer *GraphicsBuffer;
        VkDeviceSize GraphicsOffset;
        VkDeviceSize RequiredMemory;
    };

    struct CopyCommands
    {
        VKit::DeviceBuffer *Transfer;
        VKit::DeviceBuffer *Graphics;
        u32 Offset;
        u32 Size;
    };

    TKit::StackArray<RangePair> ranges{};
    ranges.Reserve(RenderMode_Count * bcount);

    TKit::StackArray<CopyCommands> copyCmds{};
    copyCmds.Reserve(RenderMode_Count * u32(Geometry_Count));

    const auto finishTasks = [&] {
        for (const Task &task : tasks)
            tm->WaitUntilFinished(task);
    };

    const auto findInstanceRanges = [&](const u32 rmode, const Geometry geo, const Asset handle,
                                        const auto getInstanceData) {
        TransferInstancePool &tpool = rdata.Geometry.Arenas[geo].Transfer;
        GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;

        contextRanges.Clear();
        VkDeviceSize requiredMem = 0;
        ViewMask viewMask = 0;
        for (const ContextInfo<D> *cinfo : dirtyContexts)
        {
            const RenderContext<D> *ctx = cinfo->Context;
            const auto &idata = getInstanceData(ctx);
            if (idata.Instances == 0)
                continue;

            ContextInstanceRange &crange = contextRanges.Append();
            crange.ContextIndex = cinfo->Index;
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

        TransferInstanceRange *trange = findTransferInstanceRange<D>(geo, tpool, requiredMem, tasks);
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
        GraphicsInstanceRange *grange = findGraphicsInstanceRange<D>(geo, gpool, requiredMem, transfer, tasks);
        grange->MeshHandle = handle;
        grange->ContextRanges = contextRanges;
        grange->ViewMask = viewMask;
        grange->RenderFlags = GetRenderModeFlags(RenderMode(rmode));
        grange->TransferTracker.MarkInUse(transfer, transferFlightValue);
        grange->GraphicsTracker = {};

        VkBufferCopy2KHR &copy = copies.Append();
        copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
        copy.pNext = nullptr;
        copy.srcOffset = trange->Offset;
        copy.dstOffset = grange->Offset;
        copy.size = requiredMem;

        ranges.Append(&gpool.Buffer, grange->Offset, requiredMem);
    };

    const auto gatherInstanceRanges = [&](const u32 rmode, const Geometry geo) {
        TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindRanges");
        TransferInstancePool &tpool = rdata.Geometry.Arenas[geo].Transfer;
        GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;

        CopyCommands copyCmd{};
        copyCmd.Transfer = &tpool.Buffer;
        copyCmd.Graphics = &gpool.Buffer;
        copyCmd.Offset = copies.GetSize();

        if (geo == Geometry_Circle)
            findInstanceRanges(
                rmode, Geometry_Circle, NullHandle,
                [rmode](const RenderContext<D> *ctx) -> const auto & { return ctx->GetInstanceData()[rmode].Circles; });
        else
        {
            const AssetType atype = getAssetType(geo);
            const TKit::Span<const u32> poolIds = Assets::GetAssetPoolIds<D>(atype);
            for (const u32 pid : poolIds)
            {
                const u32 mcount = Assets::GetAssetCount<D>(Assets::CreateAssetPoolHandle(atype, pid));
                for (u32 i = 0; i < mcount; ++i)
                    findInstanceRanges(rmode, geo, Assets::CreateAssetHandle(atype, i, pid),
                                       [atype, rmode, pid, i](const RenderContext<D> *ctx) -> const auto & {
                                           return ctx->GetInstanceData()[rmode].Meshes[atype][pid][i];
                                       });
            }
        }

        copyCmd.Size = copies.GetSize() - copyCmd.Offset;
        if (copyCmd.Size != 0)
            copyCmds.Append(copyCmd);
    };

    for (u32 rmode = 0; rmode < RenderMode_Count; ++rmode)
    {
        gatherInstanceRanges(rmode, Geometry_Circle);
        gatherInstanceRanges(rmode, Geometry_Static);
        gatherInstanceRanges(rmode, Geometry_Parametric);
        gatherInstanceRanges(rmode, Geometry_Glyph);
    }
    for (const RangePair &range : ranges)
    {
        rdata.AcquireBarriers.Append(
            createAcquireBarrier(*range.GraphicsBuffer, range.GraphicsOffset, range.RequiredMemory));

        if (release)
            release->Append(createReleaseBarrier(*range.GraphicsBuffer, range.GraphicsOffset, range.RequiredMemory));
    }
    for (const CopyCommands &cmd : copyCmds)
    {
        const TKit::Span<const VkBufferCopy2KHR> cpspan{copies.GetData() + cmd.Offset, cmd.Size};
        cmd.Graphics->CopyFromBuffer2(command, *cmd.Transfer, cpspan);
    }

    info.Command = command;
    finishTasks();
}

TransferSubmitInfo Transfer(VKit::Queue *tqueue, const VkCommandBuffer command, const u32 maxReleaseBarriers)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Transfer");
    TransferSubmitInfo submitInfo{};
    const bool separate = Execution::IsSeparateTransferMode();
    TKit::StackArray<VkBufferMemoryBarrier2KHR> release{};
    if (separate)
        release.Reserve(maxReleaseBarriers);

    const u64 transferFlight = tqueue->NextTimelineValue();

    transfer<D2>(tqueue, command, submitInfo, separate ? &release : nullptr, transferFlight);
    transfer<D3>(tqueue, command, submitInfo, separate ? &release : nullptr, transferFlight);

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
    ONYX_CHECK_EXPRESSION(transfer->Submit2(submits));
}

template <Dimension D> void gatherAcquireBarriers(TKit::StackArray<VkBufferMemoryBarrier2KHR> &barriers)
{
    RendererData<D> &rdata = getRendererData<D>();
    barriers.Insert(barriers.end(), rdata.AcquireBarriers.begin(), rdata.AcquireBarriers.end());
    rdata.AcquireBarriers.Clear();
}

template <Dimension D> void prepareRender()
{
    LightData<D> &ldata = getRendererData<D>().Lights;
    for (u32 i = 0; i < ldata.Arenas.GetSize(); ++i)
    {
        ldata.Ranges[i] = Range{};

        LightArena &arena = ldata.Arenas[i];
        arena.ActiveRange = nullptr;
        if (arena.LightCount != 0)
            for (GraphicsLightRange &grange : arena.Graphics.Ranges)
                if (grange.Generation == arena.Generation)
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
ONYX_NO_DISCARD static VKit::DeviceBuffer *findSuitableDrawBuffer(TKit::TierArray<DrawBuffer> &buffers,
                                                                  const u32 drawCount, const VKit::Queue *graphics,
                                                                  const u64 inFlightValue)
{
    for (DrawBuffer &db : buffers)
        if (!db.Tracker.InUse())
        {
            Resources::GrowBufferIfNeeded<T>(db.Buffer, drawCount);
            db.Tracker.MarkInUse(graphics, inFlightValue);
            return &db.Buffer;
        }

    DrawBuffer &db = buffers.Append();

    db.Buffer = Resources::CreateBuffer<T>(VKit::DeviceBufferFlag_HostMapped | VKit::DeviceBufferFlag_HostRandomAccess |
                                               VKit::DeviceBufferFlag_Indirect,
                                           drawCount);

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_EXPRESSION(db.Buffer.SetName("onyx-draw-buffer"));
    }

    db.Tracker.MarkInUse(graphics, inFlightValue);
    return &db.Buffer;
}

static VKit::DeviceBuffer *findSuitableDrawBuffer(const u32 drawCount, const VKit::Queue *graphics,
                                                  const u64 inFlightValue)
{
    return findSuitableDrawBuffer<VkDrawIndirectCommand>(*s_DrawBuffers, drawCount, graphics, inFlightValue);
}
static VKit::DeviceBuffer *findSuitableIndexedDrawBuffer(const u32 drawCount, const VKit::Queue *graphics,
                                                         const u64 inFlightValue)
{
    return findSuitableDrawBuffer<VkDrawIndexedIndirectCommand>(*s_IndexedDrawBuffers, drawCount, graphics,
                                                                inFlightValue);
}

template <Dimension D> static void bindMeshBuffers(const AssetPool pool, const VkCommandBuffer command)
{
    const Assets::MeshBuffers buffers = Assets::GetMeshBuffers<D>(pool);

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
static VkDrawIndexedIndirectCommand createCommand(const Asset mesh, const u32 firstInstance, const u32 instanceCount)
{
    const MeshDataLayout layout = Assets::GetMeshLayout<D>(mesh);
    VkDrawIndexedIndirectCommand cmd;
    cmd.firstInstance = firstInstance;
    cmd.instanceCount = instanceCount;
    cmd.firstIndex = layout.IndexStart;
    cmd.indexCount = layout.IndexCount;
    cmd.vertexOffset = layout.VertexStart;
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

template <Dimension D, typename F>
static void collectDrawInfo(const VKit::Queue *graphics, const Geometry geo, const ViewMask viewBit,
                            const u64 inFlightValue, const F &insertCommand, const RenderModeFlags flags,
                            TKit::StackArray<Execution::Tracker> *transferTrackers = nullptr)
{
    RendererData<D> &rdata = getRendererData<D>();
    GraphicsInstancePool &gpool = rdata.Geometry.Arenas[geo].Graphics;
    const u32 instanceSize = GetInstanceSize<D>(geo);
    const AssetType atype = geo == Geometry_Circle ? Asset_PoolCount : getAssetType(geo);

    for (GraphicsInstanceRange &grange : gpool.Ranges)
    {
        if (!(grange.ViewMask & viewBit) || !(grange.RenderFlags & flags))
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

                offset += size;
                size = 0;
                insertCommand(atype, grange, fi, ic);
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
            insertCommand(atype, grange, fi, ic);
        }
        else if (!found)
            continue;

        if (transferTrackers && grange.InUseByTransfer())
        {
            Execution::Tracker &tracker = grange.TransferTracker;
            found = false;
            for (Execution::Tracker &tr : *transferTrackers)
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
                transferTrackers->Append(tracker);
        }
        grange.GraphicsTracker.MarkInUse(graphics, inFlightValue);
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

// per stencil per draw cmd
using CircleDrawCommands = TKit::TierArray<VkDrawIndirectCommand>;

// per stencil per mesh type per asset pool per draw cmd
using MeshDrawCommands =
    TKit::FixedArray<TKit::FixedArray<TKit::TierArray<VkDrawIndexedIndirectCommand>, ONYX_MAX_ASSET_POOLS>,
                     Asset_MeshCount>;

template <Dimension D>
static void submitDrawCommands(const VKit::Queue *graphics, const u64 inFlightValue, const VkCommandBuffer cmd,
                               const RenderPass rpass, const VKit::PipelineLayout &playout,
                               const TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count> &pipelines,
                               const CircleDrawCommands &circleCmds, const MeshDrawCommands &meshCmds)
{
    const auto table = GetDeviceTable();
    u32 drawCount = circleCmds.GetSize();
    usz size = circleCmds.GetBytes();
    if (drawCount != 0)
    {
        setupState<D>(cmd, rpass, Geometry_Circle, playout, pipelines[Geometry_Circle]);
        VKit::DeviceBuffer *dbuffer = findSuitableDrawBuffer(drawCount, graphics, inFlightValue);

        dbuffer->Write(circleCmds.GetData(), {.srcOffset = 0, .dstOffset = 0, .size = size});
        ONYX_CHECK_EXPRESSION(dbuffer->Flush());
        table->CmdDrawIndirect(cmd, *dbuffer, 0, drawCount, sizeof(VkDrawIndirectCommand));
    }

    const auto renderMesh = [&](const Geometry geo) {
        setupState<D>(cmd, rpass, geo, playout, pipelines[geo]);

        const AssetType atype = getAssetType(geo);
        const TKit::Span<const u32> poolIds = Assets::GetAssetPoolIds<D>(atype);
        for (const AssetPool pid : poolIds)
        {
            const TKit::TierArray<VkDrawIndexedIndirectCommand> &cmds = meshCmds[atype][pid];
            drawCount = cmds.GetSize();
            if (drawCount == 0)
                continue;
            size = cmds.GetBytes();

            bindMeshBuffers<D>(Assets::CreateAssetPoolHandle(atype, pid), cmd);

            VKit::DeviceBuffer *dbuffer = findSuitableIndexedDrawBuffer(drawCount, graphics, inFlightValue);
            dbuffer->Write(cmds.GetData(), {.srcOffset = 0, .dstOffset = 0, .size = size});
            ONYX_CHECK_EXPRESSION(dbuffer->Flush());

            table->CmdDrawIndexedIndirect(cmd, *dbuffer, 0, drawCount, sizeof(VkDrawIndexedIndirectCommand));
        }
    };
    renderMesh(Geometry_Static);
    renderMesh(Geometry_Parametric);
    return renderMesh(Geometry_Glyph);
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

template <Dimension D>
static void renderShadows(const VKit::Queue *graphics, const VkCommandBuffer cmd, const ViewMask viewBit,
                          const u64 inFlightValue)
{
    RendererData<D> &rdata = getRendererData<D>();
    ShadowData<D> &sdata = rdata.Shadows;

    const ViewMask dirtyViews = sdata.DirtyViews;
    sdata.DirtyViews &= ~viewBit;

    const auto table = GetDeviceTable();
    const auto processLight =
        [&]<typename LightParams>(const TKit::TierArray<LightParams> &lights,
                                  const TKit::TierArray<typename LightParams::InstanceData> lightData,
                                  TextureMapArray &shadowMaps) {
            using LightData = LightParams::InstanceData;
            TKIT_ASSERT(
                lights.GetSize() == lightData.GetSize(),
                "[ONYX][RENDERER] Light count and light instance data count must match, but former has {} element(s) "
                "while latter has {}",
                lights.GetSize(), lightData.GetSize());

            for (u32 i = 0; i < lights.GetSize(); ++i)
            {
                const LightData &data = lightData[i];

                if (!(data.ViewMask & viewBit) || !(data.Flags & LightFlag_CastsShadows))
                    continue;

                const u32 viewIndex = std::popcount(data.ViewMask & (viewBit - 1));
                const u32 shindex = data.ShadowMapOffset + viewIndex;

                TextureMap &smap = shadowMaps[shindex];
                smap.Tracker.MarkInUse(graphics, inFlightValue);
                smap.Grabbed = false;

                if (!(dirtyViews & viewBit))
                    continue;

                CircleDrawCommands circleCmds{};
                MeshDrawCommands meshCmds{};
                const auto insertCommand = [&](const AssetType atype, const GraphicsInstanceRange &grange, const u32 fi,
                                               const u32 ic) {
                    if (atype == Asset_PoolCount) // circles sentry
                        circleCmds.Append(createCircleCommand(fi, ic));
                    else
                    {
                        ONYX_CHECK_ASSET_IS_NOT_NULL(grange.MeshHandle);
                        ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(grange.MeshHandle);
                        ONYX_CHECK_ASSET_POOL_IS_VALID_WITH_DIM(grange.MeshHandle, atype, D);
                        ONYX_CHECK_ASSET_IS_VALID_WITH_DIM(grange.MeshHandle, atype, D);

                        const u32 pid = Assets::GetAssetPoolId(grange.MeshHandle);
                        meshCmds[atype][pid].Append(createCommand<D>(grange.MeshHandle, fi, ic));
                    }
                };
                for (u32 i = 0; i < Geometry_Count; ++i)
                {
                    const Geometry geo = Geometry(i);
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
                        if constexpr (std::is_same_v<LightParams, PointLightParameters<D3>>)
                        {
                            pdata.LightPos = data.Position;
                            pdata.Far = data.ShadowRadius;
                            pdata.DepthBias = lights[i].DepthBias;
                        }
                        else
                        {
                            pdata.Far = 0.f;
                            pdata.DepthBias = lights[i].Cascades.DepthBias[viewIndex];
                        }
                    }

                    table->CmdPushConstants(cmd, playout, flags, 0, sizeof(ShadowPushConstantData<D>), &pdata);
                    submitDrawCommands<D>(graphics, inFlightValue, cmd, RenderPass_Shadow, playout, sdata.Pipelines,
                                          circleCmds, meshCmds);

                    endShadowPass(cmd);
                };

                if constexpr (D == D2)
                {
                    const u32 ocindex = findSuitableOcclusionMap();
                    TextureMap &ocmap = sdata.OcclusionMaps[ocindex];
                    ocmap.Tracker.MarkInUse(graphics, inFlightValue);

                    beginShadowTransitionLayout<D>(cmd, ocmap);
                    if constexpr (std::is_same_v<LightData, PointLightData<D>>)
                    {
                        const f32 r = data.ShadowRadius;
                        f32m4 pv = Transform<D3>::Orthographic(-r, r, -r, r, 0.f, 1.f);
                        pv[3][0] -= data.Position[0] * pv[0][0];
                        pv[3][1] -= data.Position[1] * pv[1][1];
                        processMap(ocmap, pv);
                    }
                    else
                        processMap(ocmap, createTransform(data.ProjectionView));
                    endShadowTransitionLayout<D>(cmd, ocmap);

                    smap.Image.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_GENERAL,
                                                 {.SrcAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                                  .DstAccess = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
                                                  .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                                  .DstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR});

                    const VkDescriptorSet set = sdata.DistanceSet;

                    sdata.DistancePipeline.Bind(cmd);
                    const VKit::PipelineLayout &playout = Pipelines::GetDistancePipelineLayout();
                    VKit::DescriptorSet::Bind(GetDevice(), cmd, set, VK_PIPELINE_BIND_POINT_COMPUTE, playout);

                    DistancePushConstantData pdata;
                    pdata.OcclusionMapIndex = ocindex;
                    pdata.OcclusionResolution = sdata.OcclusionResolution;
                    pdata.ShadowMapIndex = shindex;
                    pdata.ShadowResolution = sdata.ShadowResolution;
                    pdata.DistanceBias = lights[i].DepthBias;
                    table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                            sizeof(DistancePushConstantData), &pdata);

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
                    if constexpr (std::is_same_v<LightData, PointLightData<D>>)
                    {
                        const f32m4 proj = Transform<D3>::Perspective(0.5f * Math::Pi<f32>(), 0.01f, data.ShadowRadius);
                        const f32v3 &pos = data.Position;
                        constexpr f32v3 faceDir[6] = {
                            f32v3{1.f, 0.f, 0.f},  f32v3{-1.f, 0.f, 0.f}, f32v3{0.f, 1.f, 0.f},
                            f32v3{0.f, -1.f, 0.f}, f32v3{0.f, 0.f, 1.f},  f32v3{0.f, 0.f, -1.f},
                        };
                        constexpr f32v3 faceUp[6] = {
                            f32v3{0.f, -1.f, 0.f}, f32v3{0.f, -1.f, 0.f}, f32v3{0.f, 0.f, 1.f},
                            f32v3{0.f, 0.f, -1.f}, f32v3{0.f, -1.f, 0.f}, f32v3{0.f, -1.f, 0.f},
                        };
                        for (u32 i = 0; i < 6; ++i)
                        {
                            const f32m4 view = Transform<D3>::LookTowardsMatrix(pos, faceDir[i], faceUp[i]);
                            processMap(smap, proj * view, i);
                        }
                    }
                    else
                        for (u32 i = 0; i < data.CascadeCount; ++i)
                            processMap(smap, createTransform(data.Cascades[i]), i);
                    endShadowTransitionLayout<D>(cmd, smap);
                }
            }
        };

    const LightData<D> &ldata = rdata.Lights;
    if constexpr (D == D2)
        processLight(ldata.Instances.Points.Lights, ldata.Instances.Points.Data, sdata.ShadowMaps);
    else
    {
        processLight(ldata.Instances.Points.Lights, ldata.Instances.Points.Data, sdata.PointMaps);
        processLight(ldata.Instances.Directionals.Lights, ldata.Instances.Directionals.Data, sdata.DirectionalMaps);
    }
}

template <Dimension D>
static void renderGeometry(const VKit::Queue *graphics, const VkCommandBuffer cmd, const ViewInfo<D> &vinfo,
                           const u64 inFlightValue, TKit::StackArray<Execution::Tracker> &transferTrackers)
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
    TKit::FixedArray<MeshDrawCommands, PipelinePass_Count> meshCmds{};

    const auto insertCommand = [&](const AssetType atype, const GraphicsInstanceRange &grange, const u32 fi,
                                   const u32 ic) {
        u32 pcount = 0;
        TKit::FixedArray<PipelinePass, 3> passes;
        const RenderModeFlags rmode = grange.RenderFlags;
        if (rmode & RenderModeFlag_Shaded)
            passes[pcount++] = PipelinePass_Shaded;
        if (rmode & RenderModeFlag_Flat)
            passes[pcount++] = PipelinePass_Flat;
        if (rmode & RenderModeFlag_Outlined)
            passes[pcount++] = PipelinePass_Outlined;
        TKIT_ASSERT(pcount != 0, "[ONYX][RENDERER] Pass count should not be zero");

        if (atype == Asset_PoolCount) // circles sentry
            for (u32 i = 0; i < pcount; ++i)
                circleCmds[passes[i]].Append(createCircleCommand(fi, ic));
        else
        {
            ONYX_CHECK_ASSET_IS_NOT_NULL(grange.MeshHandle);
            ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(grange.MeshHandle);
            ONYX_CHECK_ASSET_POOL_IS_VALID_WITH_DIM(grange.MeshHandle, atype, D);
            ONYX_CHECK_ASSET_IS_VALID_WITH_DIM(grange.MeshHandle, atype, D);

            const u32 pid = Assets::GetAssetPoolId(grange.MeshHandle);
            for (u32 i = 0; i < pcount; ++i)
                meshCmds[passes[i]][atype][pid].Append(createCommand<D>(grange.MeshHandle, fi, ic));
        }
    };

    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        collectDrawInfo<D>(graphics, geo, viewBit, inFlightValue, insertCommand,
                           RenderModeFlag_Shaded | RenderModeFlag_Outlined | RenderModeFlag_Flat, &transferTrackers);
    }

    LightData<D> &ldata = rdata.Lights;
    for (u32 i = 0; i < ldata.Arenas.GetSize(); ++i)
    {
        LightArena &arena = ldata.Arenas[i];
        if (arena.LightCount == 0)
            continue;

        TKIT_ASSERT(arena.ActiveRange && arena.ActiveRange->Generation == arena.Generation,
                    "[ONYX][RENDERER] Active light graphics range arena does not have a valid generation or is just "
                    "null (forgot to call Renderer::PrepareRender()?)");

        arena.ActiveRange->GraphicsTracker.MarkInUse(graphics, inFlightValue);
    }

    const auto table = GetDeviceTable();

    ShadowData<D> &sdata = rdata.Shadows;
    for (u32 i = 0; i < PipelinePass_Count; ++i)
    {
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

            if constexpr (D == D3)
            {
                pdata.ViewPosition = vinfo.ViewPosition;
                pdata.ViewForward = vinfo.ViewForward;
                pdata.TexelSizes[Light_Point] = 1.f / f32(sdata.ShadowResolutions[Light_Point]);
                pdata.TexelSizes[Light_Directional] = 1.f / f32(sdata.ShadowResolutions[Light_Directional]);
            }
            else
                pdata.TexelSize = 1.f / f32(sdata.ShadowResolution);

            pdata.LightRanges = ldata.Ranges;
            pdata.ViewBit = viewBit;
            pdata.AmbientColor = ambientColor;
            table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                    sizeof(ShadedPushConstantData<D>), &pdata);
        }

        submitDrawCommands<D>(graphics, inFlightValue, cmd, rpass, playout, rdata.Geometry.Pipelines[pass],
                              circleCmds[pass], meshCmds[pass]);
    }
}

static RenderSubmitInfo createRenderSubmitInfo(VKit::Queue *graphics, const VkCommandBuffer command,
                                               const u64 graphicsFlight, const RenderTargetInfo &tinfo,
                                               TKit::StackArray<Execution::Tracker> &transferTrackers)
{
    RenderSubmitInfo submitInfo{};
    submitInfo.Command = command;
    submitInfo.InFlightValue = graphicsFlight;

    VkSemaphoreSubmitInfoKHR &imgInfo = submitInfo.WaitSemaphores.Append();
    imgInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    imgInfo.pNext = nullptr;
    imgInfo.semaphore = tinfo.ImageAvailableSemaphore;
    imgInfo.deviceIndex = 0;
    imgInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfoKHR &gtimSemInfo = submitInfo.SignalSemaphores.Append();
    gtimSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    gtimSemInfo.pNext = nullptr;
    gtimSemInfo.value = graphicsFlight;
    gtimSemInfo.deviceIndex = 0;
    gtimSemInfo.semaphore = graphics->GetTimelineSempahore();
    gtimSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

    VkSemaphoreSubmitInfoKHR &rendFinInfo = submitInfo.SignalSemaphores.Append();
    rendFinInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    rendFinInfo.pNext = nullptr;
    rendFinInfo.value = 0;
    rendFinInfo.semaphore = tinfo.RenderFinishedSemaphore;
    rendFinInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    rendFinInfo.deviceIndex = 0;

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
static void renderViews(const TKit::TierArray<RenderView<D> *> &views, const VkDescriptorSet ppSet,
                        VKit::Queue *graphics, const VkCommandBuffer cmd, const u64 graphicsFlight,
                        TKit::StackArray<Execution::Tracker> &transferTrackers)
{
    Execution::Tracker tracker;
    tracker.Queue = graphics;
    tracker.InFlightValue = graphicsFlight;

    RenderViewFlags flags = 0;
    for (RenderView<D> *rv : views)
    {
        flags |= rv->Flags;
        if (rv->Flags & RenderViewFlag_Shadows)
            renderShadows<D>(graphics, cmd, rv->GetViewBit(), graphicsFlight);

        rv->CacheMatrices();
        rv->BeginRendering(cmd, tracker);
        renderGeometry<D>(graphics, cmd, rv->CreateViewInfo(), graphicsFlight, transferTrackers);
        rv->EndRendering(cmd);
    }

    if (flags & RenderViewFlag_PostProcess)
    {
        const auto table = GetDeviceTable();
        const VKit::PipelineLayout &playout = Pipelines::GetPostProcessPipelineLayout();
        VKit::DescriptorSet::Bind(GetDevice(), cmd, ppSet, VK_PIPELINE_BIND_POINT_GRAPHICS, playout);
        for (RenderView<D> *rv : views)
            if (rv->Flags & RenderViewFlag_PostProcess)
            {
                rv->BeginPostProcess(cmd);
                s_PostProcessPipeline.Bind(cmd);

                PostProcessPushConstantData pdata;
                pdata.AttachmentIndex = rv->GetDescriptorIndex();

                const VkExtent2D extent = rv->GetExtent();
                pdata.Extent = u32v2{extent.width, extent.height};
                pdata.MaxOutlineWidth = rv->MaxOutlineWidth;
                pdata.Flags = rv->Flags;

                table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                        sizeof(PostProcessPushConstantData), &pdata);
                table->CmdDraw(cmd, 6, 1, 0, 0);
                rv->EndPostProcess(cmd);
            }
    }
}

template <Dimension D>
static void renderCompositor(const TKit::TierArray<RenderView<D> *> &views, const VkCommandBuffer cmd,
                             const VKit::PipelineLayout &playout)
{
    const auto table = GetDeviceTable();
    for (const RenderView<D> *rv : views)
    {
        const VkViewport vp = rv->GetVulkanViewport();
        const VkRect2D sc = rv->GetVulkanScissor();
        const u32 idx = rv->GetDescriptorIndex();

        table->CmdSetViewport(cmd, 0, 1, &vp);
        table->CmdSetScissor(cmd, 0, 1, &sc);
        table->CmdPushConstants(cmd, playout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(u32), &idx);
        table->CmdDraw(cmd, 6, 1, 0, 0);
    }
}

RenderSubmitInfo Render(VKit::Queue *graphics, const VkCommandBuffer cmd, Window *window, const RenderFlags flags)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Render");

    const u64 graphicsFlight = graphics->NextTimelineValue();
    const RenderTargetInfo tinfo = window->CreateRenderTargetInfo();

    TKit::StackArray<Execution::Tracker> transferTrackers{};
    transferTrackers.Reserve(s_SyncPointCount);

    // TODO(Isma): Pass here the render flags and & it with the render view flag, and send that flag as push constant to
    // geometry to globally disable shadows
    renderViews(tinfo.Views2, window->GetPostProcessSet(), graphics, cmd, graphicsFlight, transferTrackers);
    renderViews(tinfo.Views3, window->GetPostProcessSet(), graphics, cmd, graphicsFlight, transferTrackers);

    Execution::Tracker tracker;
    tracker.Queue = graphics;
    tracker.InFlightValue = graphicsFlight;
    window->BeginRendering(cmd, tracker);

    s_CompositorPipeline.Bind(cmd);

    const VkDescriptorSet set = window->GetCompositorSet();
    const VKit::PipelineLayout &playout = Pipelines::GetCompositorPipelineLayout();
    VKit::DescriptorSet::Bind(GetDevice(), cmd, set, VK_PIPELINE_BIND_POINT_GRAPHICS, playout);

    renderCompositor(tinfo.Views2, cmd, playout);
    renderCompositor(tinfo.Views3, cmd, playout);

#ifdef ONYX_ENABLE_IMGUI
    if (flags & RenderFlag_ImGui)
    {
        ImGui::Render();
        ImGuiBackend::RenderData(ImGui::GetDrawData(), cmd);
        ImGuiBackend::UpdatePlatformWindows();
    }
#endif

    window->EndRendering(cmd);
    return createRenderSubmitInfo(graphics, cmd, graphicsFlight, tinfo, transferTrackers);
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
    ONYX_CHECK_EXPRESSION(graphics->Submit2(submits));
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
        coalesceRanges(arena.Graphics, [&arena](const GraphicsLightRange &grange) {
            return grange.Generation == arena.Generation || grange.InUse();
        });
    }

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
    const auto fmts = [](const VkDeviceSize bytes) -> std::string {
        if (bytes > 1_gib)
            return TKit::Format("{:.2f} gib", f32(bytes) / f32(1_gib));
        if (bytes > 1_mib)
            return TKit::Format("{:.2f} mib", f32(bytes) / f32(1_mib));
        if (bytes > 1_kib)
            return TKit::Format("{:.2f} kib", f32(bytes) / f32(1_kib));
        return TKit::Format("{:L} b", bytes);
    };

    const auto fmtb = [](const VkDeviceSize bytes) -> std::string { return TKit::Format("{:L} b", bytes); };

    if (ImGui::TreeNode(&pool, "%s pool ranges (%u)", name, pool.Ranges.GetSize()))
    {
        ImGui::Text("Buffer size: %s", fmts(pool.Buffer.GetInfo().Size).c_str());
        for (const Range &range : pool.Ranges)
            if constexpr (std::is_same_v<Range, TransferInstanceRange> || std::is_same_v<Range, TransferLightRange>)
                ImGui::Text("%s (%s): %s - %s", range.Tracker.InUse() ? "IN-USE" : "FREE", fmts(range.Size).c_str(),
                            fmtb(range.Offset).c_str(), fmtb(range.Offset + range.Size).c_str());
            else if constexpr (std::is_same_v<Range, GraphicsInstanceRange>)
            {
                if (ImGui::TreeNode(
                        &range, "%s (%s): %s - %s",
                        range.InUse() ? "IN-USE"
                                      : (rdata.AreAllContextRangesDirty(range)
                                             ? "FREE"
                                             : (rdata.AreAllContextRangesClean(range) ? "CLEAN" : "FRAGMENTED")),
                        fmts(range.Size).c_str(), fmtb(range.Offset).c_str(), fmtb(range.Offset + range.Size).c_str()))
                {
                    ImGui::Text("In use by transfer queue: %s", range.TransferTracker.InUse() ? "YES" : "NO");
                    ImGui::Text("In use by graphics queue: %s", range.GraphicsTracker.InUse() ? "YES" : "NO");
                    if (range.MeshHandle != NullHandle)
                        ImGui::Text("Mesh handle: %u", range.MeshHandle);

                    ImGui::Text("Render mode: %s", ToString(GetRenderMode(range.RenderFlags)));
                    const std::string vmask = TKit::Format("{:032b}", range.ViewMask);
                    ImGui::Text("View mask: %s", vmask.c_str());
                    if (ImGui::TreeNode(&range.ContextRanges, "Context ranges (%u)", range.ContextRanges.GetSize()))
                    {
                        for (const ContextInstanceRange &crange : range.ContextRanges)
                            if (ImGui::TreeNode(&crange, "%s (%s): %s - %s",
                                                rdata.IsContextRangeClean(crange) ? "CLEAN" : "DIRTY",
                                                fmts(crange.Size).c_str(), fmtb(crange.Offset).c_str(),
                                                fmtb(crange.Offset + crange.Size).c_str()))
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

                                const std::string cvmask = TKit::Format("{:032b}", crange.ViewMask);
                                ImGui::Text("View mask: %s", cvmask.c_str());
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
                ImGui::Text(
                    "%s (%s): %s - %s", range.InUse() ? "IN-USE" : (range.Generation == generation ? "CLEAN" : "FREE"),
                    fmts(range.Size).c_str(), fmtb(range.Offset).c_str(), fmtb(range.Offset + range.Size).c_str());
        ImGui::TreePop();
        ImGui::Spacing();
    }
}

#    ifdef ONYX_ENABLE_IMPLOT
template <Dimension D, typename TRange, typename GRange>
static void plotRanges(const Pool<TRange> &tpool, const Pool<GRange> &gpool, const u64 generation = 0)
{
    const RendererData<D> &rdata = getRendererData<D>();
    const auto fmts = [](const VkDeviceSize bytes) -> std::string {
        if (bytes > 1_gib)
            return TKit::Format("{:.2f} gib", f32(bytes) / f32(1_gib));
        if (bytes > 1_mib)
            return TKit::Format("{:.2f} mib", f32(bytes) / f32(1_mib));
        if (bytes > 1_kib)
            return TKit::Format("{:.2f} kib", f32(bytes) / f32(1_kib));
        return TKit::Format("{:L} b", bytes);
    };

    const auto fmtb = [](const VkDeviceSize bytes) -> std::string { return TKit::Format("{:L} b", bytes); };
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
                                  const Asset meshHandle = NullHandle, const RenderMode rmode = RenderMode_None) {
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
                    ImGui::Text("%s - Offset: %s - Size: %s", lbl, fmtb(offset).c_str(), fmts(size).c_str());
                    if (rmode != RenderMode_None)
                    {
                        ImGui::SameLine();
                        ImGui::Text("- Mesh handle: %s - Render mode: %s", TKit::Format("{:#010x}", meshHandle).c_str(),
                                    ToString(rmode));
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
                drawPlot(1, range.Offset, range.Size, idx, range.MeshHandle, GetRenderMode(range.RenderFlags));
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
            const u32 stsize = status.GetSize() - 2 * std::is_same_v<GRange, GraphicsLightRange>;
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
            plotRanges<D>(arena.Transfer, arena.Graphics);
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
            displayRanges<D>("Graphics", arena.Graphics, arena.Generation);
            plotRanges<D>(arena.Transfer, arena.Graphics, arena.Generation);
            ImGui::TreePop();
            ImGui::Spacing();
        }
    }
    ImGui::PopID();
}

template void DisplayMemoryLayout<D2>();
template void DisplayMemoryLayout<D3>();
#endif

template const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets<D2>(RenderPass rpass);
template const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets<D3>(RenderPass rpass);

template RenderContext<D2> *CreateContext();
template RenderContext<D3> *CreateContext();

template void DestroyContext(RenderContext<D2> *context);
template void DestroyContext(RenderContext<D3> *context);

template void BindBuffer<D2>(u32 binding, TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass,
                             u32 dstElement);
template void BindBuffer<D3>(u32 binding, TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass,
                             u32 dstElement);

template void BindImage<D2>(u32 binding, TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement);
template void BindImage<D3>(u32 binding, TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement);

} // namespace Onyx::Renderer
