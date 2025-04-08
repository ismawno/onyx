#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"

#include "tkit/utils/math.hpp"

namespace Onyx
{
using namespace Detail;

template <Dimension D>
IRenderContext<D>::IRenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : m_Renderer(p_RenderPass), m_Window(p_Window)
{
    m_StateStack.push_back(RenderState<D>{});
    m_State = &m_StateStack.back();
    UpdateViewAspect(p_Window->GetScreenAspect());
}

template <Dimension D> void IRenderContext<D>::FlushState() noexcept
{
    TKIT_ASSERT(m_StateStack.size() == 1, "[ONYX] For every push, there must be a pop");
    m_StateStack[0] = RenderState<D>{};
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

template <Dimension D> void IRenderContext<D>::Transform(const fmat<D> &p_Transform) noexcept
{
    m_State->Transform = p_Transform * m_State->Transform;
}
template <Dimension D>
void IRenderContext<D>::Transform(const fvec<D> &p_Translation, const fvec<D> &p_Scale,
                                  const rot<D> &p_Rotation) noexcept
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
template <Dimension D>
void IRenderContext<D>::Transform(const fvec<D> &p_Translation, const f32 p_Scale, const rot<D> &p_Rotation) noexcept
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

template <Dimension D> void IRenderContext<D>::TransformAxes(const fmat<D> &p_Axes) noexcept
{
    m_State->Axes *= p_Axes;
}
template <Dimension D>
void IRenderContext<D>::TransformAxes(const fvec<D> &p_Translation, const fvec<D> &p_Scale,
                                      const rot<D> &p_Rotation) noexcept
{
    m_State->Axes *= Onyx::Transform<D>::ComputeReversedTransform(p_Translation, p_Scale, p_Rotation);
}
template <Dimension D>
void IRenderContext<D>::TransformAxes(const fvec<D> &p_Translation, const f32 p_Scale,
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

template <Dimension D> void IRenderContext<D>::Translate(const fvec<D> &p_Translation) noexcept
{
    Onyx::Transform<D>::TranslateExtrinsic(m_State->Transform, p_Translation);
}

template <Dimension D> void IRenderContext<D>::Scale(const fvec<D> &p_Scale) noexcept
{
    Onyx::Transform<D>::ScaleExtrinsic(m_State->Transform, p_Scale);
}
template <Dimension D> void IRenderContext<D>::Scale(const f32 p_Scale) noexcept
{
    Scale(fvec<D>{p_Scale});
}

template <Dimension D> void IRenderContext<D>::TranslateX(const f32 p_X) noexcept
{
    Onyx::Transform<D>::TranslateExtrinsic(m_State->Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::TranslateY(const f32 p_Y) noexcept
{
    Onyx::Transform<D>::TranslateExtrinsic(m_State->Transform, 1, p_Y);
}
void RenderContext<D3>::TranslateZ(const f32 p_Z) noexcept
{
    Onyx::Transform<D3>::TranslateExtrinsic(m_State->Transform, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::ScaleX(const f32 p_X) noexcept
{
    Onyx::Transform<D>::ScaleExtrinsic(m_State->Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::ScaleY(const f32 p_Y) noexcept
{
    Onyx::Transform<D>::ScaleExtrinsic(m_State->Transform, 1, p_Y);
}
void RenderContext<D3>::ScaleZ(const f32 p_Z) noexcept
{
    Onyx::Transform<D3>::ScaleExtrinsic(m_State->Transform, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::TranslateXAxis(const f32 p_X) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(m_State->Axes, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::TranslateYAxis(const f32 p_Y) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(m_State->Axes, 1, p_Y);
}
void RenderContext<D3>::TranslateZAxis(const f32 p_Z) noexcept
{
    Onyx::Transform<D3>::TranslateIntrinsic(m_State->Axes, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::ScaleXAxis(const f32 p_X) noexcept
{
    Onyx::Transform<D>::ScaleIntrinsic(m_State->Axes, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::ScaleYAxis(const f32 p_Y) noexcept
{
    Onyx::Transform<D>::ScaleIntrinsic(m_State->Axes, 1, p_Y);
}
void RenderContext<D3>::ScaleZAxis(const f32 p_Z) noexcept
{
    Onyx::Transform<D3>::ScaleIntrinsic(m_State->Axes, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::UpdateViewAspect(const f32 p_Aspect) noexcept
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

template <Dimension D> void IRenderContext<D>::TranslateAxes(const fvec<D> &p_Translation) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(m_State->Axes, p_Translation);
}

template <Dimension D> void IRenderContext<D>::ScaleAxes(const fvec<D> &p_Scale) noexcept
{
    Onyx::Transform<D>::ScaleIntrinsic(m_State->Axes, p_Scale);
}
template <Dimension D> void IRenderContext<D>::ScaleAxes(const f32 p_Scale) noexcept
{
    ScaleAxes(fvec<D>{p_Scale});
}

void RenderContext<D2>::Rotate(const f32 p_Angle) noexcept
{
    Onyx::Transform<D2>::RotateExtrinsic(m_State->Transform, p_Angle);
}

void RenderContext<D3>::Rotate(const quat &p_Quaternion) noexcept
{
    Onyx::Transform<D3>::RotateExtrinsic(m_State->Transform, p_Quaternion);
}
void RenderContext<D3>::Rotate(const f32 p_Angle, const fvec3 &p_Axis) noexcept
{
    Rotate(glm::angleAxis(p_Angle, p_Axis));
}
void RenderContext<D3>::Rotate(const fvec3 &p_Angles) noexcept
{
    Rotate(glm::quat(p_Angles));
}

// This could be optimized a bit
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
    Onyx::Transform<D2>::RotateIntrinsic(m_State->Axes, p_Angle);
}
void RenderContext<D3>::RotateAxes(const quat &p_Quaternion) noexcept
{
    Onyx::Transform<D3>::RotateIntrinsic(m_State->Axes, p_Quaternion);
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

template <Dimension D>
template <typename F1, typename F2>
void IRenderContext<D>::resolveDrawFlagsWithState(F1 &&p_FillDraw, F2 &&p_OutlineDraw) noexcept
{
    if (m_State->Fill)
    {
        if (m_State->Outline)
        {
            std::forward<F1>(p_FillDraw)(DrawFlags_DoStencilWriteDoFill);
            std::forward<F2>(p_OutlineDraw)(DrawFlags_DoStencilTestNoFill);
        }
        else
            std::forward<F1>(p_FillDraw)(DrawFlags_NoStencilWriteDoFill);
    }
    else if (m_State->Outline)
    {
        std::forward<F1>(p_FillDraw)(DrawFlags_DoStencilWriteNoFill);
        std::forward<F2>(p_OutlineDraw)(DrawFlags_DoStencilTestNoFill);
    }
}

template <Dimension D> fmat4 IRenderContext<D>::computeFinalTransform(const fmat<D> &p_Transform) noexcept
{
    if constexpr (D == D2)
        return PromoteTransform(m_State->Axes * p_Transform);
    else
        return m_State->Axes * p_Transform;
}

template <Dimension D>
template <Dimension PDim>
void IRenderContext<D>::drawPrimitive(const fmat<D> &p_Transform, const u32 p_PrimitiveIndex) noexcept
{
    const auto fill = [this, &p_Transform, p_PrimitiveIndex](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(p_Transform), p_PrimitiveIndex, p_Flags);
    };
    const auto outline = [this, &p_Transform, p_PrimitiveIndex](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 thickness = 1.f + m_State->OutlineWidth;
        for (u32 i = 0; i < PDim; ++i)
            Onyx::Transform<D>::ScaleIntrinsic(transform, i, thickness);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), p_PrimitiveIndex, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}
template <Dimension D>
template <Dimension PDim>
void IRenderContext<D>::drawPrimitive(const fmat<D> &p_Transform, const u32 p_PrimitiveIndex,
                                      const fvec<PDim> &p_Dimensions) noexcept
{
    const auto fill = [this, &p_Transform, &p_Dimensions, p_PrimitiveIndex](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        for (u32 i = 0; i < PDim; ++i)
            Onyx::Transform<D>::ScaleIntrinsic(transform, i, p_Dimensions[i]);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), p_PrimitiveIndex, p_Flags);
    };
    const auto outline = [this, &p_Transform, &p_Dimensions, p_PrimitiveIndex](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 width = m_State->OutlineWidth;
        for (u32 i = 0; i < PDim; ++i)
            Onyx::Transform<D>::ScaleIntrinsic(transform, i, p_Dimensions[i] + width);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), p_PrimitiveIndex, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}

template <Dimension D> void IRenderContext<D>::Triangle() noexcept
{
    drawPrimitive<D2>(m_State->Transform, Primitives<D>::GetTriangleIndex());
}
template <Dimension D> void IRenderContext<D>::Triangle(const fmat<D> &p_Transform) noexcept
{
    drawPrimitive<D2>(p_Transform * m_State->Transform, Primitives<D>::GetTriangleIndex());
}
template <Dimension D> void IRenderContext<D>::Triangle(const fvec2 &p_Dimensions) noexcept
{
    drawPrimitive<D2>(m_State->Transform, Primitives<D>::GetTriangleIndex(), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Triangle(const fmat<D> &p_Transform, const fvec2 &p_Dimensions) noexcept
{
    drawPrimitive<D2>(p_Transform * m_State->Transform, Primitives<D>::GetTriangleIndex(), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Triangle(const f32 p_Size) noexcept
{
    Triangle(fvec2{p_Size});
}
template <Dimension D> void IRenderContext<D>::Triangle(const fmat<D> &p_Transform, const f32 p_Size) noexcept
{
    Triangle(p_Transform, fvec2{p_Size});
}

template <Dimension D> void IRenderContext<D>::Square() noexcept
{
    drawPrimitive<D2>(m_State->Transform, Primitives<D>::GetSquareIndex());
}
template <Dimension D> void IRenderContext<D>::Square(const fmat<D> &p_Transform) noexcept
{
    drawPrimitive<D2>(p_Transform * m_State->Transform, Primitives<D>::GetSquareIndex());
}
template <Dimension D> void IRenderContext<D>::Square(const fvec2 &p_Dimensions) noexcept
{
    drawPrimitive<D2>(m_State->Transform, Primitives<D>::GetSquareIndex(), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Square(const fmat<D> &p_Transform, const fvec2 &p_Dimensions) noexcept
{
    drawPrimitive<D2>(p_Transform * m_State->Transform, Primitives<D>::GetSquareIndex(), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Square(const f32 p_Size) noexcept
{
    Square(fvec2{p_Size});
}
template <Dimension D> void IRenderContext<D>::Square(const fmat<D> &p_Transform, const f32 p_Size) noexcept
{
    Square(p_Transform, fvec2{p_Size});
}

template <Dimension D> void IRenderContext<D>::NGon(const u32 p_Sides) noexcept
{
    drawPrimitive<D2>(m_State->Transform, Primitives<D>::GetNGonIndex(p_Sides));
}
template <Dimension D> void IRenderContext<D>::NGon(const fmat<D> &p_Transform, const u32 p_Sides) noexcept
{
    drawPrimitive<D2>(p_Transform * m_State->Transform, Primitives<D>::GetNGonIndex(p_Sides));
}
template <Dimension D> void IRenderContext<D>::NGon(const u32 p_Sides, const fvec2 &p_Dimensions) noexcept
{
    drawPrimitive<D2>(m_State->Transform, Primitives<D>::GetNGonIndex(p_Sides), p_Dimensions);
}
template <Dimension D>
void IRenderContext<D>::NGon(const fmat<D> &p_Transform, const u32 p_Sides, const fvec2 &p_Dimensions) noexcept
{
    drawPrimitive<D2>(p_Transform * m_State->Transform, Primitives<D>::GetNGonIndex(p_Sides), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::NGon(const u32 p_Sides, const f32 p_Size) noexcept
{
    NGon(p_Sides, fvec2{p_Size});
}
template <Dimension D>
void IRenderContext<D>::NGon(const fmat<D> &p_Transform, const u32 p_Sides, const f32 p_Size) noexcept
{
    NGon(p_Transform, p_Sides, fvec2{p_Size});
}

template <Dimension D>
void IRenderContext<D>::drawConvexPolygon(const fmat<D> &p_Transform, const TKit::Span<const fvec2> p_Vertices) noexcept
{
    const auto fill = [this, &p_Transform, p_Vertices](const DrawFlags p_Flags) {
        m_Renderer.DrawPolygon(m_State, computeFinalTransform(p_Transform), p_Vertices, p_Flags);
    };
    const auto outline = [this, &p_Transform, p_Vertices](const DrawFlags p_Flags) {
        TKIT_ASSERT(p_Vertices.size() < ONYX_MAX_POLYGON_VERTICES,
                    "[ONYX] The provided vertices ({}) exceed the maximum: {}", p_Vertices.size(),
                    ONYX_MAX_POLYGON_VERTICES);
        TKit::StaticArray<fvec2, ONYX_MAX_POLYGON_VERTICES> vertices;
        const f32 width = m_State->OutlineWidth;
        for (u32 prev = 0; prev < p_Vertices.size(); ++prev)
        {
            const u32 current = (prev + 1) % p_Vertices.size();
            const u32 next = (current + 1) % p_Vertices.size();

            const fvec2 edge1 = p_Vertices[current] - p_Vertices[prev];
            const fvec2 edge2 = p_Vertices[next] - p_Vertices[current];

            const fvec2 normal1 = glm::normalize(fvec2{edge1.y, -edge1.x});
            const fvec2 normal2 = glm::normalize(fvec2{edge2.y, -edge2.x});
            vertices.push_back(p_Vertices[current] + width * glm::normalize(normal1 + normal2));
        }
        m_Renderer.DrawPolygon(m_State, computeFinalTransform(p_Transform), vertices, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}

template <Dimension D> void IRenderContext<D>::ConvexPolygon(const TKit::Span<const fvec2> p_Vertices) noexcept
{
    drawConvexPolygon(m_State->Transform, p_Vertices);
}
template <Dimension D>
void IRenderContext<D>::ConvexPolygon(const fmat<D> &p_Transform, const TKit::Span<const fvec2> p_Vertices) noexcept
{
    drawConvexPolygon(p_Transform * m_State->Transform, p_Vertices);
}

template <Dimension D>
void IRenderContext<D>::drawCircle(const fmat<D> &p_Transform, const CircleOptions &p_Options) noexcept
{
    const auto fill = [this, &p_Transform, &p_Options](const DrawFlags p_Flags) {
        m_Renderer.DrawCircle(m_State, computeFinalTransform(p_Transform), p_Options, p_Flags);
    };
    const auto outline = [this, &p_Transform, &p_Options](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 diameter = 1.f + m_State->OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, diameter);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, diameter);

        m_Renderer.DrawCircle(m_State, computeFinalTransform(transform), p_Options, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}
template <Dimension D>
void IRenderContext<D>::drawCircle(const fmat<D> &p_Transform, const CircleOptions &p_Options,
                                   const fvec2 &p_Dimensions) noexcept
{
    const auto fill = [this, &p_Transform, &p_Options, &p_Dimensions](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Dimensions[0]);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Dimensions[1]);
        m_Renderer.DrawCircle(m_State, computeFinalTransform(transform), p_Options, p_Flags);
    };
    const auto outline = [this, &p_Transform, &p_Options, &p_Dimensions](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 diameter = m_State->OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, diameter + p_Dimensions[0]);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, diameter + p_Dimensions[1]);

        m_Renderer.DrawCircle(m_State, computeFinalTransform(transform), p_Options, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}

template <Dimension D> void IRenderContext<D>::Circle(const CircleOptions &p_Options) noexcept
{
    drawCircle(m_State->Transform, p_Options);
}
template <Dimension D>
void IRenderContext<D>::Circle(const fmat<D> &p_Transform, const CircleOptions &p_Options) noexcept
{
    drawCircle(p_Transform * m_State->Transform, p_Options);
}
template <Dimension D>
void IRenderContext<D>::Circle(const fvec2 &p_Dimensions, const CircleOptions &p_Options) noexcept
{
    drawCircle(m_State->Transform, p_Options, p_Dimensions);
}
template <Dimension D>
void IRenderContext<D>::Circle(const fmat<D> &p_Transform, const fvec2 &p_Dimensions,
                               const CircleOptions &p_Options) noexcept
{
    drawCircle(p_Transform * m_State->Transform, p_Options, p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Circle(const f32 p_Diameter, const CircleOptions &p_Options) noexcept
{
    Circle(fvec2{p_Diameter}, p_Options);
}
template <Dimension D>
void IRenderContext<D>::Circle(const fmat<D> &p_Transform, const f32 p_Diameter,
                               const CircleOptions &p_Options) noexcept
{
    Circle(p_Transform, fvec2{p_Diameter}, p_Options);
}

template <Dimension D>
void IRenderContext<D>::drawChildCircle(fmat<D> p_Transform, const fvec<D> &p_Position, const CircleOptions &p_Options,
                                        const DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(p_Transform, p_Position);
    m_Renderer.DrawCircle(m_State, computeFinalTransform(p_Transform), p_Options, p_Flags);
}
template <Dimension D>
void IRenderContext<D>::drawChildCircle(fmat<D> p_Transform, const fvec<D> &p_Position, const f32 p_Diameter,
                                        const CircleOptions &p_Options, const DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D>::TranslateIntrinsic(p_Transform, p_Position);
    Onyx::Transform<D>::ScaleIntrinsic(p_Transform, 0, p_Diameter);
    Onyx::Transform<D>::ScaleIntrinsic(p_Transform, 1, p_Diameter);
    m_Renderer.DrawCircle(m_State, computeFinalTransform(p_Transform), p_Options, p_Flags);
}

template <Dimension D>
void IRenderContext<D>::drawStadiumMoons(const fmat<D> &p_Transform, const DrawFlags p_Flags) noexcept
{
    fvec<D> pos{0.f};
    pos.x = -0.5f;
    drawChildCircle(p_Transform, pos,
                    CircleOptions{.LowerAngle = glm::radians(90.f), .UpperAngle = glm::radians(270.f)}, p_Flags);
    pos.x = -pos.x;
    drawChildCircle(p_Transform, pos,
                    CircleOptions{.LowerAngle = glm::radians(-90.f), .UpperAngle = glm::radians(90.f)}, p_Flags);
}
template <Dimension D>
void IRenderContext<D>::drawStadiumMoons(const fmat<D> &p_Transform, const f32 p_Length, const f32 p_Diameter,
                                         const DrawFlags p_Flags) noexcept
{
    fvec<D> pos{0.f};
    pos.x = -0.5f * p_Length;
    drawChildCircle(p_Transform, pos, p_Diameter,
                    CircleOptions{.LowerAngle = glm::radians(90.f), .UpperAngle = glm::radians(270.f)}, p_Flags);
    pos.x = -pos.x;
    drawChildCircle(p_Transform, pos, p_Diameter,
                    CircleOptions{.LowerAngle = glm::radians(-90.f), .UpperAngle = glm::radians(90.f)}, p_Flags);
}

template <Dimension D> void IRenderContext<D>::drawStadium(const fmat<D> &p_Transform) noexcept
{
    const auto fill = [this, &p_Transform](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(p_Transform), Primitives<D>::GetSquareIndex(), p_Flags);
        drawStadiumMoons(p_Transform, p_Flags);
    };
    const auto outline = [this, &p_Transform](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 diameter = 1.f + m_State->OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, diameter);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D>::GetSquareIndex(), p_Flags);
        drawStadiumMoons(p_Transform, 1.f, diameter, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}
template <Dimension D>
void IRenderContext<D>::drawStadium(const fmat<D> &p_Transform, const f32 p_Length, const f32 p_Diameter) noexcept
{
    const auto fill = [this, &p_Transform, p_Length, p_Diameter](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Length);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Diameter);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D>::GetSquareIndex(), p_Flags);
        drawStadiumMoons(p_Transform, p_Length, p_Diameter, p_Flags);
    };
    const auto outline = [this, &p_Transform, p_Length, p_Diameter](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 diameter = p_Diameter + m_State->OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Length);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, diameter);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D>::GetSquareIndex(), p_Flags);
        drawStadiumMoons(p_Transform, p_Length, diameter, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}

template <Dimension D> void IRenderContext<D>::Stadium() noexcept
{
    drawStadium(m_State->Transform);
}
template <Dimension D> void IRenderContext<D>::Stadium(const fmat<D> &p_Transform) noexcept
{
    drawStadium(p_Transform * m_State->Transform);
}

template <Dimension D> void IRenderContext<D>::Stadium(const f32 p_Length, const f32 p_Diameter) noexcept
{
    drawStadium(m_State->Transform, p_Length, p_Diameter);
}
template <Dimension D>
void IRenderContext<D>::Stadium(const fmat<D> &p_Transform, const f32 p_Length, const f32 p_Diameter) noexcept
{
    drawStadium(p_Transform * m_State->Transform, p_Length, p_Diameter);
}

template <Dimension D>
void IRenderContext<D>::drawRoundedSquareMoons(const fmat<D> &p_Transform, const fvec2 &p_Dimensions,
                                               const f32 p_Diameter, const DrawFlags p_Flags) noexcept
{
    const f32 radius = 0.5f * p_Diameter;
    const fvec2 halfDims = 0.5f * p_Dimensions;
    const fvec2 paddedDims = halfDims + 0.5f * radius;

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
        Onyx::Transform<D>::ScaleIntrinsic(transform, index1, radius);
        Onyx::Transform<D>::ScaleIntrinsic(transform, index2, p_Dimensions[index2]);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D>::GetSquareIndex(), p_Flags);

        const f32 a1 = i * glm::half_pi<f32>();
        const f32 a2 = a1 + glm::half_pi<f32>();
        drawChildCircle(p_Transform, pos, p_Diameter, CircleOptions{.LowerAngle = a1, .UpperAngle = a2}, p_Flags);
        pos[index1] = -pos[index1];
    }
}

template <Dimension D> void IRenderContext<D>::drawRoundedSquare(const fmat<D> &p_Transform) noexcept
{
    const auto fill = [this, &p_Transform](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(p_Transform), Primitives<D>::GetSquareIndex(), p_Flags);
        drawRoundedSquareMoons(p_Transform, fvec2{1.f}, 1.f, p_Flags);
    };
    const auto outline = [this, &p_Transform](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 thickness = 1.f + m_State->OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, thickness);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, thickness);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D>::GetSquareIndex(), p_Flags);

        drawRoundedSquareMoons(p_Transform, fvec2{1.f}, thickness, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}
template <Dimension D>
void IRenderContext<D>::drawRoundedSquare(const fmat<D> &p_Transform, const fvec2 &p_Dimensions,
                                          const f32 p_Diameter) noexcept
{
    const auto fill = [this, &p_Transform, &p_Dimensions, p_Diameter](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Dimensions[0]);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Dimensions[1]);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D>::GetSquareIndex(), p_Flags);

        drawRoundedSquareMoons(p_Transform, p_Dimensions, p_Diameter, p_Flags);
    };
    const auto outline = [this, &p_Transform, &p_Dimensions, p_Diameter](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 width = m_State->OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Dimensions[0] + width);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Dimensions[1] + width);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D>::GetSquareIndex(), p_Flags);

        drawRoundedSquareMoons(p_Transform, p_Dimensions, p_Diameter + width, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}

template <Dimension D> void IRenderContext<D>::RoundedSquare() noexcept
{
    drawRoundedSquare(m_State->Transform);
}
template <Dimension D> void IRenderContext<D>::RoundedSquare(const fmat<D> &p_Transform) noexcept
{
    drawRoundedSquare(p_Transform * m_State->Transform);
}
template <Dimension D> void IRenderContext<D>::RoundedSquare(const fvec2 &p_Dimensions, const f32 p_Diameter) noexcept
{
    drawRoundedSquare(m_State->Transform, p_Dimensions, p_Diameter);
}
template <Dimension D>
void IRenderContext<D>::RoundedSquare(const fmat<D> &p_Transform, const fvec2 &p_Dimensions,
                                      const f32 p_Diameter) noexcept
{
    drawRoundedSquare(p_Transform * m_State->Transform, p_Dimensions, p_Diameter);
}
template <Dimension D> void IRenderContext<D>::RoundedSquare(const f32 p_Size, const f32 p_Diameter) noexcept
{
    RoundedSquare(fvec2{p_Size}, p_Diameter);
}
template <Dimension D>
void IRenderContext<D>::RoundedSquare(const fmat<D> &p_Transform, const f32 p_Size, const f32 p_Diameter) noexcept
{
    RoundedSquare(p_Transform, fvec2{p_Size}, p_Diameter);
}

template <Dimension D> static rot<D> computeLineRotation(const fvec<D> &p_Start, const fvec<D> &p_End) noexcept
{
    const fvec<D> delta = p_End - p_Start;
    if constexpr (D == D2)
        return glm::atan(delta.y, delta.x);
    else
    {
        const fvec3 dir = glm::normalize(delta);
        const fvec3 r = {0.f, -dir.z, dir.y};
        const f32 theta = 0.5f * glm::acos(dir.x);
        if (!TKit::ApproachesZero(glm::length2(r)))
            return quat{glm::cos(theta), glm::normalize(r) * glm::sin(theta)};
        if (dir.x < 0.f)
            return quat{0.f, 0.f, 1.f, 0.f};
    }
    return RotType<D>::Identity;
}

void RenderContext<D2>::Line(const fvec2 &p_Start, const fvec2 &p_End, const f32 p_Thickness) noexcept
{
    const fvec2 delta = p_End - p_Start;
    fmat3 transform = m_State->Transform;
    Onyx::Transform<D2>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D2>::RotateIntrinsic(transform, computeLineRotation<D2>(p_Start, p_End));

    Square(transform, fvec2{glm::length(delta), p_Thickness});
}
void RenderContext<D3>::Line(const fvec3 &p_Start, const fvec3 &p_End, const LineOptions &p_Options) noexcept
{
    const fvec3 delta = p_End - p_Start;
    fmat4 transform = m_State->Transform;
    Onyx::Transform<D3>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D3>::RotateIntrinsic(transform, computeLineRotation<D3>(p_Start, p_End));
    Cylinder(transform, glm::length(delta), p_Options.Thickness, p_Options.Resolution);
}

void RenderContext<D2>::LineStrip(const TKit::Span<const fvec2> p_Points, const f32 p_Thickness) noexcept
{
    TKIT_ASSERT(p_Points.size() > 1, "[ONYX] A line strip must have at least two points");
    for (u32 i = 0; i < p_Points.size() - 1; ++i)
        Line(p_Points[i], p_Points[i + 1], p_Thickness);
}
void RenderContext<D3>::LineStrip(const TKit::Span<const fvec3> p_Points, const LineOptions &p_Options) noexcept
{
    TKIT_ASSERT(p_Points.size() > 1, "[ONYX] A line strip must have at least two points");
    for (u32 i = 0; i < p_Points.size() - 1; ++i)
        Line(p_Points[i], p_Points[i + 1], p_Options);
}

void RenderContext<D2>::RoundedLine(const fvec2 &p_Start, const fvec2 &p_End, const f32 p_Thickness) noexcept
{
    const fvec2 delta = p_End - p_Start;
    fmat3 transform = m_State->Transform;
    Onyx::Transform<D2>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D2>::RotateIntrinsic(transform, computeLineRotation<D2>(p_Start, p_End));

    Stadium(transform, glm::length(delta), p_Thickness);
}
void RenderContext<D3>::RoundedLine(const fvec3 &p_Start, const fvec3 &p_End, const LineOptions &p_Options) noexcept
{
    const fvec3 delta = p_End - p_Start;
    fmat4 transform = m_State->Transform;
    Onyx::Transform<D3>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D3>::RotateIntrinsic(transform, computeLineRotation<D3>(p_Start, p_End));
    Capsule(transform, glm::length(delta), p_Options.Thickness, p_Options.Resolution);
}

void RenderContext<D3>::Cube() noexcept
{
    drawPrimitive<D3>(m_State->Transform, Primitives<D3>::GetCubeIndex());
}
void RenderContext<D3>::Cube(const fmat4 &p_Transform) noexcept
{
    drawPrimitive<D3>(p_Transform * m_State->Transform, Primitives<D3>::GetCubeIndex());
}
void RenderContext<D3>::Cube(const fvec3 &p_Dimensions) noexcept
{
    drawPrimitive<D3>(m_State->Transform, Primitives<D3>::GetCubeIndex(), p_Dimensions);
}
void RenderContext<D3>::Cube(const fmat4 &p_Transform, const fvec3 &p_Dimensions) noexcept
{
    drawPrimitive<D3>(p_Transform * m_State->Transform, Primitives<D3>::GetCubeIndex(), p_Dimensions);
}
void RenderContext<D3>::Cube(const f32 p_Size) noexcept
{
    Cube(fvec3{p_Size});
}
void RenderContext<D3>::Cube(const fmat4 &p_Transform, const f32 p_Size) noexcept
{
    Cube(p_Transform, fvec3{p_Size});
}

void RenderContext<D3>::Cylinder(const Resolution p_Res) noexcept
{
    drawPrimitive<D3>(m_State->Transform, Primitives<D3>::GetCylinderIndex(p_Res));
}
void RenderContext<D3>::Cylinder(const fmat4 &p_Transform, const Resolution p_Res) noexcept
{
    drawPrimitive<D3>(p_Transform * m_State->Transform, Primitives<D3>::GetCylinderIndex(p_Res));
}
void RenderContext<D3>::Cylinder(const fvec3 &p_Dimensions, const Resolution p_Res) noexcept
{
    drawPrimitive<D3>(m_State->Transform, Primitives<D3>::GetCylinderIndex(p_Res), p_Dimensions);
}
void RenderContext<D3>::Cylinder(const fmat4 &p_Transform, const fvec3 &p_Dimensions, const Resolution p_Res) noexcept
{
    drawPrimitive<D3>(p_Transform * m_State->Transform, Primitives<D3>::GetCylinderIndex(p_Res), p_Dimensions);
}
void RenderContext<D3>::Cylinder(const f32 p_Length, const f32 p_Diameter, const Resolution p_Res) noexcept
{
    Cylinder(fvec3{p_Length, p_Diameter, p_Diameter}, p_Res);
}
void RenderContext<D3>::Cylinder(const fmat4 &p_Transform, const f32 p_Length, const f32 p_Diameter,
                                 const Resolution p_Res) noexcept
{
    Cylinder(p_Transform, fvec3{p_Length, p_Diameter, p_Diameter}, p_Res);
}

void RenderContext<D3>::Sphere(const Resolution p_Res) noexcept
{
    drawPrimitive<D3>(m_State->Transform, Primitives<D3>::GetSphereIndex(p_Res));
}
void RenderContext<D3>::Sphere(const fmat4 &p_Transform, const Resolution p_Res) noexcept
{
    drawPrimitive<D3>(p_Transform * m_State->Transform, Primitives<D3>::GetSphereIndex(p_Res));
}
void RenderContext<D3>::Sphere(const fvec3 &p_Dimensions, const Resolution p_Res) noexcept
{
    drawPrimitive<D3>(m_State->Transform, Primitives<D3>::GetSphereIndex(p_Res), p_Dimensions);
}
void RenderContext<D3>::Sphere(const fmat4 &p_Transform, const fvec3 &p_Dimensions, const Resolution p_Res) noexcept
{
    drawPrimitive<D3>(p_Transform * m_State->Transform, Primitives<D3>::GetSphereIndex(p_Res), p_Dimensions);
}
void RenderContext<D3>::Sphere(const f32 p_Diameter, const Resolution p_Res) noexcept
{
    Sphere(fvec3{p_Diameter}, p_Res);
}
void RenderContext<D3>::Sphere(const fmat4 &p_Transform, const f32 p_Diameter, const Resolution p_Res) noexcept
{
    Sphere(p_Transform, fvec3{p_Diameter}, p_Res);
}

void RenderContext<D3>::drawChildSphere(fmat4 p_Transform, const fvec3 &p_Position, const Resolution p_Res,
                                        const DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D3>::TranslateIntrinsic(p_Transform, p_Position);
    m_Renderer.DrawPrimitive(m_State, computeFinalTransform(p_Transform), Primitives<D3>::GetSphereIndex(p_Res),
                             p_Flags);
}
void RenderContext<D3>::drawChildSphere(fmat4 p_Transform, const fvec3 &p_Position, const f32 p_Diameter,
                                        const Resolution p_Res, const DrawFlags p_Flags) noexcept
{
    Onyx::Transform<D3>::TranslateIntrinsic(p_Transform, p_Position);
    Onyx::Transform<D3>::ScaleIntrinsic(p_Transform, fvec3{p_Diameter});
    m_Renderer.DrawPrimitive(m_State, computeFinalTransform(p_Transform), Primitives<D3>::GetSphereIndex(p_Res),
                             p_Flags);
}

void RenderContext<D3>::drawCapsule(const fmat4 &p_Transform, const Resolution p_Res) noexcept
{
    const auto fill = [this, &p_Transform, p_Res](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(p_Transform), Primitives<D3>::GetCylinderIndex(p_Res),
                                 p_Flags);
        fvec3 pos{0.f};

        pos.x = -0.5f;
        drawChildSphere(p_Transform, pos, p_Res, p_Flags);
        pos.x = 0.5f;
        drawChildSphere(p_Transform, pos, p_Res, p_Flags);
    };
    const auto outline = [this, &p_Transform, p_Res](const DrawFlags p_Flags) {
        const f32 thickness = 1.f + m_State->OutlineWidth;
        fmat4 transform = p_Transform;

        Onyx::Transform<D3>::ScaleIntrinsic(transform, 1, thickness);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, 2, thickness);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D3>::GetCylinderIndex(p_Res),
                                 p_Flags);
        fvec3 pos{0.f};

        pos.x = -0.5f;
        drawChildSphere(p_Transform, pos, thickness, p_Res, p_Flags);
        pos.x = 0.5f;
        drawChildSphere(p_Transform, pos, thickness, p_Res, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}
void RenderContext<D3>::drawCapsule(const fmat4 &p_Transform, const f32 p_Length, const f32 p_Diameter,
                                    const Resolution p_Res) noexcept
{
    const auto fill = [this, &p_Transform, p_Length, p_Diameter, p_Res](const DrawFlags p_Flags) {
        fmat4 transform = p_Transform;
        Onyx::Transform<D3>::ScaleIntrinsic(transform, fvec3{p_Length, p_Diameter, p_Diameter});

        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D3>::GetCylinderIndex(p_Res),
                                 p_Flags);
        fvec3 pos{0.f};

        pos.x = -0.5f * p_Length;
        drawChildSphere(p_Transform, pos, p_Diameter, p_Res, p_Flags);
        pos.x = -pos.x;
        drawChildSphere(p_Transform, pos, p_Diameter, p_Res, p_Flags);
    };
    const auto outline = [this, &p_Transform, p_Length, p_Diameter, p_Res](const DrawFlags p_Flags) {
        const f32 diameter = p_Diameter + m_State->OutlineWidth;
        fmat4 transform = p_Transform;
        Onyx::Transform<D3>::ScaleIntrinsic(transform, fvec3{p_Length, diameter, diameter});
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D3>::GetCylinderIndex(p_Res),
                                 p_Flags);
        fvec3 pos{0.f};

        pos.x = -0.5f * p_Length;
        drawChildSphere(p_Transform, pos, diameter, p_Res, p_Flags);
        pos.x = -pos.x;
        drawChildSphere(p_Transform, pos, diameter, p_Res, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}

void RenderContext<D3>::Capsule(const Resolution p_Res) noexcept
{
    drawCapsule(m_State->Transform, p_Res);
}
void RenderContext<D3>::Capsule(const fmat4 &p_Transform, const Resolution p_Res) noexcept
{
    drawCapsule(p_Transform * m_State->Transform, p_Res);
}
void RenderContext<D3>::Capsule(const f32 p_Length, const f32 p_Diameter, const Resolution p_Res) noexcept
{
    drawCapsule(m_State->Transform, p_Length, p_Diameter, p_Res);
}
void RenderContext<D3>::Capsule(const fmat4 &p_Transform, const f32 p_Length, const f32 p_Diameter,
                                const Resolution p_Res) noexcept
{
    drawCapsule(p_Transform * m_State->Transform, p_Length, p_Diameter, p_Res);
}

void RenderContext<D3>::drawRoundedCubeMoons(const fmat4 &p_Transform, const fvec3 &p_Dimensions, const f32 p_Diameter,
                                             const Resolution p_Res, const DrawFlags p_Flags) noexcept
{
    const f32 radius = 0.5f * p_Diameter;
    const fvec3 halfDims = 0.5f * p_Dimensions;
    const fvec3 paddedDims = halfDims + 0.5f * radius;
    for (u32 i = 0; i < 6; ++i)
    {
        fmat4 transform = p_Transform;
        const u32 index1 = i % 3;
        const u32 index2 = (i + 1) % 3;
        const u32 index3 = (i + 2) % 3;
        const f32 dim = i < 3 ? paddedDims[index1] : -paddedDims[index1];
        Onyx::Transform<D3>::TranslateIntrinsic(transform, index1, dim);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index1, radius);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index2, p_Dimensions[index2]);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index3, p_Dimensions[index3]);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D3>::GetCubeIndex(), p_Flags);
    }
    fvec3 pos = halfDims;
    for (u32 i = 0; i < 8; ++i)
    {
        drawChildSphere(p_Transform, pos, p_Diameter, p_Res, p_Flags);
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
                Onyx::Transform<D3>::ScaleIntrinsic(transform, {p_Dimensions[axis], p_Diameter, p_Diameter});
                m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform),
                                         Primitives<D3>::GetCylinderIndex(p_Res), p_Flags);
            }
    }
}

void RenderContext<D3>::drawRoundedCube(const fmat4 &p_Transform, const Resolution p_Res) noexcept
{
    const auto fill = [this, &p_Transform, p_Res](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(p_Transform), Primitives<D3>::GetSquareIndex(),
                                 p_Flags);
        drawRoundedCubeMoons(p_Transform, fvec3{1.f}, 1.f, p_Res, p_Flags);
    };
    const auto outline = [this, &p_Transform, p_Res](const DrawFlags p_Flags) {
        const f32 thickness = 1.f + m_State->OutlineWidth;
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(p_Transform), Primitives<D3>::GetSquareIndex(),
                                 p_Flags);
        drawRoundedCubeMoons(p_Transform, fvec3{1.f}, thickness, p_Res, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}
void RenderContext<D3>::drawRoundedCube(const fmat4 &p_Transform, const fvec3 &p_Dimensions, const f32 p_Diameter,
                                        const Resolution p_Res) noexcept
{
    const auto fill = [this, &p_Transform, &p_Dimensions, p_Diameter, p_Res](const DrawFlags p_Flags) {
        fmat4 transform = p_Transform;
        Onyx::Transform<D3>::ScaleIntrinsic(transform, p_Dimensions);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D3>::GetSquareIndex(), p_Flags);

        drawRoundedCubeMoons(p_Transform, p_Dimensions, p_Diameter, p_Res, p_Flags);
    };
    const auto outline = [this, &p_Transform, &p_Dimensions, p_Diameter, p_Res](const DrawFlags p_Flags) {
        const f32 width = m_State->OutlineWidth;
        fmat4 transform = p_Transform;
        Onyx::Transform<D3>::ScaleIntrinsic(transform, p_Dimensions);
        m_Renderer.DrawPrimitive(m_State, computeFinalTransform(transform), Primitives<D3>::GetSquareIndex(), p_Flags);

        drawRoundedCubeMoons(p_Transform, p_Dimensions, p_Diameter + width, p_Res, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}

void RenderContext<D3>::RoundedCube(const Resolution p_Res) noexcept
{
    drawRoundedCube(m_State->Transform, p_Res);
}
void RenderContext<D3>::RoundedCube(const fmat4 &p_Transform, const Resolution p_Res) noexcept
{
    drawRoundedCube(p_Transform * m_State->Transform, p_Res);
}
void RenderContext<D3>::RoundedCube(const fvec3 &p_Dimensions, const f32 p_Diameter, const Resolution p_Res) noexcept
{
    drawRoundedCube(m_State->Transform, p_Dimensions, p_Diameter, p_Res);
}
void RenderContext<D3>::RoundedCube(const fmat4 &p_Transform, const fvec3 &p_Dimensions, const f32 p_Diameter,
                                    const Resolution p_Res) noexcept
{
    drawRoundedCube(p_Transform * m_State->Transform, p_Dimensions, p_Diameter, p_Res);
}
void RenderContext<D3>::RoundedCube(const f32 p_Size, const f32 p_Diameter, const Resolution p_Res) noexcept
{
    RoundedCube(fvec3{p_Size}, p_Diameter, p_Res);
}
void RenderContext<D3>::RoundedCube(const fmat4 &p_Transform, const f32 p_Size, const f32 p_Diameter,
                                    const Resolution p_Res) noexcept
{
    RoundedCube(p_Transform, fvec3{p_Size}, p_Diameter, p_Res);
}

void RenderContext<D3>::LightColor(const Color &p_Color) noexcept
{
    m_State->LightColor = p_Color;
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
    const fmat4 transform = m_State->Axes * m_State->Transform;
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
    light.Color = m_State->LightColor;
    DirectionalLight(light);
}

void RenderContext<D3>::PointLight(Onyx::PointLight p_Light) noexcept
{
    const fmat4 transform = m_State->Axes * m_State->Transform;
    fvec4 position = p_Light.PositionAndIntensity;
    position.w = 1.f;
    position = transform * position;
    position.w = p_Light.PositionAndIntensity.w;
    p_Light.PositionAndIntensity = position;
    m_Renderer.AddPointLight(p_Light);
}
void RenderContext<D3>::PointLight(const fvec3 &p_Position, const f32 p_Diameter, const f32 p_Intensity) noexcept
{
    Onyx::PointLight light;
    light.PositionAndIntensity = fvec4{p_Position, p_Intensity};
    light.Radius = p_Diameter;
    light.Color = m_State->LightColor;
    PointLight(light);
}
void RenderContext<D3>::PointLight(const f32 p_Diameter, const f32 p_Intensity) noexcept
{
    PointLight(fvec3{0.f}, p_Diameter, p_Intensity);
}

void RenderContext<D3>::DiffuseContribution(const f32 p_Contribution) noexcept
{
    m_State->Material.DiffuseContribution = p_Contribution;
}
void RenderContext<D3>::SpecularContribution(const f32 p_Contribution) noexcept
{
    m_State->Material.SpecularContribution = p_Contribution;
}
void RenderContext<D3>::SpecularSharpness(const f32 p_Sharpness) noexcept
{
    m_State->Material.SpecularSharpness = p_Sharpness;
}

fvec3 RenderContext<D3>::GetViewLookDirectionInCurrentAxes() const noexcept
{
    return glm::normalize(GetCoordinates(fvec3{0.f, 0.f, 1.f}));
}
fvec3 RenderContext<D3>::GetMouseRayCastDirection() const noexcept
{
    return glm::normalize(GetMouseCoordinates(0.25f) - GetMouseCoordinates(0.f));
}

template <Dimension D> void IRenderContext<D>::Fill(const bool p_Enabled) noexcept
{
    m_State->Fill = p_Enabled;
}

template <Dimension D> void IRenderContext<D>::drawMesh(const fmat<D> &p_Transform, const Model<D> &p_Model) noexcept
{
    const auto fill = [this, &p_Transform, &p_Model](const DrawFlags p_Flags) {
        m_Renderer.DrawMesh(m_State, p_Transform, p_Model, p_Flags);
    };
    const auto outline = [this, &p_Transform, &p_Model](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 scale = 1.f + m_State->OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, fvec<D>{scale});
        m_Renderer.DrawMesh(m_State, transform, p_Model, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}
template <Dimension D>
void IRenderContext<D>::drawMesh(const fmat<D> &p_Transform, const Model<D> &p_Model,
                                 const fvec<D> &p_Dimensions) noexcept
{
    const auto fill = [this, &p_Transform, &p_Model, &p_Dimensions](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        Onyx::Transform<D>::ScaleIntrinsic(transform, p_Dimensions);
        m_Renderer.DrawMesh(m_State, transform, p_Model, p_Flags);
    };
    const auto outline = [this, &p_Transform, &p_Model, &p_Dimensions](const DrawFlags p_Flags) {
        fmat<D> transform = p_Transform;
        const f32 width = m_State->OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, p_Dimensions + width);
        m_Renderer.DrawMesh(m_State, transform, p_Model, p_Flags);
    };
    resolveDrawFlagsWithState(fill, outline);
}

template <Dimension D> void IRenderContext<D>::Mesh(const Model<D> &p_Model) noexcept
{
    drawMesh(m_State->Transform, p_Model);
}
template <Dimension D> void IRenderContext<D>::Mesh(const fmat<D> &p_Transform, const Model<D> &p_Model) noexcept
{
    drawMesh(p_Transform * m_State->Transform, p_Model);
}
template <Dimension D> void IRenderContext<D>::Mesh(const Model<D> &p_Model, const fvec<D> &p_Dimensions) noexcept
{
    drawMesh(m_State->Transform, p_Model, p_Dimensions);
}
template <Dimension D>
void IRenderContext<D>::Mesh(const fmat<D> &p_Transform, const Model<D> &p_Model, const fvec<D> &p_Dimensions) noexcept
{
    drawMesh(p_Transform * m_State->Transform, p_Model, p_Dimensions);
}

template <Dimension D> void IRenderContext<D>::Push() noexcept
{
    m_StateStack.push_back(m_StateStack.back());
    m_State = &m_StateStack.back();
}
template <Dimension D> void IRenderContext<D>::PushAndClear() noexcept
{
    m_StateStack.push_back(RenderState<D>{});
    m_State = &m_StateStack.back();
}
template <Dimension D> void IRenderContext<D>::Pop() noexcept
{
    TKIT_ASSERT(m_StateStack.size() > 1, "[ONYX] For every push, there must be a pop");
    m_StateStack.pop_back();
    m_State = &m_StateStack.back();
}

template <Dimension D> void IRenderContext<D>::Alpha(const f32 p_Alpha) noexcept
{
    m_State->Material.Color.RGBA.a = p_Alpha;
}
template <Dimension D> void IRenderContext<D>::Alpha(const u8 p_Alpha) noexcept
{
    m_State->Material.Color.RGBA.a = static_cast<f32>(p_Alpha) / 255.f;
}
template <Dimension D> void IRenderContext<D>::Alpha(const u32 p_Alpha) noexcept
{
    m_State->Material.Color.RGBA.a = static_cast<f32>(p_Alpha) / 255.f;
}

template <Dimension D> void IRenderContext<D>::Fill(const Color &p_Color) noexcept
{
    Fill(true);
    m_State->Material.Color = p_Color;
}
template <Dimension D> void IRenderContext<D>::Outline(const bool p_Enabled) noexcept
{
    m_State->Outline = p_Enabled;
}
template <Dimension D> void IRenderContext<D>::Outline(const Color &p_Color) noexcept
{
    Outline(true);
    m_State->OutlineColor = p_Color;
}
template <Dimension D> void IRenderContext<D>::OutlineWidth(const f32 p_Width) noexcept
{
    Outline(true);
    m_State->OutlineWidth = p_Width;
}

template <Dimension D> void IRenderContext<D>::Material(const MaterialData<D> &p_Material) noexcept
{
    m_State->Material = p_Material;
}

template <Dimension D> const RenderState<D> &IRenderContext<D>::GetCurrentState() const noexcept
{
    return m_StateStack.back();
}
template <Dimension D> RenderState<D> &IRenderContext<D>::GetCurrentState() noexcept
{
    return m_StateStack.back();
}

template <Dimension D> const ProjectionViewData<D> &IRenderContext<D>::GetProjectionViewData() const noexcept
{
    return m_ProjectionView;
}

template <Dimension D> Onyx::Transform<D> IRenderContext<D>::GetViewTransformInCurrentAxes() const noexcept
{
    const fmat<D> vmat = glm::inverse(m_State->Axes) * m_ProjectionView.View.ComputeTransform();
    return Onyx::Transform<D>::Extract(vmat);
}

template <Dimension D> void IRenderContext<D>::SetView(const Onyx::Transform<D> &p_View) noexcept
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

template <Dimension D> void IRenderContext<D>::Render(const VkCommandBuffer p_Commandbuffer) noexcept
{
    m_Renderer.Render(p_Commandbuffer, m_ProjectionView);
}

void RenderContext<D2>::Axes(const AxesOptions<D2> &p_Options) noexcept
{
    // TODO: Parametrize this
    Color &color = m_State->Material.Color;
    const Color oldColor = color; // A cheap filthy push

    const fvec2 xLeft = fvec2{-p_Options.Size, 0.f};
    const fvec2 xRight = fvec2{p_Options.Size, 0.f};

    const fvec2 yDown = fvec2{0.f, -p_Options.Size};
    const fvec2 yUp = fvec2{0.f, p_Options.Size};

    color = Color{245u, 64u, 90u};
    Line(xLeft, xRight, p_Options.Thickness);
    color = Color{65u, 135u, 245u};
    Line(yDown, yUp, p_Options.Thickness);

    color = oldColor; // A cheap filthy pop
}

void RenderContext<D3>::Axes(const AxesOptions<D3> &p_Options) noexcept
{
    // TODO: Parametrize this
    Color &color = m_State->Material.Color;
    const Color oldColor = color; // A cheap filthy push

    const fvec3 xLeft = fvec3{-p_Options.Size, 0.f, 0.f};
    const fvec3 xRight = fvec3{p_Options.Size, 0.f, 0.f};

    const fvec3 yDown = fvec3{0.f, -p_Options.Size, 0.f};
    const fvec3 yUp = fvec3{0.f, p_Options.Size, 0.f};

    const fvec3 zBack = fvec3{0.f, 0.f, -p_Options.Size};
    const fvec3 zFront = fvec3{0.f, 0.f, p_Options.Size};

    color = Color{245u, 64u, 90u};
    Line(xLeft, xRight, {.Thickness = p_Options.Thickness, .Resolution = p_Options.Resolution});
    color = Color{65u, 135u, 245u};
    Line(yDown, yUp, {.Thickness = p_Options.Thickness, .Resolution = p_Options.Resolution});
    color = Color{180u, 245u, 65u};
    Line(zBack, zFront, {.Thickness = p_Options.Thickness, .Resolution = p_Options.Resolution});
    color = oldColor; // A cheap filthy pop
}

template <Dimension D>
void IRenderContext<D>::ApplyCameraMovementControls(const CameraMovementControls<D> &p_Controls) noexcept
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
template <Dimension D> void IRenderContext<D>::ApplyCameraMovementControls(const TKit::Timespan p_DeltaTime) noexcept
{
    CameraMovementControls<D> controls{};
    controls.TranslationStep = p_DeltaTime.AsSeconds();
    controls.RotationStep = p_DeltaTime.AsSeconds();
    ApplyCameraMovementControls(controls);
}

void RenderContext<D2>::ApplyCameraScalingControls(const f32 p_ScaleStep) noexcept
{
    fmat4 transform = PromoteTransform(m_ProjectionView.View.ComputeTransform());
    ApplyCoordinateSystemIntrinsic(transform);
    const fvec2 mpos = transform * fvec4{Input::GetMousePosition(m_Window), 0.f, 1.f};

    const fvec2 dpos = p_ScaleStep * (mpos - m_ProjectionView.View.Translation);
    m_ProjectionView.View.Translation += dpos;
    m_ProjectionView.View.Scale *= 1.f - p_ScaleStep;

    m_ProjectionView.ProjectionView = m_ProjectionView.View.ComputeInverseTransform();
}

template <Dimension D> fvec<D> IRenderContext<D>::GetCoordinates(const fvec<D> &p_NormalizedPos) const noexcept
{
    if constexpr (D == D2)
    {
        const fmat3 itransform3 = glm::inverse(m_ProjectionView.ProjectionView * m_State->Axes);
        fmat4 itransform = PromoteTransform(itransform3);
        ApplyCoordinateSystemIntrinsic(itransform);
        return itransform * fvec4{p_NormalizedPos, 0.f, 1.f};
    }
    else
    {
        const fmat4 transform = m_ProjectionView.ProjectionView * m_State->Axes;
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

template class ONYX_API Detail::IRenderContext<D2>;
template class ONYX_API Detail::IRenderContext<D3>;

} // namespace Onyx