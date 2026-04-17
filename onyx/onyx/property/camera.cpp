#include "onyx/core/pch.hpp"
#include "onyx/property/camera.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/rendering/view.hpp"

namespace Onyx
{
template <Dimension D> void CameraController<D>::ControlMovement(const f32 translationStep, const f32 rotationStep)
{
    Onyx::Transform<D> &view = Camera->View;
    f32v<D> translation{0.f};
    if (m_Window->IsKeyPressed(Controls.Left))
        translation[0] -= view.Scale[0] * translationStep;
    if (m_Window->IsKeyPressed(Controls.Right))
        translation[0] += view.Scale[0] * translationStep;

    if (m_Window->IsKeyPressed(Controls.Up))
        translation[1] += view.Scale[1] * translationStep;
    if (m_Window->IsKeyPressed(Controls.Down))
        translation[1] -= view.Scale[1] * translationStep;

    if constexpr (D == D2)
    {
        if (m_Window->IsKeyPressed(Controls.RotateLeft))
            view.Rotation += rotationStep;
        if (m_Window->IsKeyPressed(Controls.RotateRight))
            view.Rotation -= rotationStep;
    }
    else
    {
        if (m_Window->IsKeyPressed(Controls.Forward))
            translation[2] -= view.Scale[2] * translationStep;
        if (m_Window->IsKeyPressed(Controls.Backward))
            translation[2] += view.Scale[2] * translationStep;

        f32v2 mpos = m_Window->GetScreenMousePosition();
        mpos[1] = -mpos[1]; // Invert y axis to undo onyx's inversion to GLFW, so that now when applying the
                            // rotation around x axis everything works out

        const bool lookAround = m_Window->IsKeyPressed(Controls.ToggleLookAround);
        const f32v2 delta = lookAround ? 3.f * (m_PrevMousePos - mpos) : f32v2{0.f};
        m_PrevMousePos = mpos;

        f32v3 angles{delta[1], delta[0], 0.f};
        if (m_Window->IsKeyPressed(Controls.RotateLeft))
            angles[2] += rotationStep;
        if (m_Window->IsKeyPressed(Controls.RotateRight))
            angles[2] -= rotationStep;

        view.Rotation *= f32q{angles};
    }

    const auto rmat = Onyx::Transform<D>::ComputeRotationMatrix(view.Rotation);
    view.Translation += rmat * translation;
}

// sloppy design
template <Dimension D> void CameraController<D>::ControlScroll(const f32 scaleStep, RenderView<D> *rview)
{
    TKIT_ASSERT(!rview || rview->GetCamera() == Camera,
                "[ONYX][CONTROLLER] If passing a render view, its camera must be the same as the controller's");
    const f32 factor = 1.f - scaleStep;
    if constexpr (D == D3)
    {
        if (Camera->Mode == CameraMode_Orthographic)
            Camera->OrthoParameters.Size *= factor;
        else
            Camera->PerspParameters.FieldOfView *= factor;
    }
    else
    {
        Camera->OrthoParameters.Size *= factor;
        if (rview)
        {
            const f32v2 scpos = m_Window->GetScreenMousePosition();
            const f32v2 wb = rview->ScreenToWorld(scpos);
            rview->CacheProjectionView();
            const f32v2 wa = rview->ScreenToWorld(scpos);

            Camera->View.Translation += wb - wa;
        }
    }
}

template class CameraController<D2>;
template class CameraController<D3>;
} // namespace Onyx
