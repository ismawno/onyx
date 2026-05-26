#pragma once

#include "onyx/input.hpp"
#include "onyx/transform.hpp"

namespace Onyx
{
template <Dimension D> struct OrthographicParameters
{
    f32 Size = 5.f;
};

template <> struct OrthographicParameters<D3>
{
    f32 Size = 5.f;
    f32 Near = -5.f;
    f32 Far = 5.f;
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
    CameraMode_Viewport,
    CameraMode_Perspective,
};

template <Dimension D> struct Camera
{
    Transform<D> View{};
    OrthographicParameters<D> OrthoParameters{};
    CameraMode Mode = CameraMode_Orthographic;
};

template <> struct Camera<D3>
{
    Transform<D3> View{};
    OrthographicParameters<D3> OrthoParameters{};
    PerspectiveParameters PerspParameters{};
    CameraMode Mode = CameraMode_Perspective;
};
} // namespace Onyx
