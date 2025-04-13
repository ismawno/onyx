#include "onyx/core/pch.hpp"
#include "onyx/rendering/camera.hpp"
#include "onyx/app/window.hpp"

namespace Onyx
{
using namespace Detail;

VkViewport ScreenViewport::AsVulkanViewport(const VkExtent2D &p_Extent) const noexcept
{
    VkViewport viewport;
    viewport.x = 0.5f * (Position.x + 1.f) * p_Extent.width;
    viewport.y = 0.5f * (Position.y + 1.f) * p_Extent.height;
    viewport.width = Size.x * p_Extent.width;
    viewport.height = Size.y * p_Extent.height;
    viewport.minDepth = Position.z;
    viewport.maxDepth = Position.z + Size.z;

    return viewport;
}

VkRect2D ScreenScissor::AsVulkanScissor(const VkExtent2D &p_Extent) const noexcept
{
    VkRect2D scissor;
    scissor.offset.x = static_cast<i32>(0.5f * (Position.x + 1.f) * p_Extent.width);
    scissor.offset.y = static_cast<i32>(0.5f * (Position.y + 1.f) * p_Extent.height);
    scissor.extent.width = static_cast<u32>(Size.x * p_Extent.width);
    scissor.extent.height = static_cast<u32>(Size.y * p_Extent.height);

    return scissor;
}

template <Dimension D> fvec<D> ICamera<D>::ScreenToWorld(const fvec<D> &p_ScreenPos) const noexcept
{
    if constexpr (D == D2)
    {
        const fmat3 itransform3 = glm::inverse(m_ProjectionView.ProjectionView * m_State->Axes);
        fmat4 itransform = Onyx::Transform<D2>::Promote(itransform3);
        ApplyCoordinateSystemIntrinsic(itransform);
        return itransform * fvec4{p_ScreenPos, 0.f, 1.f};
    }
    else
    {
        const fmat4 transform = m_ProjectionView.ProjectionView * m_State->Axes;
        const fvec4 clip = glm::inverse(transform) * fvec4{p_ScreenPos, 1.f};
        return fvec3{clip} / clip.w;
    }
}

template <Dimension D> const ProjectionViewData<D> &ICamera<D>::GetProjectionViewData() const noexcept
{
    return m_ProjectionView;
}
template <Dimension D> const ScreenViewport &ICamera<D>::GetViewport() const noexcept
{
    return m_Viewport;
}
template <Dimension D> const ScreenScissor &ICamera<D>::GetScissor() const noexcept
{
    return m_Scissor;
}

template <Dimension D> Onyx::Transform<D> ICamera<D>::GetViewTransform() const noexcept
{
    const fmat<D> vmat = glm::inverse(m_State->Axes) * m_ProjectionView.View.ComputeTransform();
    return Onyx::Transform<D>::Extract(vmat);
}

template <Dimension D> void ICamera<D>::SetView(const Onyx::Transform<D> &p_View) noexcept
{
    m_ProjectionView.View = p_View;
    updateProjectionView();
}
template <Dimension D> void ICamera<D>::SetViewport(const ScreenViewport &p_Viewport) noexcept
{
    m_Viewport = p_Viewport;
    adaptViewToViewportAspect();
}
template <Dimension D> void ICamera<D>::SetScissor(const ScreenScissor &p_Scissor) noexcept
{
    m_Scissor = p_Scissor;
}

fvec2 Camera<D2>::GetMousePosition() const noexcept
{
    return ScreenToWorld(Input::GetNativeMousePosition(m_Window));
}
fvec3 Camera<D3>::GetMousePosition(const f32 p_Depth) const noexcept
{
    return ScreenToWorld(fvec3{Input::GetNativeMousePosition(m_Window), p_Depth});
}

template <Dimension D> void ICamera<D>::ControlMovementWithUserInput(const CameraControls<D> &p_Controls) noexcept
{
    Onyx::Transform<D> &view = m_ProjectionView.View;
    fvec<D> translation{0.f};
    if (Input::IsKeyPressed(m_Window, p_Controls.Left))
        translation.x -= view.Scale.x * p_Controls.TranslationStep;
    if (Input::IsKeyPressed(m_Window, p_Controls.Right))
        translation.x += view.Scale.x * p_Controls.TranslationStep;

    if (Input::IsKeyPressed(m_Window, p_Controls.Up))
        translation.y += view.Scale.y * p_Controls.TranslationStep;
    if (Input::IsKeyPressed(m_Window, p_Controls.Down))
        translation.y -= view.Scale.y * p_Controls.TranslationStep;

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
            translation.z -= view.Scale.z * p_Controls.TranslationStep;
        if (Input::IsKeyPressed(m_Window, p_Controls.Backward))
            translation.z += view.Scale.z * p_Controls.TranslationStep;

        const fvec2 mpos = Input::GetNativeMousePosition(m_Window);

        const bool lookAround = Input::IsKeyPressed(m_Window, p_Controls.ToggleLookAround);
        const fvec2 delta = lookAround ? 3.f * (m_PrevMousePos - mpos) : fvec2{0.f};
        m_PrevMousePos = mpos;

        fvec3 angles{delta.y, delta.x, 0.f};
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateLeft))
            angles.z += p_Controls.RotationStep;
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateRight))
            angles.z -= p_Controls.RotationStep;

        view.Rotation *= quat{angles};
    }

    const auto rmat = Onyx::Transform<D>::ComputeRotationMatrix(view.Rotation);
    view.Translation += rmat * translation;

    updateProjectionView();
}

void Camera<D3>::SetProjection(const fmat4 &p_Projection) noexcept
{
    m_ProjectionView.Projection = p_Projection;
    updateProjectionView();
}
void Camera<D3>::SetPerspectiveProjection(const f32 p_FieldOfView, const f32 p_Near, const f32 p_Far) noexcept
{
    fmat4 projection{0.f};
    const f32 invHalfPov = 1.f / glm::tan(0.5f * p_FieldOfView);

    projection[0][0] = invHalfPov; // Aspect applied in view
    projection[1][1] = invHalfPov;
    projection[2][2] = p_Far / (p_Far - p_Near);
    projection[2][3] = 1.f;
    projection[3][2] = p_Far * p_Near / (p_Near - p_Far);
    SetProjection(projection);
}

void Camera<D3>::SetOrthographicProjection() noexcept
{
    SetProjection(fmat4{1.f});
}

template <Dimension D> void ICamera<D>::ControlMovementWithUserInput(const TKit::Timespan p_DeltaTime) noexcept
{
    CameraControls<D> controls{};
    controls.TranslationStep = p_DeltaTime.AsSeconds();
    controls.RotationStep = p_DeltaTime.AsSeconds();
    ControlMovementWithUserInput(controls);
}

template <Dimension D> void ICamera<D>::adaptViewToViewportAspect() noexcept
{
    const VkExtent2D extent = {m_Window->GetPixelWidth(), m_Window->GetPixelHeight()};
    const VkViewport viewport = m_Viewport.AsVulkanViewport(extent);
    const f32 aspect = viewport.width / viewport.height;

    m_ProjectionView.View.Scale.x = m_ProjectionView.View.Scale.y * aspect;
    updateProjectionView();
}
template <Dimension D> void ICamera<D>::updateProjectionView() noexcept
{
    if constexpr (D == D2)
        m_ProjectionView.ProjectionView = m_ProjectionView.View.ComputeInverseTransform();
    else
    {
        fmat4 vmat = m_ProjectionView.View.ComputeInverseTransform();
        ApplyCoordinateSystemExtrinsic(vmat);
        m_ProjectionView.ProjectionView = m_ProjectionView.Projection * vmat;
    }
}

template <Dimension D> CameraInfo ICamera<D>::CreateCameraInfo() const noexcept
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
    info.Viewport = m_Viewport.AsVulkanViewport(extent);
    info.Scissor = m_Scissor.AsVulkanScissor(extent);
    return info;
}

void Camera<D2>::ControlScrollWithUserInput(const f32 p_ScaleStep) noexcept
{
    fmat4 transform = Onyx::Transform<D2>::Promote(m_ProjectionView.View.ComputeTransform());
    ApplyCoordinateSystemIntrinsic(transform);
    const fvec2 mpos = transform * fvec4{Input::GetNativeMousePosition(m_Window), 0.f, 1.f};

    const fvec2 dpos = p_ScaleStep * (mpos - m_ProjectionView.View.Translation);
    m_ProjectionView.View.Translation += dpos;
    m_ProjectionView.View.Scale *= 1.f - p_ScaleStep;

    updateProjectionView();
}

fvec3 Camera<D3>::GetViewLookDirection() const noexcept
{
    return glm::normalize(ScreenToWorld(fvec3{0.f, 0.f, 1.f}));
}
fvec3 Camera<D3>::GetMouseRayCastDirection() const noexcept
{
    return glm::normalize(GetMousePosition(0.25f) - GetMousePosition(0.f));
}

template class ONYX_API Detail::ICamera<D2>;
template class ONYX_API Detail::ICamera<D3>;

} // namespace Onyx