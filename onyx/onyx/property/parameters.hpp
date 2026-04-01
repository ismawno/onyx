#pragma once

#include "onyx/property/camera.hpp"

namespace Onyx
{
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

enum TextAlignment : u8
{
    TextAlignment_Center,
    TextAlignment_Left,
    TextAlignment_Right,
};

struct TextParameters
{
    TKIT_REFLECT_DECLARE(TextParameters)
    TKIT_YAML_SERIALIZE_DECLARE(TextParameters)

    f32 Kerning = 0.f;
    f32 LineSpacing = 0.f;
    f32 Width = 4.f;
    TextAlignment Alignment = TextAlignment_Left;
};
} // namespace Onyx
