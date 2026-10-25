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
static Graph::Image createColorAttachment(const u32v2 &ext, const TKit::Span<const Graph::Format> formats)
{
    Graph::ImageSpecs specs{};
    specs.Formats = formats;

    const Graph::Image img =
        Graph::Image_Create(ext, specs, Graph::ImageFlag_ColorAttachment | Graph::ImageFlag_Sampled);

    for (const Graph::Format fmt : formats)
        Graph::Image_AddView(img, {.Format = fmt});
    return img;
}

static Graph::Image createDepthStencilAttachment(const u32v2 &ext)
{
    const Graph::Format fmt = GetAttachmentFormat(AttachmentKind_DepthStencil);
    const Graph::Image img = Graph::Image_Create(ext, {.Formats = fmt},
                                                 Graph::ImageFlag_DepthAttachment | Graph::ImageFlag_StencilAttachment |
                                                     Graph::ImageFlag_Sampled);
    Graph::Image_AddView(img);
    Graph::Image_AddView(img, {.Range = {.Aspect = Graph::ImageAspectFlag_Stencil}});
    return img;
}

static Graph::Image createAttachment(const u32v2 &ext, const AttachmentRole role)
{
    if (role == AttachmentRole_DepthStencil)
        return createDepthStencilAttachment(ext);
    if (role == AttachmentRole_Accumulation)
    {
        const Graph::Format fmt = GetAttachmentFormat(AttachmentKind_Accumulation);
        return createColorAttachment(ext, fmt);
    }
    if (role == AttachmentRole_Revealage)
    {
        const Graph::Format fmt = GetAttachmentFormat(AttachmentKind_Revealage);
        return createColorAttachment(ext, fmt);
    }

    TKit::StaticArray<Graph::Format, 2> formats{};
    formats.Append(GetAttachmentFormat(AttachmentKind_SrgbColor));
    if (role == AttachmentRole_OutlineColor)
        return createColorAttachment(ext, formats);

    formats.Append(GetAttachmentFormat(AttachmentKind_LinearColor));
    return createColorAttachment(ext, formats);
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
RenderView<D>::RenderView(const u32v2 &size, const f32v2 &contentScale, Camera<D> *camera, const RenderViewSpecs &specs)
    : AntiAliasingMode(specs.AntiAliasingMode), m_Camera(camera), m_ViewportSizing(specs.ViewportSizing),
      m_ScissorSizing(specs.ScissorSizing), m_ImageSizing(specs.ImageSizing), m_ParentSize(size),
      m_ContentScale(contentScale), m_FrameBufferCount(specs.FrameBufferCount), m_Flags(specs.Flags)

{
    TKIT_ASSERT(specs.FrameBufferCount != 0, "[ONYX][VIEW] At least one framebuffer is required for a render view");
    m_ViewBit = allocateViewBit();

    m_BlendSet = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_Blend));
    m_OutlineSet = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_Outline));
    m_FXAASet = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_FXAA));
    m_CompositorSet = Graph::DescriptorSet_Create(GetDescriptorLayout(StandalonePass_Compositor));

    if (Graph::IsValidationEnabled())
    {
        Graph::DescriptorSet_SetName(m_BlendSet, "onyx-blend-set-window");
        Graph::DescriptorSet_SetName(m_OutlineSet, "onyx-outline-set-window");
        Graph::DescriptorSet_SetName(m_CompositorSet, "onyx-compositor-set-window");
    }

    m_Viewport.Size = size;
    m_Scissor.Size = size;
    m_ImageSize = size;

    createFrameBuffers();
    findAvailableFramebuffer();
}
template <Dimension D> RenderView<D>::~RenderView()
{
    drainWork();

    Graph::DescriptorSet_Destroy(m_BlendSet);
    Graph::DescriptorSet_Destroy(m_OutlineSet);
    Graph::DescriptorSet_Destroy(m_FXAASet);
    Graph::DescriptorSet_Destroy(m_CompositorSet);

    destroyFramebuffers();

    // TODO(Isma): Remove this!! This is responsibility of onyx.hpp, should not be in destructor
    Renderer_RemoveTarget(m_ViewBit);
    deallocateViewBit(m_ViewBit);
}

constexpr TKit::FixedArray<AttachmentRole, 2> s_ColorAtts{AttachmentRole_Color0, AttachmentRole_Color1};

template <Dimension D> void RenderView<D>::createFrameBuffers()
{
    TKIT_ASSERT(m_Framebuffers.IsEmpty(),
                "[ONYX][VIEW] Cannot create framebuffers on a non-empty frame buffer array with {} elements",
                m_Framebuffers.GetSize());
    const u32 maxFbs = m_FrameBufferCount;
    const u32v2 extent = GetImageExtent();

    const bool transparency = m_Flags & RenderViewFlag_Transparency;
    const bool outlines = m_Flags & RenderViewFlag_Outlines;
    const bool fxaa = m_Flags & RenderViewFlag_FXAA;
    TKit::FixedArray<bool, AttachmentRole_Count> mustCreate{true, true, true, true, transparency, transparency, fxaa};

    u32 pingPong = 0;

    for (u32 i = 0; i < maxFbs; ++i)
    {
        Framebuffer &fb = m_Framebuffers.Append();
        for (u32 att = 0; att < AttachmentRole_Count; ++att)
            fb.Attachments[att] = mustCreate[att] ? createAttachment(extent, AttachmentRole(att)) : Graph::NullHandle;

        const Graph::Sampler nearSampler = Renderer_GetNearSampler();
        const Graph::Sampler linearSampler = Renderer_GetLinearSampler();
        constexpr Graph::ImageLayout layout = Graph::ImageLayout_ShaderReadOnly;

        if (transparency)
        {
            const Graph::DescriptorImageInfo transparent = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[AttachmentRole_Accumulation]), layout, nearSampler);
            const Graph::DescriptorImageInfo revealage = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[AttachmentRole_Revealage]), layout, nearSampler);

            Graph::DescriptorSet_BeginRecordWrite(m_BlendSet);
            Graph::DescriptorSet_RecordWrite(m_BlendSet, ONYX_BLEND_PASS_TRANSPARENT_ATTACHMENTS_BINDING, transparent,
                                             i);
            Graph::DescriptorSet_RecordWrite(m_BlendSet, ONYX_BLEND_PASS_REVEALAGE_ATTACHMENTS_BINDING, revealage, i);
            Graph::DescriptorSet_EndRecordWrite(m_BlendSet);
        }

        if (outlines)
        {
            const Graph::DescriptorImageInfo color = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[s_ColorAtts[++pingPong & 1]]), layout, nearSampler);

            const Graph::DescriptorImageInfo outline = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[AttachmentRole_OutlineColor]), layout, nearSampler);
            // view 1 is the stencil-only view, created in createAttachment()
            const Graph::DescriptorImageInfo stencil = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[AttachmentRole_DepthStencil], 1), layout);

            Graph::DescriptorSet_BeginRecordWrite(m_OutlineSet);
            Graph::DescriptorSet_RecordWrite(m_OutlineSet, ONYX_OUTLINE_PASS_COLOR_ATTACHMENTS_BINDING, color, i);
            Graph::DescriptorSet_RecordWrite(m_OutlineSet, ONYX_OUTLINE_PASS_OUTLINE_ATTACHMENTS_BINDING, outline, i);
            Graph::DescriptorSet_RecordWrite(m_OutlineSet, ONYX_OUTLINE_PASS_STENCIL_ATTACHMENTS_BINDING, stencil, i);
            Graph::DescriptorSet_EndRecordWrite(m_OutlineSet);
        }

        if (fxaa)
        {
            const Graph::DescriptorImageInfo color = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[s_ColorAtts[++pingPong & 1]], 1), layout, linearSampler);

            Graph::DescriptorSet_Write(m_FXAASet, ONYX_FXAA_PASS_COLOR_ATTACHMENTS_BINDING, color, i);
        }

        // view 1 is the linear format view, created in createAttachment()
        const Graph::DescriptorImageInfo comp = Graph::DescriptorImageInfo::Create(
            Graph::Image_GetView(fb.Attachments[s_ColorAtts[++pingPong & 1]], 1), layout, nearSampler);

        Graph::DescriptorSet_Write(m_CompositorSet, ONYX_COMPOSITOR_PASS_COLOR_ATTACHMENTS_BINDING, comp, i);

        if (Graph::IsValidationEnabled())
        {
            TKit::FixedArray<TKit::StackString, AttachmentRole_Count> names{};
            names[AttachmentRole_Color0] = TKit::StackString::Format("onyx-color0-att-{}", i);
            names[AttachmentRole_Color1] = TKit::StackString::Format("onyx-color1-att-{}", i);
            names[AttachmentRole_OutlineColor] = TKit::StackString::Format("onyx-outline-att-{}", i);
            names[AttachmentRole_DepthStencil] = TKit::StackString::Format("onyx-depth-stencil-att-{}", i);
            names[AttachmentRole_Accumulation] = TKit::StackString::Format("onyx-transparent-att-{}", i);
            names[AttachmentRole_Revealage] = TKit::StackString::Format("onyx-revealage-att-{}", i);
            names[AttachmentRole_FXAA] = TKit::StackString::Format("onyx-fxaa-att-{}", i);
            for (u32 j = 0; j < AttachmentRole_Count; ++j)
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
    m_PingPong = 0;
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

template <Dimension D> f32v<D> RenderView<D>::ViewportToWorld(const f32v<D> &viewportPos) const
{
    const f32m<D> pv = Math::Inverse(m_ProjectionView);
    const f32v2 remapped = 2.f * (f32v2{viewportPos} / m_Viewport.Size) - 1.f;

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
        return vpos * f32v2{m_Viewport.Size};
    }
    else
    {
        const f32v4 clip = m_ProjectionView * f32v4{worldPos, 1.f};
        const f32v2 ndc = f32v2{clip} / clip[3];
        const f32v2 vpos = 0.5f * (ndc + 1.f);
        return vpos * f32v2{m_Viewport.Size};
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
                                             const Graph::StoreOp store, const f32v4 &clear = f32v4{0.f},
                                             const u32 viewIdx = 0)
{
    Graph::AttachmentInfo att{};
    att.View = Graph::Image_GetView(image, viewIdx);
    att.Layout = Graph::ImageLayout_ColorAttachment;
    att.LoadOp = load;
    att.StoreOp = store;
    att.ClearValue.Color = clear;
    return att;
}

static void beginRendering(const Graph::CommandBuffer cmd, const TKit::Span<const Graph::AttachmentInfo> colors,
                           const Graph::AttachmentInfo *depth, const u32v2 &ext, const Scissor &sc,
                           const f32v2 &contentScale)
{
    Graph::RenderPassInfo info{};
    info.RenderArea = {{0, 0}, ext};
    info.ColorAttachments = colors;
    info.DepthAttachment = depth;
    info.StencilAttachment = depth;

    const f32v2 fext = f32v2{ext};

    Graph::Command_SetViewport(cmd, f32v2{0.f}, fext);
    Graph::Command_SetScissor(cmd, i32v2{Math::Round(sc.Position * contentScale)},
                              u32v2{Math::Round(sc.Size * contentScale)});
    Graph::Command_BeginRenderPass(cmd, info);
}

template <Dimension D> void RenderView<D>::applyPostProcessBarriers(const Graph::CommandBuffer cmd) const
{
    const Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];
    const Graph::Image colorImg = fb.Attachments[s_ColorAtts[m_PingPong]];
    TKit::StaticArray<Graph::ImageMemoryBarrierInfo, 3> barriers{};
    barriers.Append(colorToShaderRead(colorImg));
    if (m_Flags & RenderViewFlag_Outlines)
    {
        barriers.Append(colorToShaderRead(fb.Attachments[AttachmentRole_OutlineColor]));
        barriers.Append(depthToShaderRead(fb.Attachments[AttachmentRole_DepthStencil]));
    }
    Graph::Command_ImageBarrier(cmd, barriers);
}

// a bit of a pass overview
//
// shadow passes are done directly in the renderer so they are not contemplated here
//
// first pass is the opaque pass. renders opaque geometry to either the intermediate attachment or the final attachment
// (latter if outlines are disabled. we skip one pass this way). this pass takes srgb textures -> linearizes ->
// does math with linear colors -> stores as srgb
//
// if transparency is enabled, transparent pass is executed. same deal as the opaque pass in terms of colorspace. the
// blend pass merges both opaque and transparent directly into the opaque's target (either intermediate attchm or final)
//
// if outlines are enabled, the pass is executed with intermediate as input and final as output
//
// the final image format is a bit special because it supports both: unorm and srgb. throughout all this pipeline, the
// format (view) used is srgb however, for the final window composite pass that merges all views, the format that is
// used for the final image is unorm. this is because the surface format is unorm (this is required for good imgui
// support)

template <Dimension D> void RenderView<D>::BeginOpaquePass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginOpaquePass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    m_PingPong = (m_PingPong + 1) & 1;
    const Graph::Image colorImg = fb.Attachments[s_ColorAtts[m_PingPong]];
    const Graph::Image outlImg = fb.Attachments[AttachmentRole_OutlineColor];
    const Graph::Image depthImg = fb.Attachments[AttachmentRole_DepthStencil];

    const bool hasOutline = m_Flags & RenderViewFlag_Outlines;
    const bool transparent = m_Flags & RenderViewFlag_Transparency;
    const bool keepDepth = transparent || hasOutline;

    Graph::ImageMemoryBarrierInfo barriers[3] = {toColorAttachment(colorImg), toColorAttachment(outlImg), {}};
    barriers[2].Image = depthImg;
    barriers[2].NewLayout = Graph::ImageLayout_DepthStencilAttachment;
    barriers[2].SrcStages = hasOutline ? Graph::PipelineStageFlag_FragmentShader : 0;
    barriers[2].DstStages = Graph::PipelineStageFlag_EarlyFragmentTests;
    barriers[2].DstAccess = Graph::AccessFlag_DepthStencilAttachmentWrite;
    Graph::Command_ImageBarrier(cmd, barriers);

    const Color ccolor = ClearColor.ToLinear();
    TKit::FixedArray<Graph::AttachmentInfo, 2> atts{};
    atts[0] = colorAttachment(colorImg, Graph::LoadOp_Clear, Graph::StoreOp_Store,
                              f32v4{ccolor.rgba[0], ccolor.rgba[1], ccolor.rgba[2], ccolor.rgba[3]});
    atts[1] = colorAttachment(outlImg, hasOutline ? Graph::LoadOp_Clear : Graph::LoadOp_DontCare,
                              hasOutline ? Graph::StoreOp_Store : Graph::StoreOp_DontCare);

    Graph::AttachmentInfo depth{};
    depth.View = Graph::Image_GetView(depthImg);
    depth.Layout = Graph::ImageLayout_DepthStencilAttachment;
    depth.LoadOp = Graph::LoadOp_Clear;
    depth.StoreOp = keepDepth ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;

    beginRendering(cmd, atts, &depth, GetImageExtent(), m_Scissor, m_ContentScale);
}

template <Dimension D> void RenderView<D>::EndOpaquePass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndOpaquePass");

    Graph::Command_EndRenderPass(cmd);

    // if there is transparency, BlendPass still needs the color image used here as the attachment output, so opaque
    // must hold on applying layout barriers if there is no transparency, opaque is the last pass until we begin the
    // post process passes, so it must handle the barriers
    if (!(m_Flags & RenderViewFlag_Transparency))
        applyPostProcessBarriers(cmd);
}

template <Dimension D> void RenderView<D>::BeginTransparentPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginTransparentPass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const Graph::Image accImg = fb.Attachments[AttachmentRole_Accumulation];
    const Graph::Image revImg = fb.Attachments[AttachmentRole_Revealage];
    const Graph::Image outlImg = fb.Attachments[AttachmentRole_OutlineColor];
    const Graph::Image depthImg = fb.Attachments[AttachmentRole_DepthStencil];

    const Graph::ImageMemoryBarrierInfo barriers[2] = {toColorAttachment(accImg), toColorAttachment(revImg)};
    Graph::Command_ImageBarrier(cmd, barriers);

    const bool hasOutline = (m_Flags & RenderViewFlag_Outlines) && (m_Flags & RenderViewFlag_Outlines);

    TKit::FixedArray<Graph::AttachmentInfo, 3> atts{};
    atts[0] = colorAttachment(accImg, Graph::LoadOp_Clear, Graph::StoreOp_Store);
    atts[1] = colorAttachment(outlImg, hasOutline ? Graph::LoadOp_Load : Graph::LoadOp_DontCare,
                              hasOutline ? Graph::StoreOp_Store : Graph::StoreOp_DontCare);
    atts[2] = colorAttachment(revImg, Graph::LoadOp_Clear, Graph::StoreOp_Store, f32v4{1.f, 0.f, 0.f, 0.f});

    Graph::AttachmentInfo depth{};
    depth.View = Graph::Image_GetView(depthImg);
    depth.Layout = Graph::ImageLayout_DepthStencilAttachment;
    depth.LoadOp = Graph::LoadOp_Load;
    depth.StoreOp = hasOutline ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;

    beginRendering(cmd, atts, &depth, GetImageExtent(), m_Scissor, m_ContentScale);
}

template <Dimension D> void RenderView<D>::EndTransparentPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndTransparentPass");

    Graph::Command_EndRenderPass(cmd);
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const Graph::ImageMemoryBarrierInfo barriers[2] = {colorToShaderRead(fb.Attachments[AttachmentRole_Accumulation]),
                                                       colorToShaderRead(fb.Attachments[AttachmentRole_Revealage])};
    Graph::Command_ImageBarrier(cmd, barriers);
}

template <Dimension D> void RenderView<D>::BeginBlendPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginBlendPass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const Graph::Image colorImg = fb.Attachments[s_ColorAtts[m_PingPong]];
    const Graph::AttachmentInfo color = colorAttachment(colorImg, Graph::LoadOp_Load, Graph::StoreOp_Store);
    beginRendering(cmd, color, nullptr, GetImageExtent(), m_Scissor, m_ContentScale);
}
template <Dimension D> void RenderView<D>::EndBlendPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndBlendPass");

    Graph::Command_EndRenderPass(cmd);
    applyPostProcessBarriers(cmd);
}

template <Dimension D> void RenderView<D>::BeginOutlinePass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::BeginOutlinePass");

    m_PingPong = (m_PingPong + 1) & 1;
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];
    const Graph::Image colorImg = fb.Attachments[s_ColorAtts[m_PingPong]];

    const Graph::ImageMemoryBarrierInfo barrier = toColorAttachment(colorImg);
    Graph::Command_ImageBarrier(cmd, barrier);

    const Graph::AttachmentInfo color = colorAttachment(colorImg, Graph::LoadOp_DontCare, Graph::StoreOp_Store);

    beginRendering(cmd, color, nullptr, GetImageExtent(), m_Scissor, m_ContentScale);
}

template <Dimension D> void RenderView<D>::EndOutlinePass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndOutlinePass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];
    Graph::Command_EndRenderPass(cmd);
    const Graph::ImageMemoryBarrierInfo barrier = colorToShaderRead(fb.Attachments[s_ColorAtts[m_PingPong]]);
    Graph::Command_ImageBarrier(cmd, barrier);
}

template <Dimension D> void RenderView<D>::BeginFXAAPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::BeginFXAAPass");

    m_PingPong = (m_PingPong + 1) & 1;
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];
    const Graph::Image colorImg = fb.Attachments[s_ColorAtts[m_PingPong]];

    const Graph::ImageMemoryBarrierInfo barrier = toColorAttachment(colorImg);
    Graph::Command_ImageBarrier(cmd, barrier);

    // we take the linear color view
    const Graph::AttachmentInfo color =
        colorAttachment(colorImg, Graph::LoadOp_DontCare, Graph::StoreOp_Store, f32v4{0.f}, 1);

    beginRendering(cmd, color, nullptr, GetImageExtent(), m_Scissor, m_ContentScale);
}

template <Dimension D> void RenderView<D>::EndFXAAPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndFXAAPass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    Graph::Command_EndRenderPass(cmd);
    const Graph::ImageMemoryBarrierInfo barrier = colorToShaderRead(fb.Attachments[s_ColorAtts[m_PingPong]]);
    Graph::Command_ImageBarrier(cmd, barrier);
}

template <Dimension D> bool RenderView<D>::IsWithinViewport(const f32v2 &screenPos) const
{
    const f32v2 viewportPos = ScreenToViewport(screenPos);

    return viewportPos[0] > 0.f && viewportPos[0] < m_Viewport.Size[0] && viewportPos[1] > 0.f &&
           viewportPos[1] < m_Viewport.Size[1];
}

template <Dimension D> void RenderView<D>::setViewport(const Viewport &vp)
{
    if (m_ScissorSizing == SizingPolicy_Proportional)
    {
        m_Scissor.Position = vp.Size * m_ScissorPosRatio;
        m_Scissor.Size = vp.Size * m_ScissorSizeRatio;
    }
    if (m_ImageSizing == SizingPolicy_Proportional)
        setImageSize(u32v2{Math::Round(vp.Size * m_ImageSizeRatio)});

    m_Viewport = vp;
}

template <Dimension D> void RenderView<D>::SetFlags(const RenderViewFlags flags)
{
    CacheMatrices();
    const RenderViewFlags f = m_Flags;

    m_Flags = flags;

    const RenderViewFlags fbFlags = RenderViewFlag_Transparency | RenderViewFlag_Outlines;
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
    //         t.Translation += 0.5f * vp.Size;
    //     else
    //         t.Translation += 0.5f * f32v3{vp.Size, 0.f};
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

    const f32 aspect = m_Viewport.Size[0] / m_Viewport.Size[1];

    if constexpr (D == D2)
    {
        TKIT_ASSERT(m_Camera->Mode != CameraMode_Perspective, "[ONYX][VIEW] 2D does not support perspective cameras");
        return m_Camera->Mode == CameraMode_Orthographic
                   ? Transform<D2>::Orthographic(m_Camera->OrthoParameters.Size, aspect)
                   : Transform<D2>::Orthographic(m_Viewport.Size[1], aspect);
    }
    else
    {
        const OrthographicParameters<D3> &oparams = m_Camera->OrthoParameters;
        const PerspectiveParameters &pparams = m_Camera->PerspParameters;

        if (m_Camera->Mode == CameraMode_Perspective)
            return Transform<D3>::Perspective(pparams.FieldOfView, pparams.Near, pparams.Far, aspect);
        if (m_Camera->Mode == CameraMode_Orthographic)
            return Transform<D3>::Orthographic(oparams.Size, aspect, oparams.Near, oparams.Far);
        return Transform<D3>::Orthographic(m_Viewport.Size[1], aspect, oparams.Near, oparams.Far);
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
