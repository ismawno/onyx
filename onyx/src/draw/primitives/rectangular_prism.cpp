#include "core/pch.hpp"
#include "onyx/draw/primitives/rectangular_prism.hpp"
#include "onyx/draw/model.hpp"

namespace ONYX
{
RectangularPrism3D::RectangularPrism3D(const vec3 &p_Position, const vec3 &p_Dimension, const Color &p_Color) noexcept
    : ModelShape(Model::GetRectangularPrism(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
    this->Transform.SetPosition(p_Position);
    this->Transform.SetScale(p_Dimension);
}

RectangularPrism3D::RectangularPrism3D(const Color &p_Color) noexcept
    : ModelShape(Model::GetRectangularPrism(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
}
} // namespace ONYX