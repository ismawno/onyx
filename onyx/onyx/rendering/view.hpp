#pragma once

#include "onyx/property/camera.hpp"
#include "onyx/property/color.hpp"
#include "onyx/property/instance.hpp"

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

    VkViewport AsVulkanViewport(const VkExtent2D &extent) const;
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
    Color BackgroundColor;
    VkViewport Viewport;
    VkRect2D Scissor;
    ViewMask ViewBit;
    bool Transparent;
};
template <> struct ViewInfo<D3>
{
    f32m4 ProjectionView;
    Color BackgroundColor;
    f32v3 ViewPosition;
    f32v3 ViewForward;
    VkViewport Viewport;
    VkRect2D Scissor;
    ViewMask ViewBit;
    bool Transparent;
};

template <Dimension D> class RenderView
{
    TKIT_NON_COPYABLE(RenderView)

  public:
    RenderView(Camera<D> *camera, const ScreenViewport &viewport = {}, const ScreenScissor &scissor = {});
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
        if (m_Mode == ViewMode_Manual)
            return;
        m_View = ComputeView();
        m_Projection = ComputeProjection();
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

    void SetViewport(const ScreenViewport &vp)
    {
        m_Viewport = vp;
        CacheMatrices();
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

        info.BackgroundColor = BackgroundColor;
        info.Viewport = m_Viewport.AsVulkanViewport(m_Extent);
        info.Scissor = m_Scissor.AsVulkanScissor(m_Extent, m_Viewport);
        info.ViewBit = m_ViewBit;
        info.Transparent = Transparent;
        return info;
    }

    Color BackgroundColor{Color::Black};
    bool Transparent = false;

  private:
    void updateExtent(const VkExtent2D &extent)
    {
        m_Extent = extent;
    }

    Camera<D> *m_Camera;
    ScreenViewport m_Viewport;
    ScreenScissor m_Scissor;
    ViewMask m_ViewBit;

    VkExtent2D m_Extent{};
    f32m<D> m_View = f32m<D>::Identity();
    f32m<D> m_Projection = f32m<D>::Identity();
    f32m<D> m_ProjectionView = f32m<D>::Identity();
    ViewMode m_Mode = ViewMode_Automatic;

    friend class Window;
};

} // namespace Onyx
