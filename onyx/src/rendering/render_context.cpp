#include "core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/app/input.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/app/window.hpp"

#include "kit/utilities/math.hpp"

namespace ONYX
{
template <Dimension D>
IRenderContext<D>::IRenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : m_Renderer(p_RenderPass, &m_RenderState), m_Window(p_Window)
{
    m_RenderState.push_back(RenderState<D>{});
    // All axes transformation come "from the right", and axes coordinate system adjustements must come "from the left",
    // so it is actually fine to have the axes starting as the current coordinate system adjustements. Can only be done
    // in 3D, because transformations may involve some axis that dont exist in 2D. This coordinate system adjustements
    // is apply later for 2D cases, when eventually the mat3's become mat4's
    if constexpr (D == D3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}

template <Dimension D> void IRenderContext<D>::FlushState() noexcept
{
    KIT_ASSERT(m_RenderState.size() == 1, "For every push, there must be a pop");
    m_RenderState[0] = RenderState<D>{};
    if constexpr (D == D3)
        ApplyCoordinateSystem(m_RenderState[0].Axes, &m_RenderState[0].InverseAxes);
}
template <Dimension D> void IRenderContext<D>::FlushState(const Color &p_Color) noexcept
{
    FlushState();
    m_Window->BackgroundColor = p_Color;
}

template <Dimension D> void IRenderContext<D>::FlushDrawData() noexcept
{
    m_Renderer.Flush();
}

template <Dimension D> void IRenderContext<D>::Flush() noexcept
{
    FlushDrawData();
    FlushState();
}
template <Dimension D> void IRenderContext<D>::Flush(const Color &p_Color) noexcept
{
    FlushDrawData();
    FlushState(p_Color);
}

template <Dimension D> void IRenderContext<D>::Transform(const mat<D> &p_Transform) noexcept
{
    m_RenderState.back().Transform = p_Transform * m_RenderState.back().Transform;
}
template <Dimension D>
void IRenderContext<D>::Transform(const vec<D> &p_Translation, const vec<D> &p_Scale, const rot<D> &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform<D>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
template <Dimension D>
void IRenderContext<D>::Transform(const vec<D> &p_Translation, const f32 p_Scale, const rot<D> &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform<D>::ComputeTransform(p_Translation, vec<D>{p_Scale}, p_Rotation));
}
void RenderContext<D3>::Transform(const vec3 &p_Translation, const vec3 &p_Scale, const vec3 &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform<D3>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
void RenderContext<D3>::Transform(const vec3 &p_Translation, const f32 p_Scale, const vec3 &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform<D3>::ComputeTransform(p_Translation, vec3{p_Scale}, p_Rotation));
}

template <Dimension D> void IRenderContext<D>::TransformAxes(const mat<D> &p_Axes) noexcept
{
    m_RenderState.back().Axes *= p_Axes;
    if constexpr (D == D3)
        m_RenderState.back().InverseAxes = glm::inverse(m_RenderState.back().Axes);
}
template <Dimension D>
void IRenderContext<D>::TransformAxes(const vec<D> &p_Translation, const vec<D> &p_Scale,
                                      const rot<D> &p_Rotation) noexcept
{
    m_RenderState.back().Axes *= ONYX::Transform<D>::ComputeAxesTransform(p_Translation, p_Scale, p_Rotation);
    if constexpr (D == D3)
        m_RenderState.back().InverseAxes =
            ONYX::Transform<D>::ComputeTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation)) *
            m_RenderState.back().InverseAxes;
}
template <Dimension D>
void IRenderContext<D>::TransformAxes(const vec<D> &p_Translation, const f32 p_Scale, const rot<D> &p_Rotation) noexcept
{
    TransformAxes(p_Translation, vec<D>{p_Scale}, p_Rotation);
}
void RenderContext<D3>::TransformAxes(const vec3 &p_Translation, const vec3 &p_Scale, const vec3 &p_Rotation) noexcept
{
    TransformAxes(ONYX::Transform<D3>::ComputeAxesTransform(p_Translation, p_Scale, p_Rotation));
}
void RenderContext<D3>::TransformAxes(const vec3 &p_Translation, const f32 p_Scale, const vec3 &p_Rotation) noexcept
{
    TransformAxes(ONYX::Transform<D3>::ComputeAxesTransform(p_Translation, vec3{p_Scale}, p_Rotation));
}

template <Dimension D> void IRenderContext<D>::Translate(const vec<D> &p_Translation) noexcept
{
    ONYX::Transform<D>::TranslateExtrinsic(m_RenderState.back().Transform, p_Translation);
}
void RenderContext<D2>::Translate(const f32 p_X, const f32 p_Y) noexcept
{
    Translate(vec2{p_X, p_Y});
}
void RenderContext<D3>::Translate(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Translate(vec3{p_X, p_Y, p_Z});
}

template <Dimension D> void IRenderContext<D>::Scale(const vec<D> &p_Scale) noexcept
{
    ONYX::Transform<D>::ScaleExtrinsic(m_RenderState.back().Transform, p_Scale);
}
template <Dimension D> void IRenderContext<D>::Scale(const f32 p_Scale) noexcept
{
    Scale(vec<D>{p_Scale});
}
void RenderContext<D2>::Scale(const f32 p_X, const f32 p_Y) noexcept
{
    Scale(vec2{p_X, p_Y});
}
void RenderContext<D3>::Scale(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Scale(vec3{p_X, p_Y, p_Z});
}

template <Dimension D> void IRenderContext<D>::TranslateX(const f32 p_X) noexcept
{
    ONYX::Transform<D>::TranslateExtrinsic(m_RenderState.back().Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::TranslateY(const f32 p_Y) noexcept
{
    ONYX::Transform<D>::TranslateExtrinsic(m_RenderState.back().Transform, 1, p_Y);
}
void RenderContext<D3>::TranslateZ(const f32 p_Z) noexcept
{
    ONYX::Transform<D3>::TranslateExtrinsic(m_RenderState.back().Transform, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::ScaleX(const f32 p_X) noexcept
{
    ONYX::Transform<D>::ScaleExtrinsic(m_RenderState.back().Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::ScaleY(const f32 p_Y) noexcept
{
    ONYX::Transform<D>::ScaleExtrinsic(m_RenderState.back().Transform, 1, p_Y);
}
void RenderContext<D3>::ScaleZ(const f32 p_Z) noexcept
{
    ONYX::Transform<D3>::ScaleExtrinsic(m_RenderState.back().Transform, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::TranslateXAxis(const f32 p_X) noexcept
{
    ONYX::Transform<D>::TranslateIntrinsic(m_RenderState.back().Axes, 0, p_X);
    if constexpr (D == D3)
        ONYX::Transform<D>::TranslateExtrinsic(m_RenderState.back().InverseAxes, 0, -p_X);
}
template <Dimension D> void IRenderContext<D>::TranslateYAxis(const f32 p_Y) noexcept
{
    ONYX::Transform<D>::TranslateIntrinsic(m_RenderState.back().Axes, 1, p_Y);
    if constexpr (D == D3)
        ONYX::Transform<D>::TranslateExtrinsic(m_RenderState.back().InverseAxes, 1, -p_Y);
}
void RenderContext<D3>::TranslateZAxis(const f32 p_Z) noexcept
{
    ONYX::Transform<D3>::TranslateIntrinsic(m_RenderState.back().Axes, 2, p_Z);
    ONYX::Transform<D3>::TranslateExtrinsic(m_RenderState.back().InverseAxes, 2, -p_Z);
}

template <Dimension D> void IRenderContext<D>::ScaleXAxis(const f32 p_X) noexcept
{
    ONYX::Transform<D>::ScaleIntrinsic(m_RenderState.back().Axes, 0, p_X);
    if constexpr (D == D3)
        ONYX::Transform<D>::ScaleExtrinsic(m_RenderState.back().InverseAxes, 0, 1.f / p_X);
}
template <Dimension D> void IRenderContext<D>::ScaleYAxis(const f32 p_Y) noexcept
{
    ONYX::Transform<D>::ScaleIntrinsic(m_RenderState.back().Axes, 1, p_Y);
    if constexpr (D == D3)
        ONYX::Transform<D>::ScaleExtrinsic(m_RenderState.back().InverseAxes, 1, 1.f / p_Y);
}
void RenderContext<D3>::ScaleZAxis(const f32 p_Z) noexcept
{
    ONYX::Transform<D3>::ScaleIntrinsic(m_RenderState.back().Axes, 2, p_Z);
    ONYX::Transform<D3>::ScaleExtrinsic(m_RenderState.back().InverseAxes, 2, 1.f / p_Z);
}

template <Dimension D> void IRenderContext<D>::KeepWindowAspect() noexcept
{
    // Scaling down the axes means "enlarging" their extent, that is why the inverse is used
    const f32 aspect = 1.f / m_Window->GetScreenAspect();
    if constexpr (D == D2)
        ScaleAxes(vec2{aspect, 1.f});
    else
        ScaleAxes(vec3{aspect, 1.f, 1.f});
}

template <Dimension D> void IRenderContext<D>::TranslateAxes(const vec<D> &p_Translation) noexcept
{
    ONYX::Transform<D>::TranslateIntrinsic(m_RenderState.back().Axes, p_Translation);
    if constexpr (D == D3)
        ONYX::Transform<D>::TranslateExtrinsic(m_RenderState.back().InverseAxes, -p_Translation);
}
void RenderContext<D2>::TranslateAxes(const f32 p_X, const f32 p_Y) noexcept
{
    TranslateAxes(vec2{p_X, p_Y});
}
void RenderContext<D3>::TranslateAxes(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    TranslateAxes(vec3{p_X, p_Y, p_Z});
}

template <Dimension D> void IRenderContext<D>::ScaleAxes(const vec<D> &p_Scale) noexcept
{
    ONYX::Transform<D>::ScaleIntrinsic(m_RenderState.back().Axes, p_Scale);
    if constexpr (D == D3)
        ONYX::Transform<D>::ScaleExtrinsic(m_RenderState.back().InverseAxes, 1.f / p_Scale);
}
template <Dimension D> void IRenderContext<D>::ScaleAxes(const f32 p_Scale) noexcept
{
    ScaleAxes(vec<D>{p_Scale});
}
void RenderContext<D2>::ScaleAxes(const f32 p_X, const f32 p_Y) noexcept
{
    ScaleAxes(vec2{p_X, p_Y});
}
void RenderContext<D3>::ScaleAxes(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    ScaleAxes(vec3{p_X, p_Y, p_Z});
}

void RenderContext<D2>::Rotate(const f32 p_Angle) noexcept
{
    ONYX::Transform<D2>::RotateExtrinsic(m_RenderState.back().Transform, p_Angle);
}

void RenderContext<D3>::Rotate(const quat &p_Quaternion) noexcept
{
    ONYX::Transform<D3>::RotateExtrinsic(m_RenderState.back().Transform, p_Quaternion);
}
void RenderContext<D3>::Rotate(const f32 p_Angle, const vec3 &p_Axis) noexcept
{
    Rotate(glm::angleAxis(p_Angle, p_Axis));
}
void RenderContext<D3>::Rotate(const vec3 &p_Angles) noexcept
{
    Rotate(glm::quat(p_Angles));
}
void RenderContext<D3>::Rotate(const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Rotate(vec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<D3>::RotateX(const f32 p_Angle) noexcept
{
    Rotate(vec3{p_Angle, 0.f, 0.f});
}
void RenderContext<D3>::RotateY(const f32 p_Angle) noexcept
{
    Rotate(vec3{0.f, p_Angle, 0.f});
}
void RenderContext<D3>::RotateZ(const f32 p_Angle) noexcept
{
    Rotate(vec3{0.f, 0.f, p_Angle});
}

void RenderContext<D2>::RotateAxes(const f32 p_Angle) noexcept
{
    ONYX::Transform<D2>::RotateIntrinsic(m_RenderState.back().Axes, p_Angle);
}
void RenderContext<D3>::RotateAxes(const quat &p_Quaternion) noexcept
{
    ONYX::Transform<D3>::RotateIntrinsic(m_RenderState.back().Axes, p_Quaternion);
    ONYX::Transform<D3>::RotateExtrinsic(m_RenderState.back().InverseAxes, glm::conjugate(p_Quaternion));
}
void RenderContext<D3>::RotateAxes(const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    RotateAxes(vec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<D3>::RotateAxes(const f32 p_Angle, const vec3 &p_Axis) noexcept
{
    RotateAxes(glm::angleAxis(p_Angle, p_Axis));
}
void RenderContext<D3>::RotateAxes(const vec3 &p_Angles) noexcept
{
    RotateAxes(glm::quat(p_Angles));
}

// This could be optimized a bit
void RenderContext<D3>::RotateXAxis(const f32 p_Angle) noexcept
{
    RotateAxes(vec3{p_Angle, 0.f, 0.f});
}
void RenderContext<D3>::RotateYAxis(const f32 p_Angle) noexcept
{
    RotateAxes(vec3{0.f, p_Angle, 0.f});
}
void RenderContext<D3>::RotateZAxis(const f32 p_Angle) noexcept
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

template <Dimension D> void IRenderContext<D>::Triangle() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D>::GetTriangleIndex());
}
template <Dimension D> void IRenderContext<D>::Triangle(const mat<D> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D>::GetTriangleIndex());
}

template <Dimension D> void IRenderContext<D>::Square() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D>::GetSquareIndex());
}
template <Dimension D> void IRenderContext<D>::Square(const mat<D> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D>::GetSquareIndex());
}

template <Dimension D> void IRenderContext<D>::NGon(const u32 p_Sides) noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D>::GetNGonIndex(p_Sides));
}
template <Dimension D> void IRenderContext<D>::NGon(const u32 p_Sides, const mat<D> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D>::GetNGonIndex(p_Sides));
}

template <Dimension D> void IRenderContext<D>::Polygon(const std::span<const vec<D>> p_Vertices) noexcept
{
    m_Renderer.DrawPolygon(m_RenderState.back().Transform, p_Vertices);
}
template <Dimension D>
void IRenderContext<D>::Polygon(const std::span<const vec<D>> p_Vertices, const mat<D> &p_Transform) noexcept
{
    m_Renderer.DrawPolygon(p_Transform * m_RenderState.back().Transform, p_Vertices);
}

template <Dimension D> void IRenderContext<D>::Circle() noexcept
{
    m_Renderer.DrawCircle(m_RenderState.back().Transform);
}
template <Dimension D> void IRenderContext<D>::Circle(const mat<D> &p_Transform) noexcept
{
    m_Renderer.DrawCircle(p_Transform * m_RenderState.back().Transform);
}
template <Dimension D>
void IRenderContext<D>::Circle(const f32 p_LowerAngle, const f32 p_UpperAngle, const f32 p_Hollowness) noexcept
{
    m_Renderer.DrawCircle(m_RenderState.back().Transform, p_LowerAngle, p_UpperAngle, p_Hollowness);
}
template <Dimension D>
void IRenderContext<D>::Circle(const f32 p_LowerAngle, const f32 p_UpperAngle, const mat<D> &p_Transform) noexcept
{
    m_Renderer.DrawCircle(p_Transform * m_RenderState.back().Transform, p_LowerAngle, p_UpperAngle);
}
template <Dimension D>
void IRenderContext<D>::Circle(const f32 p_LowerAngle, const f32 p_UpperAngle, const f32 p_Hollowness,
                               const mat<D> &p_Transform) noexcept
{
    m_Renderer.DrawCircle(p_Transform * m_RenderState.back().Transform, p_LowerAngle, p_UpperAngle, p_Hollowness);
}

template <Dimension D>
static void drawIntrinsicCircle(Renderer<D> &p_Renderer, mat<D> p_Transform, const vec<D> &p_Position,
                                const f32 p_Diameter, const f32 p_LowerAngle, const f32 p_UpperAngle,
                                const u8 p_Flags) noexcept
{
    ONYX::Transform<D>::TranslateIntrinsic(p_Transform, p_Position);
    if constexpr (D == D2)
        ONYX::Transform<D>::ScaleIntrinsic(p_Transform, vec<D>{p_Diameter});
    else
        ONYX::Transform<D>::ScaleIntrinsic(p_Transform, vec<D>{p_Diameter, p_Diameter, 1.f});
    p_Renderer.DrawCircle(p_Transform, p_LowerAngle, p_UpperAngle, 0.f, p_Flags);
}
template <Dimension D>
static void drawIntrinsicCircle(Renderer<D> &p_Renderer, mat<D> p_Transform, const vec<D> &p_Position,
                                const f32 p_LowerAngle, const f32 p_UpperAngle, const u8 p_Flags) noexcept
{
    ONYX::Transform<D>::TranslateIntrinsic(p_Transform, p_Position);
    p_Renderer.DrawCircle(p_Transform, p_LowerAngle, p_UpperAngle, 0.f, p_Flags);
}

static void drawIntrinsicSphere(Renderer<D3> &p_Renderer, mat4 p_Transform, const vec3 &p_Position,
                                const f32 p_Diameter, const u8 p_Flags) noexcept
{
    ONYX::Transform<D3>::TranslateIntrinsic(p_Transform, p_Position);
    ONYX::Transform<D3>::ScaleIntrinsic(p_Transform, vec3{p_Diameter});
    p_Renderer.DrawPrimitive(p_Transform, Primitives<D3>::GetSphereIndex(), p_Flags);
}
static void drawIntrinsicSphere(Renderer<D3> &p_Renderer, mat4 p_Transform, const vec3 &p_Position,
                                const u8 p_Flags) noexcept
{
    ONYX::Transform<D3>::TranslateIntrinsic(p_Transform, p_Position);
    p_Renderer.DrawPrimitive(p_Transform, Primitives<D3>::GetSphereIndex(), p_Flags);
}

template <Dimension D>
static void drawStadiumMoons(Renderer<D> &p_Renderer, const mat<D> &p_Transform, const u8 p_Flags,
                             const f32 p_Length = 1.f, const f32 p_Diameter = 1.f) noexcept
{
    vec<D> pos{0.f};
    pos.x = -0.5f * p_Length;
    drawIntrinsicCircle<D>(p_Renderer, p_Transform, pos, p_Diameter, glm::radians(90.f), glm::radians(270.f), p_Flags);
    pos.x = -pos.x;
    drawIntrinsicCircle<D>(p_Renderer, p_Transform, pos, p_Diameter, glm::radians(-90.f), glm::radians(90.f), p_Flags);
}

template <Dimension D>
static void drawStadium(Renderer<D> &p_Renderer, const mat<D> &p_Transform, const u8 p_Flags) noexcept
{
    p_Renderer.DrawPrimitive(p_Transform, Primitives<D>::GetSquareIndex(), p_Flags);
    drawStadiumMoons<D>(p_Renderer, p_Transform, p_Flags);
}
template <Dimension D>
static void drawStadium(Renderer<D> &p_Renderer, const mat<D> &p_Transform, const f32 p_Length, const f32 p_Diameter,
                        const u8 p_Flags) noexcept
{
    mat<D> transform = p_Transform;
    ONYX::Transform<D>::ScaleIntrinsic(transform, 0, p_Length);
    ONYX::Transform<D>::ScaleIntrinsic(transform, 1, p_Diameter);
    p_Renderer.DrawPrimitive(transform, Primitives<D>::GetSquareIndex(), p_Flags);

    drawStadiumMoons<D>(p_Renderer, p_Transform, p_Flags, p_Length, p_Diameter);
}

// Used for compound shapes when it is not enough to scale the overall shape to get a nice outline
template <typename F1, typename F2>
static void resolveDrawCallWithFlagsBasedOnState(F1 &&p_FillDraw, F2 &&p_OutlineDraw, const bool p_Fill,
                                                 const bool p_Outline) noexcept
{
    if (p_Fill)
    {
        if (p_Outline)
        {
            p_FillDraw(DrawFlags_DoStencilWriteDoFill);
            p_OutlineDraw(DrawFlags_DoStencilTestNoFill);
        }
        else
            p_FillDraw(DrawFlags_NoStencilWriteDoFill);
    }
    else if (p_Outline)
    {
        p_FillDraw(DrawFlags_DoStencilWriteNoFill);
        p_OutlineDraw(DrawFlags_DoStencilTestNoFill);
    }
}

template <Dimension D> void IRenderContext<D>::Stadium() noexcept
{
    const mat<D> &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform](const u8 p_Flags) { drawStadium(m_Renderer, transform, p_Flags); };
    const auto outline = [this, &transform](const u8 p_Flags) {
        const f32 diameter = 1.f + m_RenderState.back().OutlineWidth;
        drawStadium(m_Renderer, transform, 1.f, diameter, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D> void IRenderContext<D>::Stadium(const mat<D> &p_Transform) noexcept
{
    const mat<D> transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform](const u8 p_Flags) { drawStadium(m_Renderer, transform, p_Flags); };
    const auto outline = [this, &transform](const u8 p_Flags) {
        const f32 diameter = 1.f + m_RenderState.back().OutlineWidth;
        drawStadium(m_Renderer, transform, 1.f, diameter, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

template <Dimension D> void IRenderContext<D>::Stadium(const f32 p_Length, const f32 p_Radius) noexcept
{
    const mat<D> &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform, p_Length, p_Radius](const u8 p_Flags) {
        drawStadium(m_Renderer, transform, p_Length, 2.f * p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, p_Length, p_Radius](const u8 p_Flags) {
        const f32 diameter = 2.f * p_Radius + m_RenderState.back().OutlineWidth;
        drawStadium(m_Renderer, transform, p_Length, diameter, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D>
void IRenderContext<D>::Stadium(const f32 p_Length, const f32 p_Radius, const mat<D> &p_Transform) noexcept
{
    const mat<D> transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform, p_Length, p_Radius](const u8 p_Flags) {
        drawStadium(m_Renderer, transform, p_Length, 2.f * p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, p_Length, p_Radius](const u8 p_Flags) {
        const f32 diameter = 2.f * p_Radius + m_RenderState.back().OutlineWidth;
        drawStadium(m_Renderer, transform, p_Length, diameter, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

template <Dimension D>
static void drawRoundedSquareEdges(Renderer<D> &p_Renderer, const mat<D> &p_Transform, const u8 p_Flags,
                                   const vec2 &p_Dimensions = vec2{1.f}, const f32 p_Radius = 0.5f) noexcept
{
    const vec2 halfDims = 0.5f * p_Dimensions;
    const vec2 paddedDims = halfDims + 0.5f * p_Radius;
    const f32 diameter = 2.f * p_Radius;

    vec<D> pos;
    if constexpr (D == D2)
        pos = halfDims;
    else
        pos = vec3{halfDims.x, halfDims.y, 0.f};
    for (u32 i = 0; i < 4; ++i)
    {
        mat<D> transform = p_Transform;
        const u32 index1 = i % 2;
        const u32 index2 = 1 - index1;
        const f32 dim = i < 2 ? paddedDims[index1] : -paddedDims[index1];
        ONYX::Transform<D>::TranslateIntrinsic(transform, index1, dim);
        ONYX::Transform<D>::ScaleIntrinsic(transform, index1, p_Radius);
        ONYX::Transform<D>::ScaleIntrinsic(transform, index2, p_Dimensions[index2]);
        p_Renderer.DrawPrimitive(transform, Primitives<D>::GetSquareIndex(), p_Flags);

        const f32 angle = i * glm::half_pi<f32>();
        drawIntrinsicCircle<D>(p_Renderer, p_Transform, pos, diameter, angle, angle + glm::half_pi<f32>(), p_Flags);
        pos[index1] = -pos[index1];
    }
}

template <Dimension D>
static void drawRoundedSquare(Renderer<D> &p_Renderer, const mat<D> &p_Transform, const u8 p_Flags) noexcept
{
    p_Renderer.DrawPrimitive(p_Transform, Primitives<D>::GetSquareIndex(), p_Flags);
    drawRoundedSquareEdges<D>(p_Renderer, p_Transform, p_Flags);
}
template <Dimension D>
static void drawRoundedSquare(Renderer<D> &p_Renderer, const mat<D> &p_Transform, const vec2 &p_Dimensions,
                              const f32 p_Radius, const u8 p_Flags) noexcept
{
    mat<D> transform = p_Transform;
    ONYX::Transform<D>::ScaleIntrinsic(transform, 0, p_Dimensions.x);
    ONYX::Transform<D>::ScaleIntrinsic(transform, 1, p_Dimensions.y);
    p_Renderer.DrawPrimitive(transform, Primitives<D>::GetSquareIndex(), p_Flags);

    drawRoundedSquareEdges<D>(p_Renderer, p_Transform, p_Flags, p_Dimensions, p_Radius);
}

template <Dimension D> void IRenderContext<D>::RoundedSquare() noexcept
{
    const mat<D> &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform](const u8 p_Flags) { drawRoundedSquare(m_Renderer, transform, p_Flags); };
    const auto outline = [this, &transform](const u8 p_Flags) {
        const f32 radius = 0.5f + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedSquare(m_Renderer, transform, vec2{1.f}, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D> void IRenderContext<D>::RoundedSquare(const mat<D> &p_Transform) noexcept
{
    const mat<D> transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform](const u8 p_Flags) { drawRoundedSquare(m_Renderer, transform, p_Flags); };
    const auto outline = [this, &transform](const u8 p_Flags) {
        const f32 radius = 0.5f + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedSquare(m_Renderer, transform, vec2{1.f}, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D> void IRenderContext<D>::RoundedSquare(const vec2 &p_Dimensions, const f32 p_Radius) noexcept
{
    const mat<D> &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform, &p_Dimensions, p_Radius](const u8 p_Flags) {
        drawRoundedSquare(m_Renderer, transform, p_Dimensions, p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, &p_Dimensions, p_Radius](const u8 p_Flags) {
        const f32 radius = p_Radius + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedSquare(m_Renderer, transform, p_Dimensions, radius + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D>
void IRenderContext<D>::RoundedSquare(const vec2 &p_Dimensions, const f32 p_Radius, const mat<D> &p_Transform) noexcept
{
    const mat<D> transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform, &p_Dimensions, p_Radius](const u8 p_Flags) {
        drawRoundedSquare(m_Renderer, transform, p_Dimensions, p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, &p_Dimensions, p_Radius](const u8 p_Flags) {
        const f32 radius = p_Radius + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedSquare(m_Renderer, transform, p_Dimensions, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D>
void IRenderContext<D>::RoundedSquare(const f32 p_Width, const f32 p_Height, const f32 p_Radius) noexcept
{
    RoundedSquare(vec2{p_Width, p_Height}, p_Radius);
}
template <Dimension D>
void IRenderContext<D>::RoundedSquare(const f32 p_Width, const f32 p_Height, const f32 p_Radius,
                                      const mat<D> &p_Transform) noexcept
{
    RoundedSquare(vec2{p_Width, p_Height}, p_Radius, p_Transform);
}

template <Dimension D>
void IRenderContext<D>::Line(const vec<D> &p_Start, const vec<D> &p_End, const f32 p_Thickness) noexcept
{
    ONYX::Transform<D> t;
    t.Translation = 0.5f * (p_Start + p_End);
    const vec<D> delta = p_End - p_Start;
    if constexpr (D == D2)
        t.Rotation = glm::atan(delta.y, delta.x);
    else
        t.Rotation = quat{{0.f, glm::atan(delta.y, delta.x), glm::atan(delta.z, delta.x)}};
    t.Scale.x = glm::length(delta);
    t.Scale.y = p_Thickness;
    if constexpr (D == D3)
        t.Scale.z = p_Thickness;

    const mat<D> transform = m_RenderState.back().Transform * t.ComputeTransform();
    if constexpr (D == D2)
        m_Renderer.DrawPrimitive(transform, Primitives<D2>::GetSquareIndex());
    else
        m_Renderer.DrawPrimitive(transform, Primitives<D3>::GetCylinderIndex());
}

void RenderContext<D2>::Line(const f32 p_X1, const f32 p_Y1, const f32 p_X2, const f32 p_Y2,
                             const f32 p_Thickness) noexcept
{
    Line(vec2{p_X1, p_Y1}, vec2{p_X2, p_Y2}, p_Thickness);
}
void RenderContext<D3>::Line(const f32 p_X1, const f32 p_Y1, const f32 p_Z1, const f32 p_X2, const f32 p_Y2,
                             const f32 p_Z2, const f32 p_Thickness) noexcept
{
    Line(vec3{p_X1, p_Y1, p_Z1}, vec3{p_X2, p_Y2, p_Z2}, p_Thickness);
}

template <Dimension D>
void IRenderContext<D>::LineStrip(std::span<const vec<D>> p_Points, const f32 p_Thickness) noexcept
{
    KIT_ASSERT(p_Points.size() > 1, "A line strip must have at least two points");
    for (u32 i = 0; i < p_Points.size() - 1; ++i)
        Line(p_Points[i], p_Points[i + 1], p_Thickness);
}

void RenderContext<D2>::RoundedLine(const vec2 &p_Start, const vec2 &p_End, const f32 p_Thickness) noexcept
{
    const vec2 delta = p_End - p_Start;
    mat3 transform = m_RenderState.back().Transform;
    ONYX::Transform<D2>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    ONYX::Transform<D2>::RotateIntrinsic(transform, glm::atan(delta.y, delta.x));

    Stadium(glm::length(delta), p_Thickness, transform);
}
void RenderContext<D3>::RoundedLine(const vec3 &p_Start, const vec3 &p_End, const f32 p_Thickness) noexcept
{
    const vec3 delta = p_End - p_Start;
    mat4 transform = m_RenderState.back().Transform;
    ONYX::Transform<D3>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    ONYX::Transform<D3>::RotateIntrinsic(transform,
                                         quat{{0.f, glm::atan(delta.y, delta.x), glm::atan(delta.z, delta.x)}});
    Capsule(glm::length(delta), p_Thickness, transform);
}

void RenderContext<D2>::RoundedLine(const f32 p_X1, const f32 p_Y1, const f32 p_X2, const f32 p_Y2,
                                    const f32 p_Thickness) noexcept
{
    RoundedLine(vec2{p_X1, p_Y1}, vec2{p_X2, p_Y2}, p_Thickness);
}
void RenderContext<D3>::RoundedLine(const f32 p_X1, const f32 p_Y1, const f32 p_Z1, const f32 p_X2, const f32 p_Y2,
                                    const f32 p_Z2, const f32 p_Thickness) noexcept
{
    RoundedLine(vec3{p_X1, p_Y1, p_Z1}, vec3{p_X2, p_Y2, p_Z2}, p_Thickness);
}

void RenderContext<D3>::Cube() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D3>::GetCubeIndex());
}
void RenderContext<D3>::Cube(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D3>::GetCubeIndex());
}

void RenderContext<D3>::Cylinder() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D3>::GetCylinderIndex());
}
void RenderContext<D3>::Cylinder(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D3>::GetCylinderIndex());
}

void RenderContext<D3>::Sphere() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D3>::GetSphereIndex());
}
void RenderContext<D3>::Sphere(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D3>::GetSphereIndex());
}

static void drawCapsule(Renderer<D3> &p_Renderer, const mat4 &p_Transform, const u8 p_Flags) noexcept
{
    p_Renderer.DrawPrimitive(p_Transform, Primitives<D3>::GetCylinderIndex());

    vec3 pos{0.f};
    pos.x = -0.5f;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Flags);
    pos.x = 0.5f;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Flags);
}
static void drawCapsule(Renderer<D3> &p_Renderer, const mat4 &p_Transform, const f32 p_Length, const f32 p_Diameter,
                        const u8 p_Flags) noexcept
{
    mat4 transform = p_Transform;
    ONYX::Transform<D3>::ScaleIntrinsic(transform, {p_Length, p_Diameter, p_Diameter});
    p_Renderer.DrawPrimitive(transform, Primitives<D3>::GetCylinderIndex(), p_Flags);

    vec3 pos{0.f};
    pos.x = -0.5f * p_Length;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Diameter, p_Flags);
    pos.x = -pos.x;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Diameter, p_Flags);
}

void RenderContext<D3>::Capsule() noexcept
{
    const mat4 &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform](const u8 p_Flags) { drawCapsule(m_Renderer, transform, p_Flags); };
    const auto outline = [this, &transform](const u8 p_Flags) {
        drawCapsule(m_Renderer, transform, 1.f, 1.f + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::Capsule(const mat4 &p_Transform) noexcept
{
    const mat4 transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform](const u8 p_Flags) { drawCapsule(m_Renderer, transform, p_Flags); };
    const auto outline = [this, &transform](const u8 p_Flags) {
        drawCapsule(m_Renderer, transform, 1.f, 1.f + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

void RenderContext<D3>::Capsule(const f32 p_Length, const f32 p_Radius) noexcept
{
    const mat4 &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform, p_Length, p_Radius](const u8 p_Flags) {
        drawCapsule(m_Renderer, transform, p_Length, 2.f * p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, p_Length, p_Radius](const u8 p_Flags) {
        drawCapsule(m_Renderer, transform, p_Length, 2.f * p_Radius + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::Capsule(const f32 p_Length, const f32 p_Radius, const mat4 &p_Transform) noexcept
{
    const mat4 transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform, p_Length, p_Radius](const u8 p_Flags) {
        drawCapsule(m_Renderer, transform, p_Length, 2.f * p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, p_Length, p_Radius](const u8 p_Flags) {
        drawCapsule(m_Renderer, transform, p_Length, 2.f * p_Radius + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

static void drawRoundedCubeEdges(Renderer<D3> &p_Renderer, const mat4 &p_Transform, const u8 p_Flags,
                                 const vec3 &p_Dimensions = vec3{1.f}, const f32 p_Radius = 0.5f) noexcept
{
    const vec3 halfDims = 0.5f * p_Dimensions;
    const vec3 paddedDims = halfDims + 0.5f * p_Radius;
    for (u32 i = 0; i < 6; ++i)
    {
        mat4 transform = p_Transform;
        const u32 index1 = i % 3;
        const u32 index2 = (i + 1) % 3;
        const u32 index3 = (i + 2) % 3;
        const f32 dim = i < 3 ? paddedDims[index1] : -paddedDims[index1];
        ONYX::Transform<D3>::TranslateIntrinsic(transform, index1, dim);
        ONYX::Transform<D3>::ScaleIntrinsic(transform, index1, p_Radius);
        ONYX::Transform<D3>::ScaleIntrinsic(transform, index2, p_Dimensions[index2]);
        ONYX::Transform<D3>::ScaleIntrinsic(transform, index3, p_Dimensions[index3]);
        p_Renderer.DrawPrimitive(transform, Primitives<D3>::GetCubeIndex(), p_Flags);
    }
    const f32 diameter = 2.f * p_Radius;
    vec3 pos = halfDims;
    for (u32 i = 0; i < 8; ++i)
    {
        drawIntrinsicSphere(p_Renderer, p_Transform, pos, diameter, p_Flags);
        const u32 index = i % 2;
        pos[index] = -pos[index];
        if (i == 3)
            pos.z = -pos.z;
    }

    for (u32 axis = 0; axis < 3; ++axis)
    {
        const u32 dimIndex1 = (axis + 1) % 3;
        const u32 dimIndex2 = (axis + 2) % 3;
        const vec4 relevantDims = {halfDims[dimIndex1], -halfDims[dimIndex1], halfDims[dimIndex2],
                                   -halfDims[dimIndex2]};
        for (u32 i = 0; i < 2; ++i)
            for (u32 j = 0; j < 2; ++j)
            {
                vec3 pos{0.f};
                pos[dimIndex1] = relevantDims[i];
                pos[dimIndex2] = relevantDims[2 + j];

                mat4 transform = p_Transform;
                ONYX::Transform<D3>::TranslateIntrinsic(transform, pos);
                if (axis > 0)
                    ONYX::Transform<D3>::RotateZIntrinsic(transform, glm::half_pi<f32>());
                if (axis > 1)
                    ONYX::Transform<D3>::RotateYIntrinsic(transform, glm::half_pi<f32>());
                ONYX::Transform<D3>::ScaleIntrinsic(transform, {p_Dimensions[axis], diameter, diameter});
                p_Renderer.DrawPrimitive(transform, Primitives<D3>::GetCylinderIndex(), p_Flags);
            }
    }
}

static void drawRoundedCube(Renderer<D3> &p_Renderer, const mat4 &p_Transform, const u8 p_Flags) noexcept
{
    p_Renderer.DrawPrimitive(p_Transform, Primitives<D3>::GetSquareIndex(), p_Flags);
    drawRoundedCubeEdges(p_Renderer, p_Transform, p_Flags);
}
static void drawRoundedCube(Renderer<D3> &p_Renderer, const mat4 &p_Transform, const vec3 &p_Dimensions,
                            const f32 p_Radius, const u8 p_Flags) noexcept
{
    mat4 transform = p_Transform;
    ONYX::Transform<D3>::ScaleIntrinsic(transform, p_Dimensions);
    p_Renderer.DrawPrimitive(transform, Primitives<D3>::GetCubeIndex(), p_Flags);

    drawRoundedCubeEdges(p_Renderer, p_Transform, p_Flags, p_Dimensions, p_Radius);
}

void RenderContext<D3>::RoundedCube() noexcept
{
    const mat4 &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform](const u8 p_Flags) { drawRoundedCube(m_Renderer, transform, p_Flags); };
    const auto outline = [this, &transform](const u8 p_Flags) {
        const f32 radius = 0.5f + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedCube(m_Renderer, transform, vec3{1.f}, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::RoundedCube(const mat4 &p_Transform) noexcept
{
    const mat4 transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform](const u8 p_Flags) { drawRoundedCube(m_Renderer, transform, p_Flags); };
    const auto outline = [this, &transform](const u8 p_Flags) {
        const f32 radius = 0.5f + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedCube(m_Renderer, transform, vec3{1.f}, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::RoundedCube(const vec3 &p_Dimensions, const f32 p_Radius) noexcept
{
    const mat4 &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform, &p_Dimensions, p_Radius](const u8 p_Flags) {
        drawRoundedCube(m_Renderer, transform, p_Dimensions, p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, &p_Dimensions, p_Radius](const u8 p_Flags) {
        const f32 radius = p_Radius + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedCube(m_Renderer, transform, p_Dimensions, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::RoundedCube(const vec3 &p_Dimensions, const f32 p_Radius, const mat4 &p_Transform) noexcept
{
    const mat4 transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform, &p_Dimensions, p_Radius](const u8 p_Flags) {
        drawRoundedCube(m_Renderer, transform, p_Dimensions, p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, &p_Dimensions, p_Radius](const u8 p_Flags) {
        const f32 radius = p_Radius + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedCube(m_Renderer, transform, p_Dimensions, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::RoundedCube(const f32 p_Width, const f32 p_Height, const f32 p_Depth,
                                    const f32 p_Radius) noexcept
{
    RoundedCube(vec3{p_Width, p_Height, p_Depth}, p_Radius);
}
void RenderContext<D3>::RoundedCube(const f32 p_Width, const f32 p_Height, const f32 p_Depth, const f32 p_Radius,
                                    const mat4 &p_Transform) noexcept
{
    RoundedCube(vec3{p_Width, p_Height, p_Depth}, p_Radius, p_Transform);
}

void RenderContext<D3>::LightColor(const Color &p_Color) noexcept
{
    m_RenderState.back().LightColor = p_Color;
}
void RenderContext<D3>::AmbientColor(const Color &p_Color) noexcept
{
    m_Renderer.AmbientColor = p_Color;
}
void RenderContext<D3>::AmbientIntensity(const f32 p_Intensity) noexcept
{
    m_Renderer.AmbientColor.RGBA.a = p_Intensity;
}

void RenderContext<D3>::DirectionalLight(ONYX::DirectionalLight p_Light) noexcept
{
    p_Light.DirectionAndIntensity =
        vec4{glm::normalize(vec3{p_Light.DirectionAndIntensity}), p_Light.DirectionAndIntensity.w};
    m_Renderer.AddDirectionalLight(p_Light);
}
void RenderContext<D3>::DirectionalLight(const vec3 &p_Direction, const f32 p_Intensity) noexcept
{
    ONYX::DirectionalLight light;
    light.DirectionAndIntensity = vec4{glm::normalize(p_Direction), p_Intensity};
    light.Color = m_RenderState.back().LightColor;
    m_Renderer.AddDirectionalLight(light);
}
void RenderContext<D3>::DirectionalLight(const f32 p_DX, const f32 p_DY, const f32 p_DZ, const f32 p_Intensity) noexcept
{
    DirectionalLight(vec3{p_DX, p_DY, p_DZ}, p_Intensity);
}

void RenderContext<D3>::PointLight(const ONYX::PointLight &p_Light) noexcept
{
    m_Renderer.AddPointLight(p_Light);
}
void RenderContext<D3>::PointLight(const vec3 &p_Position, const f32 p_Radius, const f32 p_Intensity) noexcept
{
    ONYX::PointLight light;
    light.PositionAndIntensity = vec4{p_Position, p_Intensity};
    light.Radius = p_Radius;
    light.Color = m_RenderState.back().LightColor;
    m_Renderer.AddPointLight(light);
}
void RenderContext<D3>::PointLight(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius,
                                   const f32 p_Intensity) noexcept
{
    PointLight(vec3{p_X, p_Y, p_Z}, p_Radius, p_Intensity);
}
void RenderContext<D3>::PointLight(const f32 p_Radius, const f32 p_Intensity) noexcept
{
    PointLight(vec3{0.f}, p_Radius, p_Intensity);
}

void RenderContext<D3>::DiffuseContribution(const f32 p_Contribution) noexcept
{
    m_RenderState.back().Material.DiffuseContribution = p_Contribution;
}
void RenderContext<D3>::SpecularContribution(const f32 p_Contribution) noexcept
{
    m_RenderState.back().Material.SpecularContribution = p_Contribution;
}
void RenderContext<D3>::SpecularSharpness(const f32 p_Sharpness) noexcept
{
    m_RenderState.back().Material.SpecularSharpness = p_Sharpness;
}

template <Dimension D> void IRenderContext<D>::Fill(const bool p_Enabled) noexcept
{
    m_RenderState.back().Fill = p_Enabled;
}

template <Dimension D> void IRenderContext<D>::Mesh(const KIT::Ref<const Model<D>> &p_Model) noexcept
{
    m_Renderer.DrawMesh(m_RenderState.back().Transform, p_Model);
}
template <Dimension D>
void IRenderContext<D>::Mesh(const KIT::Ref<const Model<D>> &p_Model, const mat<D> &p_Transform) noexcept
{
    m_Renderer.DrawMesh(p_Transform * m_RenderState.back().Transform, p_Model);
}

template <Dimension D> void IRenderContext<D>::Push() noexcept
{
    m_RenderState.push_back(m_RenderState.back());
}
template <Dimension D> void IRenderContext<D>::PushAndClear() noexcept
{
    m_RenderState.push_back(RenderState<D>{});
    if constexpr (D == D3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}
template <Dimension D> void IRenderContext<D>::Pop() noexcept
{
    KIT_ASSERT(m_RenderState.size() > 1, "For every push, there must be a pop");
    m_RenderState.pop_back();
}

void RenderContext<D2>::Alpha(const f32 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = p_Alpha;
}
void RenderContext<D2>::Alpha(const u8 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = static_cast<f32>(p_Alpha) / 255.f;
}
void RenderContext<D2>::Alpha(const u32 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = static_cast<f32>(p_Alpha) / 255.f;
}

template <Dimension D> void IRenderContext<D>::Fill(const Color &p_Color) noexcept
{
    m_RenderState.back().Material.Color = p_Color;
}
template <Dimension D> void IRenderContext<D>::Outline(const bool p_Enabled) noexcept
{
    m_RenderState.back().Outline = p_Enabled;
}
template <Dimension D> void IRenderContext<D>::Outline(const Color &p_Color) noexcept
{
    m_RenderState.back().OutlineColor = p_Color;
}
template <Dimension D> void IRenderContext<D>::OutlineWidth(const f32 p_Width) noexcept
{
    m_RenderState.back().OutlineWidth = p_Width;
}

template <Dimension D> void IRenderContext<D>::Material(const MaterialData<D> &p_Material) noexcept
{
    m_RenderState.back().Material = p_Material;
}

template <Dimension D> const RenderState<D> &IRenderContext<D>::GetState() const noexcept
{
    return m_RenderState.back();
}
template <Dimension D> RenderState<D> &IRenderContext<D>::GetState() noexcept
{
    return m_RenderState.back();
}
template <Dimension D> void IRenderContext<D>::SetState(const RenderState<D> &p_State) noexcept
{
    m_RenderState.back() = p_State;
}

template <Dimension D> void IRenderContext<D>::SetCurrentTransform(const mat<D> &p_Transform) noexcept
{
    m_RenderState.back().Transform = p_Transform;
}
template <Dimension D> void IRenderContext<D>::SetCurrentAxes(const mat<D> &p_Axes) noexcept
{
    m_RenderState.back().Axes = p_Axes;
    if constexpr (D == D3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}

template <Dimension D> void IRenderContext<D>::Render(const VkCommandBuffer p_Commandbuffer) noexcept
{
    m_Renderer.Render(p_Commandbuffer);
}

template <Dimension D> void IRenderContext<D>::Axes(const f32 p_Thickness, const f32 p_Size) noexcept
{
    // TODO: Parametrize this
    Color &color = m_RenderState.back().Material.Color;
    const Color oldColor = color; // A cheap filthy push
    if constexpr (D == D2)
    {
        const vec2 xLeft = vec2{-p_Size, 0.f};
        const vec2 xRight = vec2{p_Size, 0.f};

        const vec2 yDown = vec2{0.f, -p_Size};
        const vec2 yUp = vec2{0.f, p_Size};

        color = Color{245u, 64u, 90u};
        Line(xLeft, xRight, p_Thickness);
        color = Color{65u, 135u, 245u};
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

        color = Color{245u, 64u, 90u};
        Line(xLeft, xRight, p_Thickness);
        color = Color{65u, 135u, 245u};
        Line(yDown, yUp, p_Thickness);
        color = Color{180u, 245u, 65u};
        Line(zBack, zFront, p_Thickness);
    }
    color = oldColor; // A cheap filthy pop
}

vec2 RenderContext<D2>::GetMouseCoordinates() const noexcept
{
    const vec2 mpos = Input::GetMousePosition(m_Window);
    mat4 axes = transform3ToTransform4(m_RenderState.back().Axes);
    ApplyCoordinateSystem(axes);
    return glm::inverse(axes) * vec4{mpos, 1.f, 1.f};
}
vec3 RenderContext<D3>::GetMouseCoordinates(const f32 p_Depth) const noexcept
{
    const vec2 mpos = Input::GetMousePosition(m_Window);
    const mat4 &axes = m_RenderState.back().Axes;
    if (!m_RenderState.back().HasProjection)
        return m_RenderState.back().InverseAxes * vec4{mpos, p_Depth, 1.f};

    const vec4 clip = glm::inverse(m_RenderState.back().Projection * axes) * vec4{mpos, p_Depth, 1.f};
    return vec3{clip} / clip.w;
}

void RenderContext<D3>::Projection(const mat4 &p_Projection) noexcept
{
    m_RenderState.back().Projection = p_Projection;
    m_RenderState.back().HasProjection = true;
}
void RenderContext<D3>::Perspective(const f32 p_FieldOfView, const f32 p_Near, const f32 p_Far) noexcept
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

void RenderContext<D3>::Orthographic() noexcept
{
    m_RenderState.back().Projection = mat4{1.f};
    m_RenderState.back().HasProjection = false;
}

template class IRenderContext<D2>;
template class IRenderContext<D3>;

} // namespace ONYX