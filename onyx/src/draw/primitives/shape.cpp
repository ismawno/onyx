#include "core/pch.hpp"
#include "onyx/draw/primitives/shape.hpp"
#include "onyx/app/window.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Shape<N>::Shape(const Model *p_Model) noexcept : m_Model(p_Model)
{
}

ONYX_DIMENSION_TEMPLATE void Shape<N>::Draw(Window &p_Window) noexcept
{
    DefaultDraw(p_Window.GetRenderSystem(0), m_Model, Color, Transform.ModelTransform());
}

template class Shape<2>;
template class Shape<3>;
} // namespace ONYX