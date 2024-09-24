#pragma once

#include "onyx/draw/primitives/shape.hpp"
#include <span>

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class ONYX_API RegularPolygon final : public ModelShape<N>
{
  public:
    RegularPolygon(u32 p_Sides, const vec<N> &p_Position = vec<N>(0.f), const Color &p_Color = Color::WHITE) noexcept;
    RegularPolygon(u32 p_Sides, const Color &p_Color) noexcept;
};

using RegularPolygon2D = RegularPolygon<2>;
using RegularPolygon3D = RegularPolygon<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API Polygon final : public ModelShape<N>
{
  public:
    Polygon(const Model *p_Model, const Color &p_Color = Color::WHITE) noexcept;
    Polygon(std::span<const vec<N>> p_Vertices, const Color &p_Color = Color::WHITE) noexcept;
};

using Polygon2D = Polygon<2>;
using Polygon3D = Polygon<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API MutablePolygon final : public IShape<N>
{
  public:
    MutablePolygon(Model *p_Model, const Color &p_Color = Color::WHITE) noexcept;
    MutablePolygon(std::span<const vec<N>> p_Vertices, const Color &p_Color = Color::WHITE) noexcept;

    const Color &GetColor() const noexcept override;
    void SetColor(const Color &p_Color) noexcept override;

    void Draw(Window &p_Window) noexcept override;

    const vec<N> &operator[](usize p_Index) const noexcept;
    const vec<N> &GetVertex(usize p_Index) const noexcept;

    void SetVertex(usize p_Index, const vec<N> &p_Vertex) noexcept;

  private:
    Model *m_Model;
    Color m_Color;
};
} // namespace ONYX