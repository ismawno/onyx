#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/glm.hpp"

namespace ONYX
{
class Window;
class Model;
ONYX_DIMENSION_TEMPLATE class RenderSystem;
class ONYX_API IDrawable
{
  public:
    virtual ~IDrawable() = default;
    virtual void Draw(Window &p_Window) = 0;

    ONYX_DIMENSION_TEMPLATE static void DefaultModelDraw(RenderSystem<N> &p_RenderSystem, const Model *p_Model,
                                                         const vec4 &p_Color, const mat4 &p_Transform) noexcept;
};
} // namespace ONYX