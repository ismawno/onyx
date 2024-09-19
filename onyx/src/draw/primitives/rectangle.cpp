#include "core/pch.hpp"
#include "onyx/draw/primitives/rectangle.hpp"
#include "onyx/draw/model.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Rectangle<N>::Rectangle(const vec<N> &p_Position, const vec2 &p_Dimension, const Color &p_Color)
    : Shape<N>(Model::GetRectangle<N>(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
{
    this->Transform.Position = p_Position;
    if constexpr (N == 2)
        this->Transform.Scale = p_Dimension;
    else
        this->Transform.Scale = vec3(p_Dimension, 1.f);
    this->Color = p_Color;
}

ONYX_DIMENSION_TEMPLATE Rectangle<N>::Rectangle(const Color &p_Color)
    : Shape<N>(Model::GetRectangle<N>(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
{
    this->Color = p_Color;
}

template class Rectangle<2>;
template class Rectangle<3>;
} // namespace ONYX