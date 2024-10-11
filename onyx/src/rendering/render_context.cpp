#include "core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"
#include "onyx/draw/transform.hpp"

#include "kit/utilities/math.hpp"

namespace ONYX
{

ONYX_DIMENSION_TEMPLATE IRenderContext<N>::IRenderContext(Window *p_Window) noexcept : m_Window(p_Window)
{
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::initializeRenderers(const VkRenderPass p_RenderPass,
                                                                    const VkDescriptorSetLayout p_Layout) noexcept
{
    m_MeshRenderer.Create(p_RenderPass, p_Layout);
    m_CircleRenderer.Create(p_RenderPass, p_Layout);
    m_RenderState.push_back(RenderState<N>{});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Background(const Color &p_Color) noexcept
{
    m_MeshRenderer->Flush();
    m_CircleRenderer->Flush();
    m_Window->BackgroundColor = p_Color;
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Translate(const vec<N> &p_Translation) noexcept
{
    mat<N> &transform = m_RenderState.back().Transform;
    transform = Transform<N>::ComputeTranslationMatrix(p_Translation) * transform;
}
void RenderContext<2>::Translate(const f32 p_X, const f32 p_Y) noexcept
{
    Translate(vec2{p_X, p_Y});
}
void RenderContext<3>::Translate(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Translate(vec3{p_X, p_Y, p_Z});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Scale(const vec<N> &p_Scale) noexcept
{
    mat<N> &transform = m_RenderState.back().Transform;
    transform = Transform<N>::ComputeScaleMatrix(p_Scale) * transform;
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Scale(const f32 p_Scale) noexcept
{
    mat<N> &transform = m_RenderState.back().Transform;
    transform = Transform<N>::ComputeScaleMatrix(p_Scale) * transform;
}
void RenderContext<2>::Scale(const f32 p_X, const f32 p_Y) noexcept
{
    Scale(vec2{p_X, p_Y});
}
void RenderContext<3>::Scale(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Scale(vec3{p_X, p_Y, p_Z});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::KeepWindowAspect() noexcept
{
    // Scaling down the axes means "enlarging" their extent, that is why the inverse is used
    const f32 aspect = 1.f / m_Window->GetScreenAspect();
    mat<N> &axes = m_RenderState.back().Axes;
    if constexpr (N == 2)
        axes *= Transform2D::ComputeScaleMatrix(vec2{aspect, 1.f});
    else
        axes *= Transform3D::ComputeScaleMatrix(vec3{aspect, 1.f, aspect});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::TranslateAxes(const vec<N> &p_Translation) noexcept
{
    mat<N> &axes = m_RenderState.back().Axes;
    axes *= Transform<N>::ComputeTranslationMatrix(p_Translation);
}
void RenderContext<2>::TranslateAxes(const f32 p_X, const f32 p_Y) noexcept
{
    TranslateAxes(vec2{p_X, p_Y});
}
void RenderContext<3>::TranslateAxes(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    TranslateAxes(vec3{p_X, p_Y, p_Z});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::ScaleAxes(const vec<N> &p_Scale) noexcept
{
    mat<N> &axes = m_RenderState.back().Axes;
    axes *= Transform<N>::ComputeScaleMatrix(p_Scale);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::ScaleAxes(const f32 p_Scale) noexcept
{
    mat<N> &axes = m_RenderState.back().Axes;
    axes *= Transform<N>::ComputeScaleMatrix(p_Scale);
}
void RenderContext<2>::ScaleAxes(const f32 p_X, const f32 p_Y) noexcept
{
    ScaleAxes(vec2{p_X, p_Y});
}
void RenderContext<3>::ScaleAxes(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    ScaleAxes(vec3{p_X, p_Y, p_Z});
}

void RenderContext<2>::Rotate(const f32 p_Angle) noexcept
{
    mat3 &transform = m_RenderState.back().Transform;
    transform = Transform2D::ComputeRotationMatrix(p_Angle) * transform;
}

void RenderContext<3>::Rotate(const quat &p_Quaternion) noexcept
{
    mat4 &transform = m_RenderState.back().Transform;
    transform = Transform3D::ComputeRotationMatrix(p_Quaternion) * transform;
}

void RenderContext<3>::Rotate(const f32 p_Angle, const vec3 &p_Axis) noexcept
{
    Rotate(glm::angleAxis(p_Angle, p_Axis));
}

void RenderContext<3>::Rotate(const vec3 &p_Angles) noexcept
{
    Rotate(glm::quat(p_Angles));
}

void RenderContext<2>::RotateAxes(const f32 p_Angle) noexcept
{
    mat3 &axes = m_RenderState.back().Axes;
    axes *= Transform2D::ComputeRotationMatrix(p_Angle);
}

void RenderContext<3>::RotateAxes(const quat &p_Quaternion) noexcept
{
    mat4 &axes = m_RenderState.back().Axes;
    axes *= Transform3D::ComputeRotationMatrix(glm::conjugate(p_Quaternion));
}

void RenderContext<3>::RotateAxes(const f32 p_Angle, const vec3 &p_Axis) noexcept
{
    RotateAxes(glm::angleAxis(p_Angle, p_Axis));
}

void RenderContext<3>::RotateAxes(const vec3 &p_Angles) noexcept
{
    RotateAxes(glm::quat(p_Angles));
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square() noexcept
{
    const Model *model = Model::GetSquare<N>();
    Mesh(model);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const f32 p_Size) noexcept
{
    const mat<N> transform = Transform<N>::ComputeScaleMatrix(p_Size) * m_RenderState.back().Transform;
    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const vec<N> &p_Position) noexcept
{
    const mat<N> transform = Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const vec<N> &p_Position, const f32 p_Size) noexcept
{
    const mat<N> transform =
        Transform<N>::ComputeTranslationScaleMatrix(p_Position, p_Size) * m_RenderState.back().Transform;
    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
void RenderContext<2>::Square(const f32 p_X, const f32 p_Y) noexcept
{
    Square(vec2{p_X, p_Y});
}
void RenderContext<2>::Square(const f32 p_X, const f32 p_Y, const f32 p_Size) noexcept
{
    Square(vec2{p_X, p_Y}, p_Size);
}
void RenderContext<3>::Square(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Square(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Square(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size) noexcept
{
    Square(vec3{p_X, p_Y, p_Z}, p_Size);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform = Transform2D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeScaleMatrix(vec3{p_Dimensions, 1.f}) * m_RenderState.back().Transform;

    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const vec<N> &p_Position, const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform =
            Transform2D::ComputeTranslationScaleMatrix(p_Position, p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeTranslationScaleMatrix(p_Position, vec3{p_Dimensions, 1.f}) *
                    m_RenderState.back().Transform;

    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const f32 p_XDim, const f32 p_YDim) noexcept
{
    Rect(vec2{p_XDim, p_YDim});
}
void RenderContext<2>::Rect(const f32 p_X, const f32 p_Y, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Rect(vec2{p_X, p_Y}, vec2{p_XDim, p_YDim});
}
void RenderContext<3>::Rect(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Rect(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides) noexcept
{
    const Model *model = Model::GetRegularPolygon<N>(p_Sides);
    Mesh(model);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const vec<N> &p_Position) noexcept
{
    const mat<N> transform = Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    const Model *model = Model::GetRegularPolygon<N>(p_Sides);
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const vec<N> &p_Position,
                                                     const f32 p_Radius) noexcept
{
    const mat<N> transform =
        Transform<N>::ComputeTranslationScaleMatrix(p_Position, p_Radius) * m_RenderState.back().Transform;
    const Model *model = Model::GetRegularPolygon<N>(p_Sides);
    Mesh(model, transform);
}
void RenderContext<2>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y) noexcept
{
    NGon(p_Sides, vec2{p_X, p_Y});
}
void RenderContext<2>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y, const f32 p_Radius) noexcept
{
    NGon(p_Sides, vec2{p_X, p_Y}, p_Radius);
}
void RenderContext<3>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    NGon(p_Sides, vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius) noexcept
{
    NGon(p_Sides, vec3{p_X, p_Y, p_Z}, p_Radius);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle() noexcept
{
    circleMesh(m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle(const f32 p_Radius) noexcept
{
    const mat<N> transform = Transform<N>::ComputeScaleMatrix(p_Radius) * m_RenderState.back().Transform;
    circleMesh(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle(const vec<N> &p_Position) noexcept
{
    const mat<N> transform = Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    circleMesh(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle(const vec<N> &p_Position, const f32 p_Radius) noexcept
{
    const mat<N> transform =
        Transform<N>::ComputeTranslationScaleMatrix(p_Position, p_Radius) * m_RenderState.back().Transform;
    circleMesh(transform);
}
void RenderContext<2>::Circle(const f32 p_X, const f32 p_Y) noexcept
{
    Circle(vec2{p_X, p_Y});
}
void RenderContext<2>::Circle(const f32 p_X, const f32 p_Y, const f32 p_Radius) noexcept
{
    Circle(vec2{p_X, p_Y}, p_Radius);
}
void RenderContext<3>::Circle(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Circle(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Circle(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius) noexcept
{
    Circle(vec3{p_X, p_Y, p_Z}, p_Radius);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform = Transform2D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeScaleMatrix(vec3{p_Dimensions, 1.f}) * m_RenderState.back().Transform;
    circleMesh(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const vec<N> &p_Position, const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform =
            Transform2D::ComputeTranslationScaleMatrix(p_Position, p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeTranslationScaleMatrix(p_Position, vec3{p_Dimensions, 1.f}) *
                    m_RenderState.back().Transform;
    circleMesh(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec2{p_XDim, p_YDim});
}
void RenderContext<2>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec2{p_X, p_Y}, vec2{p_XDim, p_YDim});
}
void RenderContext<3>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Line(const vec<N> &p_Start, const vec<N> &p_End,
                                                     const f32 p_Thickness) noexcept
{
    Transform<N> t;
    t.Translation = 0.5f * (p_Start + p_End);
    const vec<N> delta = p_End - p_Start;
    if constexpr (N == 2)
        t.Rotation = glm::atan(delta.y, delta.x);
    else
        t.Rotation = quat{{0.f, glm::atan(delta.y, delta.x), glm::atan(delta.z, delta.x)}};
    t.Scale.x = glm::length(delta);
    t.Scale.y = p_Thickness;

    const mat<N> transform = m_RenderState.back().Axes * t.ComputeTransform() * m_RenderState.back().Transform;
    const Model *model = Model::GetSquare<N>();
    m_MeshRenderer->Draw(model, transform, m_RenderState.back().FillColor);
}

void RenderContext<2>::Line(const f32 p_X1, const f32 p_Y1, const f32 p_X2, const f32 p_Y2,
                            const f32 p_Thickness) noexcept
{
    Line(vec2{p_X1, p_Y1}, vec2{p_X2, p_Y2}, p_Thickness);
}
void RenderContext<3>::Line(const f32 p_X1, const f32 p_Y1, const f32 p_Z1, const f32 p_X2, const f32 p_Y2,
                            const f32 p_Z2, const f32 p_Thickness) noexcept
{
    Line(vec3{p_X1, p_Y1, p_Z1}, vec3{p_X2, p_Y2, p_Z2}, p_Thickness);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::LineStrip(std::span<const vec<N>> p_Points,
                                                          const f32 p_Thickness) noexcept
{
    KIT_ASSERT(p_Points.size() > 1, "A line strip must have at least two points");
    for (u32 i = 0; i < p_Points.size() - 1; ++i)
        Line(p_Points[i], p_Points[i + 1], p_Thickness);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::RoundedLine(const vec<N> &p_Start, const vec<N> &p_End,
                                                            const f32 p_Thickness) noexcept
{
    Line(p_Start, p_End, p_Thickness);
    Circle(p_Start, p_Thickness);
    Circle(p_End, p_Thickness);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::RoundedLineStrip(std::span<const vec<N>> p_Points,
                                                                 const f32 p_Thickness) noexcept
{
    LineStrip(p_Points, p_Thickness);
    for (const vec<N> &point : p_Points)
        Circle(point, p_Thickness);
}

void RenderContext<2>::RoundedLine(const f32 p_X1, const f32 p_Y1, const f32 p_X2, const f32 p_Y2,
                                   const f32 p_Thickness) noexcept
{
    RoundedLine(vec2{p_X1, p_Y1}, vec2{p_X2, p_Y2}, p_Thickness);
}
void RenderContext<3>::RoundedLine(const f32 p_X1, const f32 p_Y1, const f32 p_Z1, const f32 p_X2, const f32 p_Y2,
                                   const f32 p_Z2, const f32 p_Thickness) noexcept
{
    RoundedLine(vec3{p_X1, p_Y1, p_Z1}, vec3{p_X2, p_Y2, p_Z2}, p_Thickness);
}

void RenderContext<3>::Cube() noexcept
{
    const Model *model = Model::GetCube();
    Mesh(model);
}
void RenderContext<3>::Cube(const f32 p_Size) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(p_Size) * m_RenderState.back().Transform;
    const Model *model = Model::GetCube();
    Mesh(model, transform);
}
void RenderContext<3>::Cube(const vec3 &p_Position) noexcept
{
    const mat4 transform = Transform3D::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    const Model *model = Model::GetCube();
    Mesh(model, transform);
}
void RenderContext<3>::Cube(const vec3 &p_Position, const f32 p_Size) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, p_Size) * m_RenderState.back().Transform;
    const Model *model = Model::GetCube();
    Mesh(model, transform);
}
void RenderContext<3>::Cube(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Cube(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Cube(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size) noexcept
{
    Cube(vec3{p_X, p_Y, p_Z}, p_Size);
}

void RenderContext<3>::Cuboid(const vec3 &p_Dimensions) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    const Model *model = Model::GetCube();
    Mesh(model, transform);
}
void RenderContext<3>::Cuboid(const vec3 &p_Position, const vec3 &p_Dimensions) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, p_Dimensions) * m_RenderState.back().Transform;
    const Model *model = Model::GetCube();
    Mesh(model, transform);
}
void RenderContext<3>::Cuboid(const f32 p_XDim, const f32 p_YDim, const f32 p_ZDim) noexcept
{
    Cuboid(vec3{p_XDim, p_YDim, p_ZDim});
}
void RenderContext<3>::Cuboid(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                              const f32 p_ZDim) noexcept
{
    Cuboid(vec3{p_X, p_Y, p_Z}, vec3{p_XDim, p_YDim, p_ZDim});
}

void RenderContext<3>::Sphere() noexcept
{
    const Model *model = Model::GetSphere();
    Mesh(model);
}
void RenderContext<3>::Sphere(const f32 p_Radius) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(p_Radius) * m_RenderState.back().Transform;
    const Model *model = Model::GetSphere();
    Mesh(model, transform);
}
void RenderContext<3>::Sphere(const vec3 &p_Position) noexcept
{
    const mat4 transform = Transform3D::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    const Model *model = Model::GetSphere();
    Mesh(model, transform);
}
void RenderContext<3>::Sphere(const vec3 &p_Position, const f32 p_Radius) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, p_Radius) * m_RenderState.back().Transform;
    const Model *model = Model::GetSphere();
    Mesh(model, transform);
}
void RenderContext<3>::Sphere(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Sphere(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Sphere(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius) noexcept
{
    Sphere(vec3{p_X, p_Y, p_Z}, p_Radius);
}

void RenderContext<3>::Ellipsoid(const vec3 &p_Dimensions) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    const Model *model = Model::GetSphere();
    Mesh(model, transform);
}
void RenderContext<3>::Ellipsoid(const vec3 &p_Position, const vec3 &p_Dimensions) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, p_Dimensions) * m_RenderState.back().Transform;
    const Model *model = Model::GetSphere();
    Mesh(model, transform);
}
void RenderContext<3>::Ellipsoid(const f32 p_XDim, const f32 p_YDim, const f32 p_ZDim) noexcept
{
    Ellipsoid(vec3{p_XDim, p_YDim, p_ZDim});
}
void RenderContext<3>::Ellipsoid(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                                 const f32 p_ZDim) noexcept
{
    Ellipsoid(vec3{p_X, p_Y, p_Z}, vec3{p_XDim, p_YDim, p_ZDim});
}

void RenderContext<2>::Fill() noexcept
{
    m_RenderState.back().NoFill = false;
}
void RenderContext<2>::NoFill() noexcept
{
    m_RenderState.back().NoFill = true;
}

// this is a sorry function
template <u32 N, typename Renderer>
static void drawMesh(Renderer *p_Renderer, const RenderState<N> &p_State, const Window *p_Window,
                     const mat<N> &p_Transform, const Model *p_Model = nullptr)
{
    if constexpr (N == 2)
    {
        if (!p_State.NoStroke && !KIT::ApproachesZero(p_State.StrokeWidth))
        {
            const vec2 scale = Transform2D::ExtractScaleTransform(p_Transform);
            const vec2 stroke = (scale + p_State.StrokeWidth) / scale;
            const mat<N> transform = p_State.Axes * Transform2D::ComputeScaleMatrix(stroke) * p_Transform;
            if constexpr (std::is_same_v<Renderer, MeshRenderer<N>>)
                p_Renderer->Draw(p_Model, transform, p_State.StrokeColor);
            else
                p_Renderer->Draw(transform, p_State.StrokeColor);
        }
        if constexpr (std::is_same_v<Renderer, MeshRenderer<N>>)
            p_Renderer->Draw(p_Model, p_State.Axes * p_Transform,
                             p_State.NoFill ? p_Window->BackgroundColor : p_State.FillColor);
        else
            p_Renderer->Draw(p_State.Axes * p_Transform,
                             p_State.NoFill ? p_Window->BackgroundColor : p_State.FillColor);
    }
    else if constexpr (std::is_same_v<Renderer, MeshRenderer<N>>)
        p_Renderer->Draw(p_Model, p_State.Axes * p_Transform, p_State.FillColor);
    else
        p_Renderer->Draw(p_State.Axes * p_Transform, p_State.FillColor);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const Model *p_Model, const mat<N> &p_Transform) noexcept
{
    drawMesh(m_MeshRenderer.Get(), m_RenderState.back(), m_Window, p_Transform, p_Model);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const Model *p_Model) noexcept
{
    Mesh(p_Model, m_RenderState.back().Transform);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Push() noexcept
{
    m_RenderState.push_back(m_RenderState.back());
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::PushAndClear() noexcept
{
    m_RenderState.push_back(RenderState<N>{});
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Pop() noexcept
{
    KIT_ASSERT(m_RenderState.size() > 1, "For every push, there must be a pop");
    m_RenderState.pop_back();
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Fill(const Color &p_Color) noexcept
{
    m_RenderState.back().FillColor = p_Color;
}
void RenderContext<2>::Stroke() noexcept
{
    m_RenderState.back().NoStroke = false;
}
void RenderContext<2>::Stroke(const Color &p_Color) noexcept
{
    m_RenderState.back().StrokeColor = p_Color;
    m_RenderState.back().NoStroke = false;
}
void RenderContext<2>::StrokeWidth(const f32 p_Width) noexcept
{
    m_RenderState.back().StrokeWidth = p_Width;
    m_RenderState.back().NoStroke = false;
}
void RenderContext<2>::NoStroke() noexcept
{
    m_RenderState.back().NoStroke = true;
}

ONYX_DIMENSION_TEMPLATE const mat<N> &IRenderContext<N>::GetCurrentTransform() const noexcept
{
    return m_RenderState.back().Transform;
}
ONYX_DIMENSION_TEMPLATE const mat<N> &IRenderContext<N>::GetCurrentAxes() const noexcept
{
    return m_RenderState.back().Axes;
}

ONYX_DIMENSION_TEMPLATE mat<N> &IRenderContext<N>::GetCurrentTransform() noexcept
{
    return m_RenderState.back().Transform;
}
ONYX_DIMENSION_TEMPLATE mat<N> &IRenderContext<N>::GetCurrentAxes() noexcept
{
    return m_RenderState.back().Axes;
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::SetCurrentTransform(const mat<N> &p_Transform) noexcept
{
    m_RenderState.back().Transform = p_Transform;
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::SetCurrentAxes(const mat<N> &p_Axes) noexcept
{
    m_RenderState.back().Axes = p_Axes;
}

vec2 RenderContext<2>::GetMouseCoordinates() const noexcept
{
    const vec2 mpos = Input::GetMousePosition(m_Window);
#if ONYX_COORDINATE_SYSTEM == ONYX_CS_CENTERED_CARTESIAN
    mat3 axes = m_RenderState.back().Axes;
    axes[2][1] = -axes[2][1];
#else
    const mat3 &axes = m_RenderState.back().Axes;
#endif
    return glm::inverse(axes) * vec3{mpos, 1.f};
}
vec3 RenderContext<3>::GetMouseCoordinates(const f32 p_Depth) const noexcept
{
    const vec2 mpos = Input::GetMousePosition(m_Window);
#if ONYX_COORDINATE_SYSTEM == ONYX_CS_CENTERED_CARTESIAN
    mat4 axes = m_RenderState.back().Axes;
    axes[2][1] = -axes[2][1];
#else
    const mat4 &axes = m_RenderState.back().Axes;
#endif
    return glm::inverse(axes) * vec4{mpos, p_Depth, 1.f};
}

void RenderContext<3>::SetPerspectiveProjection(const f32 p_FieldOfView, const f32 p_Aspect, const f32 p_Near,
                                                const f32 p_Far) noexcept
{
    const f32 halfPov = glm::tan(0.5f * p_FieldOfView);

    m_Projection = mat4{0.f};
    m_Projection[0][0] = 1.f / (p_Aspect * halfPov);
    m_Projection[1][1] = 1.f / halfPov;
    m_Projection[2][2] = p_Far / (p_Far - p_Near);
    m_Projection[2][3] = 1.f;
    m_Projection[3][2] = p_Far * p_Near / (p_Near - p_Far);
}
void RenderContext<3>::SetOrthographicProjection() noexcept
{
    // Already included in the axes matrix
    m_Projection = mat4{1.f};
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::circleMesh(const mat<N> &p_Transform) noexcept
{
    drawMesh(m_CircleRenderer.Get(), m_RenderState.back(), m_Window, p_Transform);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::resetRenderState() noexcept
{
    KIT_ASSERT(m_RenderState.size() == 1, "For every push, there must be a pop");
    m_RenderState[0] = RenderState<N>{};
}

RenderContext<2>::RenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : IRenderContext<2>(p_Window)
{
    initializeRenderers(p_RenderPass, nullptr);
}

void RenderContext<2>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    RenderInfo<2> info{};
    info.CommandBuffer = p_CommandBuffer;

    m_MeshRenderer->Render(info);
    m_CircleRenderer->Render(info);
    resetRenderState();
}

RenderContext<3>::RenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : IRenderContext<3>(p_Window)
{
    m_Device = Core::GetDevice();
    createGlobalUniformHelper();
    initializeRenderers(p_RenderPass, m_GlobalUniformHelper->Layout.GetLayout());
}

RenderContext<3>::~RenderContext() noexcept
{
    m_GlobalUniformHelper.Destroy();
}

struct GlobalUBO
{
    vec4 LightDirection;
    f32 LightIntensity;
    f32 AmbientIntensity;
};

void RenderContext<3>::Render(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    GlobalUBO ubo{};
    ubo.LightDirection = vec4{glm::normalize(LightDirection), 0.f};
    ubo.LightIntensity = LightIntensity;
    ubo.AmbientIntensity = AmbientIntensity;

    m_GlobalUniformHelper->UniformBuffer.WriteAt(p_FrameIndex, &ubo);
    m_GlobalUniformHelper->UniformBuffer.FlushAt(p_FrameIndex);

    RenderInfo<3> info{};
    info.CommandBuffer = p_CommandBuffer;
    info.GlobalDescriptorSet = m_GlobalUniformHelper->DescriptorSets[p_FrameIndex];

    m_MeshRenderer->Render(info);
    m_CircleRenderer->Render(info);
    resetRenderState();
}

void RenderContext<3>::createGlobalUniformHelper() noexcept
{
    DescriptorPool::Specs poolSpecs{};
    poolSpecs.MaxSets = SwapChain::MFIF;
    poolSpecs.PoolSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MFIF});

    static constexpr std::array<VkDescriptorSetLayoutBinding, 1> bindings = {
        {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};

    const auto &props = m_Device->GetProperties();
    Buffer::Specs bufferSpecs{};
    bufferSpecs.InstanceCount = SwapChain::MFIF;
    bufferSpecs.InstanceSize = sizeof(GlobalUBO);
    bufferSpecs.Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    bufferSpecs.MinimumAlignment =
        glm::max(props.limits.minUniformBufferOffsetAlignment, props.limits.nonCoherentAtomSize);

    m_GlobalUniformHelper.Create(poolSpecs, bindings, bufferSpecs);
    m_GlobalUniformHelper->UniformBuffer.Map();

    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        const auto info = m_GlobalUniformHelper->UniformBuffer.DescriptorInfoAt(i);
        DescriptorWriter writer{&m_GlobalUniformHelper->Layout, &m_GlobalUniformHelper->Pool};
        writer.WriteBuffer(0, &info);
        m_GlobalUniformHelper->DescriptorSets[i] = writer.Build();
    }
}

template class IRenderContext<2>;
template class IRenderContext<3>;

} // namespace ONYX