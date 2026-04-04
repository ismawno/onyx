#pragma once

#include "onyx/property/camera.hpp"

namespace Onyx
{
enum Alignment : u8
{
    Alignment_Left,
    Alignment_Center,
    Alignment_Right,
    Alignment_Bottom = Alignment_Left,
    Alignment_Top = Alignment_Right,
    Alignment_Near = Alignment_Left,
    Alignment_Far = Alignment_Right,
    Alignment_None = 3,
};

struct CircleParameters
{
    TKIT_REFLECT_DECLARE(CircleParameters)
    TKIT_YAML_SERIALIZE_DECLARE(CircleParameters)
    f32 InnerFade = 0.f;
    f32 OuterFade = 0.f;
    f32 Hollowness = 0.f;
    f32 LowerAngle = 0.f;
    f32 UpperAngle = 2.f * Math::Pi<f32>();
};

struct CameraParameters
{
    TKIT_REFLECT_DECLARE(CameraParameters)
    TKIT_YAML_SERIALIZE_DECLARE(CameraParameters)
    ScreenViewport Viewport{};
    ScreenScissor Scissor{};
};

struct AxesParameters
{
    TKIT_REFLECT_DECLARE(AxesParameters)
    TKIT_YAML_SERIALIZE_DECLARE(AxesParameters)
    f32 Thickness = 0.1f;
    f32 Size = 50.f;
};

struct TextParameters
{
    TKIT_REFLECT_DECLARE(TextParameters)
    TKIT_YAML_SERIALIZE_DECLARE(TextParameters)

    f32 Kerning = 0.f;
    f32 LineSpacing = 0.f;
    f32 Width = 12.f;
};
} // namespace Onyx
