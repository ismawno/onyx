#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/core/limits.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/app/window.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/state/pipelines.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/execution/execution.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/rendering/context.hpp"
#include "vkit/resource/device_buffer.hpp"
#include "tkit/memory/block_allocator.hpp"
#include "tkit/multiprocessing/task_manager.hpp"

namespace Onyx::Renderer
{
using namespace Detail;
static bool inUse(const VKit::Queue *p_Queue, const u64 p_InFlightValue)
{
    return p_Queue && p_Queue->GetCompletedTimeline() < p_InFlightValue;
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
    TKit::Array<ContextRange, MaxRenderContexts> ContextRanges{};

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
    TKit::Array<TransferMemoryRange, MaxRendererRanges> MemoryRanges{};
};
struct GraphicsArena
{
    VKit::DeviceBuffer Buffer{};
    TKit::Array<GraphicsMemoryRange, MaxRendererRanges> MemoryRanges{};
};

template <Dimension D> struct RendererData
{
    TKit::BlockAllocator ContextAllocator = TKit::BlockAllocator::CreateFromType<RenderContext<D>>(MaxRenderContexts);
    TKit::Array<RenderContext<D> *, MaxRenderContexts> Contexts{};
    TKit::Array<u64, MaxRenderContexts> Generations{};
    TKit::FixedArray<TransferArena, Primitive_Count> TransferArenas{};
    TKit::FixedArray<GraphicsArena, Primitive_Count> GraphicsArenas{};
    TKit::FixedArray<VKit::GraphicsPipeline, StencilPass_Count> StatMeshPipelines{};
    TKit::FixedArray<VKit::GraphicsPipeline, StencilPass_Count> CirclePipelines{};
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

template <Dimension D> static VkDeviceSize getInstanceSize(const PrimitiveType p_Type)
{
    switch (p_Type)
    {
    case Primitive_StaticMesh:
        return sizeof(InstanceData<D>);
    case Primitive_Circle:
        return sizeof(CircleInstanceData<D>);
    case Primitive_Count:
        TKIT_FATAL("[ONYX] Unrecognized primitive type");
        return 0;
    }
    TKIT_FATAL("[ONYX] Unrecognized primitive type");
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

template <Dimension D> static void initialize()
{
    RendererData<D> &rdata = getRendererData<D>();

    const VkPipelineRenderingCreateInfoKHR renderInfo = CreatePipelineRenderingCreateInfo();
    for (u32 i = 0; i < Primitive_Count; ++i)
    {
        const PrimitiveType type = static_cast<PrimitiveType>(i);
        TransferArena &tarena = rdata.TransferArenas[type];
        tarena.Buffer = Resources::CreateBuffer(Buffer_Staging, getInstanceSize<D>(type));
        tarena.MemoryRanges.Append(TransferMemoryRange{.Size = tarena.Buffer.GetInfo().Size});

        GraphicsArena &garena = rdata.GraphicsArenas[type];
        garena.Buffer = Resources::CreateBuffer(Buffer_DeviceStorage, getInstanceSize<D>(type));
        garena.MemoryRanges.Append(GraphicsMemoryRange{.Size = garena.Buffer.GetInfo().Size});
    }
    for (u32 i = 0; i < StencilPass_Count; ++i)
    {
        const StencilPass pass = static_cast<StencilPass>(i);
        rdata.StatMeshPipelines[pass] = Pipelines::CreateStaticMeshPipeline<D>(pass, renderInfo);
        rdata.CirclePipelines[pass] = Pipelines::CreateCirclePipeline<D>(pass, renderInfo);
    }
}

template <Dimension D> static void terminate()
{
    RendererData<D> &rdata = getRendererData<D>();
    Core::DeviceWaitIdle();
    for (u32 prtype = 0; prtype < Primitive_Count; ++prtype)
    {
        TransferArena &tarena = rdata.TransferArenas[prtype];
        tarena.Buffer.Destroy();
        GraphicsArena &garena = rdata.GraphicsArenas[prtype];
        garena.Buffer.Destroy();
    }
    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
    {
        rdata.StatMeshPipelines[pass].Destroy();
        rdata.CirclePipelines[pass].Destroy();
    }
}

void Initialize()
{
    initialize<D2>();
    initialize<D3>();
}
void Terminate()
{
    terminate<D2>();
    terminate<D3>();
}

template <Dimension D> RenderContext<D> *CreateContext()
{
    RendererData<D> &rdata = getRendererData<D>();
    RenderContext<D> *ctx = rdata.ContextAllocator.template Create<RenderContext<D>>();
    rdata.Contexts.Append(ctx);
    rdata.Generations.Append(ctx->GetGeneration());
    return ctx;
}

template <Dimension D> void DestroyContext(const RenderContext<D> *p_Context)
{
    RendererData<D> &rdata = getRendererData<D>();
    u32 index = TKIT_U32_MAX;
    for (u32 i = 0; i < rdata.Contexts.GetSize(); ++i)
        if (rdata.Contexts[i] == p_Context)
        {
            index = i;
            break;
        }
    TKIT_ASSERT(index != TKIT_U32_MAX, "[ONYX] Render context not found when attempting to destroy it");
    for (GraphicsArena &garena : rdata.GraphicsArenas)
        for (GraphicsMemoryRange &grange : garena.MemoryRanges)
            for (ContextRange &crange : grange.ContextRanges)
                if (crange.ContextIndex > index)
                    --crange.ContextIndex;
                else if (crange.ContextIndex == index)
                    crange.ContextIndex = TKIT_U32_MAX;
}

template <Dimension D> static void clearWindow(const Window *p_Window)
{
    RendererData<D> &rdata = getRendererData<D>();
    const u64 viewBit = p_Window->GetViewBit();
    for (RenderContext<D> *ctx : rdata.Contexts)
        ctx->RemoveTarget(p_Window);

    for (GraphicsArena &garena : rdata.GraphicsArenas)
        for (GraphicsMemoryRange &grange : garena.MemoryRanges)
        {
            for (ContextRange &crange : grange.ContextRanges)
                crange.ViewMask &= ~viewBit;
            grange.ViewMask &= ~viewBit;
        }
}

void ClearWindow(const Window *p_Window)
{
    clearWindow<D2>(p_Window);
    clearWindow<D3>(p_Window);
}

static TransferMemoryRange &findTransferRange(TransferArena &p_Arena, const VkDeviceSize p_RequiredMem)
{
    auto &ranges = p_Arena.MemoryRanges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX] Memory ranges cannot be empty");

    VKit::DeviceBuffer &buffer = p_Arena.Buffer;
    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        TransferMemoryRange &range = ranges[i];
        if (range.Size >= p_RequiredMem && !range.InUse())
        {
            if (range.Size == p_RequiredMem)
                return range;

            TransferMemoryRange child;
            child.Size = p_RequiredMem;
            child.Offset = range.Offset;

            range.Offset += p_RequiredMem;
            range.Size -= p_RequiredMem;

            ranges.Insert(ranges.begin() + i, child);
            return ranges[i];
        }
    }

    const VkDeviceSize isize = buffer.GetInfo().InstanceSize;
    const VkDeviceSize icount = buffer.GetInfo().InstanceCount;
    const VkDeviceSize size = buffer.GetInfo().Size;

    Core::DeviceWaitIdle();
    buffer.Destroy();
    buffer = Resources::CreateBuffer(Buffer_Staging, isize, 2 * icount);

    TransferMemoryRange bigRange;
    bigRange.Transfer = nullptr;
    bigRange.TransferFlightValue = 0;
    bigRange.Offset = size + p_RequiredMem;
    bigRange.Size = size - p_RequiredMem;

    TransferMemoryRange smallRange;
    smallRange.Offset = size;
    smallRange.Size = p_RequiredMem;

    ranges.Append(smallRange);
    ranges.Append(bigRange);

    return ranges[ranges.GetSize() - 2];
}

static TransferMemoryRange &assignTransferRange(const VKit::Queue *p_Transfer, TransferArena &p_Arena,
                                                const VkDeviceSize p_RequiredMem, const u64 p_TransferFlightValue)
{
    TransferMemoryRange &range = findTransferRange(p_Arena, p_RequiredMem);
    range.Transfer = p_Transfer;
    range.TransferFlightValue = p_TransferFlightValue;
    return range;
}

static GraphicsMemoryRange &findGraphicsRange(GraphicsArena &p_Arena, const VkDeviceSize p_RequiredMem)
{
    auto &ranges = p_Arena.MemoryRanges;
    TKIT_ASSERT(!ranges.IsEmpty(), "[ONYX] Memory ranges cannot be empty");

    VKit::DeviceBuffer &buffer = p_Arena.Buffer;
    for (u32 i = 0; i < ranges.GetSize(); ++i)
    {
        GraphicsMemoryRange &range = ranges[i];
        // when reaching here, all free device memory ranges must have been curated. non dirty contexts now have a
        // memory range for themselves, and free memory ranges must have a u32 max batch index
        if (range.Size >= p_RequiredMem && range.ContextRanges.IsEmpty() && !range.InUse())
        {
            if (range.Size == p_RequiredMem)
                return range;

            GraphicsMemoryRange child;
            child.Size = p_RequiredMem;
            child.Offset = range.Offset;

            range.Offset += p_RequiredMem;
            range.Size -= p_RequiredMem;

            ranges.Insert(ranges.begin() + i, child);
            return ranges[i];
        }
    }

    const VkDeviceSize isize = buffer.GetInfo().InstanceSize;
    const VkDeviceSize icount = buffer.GetInfo().InstanceCount;
    const VkDeviceSize size = buffer.GetInfo().Size;

    Core::DeviceWaitIdle();

    buffer.Destroy();
    buffer = Resources::CreateBuffer(Buffer_DeviceStorage, isize, 2 * icount);

    GraphicsMemoryRange bigRange{};
    bigRange.Offset = size + p_RequiredMem;
    bigRange.Size = size - p_RequiredMem;

    GraphicsMemoryRange smallRange{};
    smallRange.Offset = size;
    smallRange.Size = p_RequiredMem;

    ranges.Append(smallRange);
    ranges.Append(bigRange);

    return ranges[ranges.GetSize() - 2];
}

static GraphicsMemoryRange &assignGraphicsRange(const VKit::Queue *p_Transfer, GraphicsArena &p_Arena,
                                                const VkDeviceSize p_RequiredMem, const u64 p_TransferFlightValue)
{
    GraphicsMemoryRange &range = findGraphicsRange(p_Arena, p_RequiredMem);
    range.Transfer = p_Transfer;
    range.TransferFlightValue = p_TransferFlightValue;
    return range;
}

VkBufferMemoryBarrier2KHR createAcquireBarrier(const VkBuffer p_DeviceLocalBuffer, const VkDeviceSize p_Offset,
                                               const VkDeviceSize p_Size)
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
    barrier.buffer = p_DeviceLocalBuffer;
    barrier.offset = p_Offset;
    barrier.size = p_Size;

    return barrier;
}
VkBufferMemoryBarrier2KHR createReleaseBarrier(const VkBuffer p_DeviceLocalBuffer, const VkDeviceSize p_Offset,
                                               const VkDeviceSize p_Size)
{
    const u32 qsrc = Execution::GetFamilyIndex(VKit::Queue_Transfer);
    const u32 qdst = Execution::GetFamilyIndex(VKit::Queue_Graphics);
    TKIT_ASSERT(
        qsrc != qdst,
        "[ONYX] Cannot create a release barrier if the graphics and transfer Execution belong to the same family");

    VkBufferMemoryBarrier2KHR barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_2_NONE_KHR;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
    barrier.srcQueueFamilyIndex = qsrc;
    barrier.dstQueueFamilyIndex = qdst;
    barrier.buffer = p_DeviceLocalBuffer;
    barrier.offset = p_Offset;
    barrier.size = p_Size;

    return barrier;
}

using BarrierArray = TKit::Array<VkBufferMemoryBarrier2KHR, MaxBatches>;

template <Dimension D>
static void transfer(VKit::Queue *p_Transfer, const VkCommandBuffer p_Command, TransferSubmitInfo &p_Info,
                     BarrierArray *p_Release, const u64 p_TransferFlightValue)
{
    struct ContextInfo
    {
        const RenderContext<D> *Context;
        u32 Index;
    };

    RendererData<D> &rdata = getRendererData<D>();
    TKit::Array<ContextInfo, MaxRenderContexts> dirtyContexts{};
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
        return;

    TKit::Array<ContextRange, MaxRenderContexts> contextRanges{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    TKit::Array<Task, MaxTasks> tasks{};
    u32 sindex = 0;

    const auto processBatches = [&](const u32 p_Pass, const PrimitiveType p_Type, const u32 p_BatchStart,
                                    const u32 p_BatchEnd) {
        TransferArena &tarena = rdata.TransferArenas[p_Type];
        GraphicsArena &garena = rdata.GraphicsArenas[p_Type];

        TKit::Array<VkBufferCopy2KHR, MaxBatches> copies{};
        for (u32 batch = p_BatchStart; batch < p_BatchEnd; ++batch)
        {
            contextRanges.Clear();
            VkDeviceSize requiredMem = 0;
            u64 viewMask = 0;
            for (const ContextInfo &cinfo : dirtyContexts)
            {
                const RenderContext<D> *ctx = cinfo.Context;
                const auto &idata = ctx->GetInstanceData()[p_Pass][batch];
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

            TransferMemoryRange &trange = assignTransferRange(p_Transfer, tarena, requiredMem, p_TransferFlightValue);
            for (const ContextRange &crange : contextRanges)
            {
                const RenderContext<D> *ctx = rdata.Contexts[crange.ContextIndex];

                const auto &idata = ctx->GetInstanceData()[p_Pass][batch];
                const auto copy = [&, crange, trange] {
                    tarena.Buffer.Write(
                        idata.Data.GetData(),
                        {.srcOffset = 0, .dstOffset = trange.Offset + crange.Offset, .size = crange.Size});
                };

                Task &task = tasks.Append(copy);
                sindex = tm->SubmitTask(&task, sindex);
            }

            GraphicsMemoryRange &grange = assignGraphicsRange(p_Transfer, garena, requiredMem, p_TransferFlightValue);
            grange.BatchIndex = batch;
            grange.ContextRanges = contextRanges;
            grange.ViewMask = viewMask;
            grange.Pass = static_cast<StencilPass>(p_Pass);
            grange.Transfer = p_Transfer;
            grange.Barrier = createAcquireBarrier(garena.Buffer, grange.Offset, requiredMem);

            VkBufferCopy2KHR &copy = copies.Append();
            copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR;
            copy.pNext = nullptr;
            copy.srcOffset = trange.Offset;
            copy.dstOffset = grange.Offset;
            copy.size = requiredMem;

            if (p_Release)
                p_Release->Append(createReleaseBarrier(garena.Buffer, grange.Offset, requiredMem));
        }
        if (!copies.IsEmpty())
            garena.Buffer.CopyFromBuffer2(p_Command, tarena.Buffer, copies);
    };

    for (u32 pass = 0; pass < StencilPass_Count; ++pass)
    {
        processBatches(pass, Primitive_StaticMesh, BatchRangeStart_StaticMesh, BatchRangeEnd_StaticMesh);
        // processBatches(pass, Primitive_Circle, BatchRangeStart_Circle, BatchRangeEnd_Circle,
        //                );
    }

    p_Info.Command = p_Command;

    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
}

TransferSubmitInfo Transfer(VKit::Queue *p_Transfer, const VkCommandBuffer p_Command)
{
    TransferSubmitInfo submitInfo{};
    const bool separate = Execution::IsSeparateTransferMode();
    BarrierArray release{};

    const u64 transferFlight = p_Transfer->NextTimelineValue();
    transfer<D2>(p_Transfer, p_Command, submitInfo, separate ? &release : nullptr, transferFlight);
    transfer<D3>(p_Transfer, p_Command, submitInfo, separate ? &release : nullptr, transferFlight);
    if (separate)
    {
        VkDependencyInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        info.bufferMemoryBarrierCount = release.GetSize();
        info.pBufferMemoryBarriers = release.GetData();
        info.dependencyFlags = 0;
        const auto &table = Core::GetDeviceTable();
        table.CmdPipelineBarrier2KHR(p_Command, &info);
    }
    if (submitInfo)
    {
        VkSemaphoreSubmitInfoKHR semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
        semInfo.semaphore = p_Transfer->GetTimelineSempahore();
        semInfo.value = transferFlight;
        semInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
        submitInfo.SignalSemaphore = semInfo;
        submitInfo.InFlightValue = transferFlight;
    }
    return submitInfo;
}

void SubmitTransfer(VKit::Queue *p_Transfer, CommandPool &p_Pool, TKit::Span<const TransferSubmitInfo> p_Info)
{
    TKit::Array16<VkSubmitInfo2KHR> submits{};
    TKit::Array16<VkCommandBufferSubmitInfoKHR> cmds{};
    u64 maxFlight = 0;
    for (const TransferSubmitInfo &info : p_Info)
    {
        TKIT_ASSERT(info.Command, "[ONYX] A submission must have a valid transfer command buffer to be submitted");
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

    p_Pool.MarkInUse(p_Transfer, maxFlight);
    VKIT_CHECK_EXPRESSION(p_Transfer->Submit2(submits));
}

template <Dimension D> void applyAcquireBarriers(BarrierArray &p_Barriers)
{
    const RendererData<D> &rdata = getRendererData<D>();
    for (const GraphicsArena &garena : rdata.GraphicsArenas)
        for (const GraphicsMemoryRange &grange : garena.MemoryRanges)
            if (grange.InUseByTransfer())
                p_Barriers.Append(grange.Barrier);
}

void ApplyAcquireBarriers(const VkCommandBuffer p_GraphicsCommand)
{
    BarrierArray barriers{};
    applyAcquireBarriers<D2>(barriers);
    applyAcquireBarriers<D3>(barriers);
    if (!barriers.IsEmpty())
    {
        const auto &table = Core::GetDeviceTable();
        VkDependencyInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        info.bufferMemoryBarrierCount = barriers.GetSize();
        info.pBufferMemoryBarriers = barriers.GetData();
        info.dependencyFlags = 0;
        table.CmdPipelineBarrier2KHR(p_GraphicsCommand, &info);
    }
}

template <Dimension D>
static void setCameraViewport(const VkCommandBuffer p_Command, const Detail::CameraInfo<D> &p_Camera)
{
    const auto &table = Core::GetDeviceTable();
    if (!p_Camera.Transparent)
    {
        const Color &bg = p_Camera.BackgroundColor;
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
        clearRect.rect.offset = {static_cast<i32>(p_Camera.Viewport.x), static_cast<i32>(p_Camera.Viewport.y)};
        clearRect.rect.extent = {static_cast<u32>(p_Camera.Viewport.width), static_cast<u32>(p_Camera.Viewport.height)};
        clearRect.layerCount = 1;
        clearRect.baseArrayLayer = 0;

        table.CmdClearAttachments(p_Command, D - 1, clearAttachments.GetData(), 1, &clearRect);
    }
    table.CmdSetViewport(p_Command, 0, 1, &p_Camera.Viewport);
    table.CmdSetScissor(p_Command, 0, 1, &p_Camera.Scissor);
}

template <Dimension D>
static void pushConstantData(const VkCommandBuffer p_Command, const Detail::CameraInfo<D> &p_Camera)
{
    PushConstantData<Shading_Unlit> pdata{};
    pdata.ProjectionView = p_Camera.ProjectionView;

    VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT;
    // if constexpr (Shade == Shading_Lit)
    // {
    //     pdata.ViewPosition = f32v4(p_Camera.Camera->ViewPosition, 1.f);
    //     pdata.AmbientColor = p_Camera.Light.AmbientColor->RGBA;
    //     pdata.DirectionalLightCount = p_Camera.Light.DirectionalCount;
    //     pdata.PointLightCount = p_Camera.Light.PointCount;
    //     stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
    // }
    const auto &table = Core::GetDeviceTable();
    table.CmdPushConstants(p_Command, Pipelines::GetGraphicsPipelineLayout(Shading_Unlit), stages, 0,
                           sizeof(PushConstantData<Shading_Unlit>), &pdata);
}

struct InstanceDrawInfo
{
    u32 FirstInstance;
    u32 InstanceCount;
};

constexpr u32 MaxDrawCallsPerBatch = 8;

struct TransferSyncPoint
{
    const VKit::Queue *Transfer;
    u64 TransferFlightValue;
};

using SyncArray = TKit::Array<TransferSyncPoint, MaxRendererRanges>;

template <Dimension D>
static void render(VKit::Queue *p_Graphics, const VkCommandBuffer p_GraphicsCommand, const Window *p_Window,
                   const u64 p_GraphicsFlightValue, SyncArray &p_SyncPoints)
{
    const auto camInfos = p_Window->GetCameraInfos<D>();
    if (camInfos.IsEmpty())
        return;

    const u64 viewBit = p_Window->GetViewBit();
    const auto &device = Core::GetDevice();

    RendererData<D> &rdata = getRendererData<D>();

    u32 batches = 0;
    TKit::FixedArray<TKit::FixedArray<TKit::Array<InstanceDrawInfo, MaxDrawCallsPerBatch>, MaxBatches>,
                     StencilPass_Count>
        drawInfo{};

    const auto collectDrawInfo = [&](const PrimitiveType p_Type) {
        GraphicsArena &garena = rdata.GraphicsArenas[p_Type];
        const VkDeviceSize instanceSize = garena.Buffer.GetInfo().InstanceSize;
        for (GraphicsMemoryRange &grange : garena.MemoryRanges)
        {
            if (!(grange.ViewMask & viewBit) || grange.InUseByGraphics())
                continue;
            TKIT_ASSERT(!grange.ContextRanges.IsEmpty(),
                        "[ONYX] Context ranges cannot be empty for a graphics memory range");
            VkDeviceSize offset = grange.Offset;
            VkDeviceSize size = 0;
            for (const ContextRange &crange : grange.ContextRanges)
            {
                if ((crange.ViewMask & viewBit) && crange.ContextIndex != TKIT_U32_MAX &&
                    !rdata.Contexts[crange.ContextIndex]->IsDirty(crange.Generation))
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
            TKIT_ASSERT(size != 0 || offset > grange.Offset,
                        "[ONYX] Found labeled graphics memory arena range for window with view bit {} with no context "
                        "ranges targetting said view",
                        viewBit);
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
                for (TransferSyncPoint &sp : p_SyncPoints)
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
                    p_SyncPoints.Append(grange.Transfer, grange.TransferFlightValue);
            }
            grange.GraphicsFlightValue = p_GraphicsFlightValue;
        }
    };

    collectDrawInfo(Primitive_StaticMesh);
    // collectDrawInfo(Primitive_Circle);

    if (batches == 0)
        return;

    for (const Detail::CameraInfo<D> &camInfo : camInfos)
    {
        setCameraViewport<D>(p_GraphicsCommand, camInfo);
        for (u32 pass = 0; pass < StencilPass_Count; ++pass)
        {
            DescriptorSet &set =
                Descriptors::FindSuitableDescriptorSet(rdata.GraphicsArenas[Primitive_StaticMesh].Buffer);

            rdata.StatMeshPipelines[pass].Bind(p_GraphicsCommand);
            BindStaticMeshes<D>(p_GraphicsCommand);
            pushConstantData(p_GraphicsCommand, camInfo);

            VKit::DescriptorSet::Bind(device, p_GraphicsCommand, set.Set, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      Pipelines::GetGraphicsPipelineLayout(Shading_Unlit));
            set.MarkInUse(p_Graphics, p_GraphicsFlightValue);

            for (u32 batch = BatchRangeStart_StaticMesh; batch < BatchRangeEnd_StaticMesh; ++batch)
                for (const InstanceDrawInfo &draw : drawInfo[pass][batch])
                {
                    const Mesh mesh = batch - BatchRangeStart_StaticMesh;
                    DrawStaticMesh<D>(p_GraphicsCommand, mesh, draw.FirstInstance, draw.InstanceCount);
                }
        }
    }
}

RenderSubmitInfo Render(VKit::Queue *p_Graphics, const VkCommandBuffer p_Command, const Window *p_Window)
{
    RenderSubmitInfo submitInfo{};
    submitInfo.Command = p_Command;
    const u64 graphicsFlight = p_Graphics->NextTimelineValue();
    SyncArray syncPoints{};
    render<D2>(p_Graphics, p_Command, p_Window, graphicsFlight, syncPoints);
    render<D3>(p_Graphics, p_Command, p_Window, graphicsFlight, syncPoints);

    VkSemaphoreSubmitInfoKHR &rendFinInfo = submitInfo.SignalSemaphores[1];
    rendFinInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    rendFinInfo.pNext = nullptr;
    rendFinInfo.semaphore = p_Window->GetRenderFinishedSemaphore();
    rendFinInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    rendFinInfo.deviceIndex = 0;

    VkSemaphoreSubmitInfoKHR &imgInfo = submitInfo.WaitSemaphores.Append();
    imgInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    imgInfo.pNext = nullptr;
    imgInfo.semaphore = p_Window->GetImageAvailableSemaphore();
    imgInfo.deviceIndex = 0;
    imgInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfoKHR &gtimSemInfo = submitInfo.SignalSemaphores[0];
    gtimSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    gtimSemInfo.pNext = nullptr;
    gtimSemInfo.value = graphicsFlight;
    gtimSemInfo.deviceIndex = 0;
    gtimSemInfo.semaphore = p_Graphics->GetTimelineSempahore();
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

void SubmitRender(VKit::Queue *p_Graphics, CommandPool &p_Pool, const TKit::Span<const RenderSubmitInfo> p_Info)
{
    TKit::Array16<VkSubmitInfo2KHR> submits{};
    TKit::Array16<VkCommandBufferSubmitInfoKHR> cmds{};
    u64 maxFlight = 0;
    for (const RenderSubmitInfo &info : p_Info)
    {
        TKIT_ASSERT(info.Command, "[ONYX] A submission must have a valid graphics command buffer to be submitted");
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

    p_Pool.MarkInUse(p_Graphics, maxFlight);
    VKIT_CHECK_EXPRESSION(p_Graphics->Submit2(submits));
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
    for (TransferArena &tarena : rdata.TransferArenas)
    {
        TransferMemoryRange mergeRange{};
        TKit::Array<TransferMemoryRange, MaxRendererRanges> tranges{};
        for (TransferMemoryRange &trange : tarena.MemoryRanges)
        {
            if (trange.InUse())
            {
                if (mergeRange.Size != 0)
                {
                    tranges.Append(mergeRange);
                    mergeRange.Offset = mergeRange.Size + trange.Size;
                    mergeRange.Size = 0;
                }
                tranges.Append(trange);
            }
            else
                mergeRange.Size += trange.Size;
        }
        if (mergeRange.Size != 0)
            tranges.Append(mergeRange);
        tarena.MemoryRanges = tranges;
        TKIT_ASSERT(!tranges.IsEmpty(),
                    "[ONYX] All memory ranges for the transfer arena have been removed after coalesce operation!");
    }
    for (GraphicsArena &garena : rdata.GraphicsArenas)
    {
        GraphicsMemoryRange mergeRange{};
        TKit::Array<GraphicsMemoryRange, MaxRendererRanges> granges{};
        for (GraphicsMemoryRange &grange : garena.MemoryRanges)
        {
            if (grange.InUse())
            {
                if (mergeRange.Size != 0)
                {
                    granges.Append(mergeRange);
                    mergeRange.Offset = mergeRange.Size + grange.Size;
                    mergeRange.Size = 0;
                }
                granges.Append(grange);
            }
            else if (!grange.ContextRanges.IsEmpty())
            {
                TKit::Array<ContextRange, MaxRenderContexts> cranges{};
                grange.Size = 0;
                grange.ViewMask = 0;
                grange.Transfer = nullptr;
                grange.Graphics = nullptr;
                for (ContextRange &crange : grange.ContextRanges)
                    if (crange.ViewMask != 0 && crange.ContextIndex != TKIT_U32_MAX &&
                        !rdata.Contexts[crange.ContextIndex]->IsDirty(crange.Generation))
                    {
                        TKIT_ASSERT(grange.Size != 0, "[ONYX] Graphics memory range should not have reached a zero "
                                                      "size if there are context ranges left");
                        if (mergeRange.Size != 0)
                        {
                            granges.Append(mergeRange);
                            mergeRange.Size = 0;
                        }
                        mergeRange.Offset += crange.Size;
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
                        mergeRange.Size += crange.Size;
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
        if (mergeRange.Size != 0)
            granges.Append(mergeRange);
        garena.MemoryRanges = granges;

        TKIT_ASSERT(!granges.IsEmpty(),
                    "[ONYX] All memory ranges for the graphics arena have been removed after coalesce operation");
    }
}

void Coalesce()
{
    coalesce<D2>();
    coalesce<D3>();
}

template <Dimension D> void BindStaticMeshes(const VkCommandBuffer p_Command)
{
    const VKit::DeviceBuffer &vbuffer = Assets::GetStaticMeshVertexBuffer<D>();
    const VKit::DeviceBuffer &ibuffer = Assets::GetStaticMeshIndexBuffer<D>();

    vbuffer.BindAsVertexBuffer(p_Command);
    ibuffer.BindAsIndexBuffer<Index>(p_Command);
}

template <Dimension D>
void DrawStaticMesh(const VkCommandBuffer p_Command, const Mesh p_Mesh, const u32 p_FirstInstance,
                    const u32 p_InstanceCount)
{
    const MeshDataLayout layout = Assets::GetStaticMeshLayout<D>(p_Mesh);
    const auto &table = Core::GetDeviceTable();
    table.CmdDrawIndexed(p_Command, layout.IndexCount, p_InstanceCount, layout.IndexStart, layout.VertexStart,
                         p_FirstInstance);
}

template RenderContext<D2> *CreateContext();
template RenderContext<D3> *CreateContext();

template void DestroyContext(const RenderContext<D2> *p_Context);
template void DestroyContext(const RenderContext<D3> *p_Context);

template void BindStaticMeshes<D2>(VkCommandBuffer p_Command);
template void BindStaticMeshes<D3>(VkCommandBuffer p_Command);

template void DrawStaticMesh<D2>(VkCommandBuffer p_Command, Mesh p_Mesh, u32 p_FirstInstance, u32 p_InstanceCount);
template void DrawStaticMesh<D3>(VkCommandBuffer p_Command, Mesh p_Mesh, u32 p_FirstInstance, u32 p_InstanceCount);

} // namespace Onyx::Renderer
