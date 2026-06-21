#include "pch.hpp"
#include "onyx/view.hpp"
#include "onyx/definitions.hpp"
#include "renderer.hpp"
#include "conversion.hpp"
#include "platform.hpp"
#include "descriptors.hpp"
#include "core.hpp"
#include "attachment.hpp"
#include "vkit/resource/device_image.hpp"
#include "vkit/state/descriptor_set.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
static VKit::DeviceImage createAttachment(const VkExtent2D &ext, const AttachmentType atype)
{
    const auto &device = GetDevice();
    const VmaAllocator alloc = GetVulkanAllocator();
    const VkFormat format = GetAttachmentFormat(atype);
    if (atype == Attachment_DepthStencil)
    {
        VkImageSubresourceRange stencilRange{};
        stencilRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        stencilRange.levelCount = 1;
        stencilRange.layerCount = 1;
        return ONYX_CHECK_VKIT_RESULT(VKit::DeviceImage::Builder(device, alloc, ext, format,
                                                                 VKit::DeviceImageFlag_DepthAttachment |
                                                                     VKit::DeviceImageFlag_StencilAttachment |
                                                                     VKit::DeviceImageFlag_Sampled)
                                          .AddImageView()
                                          .AddImageView(stencilRange)
                                          .Build());
    }
    else if (atype == Attachment_Final)
    {
        const VkFormat sformat = Platform::GetSurfaceFormat().format;
        const TKit::FixedArray<VkFormat, 2> ppFormats{format, sformat};
        return ONYX_CHECK_VKIT_RESULT(
            VKit::DeviceImage::Builder(device, alloc, ext, ppFormats,
                                       VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Sampled)
                .AddImageView(format)
                .AddImageView(sformat)
                .Build());
    }

    return ONYX_CHECK_VKIT_RESULT(
        VKit::DeviceImage::Builder(device, alloc, ext, format,
                                   VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Sampled)
            .AddImageView()
            .Build());
}

struct Framebuffer
{
    Execution::Tracker Tracker{};
    TKit::FixedArray<VKit::DeviceImage, Attachment_Count> Attachments{};
};

// NOTE(Isma): Consider having 2D and 3D view sets
static ViewMask s_ViewCache = TKit::Limits<ViewMask>::Max();
static ViewMask allocateViewBit()
{
    TKIT_ASSERT(s_ViewCache != 0, "[ONYX][VIEW] Maximum amount of views exceeded. There is a hard limit of {} views",
                8 * sizeof(ViewMask));

    const u32 index = u32(std::countr_zero(s_ViewCache));
    const ViewMask viewBit = 1U << index;
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

template <Dimension D>
RenderView<D>::RenderView(const u32v2 &extent, Camera<D> *camera, const RenderViewFlags flags)
    : m_Camera(camera), m_ParentExtent(extent), m_Flags(flags)

{
    m_ViewBit = allocateViewBit();

    m_BlendSet = ONYX_CHECK_VKIT_RESULT(
        Descriptors::GetDescriptorPool().Allocate(Descriptors::GetDescriptorLayout(StandalonePass_Blend)));
    m_PostProcessSet = ONYX_CHECK_VKIT_RESULT(
        Descriptors::GetDescriptorPool().Allocate(Descriptors::GetDescriptorLayout(StandalonePass_PostProcess)));
    m_CompositorSet = ONYX_CHECK_VKIT_RESULT(
        Descriptors::GetDescriptorPool().Allocate(Descriptors::GetDescriptorLayout(StandalonePass_Compositor)));
    if (IsDebugUtilsEnabled())
    {
        const auto &device = GetDevice();
        ONYX_CHECK_VKIT_RESULT(
            device.SetObjectName(m_PostProcessSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "onyx-post-process-set-window"));
        ONYX_CHECK_VKIT_RESULT(
            device.SetObjectName(m_CompositorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "onyx-compositor-set-window"));
    }

    SetNormalizedViewport(Viewport{});
    SetNormalizedScissor(Scissor{});
    findAvailableFramebuffer();
}
template <Dimension D> RenderView<D>::~RenderView()
{
    drainWork();
    destroyFramebuffers();

    // TODO(Isma): Remove this!! This is responsibility of onyx.hpp, should not be in destructor
    Renderer::RemoveTarget(m_ViewBit);
    deallocateViewBit(m_ViewBit);
}

template <Dimension D> void RenderView<D>::findAvailableFramebuffer()
{
    for (u32 i = 0; i < m_Framebuffers.GetSize(); ++i)
        if (!m_Framebuffers[i]->Tracker.InUse())
        {
            m_AttachmentIndex = i;
            return;
        }

    m_AttachmentIndex = m_Framebuffers.GetSize();
    TKIT_LOG_DEBUG("[ONYX][VIEW] Failed to find an available frame buffer. Increasing frame buffer pool to {} for "
                   "this particular view",
                   m_AttachmentIndex + 1);

    TKit::TierAllocator *tier = GetTier();
    const VkExtent2D extent = AsVulkanExtent(GetRenderExtent());

    const bool transparency = m_Flags & RenderViewFlag_Transparency;
    const bool pprocess = m_Flags & RenderViewFlag_PostProcess;
    TKit::FixedArray<bool, Attachment_Count> mustCreate{transparency, transparency, pprocess, true, true, true};

    Framebuffer *fb = m_Framebuffers.Append(tier->Create<Framebuffer>());
    for (u32 att = 0; att < Attachment_Count; ++att)
        fb->Attachments[att] = mustCreate[att] ? createAttachment(extent, AttachmentType(att)) : VKit::DeviceImage{};

    VKit::DescriptorSet::Writer blend{GetDevice(), &Descriptors::GetDescriptorLayout(StandalonePass_Blend)};
    VKit::DescriptorSet::Writer pp{GetDevice(), &Descriptors::GetDescriptorLayout(StandalonePass_PostProcess)};
    VKit::DescriptorSet::Writer compositor{GetDevice(), &Descriptors::GetDescriptorLayout(StandalonePass_Compositor)};

    TKit::StaticArray<VkDescriptorImageInfo, 6> infos{};
    if (transparency)
    {
        VkDescriptorImageInfo &transparent = infos.Append();
        transparent.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transparent.imageView = fb->Attachments[Attachment_Transparent].GetView();
        transparent.sampler = Renderer::GetNearSampler();

        VkDescriptorImageInfo &revealage = infos.Append();
        revealage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        revealage.imageView = fb->Attachments[Attachment_Revealage].GetView();
        revealage.sampler = Renderer::GetNearSampler();

        blend.WriteImage(ONYX_BLEND_TRANSPARENT_ATTACHMENTS_BINDING, transparent, m_AttachmentIndex);
        blend.WriteImage(ONYX_BLEND_REVEALAGE_ATTACHMENTS_BINDING, revealage, m_AttachmentIndex);
    }

    if (pprocess)
    {
        VkDescriptorImageInfo &color = infos.Append();
        color.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        color.imageView = fb->Attachments[Attachment_Intermediate].GetView();
        color.sampler = Renderer::GetNearSampler();

        pp.WriteImage(ONYX_POST_PROCESS_COLOR_ATTACHMENTS_BINDING, color, m_AttachmentIndex);
        VkDescriptorImageInfo &outline = infos.Append();
        outline.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        outline.imageView = fb->Attachments[Attachment_Outline].GetView();
        outline.sampler = Renderer::GetNearSampler();

        VkDescriptorImageInfo &stencil = infos.Append();
        stencil.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        stencil.imageView = fb->Attachments[Attachment_DepthStencil].GetViews().GetBack();
        stencil.sampler = VK_NULL_HANDLE;

        pp.WriteImage(ONYX_POST_PROCESS_OUTLINE_ATTACHMENTS_BINDING, outline, m_AttachmentIndex);
        pp.WriteImage(ONYX_POST_PROCESS_STENCIL_ATTACHMENTS_BINDING, stencil, m_AttachmentIndex);
    }

    VkDescriptorImageInfo &comp = infos.Append();
    comp.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    comp.imageView = fb->Attachments[Attachment_Final].GetView(1);
    comp.sampler = Renderer::GetNearSampler();

    compositor.WriteImage(ONYX_COMPOSITOR_COLOR_ATTACHMENTS_BINDING, comp, m_AttachmentIndex);
    if (transparency)
        blend.Overwrite(m_BlendSet);
    if (pprocess)
        pp.Overwrite(m_PostProcessSet);
    compositor.Overwrite(m_CompositorSet);
    if (IsDebugUtilsEnabled())
    {
        TKit::FixedArray<TKit::StackString, Attachment_Count> names{};
        names[Attachment_Transparent] = TKit::StackString::Format("onyx-transparent-att-{}", m_AttachmentIndex);
        names[Attachment_Revealage] = TKit::StackString::Format("onyx-revealage-att-{}", m_AttachmentIndex);
        names[Attachment_Intermediate] = TKit::StackString::Format("onyx-intermediate-att-{}", m_AttachmentIndex);
        names[Attachment_Outline] = TKit::StackString::Format("onyx-outline-att-{}", m_AttachmentIndex);
        names[Attachment_DepthStencil] = TKit::StackString::Format("onyx-depth-stencil-att-{}", m_AttachmentIndex);
        names[Attachment_Final] = TKit::StackString::Format("onyx-final-att-{}", m_AttachmentIndex);
        for (u32 j = 0; j < Attachment_Count; ++j)
            if (fb->Attachments[j])
            {
                ONYX_CHECK_VKIT_RESULT(fb->Attachments[j].SetName(names[j].CString()));
                ONYX_CHECK_VKIT_RESULT(fb->Attachments[j].SetViewNames(names[j].CString()));
            }
    }
}

template <Dimension D> void RenderView<D>::drainWork()
{
    TKit::StackArray<VkSemaphore> semaphores{};
    semaphores.Reserve(m_Framebuffers.GetSize());
    TKit::StackArray<u64> values{};
    values.Reserve(m_Framebuffers.GetSize());
    for (const Framebuffer *fb : m_Framebuffers)
        if (fb->Tracker.InFlight())
        {
            semaphores.Append(fb->Tracker.Queue->GetTimelineSempahore());
            values.Append(fb->Tracker.InFlightValue);
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
        ONYX_CHECK_VKIT_RESULT(table->WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX));
    }
}

template <Dimension D> void RenderView<D>::destroyFramebuffers()
{
    TKit::TierAllocator *tier = GetTier();
    for (Framebuffer *fb : m_Framebuffers)
    {
        for (VKit::DeviceImage &att : fb->Attachments)
            att.Destroy();
        tier->Destroy(fb);
    }
    m_Framebuffers.Clear();
}

template <Dimension D> f32v2 RenderView<D>::ScreenToViewport(const f32v2 &screenPos) const
{
    const f32v2 dif = screenPos - m_Viewport.Position;
    if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
        return dif / m_Viewport.Extent;
    return dif;
}
template <Dimension D> f32v2 RenderView<D>::ViewportToScreen(const f32v2 &viewportPos) const
{
    if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
        return viewportPos * m_Viewport.Extent + m_Viewport.Position;
    return viewportPos + m_Viewport.Position;
}

template <Dimension D> f32v<D> RenderView<D>::ViewportToWorld(const f32v<D> &viewportPos) const
{
    const f32m<D> pv = Math::Inverse(m_ProjectionView);
    const f32v2 remapped = 2.f * ((m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
                                      ? f32v2{viewportPos}
                                      : (f32v2{viewportPos} / f32v2{m_Viewport.Extent})) -
                           1.f;

    if constexpr (D == D2)
        return pv * f32v3{remapped, 1.f};
    else
    {
        const f32v3 rm = f32v3{remapped, viewportPos[2]};
        const f32v4 clip = pv * f32v4{rm, 1.f};
        return f32v3{clip} / clip[3];
    }
}

template <Dimension D> static VkRect2D getScissor(const RenderView<D> *view, const u32v2 &ext)
{
    const f32v2 fext = f32v2{ext};
    Scissor sc = view->GetNormalizedScissor();
    sc.Position *= fext;
    sc.Extent *= fext;
    return AsVulkanScissor(sc);
}

template <Dimension D> f32v2 RenderView<D>::WorldToViewport(const f32v<D> &worldPos) const
{
    if constexpr (D == D2)
    {
        const f32v2 ndc = f32v2{m_ProjectionView * f32v3{worldPos, 1.f}};
        const f32v2 vpos = 0.5f * (ndc + 1.f);
        return (m_Flags & RenderViewFlag_NormalizedViewportCoordinates) ? vpos : (vpos * f32v2{m_ParentExtent});
    }
    else
    {
        const f32v4 clip = m_ProjectionView * f32v4{worldPos, 1.f};
        const f32v2 ndc = f32v2{clip} / clip[3];
        const f32v2 vpos = 0.5f * (ndc + 1.f);
        return (m_Flags & RenderViewFlag_NormalizedViewportCoordinates) ? vpos : (vpos * f32v2{m_ParentExtent});
    }
}

template <Dimension D> void RenderView<D>::MarkCurrentAttachmentsInUse(const Execution::Tracker &tracker)
{
    m_Framebuffers[m_AttachmentIndex]->Tracker = tracker;
}

static void beginRendering(const VkCommandBuffer cmd, const TKit::Span<const VkRenderingAttachmentInfoKHR> &colors,
                           const VkRenderingAttachmentInfoKHR &depth, const u32v2 &ext, Scissor normScissor)
{
    const VkRenderingAttachmentInfoKHR stencil = depth;
    const VkExtent2D vkExtent = AsVulkanExtent(ext);

    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, vkExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = colors.GetSize();
    renderInfo.pColorAttachments = colors.GetData();
    if (depth.imageView)
    {
        renderInfo.pDepthAttachment = &depth;
        renderInfo.pStencilAttachment = &stencil;
    }

    VkViewport viewport;
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = f32(vkExtent.width);
    viewport.height = f32(vkExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    const f32v2 fext = f32v2{ext};
    normScissor.Position *= fext;
    normScissor.Extent *= fext;
    const VkRect2D scissor = AsVulkanScissor(normScissor);

    const auto table = GetDeviceTable();
    table->CmdSetViewport(cmd, 0, 1, &viewport);
    table->CmdSetScissor(cmd, 0, 1, &scissor);
    table->CmdBeginRenderingKHR(cmd, &renderInfo);
}

// a bit of a pass overview
//
// shadow passes are done directly in the renderer so they are not contemplated here
//
// first pass is the opaque pass. renders opaque geometry to either the intermediate attachment or the final attachment
// (latter if post processing is disabled. we skip one pass this way). this pass takes srgb textures -> linearizes ->
// does math with linear colors -> stores as srgb
//
// if transparency is enabled, transparent pass is executed. same deal as the opaque pass in terms of colorspace. the
// blend pass merges both opaque and transparent directly into the opaque's target (either intermediate attchm or final)
//
// if post processing is enabled, the pass is executed with intermediate as input and final as output
//
// the final image format is a bit special because it supports both: unorm and srgb. throughout all this pipeline, the
// format (view) used is srgb however, for the final window composite pass that merges all views, the format that is
// used for the final image is unorm. this is because the surface format is unorm (this is required for good imgui
// support)

template <Dimension D> void RenderView<D>::BeginOpaquePass(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginOpaquePass");

    Framebuffer *fb = m_Framebuffers[m_AttachmentIndex];
    TKit::FixedArray<VkRenderingAttachmentInfoKHR, 2> atts{};

    const bool hasPostProcess = m_Flags & RenderViewFlag_PostProcess;

    VKit::DeviceImage &colorImg =
        hasPostProcess ? fb->Attachments[Attachment_Intermediate] : fb->Attachments[Attachment_Final];
    VKit::DeviceImage &outlImg = fb->Attachments[Attachment_Outline];
    VKit::DeviceImage &depthImg = fb->Attachments[Attachment_DepthStencil];

    VkRenderingAttachmentInfoKHR &color = atts[0];
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color.imageView = colorImg.GetView();
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    const Color ccolor = ClearColor.ToLinear();
    color.clearValue.color = {{ccolor.rgba[0], ccolor.rgba[1], ccolor.rgba[2], ccolor.rgba[3]}};

    colorImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    const bool transparent = m_Flags & RenderViewFlag_Transparency;
    const bool hasOutlines = (m_Flags & RenderViewFlag_PostProcess) && (m_Flags & RenderViewFlag_Outlines);

    VkRenderingAttachmentInfoKHR &outline = atts[1];
    outline.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    outline.imageView = outlImg.GetView();
    outline.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    outline.loadOp = hasOutlines ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    outline.storeOp = hasOutlines ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    outline.clearValue.color = {{0.f, 0.f, 0.f, 0.f}};

    outlImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                               .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                               .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    VkRenderingAttachmentInfoKHR depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depth.imageView = depthImg.GetView();
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = (transparent || hasOutlines) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil = {1.f, 0};

    depthImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                               {.DstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR,
                                .SrcStage = (transparent || hasOutlines) ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR
                                                                         : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
                                .DstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR});

    const u32v2 ext = GetRenderExtent();
    beginRendering(cmd, atts, depth, ext, GetNormalizedScissor());
}

template <Dimension D> void RenderView<D>::EndOpaquePass(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndOpaquePass");
    const auto table = GetDeviceTable();

    table->CmdEndRenderingKHR(cmd);
    Framebuffer *fb = m_Framebuffers[m_AttachmentIndex];

    const bool transparent = m_Flags & RenderViewFlag_Transparency;
    const bool hasPostProcess = m_Flags & RenderViewFlag_PostProcess;

    if (hasPostProcess && !transparent)
    {
        VKit::DeviceImage &colorImg =
            hasPostProcess ? fb->Attachments[Attachment_Intermediate] : fb->Attachments[Attachment_Final];
        VKit::DeviceImage &outlImg = fb->Attachments[Attachment_Outline];
        VKit::DeviceImage &depthImg = fb->Attachments[Attachment_DepthStencil];

        colorImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   {
                                       .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                       .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,

                                       .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                       .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                   });
        outlImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  {
                                      .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                      .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,

                                      .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                      .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                  });
        depthImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   {
                                       .SrcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR,
                                       .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,

                                       .SrcStage = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
                                       .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                   });
    }
}

template <Dimension D> void RenderView<D>::BeginTransparentPass(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginTransparentPass");

    Framebuffer *fb = m_Framebuffers[m_AttachmentIndex];
    TKit::FixedArray<VkRenderingAttachmentInfoKHR, 3> atts{};

    VKit::DeviceImage &trImg = fb->Attachments[Attachment_Transparent];
    VKit::DeviceImage &revImg = fb->Attachments[Attachment_Revealage];
    VKit::DeviceImage &outlImg = fb->Attachments[Attachment_Outline];
    VKit::DeviceImage &depthImg = fb->Attachments[Attachment_DepthStencil];

    VkRenderingAttachmentInfoKHR &color = atts[0];
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color.imageView = trImg.GetView();
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = {{0.f, 0.f, 0.f, 0.f}};

    trImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                             .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                             .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    const bool hasOutlines = (m_Flags & RenderViewFlag_PostProcess) && (m_Flags & RenderViewFlag_Outlines);

    VkRenderingAttachmentInfoKHR &outline = atts[1];
    outline.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    outline.imageView = outlImg.GetView();
    outline.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    outline.loadOp = hasOutlines ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    outline.storeOp = hasOutlines ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    outline.clearValue.color = {{0.f, 0.f, 0.f, 0.f}};

    VkRenderingAttachmentInfoKHR &revealage = atts[2];
    revealage.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    revealage.imageView = revImg.GetView();
    revealage.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    revealage.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    revealage.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    revealage.clearValue.color = {{1.f, 0.f, 0.f, 0.f}};

    revImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                              .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                              .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    VkRenderingAttachmentInfoKHR depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depth.imageView = depthImg.GetView();
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp = hasOutlines ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil = {1.f, 0};

    const u32v2 ext = GetRenderExtent();
    beginRendering(cmd, atts, depth, ext, GetNormalizedScissor());
}

template <Dimension D> void RenderView<D>::EndTransparentPass(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndTransparentPass");
    const auto table = GetDeviceTable();

    table->CmdEndRenderingKHR(cmd);
    Framebuffer *fb = m_Framebuffers[m_AttachmentIndex];

    VKit::DeviceImage &trImg = fb->Attachments[Attachment_Transparent];
    VKit::DeviceImage &revImg = fb->Attachments[Attachment_Revealage];
    trImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            {
                                .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                            });

    revImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             {
                                 .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                 .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                 .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                 .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                             });
}

template <Dimension D> void RenderView<D>::BeginBlendPass(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginBlendPass");

    Framebuffer *fb = m_Framebuffers[m_AttachmentIndex];

    const bool hasPostProcess = m_Flags & RenderViewFlag_PostProcess;

    VKit::DeviceImage &colorImg =
        hasPostProcess ? fb->Attachments[Attachment_Intermediate] : fb->Attachments[Attachment_Final];

    VkRenderingAttachmentInfoKHR color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color.imageView = colorImg.GetView();
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    const u32v2 ext = GetRenderExtent();

    beginRendering(cmd, color, {}, ext, GetNormalizedScissor());
}
template <Dimension D> void RenderView<D>::EndBlendPass(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndBlendPass");
    const auto table = GetDeviceTable();

    table->CmdEndRenderingKHR(cmd);
    Framebuffer *fb = m_Framebuffers[m_AttachmentIndex];

    const bool hasPostProcess = m_Flags & RenderViewFlag_PostProcess;

    VKit::DeviceImage &colorImg =
        hasPostProcess ? fb->Attachments[Attachment_Intermediate] : fb->Attachments[Attachment_Final];

    colorImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               {
                                   .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                   .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                   .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                   .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                               });

    if (hasPostProcess)
    {
        VKit::DeviceImage &outlImg = fb->Attachments[Attachment_Outline];
        VKit::DeviceImage &depthImg = fb->Attachments[Attachment_DepthStencil];
        outlImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  {
                                      .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                      .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                      .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                      .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                  });
        depthImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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

    Framebuffer *fb = m_Framebuffers[m_AttachmentIndex];

    VKit::DeviceImage &finalImg = fb->Attachments[Attachment_Final];

    VkRenderingAttachmentInfoKHR color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color.imageView = finalImg.GetView();
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    finalImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    const u32v2 ext = GetRenderExtent();
    beginRendering(cmd, color, {}, ext, GetNormalizedScissor());
}

template <Dimension D> void RenderView<D>::EndPostProcess(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndPostProcess");
    const auto table = GetDeviceTable();
    Framebuffer *fb = m_Framebuffers[m_AttachmentIndex];

    VKit::DeviceImage &finalImg = fb->Attachments[Attachment_Final];

    table->CmdEndRenderingKHR(cmd);
    finalImg.TransitionLayout2(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               {
                                   .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                   .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,
                                   .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                   .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                               });
}

template <Dimension D> f32m<D> RenderView<D>::ComputeView() const
{
    if (m_Flags & RenderViewFlag_ManualProjectionView)
        return m_View;

    f32m<D> view = m_Camera->View.ComputeInverseTransform();
    applyCoordinateSystemExtrinsic<D>(view);
    return view;
}
template <Dimension D> f32m<D> RenderView<D>::ComputeProjection() const
{
    if (m_Flags & RenderViewFlag_ManualProjectionView)
        return m_Projection;

    const Viewport viewport = GetAbsoluteViewport();
    const f32 aspect = viewport.Extent[0] / viewport.Extent[1];

    if constexpr (D == D2)
    {
        TKIT_ASSERT(m_Camera->Mode != CameraMode_Perspective, "[ONYX][VIEW] 2D does not support perspective cameras");
        return m_Camera->Mode == CameraMode_Orthographic
                   ? Transform<D2>::Orthographic(m_Camera->OrthoParameters.Size, aspect)
                   : Transform<D2>::Orthographic(viewport.Extent[1], aspect);
    }
    else
    {
        const OrthographicParameters<D3> &oparams = m_Camera->OrthoParameters;
        const PerspectiveParameters &pparams = m_Camera->PerspParameters;

        if (m_Camera->Mode == CameraMode_Perspective)
            return Transform<D3>::Perspective(pparams.FieldOfView, pparams.Near, pparams.Far, aspect);
        if (m_Camera->Mode == CameraMode_Orthographic)
            return Transform<D3>::Orthographic(oparams.Size, aspect, oparams.Near, oparams.Far);
        return Transform<D3>::Orthographic(viewport.Extent[1], aspect, oparams.Near, oparams.Far);
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
