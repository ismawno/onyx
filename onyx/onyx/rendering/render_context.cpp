#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"

#include "tkit/utils/math.hpp"

namespace Onyx
{
template <Dimension D>
Detail::IRenderContext<D>::IRenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : m_Renderer(p_RenderPass, &m_RenderState, &m_ProjectionView), m_Window(p_Window)
{
    m_RenderState.push_back(RenderState<D>{});
    UpdateViewAspect(p_Window->GetScreenAspect());
}

template <Dimension D> void Detail::IRenderContext<D>::FlushState() noexcept
{
    TKIT_ASSERT(m_RenderState.size() == 1, "[ONYX] For every push, there must be a pop");
    m_RenderState[0] = RenderState<D>{};
}
template <Dimension D> void Detail::IRenderContext<D>::FlushState(const Color &p_Color) noexcept
{
    FlushState();
    m_Window->BackgroundColor = p_Color;
}

template <Dimension D> void Detail::IRenderContext<D>::FlushDrawData() noexcept
{
    m_Renderer.Flush();
}

template <Dimension D> void Detail::IRenderContext<D>::Flush() noexcept
{
    FlushDrawData();
    FlushState();
}
template <Dimension D> void Detail::IRenderContext<D>::Flush(const Color &p_Color) noexcept
{
    FlushDrawData();
    FlushState(p_Color);
}

template <Dimension D> void Detail::IRenderContext<D>::Transform(const fmat<D> &p_Transform) noexcept
{
    m_RenderState.back().Transform = p_Transform * m_RenderState.back().Transform;
}
template <Dimension D>
void Detail::IRenderContext<D>::Transform(const fvec<D> &p_Translation, const fvec<D> &p_Scale,
                                          const rot<D> &p_Rotation) noexcept
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
template <Dimension D>
void Detail::IRenderContext<D>::Transform(const fvec<D> &p_Translation, const f32 p_Scale,
                                          const rot<D> &p_Rotation) noexcept
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(p_Translation, fvec<D>{p_Scale}, p_Rotation));
}
void RenderContext<D3>::Transform(const fvec3 &p_Translation, const fvec3 &p_Scale, const fvec3 &p_Rotation) noexcept
{
    this->Transform(Onyx::Transform<D3>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
void RenderContext<D3>::Transform(const fvec3 &p_Translation, const f32 p_Scale, const fvec3 &p_Rotation) noexcept
{
    this->Transform(Onyx::Transform<D3>::ComputeTransform(p_Translation, fvec3{p_Scale}, p_Rotation));
}

template <Dimension D> void Detail::IRenderContext<D>::TransformAxes(const fmat<D> &p_Axes) noexcept
{
    m_RenderState.back().Axes *= p_Axes;
}
template <Dimension D>
void Detail::IRenderContext<D>::TransformAxes(const fvec<D> &p_Translation, const fvec<D> &p_Scale,
                                              const rot<D> &p_Rotation) noexcept
{
    m_RenderState.back().Axes *= Onyx::Transform<D>::ComputeReversedTransform(p_Translation, p_Scale, p_Rotation);
}
template <Dimension D>
void Detail::IRenderContext<D>::TransformAxes(const fvec<D> &p_Translation, const f32 p_Scale,
                                              const rot<D> &p_Rotation) noexcept
{
    TransformAxes(p_Translation, fvec<D>{p_Scale}, p_Rotation);
}
void RenderContext<D3>::TransformAxes(const fvec3 &p_Translation, const fvec3 &p_Scale,
                                      const fvec3 &p_Rotation) noexcept
{
    TransformAxes(Onyx::Transform<D3>::ComputeReversedTransform(p_Translation, p_Scale, p_Rotation));
}
void RenderContext<D3>::TransformAxes(const fvec3 &p_Translation, const f32 p_Scale, const fvec3 &p_Rotation) noexcept
{
    TransformAxes(Onyx::Transform<D3>::ComputeReversedTransform(p_Translation, fvec3{p_Scale}, p_Rotation));
}

template <Dimension D> void Detail::IRenderContext<D>::Translate(const fvec<D> &p_Translation) noexcept
{
    Onyx::Transform<D>::TranslateExtrinsic(m_RenderState.back().Transform, p_Translation);
}
void RenderContext<D2>::Translate(const f32 p_X, const f32 p_Y) noexcept
{
    Translate(fvec2{p_X, p_Y});
}
void RenderContext<D3>::Translate(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Translate(fvec3{p_X, p_Y, p_Z});
}

template <Dimension D> void Detail::IRenderContext<D>::Scale(const fvec<D> &p_Scale) noexcept
{
    Onyx::Transform<D>::ScaleExtrinsic(m_RenderState.back().Transform, p_Scale);
}
template <Dimension D> void Detail::IRenderContext<D>::Scale(const f32 p_Scale) noexcept
{
    Scale(fvec<D>{p_Scale});
}
void RenderContext<D2>::Scale(const f32 p_X, const f32 p_Y) noexcept
{
    Scale(fvec2{p_X, p_Y});
}
void RenderContext<D3>::Scale(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Scale(fvec3{p_X, p_Y, p_Z});
}

template <Dimension D> void Detail::IRenderContext<D>::TranslateX(const f32 p_X) noexcept
{
    Onyx::Transform<D>::TranslateExtrinsic(m_RenderState.back().Transform, 0, p_X);
}
template <Dimension D> void Detail::IRenderContext<D>::TranslateY(const f32 p_Y) noexcept
{
    Onyx::Transform<D>::TranslateExtrinsic(m_RenderState.back().Transform, 1, p_Y);
}
void RenderContext<D3>::TranslateZ(const f32 p_Z) noexcept
{
    Onyx::Transform<D3>::TranslateExtrinsic(m_RenderState.back().Transform, 2, p_Z);
}

template <Dimension D> void Detail::IRenderContext<D>::ScaleX(const f32 p_X) noexcept
{
    Onyx::Transform<D>::ScaleExtrinsic(m_RenderState.back().Transform, 0, p_X);
}
template <Dimension D> void Detail::IRenderContext<D>::ScaleY(const f32 p_Y) noexcept
{
    Onyx::Transform<D>::ScaleExtrinsic(m_RenderState.back().Transform, 1, p_Y);
}
void RenderContext<D3>::ScaleZ(const f32 p_Z) noexcept
{
    Onyx::Transform<D3>::ScaleExtrinsic(m_RenderState.back().Transform, 2, p_Z);
}

template <Dimension D> void Detail::IRenderContext<D>::TranslateXAxis(const f32 p_X) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(m_RenderState.back().Axes, 0, p_X);
}
template <Dimension D> void Detail::IRenderContext<D>::TranslateYAxis(const f32 p_Y) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(m_RenderState.back().Axes, 1, p_Y);
}
void RenderContext<D3>::TranslateZAxis(const f32 p_Z) noexcept
{
    Onyx::Transform<D3>::TranslateIntrinsic(m_RenderState.back().Axes, 2, p_Z);
}

template <Dimension D> void Detail::IRenderContext<D>::ScaleXAxis(const f32 p_X) noexcept
{
    Onyx::Transform<D>::ScaleIntrinsic(m_RenderState.back().Axes, 0, p_X);
}
template <Dimension D> void Detail::IRenderContext<D>::ScaleYAxis(const f32 p_Y) noexcept
{
    Onyx::Transform<D>::ScaleIntrinsic(m_RenderState.back().Axes, 1, p_Y);
}
void RenderContext<D3>::ScaleZAxis(const f32 p_Z) noexcept
{
    Onyx::Transform<D3>::ScaleIntrinsic(m_RenderState.back().Axes, 2, p_Z);
}

template <Dimension D> void Detail::IRenderContext<D>::UpdateViewAspect(const f32 p_Aspect) noexcept
{
    m_ProjectionView.View.Scale.x = m_ProjectionView.View.Scale.y * p_Aspect;
    if constexpr (D == D2)
        m_ProjectionView.ProjectionView = m_ProjectionView.View.ComputeInverseTransform();
    else
    {
        fmat4 vmat = m_ProjectionView.View.ComputeInverseTransform();
        ApplyCoordinateSystemExtrinsic(vmat);
        m_ProjectionView.ProjectionView = m_ProjectionView.Projection * vmat;
    }
}

template <Dimension D> void Detail::IRenderContext<D>::TranslateAxes(const fvec<D> &p_Translation) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(m_RenderState.back().Axes, p_Translation);
}
void RenderContext<D2>::TranslateAxes(const f32 p_X, const f32 p_Y) noexcept
{
    TranslateAxes(fvec2{p_X, p_Y});
}
void RenderContext<D3>::TranslateAxes(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    TranslateAxes(fvec3{p_X, p_Y, p_Z});
}

template <Dimension D> void Detail::IRenderContext<D>::ScaleAxes(const fvec<D> &p_Scale) noexcept
{
    Onyx::Transform<D>::ScaleIntrinsic(m_RenderState.back().Axes, p_Scale);
}
template <Dimension D> void Detail::IRenderContext<D>::ScaleAxes(const f32 p_Scale) noexcept
{
    ScaleAxes(fvec<D>{p_Scale});
}
void RenderContext<D2>::ScaleAxes(const f32 p_X, const f32 p_Y) noexcept
{
    ScaleAxes(fvec2{p_X, p_Y});
}
void RenderContext<D3>::ScaleAxes(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    ScaleAxes(fvec3{p_X, p_Y, p_Z});
}

void RenderContext<D2>::Rotate(const f32 p_Angle) noexcept
{
    Onyx::Transform<D2>::RotateExtrinsic(m_RenderState.back().Transform, p_Angle);
}

void RenderContext<D3>::Rotate(const quat &p_Quaternion) noexcept
{
    Onyx::Transform<D3>::RotateExtrinsic(m_RenderState.back().Transform, p_Quaternion);
}
void RenderContext<D3>::Rotate(const f32 p_Angle, const fvec3 &p_Axis) noexcept
{
    Rotate(glm::angleAxis(p_Angle, p_Axis));
}
void RenderContext<D3>::Rotate(const fvec3 &p_Angles) noexcept
{
    Rotate(glm::quat(p_Angles));
}
void RenderContext<D3>::Rotate(const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    Rotate(fvec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<D3>::RotateX(const f32 p_Angle) noexcept
{
    Rotate(fvec3{p_Angle, 0.f, 0.f});
}
void RenderContext<D3>::RotateY(const f32 p_Angle) noexcept
{
    Rotate(fvec3{0.f, p_Angle, 0.f});
}
void RenderContext<D3>::RotateZ(const f32 p_Angle) noexcept
{
    Rotate(fvec3{0.f, 0.f, p_Angle});
}

void RenderContext<D2>::RotateAxes(const f32 p_Angle) noexcept
{
    Onyx::Transform<D2>::RotateIntrinsic(m_RenderState.back().Axes, p_Angle);
}
void RenderContext<D3>::RotateAxes(const quat &p_Quaternion) noexcept
{
    Onyx::Transform<D3>::RotateIntrinsic(m_RenderState.back().Axes, p_Quaternion);
}
void RenderContext<D3>::RotateAxes(const f32 p_XRot, const f32 p_YRot, const f32 p_ZRot) noexcept
{
    RotateAxes(fvec3{p_XRot, p_YRot, p_ZRot});
}
void RenderContext<D3>::RotateAxes(const f32 p_Angle, const fvec3 &p_Axis) noexcept
{
    RotateAxes(glm::angleAxis(p_Angle, p_Axis));
}
void RenderContext<D3>::RotateAxes(const fvec3 &p_Angles) noexcept
{
    RotateAxes(glm::quat(p_Angles));
}

// This could be optimized a bit
void RenderContext<D3>::RotateXAxis(const f32 p_Angle) noexcept
{
    RotateAxes(fvec3{p_Angle, 0.f, 0.f});
}
void RenderContext<D3>::RotateYAxis(const f32 p_Angle) noexcept
{
    RotateAxes(fvec3{0.f, p_Angle, 0.f});
}
void RenderContext<D3>::RotateZAxis(const f32 p_Angle) noexcept
{
    RotateAxes(fvec3{0.f, 0.f, p_Angle});
}

static fmat4 transform3ToTransform4(const fmat3 &p_Transform) noexcept
{
    fmat4 t4{1.f};
    t4[0][0] = p_Transform[0][0];
    t4[0][1] = p_Transform[0][1];
    t4[1][0] = p_Transform[1][0];
    t4[1][1] = p_Transform[1][1];

    t4[3][0] = p_Transform[2][0];
    t4[3][1] = p_Transform[2][1];
    return t4;
}

template <Dimension D> void Detail::IRenderContext<D>::Triangle() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D>::GetTriangleIndex());
}
template <Dimension D> void Detail::IRenderContext<D>::Triangle(const fmat<D> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D>::GetTriangleIndex());
}

template <Dimension D> void Detail::IRenderContext<D>::Square() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D>::GetSquareIndex());
}
template <Dimension D> void Detail::IRenderContext<D>::Square(const fmat<D> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D>::GetSquareIndex());
}

template <Dimension D> void Detail::IRenderContext<D>::NGon(const u32 p_Sides) noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Primitives<D>::GetNGonIndex(p_Sides));
}
template <Dimension D> void Detail::IRenderContext<D>::NGon(const fmat<D> &p_Transform, const u32 p_Sides) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Primitives<D>::GetNGonIndex(p_Sides));
}

template <Dimension D> void Detail::IRenderContext<D>::Polygon(const TKit::Span<const fvec<D>> p_Vertices) noexcept
{
    m_Renderer.DrawPolygon(m_RenderState.back().Transform, p_Vertices);
}
template <Dimension D>
void Detail::IRenderContext<D>::Polygon(const fmat<D> &p_Transform, const TKit::Span<const fvec<D>> p_Vertices) noexcept
{
    m_Renderer.DrawPolygon(p_Transform * m_RenderState.back().Transform, p_Vertices);
}

template <Dimension D> void Detail::IRenderContext<D>::Circle() noexcept
{
    m_Renderer.DrawCircleOrArc(m_RenderState.back().Transform);
}
template <Dimension D> void Detail::IRenderContext<D>::Circle(const fmat<D> &p_Transform) noexcept
{
    m_Renderer.DrawCircleOrArc(p_Transform * m_RenderState.back().Transform);
}
template <Dimension D>
void Detail::IRenderContext<D>::Circle(const f32 p_InnerFade, const f32 p_LowerFade, const f32 p_Hollowness) noexcept
{
    m_Renderer.DrawCircleOrArc(m_RenderState.back().Transform, p_InnerFade, p_LowerFade, p_Hollowness);
}
template <Dimension D>
void Detail::IRenderContext<D>::Circle(const fmat<D> &p_Transform, const f32 p_InnerFade, const f32 p_LowerFade,
                                       const f32 p_Hollowness) noexcept
{
    m_Renderer.DrawCircleOrArc(p_Transform * m_RenderState.back().Transform, p_InnerFade, p_LowerFade, p_Hollowness);
}

template <Dimension D>
void Detail::IRenderContext<D>::Arc(const f32 p_LowerAngle, const f32 p_UpperAngle, const f32 p_Hollowness) noexcept
{
    m_Renderer.DrawCircleOrArc(m_RenderState.back().Transform, 0.f, 0.f, p_Hollowness, p_LowerAngle, p_UpperAngle);
}
template <Dimension D>
void Detail::IRenderContext<D>::Arc(const fmat<D> &p_Transform, const f32 p_LowerAngle, const f32 p_UpperAngle,
                                    const f32 p_Hollowness) noexcept
{
    m_Renderer.DrawCircleOrArc(p_Transform * m_RenderState.back().Transform, 0.f, 0.f, p_Hollowness, p_LowerAngle,
                               p_UpperAngle);
}
template <Dimension D>
void Detail::IRenderContext<D>::Arc(const f32 p_LowerAngle, const f32 p_UpperAngle, const f32 p_InnerFade,
                                    const f32 p_OuterFade, const f32 p_Hollowness) noexcept
{
    m_Renderer.DrawCircleOrArc(m_RenderState.back().Transform, p_InnerFade, p_OuterFade, p_Hollowness, p_LowerAngle,
                               p_UpperAngle);
}
template <Dimension D>
void Detail::IRenderContext<D>::Arc(const fmat<D> &p_Transform, const f32 p_LowerAngle, const f32 p_UpperAngle,
                                    const f32 p_InnerFade, const f32 p_OuterFade, const f32 p_Hollowness) noexcept
{
    m_Renderer.DrawCircleOrArc(p_Transform * m_RenderState.back().Transform, p_InnerFade, p_OuterFade, p_Hollowness,
                               p_LowerAngle, p_UpperAngle);
}

template <Dimension D>
static void drawIntrinsicArc(Detail::Renderer<D> &p_Renderer, fmat<D> p_Transform, const fvec<D> &p_Position,
                             const f32 p_Diameter, const f32 p_LowerAngle, const f32 p_UpperAngle,
                             const Detail::DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(p_Transform, p_Position);
    if constexpr (D == D2)
        Onyx::Transform<D>::ScaleIntrinsic(p_Transform, fvec<D>{p_Diameter});
    else
        Onyx::Transform<D>::ScaleIntrinsic(p_Transform, fvec<D>{p_Diameter, p_Diameter, 1.f});
    p_Renderer.DrawCircleOrArc(p_Transform, 0.f, 0.f, 0.f, p_LowerAngle, p_UpperAngle, p_Flags);
}
template <Dimension D>
static void drawIntrinsicArc(Detail::Renderer<D> &p_Renderer, fmat<D> p_Transform, const fvec<D> &p_Position,
                             const f32 p_LowerAngle, const f32 p_UpperAngle, const Detail::DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(p_Transform, p_Position);
    p_Renderer.DrawCircleOrArc(p_Transform, 0.f, 0.f, p_LowerAngle, p_UpperAngle, 0.f, p_Flags);
}

static void drawIntrinsicSphere(Detail::Renderer<D3> &p_Renderer, fmat4 p_Transform, const fvec3 &p_Position,
                                const f32 p_Diameter, const Detail::DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D3>::TranslateIntrinsic(p_Transform, p_Position);
    Onyx::Transform<D3>::ScaleIntrinsic(p_Transform, fvec3{p_Diameter});
    p_Renderer.DrawPrimitive(p_Transform, Detail::Primitives<D3>::GetSphereIndex(), p_Flags);
}
static void drawIntrinsicSphere(Detail::Renderer<D3> &p_Renderer, fmat4 p_Transform, const fvec3 &p_Position,
                                const Detail::DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D3>::TranslateIntrinsic(p_Transform, p_Position);
    p_Renderer.DrawPrimitive(p_Transform, Detail::Primitives<D3>::GetSphereIndex(), p_Flags);
}

template <Dimension D>
static void drawStadiumMoons(Detail::Renderer<D> &p_Renderer, const fmat<D> &p_Transform,
                             const Detail::DrawFlags p_Flags, const f32 p_Length = 1.f,
                             const f32 p_Diameter = 1.f) noexcept
{
    fvec<D> pos{0.f};
    pos.x = -0.5f * p_Length;
    drawIntrinsicArc<D>(p_Renderer, p_Transform, pos, p_Diameter, glm::radians(90.f), glm::radians(270.f), p_Flags);
    pos.x = -pos.x;
    drawIntrinsicArc<D>(p_Renderer, p_Transform, pos, p_Diameter, glm::radians(-90.f), glm::radians(90.f), p_Flags);
}

template <Dimension D>
static void drawStadium(Detail::Renderer<D> &p_Renderer, const fmat<D> &p_Transform,
                        const Detail::DrawFlags p_Flags) noexcept
{
    p_Renderer.DrawPrimitive(p_Transform, Detail::Primitives<D>::GetSquareIndex(), p_Flags);
    drawStadiumMoons<D>(p_Renderer, p_Transform, p_Flags);
}
template <Dimension D>
static void drawStadium(Detail::Renderer<D> &p_Renderer, const fmat<D> &p_Transform, const f32 p_Length,
                        const f32 p_Diameter, const Detail::DrawFlags p_Flags) noexcept
{
    fmat<D> transform = p_Transform;
    Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Length);
    Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Diameter);
    p_Renderer.DrawPrimitive(transform, Detail::Primitives<D>::GetSquareIndex(), p_Flags);

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
            p_FillDraw(Detail::DrawFlags_DoStencilWriteDoFill);
            p_OutlineDraw(Detail::DrawFlags_DoStencilTestNoFill);
        }
        else
            p_FillDraw(Detail::DrawFlags_NoStencilWriteDoFill);
    }
    else if (p_Outline)
    {
        p_FillDraw(Detail::DrawFlags_DoStencilWriteNoFill);
        p_OutlineDraw(Detail::DrawFlags_DoStencilTestNoFill);
    }
}

template <Dimension D> void Detail::IRenderContext<D>::Stadium() noexcept
{
    const fmat<D> &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawStadium(m_Renderer, transform, p_Flags);
    };
    const auto outline = [this, &transform](const Detail::DrawFlags p_Flags) {
        const f32 diameter = 1.f + m_RenderState.back().OutlineWidth;
        drawStadium(m_Renderer, transform, 1.f, diameter, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D> void Detail::IRenderContext<D>::Stadium(const fmat<D> &p_Transform) noexcept
{
    const fmat<D> transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawStadium(m_Renderer, transform, p_Flags);
    };
    const auto outline = [this, &transform](const Detail::DrawFlags p_Flags) {
        const f32 diameter = 1.f + m_RenderState.back().OutlineWidth;
        drawStadium(m_Renderer, transform, 1.f, diameter, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

template <Dimension D> void Detail::IRenderContext<D>::Stadium(const f32 p_Length, const f32 p_Radius) noexcept
{
    const fmat<D> &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform, p_Length, p_Radius](const Detail::DrawFlags p_Flags) {
        drawStadium(m_Renderer, transform, p_Length, 2.f * p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, p_Length, p_Radius](const Detail::DrawFlags p_Flags) {
        const f32 diameter = 2.f * p_Radius + m_RenderState.back().OutlineWidth;
        drawStadium(m_Renderer, transform, p_Length, diameter, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D>
void Detail::IRenderContext<D>::Stadium(const fmat<D> &p_Transform, const f32 p_Length, const f32 p_Radius) noexcept
{
    const fmat<D> transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform, p_Length, p_Radius](const Detail::DrawFlags p_Flags) {
        drawStadium(m_Renderer, transform, p_Length, 2.f * p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, p_Length, p_Radius](const Detail::DrawFlags p_Flags) {
        const f32 diameter = 2.f * p_Radius + m_RenderState.back().OutlineWidth;
        drawStadium(m_Renderer, transform, p_Length, diameter, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

template <Dimension D>
static void drawRoundedSquareEdges(Detail::Renderer<D> &p_Renderer, const fmat<D> &p_Transform,
                                   const Detail::DrawFlags p_Flags, const fvec2 &p_Dimensions = fvec2{1.f},
                                   const f32 p_Radius = 0.5f) noexcept
{
    const fvec2 halfDims = 0.5f * p_Dimensions;
    const fvec2 paddedDims = halfDims + 0.5f * p_Radius;
    const f32 diameter = 2.f * p_Radius;

    fvec<D> pos;
    if constexpr (D == D2)
        pos = halfDims;
    else
        pos = fvec3{halfDims.x, halfDims.y, 0.f};
    for (u32 i = 0; i < 4; ++i)
    {
        fmat<D> transform = p_Transform;
        const u32 index1 = i % 2;
        const u32 index2 = 1 - index1;
        const f32 dim = i < 2 ? paddedDims[index1] : -paddedDims[index1];
        Onyx::Transform<D>::TranslateIntrinsic(transform, index1, dim);
        Onyx::Transform<D>::ScaleIntrinsic(transform, index1, p_Radius);
        Onyx::Transform<D>::ScaleIntrinsic(transform, index2, p_Dimensions[index2]);
        p_Renderer.DrawPrimitive(transform, Detail::Primitives<D>::GetSquareIndex(), p_Flags);

        const f32 angle = i * glm::half_pi<f32>();
        drawIntrinsicArc<D>(p_Renderer, p_Transform, pos, diameter, angle, angle + glm::half_pi<f32>(), p_Flags);
        pos[index1] = -pos[index1];
    }
}

template <Dimension D>
static void drawRoundedSquare(Detail::Renderer<D> &p_Renderer, const fmat<D> &p_Transform,
                              const Detail::DrawFlags p_Flags) noexcept
{
    p_Renderer.DrawPrimitive(p_Transform, Detail::Primitives<D>::GetSquareIndex(), p_Flags);
    drawRoundedSquareEdges<D>(p_Renderer, p_Transform, p_Flags);
}
template <Dimension D>
static void drawRoundedSquare(Detail::Renderer<D> &p_Renderer, const fmat<D> &p_Transform, const fvec2 &p_Dimensions,
                              const f32 p_Radius, const Detail::DrawFlags p_Flags) noexcept
{
    fmat<D> transform = p_Transform;
    Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Dimensions.x);
    Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Dimensions.y);
    p_Renderer.DrawPrimitive(transform, Detail::Primitives<D>::GetSquareIndex(), p_Flags);

    drawRoundedSquareEdges<D>(p_Renderer, p_Transform, p_Flags, p_Dimensions, p_Radius);
}

template <Dimension D> void Detail::IRenderContext<D>::RoundedSquare() noexcept
{
    const fmat<D> &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawRoundedSquare(m_Renderer, transform, p_Flags);
    };
    const auto outline = [this, &transform](const Detail::DrawFlags p_Flags) {
        const f32 radius = 0.5f + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedSquare(m_Renderer, transform, fvec2{1.f}, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D> void Detail::IRenderContext<D>::RoundedSquare(const fmat<D> &p_Transform) noexcept
{
    const fmat<D> transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawRoundedSquare(m_Renderer, transform, p_Flags);
    };
    const auto outline = [this, &transform](const Detail::DrawFlags p_Flags) {
        const f32 radius = 0.5f + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedSquare(m_Renderer, transform, fvec2{1.f}, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D>
void Detail::IRenderContext<D>::RoundedSquare(const fvec2 &p_Dimensions, const f32 p_Radius) noexcept
{
    const fmat<D> &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform, &p_Dimensions, p_Radius](const Detail::DrawFlags p_Flags) {
        drawRoundedSquare(m_Renderer, transform, p_Dimensions, p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, &p_Dimensions, p_Radius](const Detail::DrawFlags p_Flags) {
        const f32 radius = p_Radius + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedSquare(m_Renderer, transform, p_Dimensions, radius + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D>
void Detail::IRenderContext<D>::RoundedSquare(const fmat<D> &p_Transform, const fvec2 &p_Dimensions,
                                              const f32 p_Radius) noexcept
{
    const fmat<D> transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform, &p_Dimensions, p_Radius](const Detail::DrawFlags p_Flags) {
        drawRoundedSquare(m_Renderer, transform, p_Dimensions, p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, &p_Dimensions, p_Radius](const Detail::DrawFlags p_Flags) {
        const f32 radius = p_Radius + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedSquare(m_Renderer, transform, p_Dimensions, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
template <Dimension D>
void Detail::IRenderContext<D>::RoundedSquare(const f32 p_Width, const f32 p_Height, const f32 p_Radius) noexcept
{
    RoundedSquare(fvec2{p_Width, p_Height}, p_Radius);
}
template <Dimension D>
void Detail::IRenderContext<D>::RoundedSquare(const fmat<D> &p_Transform, const f32 p_Width, const f32 p_Height,
                                              const f32 p_Radius) noexcept
{
    RoundedSquare(p_Transform, fvec2{p_Width, p_Height}, p_Radius);
}

template <Dimension D>
static void drawLine(Detail::Renderer<D> &p_Renderer, const RenderState<D> &p_State, const fvec<D> &p_Start,
                     const fvec<D> &p_End, const f32 p_Thickness, const Detail::DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D> t;
    t.Translation = 0.5f * (p_Start + p_End);
    const fvec<D> delta = p_End - p_Start;
    if constexpr (D == D2)
        t.Rotation = glm::atan(delta.y, delta.x);
    else
        t.Rotation = quat{{0.f, glm::atan(delta.z, delta.x), glm::atan(delta.y, delta.x)}};
    t.Scale.x = glm::length(delta);
    t.Scale.y = p_Thickness;
    if constexpr (D == D3)
        t.Scale.z = p_Thickness;

    const fmat<D> transform = p_State.Transform * t.ComputeTransform();
    if constexpr (D == D2)
        p_Renderer.DrawPrimitive(transform, Detail::Primitives<D2>::GetSquareIndex(), p_Flags);
    else
        p_Renderer.DrawPrimitive(transform, Detail::Primitives<D3>::GetCylinderIndex(), p_Flags);
}

template <Dimension D>
void Detail::IRenderContext<D>::Line(const fvec<D> &p_Start, const fvec<D> &p_End, const f32 p_Thickness) noexcept
{
    const auto fill = [this, &p_Start, &p_End, p_Thickness](const Detail::DrawFlags p_Flags) {
        drawLine<D>(m_Renderer, m_RenderState.back(), p_Start, p_End, p_Thickness, p_Flags);
    };
    const auto outline = [this, &p_Start, &p_End, p_Thickness](const Detail::DrawFlags p_Flags) {
        const f32 w = m_RenderState.back().OutlineWidth;
        const f32 thickness = p_Thickness * (1.f + w);
        const fvec<D> delta = 0.5f * w * glm::normalize(p_End - p_Start);
        drawLine<D>(m_Renderer, m_RenderState.back(), p_Start - delta, p_End + delta, thickness, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

void RenderContext<D2>::Line(const f32 p_X1, const f32 p_Y1, const f32 p_X2, const f32 p_Y2,
                             const f32 p_Thickness) noexcept
{
    Line(fvec2{p_X1, p_Y1}, fvec2{p_X2, p_Y2}, p_Thickness);
}
void RenderContext<D3>::Line(const f32 p_X1, const f32 p_Y1, const f32 p_Z1, const f32 p_X2, const f32 p_Y2,
                             const f32 p_Z2, const f32 p_Thickness) noexcept
{
    Line(fvec3{p_X1, p_Y1, p_Z1}, fvec3{p_X2, p_Y2, p_Z2}, p_Thickness);
}

template <Dimension D>
void Detail::IRenderContext<D>::LineStrip(TKit::Span<const fvec<D>> p_Points, const f32 p_Thickness) noexcept
{
    TKIT_ASSERT(p_Points.size() > 1, "[ONYX] A line strip must have at least two points");
    for (u32 i = 0; i < p_Points.size() - 1; ++i)
        Line(p_Points[i], p_Points[i + 1], p_Thickness);
}

void RenderContext<D2>::RoundedLine(const fvec2 &p_Start, const fvec2 &p_End, const f32 p_Thickness) noexcept
{
    const fvec2 delta = p_End - p_Start;
    fmat3 transform = m_RenderState.back().Transform;
    Onyx::Transform<D2>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D2>::RotateIntrinsic(transform, glm::atan(delta.y, delta.x));

    Stadium(transform, glm::length(delta), p_Thickness);
}
void RenderContext<D3>::RoundedLine(const fvec3 &p_Start, const fvec3 &p_End, const f32 p_Thickness) noexcept
{
    const fvec3 delta = p_End - p_Start;
    fmat4 transform = m_RenderState.back().Transform;
    Onyx::Transform<D3>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D3>::RotateIntrinsic(transform,
                                         quat{{0.f, glm::atan(delta.y, delta.x), glm::atan(delta.z, delta.x)}});
    Capsule(transform, glm::length(delta), p_Thickness);
}

void RenderContext<D2>::RoundedLine(const f32 p_X1, const f32 p_Y1, const f32 p_X2, const f32 p_Y2,
                                    const f32 p_Thickness) noexcept
{
    RoundedLine(fvec2{p_X1, p_Y1}, fvec2{p_X2, p_Y2}, p_Thickness);
}
void RenderContext<D3>::RoundedLine(const f32 p_X1, const f32 p_Y1, const f32 p_Z1, const f32 p_X2, const f32 p_Y2,
                                    const f32 p_Z2, const f32 p_Thickness) noexcept
{
    RoundedLine(fvec3{p_X1, p_Y1, p_Z1}, fvec3{p_X2, p_Y2, p_Z2}, p_Thickness);
}

void RenderContext<D3>::Cube() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Detail::Primitives<D3>::GetCubeIndex());
}
void RenderContext<D3>::Cube(const fmat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Detail::Primitives<D3>::GetCubeIndex());
}

void RenderContext<D3>::Cylinder() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Detail::Primitives<D3>::GetCylinderIndex());
}
void RenderContext<D3>::Cylinder(const fmat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Detail::Primitives<D3>::GetCylinderIndex());
}

void RenderContext<D3>::Sphere() noexcept
{
    m_Renderer.DrawPrimitive(m_RenderState.back().Transform, Detail::Primitives<D3>::GetSphereIndex());
}
void RenderContext<D3>::Sphere(const fmat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(p_Transform * m_RenderState.back().Transform, Detail::Primitives<D3>::GetSphereIndex());
}

static void drawCapsule(Detail::Renderer<D3> &p_Renderer, const fmat4 &p_Transform,
                        const Detail::DrawFlags p_Flags) noexcept
{
    p_Renderer.DrawPrimitive(p_Transform, Detail::Primitives<D3>::GetCylinderIndex());

    fvec3 pos{0.f};
    pos.x = -0.5f;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Flags);
    pos.x = 0.5f;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Flags);
}
static void drawCapsule(Detail::Renderer<D3> &p_Renderer, const fmat4 &p_Transform, const f32 p_Length,
                        const f32 p_Diameter, const Detail::DrawFlags p_Flags) noexcept
{
    fmat4 transform = p_Transform;
    Onyx::Transform<D3>::ScaleIntrinsic(transform, {p_Length, p_Diameter, p_Diameter});
    p_Renderer.DrawPrimitive(transform, Detail::Primitives<D3>::GetCylinderIndex(), p_Flags);

    fvec3 pos{0.f};
    pos.x = -0.5f * p_Length;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Diameter, p_Flags);
    pos.x = -pos.x;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Diameter, p_Flags);
}

void RenderContext<D3>::Capsule() noexcept
{
    const fmat4 &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawCapsule(m_Renderer, transform, p_Flags);
    };
    const auto outline = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawCapsule(m_Renderer, transform, 1.f, 1.f + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::Capsule(const fmat4 &p_Transform) noexcept
{
    const fmat4 transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawCapsule(m_Renderer, transform, p_Flags);
    };
    const auto outline = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawCapsule(m_Renderer, transform, 1.f, 1.f + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

void RenderContext<D3>::Capsule(const f32 p_Length, const f32 p_Radius) noexcept
{
    const fmat4 &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform, p_Length, p_Radius](const Detail::DrawFlags p_Flags) {
        drawCapsule(m_Renderer, transform, p_Length, 2.f * p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, p_Length, p_Radius](const Detail::DrawFlags p_Flags) {
        drawCapsule(m_Renderer, transform, p_Length, 2.f * p_Radius + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::Capsule(const fmat4 &p_Transform, const f32 p_Length, const f32 p_Radius) noexcept
{
    const fmat4 transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform, p_Length, p_Radius](const Detail::DrawFlags p_Flags) {
        drawCapsule(m_Renderer, transform, p_Length, 2.f * p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, p_Length, p_Radius](const Detail::DrawFlags p_Flags) {
        drawCapsule(m_Renderer, transform, p_Length, 2.f * p_Radius + m_RenderState.back().OutlineWidth, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}

static void drawRoundedCubeEdges(Detail::Renderer<D3> &p_Renderer, const fmat4 &p_Transform,
                                 const Detail::DrawFlags p_Flags, const fvec3 &p_Dimensions = fvec3{1.f},
                                 const f32 p_Radius = 0.5f) noexcept
{
    const fvec3 halfDims = 0.5f * p_Dimensions;
    const fvec3 paddedDims = halfDims + 0.5f * p_Radius;
    for (u32 i = 0; i < 6; ++i)
    {
        fmat4 transform = p_Transform;
        const u32 index1 = i % 3;
        const u32 index2 = (i + 1) % 3;
        const u32 index3 = (i + 2) % 3;
        const f32 dim = i < 3 ? paddedDims[index1] : -paddedDims[index1];
        Onyx::Transform<D3>::TranslateIntrinsic(transform, index1, dim);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index1, p_Radius);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index2, p_Dimensions[index2]);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index3, p_Dimensions[index3]);
        p_Renderer.DrawPrimitive(transform, Detail::Primitives<D3>::GetCubeIndex(), p_Flags);
    }
    const f32 diameter = 2.f * p_Radius;
    fvec3 pos = halfDims;
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
        const fvec4 relevantDims = {halfDims[dimIndex1], -halfDims[dimIndex1], halfDims[dimIndex2],
                                    -halfDims[dimIndex2]};
        for (u32 i = 0; i < 2; ++i)
            for (u32 j = 0; j < 2; ++j)
            {
                pos = fvec3{0.f};
                pos[dimIndex1] = relevantDims[i];
                pos[dimIndex2] = relevantDims[2 + j];

                fmat4 transform = p_Transform;
                Onyx::Transform<D3>::TranslateIntrinsic(transform, pos);
                if (axis > 0)
                    Onyx::Transform<D3>::RotateZIntrinsic(transform, glm::half_pi<f32>());
                if (axis > 1)
                    Onyx::Transform<D3>::RotateYIntrinsic(transform, glm::half_pi<f32>());
                Onyx::Transform<D3>::ScaleIntrinsic(transform, {p_Dimensions[axis], diameter, diameter});
                p_Renderer.DrawPrimitive(transform, Detail::Primitives<D3>::GetCylinderIndex(), p_Flags);
            }
    }
}

static void drawRoundedCube(Detail::Renderer<D3> &p_Renderer, const fmat4 &p_Transform,
                            const Detail::DrawFlags p_Flags) noexcept
{
    p_Renderer.DrawPrimitive(p_Transform, Detail::Primitives<D3>::GetSquareIndex(), p_Flags);
    drawRoundedCubeEdges(p_Renderer, p_Transform, p_Flags);
}
static void drawRoundedCube(Detail::Renderer<D3> &p_Renderer, const fmat4 &p_Transform, const fvec3 &p_Dimensions,
                            const f32 p_Radius, const Detail::DrawFlags p_Flags) noexcept
{
    fmat4 transform = p_Transform;
    Onyx::Transform<D3>::ScaleIntrinsic(transform, p_Dimensions);
    p_Renderer.DrawPrimitive(transform, Detail::Primitives<D3>::GetCubeIndex(), p_Flags);

    drawRoundedCubeEdges(p_Renderer, p_Transform, p_Flags, p_Dimensions, p_Radius);
}

void RenderContext<D3>::RoundedCube() noexcept
{
    const fmat4 &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawRoundedCube(m_Renderer, transform, p_Flags);
    };
    const auto outline = [this, &transform](const Detail::DrawFlags p_Flags) {
        const f32 radius = 0.5f + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedCube(m_Renderer, transform, fvec3{1.f}, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::RoundedCube(const fmat4 &p_Transform) noexcept
{
    const fmat4 transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform](const Detail::DrawFlags p_Flags) {
        drawRoundedCube(m_Renderer, transform, p_Flags);
    };
    const auto outline = [this, &transform](const Detail::DrawFlags p_Flags) {
        const f32 radius = 0.5f + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedCube(m_Renderer, transform, fvec3{1.f}, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::RoundedCube(const fvec3 &p_Dimensions, const f32 p_Radius) noexcept
{
    const fmat4 &transform = m_RenderState.back().Transform;
    const auto fill = [this, &transform, &p_Dimensions, p_Radius](const Detail::DrawFlags p_Flags) {
        drawRoundedCube(m_Renderer, transform, p_Dimensions, p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, &p_Dimensions, p_Radius](const Detail::DrawFlags p_Flags) {
        const f32 radius = p_Radius + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedCube(m_Renderer, transform, p_Dimensions, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::RoundedCube(const fmat4 &p_Transform, const fvec3 &p_Dimensions, const f32 p_Radius) noexcept
{
    const fmat4 transform = p_Transform * m_RenderState.back().Transform;
    const auto fill = [this, &transform, &p_Dimensions, p_Radius](const Detail::DrawFlags p_Flags) {
        drawRoundedCube(m_Renderer, transform, p_Dimensions, p_Radius, p_Flags);
    };
    const auto outline = [this, &transform, &p_Dimensions, p_Radius](const Detail::DrawFlags p_Flags) {
        const f32 radius = p_Radius + 0.5f * m_RenderState.back().OutlineWidth;
        drawRoundedCube(m_Renderer, transform, p_Dimensions, radius, p_Flags);
    };
    resolveDrawCallWithFlagsBasedOnState(fill, outline, m_RenderState.back().Fill, m_RenderState.back().Outline);
}
void RenderContext<D3>::RoundedCube(const f32 p_Width, const f32 p_Height, const f32 p_Depth,
                                    const f32 p_Radius) noexcept
{
    RoundedCube(fvec3{p_Width, p_Height, p_Depth}, p_Radius);
}
void RenderContext<D3>::RoundedCube(const fmat4 &p_Transform, const f32 p_Width, const f32 p_Height, const f32 p_Depth,
                                    const f32 p_Radius) noexcept
{
    RoundedCube(p_Transform, fvec3{p_Width, p_Height, p_Depth}, p_Radius);
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

void RenderContext<D3>::DirectionalLight(Onyx::DirectionalLight p_Light) noexcept
{
    const fmat4 transform = m_RenderState.back().Axes * m_RenderState.back().Transform;
    fvec4 direction = p_Light.DirectionAndIntensity;
    direction.w = 0.f;
    direction = transform * direction;

    p_Light.DirectionAndIntensity = fvec4{glm::normalize(fvec3{direction}), p_Light.DirectionAndIntensity.w};
    m_Renderer.AddDirectionalLight(p_Light);
}
void RenderContext<D3>::DirectionalLight(const fvec3 &p_Direction, const f32 p_Intensity) noexcept
{
    Onyx::DirectionalLight light;
    light.DirectionAndIntensity = fvec4{p_Direction, p_Intensity};
    light.Color = m_RenderState.back().LightColor;
    DirectionalLight(light);
}
void RenderContext<D3>::DirectionalLight(const f32 p_DX, const f32 p_DY, const f32 p_DZ, const f32 p_Intensity) noexcept
{
    DirectionalLight(fvec3{p_DX, p_DY, p_DZ}, p_Intensity);
}

void RenderContext<D3>::PointLight(Onyx::PointLight p_Light) noexcept
{
    const fmat4 transform = m_RenderState.back().Axes * m_RenderState.back().Transform;
    fvec4 position = p_Light.PositionAndIntensity;
    position.w = 1.f;
    position = transform * position;
    position.w = p_Light.PositionAndIntensity.w;
    p_Light.PositionAndIntensity = position;
    m_Renderer.AddPointLight(p_Light);
}
void RenderContext<D3>::PointLight(const fvec3 &p_Position, const f32 p_Radius, const f32 p_Intensity) noexcept
{
    Onyx::PointLight light;
    light.PositionAndIntensity = fvec4{p_Position, p_Intensity};
    light.Radius = p_Radius;
    light.Color = m_RenderState.back().LightColor;
    PointLight(light);
}
void RenderContext<D3>::PointLight(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius,
                                   const f32 p_Intensity) noexcept
{
    PointLight(fvec3{p_X, p_Y, p_Z}, p_Radius, p_Intensity);
}
void RenderContext<D3>::PointLight(const f32 p_Radius, const f32 p_Intensity) noexcept
{
    PointLight(fvec3{0.f}, p_Radius, p_Intensity);
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

fvec3 RenderContext<D3>::GetViewLookDirectionInCurrentAxes() const noexcept
{
    return glm::normalize(GetCoordinates(fvec3{0.f, 0.f, 1.f}));
}

template <Dimension D> void Detail::IRenderContext<D>::Fill(const bool p_Enabled) noexcept
{
    m_RenderState.back().Fill = p_Enabled;
}

template <Dimension D> void Detail::IRenderContext<D>::Mesh(const Model<D> &p_Model) noexcept
{
    m_Renderer.DrawMesh(m_RenderState.back().Transform, p_Model);
}
template <Dimension D>
void Detail::IRenderContext<D>::Mesh(const fmat<D> &p_Transform, const Model<D> &p_Model) noexcept
{
    m_Renderer.DrawMesh(p_Transform * m_RenderState.back().Transform, p_Model);
}

template <Dimension D> void Detail::IRenderContext<D>::Push() noexcept
{
    m_RenderState.push_back(m_RenderState.back());
}
template <Dimension D> void Detail::IRenderContext<D>::PushAndClear() noexcept
{
    m_RenderState.push_back(RenderState<D>{});
}
template <Dimension D> void Detail::IRenderContext<D>::Pop() noexcept
{
    TKIT_ASSERT(m_RenderState.size() > 1, "[ONYX] For every push, there must be a pop");
    m_RenderState.pop_back();
}

template <Dimension D> void Detail::IRenderContext<D>::Alpha(const f32 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = p_Alpha;
}
template <Dimension D> void Detail::IRenderContext<D>::Alpha(const u8 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = static_cast<f32>(p_Alpha) / 255.f;
}
template <Dimension D> void Detail::IRenderContext<D>::Alpha(const u32 p_Alpha) noexcept
{
    m_RenderState.back().Material.Color.RGBA.a = static_cast<f32>(p_Alpha) / 255.f;
}

template <Dimension D> void Detail::IRenderContext<D>::Fill(const Color &p_Color) noexcept
{
    Fill(true);
    m_RenderState.back().Material.Color = p_Color;
}
template <Dimension D> void Detail::IRenderContext<D>::Outline(const bool p_Enabled) noexcept
{
    m_RenderState.back().Outline = p_Enabled;
}
template <Dimension D> void Detail::IRenderContext<D>::Outline(const Color &p_Color) noexcept
{
    Outline(true);
    m_RenderState.back().OutlineColor = p_Color;
}
template <Dimension D> void Detail::IRenderContext<D>::OutlineWidth(const f32 p_Width) noexcept
{
    Outline(true);
    m_RenderState.back().OutlineWidth = p_Width;
}

template <Dimension D> void Detail::IRenderContext<D>::Material(const MaterialData<D> &p_Material) noexcept
{
    m_RenderState.back().Material = p_Material;
}

template <Dimension D> const RenderState<D> &Detail::IRenderContext<D>::GetCurrentState() const noexcept
{
    return m_RenderState.back();
}
template <Dimension D> RenderState<D> &Detail::IRenderContext<D>::GetCurrentState() noexcept
{
    return m_RenderState.back();
}

template <Dimension D> const ProjectionViewData<D> &Detail::IRenderContext<D>::GetProjectionViewData() const noexcept
{
    return m_ProjectionView;
}

template <Dimension D> Onyx::Transform<D> Detail::IRenderContext<D>::GetViewTransformInCurrentAxes() const noexcept
{
    const fmat<D> vmat = glm::inverse(m_RenderState.back().Axes) * m_ProjectionView.View.ComputeTransform();
    return Onyx::Transform<D>::Extract(vmat);
}

template <Dimension D> void Detail::IRenderContext<D>::SetView(const Onyx::Transform<D> &p_View) noexcept
{
    m_ProjectionView.View = p_View;
    if constexpr (D == D2)
        m_ProjectionView.ProjectionView = p_View.ComputeInverseTransform();
    else
    {
        fmat4 vmat = p_View.ComputeInverseTransform();
        ApplyCoordinateSystemExtrinsic(vmat);
        m_ProjectionView.ProjectionView = m_ProjectionView.Projection * vmat;
    }
}

template <Dimension D> void Detail::IRenderContext<D>::Render(const VkCommandBuffer p_Commandbuffer) noexcept
{
    m_Renderer.Render(p_Commandbuffer);
}

template <Dimension D> void Detail::IRenderContext<D>::Axes(const f32 p_Thickness, const f32 p_Size) noexcept
{
    // TODO: Parametrize this
    Color &color = m_RenderState.back().Material.Color;
    const Color oldColor = color; // A cheap filthy push
    if constexpr (D == D2)
    {
        const fvec2 xLeft = fvec2{-p_Size, 0.f};
        const fvec2 xRight = fvec2{p_Size, 0.f};

        const fvec2 yDown = fvec2{0.f, -p_Size};
        const fvec2 yUp = fvec2{0.f, p_Size};

        color = Color{245u, 64u, 90u};
        Line(xLeft, xRight, p_Thickness);
        color = Color{65u, 135u, 245u};
        Line(yDown, yUp, p_Thickness);
    }
    else
    {
        const fvec3 xLeft = fvec3{-p_Size, 0.f, 0.f};
        const fvec3 xRight = fvec3{p_Size, 0.f, 0.f};

        const fvec3 yDown = fvec3{0.f, -p_Size, 0.f};
        const fvec3 yUp = fvec3{0.f, p_Size, 0.f};

        const fvec3 zBack = fvec3{0.f, 0.f, -p_Size};
        const fvec3 zFront = fvec3{0.f, 0.f, p_Size};

        color = Color{245u, 64u, 90u};
        Line(xLeft, xRight, p_Thickness);
        color = Color{65u, 135u, 245u};
        Line(yDown, yUp, p_Thickness);
        color = Color{180u, 245u, 65u};
        Line(zBack, zFront, p_Thickness);
    }
    color = oldColor; // A cheap filthy pop
}

template <Dimension D>
void Detail::IRenderContext<D>::ApplyCameraMovementControls(const CameraMovementControls<D> &p_Controls) noexcept
{
    Onyx::Transform<D> &view = m_ProjectionView.View;
    fvec<D> translation{0.f};
    if (Input::IsKeyPressed(m_Window, p_Controls.Left))
        translation.x -= view.Scale.x * p_Controls.TranslationStep;
    if (Input::IsKeyPressed(m_Window, p_Controls.Right))
        translation.x += view.Scale.x * p_Controls.TranslationStep;

    if (Input::IsKeyPressed(m_Window, p_Controls.Up))
        translation.y += view.Scale.y * p_Controls.TranslationStep;
    if (Input::IsKeyPressed(m_Window, p_Controls.Down))
        translation.y -= view.Scale.y * p_Controls.TranslationStep;

    if constexpr (D == D2)
    {
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateLeft))
            view.Rotation += p_Controls.RotationStep;
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateRight))
            view.Rotation -= p_Controls.RotationStep;
    }
    else
    {
        if (Input::IsKeyPressed(m_Window, p_Controls.Forward))
            translation.z -= view.Scale.z * p_Controls.TranslationStep;
        if (Input::IsKeyPressed(m_Window, p_Controls.Backward))
            translation.z += view.Scale.z * p_Controls.TranslationStep;

        const fvec2 mpos = Input::GetMousePosition(m_Window);

        const bool lookAround = Input::IsKeyPressed(m_Window, p_Controls.ToggleLookAround);
        const fvec2 delta = lookAround ? 3.f * (m_PrevMousePos - mpos) : fvec2{0.f};
        m_PrevMousePos = mpos;

        fvec3 angles{delta.y, delta.x, 0.f};
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateLeft))
            angles.z += p_Controls.RotationStep;
        if (Input::IsKeyPressed(m_Window, p_Controls.RotateRight))
            angles.z -= p_Controls.RotationStep;

        view.Rotation *= quat{angles};
    }

    const auto rmat = Onyx::Transform<D>::ComputeRotationMatrix(view.Rotation);
    view.Translation += rmat * translation;

    if constexpr (D == D2)
        m_ProjectionView.ProjectionView = view.ComputeInverseTransform();
    else
    {
        fmat4 vmat = view.ComputeInverseTransform();
        ApplyCoordinateSystemExtrinsic(vmat);
        m_ProjectionView.ProjectionView = m_ProjectionView.Projection * vmat;
    }
}
template <Dimension D>
void Detail::IRenderContext<D>::ApplyCameraMovementControls(const TKit::Timespan p_DeltaTime) noexcept
{
    CameraMovementControls<D> controls{};
    controls.TranslationStep = p_DeltaTime.AsSeconds();
    controls.RotationStep = p_DeltaTime.AsSeconds();
    ApplyCameraMovementControls(controls);
}

void RenderContext<D2>::ApplyCameraScalingControls(const f32 p_ScaleStep) noexcept
{
    fmat4 transform = transform3ToTransform4(m_ProjectionView.View.ComputeTransform());
    ApplyCoordinateSystemIntrinsic(transform);
    const fvec2 mpos = transform * fvec4{Input::GetMousePosition(m_Window), 0.f, 1.f};

    const fvec2 dpos = p_ScaleStep * (mpos - m_ProjectionView.View.Translation);
    m_ProjectionView.View.Translation += dpos;
    m_ProjectionView.View.Scale *= 1.f - p_ScaleStep;

    m_ProjectionView.ProjectionView = m_ProjectionView.View.ComputeInverseTransform();
}

template <Dimension D> fvec<D> Detail::IRenderContext<D>::GetCoordinates(const fvec<D> &p_NormalizedPos) const noexcept
{
    if constexpr (D == D2)
    {
        const fmat3 itransform3 = glm::inverse(m_ProjectionView.ProjectionView * m_RenderState.back().Axes);
        fmat4 itransform = transform3ToTransform4(itransform3);
        ApplyCoordinateSystemIntrinsic(itransform);
        return itransform * fvec4{p_NormalizedPos, 0.f, 1.f};
    }
    else
    {
        const fmat4 transform = m_ProjectionView.ProjectionView * m_RenderState.back().Axes;
        const fvec4 clip = glm::inverse(transform) * fvec4{p_NormalizedPos, 1.f};
        return fvec3{clip} / clip.w;
    }
}
fvec2 RenderContext<D2>::GetMouseCoordinates() const noexcept
{
    return GetCoordinates(Input::GetMousePosition(m_Window));
}
fvec3 RenderContext<D3>::GetMouseCoordinates(const f32 p_Depth) const noexcept
{
    return GetCoordinates(fvec3{Input::GetMousePosition(m_Window), p_Depth});
}

void RenderContext<D3>::SetProjection(const fmat4 &p_Projection) noexcept
{
    m_ProjectionView.Projection = p_Projection;

    fmat4 vmat = m_ProjectionView.View.ComputeInverseTransform();
    ApplyCoordinateSystemExtrinsic(vmat);
    m_ProjectionView.ProjectionView = p_Projection * vmat;
}
void RenderContext<D3>::SetPerspectiveProjection(const f32 p_FieldOfView, const f32 p_Near, const f32 p_Far) noexcept
{
    fmat4 projection{0.f};
    const f32 invHalfPov = 1.f / glm::tan(0.5f * p_FieldOfView);

    projection[0][0] = invHalfPov; // Aspect applied in view
    projection[1][1] = invHalfPov;
    projection[2][2] = p_Far / (p_Far - p_Near);
    projection[2][3] = 1.f;
    projection[3][2] = p_Far * p_Near / (p_Near - p_Far);
    SetProjection(projection);
}

void RenderContext<D3>::SetOrthographicProjection() noexcept
{
    SetProjection(fmat4{1.f});
}

template class Detail::IRenderContext<D2>;
template class Detail::IRenderContext<D3>;

} // namespace Onyx