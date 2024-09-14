#pragma once

#include "onyx/draw/primitives/shape.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class ONYX_API Rectangle final : public Shape<N>
{
  public:
    Rectangle(const vec<N> &p_Position = vec<N>(0.f), const vec2 &p_Dimension = {1.f, 1.f},
              const Color &p_Color = Color::WHITE);
    Rectangle(const Color &p_Color);
};

using Rectangle2D = Rectangle<2>;
using Rectangle3D = Rectangle<3>;
} // namespace ONYX