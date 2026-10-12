#pragma once

#include "graph/input.hpp"
#include "onyx/transform.hpp"

namespace Onyx
{
template <Dimension D> struct OrthographicParameters
{
    TKIT_REFLECT_DECLARE(OrthographicParameters)
    TKIT_YAML_SERIALIZE_DECLARE(OrthographicParameters)
    f32 Size = 5.f;
};

template <> struct OrthographicParameters<D3>
{
    TKIT_REFLECT_DECLARE(OrthographicParameters)
    TKIT_YAML_SERIALIZE_DECLARE(OrthographicParameters)
    f32 Size = 5.f;
    f32 Near = -5.f;
    f32 Far = 5.f;
};

struct PerspectiveParameters
{
    TKIT_REFLECT_DECLARE(PerspectiveParameters)
    TKIT_YAML_SERIALIZE_DECLARE(PerspectiveParameters)
    f32 FieldOfView = Math::Radians(75.f);
    f32 Near = 0.1f;
    f32 Far = 100.f;
};

class Window;

template <Dimension D> struct CameraControls;

template <> struct CameraControls<D2>
{
    Graph::Key Up = Graph::Key_W;
    Graph::Key Down = Graph::Key_S;
    Graph::Key Left = Graph::Key_A;
    Graph::Key Right = Graph::Key_D;
    Graph::Key RotateLeft = Graph::Key_Q;
    Graph::Key RotateRight = Graph::Key_E;
};
template <> struct CameraControls<D3>
{
    Graph::Key Forward = Graph::Key_W;
    Graph::Key Backward = Graph::Key_S;
    Graph::Key Left = Graph::Key_A;
    Graph::Key Right = Graph::Key_D;
    Graph::Key Up = Graph::Key_Space;
    Graph::Key Down = Graph::Key_LeftControl;
    Graph::Key RotateLeft = Graph::Key_Q;
    Graph::Key RotateRight = Graph::Key_E;
    Graph::Key ToggleLookAround = Graph::Key_LeftShift;
};

} // namespace Onyx

namespace Onyx
{

TKIT_REFLECT_DECLARE_ENUM(CameraMode)
TKIT_YAML_SERIALIZE_DECLARE_ENUM(CameraMode)

enum CameraMode : u8
{
    CameraMode_Orthographic,
    CameraMode_Viewport,
    CameraMode_Perspective,
};

template <Dimension D> struct Camera
{
    TKIT_REFLECT_DECLARE(Camera)
    TKIT_YAML_SERIALIZE_DECLARE(Camera)
    Transform<D> View{};
    OrthographicParameters<D> OrthoParameters{};
    CameraMode Mode = CameraMode_Orthographic;
};

template <> struct Camera<D3>
{
    TKIT_REFLECT_DECLARE(Camera)
    TKIT_YAML_SERIALIZE_DECLARE(Camera)
    Transform<D3> View{};
    OrthographicParameters<D3> OrthoParameters{};
    PerspectiveParameters PerspParameters{};
    CameraMode Mode = CameraMode_Perspective;
};
} // namespace Onyx
