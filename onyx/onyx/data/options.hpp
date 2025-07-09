#pragma once

#include "onyx/object/primitives.hpp"
#include "onyx/rendering/camera.hpp"

namespace Onyx
{
struct CircleOptions
{
    f32 InnerFade = 0.f;
    f32 OuterFade = 0.f;
    f32 Hollowness = 0.f;
    f32 LowerAngle = 0.f;
    f32 UpperAngle = glm::two_pi<f32>();
};
template <Dimension D> struct AxesOptions;

template <> struct ONYX_API AxesOptions<D2>
{
    f32 Thickness = 0.01f;
    f32 Size = 50.f;
};
template <> struct ONYX_API AxesOptions<D3>
{
    f32 Thickness = 0.01f;
    f32 Size = 50.f;
    Onyx::Resolution Resolution = Onyx::Resolution::Medium;
};

struct ONYX_API LineOptions
{
    f32 Thickness = 0.01f;
    Onyx::Resolution Resolution = Onyx::Resolution::Medium;
};

struct ONYX_API CameraOptions
{
    ScreenViewport Viewport{};
    ScreenScissor Scissor{};
};
} // namespace Onyx
