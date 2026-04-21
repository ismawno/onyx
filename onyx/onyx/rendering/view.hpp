#pragma once

#include "onyx/property/camera.hpp"
#include "onyx/property/color.hpp"
#include "onyx/execution/execution.hpp"
#include "vkit/resource/device_image.hpp"

namespace Onyx
{
class Window;

/**
 * @brief The `ScreenViewport` struct holds screen viewport dimensions.
 *
 * It is represented as an axis-aligned rectangle with the `Min` and `Max` coordinates ranging from -1 to 1. The
 * `DepthBounds` are normalized, ranging from 0 to 1. The default values are set to cover the entire screen.
 */
struct ScreenViewport
{
    TKIT_REFLECT_DECLARE(ScreenViewport)
    TKIT_YAML_SERIALIZE_DECLARE(ScreenViewport)
    f32v2 Min{-1.f};
    f32v2 Max{1.f};
    f32v2 DepthBounds{0.f, 1.f};

    VkViewport AsVulkanViewport(const VkExtent2D &parent) const;
    VkExtent2D AsVulkanExtent(const VkExtent2D &parent) const;
};

/**
 * @brief The `ScreenScissor` struct holds screen scissor dimensions relative to a viewport.
 *
 * It is represented as an axis-aligned rectangle with the `Min` and `Max` coordinates ranging from -1 to 1. The default
 * values are set to cover the entire screen.
 */
struct ScreenScissor
{
    TKIT_REFLECT_DECLARE(ScreenScissor)
    TKIT_YAML_SERIALIZE_DECLARE(ScreenScissor)
    f32v2 Min{-1.f};
    f32v2 Max{1.f};

    VkRect2D AsVulkanScissor(const VkExtent2D &extent, const ScreenViewport &viewport) const;
};

enum ViewMode : u8
{
    ViewMode_Automatic,
    ViewMode_Manual
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

struct FrameBuffer
{
    Execution::Tracker Tracker{};
    VKit::DeviceImage Color{};
    VKit::DeviceImage DepthStencil{};
};

using RenderViewFlags = u8;
enum RenderViewFlagBit : RenderViewFlags
{
    RenderViewFlag_Shadows = 1 << 0,
};

template <Dimension D> class RenderView
{
    TKIT_NON_COPYABLE(RenderView)

  public:
    RenderView(const VkExtent2D &extent, VkDescriptorSet compositorSet, u32 id, Camera<D> *camera,
               RenderViewFlags flags = 0, const ScreenViewport &viewport = {}, const ScreenScissor &scissor = {});
    ~RenderView();

    f32v2 ScreenToViewport(const f32v2 &screenPos) const;
    f32v<D> ViewportToWorld(f32v<D> viewportPos) const;
    f32v2 WorldToViewport(const f32v<D> &worldPos) const;

    f32v2 ViewportToScreen(const f32v2 &viewportPos) const;
    f32v<D> ScreenToWorld(const f32v<D> &screenPos) const;
    f32v2 WorldToScreen(const f32v<D> &worldPos) const
    {
        return ViewportToScreen(WorldToViewport(worldPos));
    }

    void BeginRendering(VkCommandBuffer cmd, const Execution::Tracker &tracker);
    void EndRendering(VkCommandBuffer cmd);

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
        if (m_Mode != ViewMode_Manual)
        {
            m_View = ComputeView();
            m_Projection = ComputeProjection();
        }
        m_ProjectionView = m_Projection * m_View;
    }

    RenderViewFlags GetFlags() const
    {
        return m_Flags;
    }
    void SetFlags(const RenderViewFlags flags)
    {
        m_Flags = flags;
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

    void SetView(const f32m<D> &view)
    {
        m_View = view;
        m_Mode = ViewMode_Manual;
        m_ProjectionView = m_Projection * m_View;
    }
    void SetProjection(const f32m<D> &proj)
    {
        m_Projection = proj;
        m_Mode = ViewMode_Manual;
        m_ProjectionView = m_Projection * m_View;
    }

    u32 GetId() const
    {
        return m_Id;
    }
    u32 GetDescriptorIndex() const
    {
        return m_Id * m_FrameBuffers.GetSize() + m_ImageIndex;
    }

    ViewMask GetViewBit() const
    {
        return m_ViewBit;
    }

    const ScreenViewport &GetViewport() const
    {
        return m_Viewport;
    }
    const ScreenScissor &GetScissor() const
    {
        return m_Scissor;
    }

    VkViewport GetVulkanViewport() const
    {
        return m_Viewport.AsVulkanViewport(m_ParentExtent);
    }
    VkRect2D GetVulkanScissor() const
    {
        return m_Scissor.AsVulkanScissor(m_ParentExtent, m_Viewport);
    }

    // a bit risky to set in certain situations. must make sure no unsubmitted cmd buffer references any of the
    // framebuffers
    void SetViewport(const ScreenViewport &vp)
    {
        m_Viewport = vp;
        CacheMatrices();
        drainWork();
        recreateFrameBuffers(m_FrameBuffers.GetSize());
    }
    void SetScissor(const ScreenScissor &sc)
    {
        m_Scissor = sc;
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

    ViewMode GetMode() const
    {
        return m_Mode;
    }
    void SetMode(const ViewMode mode)
    {
        m_Mode = mode;
        if (mode == ViewMode_Automatic)
            CacheMatrices();
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
            info.ViewForward = m_Camera->View.Rotation * f32v3{0.f, 0.f, -1.f};
        }
        info.ViewBit = m_ViewBit;
        return info;
    }

    Color ClearColor{Color::Black};

  private:
    void createFramebuffers(u32 imageCount);
    void recreateFrameBuffers(u32 imageCount);
    void nameFramebuffers();
    void drainWork();

    void update(const VkExtent2D &parent, const u32 imageCount)
    {
        m_ParentExtent = parent;
        return recreateFrameBuffers(imageCount);
    }

    void destroyFrameBuffers();
    void acquireImage(const u32 index)
    {
        m_ImageIndex = index;
    }

    Camera<D> *m_Camera;
    ScreenViewport m_Viewport;
    ScreenScissor m_Scissor;
    ViewMask m_ViewBit;

    VkExtent2D m_ParentExtent{};
    f32m<D> m_View = f32m<D>::Identity();
    f32m<D> m_Projection = f32m<D>::Identity();
    f32m<D> m_ProjectionView = f32m<D>::Identity();
    ViewMode m_Mode = ViewMode_Automatic;

    TKit::TierArray<FrameBuffer> m_FrameBuffers{};
    VkDescriptorSet m_CompositorSet;
    u32 m_Id;
    u32 m_ImageIndex = TKIT_U32_MAX;
    RenderViewFlags m_Flags;

    friend class Window;
};

} // namespace Onyx
