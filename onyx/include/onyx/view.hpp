#pragma once

#include "onyx/camera.hpp"
#include "onyx/color.hpp"
#include "onyx/pass.hpp"
#include "graph/execution.hpp"
#include "tkit/container/tier_array.hpp"

namespace Onyx
{
constexpr u32v2 ApplyScale(const u32v2 &size, const f32v2 &contentScale)
{
    return u32v2{Math::Round(f32v2{size} * contentScale)};
}
constexpr u32v2 UnapplyScale(const u32v2 &size, const f32v2 &contentScale)
{
    return u32v2{Math::Round(f32v2{size} / contentScale)};
}

TKIT_REFLECT_DECLARE_ENUM(SizingPolicy)
TKIT_YAML_SERIALIZE_DECLARE_ENUM(SizingPolicy)
enum SizingPolicy : u8
{
    SizingPolicy_Proportional,
    SizingPolicy_Fixed,
};

struct Viewport
{
    TKIT_REFLECT_DECLARE(Viewport)
    TKIT_YAML_SERIALIZE_DECLARE(Viewport)
    f32v2 Position{0.f};
    f32v2 Size{0.f};
    f32v2 Depth{0.f, 1.f};
};

struct Scissor
{
    TKIT_REFLECT_DECLARE(Scissor)
    TKIT_YAML_SERIALIZE_DECLARE(Scissor)
    f32v2 Position{0.f};
    f32v2 Size{0.f};
};

template <Dimension D> struct ViewInfo
{
    f32m4 ProjectionView;
    ViewMask ViewBit;
};
template <> struct ViewInfo<D3>
{
    f32m4 ProjectionView;
    f32v3 ViewPosition;
    f32v3 ViewForward;
    ViewMask ViewBit;
};

using RenderViewFlags = u16;
enum RenderViewFlagBit : RenderViewFlags
{
    RenderViewFlag_Shadows = 1U << 0,
    RenderViewFlag_PostProcess = 1U << 1,
    RenderViewFlag_Outlines = 1U << 2,
    RenderViewFlag_Transparency = 1U << 3,
    RenderViewFlag_ManualProjectionView = 1U << 4,
    RenderViewFlag_Hidden = 1U << 5,
};

struct Framebuffer
{
    Graph::Tracker Tracker{};
    TKit::FixedArray<Graph::Image, Attachment_Count> Attachments{};
};

struct RenderViewSpecs
{
    u32 FrameBufferCount = 3;
    SizingPolicy ViewportSizing = SizingPolicy_Proportional;
    SizingPolicy ScissorSizing = SizingPolicy_Proportional;
    SizingPolicy ImageSizing = SizingPolicy_Proportional;
    RenderViewFlags Flags = 0;
};

// TODO(Isma): Thoroughly test absolute viewport and scissor coordinate usage here
template <Dimension D> class RenderView
{
    TKIT_NON_COPYABLE(RenderView)

  public:
    RenderView(const u32v2 &size, const f32v2 &contentScale, Camera<D> *camera, const RenderViewSpecs &specs);
    ~RenderView();

    f32v2 ScreenToViewport(const f32v2 &screenPos) const
    {
        return screenPos - m_Viewport.Position;
    }
    f32v2 ViewportToScreen(const f32v2 &viewportPos) const
    {
        return viewportPos + m_Viewport.Position;
    }

    f32v<D> ViewportToWorld(const f32v<D> &viewportPos) const;
    f32v2 WorldToViewport(const f32v<D> &worldPos) const;

    f32v<D> ScreenToWorld(const f32v<D> &screenPos) const
    {
        if constexpr (D == D2)
            return ViewportToWorld(ScreenToViewport(screenPos));
        else
        {
            const f32 z = screenPos[2];
            return ViewportToWorld(f32v3{ScreenToViewport(f32v2{screenPos}), z});
        }
    }
    f32v2 WorldToScreen(const f32v<D> &worldPos) const
    {
        return ViewportToScreen(WorldToViewport(worldPos));
    }

    void MarkCurrentAttachmentsInUse(const Graph::Tracker &tracker);

    void BeginOpaquePass(Graph::CommandBuffer cmd);
    void EndOpaquePass(Graph::CommandBuffer cmd);

    void BeginTransparentPass(Graph::CommandBuffer cmd);
    void EndTransparentPass(Graph::CommandBuffer cmd);

    void BeginBlendPass(Graph::CommandBuffer cmd);
    void EndBlendPass(Graph::CommandBuffer cmd);

    void BeginPostProcess(Graph::CommandBuffer cmd);
    void EndPostProcess(Graph::CommandBuffer cmd);

    bool IsWithinViewport(const f32v2 &screenPos) const;

    f32m<D> ComputeView() const;
    f32m<D> ComputeProjection() const;
    f32m<D> ComputeProjectionView() const
    {
        return ComputeProjection() * ComputeView();
    }
    void CacheMatrices()
    {
        if (!(m_Flags & RenderViewFlag_ManualProjectionView))
        {
            m_View = ComputeView();
            m_Projection = ComputeProjection();
        }
        m_ProjectionView = m_Projection * m_View;
    }

    const f32m<D> &GetView() const
    {
        return m_View;
    }
    const f32m<D> &GetProjection() const
    {
        return m_Projection;
    }
    const f32m<D> &GetProjectionView() const
    {
        return m_ProjectionView;
    }

    const Viewport &GetViewport() const
    {
        return m_Viewport;
    }
    const Scissor &GetScissor() const
    {
        return m_Scissor;
    }
    const u32v2 &GetImageSize() const
    {
        return m_ImageSize;
    }

    SizingPolicy GetViewportSizing() const
    {
        return m_ViewportSizing;
    }
    SizingPolicy GetScissorSizing() const
    {
        return m_ScissorSizing;
    }
    SizingPolicy GetImageSizing() const
    {
        return m_ImageSizing;
    }

    void SetFixedViewport()
    {
        m_ViewportSizing = SizingPolicy_Fixed;
    }
    void SetFixedScissor()
    {
        m_ScissorSizing = SizingPolicy_Fixed;
    }
    void SetFixedImage()
    {
        m_ImageSizing = SizingPolicy_Fixed;
    }

    void SetProportionalViewport()
    {
        m_ViewportSizing = SizingPolicy_Proportional;
        const f32v2 psize = f32v2{m_ParentSize};
        m_ViewportPosRatio = m_Viewport.Position / psize;
        m_ViewportSizeRatio = m_Viewport.Size / psize;
    }
    void SetProportionalScissor()
    {
        m_ScissorSizing = SizingPolicy_Proportional;
        m_ScissorPosRatio = m_Scissor.Position / m_Viewport.Size;
        m_ScissorSizeRatio = m_Scissor.Size / m_Viewport.Size;
    }
    void SetProportionalImageSize()
    {
        m_ImageSizing = SizingPolicy_Proportional;
        m_ImageSizeRatio = f32v2{m_ImageSize} / m_Viewport.Size;
    }

    void SetFixedViewport(const Viewport &vp)
    {
        setViewport(vp);
        m_ViewportSizing = SizingPolicy_Fixed;
    }
    void SetProportionalViewport(const Viewport &vp)
    {
        m_ViewportPosRatio = vp.Position;
        m_ViewportSizeRatio = vp.Size;
        m_ViewportSizing = SizingPolicy_Proportional;
        setViewportBasedOnParent();
    }
    void SetFixedScissor(const Scissor &sc)
    {
        m_Scissor = sc;
        m_ScissorSizing = SizingPolicy_Fixed;
    }
    void SetProportionalScissor(const Scissor &sc)
    {
        m_ScissorPosRatio = sc.Position;
        m_ScissorSizeRatio = sc.Size;
        m_Scissor = {sc.Position * m_Viewport.Size, sc.Size * m_Viewport.Size};
        m_ScissorSizing = SizingPolicy_Proportional;
    }
    void SetFixedImageSize(const u32v2 &size)
    {
        setImageSize(size);
        m_ImageSizing = SizingPolicy_Fixed;
    }
    void SetProportionalImageSize(const f32v2 &size)
    {
        m_ImageSizeRatio = size;
        setImageSize(u32v2{Math::Round(m_Viewport.Size * size)});
        m_ImageSizing = SizingPolicy_Proportional;
    }

    RenderViewFlags GetFlags() const
    {
        return m_Flags;
    }
    void SetFlags(RenderViewFlags flags);
    void AddFlags(const RenderViewFlags flags)
    {
        SetFlags(m_Flags | flags);
    }
    void RemoveFlags(const RenderViewFlags flags)
    {
        SetFlags(m_Flags & ~flags);
    }

    void SetView(const f32m<D> &view)
    {
        m_View = view;
        m_Flags |= RenderViewFlag_ManualProjectionView;
        m_ProjectionView = m_Projection * m_View;
    }
    void SetProjection(const f32m<D> &proj)
    {
        m_Projection = proj;
        m_Flags |= RenderViewFlag_ManualProjectionView;
        m_ProjectionView = m_Projection * m_View;
    }

    ViewMask GetViewBit() const
    {
        return m_ViewBit;
    }

    const u32v2 &GetParentSize() const
    {
        return m_ParentSize;
    }
    const f32v2 &GetContentScale() const
    {
        return m_ContentScale;
    }

    Camera<D> *GetCamera() const
    {
        return m_Camera;
    }

    void SetCamera(Camera<D> *camera)
    {
        m_Camera = camera;
        CacheMatrices();
    }

    Graph::DescriptorSet GetBlendSet() const
    {
        return m_BlendSet;
    }
    Graph::DescriptorSet GetPostProcessSet() const
    {
        return m_PostProcessSet;
    }
    Graph::DescriptorSet GetCompositorSet() const
    {
        return m_CompositorSet;
    }

    u32 GetAttachmentIndex() const
    {
        return m_AttachmentIndex;
    }

    void ZoomScroll(const f32v<D> &screenPos, f32 step);

    ViewInfo<D> CreateViewInfo() const
    {
        ViewInfo<D> info;
        if constexpr (D == D2)
            info.ProjectionView = Transform<D2>::Promote(m_ProjectionView);
        else
        {
            info.ProjectionView = m_ProjectionView;
            info.ViewPosition = m_Camera->View.Translation;
            info.ViewForward = Math::Normalize(m_Camera->View.Rotation) * f32v3{0.f, 0.f, -1.f};
        }
        info.ViewBit = m_ViewBit;
        return info;
    }

    u32v2 GetImageExtent() const
    {
        return ApplyScale(m_ImageSize, m_ContentScale);
    }

    Color ClearColor{Color_Black};
    // TODO(Isma): Think about if its worth it to have a per-instance outline width
    u32 MaxOutlineWidth = 10;
    u64 Layer = 0;

  private:
    void setImageSize(const u32v2 &size)
    {
        if (size == m_ImageSize)
            return;
        m_ImageSize = size;
        drainWork();
        recreateFramebuffers();
    }
    void setViewport(const Viewport &vp);
    void setViewportBasedOnParent()
    {
        TKIT_ASSERT(m_ViewportSizing == SizingPolicy_Proportional,
                    "[ONYX][VIEW] To set a viewport based on parent, the viewport sizing policy must be proportional");

        const f32v2 psize = f32v2{m_ParentSize};
        setViewport({m_ViewportPosRatio * psize, m_ViewportSizeRatio * psize, m_Viewport.Depth});
    }
    void findAvailableFramebuffer();
    void createFrameBuffers();
    void destroyFramebuffers();
    void recreateFramebuffers()
    {
        destroyFramebuffers();
        createFrameBuffers();
        findAvailableFramebuffer();
    }
    void drainWork() const;

    void update(const u32v2 &parentSize, const f32v2 &contentScale)
    {
        const u32v2 oldExtent = GetImageExtent();
        m_ParentSize = parentSize;
        m_ContentScale = contentScale;
        if (m_ViewportSizing == SizingPolicy_Proportional)
            setViewportBasedOnParent();
        if (GetImageExtent() != oldExtent)
        {
            drainWork();
            recreateFramebuffers();
        }
    }

    Camera<D> *m_Camera;
    ViewMask m_ViewBit;

    Viewport m_Viewport;
    Scissor m_Scissor;

    SizingPolicy m_ViewportSizing;
    SizingPolicy m_ScissorSizing;
    SizingPolicy m_ImageSizing;

    f32v2 m_ViewportPosRatio{0.f};
    f32v2 m_ViewportSizeRatio{1.f};

    f32v2 m_ScissorPosRatio{0.f};
    f32v2 m_ScissorSizeRatio{1.f};

    f32v2 m_ImageSizeRatio{1.f};

    u32v2 m_ParentSize{};
    u32v2 m_ImageSize{};
    f32v2 m_ContentScale;

    f32m<D> m_View = f32m<D>::Identity();
    f32m<D> m_Projection = f32m<D>::Identity();
    f32m<D> m_ProjectionView = f32m<D>::Identity();

    TKit::TierArray<Framebuffer> m_Framebuffers{};
    Graph::DescriptorSet m_BlendSet;
    Graph::DescriptorSet m_PostProcessSet;
    Graph::DescriptorSet m_CompositorSet;

    u32 m_AttachmentIndex = TKIT_U32_MAX;
    u32 m_FrameBufferCount;
    RenderViewFlags m_Flags;

    friend class Window;
    friend class RenderTarget;
};
} // namespace Onyx
