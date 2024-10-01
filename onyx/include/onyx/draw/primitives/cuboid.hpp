#pragma once

#include "onyx/draw/primitives/shape.hpp"

namespace ONYX
{
class ONYX_API Cuboid3D final : public ModelShape3D
{
  public:
    Cuboid3D(const vec3 &p_Position = vec3(0.f), const vec3 &p_Dimension = {1.f, 1.f, 1.f},
             const Color &p_Color = Color::WHITE) noexcept;
    Cuboid3D(const Color &p_Color) noexcept;
};
} // namespace ONYX