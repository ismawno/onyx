#include "core/pch.hpp"
#include "onyx/draw/primitives/polygon.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/app/window.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE RegularPolygon<N>::RegularPolygon(u32 p_Sides, const vec<N> &p_Position,
                                                          const Color &p_Color) noexcept
    : ModelShape<N>(Model::GetRegularPolygon<N>(p_Sides), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
    this->Transform.Position = p_Position;
}

ONYX_DIMENSION_TEMPLATE RegularPolygon<N>::RegularPolygon(u32 p_Sides, const Color &p_Color) noexcept
    : ModelShape<N>(Model::GetRegularPolygon<N>(p_Sides), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
}

ONYX_DIMENSION_TEMPLATE Polygon<N>::Polygon(const Model *p_Model, const Color &p_Color) noexcept
    : ModelShape<N>(p_Model, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
}
ONYX_DIMENSION_TEMPLATE Polygon<N>::Polygon(const std::span<const vec<N>> p_Vertices, const Color &p_Color) noexcept
    : ModelShape<N>(Model::CreatePolygon<N>(p_Vertices), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
}

ONYX_DIMENSION_TEMPLATE MutablePolygon<N>::MutablePolygon(Model *p_Model, const Color &p_Color) noexcept
    : m_Model(p_Model), m_Color(p_Color)
{
    KIT_ASSERT(p_Model->IsMutable(), "Model must be mutable");
}
ONYX_DIMENSION_TEMPLATE MutablePolygon<N>::MutablePolygon(const std::span<const vec<N>> p_Vertices,
                                                          const Color &p_Color) noexcept
    : m_Model(Model::CreatePolygon<N>(p_Vertices), Model::Properties::HOST_VISIBLE), m_Color(p_Color)
{
}

ONYX_DIMENSION_TEMPLATE const Color &MutablePolygon<N>::GetColor() const noexcept
{
    return m_Color;
}
ONYX_DIMENSION_TEMPLATE void MutablePolygon<N>::SetColor(const Color &p_Color) noexcept
{
    m_Color = p_Color;
}

ONYX_DIMENSION_TEMPLATE void MutablePolygon<N>::Draw(Window &p_Window) noexcept
{
    RenderSystem<N> *rs = p_Window.GetRenderSystem<N>(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    this->Transform.UpdateMatricesAsModelIfNeeded();
    IShape<N>::DefaultModelDraw(*rs, m_Model, m_Color, this->Transform.GetGlobalTransform());
}

ONYX_DIMENSION_TEMPLATE const vec<N> &MutablePolygon<N>::operator[](const usize p_Index) const noexcept
{
    return GetVertex(p_Index);
}
ONYX_DIMENSION_TEMPLATE const vec<N> &MutablePolygon<N>::GetVertex(const usize p_Index) const noexcept
{
    return *static_cast<const vec<N> *>(m_Model->GetVertexBuffer().ReadAt(p_Index));
}

ONYX_DIMENSION_TEMPLATE void MutablePolygon<N>::SetVertex(const usize p_Index, const vec<N> &p_Vertex) noexcept
{
    m_Model->GetVertexBuffer().WriteAt(p_Index, &p_Vertex);
    if (m_Model->MustFlush())
        m_Model->GetVertexBuffer().FlushAt(p_Index);
}

template class RegularPolygon<2>;
template class RegularPolygon<3>;
} // namespace ONYX