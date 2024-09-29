#include "core/pch.hpp"
#include "onyx/draw/primitives/triangle.hpp"
#include "onyx/draw/model.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Triangle<N>::Triangle(const vec<N> &p_Position, const Color &p_Color) noexcept
    : ModelShape<N>(Model::GetTriangle<N>(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
    this->Transform.Position = p_Position;
}

ONYX_DIMENSION_TEMPLATE Triangle<N>::Triangle(const Color &p_Color) noexcept
    : ModelShape<N>(Model::GetTriangle<N>(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
}

template class Triangle<2>;
template class Triangle<3>;
} // namespace ONYX