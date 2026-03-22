#pragma once

#include "onyx/property/camera.hpp"

namespace Onyx
{
/**
 * These options would actually describe an arc, as lower and upper angles can be specified. To draw
 * a full circle, set the lower angle to 0 and the upper angle to 2 * PI (or any combination that results in `Upper`
 * - `Lower` == 2 * PI).`
 *
 * Nothing will be drawn if `LowerAngle` == `UpperAngle` or if `Hollowness` approaches 1.
 *
 * This method does not record any vulkan commands.
 *
 * `InnerFade` A value between 0 and 1, denoting how much the circle fades from the center to the
 * edge.
 * `OuterFade` A value between 0 and 1, denoting how much the circle fades from the edge to the
 * center.
 * `Hollowness` A value between 0 and 1, denoting how hollow the circle is. 0 is a full circle and
 * 1 would correspond to not having a circle at all.
 * `LowerAngle` The angle from which the arc starts.
 * `UpperAngle` The angle at which the arc ends.
 */
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
} // namespace Onyx
