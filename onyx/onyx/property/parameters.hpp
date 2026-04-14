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

template <Dimension D> struct PointLightData;
struct DirectionalLightData;

template <Dimension D> struct PointLightParameters
{
    using InstanceData = PointLightData<D>;
    f32v<D> Position = f32v<D>{0.f};
    f32 Radius = 1.f;
    f32 Intensity = 0.8f;
    Color Tint = Color::White;
};

template <> struct PointLightParameters<D3>
{
    using InstanceData = PointLightData<D3>;
    f32v3 Position = f32v3{0.f};
    Color Tint = Color::White;
    f32 Radius = 1.f;
    f32 Intensity = 0.8f;
    f32 DepthBias = 0.01f;
};

struct DirectionalLightParameters
{
    using InstanceData = DirectionalLightData;
    f32v3 Position = f32v3{5.f};
    f32v3 Direction = f32v3{1.f};
    Color Tint = Color::White;
    f32 Range = 20.f;
    f32 Depth = 200.f;
    f32 Intensity = 0.8f;
    f32 DepthBias = 0.01f;
};

struct TextParameters
{
    TKIT_REFLECT_DECLARE(TextParameters)
    TKIT_YAML_SERIALIZE_DECLARE(TextParameters)

    std::function<void(u32, char, f32v2 &)> *CharacterCallback = nullptr;
    f32 Kerning = 0.f;
    f32 LineSpacing = 0.f;
    f32 Width = 12.f;
};
} // namespace Onyx
