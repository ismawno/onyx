#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/property/instance.hpp"
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

namespace Onyx::Renderer
{
using namespace Detail;
struct ContextRange
{
    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
    ViewMask ViewMask = 0;
    u64 Generation = 0;
    u32 ContextIndex = TKIT_U32_MAX;
};

struct TransferMemoryRange
{
    Execution::Tracker Tracker{};
    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
};
struct GraphicsMemoryRange
{
    Execution::Tracker TransferTracker{};
    Execution::Tracker GraphicsTracker{};

    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
    ViewMask ViewMask = 0;
    u32 BatchIndex = TKIT_U32_MAX;
    StencilPass Pass = StencilPass_Count;
    TKit::TierArray<ContextRange> ContextRanges{};

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

struct TransferArena
{
    VKit::DeviceBuffer Buffer{};
    TKit::TierArray<TransferMemoryRange> MemoryRanges{};
};
struct GraphicsArena
{
    VKit::DeviceBuffer Buffer{};
    TKit::TierArray<GraphicsMemoryRange> MemoryRanges{};
};

struct Arena
{
    TransferArena Transfer{};
    GraphicsArena Graphics{};
};

struct LightBuffers
{
    VKit::DeviceBuffer Transfer{};
    VKit::DeviceBuffer Graphics{};
    u32 InstanceCount = 0;
};

using RendererFlags = u8;
enum RendererFlagBit : RendererFlags
{
    RendererFlag_MustReloadPointLights = 1 << 0,
    RendererFlag_MustReloadDirectionalLights = 1 << 1,
};

template <Dimension D> struct RendererData
{
    TKit::TierArray<RenderContext<D> *> Contexts{};
    TKit::TierArray<u64> Generations{};
    TKit::TierArray<VkBufferMemoryBarrier2KHR> AcquireBarriers{};
    TKit::FixedArray<Arena, Geometry_Count> Arenas{};
    TKit::FixedArray<TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count>, StencilPass_Count> Pipelines{};
    TKit::FixedArray<LightBuffers, LightTypeCount<D>> LightData{};
    TKit::FixedArray<TKit::FixedArray<VkDescriptorSet, Geometry_Count>, Shading_Count> Descriptors{};
    RendererFlags Flags = 0;

    bool IsContextRangeClean(const ContextRange &crange) const
    {
        return crange.ContextIndex != TKIT_U32_MAX && !Contexts[crange.ContextIndex]->IsDirty(crange.Generation);
    }
    bool IsContextRangeClean(const ViewMask viewBit, const ContextRange &crange) const
    {
        return (crange.ViewMask & viewBit) && crange.ContextIndex != TKIT_U32_MAX &&
               !Contexts[crange.ContextIndex]->IsDirty(crange.Generation);
    }
    bool AreContextRangesClean(const GraphicsMemoryRange &grange)
    {
        for (const ContextRange &crange : grange.ContextRanges)
            if (IsContextRangeClean(crange))
                return true;
        return false;
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

template <Dimension D> static VkDeviceSize getInstanceSize(const Geometry geo)
{
    switch (geo)
    {
    case Geometry_Circle:
        return sizeof(CircleInstanceData<D>);
    case Geometry_StaticMesh:
        return sizeof(InstanceData<D>);
    case Geometry_Count:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized geometry type");
        return 0;
    }
    TKIT_FATAL("[ONYX][RENDERER] Unrecognized geometry type");
    return 0;
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
    writer.WriteBuffer(binding, buffer.CreateDescriptorInfo());
    writer.Overwrite(set);
}

template <Dimension D> static void updateInstanceDescriptorSets(const Geometry geo)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < Shading_Count; ++i)
    {
        const Shading shading = static_cast<Shading>(i);
        updateDescriptorSet(rdata.Descriptors[i][geo], 0, Descriptors::GetDescriptorSetLayout<D>(shading),
                            rdata.Arenas[geo].Graphics.Buffer);
    }
}

template <Dimension D> static void updateLightDescriptorSets(const u32 binding)
{
    TKIT_ASSERT(binding != 0, "[ONYX][RENDERER] Light binding cannot be zero");
    RendererData<D> &rdata = getRendererData<D>();
    for (u32 i = 0; i < Geometry_Count; ++i)
        updateDescriptorSet(rdata.Descriptors[Shading_Lit][i], binding,
                            Descriptors::GetDescriptorSetLayout<D>(Shading_Lit), rdata.LightData[binding - 1].Graphics);
}

template <Dimension D> static void updateLightDescriptorSets(const LightType light)
{
    updateLightDescriptorSets<D>(light + 1);
}

static constexpr VKit::DeviceBufferFlags getStageFlags()
{
    return static_cast<VKit::DeviceBufferFlags>(Buffer_Staging) | VKit::DeviceBufferFlag_Destination;
}
static constexpr VKit::DeviceBufferFlags getDeviceLocalFlags()
{
    return static_cast<VKit::DeviceBufferFlags>(Buffer_DeviceStorage) | VKit::DeviceBufferFlag_Source;
}

template <Dimension D, typename T>
ONYX_NO_DISCARD static Result<> createLightBuffers(LightBuffers &buffers, const u32 binding,
                                                   const VKit::DeviceBufferFlags stageFlags = Buffer_Staging,
                                                   const VKit::DeviceBufferFlags devLocalFlags = Buffer_DeviceStorage)
{
    auto result = Resources::CreateBuffer<T>(stageFlags);
    TKIT_RETURN_ON_ERROR(result);
    buffers.Transfer = result.GetValue();

    result = Resources::CreateBuffer<T>(devLocalFlags);
    TKIT_RETURN_ON_ERROR(result);
    buffers.Graphics = result.GetValue();
    updateLightDescriptorSets<D>(binding);
    return Result<>::Ok();
}

template <Dimension D> ONYX_NO_DISCARD static Result<> initialize()
{
    RendererData<D> &rdata = getRendererData<D>();

    const VkPipelineRenderingCreateInfoKHR renderInfo = CreatePipelineRenderingCreateInfo();
    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const Geometry geo = static_cast<Geometry>(i);
        TransferArena &tarena = rdata.Arenas[geo].Transfer;

        auto result = Resources::CreateBuffer(getStageFlags(), getInstanceSize<D>(geo));
        TKIT_RETURN_ON_ERROR(result);
        tarena.Buffer = result.GetValue();

        tarena.MemoryRanges.Append(TransferMemoryRange{.Size = tarena.Buffer.GetInfo().Size});

        GraphicsArena &garena = rdata.Arenas[geo].Graphics;

        result = Resources::CreateBuffer(getDeviceLocalFlags(), getInstanceSize<D>(geo));
        TKIT_RETURN_ON_ERROR(result);
        garena.Buffer = result.GetValue();

        garena.MemoryRanges.Append(GraphicsMemoryRange{.Size = garena.Buffer.GetInfo().Size});
        for (u32 j = 0; j < Shading_Count; ++j)
        {
            const Shading shading = static_cast<Shading>(j);
            const VKit::DescriptorSetLayout &layout = Descriptors::GetDescriptorSetLayout<D>(shading);
            const auto dresult = Descriptors::GetDescriptorPool().Allocate(layout);
            TKIT_RETURN_ON_ERROR(dresult);
            const VkDescriptorSet set = dresult.GetValue();
            updateDescriptorSet(set, 0, layout, garena.Buffer);
            rdata.Descriptors[j][i] = set;
        }
    }
    for (u32 i = 0; i < StencilPass_Count; ++i)
    {
        const StencilPass pass = static_cast<StencilPass>(i);

        auto result = Pipelines::CreateCirclePipeline<D>(pass, renderInfo);
        TKIT_RETURN_ON_ERROR(result);
        rdata.Pipelines[pass][Geometry_Circle] = result.GetValue();

        result = Pipelines::CreateStaticMeshPipeline<D>(pass, renderInfo);
        TKIT_RETURN_ON_ERROR(result);
        rdata.Pipelines[pass][Geometry_StaticMesh] = result.GetValue();
    }

    TKIT_RETURN_IF_FAILED((createLightBuffers<D, PointLightData<D>>(rdata.LightData[Light_Point], 1)));
    if constexpr (D == D3)
    {
        TKIT_RETURN_IF_FAILED((createLightBuffers<D, DirectionalLightData>(rdata.LightData[Light_Directional], 2)))
    }

    return Result<>::Ok();
}

template <Dimension D> static void terminate()
{
    RendererData<D> &rdata = getRendererData<D>();
    VKIT_CHECK_EXPRESSION(Core::DeviceWaitIdle());
    for (Arena &arena : rdata.Arenas)
    {
        arena.Transfer.Buffer.Destroy();
        arena.Graphics.Buffer.Destroy();
    }
    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            rdata.Pipelines[pass][geo].Destroy();

    for (LightBuffers &buffers : rdata.LightData)
    {
        buffers.Transfer.Destroy();
        buffers.Graphics.Destroy();
    }

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

template <Dimension D> ONYX_NO_DISCARD static Result<> resizeLightBuffers(const LightType light, const u32 instances)
{
    LightBuffers &buffers = getRendererData<D>().LightData[light];
    const auto tresult = Resources::CreateEnlargedBufferIfNeeded(buffers.Transfer, instances);
    const auto gresult = Resources::CreateEnlargedBufferIfNeeded(buffers.Graphics, instances);
    TKIT_RETURN_ON_ERROR(tresult);
    TKIT_RETURN_ON_ERROR(gresult);

    if (tresult.GetValue() || gresult.GetValue())
    {
        TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());
    }
    if (tresult.GetValue())
    {
        buffers.Transfer.Destroy();
        buffers.Transfer = tresult.GetValue().GetValue();
    }
    if (gresult.GetValue())
    {
        buffers.Graphics.Destroy();
        buffers.Graphics = gresult.GetValue().GetValue();
    }
    return Result<>::Ok();
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
    TKIT_FATAL("[ONYX][RENDERER] Render context ({}) index not found", static_cast<const void *>(context));
    return TKIT_U32_MAX;
}

template <Dimension D> void DestroyContext(RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u32 index = getContextIndex(context);
    rdata.Generations.RemoveOrdered(rdata.Generations.begin() + index);
    for (Arena &arena : rdata.Arenas)
        for (GraphicsMemoryRange &grange : arena.Graphics.MemoryRanges)
            for (ContextRange &crange : grange.ContextRanges)
                if (crange.ContextIndex != TKIT_U32_MAX && crange.ContextIndex > index)
                    --crange.ContextIndex;
                else if (crange.ContextIndex == index)
                    crange = ContextRange{};

    if (!context->GetPointLights().IsEmpty())
        rdata.Flags |= RendererFlag_MustReloadPointLights;
    context->RemoveAllPointLights();
    if constexpr (D == D3)
    {
        if (!context->GetDirectionalLights().IsEmpty())
            rdata.Flags |= RendererFlag_MustReloadDirectionalLights;
        context->RemoveAllDirectionalLights();
    }

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(context);
    rdata.Contexts.RemoveUnordered(rdata.Contexts.begin() + index);
}

template <Dimension D> void UpdateViewMask(const RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u32 index = getContextIndex(context);
    const ViewMask vmask = context->GetViewMask();
    for (Arena &arena : rdata.Arenas)
        for (GraphicsMemoryRange &grange : arena.Graphics.MemoryRanges)
        {
            ViewMask gmask = 0;
            for (ContextRange &crange : grange.ContextRanges)
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

template <Dimension D> static void clearViews(const ViewMask viewMask)
{
    RendererData<D> &rdata = getRendererData<D>();
    for (RenderContext<D> *ctx : rdata.Contexts)
        ctx->RemoveTarget(viewMask);

    for (Arena &arena : rdata.Arenas)
        for (GraphicsMemoryRange &grange : arena.Graphics.MemoryRanges)
        {
            for (ContextRange &crange : grange.ContextRanges)
                crange.ViewMask &= ~viewMask;
            grange.ViewMask &= ~viewMask;
        }
}

void ClearViews(const ViewMask viewMask)
{
    clearViews<D2>(viewMask);
    clearViews<D3>(viewMask);
}

ONYX_NO_DISCARD static Result<TransferMemoryRange *> findTransferRange(TransferArena &arena,
                                                                       const VkDeviceSize requiredMem,
                                                                       TKit::StackArray<Task> &tasks)
{
    auto &ranges = arena.MemoryRanges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        TransferMemoryRange &range = ranges[i];
        if (range.Size >= requiredMem && !range.Tracker.InUse())
        {
            if (range.Size == requiredMem)
                return &range;

            TransferMemoryRange child;
            child.Size = requiredMem;
            child.Offset = range.Offset;

            range.Offset += requiredMem;
            range.Size -= requiredMem;

            ranges.Insert(ranges.begin() + i, child);
            return &ranges[i];
        }
    }

    VKit::DeviceBuffer &buffer = arena.Buffer;
    const VkDeviceSize isize = buffer.GetInfo().InstanceSize;
    const VkDeviceSize icount = Math::Max(requiredMem / isize, buffer.GetInfo().InstanceCount);
    const VkDeviceSize size = buffer.GetInfo().Size;

    TKIT_LOG_DEBUG("[ONYX][RENDERER] Failed to find a suitable transfer range with {} bytes of memory. A new buffer "
                   "will be created with more memory (from {} to {} bytes)",
                   requiredMem, size, 2 * icount * isize);

    auto bresult = Resources::CreateBuffer(getStageFlags(), isize, 2 * icount);
    TKIT_RETURN_ON_ERROR(bresult);

    VKit::DeviceBuffer &nbuffer = bresult.GetValue();

    const VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = size};

    TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());
    for (const Task &task : tasks)
        task.WaitUntilFinished();
    tasks.Clear();

    nbuffer.Write(buffer.GetData(), copy);
    buffer.Destroy();
    buffer = nbuffer;

    const VkDeviceSize remSize = buffer.GetInfo().Size;
    TransferMemoryRange bigRange;
    bigRange.Offset = size + requiredMem;
    bigRange.Size = remSize - bigRange.Offset;

    TKIT_ASSERT(bigRange.Size != 0, "[ONYX][RENDERER] Leftover transfer range final size is zero");

    TransferMemoryRange smallRange;
    smallRange.Offset = size;
    smallRange.Size = requiredMem;

    ranges.Append(smallRange);
    ranges.Append(bigRange);

    return &ranges[ranges.GetSize() - 2];
}

template <Dimension D>
ONYX_NO_DISCARD static Result<GraphicsMemoryRange *> findGraphicsRange(const Geometry geo, RendererData<D> &rdata,
                                                                       GraphicsArena &arena,
                                                                       const VkDeviceSize requiredMem,
                                                                       VKit::Queue *transfer,
                                                                       TKit::StackArray<Task> &tasks)
{
    auto &ranges = arena.MemoryRanges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX][RENDERER] Memory ranges cannot be empty");

    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        GraphicsMemoryRange &range = ranges[i];
        // when reaching here, all free device memory ranges must have been curated. non dirty contexts now have a
        // memory range for themselves, and free memory ranges must have a u32 max batch index
        if (range.Size >= requiredMem && !range.InUse() && !rdata.AreContextRangesClean(range))
        {
            if (range.Size == requiredMem)
                return &range;

            GraphicsMemoryRange child;
            child.Size = requiredMem;
            child.Offset = range.Offset;

            range.Offset += requiredMem;
            range.Size -= requiredMem;

            ranges.Insert(ranges.begin() + i, child);
            return &ranges[i];
        }
    }

    VKit::DeviceBuffer &buffer = arena.Buffer;
    const VkDeviceSize isize = buffer.GetInfo().InstanceSize;
    const VkDeviceSize icount = Math::Max(requiredMem / isize, buffer.GetInfo().InstanceCount);
    const VkDeviceSize size = buffer.GetInfo().Size;

    TKIT_LOG_DEBUG("[ONYX][RENDERER] Failed to find a suitable graphics range with {} bytes of memory. A new buffer "
                   "will be created with more memory (from {} to {} bytes)",
                   requiredMem, size, 2 * icount * isize);

    auto bresult = Resources::CreateBuffer(getDeviceLocalFlags(), isize, 2 * icount);
    TKIT_RETURN_ON_ERROR(bresult);

    VKit::DeviceBuffer &nbuffer = bresult.GetValue();

    const VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = size};

    TKIT_RETURN_IF_FAILED(Core::DeviceWaitIdle());
    for (const Task &task : tasks)
        task.WaitUntilFinished();
    tasks.Clear();
    TKIT_RETURN_IF_FAILED(nbuffer.CopyFromBuffer(Execution::GetTransientTransferPool(), *transfer, buffer, copy));

    buffer.Destroy();
    buffer = nbuffer;
    updateInstanceDescriptorSets<D>(geo);

    const VkDeviceSize remSize = buffer.GetInfo().Size;
    GraphicsMemoryRange bigRange{};
    bigRange.Offset = size + requiredMem;
    bigRange.Size = remSize - bigRange.Offset;

    TKIT_ASSERT(bigRange.Size != 0, "[ONYX][RENDERER] Leftover graphics range final size is zero");

    GraphicsMemoryRange smallRange{};
    smallRange.Offset = size;
    smallRange.Size = requiredMem;

    ranges.Append(smallRange);
    ranges.Append(bigRange);

    return &ranges[ranges.GetSize() - 2];
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

struct LightRange
{
    VkDeviceSize SrcOffset = 0;
    VkDeviceSize DstOffset = 0;
    VkDeviceSize Size = 0;
};

template <typename Light> struct LightCopyData
{
    TKit::StackArray<Light> Data{};
    TKit::StackArray<LightRange> Ranges{};
    u32 Count = 0;
};

template <Dimension D> struct LightStagingData;
template <> struct LightStagingData<D2>
{
    LightCopyData<PointLightData<D2>> Points{};
};
template <> struct LightStagingData<D3>
{
    LightCopyData<PointLightData<D3>> Points{};
    LightCopyData<DirectionalLightData> Dirs{};
};

template <typename Light, typename LightData>
static void gatherRanges(const TKit::TierArray<Light *> &lights, LightCopyData<LightData> &copyData,
                         const LightFlags lightsToReload, const LightFlags flags, LightRange &lrange)
{
    for (Light *light : lights)
    {
        if (lightsToReload & flags)
            copyData.Data.Append(light->CreateInstanceData());
        else if (light->IsDirty())
        {
            copyData.Data.Append(light->CreateInstanceData());
            lrange.Size += sizeof(LightData);
        }
        else if (lrange.Size != 0)
        {
            copyData.Ranges.Append(lrange);
            lrange.SrcOffset += lrange.Size;
            lrange.DstOffset += lrange.Size;
            lrange.Size = 0;
        }
        else
            lrange.DstOffset += sizeof(LightData);

        light->MarkNonDirty();
    }
    if (lrange.Size != 0)
    {
        copyData.Ranges.Append(lrange);
        lrange.SrcOffset += lrange.Size;
        lrange.DstOffset += lrange.Size;
        lrange.Size = 0;
    }
}

template <Dimension D>
ONYX_NO_DISCARD static Result<> transferFullLightBuffers(const LightType light, const VkCommandBuffer cmd,
                                                         const u32 instances, const void *data,
                                                         TKit::StackArray<VkBufferMemoryBarrier2KHR> *release)
{
    RendererData<D> &rdata = getRendererData<D>();
    LightBuffers &buffers = rdata.LightData[light];
    TKIT_RETURN_IF_FAILED(resizeLightBuffers<D>(light, instances));

    const VkDeviceSize size = instances * buffers.Transfer.GetInfo().InstanceSize;
    VkBufferCopy2KHR copy{};
    copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
    copy.pNext = nullptr;
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = size;

    buffers.Transfer.Write(data, {.srcOffset = 0, .dstOffset = 0, .size = size});
    buffers.Graphics.CopyFromBuffer2(cmd, buffers.Transfer, copy);

    if (release)
        release->Append(createReleaseBarrier(buffers.Graphics, 0, size));

    rdata.AcquireBarriers.Append(createAcquireBarrier(buffers.Graphics, 0, size));
    return Result<>::Ok();
}

template <Dimension D>
void transferPartialLightBuffers(const VkCommandBuffer cmd, LightBuffers &buffers, const void *data,
                                 const TKit::StackArray<LightRange> &ranges,
                                 TKit::StackArray<VkBufferMemoryBarrier2KHR> *release)
{
    RendererData<D> &rdata = getRendererData<D>();
    TKit::StackArray<VkBufferCopy2KHR> copies{};
    copies.Reserve(ranges.GetSize());
    for (const LightRange &range : ranges)
    {
        buffers.Transfer.Write(data, {.srcOffset = range.SrcOffset, .dstOffset = range.DstOffset, .size = range.Size});
        VkBufferCopy2KHR &copy = copies.Append();
        copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
        copy.pNext = nullptr;
        copy.srcOffset = range.DstOffset;
        copy.dstOffset = range.DstOffset;
        copy.size = range.Size;

        rdata.AcquireBarriers.Append(createAcquireBarrier(buffers.Graphics, range.DstOffset, range.Size));
        if (release)
            release->Append(createReleaseBarrier(buffers.Graphics, range.DstOffset, range.Size));
    }
    buffers.Graphics.CopyFromBuffer2(cmd, buffers.Transfer, copies);
}

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

    LightFlags lightsToReload = 0;
    LightStagingData<D> lightStaging{};

    for (u32 i = 0; i < contexts.GetSize(); ++i)
    {
        RenderContext<D> *ctx = contexts[i];
        if (ctx->IsDirty(rdata.Generations[i]))
        {
            dirtyContexts.Append(ctx, i);
            rdata.Generations[i] = ctx->GetGeneration();
        }

        lightsToReload |= ctx->GetUpdateLightFlags();
        lightStaging.Points.Count += ctx->GetPointLights().GetSize();
        if constexpr (D == D3)
            lightStaging.Dirs.Count += ctx->GetDirectionalLights().GetSize();

        ctx->MarkLightsUpdated();
    }

    if (rdata.Flags & RendererFlag_MustReloadPointLights)
        lightsToReload |= LightFlag_Point;
    if constexpr (D == D3)
        if (rdata.Flags & RendererFlag_MustReloadDirectionalLights)
            lightsToReload |= LightFlag_Directional;

    rdata.LightData[Light_Point].InstanceCount = lightStaging.Points.Count;
    lightStaging.Points.Data.Reserve(lightStaging.Points.Count);
    lightStaging.Points.Ranges.Reserve(lightStaging.Points.Count);

    if constexpr (D == D3)
    {
        rdata.LightData[Light_Directional].InstanceCount = lightStaging.Dirs.Count;
        lightStaging.Dirs.Data.Reserve(lightStaging.Dirs.Count);
        lightStaging.Dirs.Ranges.Reserve(lightStaging.Dirs.Count);
    }

    const auto transferLights = [&]() -> Result<> {
        LightRange prange{};
        LightRange drange{};
        for (const RenderContext<D> *ctx : contexts)
        {
            gatherRanges(ctx->GetPointLights(), lightStaging.Points, lightsToReload, LightFlag_Point, prange);
            if constexpr (D == D3)
                gatherRanges(ctx->GetDirectionalLights(), lightStaging.Dirs, lightsToReload, LightFlag_Directional,
                             drange);
        }

        if constexpr (D == D3)
            if (lightStaging.Dirs.Count != 0)
            {
                info.Command = command;
                if (lightsToReload & LightFlag_Directional)
                {
                    TKIT_RETURN_IF_FAILED(transferFullLightBuffers<D>(Light_Directional, command,
                                                                      lightStaging.Dirs.Count,
                                                                      lightStaging.Dirs.Data.GetData(), release));
                }
                else if (!lightStaging.Dirs.Ranges.IsEmpty())
                    transferPartialLightBuffers<D>(command, rdata.LightData[Light_Directional],
                                                   lightStaging.Dirs.Data.GetData(), lightStaging.Dirs.Ranges, release);
            }

        if (lightStaging.Points.Count != 0)
        {
            info.Command = command;
            if (lightsToReload & LightFlag_Point)
                return transferFullLightBuffers<D>(Light_Point, command, lightStaging.Points.Count,
                                                   lightStaging.Points.Data.GetData(), release);

            if (!lightStaging.Points.Ranges.IsEmpty())
                transferPartialLightBuffers<D>(command, rdata.LightData[Light_Point],
                                               lightStaging.Points.Data.GetData(), lightStaging.Points.Ranges, release);
        }
        return Result<>::Ok();
    };

    if (dirtyContexts.IsEmpty())
        return transferLights();

    TKit::StackArray<VkBufferCopy2KHR> copies{};
    copies.Reserve(Assets::GetBatchCount());

    TKit::StackArray<ContextRange> contextRanges{};
    contextRanges.Reserve(dirtyContexts.GetSize());

    TKit::ITaskManager *tm = Core::GetTaskManager();

    TKit::StackArray<Task> tasks{};
    tasks.Reserve(dirtyContexts.GetSize() * Assets::GetBatchCount());

    u32 sindex = 0;

    struct RangePair
    {
        GraphicsMemoryRange *GraphicsRange;
        VKit::DeviceBuffer *GraphicsBuffer;
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
    copyCmds.Reserve(StencilPass_Count * static_cast<u32>(Geometry_Count));

    const auto finishTasks = [&] {
        for (const Task &task : tasks)
            tm->WaitUntilFinished(task);
    };

    const auto findRanges = [&](const u32 pass, const Geometry geo) -> Result<> {
        TransferArena &tarena = rdata.Arenas[geo].Transfer;
        GraphicsArena &garena = rdata.Arenas[geo].Graphics;

        const u32 bstart = Assets::GetBatchStart(geo);
        const u32 bend = Assets::GetBatchEnd(geo);

        CopyCommands copyCmd{};
        copyCmd.Transfer = &tarena.Buffer;
        copyCmd.Graphics = &garena.Buffer;
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

                ContextRange &crange = contextRanges.Append();
                crange.ContextIndex = cinfo.Index;
                crange.Offset = requiredMem;
                crange.Size = idata.Instances * idata.Data.GetInstanceSize();
                crange.Generation = ctx->GetGeneration();

                const ViewMask vm = ctx->GetViewMask();
                viewMask |= vm;
                crange.ViewMask = vm;

                requiredMem += crange.Size;
            }
            if (requiredMem == 0)
                continue;

            const auto tresult = findTransferRange(tarena, requiredMem, tasks);
            TKIT_RETURN_ON_ERROR(tresult);
            TransferMemoryRange *trange = tresult.GetValue();
            trange->Tracker.MarkInUse(transfer, transferFlightValue);

            for (const ContextRange &crange : contextRanges)
            {
                const RenderContext<D> *ctx = contexts[crange.ContextIndex];

                const auto &idata = ctx->GetInstanceData()[pass][batch];
                const auto copy = [&, crange, trange = *trange] {
                    tarena.Buffer.Write(
                        idata.Data.GetData(),
                        {.srcOffset = 0, .dstOffset = trange.Offset + crange.Offset, .size = crange.Size});
                };

                Task &task = tasks.Append(copy);
                sindex = tm->SubmitTask(&task, sindex);
            }

            const auto gresult = findGraphicsRange(geo, rdata, garena, requiredMem, transfer, tasks);
            TKIT_RETURN_ON_ERROR(gresult);
            GraphicsMemoryRange *grange = gresult.GetValue();

            grange->BatchIndex = batch;
            grange->ContextRanges = contextRanges;
            grange->ViewMask = viewMask;
            grange->Pass = static_cast<StencilPass>(pass);
            grange->TransferTracker.MarkInUse(transfer, transferFlightValue);

            VkBufferCopy2KHR &copy = copies.Append();
            copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
            copy.pNext = nullptr;
            copy.srcOffset = trange->Offset;
            copy.dstOffset = grange->Offset;
            copy.size = requiredMem;

            ranges.Append(grange, &garena.Buffer, requiredMem);
        }
        copyCmd.Size = copies.GetSize() - copyCmd.Offset;
        if (copyCmd.Size != 0)
            copyCmds.Append(copyCmd);

        return Result<>::Ok();
    };

    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
    {
        TKIT_RETURN_IF_FAILED(findRanges(pass, Geometry_Circle), finishTasks());
        TKIT_RETURN_IF_FAILED(findRanges(pass, Geometry_StaticMesh), finishTasks());
    }
    for (const RangePair &range : ranges)
    {
        GraphicsMemoryRange *grange = range.GraphicsRange;
        rdata.AcquireBarriers.Append(createAcquireBarrier(*range.GraphicsBuffer, grange->Offset, range.RequiredMemory));

        if (release)
            release->Append(createReleaseBarrier(*range.GraphicsBuffer, grange->Offset, range.RequiredMemory));
    }
    for (const CopyCommands &cmd : copyCmds)
    {
        const TKit::Span<const VkBufferCopy2KHR> cpspan{copies.GetData() + cmd.Offset, cmd.Size};
        cmd.Graphics->CopyFromBuffer2(command, *cmd.Transfer, cpspan);
    }

    info.Command = command;

    TKIT_RETURN_IF_FAILED(transferLights(), finishTasks());
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
    for (const TransferSubmitInfo &info : info)
    {
        TKIT_ASSERT(info.Command,
                    "[ONYX][RENDERER] A submission must have a valid transfer command buffer to be submitted");
        if (info.InFlightValue > maxFlight)
            maxFlight = info.InFlightValue;

        VkSubmitInfo2KHR &sinfo = submits.Append();
        sinfo = {};
        sinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;

        sinfo.signalSemaphoreInfoCount = 1;
        sinfo.pSignalSemaphoreInfos = &info.SignalSemaphore;

        VkCommandBufferSubmitInfoKHR &cmd = cmds.Append();
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
        cmd.pNext = nullptr;
        cmd.commandBuffer = info.Command;
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
        clearRect.rect.offset = {static_cast<i32>(camera.Viewport.x), static_cast<i32>(camera.Viewport.y)};
        clearRect.rect.extent = {static_cast<u32>(camera.Viewport.width), static_cast<u32>(camera.Viewport.height)};
        clearRect.layerCount = 1;
        clearRect.baseArrayLayer = 0;

        table->CmdClearAttachments(command, D - 1, clearAttachments.GetData(), 1, &clearRect);
    }
    table->CmdSetViewport(command, 0, 1, &camera.Viewport);
    table->CmdSetScissor(command, 0, 1, &camera.Scissor);
}

template <Dimension D>
static void pushConstantData(const VkCommandBuffer command, const Shading shading, const VKit::PipelineLayout &layout,
                             const ViewMask viewBit, const u32 ambientColor, const CameraInfo<D> &camera)
{
    const auto table = Core::GetDeviceTable();
    switch (shading)
    {
    case Shading_Unlit:
        table->CmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(f32m4), &camera.ProjectionView);
        return;
    case Shading_Lit: {
        const RendererData<D> &rdata = getRendererData<D>();
        PushConstantData<D> pdata;
        pdata.ProjectionView = camera.ProjectionView;
        pdata.PointLightCount = rdata.LightData[Light_Point].InstanceCount;
        if constexpr (D == D3)
        {
            pdata.ViewPosition = f32v4{camera.ViewPosition, 1.f};
            pdata.DirectionalLightCount = rdata.LightData[Light_Directional].InstanceCount;
        }
        pdata.ViewBit = viewBit;
        pdata.AmbientColor = ambientColor;

        table->CmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                sizeof(PushConstantData<D>), &pdata);
        return;
    }
    default:
        TKIT_FATAL("[ONYX][RENDERER] Unrecognized shading");
        return;
    }
}

struct InstanceDrawInfo
{
    u32 FirstInstance;
    u32 InstanceCount;
};

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
        GraphicsArena &garena = rdata.Arenas[geo].Graphics;
        const VkDeviceSize instanceSize = garena.Buffer.GetInfo().InstanceSize;
        for (GraphicsMemoryRange &grange : garena.MemoryRanges)
        {
            if (!(grange.ViewMask & viewBit) || grange.InUseByGraphics())
                continue;
            TKIT_ASSERT(!grange.ContextRanges.IsEmpty(),
                        "[ONYX][RENDERER] Context ranges cannot be empty for a graphics memory range");
            VkDeviceSize offset = grange.Offset;
            VkDeviceSize size = 0;
            bool found = false;
            for (const ContextRange &crange : grange.ContextRanges)
            {
                if (rdata.IsContextRangeClean(viewBit, crange))
                    size += crange.Size;
                else if (size != 0)
                {
                    InstanceDrawInfo info;
                    info.FirstInstance = offset / instanceSize;
                    info.InstanceCount = size / instanceSize;
                    offset += size;
                    size = 0;
                    drawInfo[grange.Pass][grange.BatchIndex].Append(info);
                    found = true;
                }
                else
                    offset += crange.Size;
            }
            // TKIT_ASSERT(size != 0 || offset > grange.Offset,
            //             "[ONYX][RENDERER] Found labeled graphics memory arena range for window with view bit {} with
            //             " "no context ranges targetting said view", viewBit);
            if (size != 0)
            {
                InstanceDrawInfo info;
                info.FirstInstance = offset / instanceSize;
                info.InstanceCount = size / instanceSize;
                drawInfo[grange.Pass][grange.BatchIndex].Append(info);
            }
            else if (!found)
                continue;

            if (grange.InUseByTransfer())
            {
                Execution::Tracker &tracker = grange.TransferTracker;
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
            grange.GraphicsTracker.InFlightValue = graphicsFlightValue;
        }
    };

    collectDrawInfo(Geometry_Circle);
    collectDrawInfo(Geometry_StaticMesh);

    const auto table = Core::GetDeviceTable();
    for (const CameraInfo<D> &camInfo : camInfos)
    {
        setCameraViewport<D>(graphicsCommand, camInfo);
        for (u32 i = 0; i < StencilPass_Count; ++i)
        {
            const StencilPass pass = static_cast<StencilPass>(i);
            const Shading shading = GetShading(pass);

            const VKit::PipelineLayout &playout = Pipelines::GetPipelineLayout<D>(shading);

            const auto setupState = [graphicsCommand, pass, shading, &device, &playout, &rdata](const Geometry geo) {
                const VkDescriptorSet set = rdata.Descriptors[shading][geo];
                const VKit::GraphicsPipeline &pipeline = rdata.Pipelines[pass][geo];
                pipeline.Bind(graphicsCommand);

                VKit::DescriptorSet::Bind(device, graphicsCommand, set, VK_PIPELINE_BIND_POINT_GRAPHICS, playout);
            };

            pushConstantData(graphicsCommand, shading, playout, viewBit, ambientColor, camInfo);

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
    for (const Arena &arena : getRendererData<D2>().Arenas)
        maxSyncPoints += arena.Graphics.MemoryRanges.GetSize();
    for (const Arena &arena : getRendererData<D3>().Arenas)
        maxSyncPoints += arena.Graphics.MemoryRanges.GetSize();
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

// --when initializing:
// -create memories and assign Execution
// --when coalescing:
// -merge free transfer ranges into one
// -merge free graphics ranges into one
// -non-dirty context ranges get their own memory range (if 2 contiguous, they share it)
// -the view masks of non-dirty device ranges get updated to reflect the orred context ranges
// -all memory ranges get their transfer queue set to null
// -all memory ranges view mask == 0 get set to free
// -all context ranges view mask == 0: removed and split/shrink if necessary
// -all context index == max u32 get set to free
// --when removing context:
// -traverse all context ranges, removing the ones belonging to the context. split/shrink device memory ranges based on
// -local buffer barriers must be cleaned out
// what was removed
// -all other context with index > removal, get decremented by one
// --when removing window:
// -traverse all context ranges, removing the window bit. if context range becomes empty of view masks, remove it,
// splitting/shrinking if necessary

template <Dimension D> void coalesce()
{
    RendererData<D> &rdata = getRendererData<D>();
    for (Arena &arena : rdata.Arenas)
    {
        TransferArena &tarena = arena.Transfer;
        TransferMemoryRange tmergeRange{};

        TKit::StackArray<TransferMemoryRange> tranges{};
        tranges.Reserve(tarena.MemoryRanges.GetSize());

        for (TransferMemoryRange &trange : tarena.MemoryRanges)
        {
            if (trange.Tracker.InUse())
            {
                if (tmergeRange.Size != 0)
                {
                    tranges.Append(tmergeRange);
                    tmergeRange.Offset = tmergeRange.Size + trange.Size;
                    tmergeRange.Size = 0;
                }
                else
                    tmergeRange.Offset += trange.Size;
                tranges.Append(trange);
            }
            else
                tmergeRange.Size += trange.Size;
        }
        if (tmergeRange.Size != 0)
            tranges.Append(tmergeRange);

        tarena.MemoryRanges = tranges;
        TKIT_ASSERT(
            !tranges.IsEmpty(),
            "[ONYX][RENDERER] All memory ranges for the transfer arena have been removed after coalesce operation!");
        GraphicsArena &garena = arena.Graphics;
        GraphicsMemoryRange gmergeRange{};

        TKit::StackArray<GraphicsMemoryRange> granges{};
        granges.Reserve(garena.MemoryRanges.GetSize());

        for (GraphicsMemoryRange &grange : garena.MemoryRanges)
        {
            if (grange.InUse())
            {
                if (gmergeRange.Size != 0)
                {
                    granges.Append(gmergeRange);
                    gmergeRange.Offset = gmergeRange.Size + grange.Size;
                    gmergeRange.Size = 0;
                }
                else
                    gmergeRange.Offset += grange.Size;

                granges.Append(grange);
            }
            else if (!grange.ContextRanges.IsEmpty())
            {
                TKit::StackArray<ContextRange> cranges{};
                cranges.Reserve(grange.ContextRanges.GetSize());

                TKIT_ASSERT(grange.Size != 0, "[ONYX][RENDERER] Graphics memory range should not have reached a zero "
                                              "size if there are context ranges left");
                grange.Size = 0;
                grange.ViewMask = 0;
                grange.TransferTracker.Queue = nullptr;
                grange.GraphicsTracker.Queue = nullptr;
                for (ContextRange &crange : grange.ContextRanges)
                    if (rdata.IsContextRangeClean(crange))
                    {
                        if (gmergeRange.Size != 0)
                        {
                            granges.Append(gmergeRange);
                            gmergeRange.Size = 0;
                        }
                        gmergeRange.Offset += crange.Size;
                        grange.Size += crange.Size;
                        grange.ViewMask |= crange.ViewMask;
                        cranges.Append(crange);
                    }
                    else
                    {
                        if (grange.Size != 0)
                        {
                            grange.ContextRanges = cranges;
                            granges.Append(grange);
                            grange.Size = 0;
                            cranges.Clear();
                        }
                        grange.Offset += crange.Size;
                        gmergeRange.Size += crange.Size;
                    }
                if (grange.Size != 0)
                {
                    grange.ContextRanges = cranges;
                    granges.Append(grange);
                }
            }
            else
                gmergeRange.Size += grange.Size;
        }
        if (gmergeRange.Size != 0)
            granges.Append(gmergeRange);
        garena.MemoryRanges = granges;

        TKIT_ASSERT(
            !granges.IsEmpty(),
            "[ONYX][RENDERER] All memory ranges for the graphics arena have been removed after coalesce operation");
    }
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

template Result<RenderContext<D2> *> CreateContext();
template Result<RenderContext<D3> *> CreateContext();

template void DestroyContext(RenderContext<D2> *context);
template void DestroyContext(RenderContext<D3> *context);

template void BindStaticMeshes<D2>(VkCommandBuffer command);
template void BindStaticMeshes<D3>(VkCommandBuffer command);

template void DrawStaticMesh<D2>(VkCommandBuffer command, Mesh mesh, u32 firstInstance, u32 instanceCount);
template void DrawStaticMesh<D3>(VkCommandBuffer command, Mesh mesh, u32 firstInstance, u32 instanceCount);

} // namespace Onyx::Renderer
