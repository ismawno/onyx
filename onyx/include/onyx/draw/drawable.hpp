#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/glm.hpp"

namespace ONYX
{
class Window;
class RenderSystem;
class Model;
class ONYX_API IDrawable
{
  public:
    virtual ~IDrawable() = default;
    virtual void Draw(Window &p_Window) = 0;

    static void DefaultDraw(RenderSystem &p_RenderSystem, const Model *p_Model, const vec4 &p_Color = vec4{1.f},
                            const mat4 &p_Transform = mat4{1.f}) noexcept;
};
} // namespace ONYX