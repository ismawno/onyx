#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "tkit/multiprocessing/task_manager.hpp"

#include "tkit/math/math.hpp"

namespace Onyx
{
using namespace Detail;

template <Dimension D>
IRenderContext<D>::IRenderContext(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) : m_Renderer(p_RenderInfo)
{
    for (u32 i = 0; i < ONYX_MAX_THREADS; ++i)
    {
        RenderState<D> &state = m_StateStack[i].States.Append();
        m_StateStack[i].Current = &state;
    }
}

static u32 getThreadIndex()
{
    thread_local u32 tindex = Core::GetTaskManager()->GetThreadIndex();
    return tindex;
}

template <Dimension D> void IRenderContext<D>::Flush(const u32 p_ThreadCount)
{
    TKIT_ASSERT(p_ThreadCount <= ONYX_MAX_THREADS, "[ONYX] Thread count is greater than the maximum threads allowed");
    m_Renderer.Flush();
    for (u32 i = 0; i < p_ThreadCount; ++i)
    {
        Stack &stack = m_StateStack[i];
        TKIT_ASSERT(stack.States.GetSize() == 1,
                    "[ONYX] Mismatched Push() call found in thread {}. For every Push(), there must be a Pop()", i);
        stack.States[0] = RenderState<D>{};
    }
}

template <Dimension D> void IRenderContext<D>::Transform(const f32m<D> &p_Transform)
{
    RenderState<D> *state = getState();
    state->Transform = p_Transform * state->Transform;
}
template <Dimension D>
void IRenderContext<D>::Transform(const f32v<D> &p_Translation, const f32v<D> &p_Scale, const rot<D> &p_Rotation)
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
template <Dimension D>
void IRenderContext<D>::Transform(const f32v<D> &p_Translation, const f32 p_Scale, const rot<D> &p_Rotation)
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(p_Translation, f32v<D>{p_Scale}, p_Rotation));
}
void RenderContext<D3>::Transform(const f32v3 &p_Translation, const f32v3 &p_Scale, const f32v3 &p_Rotation)
{
    this->Transform(Onyx::Transform<D3>::ComputeTransform(p_Translation, p_Scale, f32q{p_Rotation}));
}
void RenderContext<D3>::Transform(const f32v3 &p_Translation, const f32 p_Scale, const f32v3 &p_Rotation)
{
    this->Transform(Onyx::Transform<D3>::ComputeTransform(p_Translation, f32v3{p_Scale}, f32q{p_Rotation}));
}

template <Dimension D> void IRenderContext<D>::TransformAxes(const f32m<D> &p_Axes)
{
    getState()->Axes *= p_Axes;
}
template <Dimension D>
void IRenderContext<D>::TransformAxes(const f32v<D> &p_Translation, const f32v<D> &p_Scale, const rot<D> &p_Rotation)
{
    getState()->Axes *= Onyx::Transform<D>::ComputeReversedTransform(p_Translation, p_Scale, p_Rotation);
}
template <Dimension D>
void IRenderContext<D>::TransformAxes(const f32v<D> &p_Translation, const f32 p_Scale, const rot<D> &p_Rotation)
{
    TransformAxes(p_Translation, f32v<D>{p_Scale}, p_Rotation);
}
void RenderContext<D3>::TransformAxes(const f32v3 &p_Translation, const f32v3 &p_Scale, const f32v3 &p_Rotation)
{
    TransformAxes(Onyx::Transform<D3>::ComputeReversedTransform(p_Translation, p_Scale, f32q{p_Rotation}));
}
void RenderContext<D3>::TransformAxes(const f32v3 &p_Translation, const f32 p_Scale, const f32v3 &p_Rotation)
{
    TransformAxes(Onyx::Transform<D3>::ComputeReversedTransform(p_Translation, f32v3{p_Scale}, f32q{p_Rotation}));
}

template <Dimension D> void IRenderContext<D>::Translate(const f32v<D> &p_Translation)
{
    Onyx::Transform<D>::TranslateExtrinsic(getState()->Transform, p_Translation);
}
template <Dimension D> void IRenderContext<D>::SetTranslation(const f32v<D> &p_Translation)
{
    RenderState<D> *state = getState();
    state->Transform[D][0] = p_Translation[0];
    state->Transform[D][1] = p_Translation[1];
    if constexpr (D == D3)
        state->Transform[D][2] = p_Translation[2];
}

template <Dimension D> void IRenderContext<D>::Scale(const f32v<D> &p_Scale)
{
    Onyx::Transform<D>::ScaleExtrinsic(getState()->Transform, p_Scale);
}
template <Dimension D> void IRenderContext<D>::Scale(const f32 p_Scale)
{
    Scale(f32v<D>{p_Scale});
}

template <Dimension D> void IRenderContext<D>::TranslateX(const f32 p_X)
{
    Onyx::Transform<D>::TranslateExtrinsic(getState()->Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::TranslateY(const f32 p_Y)
{
    Onyx::Transform<D>::TranslateExtrinsic(getState()->Transform, 1, p_Y);
}
void RenderContext<D3>::TranslateZ(const f32 p_Z)
{
    Onyx::Transform<D3>::TranslateExtrinsic(getState()->Transform, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::SetTranslationX(const f32 p_X)
{
    getState()->Transform[D][0] = p_X;
}
template <Dimension D> void IRenderContext<D>::SetTranslationY(const f32 p_Y)
{
    getState()->Transform[D][1] = p_Y;
}
void RenderContext<D3>::SetTranslationZ(const f32 p_Z)
{
    getState()->Transform[D3][2] = p_Z;
}
template <Dimension D> void IRenderContext<D>::ScaleX(const f32 p_X)
{
    Onyx::Transform<D>::ScaleExtrinsic(getState()->Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::ScaleY(const f32 p_Y)
{
    Onyx::Transform<D>::ScaleExtrinsic(getState()->Transform, 1, p_Y);
}
void RenderContext<D3>::ScaleZ(const f32 p_Z)
{
    Onyx::Transform<D3>::ScaleExtrinsic(getState()->Transform, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::TranslateXAxis(const f32 p_X)
{
    Onyx::Transform<D>::TranslateIntrinsic(getState()->Axes, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::TranslateYAxis(const f32 p_Y)
{
    Onyx::Transform<D>::TranslateIntrinsic(getState()->Axes, 1, p_Y);
}
void RenderContext<D3>::TranslateZAxis(const f32 p_Z)
{
    Onyx::Transform<D3>::TranslateIntrinsic(getState()->Axes, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::ScaleXAxis(const f32 p_X)
{
    Onyx::Transform<D>::ScaleIntrinsic(getState()->Axes, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::ScaleYAxis(const f32 p_Y)
{
    Onyx::Transform<D>::ScaleIntrinsic(getState()->Axes, 1, p_Y);
}
void RenderContext<D3>::ScaleZAxis(const f32 p_Z)
{
    Onyx::Transform<D3>::ScaleIntrinsic(getState()->Axes, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::TranslateAxes(const f32v<D> &p_Translation)
{
    Onyx::Transform<D>::TranslateIntrinsic(getState()->Axes, p_Translation);
}

template <Dimension D> void IRenderContext<D>::ScaleAxes(const f32v<D> &p_Scale)
{
    Onyx::Transform<D>::ScaleIntrinsic(getState()->Axes, p_Scale);
}
template <Dimension D> void IRenderContext<D>::ScaleAxes(const f32 p_Scale)
{
    ScaleAxes(f32v<D>{p_Scale});
}

void RenderContext<D2>::Rotate(const f32 p_Angle)
{
    Onyx::Transform<D2>::RotateExtrinsic(getState()->Transform, p_Angle);
}

void RenderContext<D3>::Rotate(const f32q &p_Quaternion)
{
    Onyx::Transform<D3>::RotateExtrinsic(getState()->Transform, p_Quaternion);
}
void RenderContext<D3>::Rotate(const f32 p_Angle, const f32v3 &p_Axis)
{
    Rotate(f32q::FromAngleAxis(p_Angle, p_Axis));
}
void RenderContext<D3>::Rotate(const f32v3 &p_Angles)
{
    Rotate(f32q(p_Angles));
}

// This could be optimized a bit
void RenderContext<D3>::RotateX(const f32 p_Angle)
{
    Rotate(f32v3{p_Angle, 0.f, 0.f});
}
void RenderContext<D3>::RotateY(const f32 p_Angle)
{
    Rotate(f32v3{0.f, p_Angle, 0.f});
}
void RenderContext<D3>::RotateZ(const f32 p_Angle)
{
    Rotate(f32v3{0.f, 0.f, p_Angle});
}

void RenderContext<D2>::RotateAxes(const f32 p_Angle)
{
    Onyx::Transform<D2>::RotateIntrinsic(getState()->Axes, p_Angle);
}
void RenderContext<D3>::RotateAxes(const f32q &p_Quaternion)
{
    Onyx::Transform<D3>::RotateIntrinsic(getState()->Axes, p_Quaternion);
}
void RenderContext<D3>::RotateAxes(const f32 p_Angle, const f32v3 &p_Axis)
{
    RotateAxes(f32q::FromAngleAxis(p_Angle, p_Axis));
}
void RenderContext<D3>::RotateAxes(const f32v3 &p_Angles)
{
    RotateAxes(f32q(p_Angles));
}

// This could be optimized a bit
void RenderContext<D3>::RotateXAxis(const f32 p_Angle)
{
    RotateAxes(f32v3{p_Angle, 0.f, 0.f});
}
void RenderContext<D3>::RotateYAxis(const f32 p_Angle)
{
    RotateAxes(f32v3{0.f, p_Angle, 0.f});
}
void RenderContext<D3>::RotateZAxis(const f32 p_Angle)
{
    RotateAxes(f32v3{0.f, 0.f, p_Angle});
}

template <Dimension D, typename F1, typename F2>
static void resolveDrawFlagsWithState(const RenderState<D> &p_State, F1 &&p_FillDraw, F2 &&p_OutlineDraw)
{
    if (p_State.Flags & RenderStateFlag_Fill)
    {
        if (p_State.Flags & RenderStateFlag_Outline)
        {
            std::forward<F1>(p_FillDraw)(DrawFlag_DoStencilWriteDoFill);
            std::forward<F2>(p_OutlineDraw)(DrawFlag_DoStencilTestNoFill);
        }
        else
            std::forward<F1>(p_FillDraw)(DrawFlag_NoStencilWriteDoFill);
    }
    else if (p_State.Flags & RenderStateFlag_Outline)
    {
        std::forward<F1>(p_FillDraw)(DrawFlag_DoStencilWriteNoFill);
        std::forward<F2>(p_OutlineDraw)(DrawFlag_DoStencilTestNoFill);
    }
}

template <Dimension D> void IRenderContext<D>::updateState()
{
    Stack &stack = m_StateStack[getThreadIndex()];
    stack.Current = &stack.States.GetBack();
}
template <Dimension D> RenderState<D> *IRenderContext<D>::getState()
{
    return m_StateStack[getThreadIndex()].Current;
}

template <Dimension D>
template <Dimension PDim>
void IRenderContext<D>::drawPrimitive(const RenderState<D> &p_State, const f32m<D> &p_Transform,
                                      const u32 p_PrimitiveIndex)
{
    const auto fill = [this, &p_State, &p_Transform, p_PrimitiveIndex](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * p_Transform, p_PrimitiveIndex, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, p_PrimitiveIndex](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 thickness = 1.f + p_State.OutlineWidth;
        for (u32 i = 0; i < PDim; ++i)
            Onyx::Transform<D>::ScaleIntrinsic(transform, i, thickness);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, p_PrimitiveIndex, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}
template <Dimension D>
template <Dimension PDim>
void IRenderContext<D>::drawPrimitive(const RenderState<D> &p_State, const f32m<D> &p_Transform,
                                      const u32 p_PrimitiveIndex, const f32v<PDim> &p_Dimensions)
{
    const auto fill = [this, &p_State, &p_Transform, &p_Dimensions, p_PrimitiveIndex](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        for (u32 i = 0; i < PDim; ++i)
            Onyx::Transform<D>::ScaleIntrinsic(transform, i, p_Dimensions[i]);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, p_PrimitiveIndex, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, &p_Dimensions, p_PrimitiveIndex](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 width = p_State.OutlineWidth;
        for (u32 i = 0; i < PDim; ++i)
            Onyx::Transform<D>::ScaleIntrinsic(transform, i, p_Dimensions[i] + width);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, p_PrimitiveIndex, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}

template <Dimension D> void IRenderContext<D>::Triangle()
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, state.Transform, Primitives<D>::GetTriangleIndex());
}
template <Dimension D> void IRenderContext<D>::Triangle(const f32m<D> &p_Transform)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, p_Transform * state.Transform, Primitives<D>::GetTriangleIndex());
}
template <Dimension D> void IRenderContext<D>::Triangle(const f32v2 &p_Dimensions)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, state.Transform, Primitives<D>::GetTriangleIndex(), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Triangle(const f32m<D> &p_Transform, const f32v2 &p_Dimensions)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, p_Transform * state.Transform, Primitives<D>::GetTriangleIndex(), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Triangle(const f32 p_Size)
{
    Triangle(f32v2{p_Size});
}
template <Dimension D> void IRenderContext<D>::Triangle(const f32m<D> &p_Transform, const f32 p_Size)
{
    Triangle(p_Transform, f32v2{p_Size});
}

template <Dimension D> void IRenderContext<D>::Square()
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, state.Transform, Primitives<D>::GetSquareIndex());
}
template <Dimension D> void IRenderContext<D>::Square(const f32m<D> &p_Transform)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, p_Transform * state.Transform, Primitives<D>::GetSquareIndex());
}
template <Dimension D> void IRenderContext<D>::Square(const f32v2 &p_Dimensions)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, state.Transform, Primitives<D>::GetSquareIndex(), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Square(const f32m<D> &p_Transform, const f32v2 &p_Dimensions)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, p_Transform * state.Transform, Primitives<D>::GetSquareIndex(), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Square(const f32 p_Size)
{
    Square(f32v2{p_Size});
}
template <Dimension D> void IRenderContext<D>::Square(const f32m<D> &p_Transform, const f32 p_Size)
{
    Square(p_Transform, f32v2{p_Size});
}

template <Dimension D> void IRenderContext<D>::NGon(const u32 p_Sides)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, state.Transform, Primitives<D>::GetNGonIndex(p_Sides));
}
template <Dimension D> void IRenderContext<D>::NGon(const f32m<D> &p_Transform, const u32 p_Sides)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, p_Transform * state.Transform, Primitives<D>::GetNGonIndex(p_Sides));
}
template <Dimension D> void IRenderContext<D>::NGon(const u32 p_Sides, const f32v2 &p_Dimensions)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, state.Transform, Primitives<D>::GetNGonIndex(p_Sides), p_Dimensions);
}
template <Dimension D>
void IRenderContext<D>::NGon(const f32m<D> &p_Transform, const u32 p_Sides, const f32v2 &p_Dimensions)
{
    const RenderState<D> &state = *getState();
    drawPrimitive<D2>(state, p_Transform * state.Transform, Primitives<D>::GetNGonIndex(p_Sides), p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::NGon(const u32 p_Sides, const f32 p_Size)
{
    NGon(p_Sides, f32v2{p_Size});
}
template <Dimension D> void IRenderContext<D>::NGon(const f32m<D> &p_Transform, const u32 p_Sides, const f32 p_Size)
{
    NGon(p_Transform, p_Sides, f32v2{p_Size});
}

template <Dimension D>
void IRenderContext<D>::drawPolygon(const RenderState<D> &p_State, const f32m<D> &p_Transform,
                                    const TKit::Span<const f32v2> p_Vertices)
{
    const auto fill = [this, &p_State, &p_Transform, p_Vertices](const DrawFlags p_Flags) {
        m_Renderer.DrawPolygon(p_State, p_State.Axes * p_Transform, p_Vertices, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, p_Vertices](const DrawFlags p_Flags) {
        TKIT_ASSERT(p_Vertices.GetSize() < ONYX_MAX_POLYGON_VERTICES,
                    "[ONYX] The provided vertices ({}) exceed the maximum: {}", p_Vertices.GetSize(),
                    ONYX_MAX_POLYGON_VERTICES);
        PolygonVerticesArray vertices;
        const f32 width = p_State.OutlineWidth;
        for (u32 prev = 0; prev < p_Vertices.GetSize(); ++prev)
        {
            const u32 current = (prev + 1) % p_Vertices.GetSize();
            const u32 next = (current + 1) % p_Vertices.GetSize();

            const f32v2 edge1 = p_Vertices[current] - p_Vertices[prev];
            const f32v2 edge2 = p_Vertices[next] - p_Vertices[current];

            const f32v2 normal1 = Math::Normalize(f32v2{edge1[1], -edge1[0]});
            const f32v2 normal2 = Math::Normalize(f32v2{edge2[1], -edge2[0]});
            vertices.Append(p_Vertices[current] + width * Math::Normalize(normal1 + normal2));
        }
        m_Renderer.DrawPolygon(p_State, p_State.Axes * p_Transform, vertices, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}

template <Dimension D> void IRenderContext<D>::Polygon(const TKit::Span<const f32v2> p_Vertices)
{
    const RenderState<D> &state = *getState();
    drawPolygon(state, state.Transform, p_Vertices);
}
template <Dimension D>
void IRenderContext<D>::Polygon(const f32m<D> &p_Transform, const TKit::Span<const f32v2> p_Vertices)
{
    const RenderState<D> &state = *getState();
    drawPolygon(state, p_Transform * state.Transform, p_Vertices);
}

template <Dimension D>
void IRenderContext<D>::drawCircle(const RenderState<D> &p_State, const f32m<D> &p_Transform,
                                   const CircleOptions &p_Options)
{
    const auto fill = [this, &p_State, &p_Transform, &p_Options](const DrawFlags p_Flags) {
        m_Renderer.DrawCircle(p_State, p_State.Axes * p_Transform, p_Options, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, &p_Options](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 diameter = 1.f + p_State.OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, diameter);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, diameter);

        m_Renderer.DrawCircle(p_State, p_State.Axes * transform, p_Options, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}
template <Dimension D>
void IRenderContext<D>::drawCircle(const RenderState<D> &p_State, const f32m<D> &p_Transform,
                                   const CircleOptions &p_Options, const f32v2 &p_Dimensions)
{
    const auto fill = [this, &p_State, &p_Transform, &p_Options, &p_Dimensions](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Dimensions[0]);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Dimensions[1]);
        m_Renderer.DrawCircle(p_State, p_State.Axes * transform, p_Options, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, &p_Options, &p_Dimensions](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 diameter = p_State.OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, diameter + p_Dimensions[0]);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, diameter + p_Dimensions[1]);

        m_Renderer.DrawCircle(p_State, p_State.Axes * transform, p_Options, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}

template <Dimension D> void IRenderContext<D>::Circle(const CircleOptions &p_Options)
{
    const RenderState<D> &state = *getState();
    drawCircle(state, state.Transform, p_Options);
}
template <Dimension D> void IRenderContext<D>::Circle(const f32m<D> &p_Transform, const CircleOptions &p_Options)
{
    const RenderState<D> &state = *getState();
    drawCircle(state, p_Transform * state.Transform, p_Options);
}
template <Dimension D> void IRenderContext<D>::Circle(const f32v2 &p_Dimensions, const CircleOptions &p_Options)
{
    const RenderState<D> &state = *getState();
    drawCircle(state, state.Transform, p_Options, p_Dimensions);
}
template <Dimension D>
void IRenderContext<D>::Circle(const f32m<D> &p_Transform, const f32v2 &p_Dimensions, const CircleOptions &p_Options)
{
    const RenderState<D> &state = *getState();
    drawCircle(state, p_Transform * state.Transform, p_Options, p_Dimensions);
}
template <Dimension D> void IRenderContext<D>::Circle(const f32 p_Diameter, const CircleOptions &p_Options)
{
    Circle(f32v2{p_Diameter}, p_Options);
}
template <Dimension D>
void IRenderContext<D>::Circle(const f32m<D> &p_Transform, const f32 p_Diameter, const CircleOptions &p_Options)
{
    Circle(p_Transform, f32v2{p_Diameter}, p_Options);
}

template <Dimension D>
void IRenderContext<D>::drawChildCircle(const RenderState<D> &p_State, f32m<D> p_Transform, const f32v<D> &p_Position,
                                        const CircleOptions &p_Options, const DrawFlags p_Flags)
{
    Onyx::Transform<D>::TranslateIntrinsic(p_Transform, p_Position);
    m_Renderer.DrawCircle(p_State, p_State.Axes * p_Transform, p_Options, p_Flags);
}
template <Dimension D>
void IRenderContext<D>::drawChildCircle(const RenderState<D> &p_State, f32m<D> p_Transform, const f32v<D> &p_Position,
                                        const f32 p_Diameter, const CircleOptions &p_Options, const DrawFlags p_Flags)
{
    Onyx::Transform<D>::TranslateIntrinsic(p_Transform, p_Position);
    Onyx::Transform<D>::ScaleIntrinsic(p_Transform, 0, p_Diameter);
    Onyx::Transform<D>::ScaleIntrinsic(p_Transform, 1, p_Diameter);
    m_Renderer.DrawCircle(p_State, p_State.Axes * p_Transform, p_Options, p_Flags);
}

template <Dimension D>
void IRenderContext<D>::drawStadiumMoons(const RenderState<D> &p_State, const f32m<D> &p_Transform,
                                         const DrawFlags p_Flags)
{
    f32v<D> pos{0.f};
    pos[0] = -0.5f;
    drawChildCircle(p_State, p_Transform, pos,
                    CircleOptions{.LowerAngle = Math::Radians(90.f), .UpperAngle = Math::Radians(270.f)}, p_Flags);
    pos[0] = -pos[0];
    drawChildCircle(p_State, p_Transform, pos,
                    CircleOptions{.LowerAngle = Math::Radians(-90.f), .UpperAngle = Math::Radians(90.f)}, p_Flags);
}
template <Dimension D>
void IRenderContext<D>::drawStadiumMoons(const RenderState<D> &p_State, const f32m<D> &p_Transform, const f32 p_Length,
                                         const f32 p_Diameter, const DrawFlags p_Flags)
{
    f32v<D> pos{0.f};
    pos[0] = -0.5f * p_Length;
    drawChildCircle(p_State, p_Transform, pos, p_Diameter,
                    CircleOptions{.LowerAngle = Math::Radians(90.f), .UpperAngle = Math::Radians(270.f)}, p_Flags);
    pos[0] = -pos[0];
    drawChildCircle(p_State, p_Transform, pos, p_Diameter,
                    CircleOptions{.LowerAngle = Math::Radians(-90.f), .UpperAngle = Math::Radians(90.f)}, p_Flags);
}

template <Dimension D> void IRenderContext<D>::drawStadium(const RenderState<D> &p_State, const f32m<D> &p_Transform)
{
    const auto fill = [this, &p_State, &p_Transform](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * p_Transform, Primitives<D>::GetSquareIndex(), p_Flags);
        drawStadiumMoons(p_State, p_Transform, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 diameter = 1.f + p_State.OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, diameter);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D>::GetSquareIndex(), p_Flags);
        drawStadiumMoons(p_State, p_Transform, 1.f, diameter, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}
template <Dimension D>
void IRenderContext<D>::drawStadium(const RenderState<D> &p_State, const f32m<D> &p_Transform, const f32 p_Length,
                                    const f32 p_Diameter)
{
    const auto fill = [this, &p_State, &p_Transform, p_Length, p_Diameter](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Length);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Diameter);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D>::GetSquareIndex(), p_Flags);
        drawStadiumMoons(p_State, p_Transform, p_Length, p_Diameter, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, p_Length, p_Diameter](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 diameter = p_Diameter + p_State.OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Length);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, diameter);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D>::GetSquareIndex(), p_Flags);
        drawStadiumMoons(p_State, p_Transform, p_Length, diameter, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}

template <Dimension D> void IRenderContext<D>::Stadium()
{
    const RenderState<D> &state = *getState();
    drawStadium(state, state.Transform);
}
template <Dimension D> void IRenderContext<D>::Stadium(const f32m<D> &p_Transform)
{
    const RenderState<D> &state = *getState();
    drawStadium(state, p_Transform * state.Transform);
}

template <Dimension D> void IRenderContext<D>::Stadium(const f32 p_Length, const f32 p_Diameter)
{
    const RenderState<D> &state = *getState();
    drawStadium(state, state.Transform, p_Length, p_Diameter);
}
template <Dimension D>
void IRenderContext<D>::Stadium(const f32m<D> &p_Transform, const f32 p_Length, const f32 p_Diameter)
{
    const RenderState<D> &state = *getState();
    drawStadium(state, p_Transform * state.Transform, p_Length, p_Diameter);
}

template <Dimension D>
void IRenderContext<D>::drawRoundedSquareMoons(const RenderState<D> &p_State, const f32m<D> &p_Transform,
                                               const f32v2 &p_Dimensions, const f32 p_Diameter, const DrawFlags p_Flags)
{
    const f32 radius = 0.5f * p_Diameter;
    const f32v2 halfDims = 0.5f * p_Dimensions;
    const f32v2 paddedDims = halfDims + 0.5f * radius;

    f32v<D> pos;
    if constexpr (D == D2)
        pos = halfDims;
    else
        pos = f32v3{halfDims[0], halfDims[1], 0.f};
    for (u32 i = 0; i < 4; ++i)
    {
        f32m<D> transform = p_Transform;
        const u32 index1 = i % 2;
        const u32 index2 = 1 - index1;
        const f32 dim = i < 2 ? paddedDims[index1] : -paddedDims[index1];
        Onyx::Transform<D>::TranslateIntrinsic(transform, index1, dim);
        Onyx::Transform<D>::ScaleIntrinsic(transform, index1, radius);
        Onyx::Transform<D>::ScaleIntrinsic(transform, index2, p_Dimensions[index2]);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D>::GetSquareIndex(), p_Flags);

        constexpr f32 hpi = 0.5f * Math::Pi<f32>();
        const f32 a1 = i * hpi;
        const f32 a2 = a1 + hpi;
        drawChildCircle(p_State, p_Transform, pos, p_Diameter, CircleOptions{.LowerAngle = a1, .UpperAngle = a2},
                        p_Flags);
        pos[index1] = -pos[index1];
    }
}

template <Dimension D>
void IRenderContext<D>::drawRoundedSquare(const RenderState<D> &p_State, const f32m<D> &p_Transform)
{
    const auto fill = [this, &p_State, &p_Transform](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * p_Transform, Primitives<D>::GetSquareIndex(), p_Flags);
        drawRoundedSquareMoons(p_State, p_Transform, f32v2{1.f}, 1.f, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 thickness = 1.f + p_State.OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, thickness);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, thickness);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D>::GetSquareIndex(), p_Flags);

        drawRoundedSquareMoons(p_State, p_Transform, f32v2{1.f}, thickness, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}
template <Dimension D>
void IRenderContext<D>::drawRoundedSquare(const RenderState<D> &p_State, const f32m<D> &p_Transform,
                                          const f32v2 &p_Dimensions, const f32 p_Diameter)
{
    const auto fill = [this, &p_State, &p_Transform, &p_Dimensions, p_Diameter](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Dimensions[0]);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Dimensions[1]);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D>::GetSquareIndex(), p_Flags);

        drawRoundedSquareMoons(p_State, p_Transform, p_Dimensions, p_Diameter, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, &p_Dimensions, p_Diameter](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 width = p_State.OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, 0, p_Dimensions[0] + width);
        Onyx::Transform<D>::ScaleIntrinsic(transform, 1, p_Dimensions[1] + width);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D>::GetSquareIndex(), p_Flags);

        drawRoundedSquareMoons(p_State, p_Transform, p_Dimensions, p_Diameter + width, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}

template <Dimension D> void IRenderContext<D>::RoundedSquare()
{
    const RenderState<D> &state = *getState();
    drawRoundedSquare(state, state.Transform);
}
template <Dimension D> void IRenderContext<D>::RoundedSquare(const f32m<D> &p_Transform)
{
    const RenderState<D> &state = *getState();
    drawRoundedSquare(state, p_Transform * state.Transform);
}
template <Dimension D> void IRenderContext<D>::RoundedSquare(const f32v2 &p_Dimensions, const f32 p_Diameter)
{
    const RenderState<D> &state = *getState();
    drawRoundedSquare(state, state.Transform, p_Dimensions, p_Diameter);
}
template <Dimension D>
void IRenderContext<D>::RoundedSquare(const f32m<D> &p_Transform, const f32v2 &p_Dimensions, const f32 p_Diameter)
{
    const RenderState<D> &state = *getState();
    drawRoundedSquare(state, p_Transform * state.Transform, p_Dimensions, p_Diameter);
}
template <Dimension D> void IRenderContext<D>::RoundedSquare(const f32 p_Size, const f32 p_Diameter)
{
    RoundedSquare(f32v2{p_Size}, p_Diameter);
}
template <Dimension D>
void IRenderContext<D>::RoundedSquare(const f32m<D> &p_Transform, const f32 p_Size, const f32 p_Diameter)
{
    RoundedSquare(p_Transform, f32v2{p_Size}, p_Diameter);
}

template <Dimension D> static rot<D> computeLineRotation(const f32v<D> &p_Start, const f32v<D> &p_End)
{
    const f32v<D> delta = p_End - p_Start;
    if constexpr (D == D2)
        return Math::AntiTangent(delta[1], delta[0]);
    else
    {
        const f32v3 dir = Math::Normalize(delta);
        const f32v3 r{0.f, -dir[2], dir[1]};
        const f32 theta = 0.5f * Math::AntiCosine(dir[0]);
        if (!TKit::ApproachesZero(Math::NormSquared(r)))
            return f32q{Math::Cosine(theta), Math::Normalize(r) * Math::Sine(theta)};
        if (dir[0] < 0.f)
            return f32q{0.f, 0.f, 1.f, 0.f};
    }
#ifndef TKIT_COMPILER_MSVC
    return Detail::RotType<D>::Identity;
#endif
}

void RenderContext<D2>::Line(const f32v2 &p_Start, const f32v2 &p_End, const f32 p_Thickness)
{
    const f32v2 delta = p_End - p_Start;
    const RenderState<D2> &state = *getState();
    f32m3 transform = state.Transform;
    Onyx::Transform<D2>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D2>::RotateIntrinsic(transform, computeLineRotation<D2>(p_Start, p_End));

    drawPrimitive<D2>(state, transform, Primitives<D2>::GetSquareIndex(), f32v2{Math::Norm(delta), p_Thickness});
}
void RenderContext<D3>::Line(const f32v3 &p_Start, const f32v3 &p_End, const LineOptions &p_Options)
{
    const f32v3 delta = p_End - p_Start;
    const RenderState<D3> &state = *getState();
    f32m4 transform = state.Transform;
    Onyx::Transform<D3>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D3>::RotateIntrinsic(transform, computeLineRotation<D3>(p_Start, p_End));

    drawPrimitive<D3>(state, transform, Primitives<D3>::GetCylinderIndex(p_Options.Resolution),
                      f32v3{Math::Norm(delta), p_Options.Thickness, p_Options.Thickness});
}

void RenderContext<D2>::LineStrip(const TKit::Span<const f32v2> p_Points, const f32 p_Thickness)
{
    TKIT_ASSERT(p_Points.GetSize() > 1, "[ONYX] A line strip must have at least two points");
    for (u32 i = 0; i < p_Points.GetSize() - 1; ++i)
        Line(p_Points[i], p_Points[i + 1], p_Thickness);
}
void RenderContext<D3>::LineStrip(const TKit::Span<const f32v3> p_Points, const LineOptions &p_Options)
{
    TKIT_ASSERT(p_Points.GetSize() > 1, "[ONYX] A line strip must have at least two points");
    for (u32 i = 0; i < p_Points.GetSize() - 1; ++i)
        Line(p_Points[i], p_Points[i + 1], p_Options);
}

void RenderContext<D2>::RoundedLine(const f32v2 &p_Start, const f32v2 &p_End, const f32 p_Thickness)
{
    const f32v2 delta = p_End - p_Start;
    const RenderState<D2> &state = *getState();
    f32m3 transform = state.Transform;
    Onyx::Transform<D2>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D2>::RotateIntrinsic(transform, computeLineRotation<D2>(p_Start, p_End));

    drawStadium(state, transform, Math::Norm(delta), p_Thickness);
}
void RenderContext<D3>::RoundedLine(const f32v3 &p_Start, const f32v3 &p_End, const LineOptions &p_Options)
{
    const f32v3 delta = p_End - p_Start;
    const RenderState<D3> &state = *getState();
    f32m4 transform = state.Transform;
    Onyx::Transform<D3>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D3>::RotateIntrinsic(transform, computeLineRotation<D3>(p_Start, p_End));

    drawCapsule(state, transform, Math::Norm(delta), p_Options.Thickness, p_Options.Resolution);
}

void RenderContext<D3>::Cube()
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, state.Transform, Primitives<D3>::GetCubeIndex());
}
void RenderContext<D3>::Cube(const f32m4 &p_Transform)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, p_Transform * state.Transform, Primitives<D3>::GetCubeIndex());
}
void RenderContext<D3>::Cube(const f32v3 &p_Dimensions)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, state.Transform, Primitives<D3>::GetCubeIndex(), p_Dimensions);
}
void RenderContext<D3>::Cube(const f32m4 &p_Transform, const f32v3 &p_Dimensions)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, p_Transform * state.Transform, Primitives<D3>::GetCubeIndex(), p_Dimensions);
}
void RenderContext<D3>::Cube(const f32 p_Size)
{
    Cube(f32v3{p_Size});
}
void RenderContext<D3>::Cube(const f32m4 &p_Transform, const f32 p_Size)
{
    Cube(p_Transform, f32v3{p_Size});
}

void RenderContext<D3>::Cylinder(const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, state.Transform, Primitives<D3>::GetCylinderIndex(p_Res));
}
void RenderContext<D3>::Cylinder(const f32m4 &p_Transform, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, p_Transform * state.Transform, Primitives<D3>::GetCylinderIndex(p_Res));
}
void RenderContext<D3>::Cylinder(const f32v3 &p_Dimensions, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, state.Transform, Primitives<D3>::GetCylinderIndex(p_Res), p_Dimensions);
}
void RenderContext<D3>::Cylinder(const f32m4 &p_Transform, const f32v3 &p_Dimensions, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, p_Transform * state.Transform, Primitives<D3>::GetCylinderIndex(p_Res), p_Dimensions);
}
void RenderContext<D3>::Cylinder(const f32 p_Length, const f32 p_Diameter, const Resolution p_Res)
{
    Cylinder(f32v3{p_Length, p_Diameter, p_Diameter}, p_Res);
}
void RenderContext<D3>::Cylinder(const f32m4 &p_Transform, const f32 p_Length, const f32 p_Diameter,
                                 const Resolution p_Res)
{
    Cylinder(p_Transform, f32v3{p_Length, p_Diameter, p_Diameter}, p_Res);
}

void RenderContext<D3>::Sphere(const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, state.Transform, Primitives<D3>::GetSphereIndex(p_Res));
}
void RenderContext<D3>::Sphere(const f32m4 &p_Transform, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, p_Transform * state.Transform, Primitives<D3>::GetSphereIndex(p_Res));
}
void RenderContext<D3>::Sphere(const f32v3 &p_Dimensions, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, state.Transform, Primitives<D3>::GetSphereIndex(p_Res), p_Dimensions);
}
void RenderContext<D3>::Sphere(const f32m4 &p_Transform, const f32v3 &p_Dimensions, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawPrimitive<D3>(state, p_Transform * state.Transform, Primitives<D3>::GetSphereIndex(p_Res), p_Dimensions);
}
void RenderContext<D3>::Sphere(const f32 p_Diameter, const Resolution p_Res)
{
    Sphere(f32v3{p_Diameter}, p_Res);
}
void RenderContext<D3>::Sphere(const f32m4 &p_Transform, const f32 p_Diameter, const Resolution p_Res)
{
    Sphere(p_Transform, f32v3{p_Diameter}, p_Res);
}

void RenderContext<D3>::drawChildSphere(const RenderState<D3> &p_State, f32m4 p_Transform, const f32v3 &p_Position,
                                        const Resolution p_Res, const DrawFlags p_Flags)
{
    Onyx::Transform<D3>::TranslateIntrinsic(p_Transform, p_Position);
    m_Renderer.DrawPrimitive(p_State, p_State.Axes * p_Transform, Primitives<D3>::GetSphereIndex(p_Res), p_Flags);
}
void RenderContext<D3>::drawChildSphere(const RenderState<D3> &p_State, f32m4 p_Transform, const f32v3 &p_Position,
                                        const f32 p_Diameter, const Resolution p_Res, const DrawFlags p_Flags)
{
    Onyx::Transform<D3>::TranslateIntrinsic(p_Transform, p_Position);
    Onyx::Transform<D3>::ScaleIntrinsic(p_Transform, f32v3{p_Diameter});
    m_Renderer.DrawPrimitive(p_State, p_State.Axes * p_Transform, Primitives<D3>::GetSphereIndex(p_Res), p_Flags);
}

void RenderContext<D3>::drawCapsule(const RenderState<D3> &p_State, const f32m4 &p_Transform, const Resolution p_Res)
{
    const auto fill = [this, &p_State, &p_Transform, p_Res](const DrawFlags p_Flags) {
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * p_Transform, Primitives<D3>::GetCylinderIndex(p_Res), p_Flags);
        f32v3 pos{0.f};

        pos[0] = -0.5f;
        drawChildSphere(p_State, p_Transform, pos, p_Res, p_Flags);
        pos[0] = 0.5f;
        drawChildSphere(p_State, p_Transform, pos, p_Res, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, p_Res](const DrawFlags p_Flags) {
        const f32 thickness = 1.f + p_State.OutlineWidth;
        f32m4 transform = p_Transform;

        Onyx::Transform<D3>::ScaleIntrinsic(transform, 1, thickness);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, 2, thickness);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D3>::GetCylinderIndex(p_Res), p_Flags);
        f32v3 pos{0.f};

        pos[0] = -0.5f;
        drawChildSphere(p_State, p_Transform, pos, thickness, p_Res, p_Flags);
        pos[0] = 0.5f;
        drawChildSphere(p_State, p_Transform, pos, thickness, p_Res, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}
void RenderContext<D3>::drawCapsule(const RenderState<D3> &p_State, const f32m4 &p_Transform, const f32 p_Length,
                                    const f32 p_Diameter, const Resolution p_Res)
{
    const auto fill = [this, &p_State, &p_Transform, p_Length, p_Diameter, p_Res](const DrawFlags p_Flags) {
        f32m4 transform = p_Transform;
        Onyx::Transform<D3>::ScaleIntrinsic(transform, f32v3{p_Length, p_Diameter, p_Diameter});

        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D3>::GetCylinderIndex(p_Res), p_Flags);
        f32v3 pos{0.f};

        pos[0] = -0.5f * p_Length;
        drawChildSphere(p_State, p_Transform, pos, p_Diameter, p_Res, p_Flags);
        pos[0] = -pos[0];
        drawChildSphere(p_State, p_Transform, pos, p_Diameter, p_Res, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, p_Length, p_Diameter, p_Res](const DrawFlags p_Flags) {
        const f32 diameter = p_Diameter + p_State.OutlineWidth;
        f32m4 transform = p_Transform;
        Onyx::Transform<D3>::ScaleIntrinsic(transform, f32v3{p_Length, diameter, diameter});
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D3>::GetCylinderIndex(p_Res), p_Flags);
        f32v3 pos{0.f};

        pos[0] = -0.5f * p_Length;
        drawChildSphere(p_State, p_Transform, pos, diameter, p_Res, p_Flags);
        pos[0] = -pos[0];
        drawChildSphere(p_State, p_Transform, pos, diameter, p_Res, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}

void RenderContext<D3>::Capsule(const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawCapsule(state, state.Transform, p_Res);
}
void RenderContext<D3>::Capsule(const f32m4 &p_Transform, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawCapsule(state, p_Transform * state.Transform, p_Res);
}
void RenderContext<D3>::Capsule(const f32 p_Length, const f32 p_Diameter, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawCapsule(state, state.Transform, p_Length, p_Diameter, p_Res);
}
void RenderContext<D3>::Capsule(const f32m4 &p_Transform, const f32 p_Length, const f32 p_Diameter,
                                const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawCapsule(state, p_Transform * state.Transform, p_Length, p_Diameter, p_Res);
}

void RenderContext<D3>::drawRoundedCubeMoons(const RenderState<D3> &p_State, const f32m4 &p_Transform,
                                             const f32v3 &p_Dimensions, const f32 p_Diameter, const Resolution p_Res,
                                             const DrawFlags p_Flags)
{
    const f32 radius = 0.5f * p_Diameter;
    const f32v3 halfDims = 0.5f * p_Dimensions;
    const f32v3 paddedDims = halfDims + 0.5f * radius;
    for (u32 i = 0; i < 6; ++i)
    {
        f32m4 transform = p_Transform;
        const u32 index1 = i % 3;
        const u32 index2 = (i + 1) % 3;
        const u32 index3 = (i + 2) % 3;
        const f32 dim = i < 3 ? paddedDims[index1] : -paddedDims[index1];
        Onyx::Transform<D3>::TranslateIntrinsic(transform, index1, dim);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index1, radius);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index2, p_Dimensions[index2]);
        Onyx::Transform<D3>::ScaleIntrinsic(transform, index3, p_Dimensions[index3]);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D3>::GetCubeIndex(), p_Flags);
    }
    f32v3 pos = halfDims;
    for (u32 i = 0; i < 8; ++i)
    {
        drawChildSphere(p_State, p_Transform, pos, p_Diameter, p_Res, p_Flags);
        const u32 index = i % 2;
        pos[index] = -pos[index];
        if (i == 3)
            pos[2] = -pos[2];
    }

    for (u32 axis = 0; axis < 3; ++axis)
    {
        const u32 dimIndex1 = (axis + 1) % 3;
        const u32 dimIndex2 = (axis + 2) % 3;
        const f32v4 relevantDims{halfDims[dimIndex1], -halfDims[dimIndex1], halfDims[dimIndex2], -halfDims[dimIndex2]};
        for (u32 i = 0; i < 2; ++i)
            for (u32 j = 0; j < 2; ++j)
            {
                pos = f32v3{0.f};
                pos[dimIndex1] = relevantDims[i];
                pos[dimIndex2] = relevantDims[2 + j];

                f32m4 transform = p_Transform;
                Onyx::Transform<D3>::TranslateIntrinsic(transform, pos);
                constexpr f32 hpi = 0.5f * Math::Pi<f32>();
                if (axis > 0)
                    Onyx::Transform<D3>::RotateZIntrinsic(transform, hpi);
                if (axis > 1)
                    Onyx::Transform<D3>::RotateYIntrinsic(transform, hpi);
                Onyx::Transform<D3>::ScaleIntrinsic(transform, f32v3{p_Dimensions[axis], p_Diameter, p_Diameter});
                m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D3>::GetCylinderIndex(p_Res),
                                         p_Flags);
            }
    }
}

void RenderContext<D3>::drawRoundedCube(const RenderState<D3> &p_State, const f32m4 &p_Transform,
                                        const Resolution p_Res)
{
    const auto fill = [this, &p_State, &p_Transform, p_Res](const DrawFlags p_Flags) {
        drawRoundedCubeMoons(p_State, p_Transform, f32v3{1.f}, 1.f, p_Res, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, p_Res](const DrawFlags p_Flags) {
        const f32 thickness = 1.f + p_State.OutlineWidth;
        drawRoundedCubeMoons(p_State, p_Transform, f32v3{1.f}, thickness, p_Res, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}
void RenderContext<D3>::drawRoundedCube(const RenderState<D3> &p_State, const f32m4 &p_Transform,
                                        const f32v3 &p_Dimensions, const f32 p_Diameter, const Resolution p_Res)
{
    const auto fill = [this, &p_State, &p_Transform, &p_Dimensions, p_Diameter, p_Res](const DrawFlags p_Flags) {
        f32m4 transform = p_Transform;
        Onyx::Transform<D3>::ScaleIntrinsic(transform, p_Dimensions);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D3>::GetSquareIndex(), p_Flags);

        drawRoundedCubeMoons(p_State, p_Transform, p_Dimensions, p_Diameter, p_Res, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, &p_Dimensions, p_Diameter, p_Res](const DrawFlags p_Flags) {
        const f32 width = p_State.OutlineWidth;
        f32m4 transform = p_Transform;
        Onyx::Transform<D3>::ScaleIntrinsic(transform, p_Dimensions);
        m_Renderer.DrawPrimitive(p_State, p_State.Axes * transform, Primitives<D3>::GetSquareIndex(), p_Flags);

        drawRoundedCubeMoons(p_State, p_Transform, p_Dimensions, p_Diameter + width, p_Res, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}

void RenderContext<D3>::RoundedCube(const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawRoundedCube(state, state.Transform, p_Res);
}
void RenderContext<D3>::RoundedCube(const f32m4 &p_Transform, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawRoundedCube(state, p_Transform * state.Transform, p_Res);
}
void RenderContext<D3>::RoundedCube(const f32v3 &p_Dimensions, const f32 p_Diameter, const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawRoundedCube(state, state.Transform, p_Dimensions, p_Diameter, p_Res);
}
void RenderContext<D3>::RoundedCube(const f32m4 &p_Transform, const f32v3 &p_Dimensions, const f32 p_Diameter,
                                    const Resolution p_Res)
{
    const RenderState<D3> &state = *getState();
    drawRoundedCube(state, p_Transform * state.Transform, p_Dimensions, p_Diameter, p_Res);
}
void RenderContext<D3>::RoundedCube(const f32 p_Size, const f32 p_Diameter, const Resolution p_Res)
{
    RoundedCube(f32v3{p_Size}, p_Diameter, p_Res);
}
void RenderContext<D3>::RoundedCube(const f32m4 &p_Transform, const f32 p_Size, const f32 p_Diameter,
                                    const Resolution p_Res)
{
    RoundedCube(p_Transform, f32v3{p_Size}, p_Diameter, p_Res);
}

void RenderContext<D3>::LightColor(const Color &p_Color)
{
    getState()->LightColor = p_Color;
}
void RenderContext<D3>::AmbientColor(const Color &p_Color)
{
    m_Renderer.AmbientColor = p_Color;
}
void RenderContext<D3>::AmbientIntensity(const f32 p_Intensity)
{
    m_Renderer.AmbientColor.RGBA[3] = p_Intensity;
}

void RenderContext<D3>::DirectionalLight(Onyx::DirectionalLight p_Light)
{
    const RenderState<D3> *state = getState();
    p_Light.Direction = Math::Normalize(state->Axes * state->Transform * f32v4{p_Light.Direction, 0.f});
    m_Renderer.AddDirectionalLight(p_Light);
}
void RenderContext<D3>::DirectionalLight(const f32v3 &p_Direction, const f32 p_Intensity)
{
    Onyx::DirectionalLight light;
    light.Direction = p_Direction;
    light.Intensity = p_Intensity;
    light.Color = getState()->LightColor.Pack();
    DirectionalLight(light);
}

void RenderContext<D3>::PointLight(Onyx::PointLight p_Light)
{
    const RenderState<D3> *state = getState();
    p_Light.Position = state->Axes * state->Transform * f32v4{p_Light.Position, 1.f};
    m_Renderer.AddPointLight(p_Light);
}
void RenderContext<D3>::PointLight(const f32v3 &p_Position, const f32 p_Radius, const f32 p_Intensity)
{
    Onyx::PointLight light;
    light.Position = p_Position;
    light.Radius = p_Radius;
    light.Intensity = p_Intensity;
    light.Color = getState()->LightColor.Pack();
    PointLight(light);
}
void RenderContext<D3>::PointLight(const f32 p_Radius, const f32 p_Intensity)
{
    PointLight(f32v3{0.f}, p_Radius, p_Intensity);
}

void RenderContext<D3>::DiffuseContribution(const f32 p_Contribution)
{
    getState()->Material.DiffuseContribution = p_Contribution;
}
void RenderContext<D3>::SpecularContribution(const f32 p_Contribution)
{
    getState()->Material.SpecularContribution = p_Contribution;
}
void RenderContext<D3>::SpecularSharpness(const f32 p_Sharpness)
{
    getState()->Material.SpecularSharpness = p_Sharpness;
}
template <Dimension D> void IRenderContext<D>::AddFlags(const RenderStateFlags p_Flags)
{
    getState()->Flags |= p_Flags;
}
template <Dimension D> void IRenderContext<D>::RemoveFlags(const RenderStateFlags p_Flags)
{
    getState()->Flags &= ~p_Flags;
}

template <Dimension D> void IRenderContext<D>::Fill(const bool p_Enabled)
{
    if (p_Enabled)
        AddFlags(RenderStateFlag_Fill);
    else
        RemoveFlags(RenderStateFlag_Fill);
}

template <Dimension D>
void IRenderContext<D>::drawMesh(const RenderState<D> &p_State, const f32m<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh)
{
    const auto fill = [this, &p_State, &p_Transform, &p_Mesh](const DrawFlags p_Flags) {
        m_Renderer.DrawMesh(p_State, p_Transform, p_Mesh, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, &p_Mesh](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 scale = 1.f + p_State.OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v<D>{scale});
        m_Renderer.DrawMesh(p_State, transform, p_Mesh, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}
template <Dimension D>
void IRenderContext<D>::drawMesh(const RenderState<D> &p_State, const f32m<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh,
                                 const f32v<D> &p_Dimensions)
{
    const auto fill = [this, &p_State, &p_Transform, &p_Mesh, &p_Dimensions](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        Onyx::Transform<D>::ScaleIntrinsic(transform, p_Dimensions);
        m_Renderer.DrawMesh(p_State, transform, p_Mesh, p_Flags);
    };
    const auto outline = [this, &p_State, &p_Transform, &p_Mesh, &p_Dimensions](const DrawFlags p_Flags) {
        f32m<D> transform = p_Transform;
        const f32 width = p_State.OutlineWidth;
        Onyx::Transform<D>::ScaleIntrinsic(transform, p_Dimensions + width);
        m_Renderer.DrawMesh(p_State, transform, p_Mesh, p_Flags);
    };
    resolveDrawFlagsWithState(p_State, fill, outline);
}

template <Dimension D> void IRenderContext<D>::Mesh(const Onyx::Mesh<D> &p_Mesh)
{
    const RenderState<D> &state = *getState();
    drawMesh(state, state.Transform, p_Mesh);
}
template <Dimension D> void IRenderContext<D>::Mesh(const f32m<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh)
{
    const RenderState<D> &state = *getState();
    drawMesh(state, p_Transform * state.Transform, p_Mesh);
}
template <Dimension D> void IRenderContext<D>::Mesh(const Onyx::Mesh<D> &p_Mesh, const f32v<D> &p_Dimensions)
{
    const RenderState<D> &state = *getState();
    drawMesh(state, state.Transform, p_Mesh, p_Dimensions);
}
template <Dimension D>
void IRenderContext<D>::Mesh(const f32m<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh, const f32v<D> &p_Dimensions)
{
    const RenderState<D> &state = *getState();
    drawMesh(state, p_Transform * state.Transform, p_Mesh, p_Dimensions);
}

template <Dimension D> void IRenderContext<D>::Push(const RenderState<D> &p_State)
{
    Stack &stack = m_StateStack[getThreadIndex()];
    stack.States.Append(p_State);
    updateState();
}
template <Dimension D> void IRenderContext<D>::Push()
{
    Push(m_StateStack[getThreadIndex()].States.GetBack());
}
template <Dimension D> void IRenderContext<D>::Pop()
{
    TKIT_ASSERT(m_StateStack.GetSize() > 1, "[ONYX] For every Push(), there must be a Pop()");
    Stack &stack = m_StateStack[getThreadIndex()];
    stack.States.Pop();
    updateState();
}

template <Dimension D> void IRenderContext<D>::Alpha(const f32 p_Alpha)
{
    getState()->Material.Color.RGBA[3] = p_Alpha;
}
template <Dimension D> void IRenderContext<D>::Alpha(const u8 p_Alpha)
{
    getState()->Material.Color.RGBA[3] = static_cast<f32>(p_Alpha) / 255.f;
}
template <Dimension D> void IRenderContext<D>::Alpha(const u32 p_Alpha)
{
    getState()->Material.Color.RGBA[3] = static_cast<f32>(p_Alpha) / 255.f;
}

template <Dimension D> void IRenderContext<D>::Fill(const Color &p_Color)
{
    Fill(true);
    getState()->Material.Color = p_Color;
}
template <Dimension D> void IRenderContext<D>::Outline(const bool p_Enabled)
{
    if (p_Enabled)
        AddFlags(RenderStateFlag_Outline);
    else
        RemoveFlags(RenderStateFlag_Outline);
}
template <Dimension D> void IRenderContext<D>::Outline(const Color &p_Color)
{
    Outline(true);
    getState()->OutlineColor = p_Color;
}
template <Dimension D> void IRenderContext<D>::OutlineWidth(const f32 p_Width)
{
    Outline(true);
    getState()->OutlineWidth = p_Width;
}

template <Dimension D> void IRenderContext<D>::Material(const MaterialData<D> &p_Material)
{
    getState()->Material = p_Material;
}

template <Dimension D> void IRenderContext<D>::ShareStateStack(const u32 p_ThreadCount)
{
    TKIT_ASSERT(p_ThreadCount <= ONYX_MAX_THREADS, "[ONYX] Thread count is greater than the maximum threads allowed");
    const u32 tindex = getThreadIndex();
    for (u32 i = 0; i < p_ThreadCount; ++i)
        if (i != tindex)
            m_StateStack[i] = m_StateStack[tindex];
}

template <Dimension D> void IRenderContext<D>::ShareCurrentState(const u32 p_ThreadCount)
{
    TKIT_ASSERT(p_ThreadCount <= ONYX_MAX_THREADS, "[ONYX] Thread count is greater than the maximum threads allowed");
    const u32 tindex = getThreadIndex();
    for (u32 i = 0; i < p_ThreadCount; ++i)
        if (i != tindex)
            *m_StateStack[i].Current = *m_StateStack[tindex].Current;
}

template <Dimension D> void IRenderContext<D>::ShareState(const RenderState<D> &p_State, const u32 p_ThreadCount)
{
    TKIT_ASSERT(p_ThreadCount <= ONYX_MAX_THREADS, "[ONYX] Thread count is greater than the maximum threads allowed");
    for (u32 i = 0; i < p_ThreadCount; ++i)
        *m_StateStack[i].Current = p_State;
}

template <Dimension D> const f32m<D> &IRenderContext<D>::GetCurrentAxes() const
{
    return m_StateStack[getThreadIndex()].Current->Axes;
}
template <Dimension D> const RenderState<D> &IRenderContext<D>::GetCurrentState() const
{
    return *m_StateStack[getThreadIndex()].Current;
}
template <Dimension D> RenderState<D> &IRenderContext<D>::GetCurrentState()
{
    return *m_StateStack[getThreadIndex()].Current;
}
template <Dimension D> void IRenderContext<D>::SetCurrentState(const RenderState<D> &p_State)
{
    *m_StateStack[getThreadIndex()].Current = p_State;
}

void RenderContext<D2>::Axes(const AxesOptions<D2> &p_Options)
{
    // TODO: Parametrize this
    Color &color = getState()->Material.Color;
    const Color oldColor = color; // A cheap filthy push

    const f32v2 xLeft = f32v2{-p_Options.Size, 0.f};
    const f32v2 xRight = f32v2{p_Options.Size, 0.f};

    const f32v2 yDown = f32v2{0.f, -p_Options.Size};
    const f32v2 yUp = f32v2{0.f, p_Options.Size};

    color = Color{245u, 64u, 90u};
    Line(xLeft, xRight, p_Options.Thickness);
    color = Color{65u, 135u, 245u};
    Line(yDown, yUp, p_Options.Thickness);

    color = oldColor; // A cheap filthy pop
}

void RenderContext<D3>::Axes(const AxesOptions<D3> &p_Options)
{
    // TODO: Parametrize this
    Color &color = getState()->Material.Color;
    const Color oldColor = color; // A cheap filthy push

    const f32v3 xLeft = f32v3{-p_Options.Size, 0.f, 0.f};
    const f32v3 xRight = f32v3{p_Options.Size, 0.f, 0.f};

    const f32v3 yDown = f32v3{0.f, -p_Options.Size, 0.f};
    const f32v3 yUp = f32v3{0.f, p_Options.Size, 0.f};

    const f32v3 zBack = f32v3{0.f, 0.f, -p_Options.Size};
    const f32v3 zFront = f32v3{0.f, 0.f, p_Options.Size};

    color = Color{245u, 64u, 90u};
    Line(xLeft, xRight, {.Thickness = p_Options.Thickness, .Resolution = p_Options.Resolution});
    color = Color{65u, 135u, 245u};
    Line(yDown, yUp, {.Thickness = p_Options.Thickness, .Resolution = p_Options.Resolution});
    color = Color{180u, 245u, 65u};
    Line(zBack, zFront, {.Thickness = p_Options.Thickness, .Resolution = p_Options.Resolution});
    color = oldColor; // A cheap filthy pop
}

template class ONYX_API Detail::IRenderContext<D2>;
template class ONYX_API Detail::IRenderContext<D3>;

} // namespace Onyx
