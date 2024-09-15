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
ONYX_DIMENSION_TEMPLATE class ONYX_API Shape : public Drawable
{
  public:
    Shape(const Model *p_Model) noexcept;
    virtual ~Shape() noexcept = default;

    Color Color;
    Transform<N> Transform;

    virtual void Draw(Window &p_Window) noexcept override;

  protected:
    const Model *m_Model;
};

using Shape2D = Shape<2>;
using Shape3D = Shape<3>;
} // namespace ONYX