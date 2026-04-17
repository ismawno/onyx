#pragma once
// TODO(Isma): Move file to rendering

#include "onyx/platform/input.hpp"
#include "onyx/property/transform.hpp"
#include "tkit/profiling/timespan.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
template <Dimension D> struct OrthographicParameters
{
    f32 Size = 1.f;
};

template <> struct OrthographicParameters<D3>
{
    f32 Size = 1.f;
    f32 Near = 0.f;
    f32 Far = 1.f;
};

struct PerspectiveParameters
{
    f32 FieldOfView = Math::Radians(75.f);
    f32 Near = 0.1f;
    f32 Far = 100.f;
};

class Window;

template <Dimension D> struct CameraControls;

template <> struct CameraControls<D2>
{
    Key Up = Key_W;
    Key Down = Key_S;
    Key Left = Key_A;
    Key Right = Key_D;
    Key RotateLeft = Key_Q;
    Key RotateRight = Key_E;
};
template <> struct CameraControls<D3>
{
    Key Forward = Key_W;
    Key Backward = Key_S;
    Key Left = Key_A;
    Key Right = Key_D;
    Key Up = Key_Space;
    Key Down = Key_LeftControl;
    Key RotateLeft = Key_Q;
    Key RotateRight = Key_E;
    Key ToggleLookAround = Key_LeftShift;
};

} // namespace Onyx

namespace Onyx
{
enum CameraMode : u8
{
    CameraMode_Orthographic,
    CameraMode_Perspective
};

template <Dimension D> struct Camera
{
    Transform<D> View{};
    OrthographicParameters<D> OrthoParameters{};
};

template <> struct Camera<D3>
{
    Transform<D3> View{};
    OrthographicParameters<D3> OrthoParameters{};
    PerspectiveParameters PerspParameters{};
    CameraMode Mode = CameraMode_Perspective;
};

template <Dimension D> class RenderView;

template <Dimension D> class CameraController
{
  public:
    CameraController(const Window *window, Camera<D> *camera, const CameraControls<D> &controls = {})
        : Camera(camera), Controls(controls), m_Window(window)
    {
    }

    void ControlMovement(f32 translationStep, f32 rotationStep);
    void ControlMovement(const TKit::Timespan deltaTime)
    {
        const f32 step = deltaTime.AsSeconds();
        ControlMovement(step, step);
    }
    void ControlScroll(f32 scaleStep, RenderView<D> *rview = nullptr);

    Camera<D> *Camera;
    CameraControls<D> Controls;

    const Window *GetWindow() const
    {
        return m_Window;
    }
    void SetWindow(const Window *window)
    {
        m_Window = window;
        m_PrevMousePos = f32v2{0.f};
    }

  private:
    const Window *m_Window;
    f32v2 m_PrevMousePos{0.f};
};

} // namespace Onyx
