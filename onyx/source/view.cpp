#include "pch.hpp"
#include "onyx/view.hpp"
#include "onyx/definitions.hpp"
#include "core.hpp"
#include "renderer.hpp"
#include "graph/descriptor.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
static Graph::Image createAttachment(const u32v2 &ext, const AttachmentType atype)
{
    const Graph::Format format = GetAttachmentFormat(atype);

    Graph::ImageSpecs specs{};
    specs.Formats = format;

    if (atype == Attachment_DepthStencil)
    {
        const Graph::Image img = Graph::Image_Create(ext, specs,
                                                     Graph::ImageFlag_DepthAttachment |
                                                         Graph::ImageFlag_StencilAttachment | Graph::ImageFlag_Sampled);
        Graph::Image_AddView(img);
        Graph::Image_AddView(img, {.Range = {.Aspect = Graph::ImageAspectFlag_Stencil}});
        return img;
    }

    if (atype == Attachment_Final)
    {
        const Graph::Format ppFormats[] = {format, SurfaceFormat};
        specs.Formats = ppFormats;

        const Graph::Image img =
            Graph::Image_Create(ext, specs, Graph::ImageFlag_ColorAttachment | Graph::ImageFlag_Sampled);
        Graph::Image_AddView(img, {.Format = format});
        Graph::Image_AddView(img, {.Format = SurfaceFormat});
        return img;
    }

    const Graph::Image img =
        Graph::Image_Create(ext, specs, Graph::ImageFlag_ColorAttachment | Graph::ImageFlag_Sampled);
    Graph::Image_AddView(img);
    return img;
}

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
RenderView<D>::RenderView(const u32v2 &extent, Camera<D> *camera, const RenderViewSpecs &specs)
    : m_Camera(camera), m_ParentExtent(extent), m_FrameBufferCount(specs.FrameBufferCount), m_Flags(specs.Flags)

{
    TKIT_ASSERT(specs.FrameBufferCount != 0, "[ONYX][VIEW] At least one framebuffer is required for a render view");
    m_ViewBit = allocateViewBit();

    m_BlendSet = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_Blend));
    m_PostProcessSet = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_PostProcess));
    m_CompositorSet = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_Compositor));

    if (Graph::IsValidationEnabled())
    {
        Graph::DescriptorSet_SetName(m_BlendSet, "onyx-blend-set-window");
        Graph::DescriptorSet_SetName(m_PostProcessSet, "onyx-post-process-set-window");
        Graph::DescriptorSet_SetName(m_CompositorSet, "onyx-compositor-set-window");
    }

    SetNormalizedViewport(Viewport{});
    SetNormalizedScissor(Scissor{});
    if (m_Framebuffers.IsEmpty())
    {
        createFrameBuffers();
        findAvailableFramebuffer();
    }
}
template <Dimension D> RenderView<D>::~RenderView()
{
    drainWork();

    Graph::DescriptorSet_Destroy(m_BlendSet);
    Graph::DescriptorSet_Destroy(m_PostProcessSet);
    Graph::DescriptorSet_Destroy(m_CompositorSet);

    destroyFramebuffers();

    // TODO(Isma): Remove this!! This is responsibility of onyx.hpp, should not be in destructor
    Renderer_RemoveTarget(m_ViewBit);
    deallocateViewBit(m_ViewBit);
}

template <Dimension D> void RenderView<D>::createFrameBuffers()
{
    TKIT_ASSERT(m_Framebuffers.IsEmpty(),
                "[ONYX][VIEW] Cannot create framebuffers on a non-empty frame buffer array with {} elements",
                m_Framebuffers.GetSize());
    const u32 maxFbs = m_FrameBufferCount;
    const u32v2 extent = GetRenderExtent();

    const bool transparency = m_Flags & RenderViewFlag_Transparency;
    const bool pprocess = m_Flags & RenderViewFlag_PostProcess;
    TKit::FixedArray<bool, Attachment_Count> mustCreate{transparency, transparency, pprocess, true, true, true};

    for (u32 i = 0; i < maxFbs; ++i)
    {
        Framebuffer &fb = m_Framebuffers.Append();
        for (u32 att = 0; att < Attachment_Count; ++att)
            fb.Attachments[att] = mustCreate[att] ? createAttachment(extent, AttachmentType(att)) : Graph::NullHandle;

        const Graph::Sampler near = Renderer_GetNearSampler();
        constexpr Graph::ImageLayout layout = Graph::ImageLayout_ShaderReadOnly;

        if (transparency)
        {
            const Graph::DescriptorImageInfo transparent = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[Attachment_Transparent]), layout, near);
            const Graph::DescriptorImageInfo revealage = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[Attachment_Revealage]), layout, near);

            Graph::DescriptorSet_BeginRecordWrite(m_BlendSet);
            Graph::DescriptorSet_RecordWrite(m_BlendSet, ONYX_BLEND_PASS_TRANSPARENT_ATTACHMENTS_BINDING, transparent,
                                             i);
            Graph::DescriptorSet_RecordWrite(m_BlendSet, ONYX_BLEND_PASS_REVEALAGE_ATTACHMENTS_BINDING, revealage, i);
            Graph::DescriptorSet_EndRecordWrite(m_BlendSet);
        }

        if (pprocess)
        {
            const Graph::DescriptorImageInfo color = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[Attachment_Intermediate]), layout, near);

            const Graph::DescriptorImageInfo outline = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[Attachment_Outline]), layout, near);
            // view 1 is the stencil-only view, created in createAttachment()
            const Graph::DescriptorImageInfo stencil = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[Attachment_DepthStencil], 1), layout);

            Graph::DescriptorSet_BeginRecordWrite(m_PostProcessSet);
            Graph::DescriptorSet_RecordWrite(m_PostProcessSet, ONYX_POST_PROCESS_PASS_COLOR_ATTACHMENTS_BINDING, color,
                                             i);
            Graph::DescriptorSet_RecordWrite(m_PostProcessSet, ONYX_POST_PROCESS_PASS_OUTLINE_ATTACHMENTS_BINDING,
                                             outline, i);
            Graph::DescriptorSet_RecordWrite(m_PostProcessSet, ONYX_POST_PROCESS_PASS_STENCIL_ATTACHMENTS_BINDING,
                                             stencil, i);
            Graph::DescriptorSet_EndRecordWrite(m_PostProcessSet);
        }

        // view 1 is the surface format view, created in createAttachment()
        const Graph::DescriptorImageInfo comp =
            Graph::DescriptorImageInfo::Create(Graph::Image_GetView(fb.Attachments[Attachment_Final], 1), layout, near);

        Graph::DescriptorSet_BeginRecordWrite(m_CompositorSet);
        Graph::DescriptorSet_RecordWrite(m_CompositorSet, ONYX_COMPOSITOR_PASS_COLOR_ATTACHMENTS_BINDING, comp, i);
        Graph::DescriptorSet_EndRecordWrite(m_CompositorSet);

        if (Graph::IsValidationEnabled())
        {
            TKit::FixedArray<TKit::StackString, Attachment_Count> names{};
            names[Attachment_Transparent] = TKit::StackString::Format("onyx-transparent-att-{}", i);
            names[Attachment_Revealage] = TKit::StackString::Format("onyx-revealage-att-{}", i);
            names[Attachment_Intermediate] = TKit::StackString::Format("onyx-intermediate-att-{}", i);
            names[Attachment_Outline] = TKit::StackString::Format("onyx-outline-att-{}", i);
            names[Attachment_DepthStencil] = TKit::StackString::Format("onyx-depth-stencil-att-{}", i);
            names[Attachment_Final] = TKit::StackString::Format("onyx-final-att-{}", i);
            for (u32 j = 0; j < Attachment_Count; ++j)
                if (fb.Attachments[j] != Graph::NullHandle)
                {
                    Graph::Image_SetName(fb.Attachments[j], names[j].CString());
                    Graph::Image_SetViewNames(fb.Attachments[j], names[j].CString());
                }
        }
    }
}

template <Dimension D> void RenderView<D>::findAvailableFramebuffer()
{
    u32 pending = TKIT_U32_MAX;
    for (u32 i = 0; i < m_Framebuffers.GetSize(); ++i)
        if (!m_Framebuffers[i].Tracker.InUse())
        {
            m_AttachmentIndex = i;
            return;
        }
        else if (pending == TKIT_U32_MAX ||
                 m_Framebuffers[pending].Tracker.InFlightValue > m_Framebuffers[i].Tracker.InFlightValue)
            pending = i;

    TKIT_ASSERT(pending != TKIT_U32_MAX, "[ONYX][VIEW] Failed to find an available frame buffer");

    TKIT_BEGIN_INFO_CLOCK();
    Graph::Queue_WaitForTracker(m_Framebuffers[pending].Tracker);
    TKIT_END_INFO_CLOCK(Milliseconds, "[ONYX][RENDER-VIEW] Waited {:.2f} ms for a new framebuffer");

    m_AttachmentIndex = pending;
}

template <Dimension D> void RenderView<D>::drainWork() const
{
    for (const Framebuffer &fb : m_Framebuffers)
        Graph::Queue_WaitForTracker(fb.Tracker);
}

template <Dimension D> void RenderView<D>::destroyFramebuffers()
{
    for (const Framebuffer &fb : m_Framebuffers)
        for (const Graph::Image img : fb.Attachments)
            if (img != Graph::NullHandle)
                Graph::Image_Destroy(img);
    m_Framebuffers.Clear();
}

template <Dimension D> f32v2 RenderView<D>::ScreenToViewport(const f32v2 &screenPos) const
{
    const f32v2 dif = screenPos - ViewportData.Position;
    if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
        return dif / ViewportData.Extent;
    return dif;
}
template <Dimension D> f32v2 RenderView<D>::ViewportToScreen(const f32v2 &viewportPos) const
{
    if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
        return viewportPos * ViewportData.Extent + ViewportData.Position;
    return viewportPos + ViewportData.Position;
}

template <Dimension D> f32v<D> RenderView<D>::ViewportToWorld(const f32v<D> &viewportPos) const
{
    const f32m<D> pv = Math::Inverse(m_ProjectionView);
    const f32v2 remapped = 2.f * ((m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
                                      ? f32v2{viewportPos}
                                      : (f32v2{viewportPos} / f32v2{ViewportData.Extent})) -
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

template <Dimension D> void RenderView<D>::MarkCurrentAttachmentsInUse(const Graph::Tracker &tracker)
{
    m_Framebuffers[m_AttachmentIndex].Tracker = tracker;
}

static Graph::ImageMemoryBarrierInfo toColorAttachment(const Graph::Image image)
{
    Graph::ImageMemoryBarrierInfo b{};
    b.Image = image;
    b.NewLayout = Graph::ImageLayout_ColorAttachment;
    b.SrcStages = Graph::PipelineStageFlag_FragmentShader;
    b.DstStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    b.DstAccess = Graph::AccessFlag_ColorAttachmentWrite;
    return b;
}
static Graph::ImageMemoryBarrierInfo colorToShaderRead(const Graph::Image image)
{
    Graph::ImageMemoryBarrierInfo b{};
    b.Image = image;
    b.NewLayout = Graph::ImageLayout_ShaderReadOnly;
    b.SrcStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    b.SrcAccess = Graph::AccessFlag_ColorAttachmentWrite;
    b.DstStages = Graph::PipelineStageFlag_FragmentShader;
    b.DstAccess = Graph::AccessFlag_ShaderRead;
    return b;
}
static Graph::ImageMemoryBarrierInfo depthToShaderRead(const Graph::Image image)
{
    Graph::ImageMemoryBarrierInfo b{};
    b.Image = image;
    b.NewLayout = Graph::ImageLayout_ShaderReadOnly;
    b.SrcStages = Graph::PipelineStageFlag_LateFragmentTests;
    b.SrcAccess = Graph::AccessFlag_DepthStencilAttachmentWrite;
    b.DstStages = Graph::PipelineStageFlag_FragmentShader;
    b.DstAccess = Graph::AccessFlag_ShaderRead;
    return b;
}

static Graph::AttachmentInfo colorAttachment(const Graph::Image image, const Graph::LoadOp load,
                                             const Graph::StoreOp store, const f32v4 &clear = f32v4{0.f})
{
    Graph::AttachmentInfo att{};
    att.View = Graph::Image_GetView(image);
    att.Layout = Graph::ImageLayout_ColorAttachment;
    att.LoadOp = load;
    att.StoreOp = store;
    att.ClearValue.Color = clear;
    return att;
}

static void beginRendering(const Graph::CommandBuffer cmd, const TKit::Span<const Graph::AttachmentInfo> colors,
                           const Graph::AttachmentInfo *depth, const u32v2 &ext, Scissor normScissor)
{
    Graph::RenderPassInfo info{};
    info.RenderArea = {{0, 0}, ext};
    info.ColorAttachments = colors;
    info.DepthAttachment = depth;
    info.StencilAttachment = depth;

    const f32v2 fext = f32v2{ext};
    normScissor.Position *= fext;
    normScissor.Extent *= fext;

    Graph::Command_SetViewport(cmd, f32v2{0.f}, fext);
    Graph::Command_SetScissor(cmd, i32v2{normScissor.Position}, u32v2{normScissor.Extent});
    Graph::Command_BeginRenderPass(cmd, info);
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

template <Dimension D> void RenderView<D>::BeginOpaquePass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginOpaquePass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const bool hasPostProcess = m_Flags & RenderViewFlag_PostProcess;
    const bool transparent = m_Flags & RenderViewFlag_Transparency;
    const bool hasOutlines = (m_Flags & RenderViewFlag_PostProcess) && (m_Flags & RenderViewFlag_Outlines);

    const Graph::Image colorImg =
        hasPostProcess ? fb.Attachments[Attachment_Intermediate] : fb.Attachments[Attachment_Final];
    const Graph::Image outlImg = fb.Attachments[Attachment_Outline];
    const Graph::Image depthImg = fb.Attachments[Attachment_DepthStencil];

    const bool keepDepth = transparent || hasOutlines;

    Graph::ImageMemoryBarrierInfo barriers[3] = {toColorAttachment(colorImg), toColorAttachment(outlImg), {}};
    barriers[2].Image = depthImg;
    barriers[2].NewLayout = Graph::ImageLayout_DepthStencilAttachment;
    barriers[2].SrcStages = hasPostProcess ? Graph::PipelineStageFlag_FragmentShader : 0;
    barriers[2].DstStages = Graph::PipelineStageFlag_EarlyFragmentTests;
    barriers[2].DstAccess = Graph::AccessFlag_DepthStencilAttachmentWrite;
    Graph::Command_ImageBarrier(cmd, barriers);

    const Color ccolor = ClearColor.ToLinear();
    TKit::FixedArray<Graph::AttachmentInfo, 2> atts{};
    atts[0] = colorAttachment(colorImg, Graph::LoadOp_Clear, Graph::StoreOp_Store,
                              f32v4{ccolor.rgba[0], ccolor.rgba[1], ccolor.rgba[2], ccolor.rgba[3]});
    atts[1] = colorAttachment(outlImg, hasOutlines ? Graph::LoadOp_Clear : Graph::LoadOp_DontCare,
                              hasOutlines ? Graph::StoreOp_Store : Graph::StoreOp_DontCare);

    Graph::AttachmentInfo depth{};
    depth.View = Graph::Image_GetView(depthImg);
    depth.Layout = Graph::ImageLayout_DepthStencilAttachment;
    depth.LoadOp = Graph::LoadOp_Clear;
    depth.StoreOp = keepDepth ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;

    const u32v2 ext = GetRenderExtent();
    beginRendering(cmd, atts, &depth, ext, GetNormalizedScissor());
}

template <Dimension D> void RenderView<D>::EndOpaquePass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndOpaquePass");

    Graph::Command_EndRenderPass(cmd);
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const bool transparent = m_Flags & RenderViewFlag_Transparency;
    if (!transparent)
    {
        const bool hasPostProcess = m_Flags & RenderViewFlag_PostProcess;
        const Graph::Image colorImg =
            hasPostProcess ? fb.Attachments[Attachment_Intermediate] : fb.Attachments[Attachment_Final];

        TKit::StaticArray<Graph::ImageMemoryBarrierInfo, 3> barriers{};
        barriers.Append(colorToShaderRead(colorImg));
        if (hasPostProcess)
        {
            barriers.Append(colorToShaderRead(fb.Attachments[Attachment_Outline]));
            barriers.Append(depthToShaderRead(fb.Attachments[Attachment_DepthStencil]));
        }
        Graph::Command_ImageBarrier(cmd, barriers);
    }
}

template <Dimension D> void RenderView<D>::BeginTransparentPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginTransparentPass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const Graph::Image trImg = fb.Attachments[Attachment_Transparent];
    const Graph::Image revImg = fb.Attachments[Attachment_Revealage];
    const Graph::Image outlImg = fb.Attachments[Attachment_Outline];
    const Graph::Image depthImg = fb.Attachments[Attachment_DepthStencil];

    const Graph::ImageMemoryBarrierInfo barriers[2] = {toColorAttachment(trImg), toColorAttachment(revImg)};
    Graph::Command_ImageBarrier(cmd, barriers);

    const bool hasOutlines = (m_Flags & RenderViewFlag_PostProcess) && (m_Flags & RenderViewFlag_Outlines);

    TKit::FixedArray<Graph::AttachmentInfo, 3> atts{};
    atts[0] = colorAttachment(trImg, Graph::LoadOp_Clear, Graph::StoreOp_Store);
    atts[1] = colorAttachment(outlImg, hasOutlines ? Graph::LoadOp_Load : Graph::LoadOp_DontCare,
                              hasOutlines ? Graph::StoreOp_Store : Graph::StoreOp_DontCare);
    atts[2] = colorAttachment(revImg, Graph::LoadOp_Clear, Graph::StoreOp_Store, f32v4{1.f, 0.f, 0.f, 0.f});

    Graph::AttachmentInfo depth{};
    depth.View = Graph::Image_GetView(depthImg);
    depth.Layout = Graph::ImageLayout_DepthStencilAttachment;
    depth.LoadOp = Graph::LoadOp_Load;
    depth.StoreOp = hasOutlines ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;

    const u32v2 ext = GetRenderExtent();
    beginRendering(cmd, atts, &depth, ext, GetNormalizedScissor());
}

template <Dimension D> void RenderView<D>::EndTransparentPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndTransparentPass");

    Graph::Command_EndRenderPass(cmd);
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const Graph::ImageMemoryBarrierInfo barriers[2] = {colorToShaderRead(fb.Attachments[Attachment_Transparent]),
                                                       colorToShaderRead(fb.Attachments[Attachment_Revealage])};
    Graph::Command_ImageBarrier(cmd, barriers);
}

template <Dimension D> void RenderView<D>::BeginBlendPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginBlendPass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const bool hasPostProcess = m_Flags & RenderViewFlag_PostProcess;
    const Graph::Image colorImg =
        hasPostProcess ? fb.Attachments[Attachment_Intermediate] : fb.Attachments[Attachment_Final];

    const Graph::AttachmentInfo color = colorAttachment(colorImg, Graph::LoadOp_Load, Graph::StoreOp_Store);
    const u32v2 ext = GetRenderExtent();

    beginRendering(cmd, color, nullptr, ext, GetNormalizedScissor());
}
template <Dimension D> void RenderView<D>::EndBlendPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndBlendPass");

    Graph::Command_EndRenderPass(cmd);
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const bool hasPostProcess = m_Flags & RenderViewFlag_PostProcess;
    const Graph::Image colorImg =
        hasPostProcess ? fb.Attachments[Attachment_Intermediate] : fb.Attachments[Attachment_Final];

    TKit::StaticArray<Graph::ImageMemoryBarrierInfo, 3> barriers{};
    barriers.Append(colorToShaderRead(colorImg));
    if (hasPostProcess)
    {
        barriers.Append(colorToShaderRead(fb.Attachments[Attachment_Outline]));
        barriers.Append(depthToShaderRead(fb.Attachments[Attachment_DepthStencil]));
    }
    Graph::Command_ImageBarrier(cmd, barriers);
}

template <Dimension D> void RenderView<D>::BeginPostProcess(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::BeginPostProcess");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];
    const Graph::Image finalImg = fb.Attachments[Attachment_Final];

    const Graph::ImageMemoryBarrierInfo barrier = toColorAttachment(finalImg);
    Graph::Command_ImageBarrier(cmd, barrier);

    const Graph::AttachmentInfo color = colorAttachment(finalImg, Graph::LoadOp_DontCare, Graph::StoreOp_Store);

    const u32v2 ext = GetRenderExtent();
    beginRendering(cmd, color, nullptr, ext, GetNormalizedScissor());
}

template <Dimension D> void RenderView<D>::EndPostProcess(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndPostProcess");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    Graph::Command_EndRenderPass(cmd);
    const Graph::ImageMemoryBarrierInfo barrier = colorToShaderRead(fb.Attachments[Attachment_Final]);
    Graph::Command_ImageBarrier(cmd, barrier);
}

template <Dimension D> bool RenderView<D>::IsWithinViewport(const f32v2 &screenPos) const
{
    const f32v2 viewportPos = ScreenToViewport(screenPos);
    if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
        return viewportPos[0] > 0.f && viewportPos[0] < 1.f && viewportPos[1] > 0.f && viewportPos[1] < 1.f;

    return viewportPos[0] > 0.f && viewportPos[0] < ViewportData.Extent[0] && viewportPos[1] > 0.f &&
           viewportPos[1] < ViewportData.Extent[1];
}

template <Dimension D> void RenderView<D>::SetNormalizedViewport(const Viewport &vp)
{
    const f32v2 ip = 1.f / f32v2{m_ParentExtent};
    ViewportData.Position = Math::Clamp(vp.Position, 0.f, 1.f);
    ViewportData.Extent = Math::Clamp(vp.Extent, ip, f32v2{1.f});

    if (!(m_Flags & RenderViewFlag_NormalizedViewportCoordinates))
        ViewportData = asAbsoluteViewport();

    CacheMatrices();
    if (!(m_Flags & RenderViewFlag_DynamicViewport))
    {
        drainWork();
        recreateFramebuffers();
    }
}
template <Dimension D> void RenderView<D>::SetAbsoluteViewport(const Viewport &vp)
{
    ViewportData.Position = Math::Clamp(vp.Position, f32v2{0.f}, f32v2{m_ParentExtent});
    ViewportData.Extent = Math::Clamp(vp.Extent, f32v2{1.f}, f32v2{m_ParentExtent});

    if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
        ViewportData = asNormalizedViewport();

    CacheMatrices();
    if (!(m_Flags & RenderViewFlag_DynamicViewport))
    {
        drainWork();
        recreateFramebuffers();
    }
}

template <Dimension D> void RenderView<D>::SetNormalizedScissor(const Scissor &sc)
{
    const u32v2 ext = GetExtent();
    const f32v2 ip = 1.f / f32v2{ext};
    ScissorData.Position = Math::Clamp(sc.Position, 0.f, 1.f);
    ScissorData.Extent = Math::Clamp(sc.Extent, ip, f32v2{1.f});

    if (!(m_Flags & RenderViewFlag_NormalizedScissorCoordinates))
        ScissorData = asAbsoluteScissor();
}
template <Dimension D> void RenderView<D>::SetAbsoluteScissor(const Scissor &sc)
{
    const u32v2 ext = GetExtent();
    ScissorData.Position = Math::Clamp(sc.Position, f32v2{0.f}, f32v2{ext});
    ScissorData.Extent = Math::Clamp(sc.Extent, f32v2{1.f}, f32v2{ext});

    if (m_Flags & RenderViewFlag_NormalizedScissorCoordinates)
        ScissorData = asNormalizedScissor();
}

template <Dimension D> void RenderView<D>::SetFlags(const RenderViewFlags flags)
{
    CacheMatrices();
    const RenderViewFlags f = m_Flags;

    const RenderViewFlags nvc = RenderViewFlag_NormalizedViewportCoordinates;
    const RenderViewFlags nsc = RenderViewFlag_NormalizedScissorCoordinates;

    if ((f & nsc) && !(flags & nsc))
        ScissorData = asAbsoluteScissor();
    else if (!(f & nsc) && (flags & nsc))
        ScissorData = asNormalizedScissor();

    if ((f & nvc) && !(flags & nvc))
        ViewportData = asAbsoluteViewport();
    else if (!(f & nvc) && (flags & nvc))
        ViewportData = asNormalizedViewport();
    m_Flags = flags;

    const RenderViewFlags fbFlags =
        RenderViewFlag_DynamicViewport | RenderViewFlag_Transparency | RenderViewFlag_PostProcess;
    if ((flags & fbFlags) != (f & fbFlags))
    {
        drainWork();
        recreateFramebuffers();
    }
}

template <Dimension D> f32m<D> RenderView<D>::ComputeView() const
{
    if (m_Flags & RenderViewFlag_ManualProjectionView)
        return m_View;

    // im not sure if this is the right way to go!! before, i was not shifting the origin so it matched the viewport
    // EDIT: doesnt really matter, lets keep the origin at the center
    // Transform<D> t = m_Camera->View;
    // if (m_Camera->Mode == CameraMode_Viewport)
    // {
    //     const Viewport vp = GetAbsoluteViewport();
    //     if constexpr (D == D2)
    //         t.Translation += 0.5f * vp.Extent;
    //     else
    //         t.Translation += 0.5f * f32v3{vp.Extent, 0.f};
    // }
    //
    // f32m<D> view = t.ComputeInverseTransform();
    //
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
