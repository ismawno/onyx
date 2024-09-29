#include "core/pch.hpp"
#include "onyx/draw/primitives/line.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/app/window.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Line<N>::Line(const vec<N> &p_Start, const vec<N> &p_End, const Color &p_Color) noexcept
    : m_Model(Model::GetLine<N>()), m_Color(p_Color), m_P1(p_Start), m_P2(p_End)
{
    adaptTransform();
}

ONYX_DIMENSION_TEMPLATE Line<N>::Line(const Color &p_Color) noexcept : Line(vec<N>{-1.f}, vec<N>{1.f}, p_Color)
{
}

ONYX_DIMENSION_TEMPLATE const vec<N> &Line<N>::GetP1() const noexcept
{
    return m_P1;
}
ONYX_DIMENSION_TEMPLATE const vec<N> &Line<N>::GetP2() const noexcept
{
    return m_P2;
}

ONYX_DIMENSION_TEMPLATE void Line<N>::SetP1(const vec<N> &p_Start) noexcept
{
    m_P1 = p_Start;
    adaptTransform();
}
ONYX_DIMENSION_TEMPLATE void Line<N>::SetP2(const vec<N> &p_End) noexcept
{
    m_P2 = p_End;
    adaptTransform();
}

ONYX_DIMENSION_TEMPLATE const Color &Line<N>::GetColor() const noexcept
{
    return m_Color;
}
ONYX_DIMENSION_TEMPLATE void Line<N>::SetColor(const Color &p_Color) noexcept
{
    m_Color = p_Color;
}

ONYX_DIMENSION_TEMPLATE void Line<N>::Draw(Window &p_Window) noexcept
{
    RenderSystem *rs = p_Window.GetRenderSystem<N>(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    ILine<N>::DefaultDraw(*rs, m_Model, m_Color, m_Transform.ComputeModelTransform());
}

ONYX_DIMENSION_TEMPLATE void Line<N>::adaptTransform() noexcept
{
    m_Transform.Position = 0.5f * (m_P1 + m_P2);

    const vec<N> dir = m_P2 - m_P1;
    if constexpr (N == 2)
        m_Transform.Rotation = glm::atan(dir.y, dir.x);
    else // TODO: Check if this is correct
        m_Transform.Rotation = quat{{0.f, glm::atan(dir.y, dir.x), glm::atan(dir.z, dir.x)}};
    m_Transform.Scale.x = glm::length(dir);
}

template class Line<2>;
template class Line<3>;
} // namespace ONYX