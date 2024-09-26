#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/draw/drawable.hpp"

namespace ONYX
{
class Model;
ONYX_DIMENSION_TEMPLATE class ILine ONYX_API : public IDrawable
{
  public:
    virtual ~ILine() noexcept = default;

    virtual const vec<N> &GetP1() const noexcept = 0;
    virtual const vec<N> &GetP2() const noexcept = 0;

    virtual void SetP1(const vec<N> &p_Start) noexcept = 0;
    virtual void SetP2(const vec<N> &p_End) noexcept = 0;

    virtual const Color &GetColor() const noexcept = 0;
    virtual void SetColor(const Color &p_Color) noexcept = 0;
};

using Line2D = ILine<2>;
using Line3D = ILine<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API Line final : public ILine<N>
{
  public:
    Line(const vec<N> &p_Start = vec<N>{-1.f}, const vec<N> &p_End = vec<N>{1.f},
         const Color &p_Color = Color::WHITE) noexcept;
    Line(const Color &p_Color) noexcept;

    const vec<N> &GetP1() const noexcept override;
    const vec<N> &GetP2() const noexcept override;

    void SetP1(const vec<N> &p_Start) noexcept override;
    void SetP2(const vec<N> &p_End) noexcept override;

    const Color &GetColor() const noexcept override;
    void SetColor(const Color &p_Color) noexcept override;

    void Draw(Window &p_Window) noexcept override;

  private:
    void adaptTransform() noexcept;

    const Model *m_Model;
    Color m_Color;
    Transform<N> m_Transform;

    vec<N> m_P1;
    vec<N> m_P2;
};

using ThinLine2D = Line<2>;
using ThinLine3D = Line<3>;
} // namespace ONYX
