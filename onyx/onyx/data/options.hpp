#pragma once

#include "onyx/object/primitives.hpp"
#include "onyx/rendering/camera.hpp"

namespace Onyx
{
struct CircleOptions
{
    TKIT_REFLECT_DECLARE(CircleOptions)
    TKIT_YAML_SERIALIZE_DECLARE(CircleOptions)
    f32 InnerFade = 0.f;
    f32 OuterFade = 0.f;
    f32 Hollowness = 0.f;
    f32 LowerAngle = 0.f;
    f32 UpperAngle = 2.f * Math::Pi<f32>();
};
template <Dimension D> struct AxesOptions;

template <> struct ONYX_API AxesOptions<D2>
{
    TKIT_REFLECT_DECLARE(AxesOptions)
    TKIT_YAML_SERIALIZE_DECLARE(AxesOptions)
    f32 Thickness = 0.01f;
    f32 Size = 50.f;
};
template <> struct ONYX_API AxesOptions<D3>
{
    TKIT_REFLECT_DECLARE(AxesOptions)
    TKIT_YAML_SERIALIZE_DECLARE(AxesOptions)
    f32 Thickness = 0.01f;
    f32 Size = 50.f;
    Onyx::Resolution Resolution = Onyx::Resolution::Medium;
};

struct ONYX_API LineOptions
{
    TKIT_REFLECT_DECLARE(LineOptions)
    TKIT_YAML_SERIALIZE_DECLARE(LineOptions)
    f32 Thickness = 0.01f;
    Onyx::Resolution Resolution = Onyx::Resolution::Medium;
};

struct ONYX_API CameraOptions
{
    TKIT_REFLECT_DECLARE(CameraOptions)
    TKIT_YAML_SERIALIZE_DECLARE(CameraOptions)
    ScreenViewport Viewport{};
    ScreenScissor Scissor{};
};
} // namespace Onyx
