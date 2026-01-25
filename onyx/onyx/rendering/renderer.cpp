#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/app/window.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/rendering/context.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx::Renderer
{
using namespace Detail;
static bool inUse(const VKit::Queue *queue, const u64 inFlightValue)
{
    return queue && queue->GetCompletedTimeline() < inFlightValue;
}
struct ContextRange
{
    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
    u64 ViewMask = 0;
    u64 Generation = 0;
    u32 ContextIndex = TKIT_U32_MAX;
};

struct TransferMemoryRange
{
    const VKit::Queue *Transfer = nullptr;
    u64 TransferFlightValue = 0;
    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;

    bool InUse() const
    {
        return inUse(Transfer, TransferFlightValue);
    }
};
struct GraphicsMemoryRange
{
    const VKit::Queue *Transfer = nullptr;
    const VKit::Queue *Graphics = nullptr;
    u64 GraphicsFlightValue = 0;
    u64 TransferFlightValue = 0;

    VkBufferMemoryBarrier2KHR Barrier{};
    VkDeviceSize Offset = 0;
    VkDeviceSize Size = 0;
    u64 ViewMask = 0;
    u32 BatchIndex = TKIT_U32_MAX;
    StencilPass Pass = StencilPass_Count;
    TKit::TierArray<ContextRange> ContextRanges{};

    bool InUseByTransfer() const
    {
        return inUse(Transfer, TransferFlightValue);
    }
    bool InUseByGraphics() const
    {
        return inUse(Graphics, GraphicsFlightValue);
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

template <Dimension D> struct RendererData
{
    TKit::TierArray<RenderContext<D> *> Contexts{};
    TKit::TierArray<u64> Generations{};
    TKit::FixedArray<Arena, Geometry_Count> Arenas{};
    TKit::FixedArray<TKit::FixedArray<VKit::GraphicsPipeline, Geometry_Count>, StencilPass_Count> Pipelines{};

    bool IsContextRangeClean(const ContextRange &crange) const
    {
        return crange.ViewMask != 0 && crange.ContextIndex != TKIT_U32_MAX &&
               !Contexts[crange.ContextIndex]->IsDirty(crange.Generation);
    }
    bool IsContextRangeClean(const u64 viewBit, const ContextRange &crange) const
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

static RendererData<D2> s_RendererData2{};
static RendererData<D3> s_RendererData3{};

template <Dimension D> static RendererData<D> &getRendererData()
{
    if constexpr (D == D2)
        return s_RendererData2;
    else
        return s_RendererData3;
}

template <Dimension D> static VkDeviceSize getInstanceSize(const GeometryType geo)
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

template <Dimension D> ONYX_NO_DISCARD static Result<> initialize()
{
    RendererData<D> &rdata = getRendererData<D>();

    const VkPipelineRenderingCreateInfoKHR renderInfo = CreatePipelineRenderingCreateInfo();
    for (u32 i = 0; i < Geometry_Count; ++i)
    {
        const GeometryType geo = static_cast<GeometryType>(i);
        TransferArena &tarena = rdata.Arenas[geo].Transfer;

        auto result = Resources::CreateBuffer(Buffer_Staging, getInstanceSize<D>(geo));
        TKIT_RETURN_ON_ERROR(result);
        tarena.Buffer = result.GetValue();

        tarena.MemoryRanges.Append(TransferMemoryRange{.Size = tarena.Buffer.GetInfo().Size});

        GraphicsArena &garena = rdata.Arenas[geo].Graphics;

        result = Resources::CreateBuffer(Buffer_DeviceStorage, getInstanceSize<D>(geo));
        TKIT_RETURN_ON_ERROR(result);
        garena.Buffer = result.GetValue();

        garena.MemoryRanges.Append(GraphicsMemoryRange{.Size = garena.Buffer.GetInfo().Size});
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
    return Result<>::Ok();
}

template <Dimension D> static void terminate()
{
    RendererData<D> &rdata = getRendererData<D>();
    ONYX_CHECK_EXPRESSION(Core::DeviceWaitIdle());
    for (Arena &arena : rdata.Arenas)
    {
        arena.Transfer.Buffer.Destroy();
        arena.Graphics.Buffer.Destroy();
    }
    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            rdata.Pipelines[pass][geo].Destroy();
}

Result<> Initialize()
{
    TKIT_RETURN_IF_FAILED(initialize<D2>());
    return initialize<D3>();
}
void Terminate()
{
    terminate<D2>();
    terminate<D3>();
}

template <Dimension D> RenderContext<D> *CreateContext()
{
    RendererData<D> &rdata = getRendererData<D>();
    TKit::TierAllocator *alloc = TKit::Memory::GetTier();
    RenderContext<D> *ctx = alloc->Create<RenderContext<D>>();
    rdata.Contexts.Append(ctx);
    rdata.Generations.Append(ctx->GetGeneration());
    return ctx;
}

template <Dimension D> void DestroyContext(RenderContext<D> *context)
{
    RendererData<D> &rdata = getRendererData<D>();
    u32 index = TKIT_U32_MAX;
    for (u32 i = 0; i < rdata.Contexts.GetSize(); ++i)
        if (rdata.Contexts[i] == context)
        {
            index = i;
            break;
        }
    TKIT_ASSERT(index != TKIT_U32_MAX, "[ONYX][RENDERER] Render context not found when attempting to destroy it");
    for (Arena &arena : rdata.Arenas)
        for (GraphicsMemoryRange &grange : arena.Graphics.MemoryRanges)
            for (ContextRange &crange : grange.ContextRanges)
                if (crange.ContextIndex > index)
                    --crange.ContextIndex;
                else if (crange.ContextIndex == index)
                    crange.ContextIndex = TKIT_U32_MAX;

    TKit::TierAllocator *alloc = TKit::Memory::GetTier();
    alloc->Destroy(context);
    rdata.Contexts.RemoveUnordered(rdata.Contexts.begin() + index);
}

template <Dimension D> static void clearWindow(const Window *window)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u64 viewBit = window->GetViewBit();
    for (RenderContext<D> *ctx : rdata.Contexts)
        ctx->RemoveTarget(window);

    for (Arena &arena : rdata.Arenas)
        for (GraphicsMemoryRange &grange : arena.Graphics.MemoryRanges)
        {
            for (ContextRange &crange : grange.ContextRanges)
                crange.ViewMask &= ~viewBit;
            grange.ViewMask &= ~viewBit;
        }
}

void ClearWindow(const Window *window)
{
    clearWindow<D2>(window);
    clearWindow<D3>(window);
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
        if (range.Size >= requiredMem && !range.InUse())
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

    auto bresult = Resources::CreateBuffer(Buffer_Staging, isize, 2 * icount);
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
ONYX_NO_DISCARD static Result<GraphicsMemoryRange *> findGraphicsRange(RendererData<D> &rdata, GraphicsArena &arena,
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

    auto bresult = Resources::CreateBuffer(Buffer_DeviceStorage, isize, 2 * icount);
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

VkBufferMemoryBarrier2KHR createAcquireBarrier(const VkBuffer deviceLocalBuffer, const VkDeviceSize offset,
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
VkBufferMemoryBarrier2KHR createReleaseBarrier(const VkBuffer deviceLocalBuffer, const VkDeviceSize offset,
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

template <Dimension D>
ONYX_NO_DISCARD static Result<> transfer(VKit::Queue *transfer, const VkCommandBuffer command, TransferSubmitInfo &info,
                                         TKit::TierArray<VkBufferMemoryBarrier2KHR> *release,
                                         const u64 transferFlightValue)
{
    struct ContextInfo
    {
        const RenderContext<D> *Context;
        u32 Index;
    };

    RendererData<D> &rdata = getRendererData<D>();
    TKit::StackArray<ContextInfo> dirtyContexts{};
    dirtyContexts.Reserve(rdata.Contexts.GetSize());

    for (u32 i = 0; i < rdata.Contexts.GetSize(); ++i)
    {
        const RenderContext<D> *ctx = rdata.Contexts[i];
        if (ctx->IsDirty(rdata.Generations[i]))
        {
            dirtyContexts.Append(ctx, i);
            rdata.Generations[i] = ctx->GetGeneration();
        }
    }

    if (dirtyContexts.IsEmpty())
        return Result<>::Ok();

    TKit::StackArray<ContextRange> contextRanges{};
    contextRanges.Reserve(dirtyContexts.GetSize());

    TKit::ITaskManager *tm = Core::GetTaskManager();

    TKit::StackArray<Task> tasks{};
    tasks.Reserve(dirtyContexts.GetSize() * Assets::GetBatchCount());

    TKit::StackArray<VkBufferCopy2KHR> copies{};
    copies.Reserve(Assets::GetBatchCount());

    u32 sindex = 0;

    const auto processBatches = [&](const u32 pass, const GeometryType geo) -> Result<> {
        TransferArena &tarena = rdata.Arenas[geo].Transfer;
        GraphicsArena &garena = rdata.Arenas[geo].Graphics;

        const u32 bstart = Assets::GetBatchStart(geo);
        const u32 bend = Assets::GetBatchEnd(geo);

        copies.Clear();
        for (u32 batch = bstart; batch < bend; ++batch)
        {
            contextRanges.Clear();
            VkDeviceSize requiredMem = 0;
            u64 viewMask = 0;
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

                const u64 vm = ctx->GetViewMask();
                viewMask |= vm;
                crange.ViewMask = vm;

                requiredMem += crange.Size;
            }
            if (requiredMem == 0)
                continue;

            const auto tresult = findTransferRange(tarena, requiredMem, tasks);
            TKIT_RETURN_ON_ERROR(tresult);
            TransferMemoryRange *trange = tresult.GetValue();
            trange->Transfer = transfer;
            trange->TransferFlightValue = transferFlightValue;

            for (const ContextRange &crange : contextRanges)
            {
                const RenderContext<D> *ctx = rdata.Contexts[crange.ContextIndex];

                const auto &idata = ctx->GetInstanceData()[pass][batch];
                const auto copy = [&, crange, trange = *trange] {
                    tarena.Buffer.Write(
                        idata.Data.GetData(),
                        {.srcOffset = 0, .dstOffset = trange.Offset + crange.Offset, .size = crange.Size});
                };

                Task &task = tasks.Append(copy);
                sindex = tm->SubmitTask(&task, sindex);
            }

            const auto gresult = findGraphicsRange(rdata, garena, requiredMem, transfer, tasks);
            TKIT_RETURN_ON_ERROR(gresult);
            GraphicsMemoryRange *grange = gresult.GetValue();

            grange->BatchIndex = batch;
            grange->ContextRanges = contextRanges;
            grange->ViewMask = viewMask;
            grange->Pass = static_cast<StencilPass>(pass);
            grange->Transfer = transfer;
            grange->Barrier = createAcquireBarrier(garena.Buffer, grange->Offset, requiredMem);

            VkBufferCopy2KHR &copy = copies.Append();
            copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
            copy.pNext = nullptr;
            copy.srcOffset = trange->Offset;
            copy.dstOffset = grange->Offset;
            copy.size = requiredMem;

            if (release)
                release->Append(createReleaseBarrier(garena.Buffer, grange->Offset, requiredMem));
        }
        if (!copies.IsEmpty())
            garena.Buffer.CopyFromBuffer2(command, tarena.Buffer, copies);

        return Result<>::Ok();
    };

    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
    {
        // processBatches(pass, Geometry_Circle);
        TKIT_RETURN_IF_FAILED(processBatches(pass, Geometry_StaticMesh));
    }

    info.Command = command;

    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);

    return Result<>::Ok();
}

Result<TransferSubmitInfo> Transfer(VKit::Queue *tqueue, const VkCommandBuffer command)
{
    TransferSubmitInfo submitInfo{};
    const bool separate = Execution::IsSeparateTransferMode();
    TKit::TierArray<VkBufferMemoryBarrier2KHR> release{};

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

    pool->MarkInUse(transfer, maxFlight);
    return transfer->Submit2(submits);
}

template <Dimension D> void gatherAcquireBarriers(TKit::TierArray<VkBufferMemoryBarrier2KHR> &barriers)
{
    const RendererData<D> &rdata = getRendererData<D>();
    for (const Arena &arena : rdata.Arenas)
        for (const GraphicsMemoryRange &grange : arena.Graphics.MemoryRanges)
            if (grange.InUseByTransfer())
                barriers.Append(grange.Barrier);
}

void ApplyAcquireBarriers(const VkCommandBuffer graphicsCommand)
{
    TKit::TierArray<VkBufferMemoryBarrier2KHR> barriers{};
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

template <Dimension D> static void setCameraViewport(const VkCommandBuffer command, const Detail::CameraInfo<D> &camera)
{
    const auto table = Core::GetDeviceTable();
    if (!camera.Transparent)
    {
        const Color &bg = camera.BackgroundColor;
        TKit::FixedArray<VkClearAttachment, D - 1> clearAttachments{};
        clearAttachments[0].colorAttachment = 0;
        clearAttachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearAttachments[0].clearValue.color = {{bg.RGBA[0], bg.RGBA[1], bg.RGBA[2], bg.RGBA[3]}};

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

template <Dimension D> static void pushConstantData(const VkCommandBuffer command, const Detail::CameraInfo<D> &camera)
{
    PushConstantData<Shading_Unlit> pdata{};
    pdata.ProjectionView = camera.ProjectionView;

    VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT;
    // if constexpr (Shade == Shading_Lit)
    // {
    //     pdata.ViewPosition = f32v4(camera.Camera->ViewPosition, 1.f);
    //     pdata.AmbientColor = camera.Light.AmbientColor->RGBA;
    //     pdata.DirectionalLightCount = camera.Light.DirectionalCount;
    //     pdata.PointLightCount = camera.Light.PointCount;
    //     stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
    // }
    const auto table = Core::GetDeviceTable();
    table->CmdPushConstants(command, Pipelines::GetGraphicsPipelineLayout(Shading_Unlit), stages, 0,
                            sizeof(PushConstantData<Shading_Unlit>), &pdata);
}

struct InstanceDrawInfo
{
    u32 FirstInstance;
    u32 InstanceCount;
};

struct TransferSyncPoint
{
    const VKit::Queue *Transfer;
    u64 TransferFlightValue;
};

template <Dimension D>
ONYX_NO_DISCARD static Result<> render(VKit::Queue *graphics, const VkCommandBuffer graphicsCommand,
                                       const Window *window, const u64 graphicsFlightValue,
                                       TKit::StackArray<TransferSyncPoint> &syncPoints)
{
    const auto camInfos = window->GetCameraInfos<D>();
    if (camInfos.IsEmpty())
        return Result<>::Ok();

    const u64 viewBit = window->GetViewBit();
    const auto &device = Core::GetDevice();

    RendererData<D> &rdata = getRendererData<D>();

    u32 batches = 0;

    TKit::FixedArray<TKit::TierArray<TKit::TierArray<InstanceDrawInfo>>, StencilPass_Count> drawInfo{};
    const u32 bcount = Assets::GetBatchCount();
    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
        drawInfo[pass].Resize(bcount);

    const auto collectDrawInfo = [&](const GeometryType geo) {
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
                }
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
            else if (offset == grange.Offset)
                continue;

            ++batches;
            if (grange.InUseByTransfer())
            {
                bool found = false;
                for (TransferSyncPoint &sp : syncPoints)
                {
                    if (sp.Transfer == grange.Transfer)
                    {
                        found = true;
                        if (sp.TransferFlightValue < grange.TransferFlightValue)
                            sp.TransferFlightValue = grange.TransferFlightValue;
                        break;
                    }
                }
                if (!found)
                    syncPoints.Append(grange.Transfer, grange.TransferFlightValue);
            }
            grange.GraphicsFlightValue = graphicsFlightValue;
        }
    };

    // collectDrawInfo(Geometry_Circle);
    collectDrawInfo(Geometry_StaticMesh);

    if (batches == 0)
        return Result<>::Ok();

    for (const Detail::CameraInfo<D> &camInfo : camInfos)
    {
        setCameraViewport<D>(graphicsCommand, camInfo);
        for (u32 pass = 0; pass < StencilPass_Count; ++pass)
        {
            const auto result =
                Descriptors::FindSuitableDescriptorSet(rdata.Arenas[Geometry_StaticMesh].Graphics.Buffer);
            TKIT_RETURN_ON_ERROR(result);
            DescriptorSet *set = result.GetValue();

            rdata.Pipelines[pass][Geometry_StaticMesh].Bind(graphicsCommand);
            BindStaticMeshes<D>(graphicsCommand);
            pushConstantData(graphicsCommand, camInfo);

            VKit::DescriptorSet::Bind(device, graphicsCommand, set->Set, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      Pipelines::GetGraphicsPipelineLayout(Shading_Unlit));
            set->MarkInUse(graphics, graphicsFlightValue);

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

Result<RenderSubmitInfo> Render(VKit::Queue *graphics, const VkCommandBuffer command, const Window *window)
{
    RenderSubmitInfo submitInfo{};
    submitInfo.Command = command;
    const u64 graphicsFlight = graphics->NextTimelineValue();
    TKit::StackArray<TransferSyncPoint> syncPoints{};

    u32 maxSyncPoints = 0;
    for (const Arena &arena : getRendererData<D2>().Arenas)
        maxSyncPoints += arena.Graphics.MemoryRanges.GetSize();
    for (const Arena &arena : getRendererData<D3>().Arenas)
        maxSyncPoints += arena.Graphics.MemoryRanges.GetSize();
    syncPoints.Reserve(maxSyncPoints);

    TKIT_RETURN_IF_FAILED(render<D2>(graphics, command, window, graphicsFlight, syncPoints));
    TKIT_RETURN_IF_FAILED(render<D3>(graphics, command, window, graphicsFlight, syncPoints));

    VkSemaphoreSubmitInfoKHR &rendFinInfo = submitInfo.SignalSemaphores[1];
    rendFinInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    rendFinInfo.pNext = nullptr;
    rendFinInfo.semaphore = window->GetRenderFinishedSemaphore();
    rendFinInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    rendFinInfo.deviceIndex = 0;

    VkSemaphoreSubmitInfoKHR &imgInfo = submitInfo.WaitSemaphores.Append();
    imgInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    imgInfo.pNext = nullptr;
    imgInfo.semaphore = window->GetImageAvailableSemaphore();
    imgInfo.deviceIndex = 0;
    imgInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfoKHR &gtimSemInfo = submitInfo.SignalSemaphores[0];
    gtimSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    gtimSemInfo.pNext = nullptr;
    gtimSemInfo.value = graphicsFlight;
    gtimSemInfo.deviceIndex = 0;
    gtimSemInfo.semaphore = graphics->GetTimelineSempahore();
    gtimSemInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

    for (const TransferSyncPoint &sp : syncPoints)
    {
        VkSemaphoreSubmitInfoKHR &ttimSemInfo = submitInfo.WaitSemaphores.Append();
        ttimSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
        ttimSemInfo.pNext = nullptr;
        ttimSemInfo.semaphore = sp.Transfer->GetTimelineSempahore();
        ttimSemInfo.deviceIndex = 0;
        ttimSemInfo.value = sp.TransferFlightValue;
        ttimSemInfo.stageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
    }
    submitInfo.InFlightValue = graphicsFlight;
    return submitInfo;
}

Result<> SubmitRender(VKit::Queue *graphics, CommandPool *pool, const TKit::Span<const RenderSubmitInfo> info)
{
    TKit::StackArray<VkSubmitInfo2KHR> submits{};
    submits.Reserve(info.GetSize());

    TKit::StackArray<VkCommandBufferSubmitInfoKHR> cmds{};
    cmds.Reserve(info.GetSize());

    u64 maxFlight = 0;
    for (const RenderSubmitInfo &info : info)
    {
        TKIT_ASSERT(info.Command,
                    "[ONYX][RENDERER] A submission must have a valid graphics command buffer to be submitted");
        if (info.InFlightValue > maxFlight)
            maxFlight = info.InFlightValue;

        VkSubmitInfo2KHR &sinfo = submits.Append();
        sinfo = {};
        sinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
        if (!info.WaitSemaphores.IsEmpty())
        {
            sinfo.waitSemaphoreInfoCount = info.WaitSemaphores.GetSize();
            sinfo.pWaitSemaphoreInfos = info.WaitSemaphores.GetData();
        }

        sinfo.signalSemaphoreInfoCount = info.SignalSemaphores.GetSize();
        sinfo.pSignalSemaphoreInfos = info.SignalSemaphores.GetData();

        VkCommandBufferSubmitInfoKHR &cmd = cmds.Append();
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
        cmd.pNext = nullptr;
        cmd.commandBuffer = info.Command;
        cmd.deviceMask = 0;

        sinfo.commandBufferInfoCount = 1;
        sinfo.pCommandBufferInfos = &cmd;
        sinfo.flags = 0;
    }

    pool->MarkInUse(graphics, maxFlight);
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
            if (trange.InUse())
            {
                if (tmergeRange.Size != 0)
                {
                    tranges.Append(tmergeRange);
                    tmergeRange.Offset = tmergeRange.Size + trange.Size;
                    tmergeRange.Size = 0;
                }
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
                granges.Append(grange);
            }
            else if (!grange.ContextRanges.IsEmpty())
            {
                TKit::StackArray<ContextRange> cranges{};
                cranges.Reserve(grange.ContextRanges.GetSize());

                grange.Size = 0;
                grange.ViewMask = 0;
                grange.Transfer = nullptr;
                grange.Graphics = nullptr;
                for (ContextRange &crange : grange.ContextRanges)
                    if (rdata.IsContextRangeClean(crange))
                    {
                        TKIT_ASSERT(grange.Size != 0,
                                    "[ONYX][RENDERER] Graphics memory range should not have reached a zero "
                                    "size if there are context ranges left");
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
                granges.Append(grange);
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

template RenderContext<D2> *CreateContext();
template RenderContext<D3> *CreateContext();

template void DestroyContext(RenderContext<D2> *context);
template void DestroyContext(RenderContext<D3> *context);

template void BindStaticMeshes<D2>(VkCommandBuffer command);
template void BindStaticMeshes<D3>(VkCommandBuffer command);

template void DrawStaticMesh<D2>(VkCommandBuffer command, Mesh mesh, u32 firstInstance, u32 instanceCount);
template void DrawStaticMesh<D3>(VkCommandBuffer command, Mesh mesh, u32 firstInstance, u32 instanceCount);

} // namespace Onyx::Renderer
