#pragma once

#include "onyx/draw/primitives/shape.hpp"

namespace ONYX
{
class ONYX_API Ellipsoid3D final : public ModelShape3D
{
  public:
    Ellipsoid3D(f32 p_Radius1, f32 p_Radius2, f32 p_Radius3, const Color &p_Color = Color::WHITE) noexcept;
    Ellipsoid3D(f32 p_Radius, const Color &p_Color = Color::WHITE) noexcept;
    Ellipsoid3D(const Color &p_Color) noexcept;

    f32 GetRadius1() const noexcept;
    f32 GetRadius2() const noexcept;
    f32 GetRadius3() const noexcept;

    f32 GetRadius() const noexcept;

    void SetRadius1(f32 p_Radius1) noexcept;
    void SetRadius2(f32 p_Radius2) noexcept;
    void SetRadius3(f32 p_Radius2) noexcept;

    void SetRadius(f32 p_Radius) noexcept;
};

} // namespace ONYX