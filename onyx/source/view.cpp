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
static ViewImage createColorAttachment(const u32v2 &ext, const TKit::Span<const Graph::Format> formats,
                                       const Graph::SampleCount samples)
{
    ViewImage img;
    img.Image =
        Graph::Image_Create(ext, {.Formats = formats}, Graph::ImageFlag_ColorAttachment | Graph::ImageFlag_Sampled);

    for (const Graph::Format fmt : formats)
        Graph::Image_AddView(img.Image, {.Format = fmt});
    if (samples != Graph::SampleCount_1)
    {
        // here, we assume formats[0] is the writing format
        img.MultiSampled =
            Graph::Image_Create(ext, {.Formats = formats[0], .Samples = samples}, Graph::ImageFlag_ColorAttachment);
        Graph::Image_AddView(img.MultiSampled);
    }
    return img;
}

static ViewImage createDepthStencilAttachment(const u32v2 &ext, const Graph::SampleCount samples)
{
    ViewImage img;
    const Graph::Format fmt = GetAttachmentFormat(AttachmentKind_DepthStencil);
    img.Image = Graph::Image_Create(ext, {.Formats = fmt},
                                    Graph::ImageFlag_DepthAttachment | Graph::ImageFlag_StencilAttachment |
                                        Graph::ImageFlag_Sampled);
    Graph::Image_AddView(img.Image);
    Graph::Image_AddView(img.Image, {.Range = {.Aspect = Graph::ImageAspectFlag_Stencil}});

    if (samples != Graph::SampleCount_1)
    {
        img.MultiSampled = Graph::Image_Create(ext, {.Formats = fmt, .Samples = samples},
                                               Graph::ImageFlag_DepthAttachment | Graph::ImageFlag_StencilAttachment);
        Graph::Image_AddView(img.MultiSampled);
    }
    return img;
}

static ViewImage createAttachment(const u32v2 &ext, const AttachmentRole role)
{
    const Graph::SampleCount sc = GetSpecs().SampleCount;
    if (role == AttachmentRole_DepthStencil)
        return createDepthStencilAttachment(ext, sc);
    if (role == AttachmentRole_Accumulation)
    {
        const Graph::Format fmt = GetAttachmentFormat(AttachmentKind_Accumulation);
        return createColorAttachment(ext, fmt, sc);
    }
    if (role == AttachmentRole_Revealage)
    {
        const Graph::Format fmt = GetAttachmentFormat(AttachmentKind_Revealage);
        return createColorAttachment(ext, fmt, sc);
    }

    TKit::StaticArray<Graph::Format, 2> formats{};
    formats.Append(GetAttachmentFormat(AttachmentKind_SrgbColor));
    if (role == AttachmentRole_OutlineColor)
        return createColorAttachment(ext, formats, sc);

    formats.Append(GetAttachmentFormat(AttachmentKind_LinearColor));
    // color1 is the first standard color attachment target that receives rasterized geometry. color0 is never such
    // target, so no need to have multisampled images
    return createColorAttachment(ext, formats, role == AttachmentRole_Color1 ? sc : Graph::SampleCount_1);
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
    TKit::FixedArray<bool, AttachmentRole_Count> mustCreate{true, true, true, true, transparency, transparency};

    for (u32 i = 0; i < maxFbs; ++i)
    {
        u32 pingPong = 0;
        Framebuffer &fb = m_Framebuffers.Append();
        for (u32 att = 0; att < AttachmentRole_Count; ++att)
            fb.Attachments[att] = mustCreate[att] ? createAttachment(extent, AttachmentRole(att)) : ViewImage{};

        const Graph::Sampler nearSampler = Renderer_GetNearSampler();
        const Graph::Sampler edgeSampler = Renderer_GetEdgeSampler();
        constexpr Graph::ImageLayout layout = Graph::ImageLayout_ShaderReadOnly;

        if (transparency)
        {
            const Graph::DescriptorImageInfo transparent = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[AttachmentRole_Accumulation].Image), layout, nearSampler);
            const Graph::DescriptorImageInfo revealage = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[AttachmentRole_Revealage].Image), layout, nearSampler);

            Graph::DescriptorSet_BeginRecordWrite(m_BlendSet);
            Graph::DescriptorSet_RecordWrite(m_BlendSet, ONYX_BLEND_PASS_TRANSPARENT_ATTACHMENTS_BINDING, transparent,
                                             i);
            Graph::DescriptorSet_RecordWrite(m_BlendSet, ONYX_BLEND_PASS_REVEALAGE_ATTACHMENTS_BINDING, revealage, i);
            Graph::DescriptorSet_EndRecordWrite(m_BlendSet);
        }

        if (outlines)
        {
            const Graph::DescriptorImageInfo color = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[s_ColorAtts[++pingPong & 1]].Image), layout, nearSampler);

            const Graph::DescriptorImageInfo outline = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[AttachmentRole_OutlineColor].Image), layout, nearSampler);
            // view 1 is the stencil-only view, created in createAttachment()
            const Graph::DescriptorImageInfo stencil = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[AttachmentRole_DepthStencil].Image, 1), layout);

            Graph::DescriptorSet_BeginRecordWrite(m_OutlineSet);
            Graph::DescriptorSet_RecordWrite(m_OutlineSet, ONYX_OUTLINE_PASS_COLOR_ATTACHMENTS_BINDING, color, i);
            Graph::DescriptorSet_RecordWrite(m_OutlineSet, ONYX_OUTLINE_PASS_OUTLINE_ATTACHMENTS_BINDING, outline, i);
            Graph::DescriptorSet_RecordWrite(m_OutlineSet, ONYX_OUTLINE_PASS_STENCIL_ATTACHMENTS_BINDING, stencil, i);
            Graph::DescriptorSet_EndRecordWrite(m_OutlineSet);
        }

        if (fxaa)
        {
            const Graph::DescriptorImageInfo color = Graph::DescriptorImageInfo::Create(
                Graph::Image_GetView(fb.Attachments[s_ColorAtts[++pingPong & 1]].Image, 1), layout, edgeSampler);

            Graph::DescriptorSet_Write(m_FXAASet, ONYX_FXAA_PASS_COLOR_ATTACHMENTS_BINDING, color, i);
        }

        // view 1 is the linear format view, created in createAttachment()
        const Graph::DescriptorImageInfo comp = Graph::DescriptorImageInfo::Create(
            Graph::Image_GetView(fb.Attachments[s_ColorAtts[++pingPong & 1]].Image, 1), layout, nearSampler);

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
            for (u32 j = 0; j < AttachmentRole_Count; ++j)
                if (fb.Attachments[j].Image != Graph::NullHandle)
                {
                    Graph::Image_SetName(fb.Attachments[j].Image, names[j].CString());
                    Graph::Image_SetViewNames(fb.Attachments[j].Image, names[j].CString());
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
        for (const ViewImage &img : fb.Attachments)
        {
            if (img.Image != Graph::NullHandle)
                Graph::Image_Destroy(img.Image);
            if (img.MultiSampled != Graph::NullHandle)
                Graph::Image_Destroy(img.MultiSampled);
        }
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

static Graph::ImageMemoryBarrierInfo toColorAttachment(const Graph::Image image, const bool sampledLater = true)
{
    Graph::ImageMemoryBarrierInfo b{};
    b.Image = image;
    b.NewLayout = Graph::ImageLayout_ColorAttachment;
    b.SrcStages =
        sampledLater ? Graph::PipelineStageFlag_FragmentShader : Graph::PipelineStageFlag_ColorAttachmentOutput;
    b.SrcAccess = sampledLater ? 0 : Graph::AccessFlag_ColorAttachmentWrite;
    b.DstStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    b.DstAccess = Graph::AccessFlag_ColorAttachmentWrite;
    return b;
}

static Graph::ImageMemoryBarrierInfo toDepthAttachment(const Graph::Image image, const bool sampledLater = true)
{
    constexpr auto depthStages =
        Graph::PipelineStageFlag_EarlyFragmentTests | Graph::PipelineStageFlag_LateFragmentTests;

    Graph::ImageMemoryBarrierInfo b{};
    b.Image = image;
    b.NewLayout = Graph::ImageLayout_DepthStencilAttachment;
    b.SrcStages = sampledLater ? Graph::PipelineStageFlag_FragmentShader : depthStages;
    b.SrcAccess = sampledLater ? 0 : Graph::AccessFlag_DepthStencilAttachmentWrite;
    b.DstStages = depthStages;
    b.DstAccess = Graph::AccessFlag_DepthStencilAttachmentWrite;
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

static Graph::AttachmentInfo attachmentInfo(const Graph::Image image, const Graph::ImageLayout layout,
                                            const Graph::LoadOp load, const Graph::StoreOp store, const u32 viewIdx = 0)
{
    Graph::AttachmentInfo att{};
    att.View = Graph::Image_GetView(image, viewIdx);
    att.Layout = layout;
    att.LoadOp = load;
    att.StoreOp = store;
    return att;
}

static Graph::AttachmentInfo attachmentInfo(const ViewImage &image, const Graph::ImageLayout layout,
                                            const Graph::LoadOp load, Graph::StoreOp storeResolve,
                                            const Graph::StoreOp storeMs)
{
    const bool msaa = image.MultiSampled != Graph::NullHandle;

    // if there is no MSAA (so the same handle is used for the "multisampled" and "resolve" slots) and the user has
    // specified the multisampled image to be store, we are forced to give the single sample image the store option even
    // if the resolve is dont care, because in a single sample, the .Image field acts as both the resolve and
    // multisample slot (even tho its only sampled once)
    if (!msaa && storeMs == Graph::StoreOp_Store)
        storeResolve = Graph::StoreOp_Store;

    Graph::AttachmentInfo att = attachmentInfo(image.GetWriteImage(), layout, load, msaa ? storeMs : storeResolve);
    if (msaa && storeResolve != Graph::StoreOp_DontCare)
    {
        att.Resolve.View = Graph::Image_GetView(image.Image);
        att.Resolve.Layout = layout;
        att.Resolve.Mode =
            layout == Graph::ImageLayout_DepthStencilAttachment ? Graph::Resolve_SampleZero : Graph::Resolve_Average;
    }
    return att;
}
static Graph::AttachmentInfo colorAttachment(const Graph::Image image, const Graph::LoadOp load,
                                             const Graph::StoreOp store, const f32v4 &clear = f32v4{0.f},
                                             const u32 viewIdx = 0)
{
    Graph::AttachmentInfo att = attachmentInfo(image, Graph::ImageLayout_ColorAttachment, load, store, viewIdx);
    att.ClearValue.Color = clear;
    return att;
}

static Graph::AttachmentInfo colorAttachment(const ViewImage &image, const Graph::LoadOp load,
                                             const Graph::StoreOp storeResolve, const Graph::StoreOp storeMs,
                                             const f32v4 &clear = f32v4{0.f})
{
    Graph::AttachmentInfo att = attachmentInfo(image, Graph::ImageLayout_ColorAttachment, load, storeResolve, storeMs);
    att.ClearValue.Color = clear;
    return att;
}

static Graph::AttachmentInfo depthAttachment(const ViewImage &image, const Graph::LoadOp load,
                                             const Graph::StoreOp storeResolve, const Graph::StoreOp storeMs)
{
    return attachmentInfo(image, Graph::ImageLayout_DepthStencilAttachment, load, storeResolve, storeMs);
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
    const Graph::Image colorImg = fb.Attachments[s_ColorAtts[m_PingPong]].Image;
    TKit::StaticArray<Graph::ImageMemoryBarrierInfo, 3> barriers{};
    barriers.Append(colorToShaderRead(colorImg));
    if (m_Flags & RenderViewFlag_Outlines)
    {
        barriers.Append(colorToShaderRead(fb.Attachments[AttachmentRole_OutlineColor].Image));
        barriers.Append(depthToShaderRead(fb.Attachments[AttachmentRole_DepthStencil].Image));
    }
    Graph::Command_ImageBarrier(cmd, barriers);
}
template <Dimension D> Scissor RenderView<D>::applyImageScissor() const
{
    const f32v2 scale = f32v2{GetImageExtent()} / Math::Max(m_Viewport.Size, f32v2{1.f});
    Scissor sc;
    sc.Position = m_Scissor.Position * scale;
    sc.Size = m_Scissor.Size * scale;
    return sc;
}

// clang-format off
// fyi i generated this doc with ai
// =====================================================================================================================
//  PASS LAYOUT
// =====================================================================================================================
//
// Shadow passes run before these, in the renderer. Each frame, a view takes the first framebuffer the GPU is done with
// (findAvailableFramebuffer), so its attachments are never shared between frames in flight.
//
// ATTACHMENTS (one set per framebuffer)
//
//   Color0        srgb + linear view   single sample   post process target only, never rasterized into
//   Color1        srgb + linear view   + ms twin       geometry target
//   OutlineColor  srgb                 + ms twin       outline color, width in alpha, of outlined draws
//   DepthStencil  depth + stencil view + ms twin       stencil counts outlined draws
//   Accumulation  transparency only    + ms twin       weighted blended transparency
//   Revealage     transparency only    + ms twin
//
//   ms twins exist only when Specs::SampleCount > 1. Geometry passes rasterize into them and resolve into the single
//   sample images. Every later pass is single sample and full screen.
//
// FLOW
//
//   OPAQUE        clear --> Color1 -----------------------------------------------+
//     |                     OutlineColor, DepthStencil (resolved) ----------+     |
//     |  ms OutlineColor, ms DepthStencil                                   |     |
//     |  (kept only with transparency)                                      |     |
//     v                                                                     |     |
//   TRANSPARENT   clear --> Accumulation, Revealage --+                     |     |
//                 loads the ms twins, resolves        |                     |     |
//                 OutlineColor, DepthStencil again ---|-------------------->+     |
//                                                     v                     |     v
//   BLEND         samples Accumulation, Revealage, blends into Color1 (load, store)
//                                                                           |     |
//                                                                           v     v
//   OUTLINE       samples Color1, OutlineColor, stencil ------------------------> Color0
//                                                                                 |
//   FXAA          samples the previous color (linear view) ---------------------> the other one
//                                                                                 |
//   COMPOSITOR    in the target's own pass, samples the last color (linear view) <+
//
//   transparent and blend run only with RenderViewFlag_Transparency, outline with RenderViewFlag_Outlines, FXAA with
//   RenderViewFlag_FXAA. Without transparency, opaque writes Color1 and the post process passes follow directly
//
// PING-PONG
//
//   m_PingPong restarts at 0 every frame. Every pass that writes a new color image moves it to the other one
//
//   pass               writes     reads       m_PingPong
//   opaque, blend      Color1     -           1
//   outline            Color0     Color1      0
//   fxaa               last read  last write  flips
//
//   the compositor samples:      no fxaa    fxaa
//                no outlines     Color1     Color0
//                outlines        Color0     Color1
//
//   createFrameBuffers writes each descriptor by walking the same sequence from 0, for every framebuffer
//
// LOADS AND STORES (geometry passes, * = only with outlines, otherwise don't care)
//
//                  load      store resolve      store ms
//   OPAQUE
//   Color1         clear     store              don't care
//   OutlineColor   clear*    store*             store if transparency*
//   DepthStencil   clear     store if outlines  store if transparency
//   TRANSPARENT
//   Accumulation   clear     store              don't care
//   Revealage      clear     store              don't care
//   OutlineColor   load*     store*             don't care
//   DepthStencil   load      store if outlines  don't care
//
//   without msaa there is one image per attachment, stored if either column says store. blend loads and stores
//   Color1. outline and FXAA overwrite their whole target, so they don't care about its old contents
//
// LAYOUT HANDOFFS
//
//   every pass first moves its targets to attachment layout
//   end of transparent:               Accumulation, Revealage                   -> shader read, for blend
//   end of blend, or of opaque
//   without transparency:             Color1, + OutlineColor and DepthStencil*  -> shader read (applyPostProcessBarriers)
//   end of outline and of FXAA:       their output                              -> shader read, for the next pass
//
// COLOR SPACE
//
//   geometry, blend and outline go through the srgb views: they do their math in linear space and store srgb.
//   FXAA and the compositor use the linear (unorm) view of the same bytes. FXAA then sees perceptual values, as it
//   expects, and the compositor copies them as they are into the unorm swapchain
//   clang-format on

template <Dimension D> void RenderView<D>::BeginOpaquePass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginOpaquePass");

    const bool msaa = IsMultiSampled();
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    m_PingPong = (m_PingPong + 1) & 1;
    const ViewImage &colorImg = fb.Attachments[s_ColorAtts[m_PingPong]];
    const ViewImage &outlImg = fb.Attachments[AttachmentRole_OutlineColor];
    const ViewImage &depthImg = fb.Attachments[AttachmentRole_DepthStencil];

    const bool hasOutline = m_Flags & RenderViewFlag_Outlines;
    const bool hasTransp = m_Flags & RenderViewFlag_Transparency;

    TKit::StaticArray<Graph::ImageMemoryBarrierInfo, 6> barriers{};
    barriers.Append(toColorAttachment(colorImg.Image));
    barriers.Append(toColorAttachment(outlImg.Image));
    barriers.Append(toDepthAttachment(depthImg.Image, hasOutline));

    if (msaa)
    {
        barriers.Append(toColorAttachment(colorImg.MultiSampled, false));
        barriers.Append(toColorAttachment(outlImg.MultiSampled, false));
        barriers.Append(toDepthAttachment(depthImg.MultiSampled, false));
    }

    Graph::Command_ImageBarrier(cmd, barriers);

    const Color ccolor = ClearColor.ToLinear();
    TKit::FixedArray<Graph::AttachmentInfo, 2> atts{};

    atts[0] = colorAttachment(colorImg, Graph::LoadOp_Clear, Graph::StoreOp_Store, Graph::StoreOp_DontCare,
                              f32v4{ccolor.rgba[0], ccolor.rgba[1], ccolor.rgba[2], ccolor.rgba[3]});

    // hasTransp pass may read the multisampled image for outlines (only if outlines are enabled), to continue to
    // append to it
    const Graph::StoreOp outlStoreMs = (hasTransp && hasOutline) ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;
    // outline pass will read the resolve attachment
    const Graph::StoreOp outlStoreRes = hasOutline ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;
    // we dont care about the outlines if they wont be read later
    const Graph::LoadOp outlLoad = hasOutline ? Graph::LoadOp_Clear : Graph::LoadOp_DontCare;

    atts[1] = colorAttachment(outlImg, outlLoad, outlStoreRes, outlStoreMs);

    // transarent pass will load the multisampled depth attachment
    const Graph::StoreOp depStoreMs = hasTransp ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;
    // outline pass will load the resolved depth attachment
    const Graph::StoreOp depStoreRes = hasOutline ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;

    const Graph::AttachmentInfo depth = depthAttachment(depthImg, Graph::LoadOp_Clear, depStoreRes, depStoreMs);
    beginRendering(cmd, atts, &depth, GetImageExtent(), applyImageScissor(), m_ContentScale);
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

    const bool msaa = IsMultiSampled();
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const ViewImage &accImg = fb.Attachments[AttachmentRole_Accumulation];
    const ViewImage &revImg = fb.Attachments[AttachmentRole_Revealage];
    const ViewImage &outlImg = fb.Attachments[AttachmentRole_OutlineColor];
    const ViewImage &depthImg = fb.Attachments[AttachmentRole_DepthStencil];

    TKit::StaticArray<Graph::ImageMemoryBarrierInfo, 8> barriers{};
    barriers.Append(toColorAttachment(accImg.Image));
    barriers.Append(toColorAttachment(revImg.Image));
    barriers.Append(toColorAttachment(outlImg.Image));
    barriers.Append(toDepthAttachment(depthImg.Image));
    if (msaa)
    {
        barriers.Append(toColorAttachment(accImg.MultiSampled, false));
        barriers.Append(toColorAttachment(revImg.MultiSampled, false));
        barriers.Append(toColorAttachment(outlImg.MultiSampled, false));
        barriers.Append(toDepthAttachment(depthImg.MultiSampled, false));
    }
    Graph::Command_ImageBarrier(cmd, barriers);

    const bool hasOutline = m_Flags & RenderViewFlag_Outlines;

    TKit::FixedArray<Graph::AttachmentInfo, 3> atts{};
    atts[0] = colorAttachment(accImg, Graph::LoadOp_Clear, Graph::StoreOp_Store, Graph::StoreOp_DontCare);

    // here, we only keep the resolve attachments, we dont have any use for the multisampled anymore, their lives end
    // here
    const Graph::StoreOp outlStoreRes = hasOutline ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;
    const Graph::LoadOp outlLoad = hasOutline ? Graph::LoadOp_Load : Graph::LoadOp_DontCare;
    atts[1] = colorAttachment(outlImg, outlLoad, outlStoreRes, Graph::StoreOp_DontCare);
    atts[2] = colorAttachment(revImg, Graph::LoadOp_Clear, Graph::StoreOp_Store, Graph::StoreOp_DontCare,
                              f32v4{1.f, 0.f, 0.f, 0.f});

    const Graph::StoreOp depStoreRes = hasOutline ? Graph::StoreOp_Store : Graph::StoreOp_DontCare;
    const Graph::AttachmentInfo depth =
        depthAttachment(depthImg, Graph::LoadOp_Load, depStoreRes, Graph::StoreOp_DontCare);
    beginRendering(cmd, atts, &depth, GetImageExtent(), applyImageScissor(), m_ContentScale);
}

template <Dimension D> void RenderView<D>::EndTransparentPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndTransparentPass");

    Graph::Command_EndRenderPass(cmd);
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const Graph::ImageMemoryBarrierInfo barriers[2] = {
        colorToShaderRead(fb.Attachments[AttachmentRole_Accumulation].Image),
        colorToShaderRead(fb.Attachments[AttachmentRole_Revealage].Image)};
    Graph::Command_ImageBarrier(cmd, barriers);
}

template <Dimension D> void RenderView<D>::BeginBlendPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderView::BeginBlendPass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    const Graph::Image colorImg = fb.Attachments[s_ColorAtts[m_PingPong]].Image;
    const Graph::AttachmentInfo color = colorAttachment(colorImg, Graph::LoadOp_Load, Graph::StoreOp_Store);
    beginRendering(cmd, color, nullptr, GetImageExtent(), applyImageScissor(), m_ContentScale);
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
    const Graph::Image outlImg = fb.Attachments[s_ColorAtts[m_PingPong]].Image;

    const Graph::ImageMemoryBarrierInfo barrier = toColorAttachment(outlImg);
    Graph::Command_ImageBarrier(cmd, barrier);

    const Graph::AttachmentInfo color = colorAttachment(outlImg, Graph::LoadOp_DontCare, Graph::StoreOp_Store);
    beginRendering(cmd, color, nullptr, GetImageExtent(), applyImageScissor(), m_ContentScale);
}

template <Dimension D> void RenderView<D>::EndOutlinePass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndOutlinePass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];
    Graph::Command_EndRenderPass(cmd);
    const Graph::ImageMemoryBarrierInfo barrier = colorToShaderRead(fb.Attachments[s_ColorAtts[m_PingPong]].Image);
    Graph::Command_ImageBarrier(cmd, barrier);
}

template <Dimension D> void RenderView<D>::BeginFXAAPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::BeginFXAAPass");

    m_PingPong = (m_PingPong + 1) & 1;
    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];
    const Graph::Image colorImg = fb.Attachments[s_ColorAtts[m_PingPong]].Image;

    const Graph::ImageMemoryBarrierInfo barrier = toColorAttachment(colorImg);
    Graph::Command_ImageBarrier(cmd, barrier);

    // we take the linear color view
    const Graph::AttachmentInfo color =
        colorAttachment(colorImg, Graph::LoadOp_DontCare, Graph::StoreOp_Store, f32v4{0.f}, 1);

    beginRendering(cmd, color, nullptr, GetImageExtent(), applyImageScissor(), m_ContentScale);
}

template <Dimension D> void RenderView<D>::EndFXAAPass(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::View::EndFXAAPass");

    Framebuffer &fb = m_Framebuffers[m_AttachmentIndex];

    Graph::Command_EndRenderPass(cmd);
    const Graph::ImageMemoryBarrierInfo barrier = colorToShaderRead(fb.Attachments[s_ColorAtts[m_PingPong]].Image);
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

    const RenderViewFlags fbFlags = RenderViewFlag_Transparency | RenderViewFlag_Outlines | RenderViewFlag_FXAA;
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
