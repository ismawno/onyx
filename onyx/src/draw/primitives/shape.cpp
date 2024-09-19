#include "core/pch.hpp"
#include "onyx/draw/primitives/shape.hpp"
#include "onyx/app/window.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Shape<N>::Shape(const Model *p_Model, const VkPrimitiveTopology p_Topology) noexcept
    : m_Model(p_Model), m_Topology(p_Topology)
{
}

ONYX_DIMENSION_TEMPLATE void Shape<N>::Draw(Window &p_Window) noexcept
{
    RenderSystem *rs = p_Window.GetRenderSystem<N>(m_Topology);
    DefaultDraw(*rs, m_Model, Color, Transform.ModelTransform());
}

template class Shape<2>;
template class Shape<3>;
} // namespace ONYX