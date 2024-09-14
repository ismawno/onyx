#include "core/pch.hpp"
#include "onyx/draw/primitives/rectangle.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Rectangle<N>::Rectangle(const vec<N> &p_Position, const vec2 &p_Dimension, const Color &p_Color)
    : Shape<N>(Model::Rectangle(p_Dimension))
{
    this->Transform.Position = p_Position;
    this->Transform.Scale = p_Dimension;
    this->Color = p_Color;
}

template class Rectangle<2>;
template class Rectangle<3>;
} // namespace ONYX