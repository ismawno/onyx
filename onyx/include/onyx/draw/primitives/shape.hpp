#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/draw/drawable.hpp"

#include <vulkan/vulkan.h>

namespace ONYX
{
class Model;
ONYX_DIMENSION_TEMPLATE class ONYX_API IShape : public IDrawable
{
  public:
    virtual ~IShape() noexcept = default;

    Transform<N> Transform;

    virtual const Color &GetColor() const noexcept = 0;
    virtual void SetColor(const Color &p_Color) noexcept = 0;
};

using IShape2D = IShape<2>;
using IShape3D = IShape<3>;

// Can also be used directly by the user. It is a base class used when the underlying model is well defined and
// immutable. It makes all the other basic shape classes easier to implement (just need to create some constructors and
// voila).
ONYX_DIMENSION_TEMPLATE class ONYX_API ModelShape : public IShape<N>
{
  public:
    ModelShape(const Model *p_Model, VkPrimitiveTopology p_Topology, const Color &p_Color = Color::WHITE) noexcept;
    virtual ~ModelShape() noexcept = default;

    const Color &GetColor() const noexcept override;
    void SetColor(const Color &p_Color) noexcept override;

    virtual void Draw(Window &p_Window) noexcept override;

  private:
    const Model *m_Model;
    VkPrimitiveTopology m_Topology;
    Color m_Color;
};

using ModelShape2D = ModelShape<2>;
using ModelShape3D = ModelShape<3>;
} // namespace ONYX