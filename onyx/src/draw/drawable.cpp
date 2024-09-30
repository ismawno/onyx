#include "core/pch.hpp"
#include "onyx/draw/drawable.hpp"
#include "onyx/rendering/render_system.hpp"

namespace ONYX
{
void IDrawable::DefaultDraw(RenderSystem &p_RenderSystem, const Model *p_Model, const vec4 &p_Color,
                            const mat4 &p_Transform) noexcept
{
    RenderSystem::DrawData data{};
    data.Model = p_Model;
    data.ModelTransform = p_Transform;
    data.Color = p_Color;
    p_RenderSystem.SubmitRenderData(data);
}
} // namespace ONYX