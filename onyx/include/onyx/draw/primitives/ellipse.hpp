#pragma once

#include "onyx/draw/primitives/shape.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class ONYX_API Ellipse final : public ModelShape<N>
{
  public:
    Ellipse(f32 m_Radius1, f32 m_Radius2, const Color &p_Color = Color::WHITE) noexcept;
    Ellipse(f32 m_Radius, const Color &p_Color = Color::WHITE) noexcept;
    Ellipse(const Color &p_Color) noexcept;

    f32 GetRadius1() const noexcept;
    f32 GetRadius2() const noexcept;

    f32 GetRadius() const noexcept;

    void SetRadius1(f32 m_Radius1) noexcept;
    void SetRadius2(f32 m_Radius2) noexcept;

    void SetRadius(f32 m_Radius) noexcept;
};

using Ellipse2D = Ellipse<2>;
using Ellipse3D = Ellipse<3>;
} // namespace ONYX