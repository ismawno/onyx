#include "core/pch.hpp"
#include "onyx/draw/primitives/shape.hpp"
#include "onyx/app/window.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE ModelShape<N>::ModelShape(const Model *p_Model, const VkPrimitiveTopology p_Topology,
                                                  const ONYX::Color &p_Color) noexcept
    : m_Model(p_Model), m_Topology(p_Topology), m_Color(p_Color)
{
}

ONYX_DIMENSION_TEMPLATE const ONYX::Color &ModelShape<N>::GetColor() const noexcept
{
    return m_Color;
}

ONYX_DIMENSION_TEMPLATE void ModelShape<N>::SetColor(const ONYX::Color &p_Color) noexcept
{
    m_Color = p_Color;
}

ONYX_DIMENSION_TEMPLATE void ModelShape<N>::Draw(Window &p_Window) noexcept
{
    RenderSystem *rs = p_Window.GetRenderSystem<N>(m_Topology);
    IShape<N>::DefaultDraw(*rs, m_Model, GetColor(), this->Transform.ComputeModelTransform());
}

template class ModelShape<2>;
template class ModelShape<3>;
} // namespace ONYX