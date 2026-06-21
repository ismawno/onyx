#pragma once

#include "onyx/core.hpp"
#include "onyx/camera.hpp"
#include "onyx/color.hpp"
#include "tkit/container/tier_array.hpp"

ONYX_DECLARE_NON_DISPATCHABLE_VK_HANDLE(DescriptorSet)
ONYX_DECLARE_NON_DISPATCHABLE_VK_HANDLE(Semaphore)
ONYX_DECLARE_DISPATCHABLE_VK_HANDLE(CommandBuffer)

namespace Onyx::Execution
{
struct Tracker;
}

namespace Onyx
{
struct Viewport
{
    f32v2 Position{0.f};
    f32v2 Extent{1.f};
    f32v2 Depth{0.f, 1.f};
};

struct Scissor
{
    f32v2 Position{0.f};
    f32v2 Extent{1.f};
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
    RenderViewFlag_NormalizedViewportCoordinates = 1U << 5,
    RenderViewFlag_NormalizedScissorCoordinates = 1U << 6,
    RenderViewFlag_NormalizedCoordinates =
        RenderViewFlag_NormalizedViewportCoordinates | RenderViewFlag_NormalizedScissorCoordinates,
    RenderViewFlag_DynamicViewport = 1U << 7,
    RenderViewFlag_Hidden = 1U << 8,
};

struct Framebuffer;

// TODO(Isma): Thoroughly test absolute viewport and scissor coordinate usage here
template <Dimension D> class RenderView
{
    TKIT_NON_COPYABLE(RenderView)

  public:
    RenderView(const u32v2 &extent, Camera<D> *camera, RenderViewFlags flags = 0);
    ~RenderView();

    // depending on flags, these will expect and return normalized or absolute coordinates!
    f32v2 ScreenToViewport(const f32v2 &screenPos) const;
    f32v2 ViewportToScreen(const f32v2 &viewportPos) const;

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

    void MarkCurrentAttachmentsInUse(const Execution::Tracker &tracker);

    void BeginOpaquePass(Onyx_CommandBuffer cmd);
    void EndOpaquePass(Onyx_CommandBuffer cmd);

    void BeginTransparentPass(Onyx_CommandBuffer cmd);
    void EndTransparentPass(Onyx_CommandBuffer cmd);

    void BeginBlendPass(Onyx_CommandBuffer cmd);
    void EndBlendPass(Onyx_CommandBuffer cmd);

    void BeginPostProcess(Onyx_CommandBuffer cmd);
    void EndPostProcess(Onyx_CommandBuffer cmd);

    bool IsWithinViewport(const f32v2 &screenPos) const
    {
        const f32v2 viewportPos = ScreenToViewport(screenPos);
        return viewportPos[0] > -1.f && viewportPos[0] < 1.f && viewportPos[1] > -1.f && viewportPos[1] < 1.f;
    }

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

    RenderViewFlags GetFlags() const
    {
        return m_Flags;
    }
    void SetFlags(const RenderViewFlags flags)
    {
        CacheMatrices();
        const RenderViewFlags f = m_Flags;
        m_Flags = flags;

        const RenderViewFlags fbFlags =
            RenderViewFlag_DynamicViewport | RenderViewFlag_Transparency | RenderViewFlag_PostProcess;
        if ((flags & fbFlags) != (f & fbFlags))
        {
            drainWork();
            recreateFramebuffers();
        }
    }
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

    Viewport GetNormalizedViewport() const
    {
        if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
            return m_Viewport;
        return asNormalizedViewport();
    }
    Viewport GetAbsoluteViewport() const
    {
        if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
            return asAbsoluteViewport();
        return m_Viewport;
    }

    void SetNormalizedViewport(const Viewport &vp)
    {
        const f32v2 ip = 1.f / f32v2{m_ParentExtent};
        m_Viewport.Position = Math::Clamp(vp.Position, 0.f, 1.f);
        m_Viewport.Extent = Math::Clamp(vp.Extent, ip, f32v2{1.f});

        if (!(m_Flags & RenderViewFlag_NormalizedViewportCoordinates))
            m_Viewport = asAbsoluteViewport();

        CacheMatrices();
        if (!(m_Flags & RenderViewFlag_DynamicViewport))
        {
            drainWork();
            recreateFramebuffers();
        }
    }
    void SetAbsoluteViewport(const Viewport &vp)
    {
        m_Viewport.Position = Math::Clamp(vp.Position, f32v2{0.f}, f32v2{m_ParentExtent});
        m_Viewport.Extent = Math::Clamp(vp.Extent, f32v2{1.f}, f32v2{m_ParentExtent});

        if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
            m_Viewport = asNormalizedViewport();

        CacheMatrices();
        if (!(m_Flags & RenderViewFlag_DynamicViewport))
        {
            drainWork();
            recreateFramebuffers();
        }
    }
    void SetNormalizedViewportPosition(const f32v2 &pos)
    {
        m_Viewport.Position = Math::Clamp(pos, 0.f, 1.f);
        if (!(m_Flags & RenderViewFlag_NormalizedViewportCoordinates))
            m_Viewport = asAbsoluteViewport();
    }
    void SetAbsoluteViewportPosition(const f32v2 &pos)
    {
        m_Viewport.Position = Math::Clamp(pos, f32v2{0.f}, f32v2{m_ParentExtent});

        if (m_Flags & RenderViewFlag_NormalizedViewportCoordinates)
            m_Viewport = asNormalizedViewport();
    }

    Scissor GetNormalizedScissor() const
    {
        if (m_Flags & RenderViewFlag_NormalizedScissorCoordinates)
            return m_Scissor;
        return asNormalizedScissor();
    }
    Scissor GetAbsoluteScissor() const
    {
        if (m_Flags & RenderViewFlag_NormalizedScissorCoordinates)
            return asAbsoluteScissor();
        return m_Scissor;
    }

    void SetNormalizedScissor(const Scissor &sc)
    {
        const u32v2 ext = GetExtent();
        const f32v2 ip = 1.f / f32v2{ext};
        m_Scissor.Position = Math::Clamp(sc.Position, 0.f, 1.f);
        m_Scissor.Extent = Math::Clamp(sc.Extent, ip, f32v2{1.f});

        if (!(m_Flags & RenderViewFlag_NormalizedScissorCoordinates))
            m_Scissor = asAbsoluteScissor();
    }
    void SetAbsoluteScissor(const Scissor &sc)
    {
        const u32v2 ext = GetExtent();
        m_Scissor.Position = Math::Clamp(sc.Position, f32v2{0.f}, f32v2{ext});
        m_Scissor.Extent = Math::Clamp(sc.Extent, f32v2{1.f}, f32v2{ext});

        if (m_Flags & RenderViewFlag_NormalizedScissorCoordinates)
            m_Scissor = asNormalizedScissor();
    }

    u32v2 GetExtent() const
    {
        return u32v2{GetAbsoluteViewport().Extent};
    }
    u32v2 GetRenderExtent() const
    {
        if (m_Flags & RenderViewFlag_DynamicViewport)
            return m_ParentExtent;

        return GetExtent();
    }
    const u32v2 &GetParentExtent() const
    {
        return m_ParentExtent;
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

    Onyx_DescriptorSet GetBlendSet() const
    {
        return m_BlendSet;
    }
    Onyx_DescriptorSet GetPostProcessSet() const
    {
        return m_PostProcessSet;
    }
    Onyx_DescriptorSet GetCompositorSet() const
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

    Color ClearColor{Color_Black};
    // TODO(Isma): Think about if its worth it to have a per-instance outline width
    u32 MaxOutlineWidth = 10;
    u64 Layer = 0;

  private:
    void findAvailableFramebuffer();
    void destroyFramebuffers();
    void recreateFramebuffers()
    {
        destroyFramebuffers();
        findAvailableFramebuffer();
    }
    void drainWork();

    Viewport asNormalizedViewport() const
    {
        const f32v2 extent = f32v2{m_ParentExtent};
        const f32v2 pos = m_Viewport.Position / extent;
        const f32v2 ext = m_Viewport.Extent / extent;
        return {pos, ext, m_Viewport.Depth};
    }
    Scissor asNormalizedScissor() const
    {
        const f32v2 extent = GetAbsoluteViewport().Extent;
        const f32v2 pos = m_Scissor.Position / extent;
        const f32v2 ext = m_Scissor.Extent / extent;
        return {pos, ext};
    }

    Viewport asAbsoluteViewport() const
    {
        const f32v2 extent = f32v2{m_ParentExtent};
        const f32v2 pos = m_Viewport.Position * extent;
        const f32v2 ext = m_Viewport.Extent * extent;
        return {pos, ext, m_Viewport.Depth};
    }
    Scissor asAbsoluteScissor() const
    {
        const f32v2 extent = f32v2{m_ParentExtent};
        const f32v2 pos = m_Scissor.Position * extent;
        const f32v2 ext = m_Scissor.Extent * extent;
        return {pos, ext};
    }

    void update(const u32v2 &parentExtent)
    {
        m_ParentExtent = parentExtent;
        if (m_Flags & (RenderViewFlag_DynamicViewport | RenderViewFlag_NormalizedViewportCoordinates))
            recreateFramebuffers();
    }

    Camera<D> *m_Camera;
    Viewport m_Viewport;
    Scissor m_Scissor;
    ViewMask m_ViewBit;

    u32v2 m_ParentExtent{};
    f32m<D> m_View = f32m<D>::Identity();
    f32m<D> m_Projection = f32m<D>::Identity();
    f32m<D> m_ProjectionView = f32m<D>::Identity();

    TKit::TierArray<Framebuffer *> m_Framebuffers{};
    Onyx_DescriptorSet m_BlendSet;
    Onyx_DescriptorSet m_PostProcessSet;
    Onyx_DescriptorSet m_CompositorSet;
    u32 m_AttachmentIndex = TKIT_U32_MAX;
    RenderViewFlags m_Flags = 0;

    friend class Window;
    friend class RenderTarget;
};
} // namespace Onyx
