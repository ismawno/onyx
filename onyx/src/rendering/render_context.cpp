#include "core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/app/input.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/app/window.hpp"

#include "kit/utilities/math.hpp"

namespace ONYX
{

ONYX_DIMENSION_TEMPLATE IRenderContext<N>::IRenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : m_Renderer(p_Window, p_RenderPass, &m_RenderState), m_Window(p_Window)
{
    m_RenderState.push_back(RenderState<N>{});
    // All axes transformation come "from the right", and axes offset must come "from the left", so it is actually fine
    // to have the axes starting as the current offset. Can only be done in 3D, because transformations may involve some
    // axis that dont exist in 2D. This offset is apply later for 2D cases, when eventually the mat3's become mat4's
    if constexpr (N == 3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::FlushState() noexcept
{
    KIT_ASSERT(m_RenderState.size() == 1, "For every push, there must be a pop");
    m_RenderState[0] = RenderState<N>{};
    if constexpr (N == 3)
        ApplyCoordinateSystem(m_RenderState[0].Axes, &m_RenderState[0].InverseAxes);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::FlushState(const Color &p_Color) noexcept
{
    FlushState();
    m_Window->BackgroundColor = p_Color;
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::FlushRenderData() noexcept
{
    m_Renderer.Flush();
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Flush() noexcept
{
    FlushRenderData();
    FlushState();
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Flush(const Color &p_Color) noexcept
{
    FlushRenderData();
    FlushState(p_Color);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Transform(const mat<N> &p_Transform) noexcept
{
    m_RenderState.back().Transform = p_Transform * m_RenderState.back().Transform;
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Transform(const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept
{
    this->Transform(ONYX::Transform<N>::ComputeTranslationScaleMatrix(p_Translation, p_Scale));
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Transform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                          const rot<N> &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform<N>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Transform(const vec<N> &p_Translation, const f32 p_Scale) noexcept
{
    this->Transform(ONYX::Transform<N>::ComputeTranslationScaleMatrix(p_Translation, vec<N>{p_Scale}));
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Transform(const vec<N> &p_Translation, const f32 p_Scale,
                                                          const rot<N> &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform<N>::ComputeTransform(p_Translation, vec<N>{p_Scale}, p_Rotation));
}
void RenderContext<2>::Transform(const f32 p_X, const f32 p_Y, const f32 p_Scale) noexcept
{
    this->Transform(vec2{p_X, p_Y}, p_Scale);
}
void RenderContext<2>::Transform(const f32 p_X, const f32 p_Y, const f32 p_XS, const f32 p_YS) noexcept
{
    this->Transform(vec2{p_X, p_Y}, vec2{p_XS, p_YS});
}
void RenderContext<2>::Transform(const f32 p_X, const f32 p_Y, const f32 p_XS, const f32 p_YS,
                                 const f32 p_Rotation) noexcept
{
    this->Transform(vec2{p_X, p_Y}, vec2{p_XS, p_YS}, p_Rotation);
}
void RenderContext<3>::Transform(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Scale) noexcept
{
    this->Transform(vec3{p_X, p_Y, p_Z}, p_Scale);
}
void RenderContext<3>::Transform(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Scale,
                                 const quat &p_Quaternion) noexcept
{
    this->Transform(vec3{p_X, p_Y, p_Z}, p_Scale, p_Quaternion);
}
void RenderContext<3>::Transform(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XS, const f32 p_YS,
                                 const f32 p_ZS) noexcept
{
    this->Transform(vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS});
}
void RenderContext<3>::Transform(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XS, const f32 p_YS,
                                 const f32 p_ZS, const quat &p_Quaternion) noexcept
{
    this->Transform(vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, p_Quaternion);
}
void RenderContext<3>::Transform(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Scale,
                                 const vec3 &p_Angles) noexcept
{
    this->Transform(vec3{p_X, p_Y, p_Z}, p_Scale, quat{p_Angles});
}
void RenderContext<3>::Transform(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XS, const f32 p_YS,
                                 const f32 p_ZS, const vec3 &p_Angles) noexcept
{
    this->Transform(vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, quat{p_Angles});
}
void RenderContext<3>::Transform(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XS, const f32 p_YS,
                                 const f32 p_ZS, const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    this->Transform(vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, quat{{p_XRot, p_YRot, p_ZRot}});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::TransformAxes(const mat<N> &p_Axes) noexcept
{
    m_RenderState.back().Axes *= p_Axes;
    if constexpr (N == 3)
        m_RenderState.back().InverseAxes = glm::inverse(m_RenderState.back().Axes);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::TransformAxes(const vec<N> &p_Translation,
                                                              const vec<N> &p_Scale) noexcept
{
    TransformAxes(ONYX::Transform<N>::ComputeReverseTranslationScaleMatrix(p_Translation, p_Scale));
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::TransformAxes(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                              const rot<N> &p_Rotation) noexcept
{
    TransformAxes(ONYX::Transform<N>::ComputeReverseTransform(p_Translation, p_Scale, p_Rotation));
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::TransformAxes(const vec<N> &p_Translation, const f32 p_Scale) noexcept
{
    TransformAxes(ONYX::Transform<N>::ComputeReverseTranslationScaleMatrix(p_Translation, vec<N>{p_Scale}));
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::TransformAxes(const vec<N> &p_Translation, const f32 p_Scale,
                                                              const rot<N> &p_Rotation) noexcept
{
    TransformAxes(ONYX::Transform<N>::ComputeReverseTransform(p_Translation, vec<N>{p_Scale}, p_Rotation));
}
void RenderContext<2>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_Scale) noexcept
{
    TransformAxes(vec2{p_X, p_Y}, p_Scale);
}
void RenderContext<2>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_XS, const f32 p_YS) noexcept
{
    TransformAxes(vec2{p_X, p_Y}, vec2{p_XS, p_YS});
}
void RenderContext<2>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_XS, const f32 p_YS,
                                     const f32 p_Rotation) noexcept
{
    TransformAxes(vec2{p_X, p_Y}, vec2{p_XS, p_YS}, p_Rotation);
}
void RenderContext<3>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Scale) noexcept
{
    TransformAxes(vec3{p_X, p_Y, p_Z}, p_Scale);
}
void RenderContext<3>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Scale,
                                     const quat &p_Quaternion) noexcept
{
    TransformAxes(vec3{p_X, p_Y, p_Z}, p_Scale, p_Quaternion);
}
void RenderContext<3>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XS, const f32 p_YS,
                                     const f32 p_ZS) noexcept
{
    TransformAxes(vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS});
}
void RenderContext<3>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XS, const f32 p_YS,
                                     const f32 p_ZS, const quat &p_Quaternion) noexcept
{
    TransformAxes(vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, p_Quaternion);
}
void RenderContext<3>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Scale,
                                     const vec3 &p_Angles) noexcept
{
    TransformAxes(vec3{p_X, p_Y, p_Z}, p_Scale, quat{p_Angles});
}
void RenderContext<3>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XS, const f32 p_YS,
                                     const f32 p_ZS, const vec3 &p_Angles) noexcept
{
    TransformAxes(vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, quat{p_Angles});
}
void RenderContext<3>::TransformAxes(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XS, const f32 p_YS,
                                     const f32 p_ZS, const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    TransformAxes(vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, vec3{p_XRot, p_YRot, p_ZRot});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Translate(const vec<N> &p_Translation) noexcept
{
    this->Transform(ONYX::Transform<N>::ComputeTranslationMatrix(p_Translation));
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
    this->Transform(ONYX::Transform<N>::ComputeScaleMatrix(p_Scale));
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Scale(const f32 p_Scale) noexcept
{
    Scale(vec<N>{p_Scale});
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
    if constexpr (N == 2)
        ScaleAxes(vec2{aspect, 1.f});
    else
        ScaleAxes(vec3{aspect, 1.f, 1.f});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::TranslateAxes(const vec<N> &p_Translation) noexcept
{
    TransformAxes(ONYX::Transform<N>::ComputeTranslationMatrix(p_Translation));
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
    TransformAxes(ONYX::Transform<N>::ComputeScaleMatrix(p_Scale));
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::ScaleAxes(const f32 p_Scale) noexcept
{
    ScaleAxes(vec<N>{p_Scale});
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
    this->Transform(Transform2D::ComputeRotationMatrix(p_Angle));
}

void RenderContext<3>::Rotate(const quat &p_Quaternion) noexcept
{
    this->Transform(Transform3D::ComputeRotationMatrix(p_Quaternion));
}
void RenderContext<3>::Rotate(const f32 p_Angle, const vec3 &p_Axis) noexcept
{
    Rotate(glm::angleAxis(p_Angle, p_Axis));
}
void RenderContext<3>::Rotate(const vec3 &p_Angles) noexcept
{
    Rotate(glm::quat(p_Angles));
}
void RenderContext<3>::Rotate(const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Rotate(vec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<3>::RotateX(const f32 p_Angle) noexcept
{
    Rotate(vec3{p_Angle, 0.f, 0.f});
}
void RenderContext<3>::RotateY(const f32 p_Angle) noexcept
{
    Rotate(vec3{0.f, p_Angle, 0.f});
}
void RenderContext<3>::RotateZ(const f32 p_Angle) noexcept
{
    Rotate(vec3{0.f, 0.f, p_Angle});
}

void RenderContext<2>::RotateAxes(const f32 p_Angle) noexcept
{
    TransformAxes(Transform2D::ComputeRotationMatrix(p_Angle));
}
void RenderContext<3>::RotateAxes(const quat &p_Quaternion) noexcept
{
    TransformAxes(Transform3D::ComputeRotationMatrix(p_Quaternion));
}
void RenderContext<3>::RotateAxes(const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    RotateAxes(vec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<3>::RotateAxes(const f32 p_Angle, const vec3 &p_Axis) noexcept
{
    RotateAxes(glm::angleAxis(p_Angle, p_Axis));
}
void RenderContext<3>::RotateAxes(const vec3 &p_Angles) noexcept
{
    RotateAxes(glm::quat(p_Angles));
}
void RenderContext<3>::RotateXAxes(const f32 p_Angle) noexcept
{
    RotateAxes(vec3{p_Angle, 0.f, 0.f});
}
void RenderContext<3>::RotateYAxes(const f32 p_Angle) noexcept
{
    RotateAxes(vec3{0.f, p_Angle, 0.f});
}
void RenderContext<3>::RotateZAxes(const f32 p_Angle) noexcept
{
    RotateAxes(vec3{0.f, 0.f, p_Angle});
}

static mat4 transform3ToTransform4(const mat3 &p_Transform) noexcept
{
    mat4 t4{1.f};
    t4[0][0] = p_Transform[0][0];
    t4[0][1] = p_Transform[0][1];
    t4[1][0] = p_Transform[1][0];
    t4[1][1] = p_Transform[1][1];

    t4[3][0] = p_Transform[2][0];
    t4[3][1] = p_Transform[2][1];
    return t4;
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Triangle() noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetTriangleIndex(), m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Triangle(const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetTriangleIndex(), p_Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Triangle(const f32 p_Size) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeScaleMatrix(vec<N>{p_Size}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetTriangleIndex(), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Triangle(const vec<N> &p_Position) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetTriangleIndex(), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Triangle(const vec<N> &p_Position, const f32 p_Size) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTranslationScaleMatrix(p_Position, vec<N>{p_Size}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetTriangleIndex(), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Triangle(const vec<N> &p_Position, f32 p_Size,
                                                         const rot<N> &p_Rotation) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTransform(p_Position, vec<N>{p_Size}, p_Rotation) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetTriangleIndex(), transform);
}
void RenderContext<2>::Triangle(const f32 p_X, const f32 p_Y) noexcept
{
    Triangle(vec2{p_X, p_Y});
}
void RenderContext<2>::Triangle(const f32 p_X, const f32 p_Y, const f32 p_Size) noexcept
{
    Triangle(vec2{p_X, p_Y}, p_Size);
}
void RenderContext<2>::Triangle(const f32 p_X, const f32 p_Y, const f32 p_Size, const f32 p_Rotation) noexcept
{
    Triangle(vec2{p_X, p_Y}, p_Size, p_Rotation);
}
void RenderContext<3>::Triangle(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Triangle(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Triangle(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size) noexcept
{
    Triangle(vec3{p_X, p_Y, p_Z}, p_Size);
}
void RenderContext<3>::Triangle(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size,
                                const quat &p_Quaternion) noexcept
{
    Triangle(vec3{p_X, p_Y, p_Z}, p_Size, p_Quaternion);
}
void RenderContext<3>::Triangle(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size, const f32 p_XRot,
                                const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Triangle(vec3{p_X, p_Y, p_Z}, p_Size, quat{{p_XRot, p_YRot, p_ZRot}});
}
void RenderContext<3>::Triangle(const vec3 &p_Position, const f32 p_Size, const vec3 &p_Angles) noexcept
{
    Triangle(p_Position, p_Size, quat{p_Angles});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square() noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const f32 p_Size) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeScaleMatrix(vec<N>{p_Size}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const vec<N> &p_Position) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const vec<N> &p_Position, const f32 p_Size) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTranslationScaleMatrix(p_Position, vec<N>{p_Size}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const vec<N> &p_Position, f32 p_Size,
                                                       const rot<N> &p_Rotation) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTransform(p_Position, vec<N>{p_Size}, p_Rotation) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), transform);
}
void RenderContext<2>::Square(const f32 p_X, const f32 p_Y) noexcept
{
    Square(vec2{p_X, p_Y});
}
void RenderContext<2>::Square(const f32 p_X, const f32 p_Y, const f32 p_Size) noexcept
{
    Square(vec2{p_X, p_Y}, p_Size);
}
void RenderContext<2>::Square(const f32 p_X, const f32 p_Y, const f32 p_Size, const f32 p_Rotation) noexcept
{
    Square(vec2{p_X, p_Y}, p_Size, p_Rotation);
}
void RenderContext<3>::Square(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Square(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Square(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size) noexcept
{
    Square(vec3{p_X, p_Y, p_Z}, p_Size);
}
void RenderContext<3>::Square(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size,
                              const quat &p_Quaternion) noexcept
{
    Square(vec3{p_X, p_Y, p_Z}, p_Size, p_Quaternion);
}
void RenderContext<3>::Square(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size, const f32 p_XRot,
                              const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Square(vec3{p_X, p_Y, p_Z}, p_Size, quat{{p_XRot, p_YRot, p_ZRot}});
}
void RenderContext<3>::Square(const vec3 &p_Position, const f32 p_Size, const vec3 &p_Angles) noexcept
{
    Square(p_Position, p_Size, quat{p_Angles});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform = Transform2D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeScaleMatrix(vec3{p_Dimensions, 1.f}) * m_RenderState.back().Transform;

    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), transform);
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

    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const vec<N> &p_Position, const vec2 &p_Dimensions,
                                                     const rot<N> &p_Rotation) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform =
            Transform2D::ComputeTransform(p_Position, p_Dimensions, p_Rotation) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeTransform(p_Position, vec<N>{p_Dimensions, 1.f}, p_Rotation) *
                    m_RenderState.back().Transform;

    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const mat<N> &p_Trasform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), p_Trasform);
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
void RenderContext<3>::Rect(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                            const quat &p_Quaternion) noexcept
{
    Rect(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim}, p_Quaternion);
}
void RenderContext<3>::Rect(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                            const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Rect(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim}, vec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<3>::Rect(const vec3 &p_Position, const vec2 &p_Dimensions, const vec3 &p_Angles) noexcept
{
    Rect(p_Position, p_Dimensions, quat{p_Angles});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides) noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetNGonIndex(p_Sides), m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const f32 p_Radius) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeScaleMatrix(vec<N>{2.f * p_Radius}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetNGonIndex(p_Sides), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetNGonIndex(p_Sides), p_Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const vec<N> &p_Position) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetNGonIndex(p_Sides), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const vec<N> &p_Position,
                                                     const f32 p_Radius) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeTranslationScaleMatrix(p_Position, vec<N>{2.f * p_Radius}) *
                             m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetNGonIndex(p_Sides), transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const vec<N> &p_Position, const f32 p_Radius,
                                                     const rot<N> &p_Rotation) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeTransform(p_Position, vec<N>{2.f * p_Radius}, p_Rotation) *
                             m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives<N>::GetNGonIndex(p_Sides), transform);
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
void RenderContext<3>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius,
                            const quat &p_Quaternion) noexcept
{
    NGon(p_Sides, vec3{p_X, p_Y, p_Z}, p_Radius, p_Quaternion);
}
void RenderContext<3>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius,
                            const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    NGon(p_Sides, vec3{p_X, p_Y, p_Z}, p_Radius, vec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<3>::NGon(const u32 p_Sides, const vec3 &p_Position, const f32 p_Radius,
                            const vec3 &p_Angles) noexcept
{
    NGon(p_Sides, p_Position, p_Radius, quat{p_Angles});
}

ONYX_DIMENSION_TEMPLATE
template <typename... Vertices>
    requires(sizeof...(Vertices) >= 3 && (std::is_same_v<Vertices, vec<N>> && ...))
void IRenderContext<N>::Polygon(Vertices &&...p_Vertices) noexcept
{
    m_Renderer.DrawPolygon({std::forward<Vertices>(p_Vertices)...}, m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Polygon(const std::span<const vec<N>> p_Vertices) noexcept
{
    m_Renderer.DrawPolygon(p_Vertices, m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Polygon(const std::span<const vec<N>> p_Vertices,
                                                        const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawPolygon(p_Vertices, p_Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Polygon(const std::span<const vec<N>> p_Vertices,
                                                        const vec<N> &p_Translation) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTranslationMatrix(p_Translation) * m_RenderState.back().Transform;
    m_Renderer.DrawPolygon(p_Vertices, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Polygon(const std::span<const vec<N>> p_Vertices,
                                                        const vec<N> &p_Translation, const rot<N> &p_Rotation) noexcept
{
    // TODO: Remove the need of this vec<N>{1.f} at some point
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTransform(p_Translation, vec<N>{1.f}, p_Rotation) * m_RenderState.back().Transform;
    m_Renderer.DrawPolygon(p_Vertices, transform);
}
void RenderContext<2>::Polygon(const std::span<const vec2> p_Vertices, const f32 p_X, const f32 p_Y) noexcept
{
    Polygon(p_Vertices, vec2{p_X, p_Y});
}
void RenderContext<2>::Polygon(const std::span<const vec2> p_Vertices, const f32 p_X, const f32 p_Y,
                               const f32 p_Rotation) noexcept
{
    Polygon(p_Vertices, vec2{p_X, p_Y}, p_Rotation);
}
void RenderContext<3>::Polygon(const std::span<const vec3> p_Vertices, const f32 p_X, const f32 p_Y,
                               const f32 p_Z) noexcept
{
    Polygon(p_Vertices, vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Polygon(const std::span<const vec3> p_Vertices, const f32 p_X, const f32 p_Y, const f32 p_Z,
                               const quat &p_Quaternion) noexcept
{
    Polygon(p_Vertices, vec3{p_X, p_Y, p_Z}, p_Quaternion);
}
void RenderContext<3>::Polygon(const std::span<const vec3> p_Vertices, const f32 p_X, const f32 p_Y, const f32 p_Z,
                               const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Polygon(p_Vertices, vec3{p_X, p_Y, p_Z}, quat{{p_XRot, p_YRot, p_ZRot}});
}
void RenderContext<3>::Polygon(const std::span<const vec3> p_Vertices, const vec3 &p_Position,
                               const vec3 &p_Angles) noexcept
{
    Polygon(p_Vertices, p_Position, quat{p_Angles});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle() noexcept
{
    m_Renderer.DrawCircle(m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle(const f32 p_Radius) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeScaleMatrix(vec<N>{p_Radius}) * m_RenderState.back().Transform;
    m_Renderer.DrawCircle(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle(const vec<N> &p_Position) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    m_Renderer.DrawCircle(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle(const vec<N> &p_Position, const f32 p_Radius) noexcept
{
    const mat<N> transform = ONYX::Transform<N>::ComputeTranslationScaleMatrix(p_Position, vec<N>{p_Radius}) *
                             m_RenderState.back().Transform;
    m_Renderer.DrawCircle(transform);
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
    m_Renderer.DrawCircle(transform);
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
    m_Renderer.DrawCircle(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const vec<N> &p_Position, const vec2 &p_Dimensions,
                                                        const rot<N> &p_Rotation) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform =
            Transform2D::ComputeTransform(p_Position, p_Dimensions, p_Rotation) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeTransform(p_Position, vec<N>{p_Dimensions, 1.f}, p_Rotation) *
                    m_RenderState.back().Transform;
    m_Renderer.DrawCircle(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec2{p_XDim, p_YDim});
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawCircle(p_Transform);
}
void RenderContext<2>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec2{p_X, p_Y}, vec2{p_XDim, p_YDim});
}
void RenderContext<2>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_XDim, const f32 p_YDim,
                               const f32 p_Rotation) noexcept
{
    Ellipse(vec2{p_X, p_Y}, vec2{p_XDim, p_YDim}, p_Rotation);
}
void RenderContext<3>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim});
}
void RenderContext<3>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                               const quat &p_Quaternion) noexcept
{
    Ellipse(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim}, p_Quaternion);
}
void RenderContext<3>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                               const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Ellipse(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim}, quat{{p_XRot, p_YRot, p_ZRot}});
}
void RenderContext<3>::Ellipse(const vec3 &p_Position, const vec2 &p_Dimensions, const vec3 &p_Angles) noexcept
{
    Ellipse(p_Position, p_Dimensions, quat{p_Angles});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Line(const vec<N> &p_Start, const vec<N> &p_End,
                                                     const f32 p_Thickness) noexcept
{
    ONYX::Transform<N> t;
    t.Translation = 0.5f * (p_Start + p_End);
    const vec<N> delta = p_End - p_Start;
    if constexpr (N == 2)
        t.Rotation = glm::atan(delta.y, delta.x);
    else
        t.Rotation = quat{{0.f, glm::atan(delta.y, delta.x), glm::atan(delta.z, delta.x)}};
    t.Scale.x = glm::length(delta);
    t.Scale.y = p_Thickness;
    if constexpr (N == 3)
        t.Scale.z = p_Thickness;

    const mat<N> transform = t.ComputeTransform() * m_RenderState.back().Transform;
    if constexpr (N == 2)
        m_Renderer.DrawPrimitive(Primitives2D::GetSquareIndex(), transform);
    else
        m_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), transform);
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

void RenderContext<2>::RoundedLine(const vec2 &p_Start, const vec2 &p_End, const f32 p_Thickness) noexcept
{
    Line(p_Start, p_End, p_Thickness);
    Circle(p_Start, p_Thickness);
    Circle(p_End, p_Thickness);
}
void RenderContext<3>::RoundedLine(const vec3 &p_Start, const vec3 &p_End, const f32 p_Thickness) noexcept
{
    Line(p_Start, p_End, p_Thickness);
    Sphere(p_Start, p_Thickness);
    Sphere(p_End, p_Thickness);
}

void RenderContext<2>::RoundedLineStrip(std::span<const vec2> p_Points, const f32 p_Thickness) noexcept
{
    LineStrip(p_Points, p_Thickness);
    for (const vec2 &point : p_Points)
        Circle(point, p_Thickness);
}
void RenderContext<3>::RoundedLineStrip(std::span<const vec3> p_Points, const f32 p_Thickness) noexcept
{
    LineStrip(p_Points, p_Thickness);
    for (const vec3 &point : p_Points)
        Sphere(point, p_Thickness);
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
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), m_RenderState.back().Transform);
}
void RenderContext<3>::Cube(const f32 p_Size) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(vec3{p_Size}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), transform);
}
void RenderContext<3>::Cube(const vec3 &p_Position) noexcept
{
    const mat4 transform = Transform3D::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), transform);
}
void RenderContext<3>::Cube(const vec3 &p_Position, const f32 p_Size) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, vec3{p_Size}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), transform);
}
void RenderContext<3>::Cube(const vec3 &p_Position, const f32 p_Size, const quat &p_Quaternion) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTransform(p_Position, vec3{p_Size}, p_Quaternion) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), transform);
}
void RenderContext<3>::Cube(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Cube(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Cube(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size) noexcept
{
    Cube(vec3{p_X, p_Y, p_Z}, p_Size);
}
void RenderContext<3>::Cube(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size,
                            const quat &p_Quaternion) noexcept
{
    Cube(vec3{p_X, p_Y, p_Z}, p_Size, p_Quaternion);
}
void RenderContext<3>::Cube(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size, const f32 p_XRot,
                            const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Cube(vec3{p_X, p_Y, p_Z}, p_Size, vec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<3>::Cube(const vec3 &p_Position, const f32 p_Size, const vec3 &p_Angles) noexcept
{
    Cube(p_Position, p_Size, quat{p_Angles});
}

void RenderContext<3>::Cuboid(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), p_Transform);
}
void RenderContext<3>::Cuboid(const vec3 &p_Dimensions) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), transform);
}
void RenderContext<3>::Cuboid(const vec3 &p_Position, const vec3 &p_Dimensions) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, p_Dimensions) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), transform);
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
void RenderContext<3>::Cuboid(const vec3 &p_Position, const vec3 &p_Dimensions, const quat &p_Quaternion) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTransform(p_Position, p_Dimensions, p_Quaternion) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), transform);
}
void RenderContext<3>::Cuboid(const vec3 &p_Position, const vec3 &p_Dimensions, const vec3 &p_Angles) noexcept
{
    Cuboid(p_Position, p_Dimensions, quat{p_Angles});
}
void RenderContext<3>::Cuboid(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                              const f32 p_ZDim, const quat &p_Quaternion) noexcept
{
    Cuboid(vec3{p_X, p_Y, p_Z}, vec3{p_XDim, p_YDim, p_ZDim}, p_Quaternion);
}
void RenderContext<3>::Cuboid(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                              const f32 p_ZDim, const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Cuboid(vec3{p_X, p_Y, p_Z}, vec3{p_XDim, p_YDim, p_ZDim}, quat{{p_XRot, p_YRot, p_ZRot}});
}

void RenderContext<3>::Cylinder() noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), m_RenderState.back().Transform);
}
void RenderContext<3>::Cylinder(const vec2 &p_Dimensions) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(vec3{p_Dimensions.x, p_Dimensions.y, p_Dimensions.x}) *
                           m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), transform);
}
void RenderContext<3>::Cylinder(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), p_Transform);
}
void RenderContext<3>::Cylinder(const vec3 &p_Position, const vec2 &p_Dimensions) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, vec3{p_Dimensions.x, p_Dimensions.y, p_Dimensions.x}) *
        m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), transform);
}
void RenderContext<3>::Cylinder(const f32 p_Radius, const f32 p_Length) noexcept
{
    Cylinder(vec2{p_Radius, p_Length});
}
void RenderContext<3>::Cylinder(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius,
                                const f32 p_Length) noexcept
{
    Cylinder(vec3{p_X, p_Y, p_Z}, vec2{p_Radius, p_Length});
}
void RenderContext<3>::Cylinder(const vec3 &p_Position, const vec2 &p_Dimensions, const quat &p_Quaternion) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTransform(p_Position, vec3{p_Dimensions.x, p_Dimensions.y, p_Dimensions.x}, p_Quaternion) *
        m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), transform);
}
void RenderContext<3>::Cylinder(const vec3 &p_Position, const vec2 &p_Dimensions, const vec3 &p_Angles) noexcept
{
    Cylinder(p_Position, p_Dimensions, quat{p_Angles});
}
void RenderContext<3>::Cylinder(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius, const f32 p_Length,
                                const quat &p_Quaternion) noexcept
{
    Cylinder(vec3{p_X, p_Y, p_Z}, vec2{p_Radius, p_Length}, p_Quaternion);
}
void RenderContext<3>::Cylinder(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius, const f32 p_Length,
                                const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Cylinder(vec3{p_X, p_Y, p_Z}, vec2{p_Radius, p_Length}, quat{{p_XRot, p_YRot, p_ZRot}});
}

void RenderContext<3>::Sphere() noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), m_RenderState.back().Transform);
}
void RenderContext<3>::Sphere(const f32 p_Radius) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(vec3{p_Radius}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), transform);
}
void RenderContext<3>::Sphere(const vec3 &p_Position) noexcept
{
    const mat4 transform = Transform3D::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), transform);
}
void RenderContext<3>::Sphere(const vec3 &p_Position, const f32 p_Radius) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, vec3{p_Radius}) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), transform);
}
void RenderContext<3>::Sphere(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Sphere(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Sphere(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius) noexcept
{
    Sphere(vec3{p_X, p_Y, p_Z}, p_Radius);
}

void RenderContext<3>::Ellipsoid(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), p_Transform);
}
void RenderContext<3>::Ellipsoid(const vec3 &p_Dimensions) noexcept
{
    const mat4 transform = Transform3D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), transform);
}
void RenderContext<3>::Ellipsoid(const vec3 &p_Position, const vec3 &p_Dimensions) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTranslationScaleMatrix(p_Position, p_Dimensions) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), transform);
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
void RenderContext<3>::Ellipsoid(const vec3 &p_Position, const vec3 &p_Dimensions, const quat &p_Quaternion) noexcept
{
    const mat4 transform =
        Transform3D::ComputeTransform(p_Position, p_Dimensions, p_Quaternion) * m_RenderState.back().Transform;
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), transform);
}
void RenderContext<3>::Ellipsoid(const vec3 &p_Position, const vec3 &p_Dimensions, const vec3 &p_Angles) noexcept
{
    Ellipsoid(p_Position, p_Dimensions, quat{p_Angles});
}
void RenderContext<3>::Ellipsoid(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                                 const f32 p_ZDim, const quat &p_Quaternion) noexcept
{
    Ellipsoid(vec3{p_X, p_Y, p_Z}, vec3{p_XDim, p_YDim, p_ZDim}, p_Quaternion);
}
void RenderContext<3>::Ellipsoid(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim,
                                 const f32 p_ZDim, const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Ellipsoid(vec3{p_X, p_Y, p_Z}, vec3{p_XDim, p_YDim, p_ZDim}, quat{{p_XRot, p_YRot, p_ZRot}});
}

void RenderContext<3>::LightColor(const Color &p_Color) noexcept
{
    m_RenderState.back().LightColor = p_Color;
}
void RenderContext<3>::AmbientColor(const Color &p_Color) noexcept
{
    m_Renderer.AmbientColor = p_Color;
}
void RenderContext<3>::AmbientIntensity(const f32 p_Intensity) noexcept
{
    m_Renderer.AmbientColor.RGBA.a = p_Intensity;
}

void RenderContext<3>::DirectionalLight(const ONYX::DirectionalLight &p_Light) noexcept
{
    m_Renderer.AddDirectionalLight(p_Light);
}
void RenderContext<3>::DirectionalLight(const vec3 &p_Direction, const f32 p_Intensity) noexcept
{
    ONYX::DirectionalLight light;
    light.DirectionAndIntensity = vec4{glm::normalize(p_Direction), p_Intensity};
    light.Color = m_RenderState.back().LightColor;
    m_Renderer.AddDirectionalLight(light);
}
void RenderContext<3>::DirectionalLight(const f32 p_DX, const f32 p_DY, const f32 p_DZ, const f32 p_Intensity) noexcept
{
    DirectionalLight(vec3{p_DX, p_DY, p_DZ}, p_Intensity);
}

void RenderContext<3>::PointLight(const ONYX::PointLight &p_Light) noexcept
{
    m_Renderer.AddPointLight(p_Light);
}
void RenderContext<3>::PointLight(const vec3 &p_Position, const f32 p_Radius, const f32 p_Intensity) noexcept
{
    ONYX::PointLight light;
    light.PositionAndIntensity = vec4{p_Position, p_Intensity};
    light.Radius = p_Radius;
    light.Color = m_RenderState.back().LightColor;
    m_Renderer.AddPointLight(light);
}
void RenderContext<3>::PointLight(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius,
                                  const f32 p_Intensity) noexcept
{
    PointLight(vec3{p_X, p_Y, p_Z}, p_Radius, p_Intensity);
}
void RenderContext<3>::PointLight(const f32 p_Radius, const f32 p_Intensity) noexcept
{
    PointLight(vec3{0.f}, p_Radius, p_Intensity);
}

void RenderContext<3>::DiffuseContribution(const f32 p_Contribution) noexcept
{
    m_RenderState.back().Material.DiffuseContribution = p_Contribution;
}
void RenderContext<3>::SpecularContribution(const f32 p_Contribution) noexcept
{
    m_RenderState.back().Material.SpecularContribution = p_Contribution;
}
void RenderContext<3>::SpecularSharpness(const f32 p_Sharpness) noexcept
{
    m_RenderState.back().Material.SpecularSharpness = p_Sharpness;
}

void RenderContext<2>::Fill() noexcept
{
    m_RenderState.back().NoFill = false;
}
void RenderContext<2>::NoFill() noexcept
{
    m_RenderState.back().NoFill = true;
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const KIT::Ref<const Model<N>> &p_Model) noexcept
{
    m_Renderer.DrawMesh(p_Model, m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const KIT::Ref<const Model<N>> &p_Model,
                                                     const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawMesh(p_Model, p_Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const KIT::Ref<const Model<N>> &p_Model,
                                                     const vec<N> &p_Translation) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTranslationMatrix(p_Translation) * m_RenderState.back().Transform;
    m_Renderer.DrawMesh(p_Model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const KIT::Ref<const Model<N>> &p_Model,
                                                     const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTranslationScaleMatrix(p_Translation, p_Scale) * m_RenderState.back().Transform;
    m_Renderer.DrawMesh(p_Model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const KIT::Ref<const Model<N>> &p_Model,
                                                     const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                     const rot<N> &p_Rotation) noexcept
{
    const mat<N> transform =
        ONYX::Transform<N>::ComputeTransform(p_Translation, p_Scale, p_Rotation) * m_RenderState.back().Transform;
    m_Renderer.DrawMesh(p_Model, transform);
}

void RenderContext<2>::Mesh(const KIT::Ref<const Model<2>> &p_Model, const f32 p_X, const f32 p_Y) noexcept
{
    Mesh(p_Model, vec2{p_X, p_Y});
}
void RenderContext<2>::Mesh(const KIT::Ref<const Model<2>> &p_Model, const f32 p_X, const f32 p_Y,
                            const f32 p_Scale) noexcept
{
    Mesh(p_Model, vec2{p_X, p_Y}, vec2{p_Scale});
}
void RenderContext<2>::Mesh(const KIT::Ref<const Model<2>> &p_Model, const f32 p_X, const f32 p_Y, const f32 p_XS,
                            const f32 p_YS) noexcept
{
    Mesh(p_Model, vec2{p_X, p_Y}, vec2{p_XS, p_YS});
}
void RenderContext<2>::Mesh(const KIT::Ref<const Model<2>> &p_Model, const f32 p_X, const f32 p_Y, const f32 p_XS,
                            const f32 p_YS, const f32 p_Angle) noexcept
{
    Mesh(p_Model, vec2{p_X, p_Y}, vec2{p_XS, p_YS}, p_Angle);
}

void RenderContext<3>::Mesh(const KIT::Ref<const Model<3>> &p_Model, const f32 p_X, const f32 p_Y,
                            const f32 p_Z) noexcept
{
    Mesh(p_Model, vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Mesh(const KIT::Ref<const Model<3>> &p_Model, const f32 p_X, const f32 p_Y, const f32 p_Z,
                            const f32 p_Scale) noexcept
{
    Mesh(p_Model, vec3{p_X, p_Y, p_Z}, vec3{p_Scale});
}
void RenderContext<3>::Mesh(const KIT::Ref<const Model<3>> &p_Model, const f32 p_X, const f32 p_Y, const f32 p_Z,
                            const f32 p_XS, const f32 p_YS, const f32 p_ZS) noexcept
{
    Mesh(p_Model, vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS});
}
void RenderContext<3>::Mesh(const KIT::Ref<const Model<3>> &p_Model, const f32 p_X, const f32 p_Y, const f32 p_Z,
                            const f32 p_XS, const f32 p_YS, const f32 p_ZS, const quat &p_Quatetnion) noexcept
{
    Mesh(p_Model, vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, p_Quatetnion);
}
void RenderContext<3>::Mesh(const KIT::Ref<const Model<3>> &p_Model, const f32 p_X, const f32 p_Y, const f32 p_Z,
                            const f32 p_XS, const f32 p_YS, const f32 p_ZS, const vec3 &p_Angles) noexcept
{
    Mesh(p_Model, vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, quat{p_Angles});
}
void RenderContext<3>::Mesh(const KIT::Ref<const Model<3>> &p_Model, const f32 p_X, const f32 p_Y, const f32 p_Z,
                            const f32 p_XS, const f32 p_YS, const f32 p_ZS, const f32 p_XRot, const f32 p_YRot,
                            const f32 p_ZRot) noexcept
{
    Mesh(p_Model, vec3{p_X, p_Y, p_Z}, vec3{p_XS, p_YS, p_ZS}, quat{{p_XRot, p_YRot, p_ZRot}});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Push() noexcept
{
    m_RenderState.push_back(m_RenderState.back());
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::PushAndClear() noexcept
{
    m_RenderState.push_back(RenderState<N>{});
    if constexpr (N == 3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Pop() noexcept
{
    KIT_ASSERT(m_RenderState.size() > 1, "For every push, there must be a pop");
    m_RenderState.pop_back();
}

void RenderContext<2>::Alpha(const f32 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = p_Alpha;
}
void RenderContext<2>::Alpha(const u8 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = static_cast<f32>(p_Alpha) / 255.f;
}
void RenderContext<2>::Alpha(const u32 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = static_cast<f32>(p_Alpha) / 255.f;
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Fill(const Color &p_Color) noexcept
{
    m_RenderState.back().Material.Color = p_Color;
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

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Material(const MaterialData<N> &p_Material) noexcept
{
    m_RenderState.back().Material = p_Material;
}

ONYX_DIMENSION_TEMPLATE const RenderState<N> &IRenderContext<N>::GetState() const noexcept
{
    return m_RenderState.back();
}
ONYX_DIMENSION_TEMPLATE RenderState<N> &IRenderContext<N>::GetState() noexcept
{
    return m_RenderState.back();
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::SetState(const RenderState<N> &p_State) noexcept
{
    m_RenderState.back() = p_State;
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::SetCurrentTransform(const mat<N> &p_Transform) noexcept
{
    m_RenderState.back().Transform = p_Transform;
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::SetCurrentAxes(const mat<N> &p_Axes) noexcept
{
    m_RenderState.back().Axes = p_Axes;
    if constexpr (N == 3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Render(const VkCommandBuffer p_Commandbuffer) noexcept
{
    m_Renderer.Render(p_Commandbuffer);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Axes(const f32 p_Thickness, const f32 p_Size) noexcept
{
    if constexpr (N == 2)
    {
        const vec2 xLeft = vec2{-p_Size, 0.f};
        const vec2 xRight = vec2{p_Size, 0.f};

        const vec2 yDown = vec2{0.f, -p_Size};
        const vec2 yUp = vec2{0.f, p_Size};

        Line(xLeft, xRight, p_Thickness);
        Line(yDown, yUp, p_Thickness);
    }
    else
    {
        const vec3 xLeft = vec3{-p_Size, 0.f, 0.f};
        const vec3 xRight = vec3{p_Size, 0.f, 0.f};

        const vec3 yDown = vec3{0.f, -p_Size, 0.f};
        const vec3 yUp = vec3{0.f, p_Size, 0.f};

        const vec3 zBack = vec3{0.f, 0.f, -p_Size};
        const vec3 zFront = vec3{0.f, 0.f, p_Size};

        Line(xLeft, xRight, p_Thickness);
        Line(yDown, yUp, p_Thickness);
        Line(zBack, zFront, p_Thickness);
    }
}

vec2 RenderContext<2>::GetMouseCoordinates() const noexcept
{
    const vec2 mpos = Input::GetMousePosition(m_Window);
    mat4 axes = transform3ToTransform4(m_RenderState.back().Axes);
    ApplyCoordinateSystem(axes);
    return glm::inverse(axes) * vec4{mpos, 1.f, 1.f};
}
vec3 RenderContext<3>::GetMouseCoordinates(const f32 p_Depth) const noexcept
{
    const vec2 mpos = Input::GetMousePosition(m_Window);
    const mat4 &axes = m_RenderState.back().Axes;
    if (!m_RenderState.back().HasProjection)
        return m_RenderState.back().InverseAxes * vec4{mpos, p_Depth, 1.f};

    const vec4 clip = glm::inverse(m_RenderState.back().Projection * axes) * vec4{mpos, p_Depth, 1.f};
    return vec3{clip} / clip.w;
}

void RenderContext<3>::Projection(const mat4 &p_Projection) noexcept
{
    m_RenderState.back().Projection = p_Projection;
    m_RenderState.back().HasProjection = true;
}
void RenderContext<3>::Perspective(const f32 p_FieldOfView, const f32 p_Near, const f32 p_Far) noexcept
{
    auto &projection = m_RenderState.back().Projection;
    const f32 invHalfPov = 1.f / glm::tan(0.5f * p_FieldOfView);

    projection = mat4{0.f};
    projection[0][0] = invHalfPov; // Aspect applied in view
    projection[1][1] = invHalfPov;
    projection[2][2] = p_Far / (p_Far - p_Near);
    projection[2][3] = 1.f;
    projection[3][2] = p_Far * p_Near / (p_Near - p_Far);
    m_RenderState.back().HasProjection = true;
}

void RenderContext<3>::Orthographic() noexcept
{
    m_RenderState.back().Projection = mat4{1.f};
    m_RenderState.back().HasProjection = false;
}

template class IRenderContext<2>;
template class IRenderContext<3>;

} // namespace ONYX