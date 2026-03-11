#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/rendering/context.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/profiling/macros.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include <imgui.h>
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
    u32 BatchIndex = TKIT_U32_MAX;
    StencilPass Pass = StencilPass_Count;
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
using LightDrawInfo = DrawInfo;

struct LightArena
{
    TransferLightPool Transfer{};
    GraphicsLightPool Graphics{};
    u64 Generation = 1;
    LightDrawInfo DrawInfo{};
};

template <Dimension D> struct RendererData
{
    TKit::TierArray<RenderContext<D> *> Contexts{};
    TKit::TierArray<u64> Generations{};
    TKit::TierArray<VkBufferMemoryBarrier2KHR> AcquireBarriers{};
    TKit::FixedArray<InstanceArena, Geometry_Count> InstanceArenas{};
    TKit::FixedArray<LightArena, LightTypeCount<D>> LightArenas{};
    TKit::FixedArray<TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count>, StencilPass_Count> Pipelines{};
    TKit::FixedArray<TKit::FixedArray<VkDescriptorSet, Geometry_Count>, Shading_Count> Descriptors{};

    bool IsContextRangeClean(const ContextInstanceRange &crange) const
    {
        return crange.ContextIndex != TKIT_U32_MAX && !Contexts[crange.ContextIndex]->IsDirty(crange.Generation);
    }
    bool IsContextRangeClean(const ViewMask viewBit, const ContextInstanceRange &crange) const
    {
        return (crange.ViewMask & viewBit) && crange.ContextIndex != TKIT_U32_MAX &&
               !Contexts[crange.ContextIndex]->IsDirty(crange.Generation);
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

static TKit::Storage<RendererData<D2>> s_RendererData2{};
static TKit::Storage<RendererData<D3>> s_RendererData3{};

template <Dimension D> static RendererData<D> &getRendererData()
{
    if constexpr (D == D2)
        return *s_RendererData2;
    else
        return *s_RendererData3;
}

template <Dimension D> static u32 getInstanceSize(const Geometry geo)
{
    switch (geo)
    {
    case Geometry_Circle:
        return sizeof(CircleInstanceData<D>);
    case Geometry_StaticMesh:
        return sizeof(InstanceData<D>);
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized geometry type");
        return 0;
    }
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

VkPipelineRenderingCreateInfoKHR CreatePipelineRenderingCreateInfo()
{
    VkPipelineRenderingCreateInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &Window::SurfaceFormat.format;
    renderInfo.depthAttachmentFormat = Window::DepthStencilFormat;
    renderInfo.stencilAttachmentFormat = Window::DepthStencilFormat;
    return renderInfo;
}

static void updateDescriptorSet(const VkDescriptorSet set, const u32 binding, const VKit::DescriptorSetLayout &layout,
                                const VKit::DeviceBuffer &buffer)
{
    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &layout};
    const VkDescriptorBufferInfo info = buffer.CreateDescriptorInfo();
    writer.WriteBuffer(binding, &info);
    writer.Overwrite(set);
}

template <Dimension D> static void updateInstanceDescriptorSets(const Geometry geo)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < Shading_Count; ++i)
    {
        const Shading shading = Shading(i);
        updateDescriptorSet(rdata.Descriptors[i][geo], 0, Descriptors::GetDescriptorSetLayout<D>(shading),
                            rdata.InstanceArenas[geo].Graphics.Buffer);
    }
}

static u32 lightToBinding(const LightType light)
{
    return light + 4;
}

template <Dimension D> static void updateLightDescriptorSets(const LightType light)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < Geometry_Count; ++i)
        updateDescriptorSet(rdata.Descriptors[Shading_Lit][i], lightToBinding(light),
                            Descriptors::GetDescriptorSetLayout<D>(Shading_Lit),
                            rdata.LightArenas[light].Graphics.Buffer);
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
ONYX_NO_DISCARD static Result<VKit::DeviceBuffer> createTransferInstanceBuffer(
    const Geometry geo, const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    auto result = Resources::CreateBuffer(getStageFlags(), getInstanceSize<D>(geo) * instances);
    TKIT_RETURN_ON_ERROR(result);
    VKit::DeviceBuffer &buffer = result.GetValue();

    if (Core::CanNameObjects())
    {
        const std::string name =
            TKit::Format("onyx-renderer-transfer-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
        TKIT_RETURN_IF_FAILED(buffer.SetName(name.c_str()));
    }
    return result;
}

template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::DeviceBuffer> createGraphicsInstanceBuffer(
    const Geometry geo, const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    auto result = Resources::CreateBuffer(getDeviceLocalFlags(), instances * getInstanceSize<D>(geo));
    TKIT_RETURN_ON_ERROR(result);
    VKit::DeviceBuffer &buffer = result.GetValue();

    if (Core::CanNameObjects())
    {
        const std::string name =
            TKit::Format("onyx-renderer-graphics-instance-buffer-{}D-geometry-{}", u8(D), ToString(geo));
        TKIT_RETURN_IF_FAILED(buffer.SetName(name.c_str()));
    }
    return result;
}

template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::DeviceBuffer> createTransferLightBuffer(
    const LightType light, const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    auto result = Resources::CreateBuffer(Buffer_Staging, instances * getLightSize<D>(light));
    TKIT_RETURN_ON_ERROR(result);
    VKit::DeviceBuffer &buffer = result.GetValue();

    if (Core::CanNameObjects())
    {
        const std::string name =
            TKit::Format("onyx-renderer-transfer-light-buffer-{}D-type-{}", u8(D), ToString(light));
        TKIT_RETURN_IF_FAILED(buffer.SetName(name.c_str()));
    }
    return result;
}

template <Dimension D>
ONYX_NO_DISCARD static Result<VKit::DeviceBuffer> createGraphicsLightBuffer(
    const LightType light, const u32 instances = ONYX_BUFFER_INITIAL_CAPACITY)
{
    auto result = Resources::CreateBuffer(Buffer_DeviceStorage, instances * getLightSize<D>(light));
    TKIT_RETURN_ON_ERROR(result);
    VKit::DeviceBuffer &buffer = result.GetValue();

    if (Core::CanNameObjects())
    {
        const std::string name =
            TKit::Format("onyx-renderer-graphics-light-buffer-{}D-type-{}", u8(D), ToString(light));
        TKIT_RETURN_IF_FAILED(buffer.SetName(name.c_str()));
    }
    return result;
}

template <Dimension D> ONYX_NO_DISCARD static Result<> initialize()
{
    RendererData<D> &rdata = getRendererData<D>();

    const VkPipelineRenderingCreateInfoKHR renderInfo = CreatePipelineRenderingCreateInfo();
    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        TransferInstancePool &tpool = rdata.InstanceArenas[geo].Transfer;

        auto result = createTransferInstanceBuffer<D>(geo);
        TKIT_RETURN_ON_ERROR(result);
        tpool.Buffer = result.GetValue();

        tpool.Ranges.Append(TransferInstanceRange{.Size = tpool.Buffer.GetInfo().Size});

        GraphicsInstancePool &gpool = rdata.InstanceArenas[geo].Graphics;

        result = createGraphicsInstanceBuffer<D>(geo);
        TKIT_RETURN_ON_ERROR(result);
        gpool.Buffer = result.GetValue();

        gpool.Ranges.Append(GraphicsInstanceRange{.Size = gpool.Buffer.GetInfo().Size});
        for (u32 j = 0; j < Shading_Count; ++j)
        {
            const Shading shading = Shading(j);
            const VKit::DescriptorSetLayout &layout = Descriptors::GetDescriptorSetLayout<D>(shading);
            const auto dresult = Descriptors::GetDescriptorPool().Allocate(layout);
            TKIT_RETURN_ON_ERROR(dresult);
            const VkDescriptorSet set = dresult.GetValue();
            if (Core::CanNameObjects())
            {
                const auto &device = Core::GetDevice();
                const std::string name = TKit::Format("onyx-renderer-descriptor-set-{}D-geometry-{}-shading-{}", u8(D),
                                                      ToString(geo), ToString(shading));
                TKIT_RETURN_IF_FAILED(device.SetObjectName(set, VK_OBJECT_TYPE_DESCRIPTOR_SET, name.c_str()));
            }

            updateDescriptorSet(set, 0, layout, gpool.Buffer);
            rdata.Descriptors[j][i] = set;
        }
    }
    for (u32 i = 0; i < StencilPass_Count; ++i)
    {
        const StencilPass pass = StencilPass(i);

        auto result = Pipelines::CreateCirclePipeline<D>(pass, renderInfo);
        TKIT_RETURN_ON_ERROR(result);
        rdata.Pipelines[pass][Geometry_Circle] = result.GetValue();

        result = Pipelines::CreateStaticMeshPipeline<D>(pass, renderInfo);
        TKIT_RETURN_ON_ERROR(result);
        rdata.Pipelines[pass][Geometry_StaticMesh] = result.GetValue();

        if (Core::CanNameObjects())
        {
            const std::string circle =
                TKit::Format("onyx-renderer-pipeline-{}D-pass-{}-geometry-Geometry_Circle", u8(D), ToString(pass));
            const std::string mesh = TKit::Format("onyx-renderer-pipeline-{}D-pass-{}-geometry-'Geometry_StaticMesh'",
                                                  u8(D), ToString(pass));

            TKIT_RETURN_IF_FAILED(rdata.Pipelines[pass][Geometry_Circle].SetName(circle.c_str()));
            TKIT_RETURN_IF_FAILED(rdata.Pipelines[pass][Geometry_StaticMesh].SetName(mesh.c_str()));
        }
    }

    for (u32 i = 0; i < LightTypeCount<D>; ++i)
    {
        const LightType light = LightType(i);
        TransferLightPool &tpool = rdata.LightArenas[light].Transfer;

        auto result = createTransferLightBuffer<D>(light);
        TKIT_RETURN_ON_ERROR(result);
        tpool.Buffer = result.GetValue();

        tpool.Ranges.Append(TransferLightRange{.Size = tpool.Buffer.GetInfo().Size});

        GraphicsLightPool &gpool = rdata.LightArenas[light].Graphics;

        result = createGraphicsLightBuffer<D>(light);
        TKIT_RETURN_ON_ERROR(result);
        gpool.Buffer = result.GetValue();

        gpool.Ranges.Append(GraphicsLightRange{.Size = gpool.Buffer.GetInfo().Size});
        updateLightDescriptorSets<D>(light);
    }

    return Result<>::Ok();
}

template <Dimension D> static void terminate()
{
    RendererData<D> &rdata = getRendererData<D>();
    ONYX_CHECK_EXPRESSION(Core::DeviceWaitIdle());
    for (InstanceArena &arena : rdata.InstanceArenas)
    {
        arena.Transfer.Buffer.Destroy();
        arena.Graphics.Buffer.Destroy();
    }
    for (LightArena &arena : rdata.LightArenas)
    {
        arena.Transfer.Buffer.Destroy();
        arena.Graphics.Buffer.Destroy();
    }
    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            rdata.Pipelines[pass][geo].Destroy();

    TKit::TierAllocator *tier = TKit::GetTier();
    for (RenderContext<D> *context : rdata.Contexts)
        tier->Destroy(context);
}

Result<> Initialize()
{
    TKIT_LOG_INFO("[ONYX][RENDERER] Initializing");
    s_RendererData2.Construct();
    s_RendererData3.Construct();
    TKIT_RETURN_IF_FAILED(initialize<D2>());
    return initialize<D3>();
}
void Terminate()
{
    terminate<D2>();
    terminate<D3>();
    s_RendererData2.Destruct();
    s_RendererData3.Destruct();
}

template <Dimension D> Result<RenderContext<D> *> CreateContext()
{
    RendererData<D> &rdata = getRendererData<D>();
    TKit::TierAllocator *tier = TKit::GetTier();
    RenderContext<D> *ctx = tier->Create<RenderContext<D>>();
    rdata.Contexts.Append(ctx);
    rdata.Generations.Append(ctx->GetGeneration());
    return ctx;
}

template <Dimension D> static u32 getContextIndex(const RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < rdata.Contexts.GetSize(); ++i)
        if (rdata.Contexts[i] == context)
            return i;
    TKIT_FATAL("[ONYX][RENDERER] Render context ({}) index not found", scast<const void *>(context));
    return TKIT_U32_MAX;
}

template <Dimension D> void DestroyContext(RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u32 index = getContextIndex(context);
    rdata.Generations.RemoveOrdered(rdata.Generations.begin() + index);
    for (InstanceArena &arena : rdata.InstanceArenas)
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

template <Dimension D> void UpdateViewMask(const RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u32 index = getContextIndex(context);
    const ViewMask vmask = context->GetViewMask();
    for (InstanceArena &arena : rdata.InstanceArenas)
        for (GraphicsInstanceRange &grange : arena.Graphics.Ranges)
        {
            ViewMask gmask = 0;
            for (ContextInstanceRange &crange : grange.ContextRanges)
            {
                if (crange.ContextIndex == index)
                    crange.ViewMask = vmask;
                gmask |= crange.ViewMask;
            }
            grange.ViewMask = gmask;
        }

    for (PointLight<D> *pl : context->GetPointLights())
        pl->SetViewMask(vmask);
    if constexpr (D == D3)
        for (DirectionalLight *dl : context->GetDirectionalLights())
            dl->SetViewMask(vmask);
}

template <Dimension D> const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets(const Shading shading)
{
    return getRendererData<D>().Descriptors[shading];
}

template <Dimension D> static void clearViews(const ViewMask viewMask)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (RenderContext<D> *ctx : rdata.Contexts)
        ctx->RemoveTarget(viewMask);

    for (InstanceArena &arena : rdata.InstanceArenas)
        for (GraphicsInstanceRange &grange : arena.Graphics.Ranges)
        {
            for (ContextInstanceRange &crange : grange.ContextRanges)
                crange.ViewMask &= ~viewMask;
            grange.ViewMask &= ~viewMask;
        }
}

void ClearViews(const ViewMask viewMask)
{
    clearViews<D2>(viewMask);
    clearViews<D3>(viewMask);
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
    for (const InstanceArena &arena : rdata.InstanceArenas)
    {
        validateRanges("transfer instance", arena.Transfer);
        validateGraphicsInstanceRanges(arena.Graphics);
    }
    for (const LightArena &arena : rdata.LightArenas)
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
ONYX_NO_DISCARD static Result<Range *> handlePoolResize(const VkDeviceSize requiredMem,
                                                        Result<VKit::DeviceBuffer> &result, VKit::DeviceBuffer &buffer,
                                                        TKit::TierArray<Range> &ranges,
                                                        TKit::StackArray<Task> *tasks = nullptr,
                                                        VKit::Queue *transfer = nullptr)
{
    TKIT_RETURN_ON_ERROR(result);

    VKit::DeviceBuffer &nbuffer = result.GetValue();
    const VkDeviceSize size = buffer.GetInfo().Size;
    const VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = size};

    if (tasks)
    {
        for (const Task &task : *tasks)
            task.WaitUntilFinished();
        tasks->Clear();
    }

    TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());

    if constexpr (std::is_same_v<Range, GraphicsInstanceRange>)
    {
        TKIT_ASSERT(transfer);
        TKIT_RETURN_IF_FAILED(nbuffer.CopyFromBuffer(Execution::GetTransientTransferPool(), *transfer, buffer, copy));
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
        range.BatchIndex = TKIT_U32_MAX;
        range.Pass = StencilPass_Count;
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
ONYX_NO_DISCARD static Result<TransferInstanceRange *> findTransferInstanceRange(const Geometry geo,
                                                                                 TransferInstancePool &pool,
                                                                                 const VkDeviceSize requiredMem,
                                                                                 TKit::StackArray<Task> &tasks)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindTransferInstanceRange");
    auto &ranges = pool.Ranges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    for (u32 i = 0; i < ranges.GetSize(); ++i)
        if (ranges[i].Size >= requiredMem && !ranges[i].Tracker.InUse())
            return splitRange(i, ranges, requiredMem);

    VKit::DeviceBuffer &buffer = pool.Buffer;
    const u32 icount = computeNewInstanceCount(getInstanceSize<D>(geo), buffer, requiredMem);

    auto bresult = createTransferInstanceBuffer<D>(geo, icount);
    return handlePoolResize<D>(requiredMem, bresult, buffer, ranges, &tasks);
}

template <Dimension D>
ONYX_NO_DISCARD static Result<GraphicsInstanceRange *> findGraphicsInstanceRange(const Geometry geo,
                                                                                 GraphicsInstancePool &pool,
                                                                                 const VkDeviceSize requiredMem,
                                                                                 VKit::Queue *transfer,
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
    const u32 icount = computeNewInstanceCount(getInstanceSize<D>(geo), buffer, requiredMem);
    for (u32 i = rdata.AcquireBarriers.GetSize() - 1; i < rdata.AcquireBarriers.GetSize(); --i)
    {
        const VkBufferMemoryBarrier2KHR &barrier = rdata.AcquireBarriers[i];
        if (barrier.buffer == buffer.GetHandle())
            rdata.AcquireBarriers.RemoveUnordered(rdata.AcquireBarriers.begin() + i);
    }

    auto bresult = createGraphicsInstanceBuffer<D>(geo, icount);
    const auto result = handlePoolResize<D>(requiredMem, bresult, buffer, ranges, &tasks, transfer);
    TKIT_RETURN_ON_ERROR(result);
    updateInstanceDescriptorSets<D>(geo);
    return result;
}

template <Dimension D>
ONYX_NO_DISCARD static Result<TransferLightRange *> findTransferLightRange(const LightType light,
                                                                           TransferLightPool &pool,
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
ONYX_NO_DISCARD static Result<GraphicsLightRange *> findGraphicsLightRange(const LightType light,
                                                                           GraphicsLightPool &pool,
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

    auto bresult = createGraphicsLightBuffer<D>(light, icount);
    const auto result = handlePoolResize<D>(requiredMem, bresult, buffer, ranges);
    TKIT_RETURN_ON_ERROR(result);
    updateLightDescriptorSets<D>(light);
    return result;
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

template <Dimension D> struct LightStagingData
{
    TKit::TierArray<const PointLight<D> *> Points{};
};

template <> struct LightStagingData<D3>
{
    TKit::TierArray<const PointLight<D3> *> Points{};
    TKit::TierArray<const DirectionalLight *> Dirs{};
};

template <Dimension D>
ONYX_NO_DISCARD static Result<> transfer(VKit::Queue *transfer, const VkCommandBuffer command, TransferSubmitInfo &info,
                                         TKit::StackArray<VkBufferMemoryBarrier2KHR> *release,
                                         const u64 transferFlightValue)
{
    struct ContextInfo
    {
        const RenderContext<D> *Context;
        u32 Index;
    };

    RendererData<D> &rdata = getRendererData<D>();
    const TKit::TierArray<RenderContext<D> *> &contexts = rdata.Contexts;
    if (contexts.IsEmpty())
        return Result<>::Ok();

    TKit::StackArray<ContextInfo> dirtyContexts{};
    dirtyContexts.Reserve(rdata.Contexts.GetSize());

    using LightFlags = u8;
    enum LightFlagBit : LightFlags
    {
        LightFlag_Point = 1 << 0,
        LightFlag_Directional = 1 << 1,
    };

    LightFlags toUpdate = 0;
    LightStagingData<D> lights{};
    for (u32 i = 0; i < contexts.GetSize(); ++i)
    {
        RenderContext<D> *ctx = contexts[i];
        if (ctx->IsDirty(rdata.Generations[i]))
        {
            dirtyContexts.Append(ctx, i);
            rdata.Generations[i] = ctx->GetGeneration();
        }

        for (PointLight<D> *pl : ctx->GetPointLights())
        {
            toUpdate |= LightFlags(LightFlag_Point) * pl->IsDirty();
            pl->MarkNonDirty();
            lights.Points.Append(pl);
        }
        if constexpr (D == D3)
            for (DirectionalLight *dl : ctx->GetDirectionalLights())
            {
                toUpdate |= LightFlags(LightFlag_Directional) * dl->IsDirty();
                dl->MarkNonDirty();
                lights.Dirs.Append(dl);
            }
    }

    const u32 npcount = lights.Points.GetSize();
    u32 &pcount = rdata.LightArenas[Light_Point].DrawInfo.InstanceCount;
    if (pcount != npcount)
    {
        toUpdate |= LightFlag_Point;
        pcount = npcount;
    }
    if constexpr (D == D3)
    {
        const u32 ndcount = lights.Dirs.GetSize();
        u32 &dcount = rdata.LightArenas[Light_Directional].DrawInfo.InstanceCount;
        if (dcount != ndcount)
        {
            toUpdate |= LightFlag_Directional;
            dcount = ndcount;
        }
    }

    const auto copyLightRanges = [&]<typename Light>(const LightType light,
                                                     const TKit::TierArray<const Light *> &lights) -> Result<> {
        info.Command = command;
        LightArena &arena = rdata.LightArenas[light];
        TransferLightPool &tpool = arena.Transfer;
        GraphicsLightPool &gpool = arena.Graphics;
        using LightData = typename Light::InstanceData;
        TKit::StackArray<LightData> data{};

        data.Reserve(lights.GetSize());
        const VkDeviceSize requiredMem = sizeof(LightData) * data.GetCapacity();
        for (const Light *light : lights)
            data.Append(light->CreateInstanceData());

        const auto tresult = findTransferLightRange<D>(light, tpool, requiredMem);
        TKIT_RETURN_ON_ERROR(tresult);

        TransferLightRange *trange = tresult.GetValue();
        trange->Tracker.MarkInUse(transfer, transferFlightValue);
        tpool.Buffer.Write(data.GetData(), {.srcOffset = 0, .dstOffset = trange->Offset, .size = trange->Size});

        const auto gresult = findGraphicsLightRange<D>(light, gpool, requiredMem);
        TKIT_RETURN_ON_ERROR(gresult);

        GraphicsLightRange *grange = gresult.GetValue();
        grange->TransferTracker.MarkInUse(transfer, transferFlightValue);
        grange->GraphicsTracker = {};
        grange->Generation = ++arena.Generation;
        arena.DrawInfo.FirstInstance = grange->Offset / getLightSize<D>(light);

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

        return Result<>::Ok();
    };

    if ((toUpdate & LightFlag_Point) && !lights.Points.IsEmpty())
        copyLightRanges(Light_Point, lights.Points);

    if constexpr (D == D3)
        if ((toUpdate & LightFlag_Directional) && !lights.Dirs.IsEmpty())
            copyLightRanges(Light_Directional, lights.Dirs);

    if (dirtyContexts.IsEmpty())
        return Result<>::Ok();

    TKit::StackArray<VkBufferCopy2KHR> copies{};
    copies.Reserve(Assets::GetBatchCount());

    TKit::StackArray<ContextInstanceRange> contextRanges{};
    contextRanges.Reserve(dirtyContexts.GetSize());

    TKit::ITaskManager *tm = Core::GetTaskManager();

    TKit::StackArray<Task> tasks{};
    tasks.Reserve(dirtyContexts.GetSize() * Assets::GetBatchCount());

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
    ranges.Reserve(StencilPass_Count * Assets::GetBatchCount());

    TKit::StackArray<CopyCommands> copyCmds{};
    copyCmds.Reserve(StencilPass_Count * u32(Geometry_Count));

    const auto finishTasks = [&] {
        for (const Task &task : tasks)
            tm->WaitUntilFinished(task);
    };

    const auto findInstanceRanges = [&](const u32 pass, const Geometry geo) -> Result<> {
        TKIT_PROFILE_NSCOPE("Onyx::Renderer::FindRanges");
        TransferInstancePool &tpool = rdata.InstanceArenas[geo].Transfer;
        GraphicsInstancePool &gpool = rdata.InstanceArenas[geo].Graphics;

        const u32 bstart = Assets::GetBatchStart(geo);
        const u32 bend = Assets::GetBatchEnd(geo);

        CopyCommands copyCmd{};
        copyCmd.Transfer = &tpool.Buffer;
        copyCmd.Graphics = &gpool.Buffer;
        copyCmd.Offset = copies.GetSize();

        for (u32 batch = bstart; batch < bend; ++batch)
        {
            contextRanges.Clear();
            VkDeviceSize requiredMem = 0;
            ViewMask viewMask = 0;
            for (const ContextInfo &cinfo : dirtyContexts)
            {
                const RenderContext<D> *ctx = cinfo.Context;
                const auto &idata = ctx->GetInstanceData()[pass][batch];
                if (idata.Instances == 0)
                    continue;

                ContextInstanceRange &crange = contextRanges.Append();
                crange.ContextIndex = cinfo.Index;
                crange.Offset = requiredMem;
                crange.Size = idata.Instances * idata.InstanceSize;
                crange.Generation = ctx->GetGeneration();

                const ViewMask vm = ctx->GetViewMask();
                viewMask |= vm;
                crange.ViewMask = vm;

                requiredMem += crange.Size;
            }
            if (requiredMem == 0)
                continue;

            const auto tresult = findTransferInstanceRange<D>(geo, tpool, requiredMem, tasks);
            TKIT_RETURN_ON_ERROR(tresult);
            TransferInstanceRange *trange = tresult.GetValue();
            trange->Tracker.MarkInUse(transfer, transferFlightValue);

            for (const ContextInstanceRange &crange : contextRanges)
            {
                const RenderContext<D> *ctx = contexts[crange.ContextIndex];

                const auto &idata = ctx->GetInstanceData()[pass][batch];
                const auto copy = [&, crange, trange = *trange] {
                    TKIT_PROFILE_NSCOPE("Onyx::Renderer::HostCopy");
                    tpool.Buffer.Write(
                        idata.Data.GetData(),
                        {.srcOffset = 0, .dstOffset = trange.Offset + crange.Offset, .size = crange.Size});
                };

                Task &task = tasks.Append(copy);
                sindex = tm->SubmitTask(&task, sindex);
            }

            const auto gresult = findGraphicsInstanceRange<D>(geo, gpool, requiredMem, transfer, tasks);
            TKIT_RETURN_ON_ERROR(gresult);
            GraphicsInstanceRange *grange = gresult.GetValue();
            grange->BatchIndex = batch;
            grange->ContextRanges = contextRanges;
            grange->ViewMask = viewMask;
            grange->Pass = StencilPass(pass);
            grange->TransferTracker.MarkInUse(transfer, transferFlightValue);
            grange->GraphicsTracker = {};

            VkBufferCopy2KHR &copy = copies.Append();
            copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
            copy.pNext = nullptr;
            copy.srcOffset = trange->Offset;
            copy.dstOffset = grange->Offset;
            copy.size = requiredMem;

            ranges.Append(&gpool.Buffer, grange->Offset, requiredMem);
        }
        copyCmd.Size = copies.GetSize() - copyCmd.Offset;
        if (copyCmd.Size != 0)
            copyCmds.Append(copyCmd);

        return Result<>::Ok();
    };

    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
    {
        TKIT_RETURN_IF_FAILED(findInstanceRanges(pass, Geometry_Circle), finishTasks());
        TKIT_RETURN_IF_FAILED(findInstanceRanges(pass, Geometry_StaticMesh), finishTasks());
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
    return Result<>::Ok();
}

Result<TransferSubmitInfo> Transfer(VKit::Queue *tqueue, const VkCommandBuffer command, const u32 maxReleaseBarriers)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Transfer");
    TransferSubmitInfo submitInfo{};
    const bool separate = Execution::IsSeparateTransferMode();
    TKit::StackArray<VkBufferMemoryBarrier2KHR> release{};
    if (separate)
        release.Reserve(maxReleaseBarriers);

    const u64 transferFlight = tqueue->NextTimelineValue();

    TKIT_RETURN_IF_FAILED(transfer<D2>(tqueue, command, submitInfo, separate ? &release : nullptr, transferFlight));
    TKIT_RETURN_IF_FAILED(transfer<D3>(tqueue, command, submitInfo, separate ? &release : nullptr, transferFlight));

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
        const auto table = Core::GetDeviceTable();
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
    return submitInfo;
}

Result<> SubmitTransfer(VKit::Queue *transfer, CommandPool *pool, TKit::Span<const TransferSubmitInfo> info)
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
    return transfer->Submit2(submits);
}

template <Dimension D> void gatherAcquireBarriers(TKit::StackArray<VkBufferMemoryBarrier2KHR> &barriers)
{
    RendererData<D> &rdata = getRendererData<D>();
    barriers.Insert(barriers.end(), rdata.AcquireBarriers.begin(), rdata.AcquireBarriers.end());
    rdata.AcquireBarriers.Clear();
}

void ApplyAcquireBarriers(const VkCommandBuffer graphicsCommand, const u32 maxAcquireBarriers)
{
    TKit::StackArray<VkBufferMemoryBarrier2KHR> barriers{};
    barriers.Reserve(maxAcquireBarriers);
    gatherAcquireBarriers<D2>(barriers);
    gatherAcquireBarriers<D3>(barriers);
    if (!barriers.IsEmpty())
    {
        const auto table = Core::GetDeviceTable();
        VkDependencyInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        info.bufferMemoryBarrierCount = barriers.GetSize();
        info.pBufferMemoryBarriers = barriers.GetData();
        info.dependencyFlags = 0;
        table->CmdPipelineBarrier2KHR(graphicsCommand, &info);
    }
}

template <Dimension D> static void setCameraViewport(const VkCommandBuffer command, const CameraInfo<D> &camera)
{
    const auto table = Core::GetDeviceTable();
    if (!camera.Transparent)
    {
        const Color &bg = camera.BackgroundColor;
        TKit::FixedArray<VkClearAttachment, D - 1> clearAttachments{};
        clearAttachments[0].colorAttachment = 0;
        clearAttachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearAttachments[0].clearValue.color = {{bg.rgba[0], bg.rgba[1], bg.rgba[2], bg.rgba[3]}};

        if constexpr (D == D3)
        {
            clearAttachments[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            clearAttachments[1].clearValue.depthStencil = {1.f, 0};
        }

        VkClearRect clearRect{};
        clearRect.rect.offset = {i32(camera.Viewport.x), i32(camera.Viewport.y)};
        clearRect.rect.extent = {u32(camera.Viewport.width), u32(camera.Viewport.height)};
        clearRect.layerCount = 1;
        clearRect.baseArrayLayer = 0;

        table->CmdClearAttachments(command, D - 1, clearAttachments.GetData(), 1, &clearRect);
    }
    table->CmdSetViewport(command, 0, 1, &camera.Viewport);
    table->CmdSetScissor(command, 0, 1, &camera.Scissor);
}

template <Dimension D>
ONYX_NO_DISCARD static Result<> render(const VkCommandBuffer graphicsCommand, const ViewInfo &vinfo,
                                       const u64 graphicsFlightValue,
                                       TKit::StackArray<Execution::Tracker> &transferTrackers)
{
    const auto &camInfos = vinfo.GetCameraInfos<D>();
    if (camInfos.IsEmpty())
        return Result<>::Ok();

    const ViewMask viewBit = vinfo.ViewBit;
    const auto &device = Core::GetDevice();

    RendererData<D> &rdata = getRendererData<D>();
    Color ambient{0.f, 0.f, 0.f, 0.f};
    for (const RenderContext<D> *ctx : rdata.Contexts)
        if (ctx->GetViewMask() & viewBit)
        {
            const Color &a = ctx->GetAmbientLight();
            ambient.rgba = Math::Max(ambient.rgba, a.rgba);
        }
    const u32 ambientColor = ambient.Pack();

    TKit::FixedArray<TKit::TierArray<TKit::TierArray<InstanceDrawInfo>>, StencilPass_Count> drawInfo{};
    const u32 bcount = Assets::GetBatchCount();
    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
        drawInfo[pass].Resize(bcount);

    const auto collectDrawInfo = [&](const Geometry geo) {
        GraphicsInstancePool &gpool = rdata.InstanceArenas[geo].Graphics;
        const u32 instanceSize = getInstanceSize<D>(geo);
        for (GraphicsInstanceRange &grange : gpool.Ranges)
        {
            if (!(grange.ViewMask & viewBit) || grange.InUseByGraphics())
                continue;
            TKIT_ASSERT(!grange.ContextRanges.IsEmpty(),
                        "[ONYX][RENDERER] Context ranges cannot be empty for a graphics memory range");
            VkDeviceSize offset = grange.Offset;
            VkDeviceSize size = 0;
            bool found = false;
            for (const ContextInstanceRange &crange : grange.ContextRanges)
            {
                if (rdata.IsContextRangeClean(viewBit, crange))
                    size += crange.Size;
                else if (size != 0)
                {
                    InstanceDrawInfo info;
                    info.FirstInstance = u32(offset / instanceSize);
                    info.InstanceCount = u32(size / instanceSize);
                    offset += size;
                    size = 0;
                    drawInfo[grange.Pass][grange.BatchIndex].Append(info);
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
                InstanceDrawInfo info;
                info.FirstInstance = u32(offset / instanceSize);
                info.InstanceCount = u32(size / instanceSize);
                drawInfo[grange.Pass][grange.BatchIndex].Append(info);
            }
            else if (!found)
                continue;

            if (grange.InUseByTransfer())
            {
                Execution::Tracker &tracker = grange.TransferTracker;
                found = false;
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
            grange.GraphicsTracker.InFlightValue = graphicsFlightValue;
        }
    };

    collectDrawInfo(Geometry_Circle);
    collectDrawInfo(Geometry_StaticMesh);

    LightRange<D> lranges;

    {
        const DrawInfo &dinfo = rdata.LightArenas[Light_Point].DrawInfo;
        lranges.PointLightOffset = dinfo.FirstInstance;
        lranges.PointLightCount = dinfo.InstanceCount;
    }

    if constexpr (D == D3)
    {
        const DrawInfo &dinfo = rdata.LightArenas[Light_Directional].DrawInfo;
        lranges.DirectionalLightOffset = dinfo.FirstInstance;
        lranges.DirectionalLightCount = dinfo.InstanceCount;
    }

    const auto table = Core::GetDeviceTable();
    for (const CameraInfo<D> &camInfo : camInfos)
    {
        setCameraViewport<D>(graphicsCommand, camInfo);
        for (u32 i = 0; i < StencilPass_Count; ++i)
        {
            const StencilPass pass = StencilPass(i);
            const Shading shading = GetShading(pass);

            const VKit::PipelineLayout &playout = Pipelines::GetPipelineLayout<D>(shading);

            const auto setupState = [graphicsCommand, pass, shading, &device, &playout, &rdata](const Geometry geo) {
                const VkDescriptorSet set = rdata.Descriptors[shading][geo];
                const VKit::GraphicsPipeline &pipeline = rdata.Pipelines[pass][geo];
                pipeline.Bind(graphicsCommand);

                VKit::DescriptorSet::Bind(device, graphicsCommand, set, VK_PIPELINE_BIND_POINT_GRAPHICS, playout);
            };

            if (shading == Shading_Unlit)
                table->CmdPushConstants(graphicsCommand, playout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(f32m4),
                                        &camInfo.ProjectionView);
            else
            {
                PushConstantData<D> pdata;
                pdata.ProjectionView = camInfo.ProjectionView;
                if constexpr (D == D3)
                    pdata.ViewPosition = f32v4{camInfo.ViewPosition, 1.f};
                pdata.LightRange = lranges;
                pdata.ViewBit = viewBit;
                pdata.AmbientColor = ambientColor;
                table->CmdPushConstants(graphicsCommand, playout,
                                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                        sizeof(PushConstantData<D>), &pdata);
            }

            setupState(Geometry_Circle);
            for (const InstanceDrawInfo &draw : drawInfo[pass][Assets::GetCircleBatchIndex()])
                table->CmdDraw(graphicsCommand, 6, draw.InstanceCount, 0, draw.FirstInstance);

            setupState(Geometry_StaticMesh);
            BindStaticMeshes<D>(graphicsCommand);
            const u32 bstart = Assets::GetBatchStart(Geometry_StaticMesh);
            const u32 bend = Assets::GetBatchEnd(Geometry_StaticMesh);
            for (u32 batch = bstart; batch < bend; ++batch)
                for (const InstanceDrawInfo &draw : drawInfo[pass][batch])
                {
                    const Mesh mesh = Assets::GetStaticMeshIndexFromBatch(batch);
                    DrawStaticMesh<D>(graphicsCommand, mesh, draw.FirstInstance, draw.InstanceCount);
                }
        }
    }
    return Result<>::Ok();
}

Result<RenderSubmitInfo> Render(VKit::Queue *graphics, const VkCommandBuffer command, const ViewInfo &vinfo)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Render");
    const u64 graphicsFlight = graphics->NextTimelineValue();
    TKit::StackArray<Execution::Tracker> transferTrackers{};

    u32 maxSyncPoints = 0;
    for (const InstanceArena &arena : getRendererData<D2>().InstanceArenas)
        maxSyncPoints += arena.Graphics.Ranges.GetSize();
    for (const InstanceArena &arena : getRendererData<D3>().InstanceArenas)
        maxSyncPoints += arena.Graphics.Ranges.GetSize();
    transferTrackers.Reserve(maxSyncPoints);

    TKIT_RETURN_IF_FAILED(render<D3>(command, vinfo, graphicsFlight, transferTrackers));
    TKIT_RETURN_IF_FAILED(render<D2>(command, vinfo, graphicsFlight, transferTrackers));

    RenderSubmitInfo submitInfo{};
    submitInfo.Command = command;
    submitInfo.InFlightValue = graphicsFlight;

    VkSemaphoreSubmitInfoKHR &imgInfo = submitInfo.WaitSemaphores.Append();
    imgInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    imgInfo.pNext = nullptr;
    imgInfo.semaphore = vinfo.ImageAvailableSemaphore;
    imgInfo.deviceIndex = 0;
    imgInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfoKHR &gtimSemInfo = submitInfo.SignalSemaphores[0];
    gtimSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    gtimSemInfo.pNext = nullptr;
    gtimSemInfo.value = graphicsFlight;
    gtimSemInfo.deviceIndex = 0;
    gtimSemInfo.semaphore = graphics->GetTimelineSempahore();
    gtimSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

    VkSemaphoreSubmitInfoKHR &rendFinInfo = submitInfo.SignalSemaphores[1];
    rendFinInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    rendFinInfo.pNext = nullptr;
    rendFinInfo.value = 0;
    rendFinInfo.semaphore = vinfo.RenderFinishedSemaphore;
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

Result<> SubmitRender(VKit::Queue *graphics, CommandPool *pool, const TKit::Span<const RenderSubmitInfo> info)
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

        sinfo.signalSemaphoreInfoCount = rinfo.SignalSemaphores.GetSize();
        sinfo.pSignalSemaphoreInfos = rinfo.SignalSemaphores.GetData();

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
    return graphics->Submit2(submits);
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

template <Dimension D> static void coalesceGraphicsInstanceRanges(GraphicsInstancePool &gpool)
{
    const RendererData<D> &rdata = getRendererData<D>();
    GraphicsInstanceRange gmergeRange{};
    TKit::StackArray<GraphicsInstanceRange> granges{};
    granges.Reserve(512); // this is a time bomb TODO(Isma): handle this

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
            ngrange.Pass = grange.Pass;
            ngrange.BatchIndex = grange.BatchIndex;

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

template <Dimension D> void coalesce()
{
#ifdef TKIT_ENABLE_ASSERTS
    validateRanges<D>();
#endif
    RendererData<D> &rdata = getRendererData<D>();
    for (InstanceArena &arena : rdata.InstanceArenas)
    {
        coalesceRanges(arena.Transfer);
        coalesceGraphicsInstanceRanges<D>(arena.Graphics);
    }
    for (LightArena &arena : rdata.LightArenas)
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

void Coalesce()
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer::Coalesce");
    coalesce<D2>();
    coalesce<D3>();
}

template <Dimension D> void BindStaticMeshes(const VkCommandBuffer command)
{
    const VKit::DeviceBuffer &vbuffer = Assets::GetStaticMeshVertexBuffer<D>();
    const VKit::DeviceBuffer &ibuffer = Assets::GetStaticMeshIndexBuffer<D>();

    vbuffer.BindAsVertexBuffer(command);
    ibuffer.BindAsIndexBuffer<Index>(command);
}

template <Dimension D>
void DrawStaticMesh(const VkCommandBuffer command, const Mesh mesh, const u32 firstInstance, const u32 instanceCount)
{
    const MeshDataLayout layout = Assets::GetStaticMeshLayout<D>(mesh);
    const auto table = Core::GetDeviceTable();
    table->CmdDrawIndexed(command, layout.IndexCount, instanceCount, layout.IndexStart, layout.VertexStart,
                          firstInstance);
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
                    ImGui::Text("Batch index: %u", range.BatchIndex);
                    ImGui::Text("Pass: %s", ToString(range.Pass));
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
                                  const u32 batchIndex = TKIT_U32_MAX, const StencilPass pass = StencilPass_Count) {
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
                    if (batchIndex != TKIT_U32_MAX && pass != StencilPass_Count)
                    {
                        ImGui::SameLine();
                        ImGui::Text("- Batch index: %u - Pass: %s", batchIndex, ToString(pass));
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
                drawPlot(1, range.Offset, range.Size, idx, range.BatchIndex, range.Pass);
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
    ImGui::PushID(&rdata);
    if (ImGui::Button("Coalesce##Button"))
        coalesce<D>();

    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = Geometry(i);
        const InstanceArena &arena = rdata.InstanceArenas[geo];
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
        const LightArena &arena = rdata.LightArenas[light];
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

template const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets<D2>(Shading shading);
template const TKit::FixedArray<VkDescriptorSet, Geometry_Count> &GetDescriptorSets<D3>(Shading shading);

template Result<RenderContext<D2> *> CreateContext();
template Result<RenderContext<D3> *> CreateContext();

template void DestroyContext(RenderContext<D2> *context);
template void DestroyContext(RenderContext<D3> *context);

template void BindStaticMeshes<D2>(VkCommandBuffer command);
template void BindStaticMeshes<D3>(VkCommandBuffer command);

template void DrawStaticMesh<D2>(VkCommandBuffer command, Mesh mesh, u32 firstInstance, u32 instanceCount);
template void DrawStaticMesh<D3>(VkCommandBuffer command, Mesh mesh, u32 firstInstance, u32 instanceCount);

} // namespace Onyx::Renderer
