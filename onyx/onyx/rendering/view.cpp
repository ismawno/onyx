#include "onyx/core/pch.hpp"
#include "onyx/rendering/view.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/platform/platform.hpp"
#include "onyx/state/descriptors.hpp"
#include "vkit/state/descriptor_set.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
// TODO(Isma): Consider having 2D and 3D view sets
static ViewMask s_ViewCache = TKit::Limits<ViewMask>::Max();
static ViewMask allocateViewBit()
{
    TKIT_ASSERT(s_ViewCache != 0,
                "[ONYX][WINDOW] Maximum amount of windows exceeded. There is a hard limit of {} windows",
                8 * sizeof(ViewMask));

    const u32 index = u32(std::countr_zero(s_ViewCache));
    const ViewMask viewBit = 1 << index;
    s_ViewCache &= ~viewBit;
    return viewBit;
}
static void deallocateViewBit(const ViewMask viewBit)
{
    s_ViewCache |= viewBit;
}

template <Dimension D> static void applyCoordinateSystemExtrinsic(f32m<D> &transform)
{
    // Essentially, a rotation around the x axis
    for (u32 i = 0; i < D + 1; ++i)
        for (u32 j = 1; j < D; ++j)
            transform[i][j] = -transform[i][j];
}

VkRect2D ScreenScissor::AsVulkanScissor(const VkExtent2D &parent, const ScreenViewport &viewport) const
{
    const f32v2 size = viewport.Max - viewport.Min;
    const f32v2 min = viewport.Min + 0.5f * (1.f + Min) * size;
    const f32v2 max = viewport.Min + 0.5f * (1.f + Max) * size;

    VkRect2D scissor;
    scissor.offset.x = i32(0.5f * (1.f + min[0]) * parent.width);
    scissor.offset.y = i32(0.5f * (1.f - max[1]) * parent.height);
    scissor.extent.width = u32(0.5f * (1.f + max[0]) * parent.width) - u32(scissor.offset.x);
    scissor.extent.height = u32(0.5f * (1.f - min[1]) * parent.height) - u32(scissor.offset.y);

    return scissor;
}
VkViewport ScreenViewport::AsVulkanViewport(const VkExtent2D &parent) const
{
    VkViewport viewport;
    viewport.x = 0.5f * (1.f + Min[0]) * parent.width;
    viewport.y = 0.5f * (1.f - Max[1]) * parent.height;
    viewport.width = 0.5f * (1.f + Max[0]) * parent.width - viewport.x;
    viewport.height = 0.5f * (1.f - Min[1]) * parent.height - viewport.y;
    viewport.minDepth = DepthBounds[0];
    viewport.maxDepth = DepthBounds[1];

    return viewport;
}
VkExtent2D ScreenViewport::AsVulkanExtent(const VkExtent2D &parent) const
{
    VkExtent2D ext;
    // TODO(Isma): If this becomes negative we have a problem
    ext.width = u32(0.5f * parent.width * (Max[0] - Min[0]));
    ext.height = u32(0.5f * parent.height * (Max[1] - Min[1]));
    return ext;
}

template <Dimension D>
RenderView<D>::RenderView(const VkExtent2D &extent, const VkDescriptorSet compositorSet, const u32 id,
                          Camera<D> *camera, const RenderViewFlags flags, const ScreenViewport &viewport,
                          const ScreenScissor &scissor)
    : m_Camera(camera), m_Viewport(viewport), m_Scissor(scissor), m_ParentExtent(extent),
      m_CompositorSet(compositorSet), m_Id(id), m_Flags(flags)
{
    m_ViewBit = allocateViewBit();
}
template <Dimension D> RenderView<D>::~RenderView()
{
    ONYX_CHECK_EXPRESSION(drainWork());
    destroyFrameBuffers();

    Renderer::RemoveTarget(m_ViewBit);
    deallocateViewBit(m_ViewBit);
}

template <Dimension D> Result<TKit::TierArray<FrameBuffer>> RenderView<D>::createFramebuffers(const u32 imageCount)
{
    TKit::TierArray<FrameBuffer> fbs{};
    const auto &device = GetDevice();
    const VmaAllocator alloc = GetVulkanAllocator();
    const VkExtent2D extent = m_Viewport.AsVulkanExtent(m_ParentExtent);
    for (u32 i = 0; i < imageCount; ++i)
    {
        FrameBuffer &fb = fbs.Append();
        auto result = VKit::DeviceImage::Builder(device, alloc, extent, Platform::GetColorFormat(),
                                                 VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Sampled)
                          .AddImageView()
                          .Build();

        TKIT_RETURN_ON_ERROR(result);
        fb.Color = result.GetValue();
        // TODO(Isma): To sample for outlines, this will need _Sampled
        result =
            VKit::DeviceImage::Builder(device, alloc, extent, Platform::GetDepthStencilFormat(),
                                       VKit::DeviceImageFlag_DepthAttachment | VKit::DeviceImageFlag_StencilAttachment)
                .AddImageView()
                .Build();
        TKIT_RETURN_ON_ERROR(result);
        fb.DepthStencil = result.GetValue();
    }

    VKit::DescriptorSet::Writer writer{GetDevice(), &Descriptors::GetCompositorDescriptorLayout()};

    TKit::StackArray<VkDescriptorImageInfo> infos{};
    infos.Reserve(fbs.GetSize());
    for (u32 i = 0; i < imageCount; ++i)
    {
        VkDescriptorImageInfo &info = infos.Append();
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = fbs[i].Color.GetView();

        writer.WriteImage(Descriptors::GetCompositorColorAttachmentsBindingPoint(), info, m_Id * imageCount + i);
    }
    writer.Overwrite(m_CompositorSet);

    return fbs;
}
template <Dimension D> Result<> RenderView<D>::recreateFrameBuffers(const u32 imageCount)
{
    const auto fbs = createFramebuffers(imageCount);
    TKIT_RETURN_ON_ERROR(fbs);

    destroyFrameBuffers();
    m_FrameBuffers = fbs.GetValue();

    if (IsDebugUtilsEnabled())
        return nameFramebuffers();

    return Result<>::Ok();
}
template <Dimension D> Result<> RenderView<D>::nameFramebuffers()
{
    for (u32 i = 0; i < m_FrameBuffers.GetSize(); ++i)
    {
        const std::string cname = TKit::Format("onyx-color-attachment-view-{}", i);
        const std::string dname = TKit::Format("onyx-depth-attachment-view-{}", i);

        TKIT_RETURN_IF_FAILED(m_FrameBuffers[i].Color.SetName(cname.c_str()));
        TKIT_RETURN_IF_FAILED(m_FrameBuffers[i].DepthStencil.SetName(dname.c_str()));
    }
    return Result<>::Ok();
}

template <Dimension D> Result<> RenderView<D>::drainWork()
{
    TKit::StackArray<VkSemaphore> semaphores{};
    semaphores.Reserve(m_FrameBuffers.GetSize());
    TKit::StackArray<u64> values{};
    values.Reserve(m_FrameBuffers.GetSize());
    for (const FrameBuffer &fb : m_FrameBuffers)
        if (fb.Tracker.InFlight())
        {
            semaphores.Append(fb.Tracker.Queue->GetTimelineSempahore());
            values.Append(fb.Tracker.InFlightValue);
        }
    if (!semaphores.IsEmpty())
    {
        const auto table = GetDeviceTable();
        VkSemaphoreWaitInfoKHR waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
        waitInfo.semaphoreCount = semaphores.GetSize();
        waitInfo.pSemaphores = semaphores.GetData();
        waitInfo.pValues = values.GetData();

        const auto &device = GetDevice();
        VKIT_RETURN_IF_FAILED(table->WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX), Result<>);
    }
    return Result<>::Ok();
}

template <Dimension D> void RenderView<D>::destroyFrameBuffers()
{
    for (FrameBuffer &fb : m_FrameBuffers)
    {
        fb.Color.Destroy();
        fb.DepthStencil.Destroy();
    }
}

template <Dimension D> f32v2 RenderView<D>::ScreenToViewport(const f32v2 &screenPos) const
{
    const f32v2 size = m_Viewport.Max - m_Viewport.Min;
    return -1.f + 2.f * (screenPos - m_Viewport.Min) / size;
}

template <Dimension D> f32v<D> RenderView<D>::ViewportToWorld(f32v<D> viewportPos) const
{
    viewportPos[1] = -viewportPos[1]; // Invert y axis to undo onyx's inversion to GLFW
    const f32m<D> pv = Math::Inverse(m_ProjectionView);
    if constexpr (D == D2)
        return pv * f32v3{viewportPos, 1.f};
    else
    {
        const f32v4 clip = pv * f32v4{viewportPos, 1.f};
        return f32v3{clip} / clip[3];
    }
}

template <Dimension D> f32v2 RenderView<D>::WorldToViewport(const f32v<D> &worldPos) const
{
    if constexpr (D == D2)
    {
        f32v2 viewportPos = f32v2{m_ProjectionView * f32v3{worldPos, 1.f}};
        viewportPos[1] = -viewportPos[1];
        return viewportPos;
    }
    else
    {
        f32v4 clip = m_ProjectionView * f32v4{worldPos, 1.f};
        clip[1] = -clip[1];
        return f32v3{clip} / clip[3];
    }
}

template <Dimension D> f32v2 RenderView<D>::ViewportToScreen(const f32v2 &viewportPos) const
{
    const f32v2 size = m_Viewport.Max - m_Viewport.Min;
    return m_Viewport.Min + 0.5f * (1.f + viewportPos) * size;
}

template <Dimension D> f32v<D> RenderView<D>::ScreenToWorld(const f32v<D> &screenPos) const
{
    if constexpr (D == D2)
        return ViewportToWorld(ScreenToViewport(screenPos));
    else
    {
        const f32 z = screenPos[2];
        return ViewportToWorld(f32v3{ScreenToViewport(f32v2{screenPos}), z});
    }
}

template <Dimension D> void RenderView<D>::BeginRendering(const VkCommandBuffer cmd, const Execution::Tracker &tracker)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginRendering");

    FrameBuffer &fb = m_FrameBuffers[m_ImageIndex];
    fb.Tracker = tracker;

    VkRenderingAttachmentInfoKHR color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color.imageView = fb.Color.GetView();
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = {{ClearColor.rgba[0], ClearColor.rgba[1], ClearColor.rgba[2], ClearColor.rgba[3]}};

    fb.Color.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    // TODO(Isma): Add extra for outline
    VkRenderingAttachmentInfoKHR depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depth.imageView = fb.DepthStencil.GetView();
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil = {1.f, 0};

    fb.DepthStencil.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                      {.DstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR,
                                       .SrcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
                                       .DstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR});

    const VkRenderingAttachmentInfoKHR stencil = depth;
    const VkExtent2D extent = m_Viewport.AsVulkanExtent(m_ParentExtent);

    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &color;
    renderInfo.pDepthAttachment = &depth;
    renderInfo.pStencilAttachment = &stencil;

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

// NOTE: Consider some flags here in case we skip post processing?
template <Dimension D> void RenderView<D>::EndRendering(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndRendering");
    const auto table = GetDeviceTable();

    table->CmdEndRenderingKHR(cmd);
    // TODO(Isma): Transition depth stencil for outlines as well
    m_FrameBuffers[m_ImageIndex].Color.TransitionLayout2(
        cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        {
            .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
            .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,

            .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
        });
}

template <Dimension D> f32m<D> RenderView<D>::ComputeView() const
{
    if (m_Mode == ViewMode_Manual)
        return m_View;

    f32m<D> view = m_Camera->View.ComputeInverseTransform();
    applyCoordinateSystemExtrinsic<D>(view);
    return view;
}
template <Dimension D> f32m<D> RenderView<D>::ComputeProjection() const
{
    if (m_Mode == ViewMode_Manual)
        return m_View;

    const VkViewport viewport = m_Viewport.AsVulkanViewport(m_ParentExtent);
    const f32 aspect = viewport.width / viewport.height;
    if constexpr (D == D2)
        return Transform<D2>::Orthographic(m_Camera->OrthoParameters.Size, aspect);
    else
    {
        const OrthographicParameters<D3> &oparams = m_Camera->OrthoParameters;
        const PerspectiveParameters &pparams = m_Camera->PerspParameters;
        return m_Camera->Mode == CameraMode_Perspective
                   ? Transform<D3>::Perspective(pparams.FieldOfView, pparams.Near, pparams.Far, aspect)
                   : Transform<D3>::Orthographic(oparams.Size, aspect, oparams.Near, oparams.Far);
    }
}

template <Dimension D> void RenderView<D>::ZoomScroll(const f32v<D> &screenPos, const f32 step)
{
    const f32 factor = 1.f - step;
    const auto doZoom = [&] {
        m_Camera->OrthoParameters.Size *= factor;
        const f32v<D> wb = ScreenToWorld(screenPos);
        CacheMatrices();
        const f32v<D> wa = ScreenToWorld(screenPos);
        m_Camera->View.Translation += wb - wa;
    };
    if constexpr (D == D2)
        doZoom();
    else
    {
        if (m_Camera->Mode == CameraMode_Orthographic)
            doZoom();
        else
            m_Camera->PerspParameters.FieldOfView *= factor;
    }
}

template class RenderView<D2>;
template class RenderView<D3>;

} // namespace Onyx
