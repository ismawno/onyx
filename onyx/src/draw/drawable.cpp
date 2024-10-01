#include "core/pch.hpp"
#include "onyx/draw/drawable.hpp"
#include "onyx/rendering/render_system.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE void IDrawable::DefaultModelDraw(RenderSystem<N> &p_RenderSystem, const Model *p_Model,
                                                         const vec4 &p_Color, const mat4 &p_Transform) noexcept
{
    typename RenderSystem<N>::DrawData data{};
    data.Model = p_Model;
    data.ModelTransform = p_Transform;
    data.Color = p_Color;
    p_RenderSystem.SubmitDrawData(data);
}

template void IDrawable::DefaultModelDraw(RenderSystem<2> &p_RenderSystem, const Model *p_Model, const vec4 &p_Color,
                                          const mat4 &p_Transform) noexcept;
template void IDrawable::DefaultModelDraw(RenderSystem<3> &p_RenderSystem, const Model *p_Model, const vec4 &p_Color,
                                          const mat4 &p_Transform) noexcept;
} // namespace ONYX