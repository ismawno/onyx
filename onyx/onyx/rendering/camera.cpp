#include "onyx/core/pch.hpp"
#include "onyx/rendering/camera.hpp"
#include "onyx/app/window.hpp"

namespace Onyx
{
using namespace Detail;

VkViewport ScreenViewport::AsVulkanViewport(const VkExtent2D &p_Extent) const
{
    VkViewport viewport;
    viewport.x = 0.5f * (1.f + Min[0]) * p_Extent.width;
    viewport.y = 0.5f * (1.f - Max[1]) * p_Extent.height;
    viewport.width = 0.5f * (1.f + Max[0]) * p_Extent.width - viewport.x;
    viewport.height = 0.5f * (1.f - Min[1]) * p_Extent.height - viewport.y;
    viewport.minDepth = DepthBounds[0];
    viewport.maxDepth = DepthBounds[1];

    return viewport;
}

VkRect2D ScreenScissor::AsVulkanScissor(const VkExtent2D &p_Extent, const ScreenViewport &p_Viewport) const
{
    const f32v2 size = p_Viewport.Max - p_Viewport.Min;
    const f32v2 min = p_Viewport.Min + 0.5f * (1.f + Min) * size;
    const f32v2 max = p_Viewport.Min + 0.5f * (1.f + Max) * size;

    VkRect2D scissor;
    scissor.offset.x = static_cast<i32>(0.5f * (1.f + min[0]) * p_Extent.width);
    scissor.offset.y = static_cast<i32>(0.5f * (1.f - max[1]) * p_Extent.height);
    scissor.extent.width =
        static_cast<u32>(0.5f * (1.f + max[0]) * p_Extent.width) - static_cast<u32>(scissor.offset.x);
    scissor.extent.height =
        static_cast<u32>(0.5f * (1.f - min[1]) * p_Extent.height) - static_cast<u32>(scissor.offset.y);

    return scissor;
}

template <Dimension D> f32v2 ICamera<D>::ScreenToViewport(const f32v2 &p_ScreenPos) const
{
    const f32v2 size = m_Viewport.Max - m_Viewport.Min;
    return -1.f + 2.f * (p_ScreenPos - m_Viewport.Min) / size;
}
template <Dimension D> f32v<D> ICamera<D>::ViewportToWorld(f32v<D> p_ViewportPos) const
{
    p_ViewportPos[1] = -p_ViewportPos[1]; // Invert y axis to undo onyx's inversion to GLFW
    if constexpr (D == D2)
    {
        const f32m3 itransform3 = Math::Inverse(m_ProjectionView.ProjectionView);
        f32m4 itransform = Onyx::Transform<D2>::Promote(itransform3);
        ApplyCoordinateSystemIntrinsic(itransform);
        return itransform * f32v4{p_ViewportPos, 0.f, 1.f};
    }
    else
    {
        const f32v4 clip = Math::Inverse(m_ProjectionView.ProjectionView) * f32v4{p_ViewportPos, 1.f};
        return f32v3{clip} / clip[3];
    }
}

template <Dimension D> f32v2 ICamera<D>::WorldToViewport(const f32v<D> &p_WorldPos) const
{
    if constexpr (D == D2)
    {
        f32m4 transform = Onyx::Transform<D2>::Promote(m_ProjectionView.ProjectionView);
        ApplyCoordinateSystemExtrinsic(transform);
        f32v2 viewportPos = f32v2{transform * f32v4{p_WorldPos, 0.f, 1.f}};
        viewportPos[1] = -viewportPos[1];
        return viewportPos;
    }
    else
    {
        f32v4 clip = m_ProjectionView.ProjectionView * f32v4{p_WorldPos, 1.f};
        clip[1] = -clip[1];
        return f32v2{clip} / clip[3];
    }
}

template <Dimension D> f32v2 ICamera<D>::ViewportToScreen(const f32v2 &p_ViewportPos) const
{
    const f32v2 size = m_Viewport.Max - m_Viewport.Min;
    return m_Viewport.Min + 0.5f * (1.f + p_ViewportPos) * size;
}

template <Dimension D> f32v<D> ICamera<D>::ScreenToWorld(const f32v<D> &p_ScreenPos) const
{
    if constexpr (D == D2)
        return ViewportToWorld(ScreenToViewport(p_ScreenPos));
    else
    {
        const f32 z = p_ScreenPos[2];
        return ViewportToWorld(f32v3{ScreenToViewport(f32v2{p_ScreenPos}), z});
    }
}
template <Dimension D> f32v2 ICamera<D>::WorldToScreen(const f32v<D> &p_WorldPos) const
{
    return ViewportToScreen(WorldToViewport(p_WorldPos));
}

template <Dimension D> f32v2 ICamera<D>::GetViewportMousePosition() const
{
    return ScreenToViewport(Input::GetScreenMousePosition(m_Window));
}

template <Dimension D> void ICamera<D>::SetView(const Onyx::Transform<D> &p_View)
{
    m_ProjectionView.View = p_View;
    adaptViewToViewportAspect();
}
template <Dimension D> void ICamera<D>::SetViewport(const ScreenViewport &p_Viewport)
{
    m_Viewport = p_Viewport;
    adaptViewToViewportAspect();
}
template <Dimension D> void ICamera<D>::SetScissor(const ScreenScissor &p_Scissor)
{
    m_Scissor = p_Scissor;
}

f32v2 Camera<D2>::GetWorldMousePosition() const
{
    return ScreenToWorld(Input::GetScreenMousePosition(m_Window));
}
void Camera<D2>::SetSize(const f32 p_Size)
{
    const f32 aspect = m_ProjectionView.View.Scale[0] / m_ProjectionView.View.Scale[0];
    m_ProjectionView.View.Scale[0] = p_Size * aspect;
    m_ProjectionView.View.Scale[1] = p_Size;
}
f32v3 Camera<D3>::GetWorldMousePosition(const f32 p_Depth) const
{
    return ScreenToWorld(f32v3{Input::GetScreenMousePosition(m_Window), p_Depth});
}

template <Dimension D> void ICamera<D>::ControlMovementWithUserInput(const CameraControls<D> &p_Controls)
{
    Onyx::Transform<D> &view = m_ProjectionView.View;
    f32v<D> translation{0.f};
    if (Input::IsKeyPressed(m_Window, p_Controls.Left))
        translation[0] -= view.Scale[0] * p_Controls.TranslationStep;
    if (Input::IsKeyPressed(m_Window, p_Controls.Right))
        translation[0] += view.Scale[0] * p_Controls.TranslationStep;

    if (Input::IsKeyPressed(m_Window, p_Controls.Up))
        translation[1] += view.Scale[1] * p_Controls.TranslationStep;
    if (Input::IsKeyPressed(m_Window, p_Controls.Down))
        translation[1] -= view.Scale[1] * p_Controls.TranslationStep;

    if constexpr (D == D2)
    {
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateLeft))
            view.Rotation += p_Controls.RotationStep;
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateRight))
            view.Rotation -= p_Controls.RotationStep;
    }
    else
    {
        if (Input::IsKeyPressed(m_Window, p_Controls.Forward))
            translation[2] -= view.Scale[2] * p_Controls.TranslationStep;
        if (Input::IsKeyPressed(m_Window, p_Controls.Backward))
            translation[2] += view.Scale[2] * p_Controls.TranslationStep;

        f32v2 mpos = Input::GetScreenMousePosition(m_Window);
        mpos[1] = -mpos[1]; // Invert y axis to undo onyx's inversion to GLFW, so that now when applying the
                            // rotation around x axis everything works out

        const bool lookAround = Input::IsKeyPressed(m_Window, p_Controls.ToggleLookAround);
        const f32v2 delta = lookAround ? 3.f * (m_PrevMousePos - mpos) : f32v2{0.f};
        m_PrevMousePos = mpos;

        f32v3 angles{delta[1], delta[0], 0.f};
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateLeft))
            angles[2] += p_Controls.RotationStep;
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateRight))
            angles[2] -= p_Controls.RotationStep;

        view.Rotation *= f32q{angles};
    }

    const auto rmat = Onyx::Transform<D>::ComputeRotationMatrix(view.Rotation);
    view.Translation += rmat * translation;

    updateProjectionView();
}

void Camera<D3>::SetProjection(const f32m4 &p_Projection)
{
    m_ProjectionView.Projection = p_Projection;
    updateProjectionView();
}
void Camera<D3>::SetPerspectiveProjection(const f32 p_FieldOfView, const f32 p_Near, const f32 p_Far)
{
    f32m4 projection{0.f};
    const f32 invHalfPov = 1.f / Math::Tangent(0.5f * p_FieldOfView);

    projection[0][0] = invHalfPov; // Aspect applied in view
    projection[1][1] = invHalfPov;
    projection[2][2] = p_Far / (p_Far - p_Near);
    projection[2][3] = 1.f;
    projection[3][2] = p_Far * p_Near / (p_Near - p_Far);
    SetProjection(projection);
}

void Camera<D3>::SetOrthographicProjection()
{
    SetProjection(f32m4::Identity());
}
void Camera<D3>::SetOrthographicProjection(const f32 p_Size)
{
    const f32 aspect = m_ProjectionView.View.Scale[0] / m_ProjectionView.View.Scale[0];
    m_ProjectionView.View.Scale[0] = p_Size * aspect;
    m_ProjectionView.View.Scale[1] = p_Size;
    SetProjection(f32m4::Identity());
}

template <Dimension D> void ICamera<D>::ControlMovementWithUserInput(const TKit::Timespan p_DeltaTime)
{
    CameraControls<D> controls{};
    controls.TranslationStep = p_DeltaTime.AsSeconds();
    controls.RotationStep = p_DeltaTime.AsSeconds();
    ControlMovementWithUserInput(controls);
}

template <Dimension D> void ICamera<D>::ControlScrollWithUserInput(const f32 p_ScaleStep)
{
    f32v2 scpos = Input::GetScreenMousePosition(m_Window);
    scpos[1] = -scpos[1]; // Invert y axis to undo onyx's inversion to GLFW, so that now when applying the
    // rotation around x axis everything works out

    if constexpr (D == D2)
    {
        f32m4 transform = Onyx::Transform<D2>::Promote(m_ProjectionView.View.ComputeTransform());
        ApplyCoordinateSystemIntrinsic(transform);
        const f32v2 mpos = transform * f32v4{scpos, 0.f, 1.f};
        const f32v2 dpos = p_ScaleStep * (mpos - m_ProjectionView.View.Translation);
        m_ProjectionView.View.Translation += dpos;
        m_ProjectionView.View.Scale *= 1.f - p_ScaleStep;
    }
    else
    {
        const f32m4 transform = m_ProjectionView.View.ComputeTransform();
        const f32v3 mpos = transform * f32v4{scpos, 0.f, 1.f};
        const f32v3 dpos = p_ScaleStep * (mpos - m_ProjectionView.View.Translation);
        m_ProjectionView.View.Translation += dpos;
        m_ProjectionView.View.Scale *= 1.f - p_ScaleStep;
    }

    updateProjectionView();
}

template <Dimension D> void ICamera<D>::adaptViewToViewportAspect()
{
    const VkExtent2D extent = {m_Window->GetPixelWidth(), m_Window->GetPixelHeight()};
    const VkViewport viewport = m_Viewport.AsVulkanViewport(extent);
    const f32 aspect = viewport.width / viewport.height;

    m_ProjectionView.View.Scale[0] = m_ProjectionView.View.Scale[1] * aspect;
    updateProjectionView();
}
template <Dimension D> void ICamera<D>::updateProjectionView()
{
    if constexpr (D == D2)
        m_ProjectionView.ProjectionView = m_ProjectionView.View.ComputeInverseTransform();
    else
    {
        f32m4 vmat = m_ProjectionView.View.ComputeInverseTransform();
        ApplyCoordinateSystemExtrinsic(vmat);
        m_ProjectionView.ProjectionView = m_ProjectionView.Projection * vmat;
    }
}

template <Dimension D> CameraInfo ICamera<D>::CreateCameraInfo() const
{
    const VkExtent2D extent = {m_Window->GetPixelWidth(), m_Window->GetPixelHeight()};
    CameraInfo info;
    if constexpr (D == D2)
    {
        info.ProjectionView = Transform<D2>::Promote(m_ProjectionView.ProjectionView);
        ApplyCoordinateSystemExtrinsic(info.ProjectionView);
    }
    else
    {
        info.ViewPosition = m_ProjectionView.View.Translation;
        info.ProjectionView = m_ProjectionView.ProjectionView;
    }
    info.BackgroundColor = BackgroundColor;
    info.Transparent = Transparent;
    info.Viewport = m_Viewport.AsVulkanViewport(extent);
    info.Scissor = m_Scissor.AsVulkanScissor(extent, m_Viewport);
    return info;
}

f32v3 Camera<D3>::GetViewLookDirection() const
{
    return Math::Normalize(ScreenToWorld(f32v3{0.f, 0.f, 1.f}));
}
f32v3 Camera<D3>::GetMouseRayCastDirection() const
{
    return Math::Normalize(GetWorldMousePosition(0.25f) - GetWorldMousePosition(0.f));
}

template class ONYX_API Detail::ICamera<D2>;
template class ONYX_API Detail::ICamera<D3>;

} // namespace Onyx
