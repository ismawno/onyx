#pragma once

#include "onyx/draw/primitives/shape.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class ONYX_API Triangle final : public ModelShape<N>
{
  public:
    Triangle(const vec<N> &p_Position = vec<N>(0.f), const Color &p_Color = Color::WHITE) noexcept;
    Triangle(const Color &p_Color) noexcept;
};

using Triangle2D = Triangle<2>;
using Triangle3D = Triangle<3>;
} // namespace ONYX