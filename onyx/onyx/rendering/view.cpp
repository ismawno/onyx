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
    ext.width = u32(0.5f * parent.width * (Max[0] - Min[0]));
    ext.height = u32(0.5f * parent.height * (Max[1] - Min[1]));
    return ext;
}

template <Dimension D>
RenderView<D>::RenderView(const VkExtent2D &extent, const VkDescriptorSet ppSet, const VkDescriptorSet compositorSet,
                          const u32 id, Camera<D> *camera, const RenderViewFlags flags, const ScreenViewport &viewport,
                          const ScreenScissor &scissor)
    : Flags(flags), m_Camera(camera), m_Viewport(viewport), m_Scissor(scissor), m_ParentExtent(extent),
      m_PostProcessSet(ppSet), m_CompositorSet(compositorSet), m_Id(id)
{
    m_ViewBit = allocateViewBit();
}
template <Dimension D> RenderView<D>::~RenderView()
{
    drainWork();
    destroyFrameBuffers();

    Renderer::RemoveTarget(m_ViewBit);
    deallocateViewBit(m_ViewBit);
}

// pass overview
//
// input colors -> linearize -> math -> store srgb (format needs srgb) (to color img if pp enabled, if not, this is
// already pp)
//
// input textures -> linearized bc of srgb -> stored as srgb (to color img if pp enabled, if not, this is
// already pp)
//
// pp takes color as srgb -> linearizes by sample -> stores again in srgb to post process image
//
// compositor loads raw srgb -> stores to unorm, no srgb conversion. color is already srgb so fine
// nice!
template <Dimension D> void RenderView<D>::createFramebuffers(const u32 imageCount)
{
    const auto &device = GetDevice();
    const VmaAllocator alloc = GetVulkanAllocator();
    const VkExtent2D extent = m_Viewport.AsVulkanExtent(m_ParentExtent);
    const VkFormat cformat = Platform::GetColorFormat();
    const VkFormat dformat = Platform::GetDepthStencilFormat();
    const VkFormat sformat = Platform::GetSurfaceFormat().format;

    VkImageSubresourceRange stencilRange{};
    stencilRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    stencilRange.levelCount = 1;
    stencilRange.layerCount = 1;

    const TKit::FixedArray<VkFormat, 2> ppFormats{cformat, sformat};
    for (u32 i = 0; i < imageCount; ++i)
    {
        FrameBuffer &fb = m_FrameBuffers.Append();
        fb.Color = ONYX_CHECK_EXPRESSION(
            VKit::DeviceImage::Builder(device, alloc, extent, cformat,
                                       VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Sampled)
                .AddImageView()
                .Build());

        fb.Outline = ONYX_CHECK_EXPRESSION(
            VKit::DeviceImage::Builder(device, alloc, extent, cformat,
                                       VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Sampled)
                .AddImageView()
                .Build());

        fb.DepthStencil = ONYX_CHECK_EXPRESSION(VKit::DeviceImage::Builder(device, alloc, extent, dformat,
                                                                           VKit::DeviceImageFlag_DepthAttachment |
                                                                               VKit::DeviceImageFlag_StencilAttachment |
                                                                               VKit::DeviceImageFlag_Sampled)
                                                    .AddImageView()
                                                    .AddImageView(stencilRange)
                                                    .Build());

        fb.PostProcess = ONYX_CHECK_EXPRESSION(
            VKit::DeviceImage::Builder(device, alloc, extent, ppFormats,
                                       VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Sampled)
                .AddImageView(cformat)
                .AddImageView(sformat)
                .Build());
    }

    VKit::DescriptorSet::Writer pp{GetDevice(), &Descriptors::GetPostProcessDescriptorLayout()};
    VKit::DescriptorSet::Writer compositor{GetDevice(), &Descriptors::GetCompositorDescriptorLayout()};

    TKit::StackArray<VkDescriptorImageInfo> infos{};
    infos.Reserve(imageCount * 4);
    for (u32 i = 0; i < imageCount; ++i)
    {
        const FrameBuffer &fb = m_FrameBuffers[i];

        VkDescriptorImageInfo &color = infos.Append();
        color.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        color.imageView = fb.Color.GetView();
        color.sampler = Renderer::GetGeneralPurposeSampler();

        const u32 idx = m_Id * imageCount + i;

        pp.WriteImage(Descriptors::GetPostProcessColorAttachmentsBindingPoint(), color, idx);

        VkDescriptorImageInfo &outline = infos.Append();
        outline.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        outline.imageView = fb.Outline.GetView();
        outline.sampler = Renderer::GetGeneralPurposeSampler();
        pp.WriteImage(Descriptors::GetPostProcessOutlineAttachmentsBindingPoint(), outline, idx);

        VkDescriptorImageInfo &stencil = infos.Append();
        stencil.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        stencil.imageView = fb.DepthStencil.GetViews().GetBack();
        stencil.sampler = VK_NULL_HANDLE;
        pp.WriteImage(Descriptors::GetPostProcessStencilAttachmentsBindingPoint(), stencil, idx);

        VkDescriptorImageInfo &postProcess = infos.Append();
        postProcess.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        postProcess.imageView = fb.PostProcess.GetView(1);
        postProcess.sampler = Renderer::GetGeneralPurposeSampler();

        compositor.WriteImage(Descriptors::GetCompositorColorAttachmentsBindingPoint(), postProcess, idx);
    }
    pp.Overwrite(m_PostProcessSet);
    compositor.Overwrite(m_CompositorSet);
    if (IsDebugUtilsEnabled())
        nameFramebuffers();
}
template <Dimension D> void RenderView<D>::recreateFrameBuffers(const u32 imageCount)
{
    destroyFrameBuffers();
    createFramebuffers(imageCount);
}
template <Dimension D> void RenderView<D>::nameFramebuffers()
{
    for (u32 i = 0; i < m_FrameBuffers.GetSize(); ++i)
    {
        const std::string cname = TKit::Format("onyx-color-attachment-{}", i);
        const std::string oname = TKit::Format("onyx-outline-attachment-{}", i);
        const std::string dname = TKit::Format("onyx-depth-attachment-{}", i);
        const std::string pname = TKit::Format("onyx-pp-attachment-{}", i);

        FrameBuffer &fb = m_FrameBuffers[i];
        ONYX_CHECK_EXPRESSION(fb.Color.SetName(cname.c_str()));
        ONYX_CHECK_EXPRESSION(fb.Outline.SetName(oname.c_str()));
        ONYX_CHECK_EXPRESSION(fb.DepthStencil.SetName(dname.c_str()));
        ONYX_CHECK_EXPRESSION(fb.PostProcess.SetName(pname.c_str()));

        ONYX_CHECK_EXPRESSION(fb.Color.SetViewNames(cname.c_str()));
        ONYX_CHECK_EXPRESSION(fb.Outline.SetViewNames(oname.c_str()));
        ONYX_CHECK_EXPRESSION(fb.DepthStencil.SetViewNames(dname.c_str()));
        ONYX_CHECK_EXPRESSION(fb.PostProcess.SetViewNames(pname.c_str()));
    }
}

template <Dimension D> void RenderView<D>::drainWork()
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
        ONYX_CHECK_EXPRESSION(table->WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX));
    }
}

template <Dimension D> void RenderView<D>::destroyFrameBuffers()
{
    for (FrameBuffer &fb : m_FrameBuffers)
    {
        fb.Color.Destroy();
        fb.Outline.Destroy();
        fb.DepthStencil.Destroy();
        fb.PostProcess.Destroy();
    }
    m_FrameBuffers.Clear();
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

    TKit::FixedArray<VkRenderingAttachmentInfoKHR, 2> atts{};
    const bool isIntermediate = Flags & RenderViewFlag_PostProcess;
    const bool hasOutlines = Flags & RenderViewFlag_Outlines;

    VKit::DeviceImage &target = isIntermediate ? fb.Color : fb.PostProcess;

    VkRenderingAttachmentInfoKHR &color = atts[0];
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color.imageView = isIntermediate ? fb.Color.GetView() : fb.PostProcess.GetView();
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    const Color ccolor = ClearColor.ToLinear();
    color.clearValue.color = {{ccolor.rgba[0], ccolor.rgba[1], ccolor.rgba[2], ccolor.rgba[3]}};

    target.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                              .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                              .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    VkRenderingAttachmentInfoKHR &outline = atts[1];
    outline.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    outline.imageView = fb.Outline.GetView();
    outline.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    outline.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    outline.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    outline.clearValue.color = {{0.f, 0.f, 0.f, 0.f}};

    // the src stage ternary kind of doesnt care as there will not be transition if new and old layout are the same

    fb.Outline.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                  .SrcStage = (isIntermediate && hasOutlines)
                                                  ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR
                                                  : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
                                  .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    VkRenderingAttachmentInfoKHR depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depth.imageView = fb.DepthStencil.GetView();
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = (isIntermediate && hasOutlines) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil = {1.f, 0};

    fb.DepthStencil.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                      {.DstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR,
                                       .SrcStage = (isIntermediate && hasOutlines)
                                                       ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR
                                                       : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
                                       .DstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR});

    const VkRenderingAttachmentInfoKHR stencil = depth;
    const VkExtent2D extent = m_Viewport.AsVulkanExtent(m_ParentExtent);

    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 2;
    renderInfo.pColorAttachments = atts.GetData();
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
    FrameBuffer &fb = m_FrameBuffers[m_ImageIndex];

    const bool isIntermediate = Flags & RenderViewFlag_PostProcess;
    const bool hasOutlines = Flags & RenderViewFlag_Outlines;

    VKit::DeviceImage &img = isIntermediate ? fb.Color : fb.PostProcess;
    img.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          {
                              .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                              .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,

                              .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                              .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                          });

    if (isIntermediate)
    {
        fb.Outline.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     {
                                         .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                         .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,

                                         .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                         .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                     });
        if (hasOutlines)
            fb.DepthStencil.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                              {
                                                  .SrcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR,
                                                  .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,

                                                  .SrcStage = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
                                                  .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                              });
    }
}

template <Dimension D> void RenderView<D>::BeginPostProcess(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::BeginPostProcess");

    FrameBuffer &fb = m_FrameBuffers[m_ImageIndex];

    VkRenderingAttachmentInfoKHR pp{};
    pp.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    pp.imageView = fb.PostProcess.GetView();
    pp.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    pp.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    pp.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    fb.PostProcess.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                      .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                      .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    const VkExtent2D extent = m_Viewport.AsVulkanExtent(m_ParentExtent);

    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &pp;

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

template <Dimension D> void RenderView<D>::EndPostProcess(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndPostProcess");
    const auto table = GetDeviceTable();
    FrameBuffer &fb = m_FrameBuffers[m_ImageIndex];

    table->CmdEndRenderingKHR(cmd);
    fb.PostProcess.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
