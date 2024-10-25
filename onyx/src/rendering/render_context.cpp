#include "core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/app/input.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/app/window.hpp"

#include "kit/utilities/math.hpp"

namespace ONYX
{
template <u32 N>
    requires(IsDim<N>())
IRenderContext<N>::IRenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : m_Renderer(p_Window, p_RenderPass, &m_RenderState), m_Window(p_Window)
{
    m_RenderState.push_back(RenderState<N>{});
    // All axes transformation come "from the right", and axes offset must come "from the left", so it is actually fine
    // to have the axes starting as the current offset. Can only be done in 3D, because transformations may involve some
    // axis that dont exist in 2D. This offset is apply later for 2D cases, when eventually the mat3's become mat4's
    if constexpr (N == 3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}

template <glm::length_t N>
static void translateIntrinsic(mat<N> &p_Transform, const glm::length_t index, const f32 p_Translation) noexcept
{
    for (glm::length_t i = 0; i < N; ++i)
        p_Transform[N][i] += p_Transform[index][i] * p_Translation;
}
template <glm::length_t N> static void translateIntrinsic(mat<N> &p_Transform, const vec<N> &p_Translation) noexcept
{
    for (glm::length_t i = 0; i < N; ++i)
        translateIntrinsic<N>(p_Transform, i, p_Translation[i]);
}

template <glm::length_t N>
static void translateExtrinsic(mat<N> &p_Transform, const glm::length_t index, const f32 p_Translation) noexcept
{
    p_Transform[N][index] += p_Translation;
}
template <glm::length_t N> static void translateExtrinsic(mat<N> &p_Transform, const vec<N> &p_Translation) noexcept
{
    for (glm::length_t i = 0; i < N; ++i)
        p_Transform[N][i] += p_Translation[i];
}

template <glm::length_t N>
static void scaleIntrinsic(mat<N> &p_Transform, const glm::length_t index, const f32 p_Scale) noexcept
{
    for (glm::length_t i = 0; i < N; ++i)
        p_Transform[index][i] *= p_Scale;
}
template <glm::length_t N> static void scaleIntrinsic(mat<N> &p_Transform, const vec<N> &p_Scale) noexcept
{
    for (glm::length_t i = 0; i < N; ++i)
        for (glm::length_t j = 0; j < N; ++j)
            p_Transform[i][j] *= p_Scale[i];
}
template <glm::length_t N>
static void scaleExtrinsic(mat<N> &p_Transform, const glm::length_t index, const f32 p_Scale) noexcept
{
    for (glm::length_t i = 0; i < N + 1; ++i)
        p_Transform[i][index] *= p_Scale;
}
template <glm::length_t N> static void scaleExtrinsic(mat<N> &p_Transform, const vec<N> &p_Scale) noexcept
{
    for (glm::length_t i = 0; i < N + 1; ++i)
        for (glm::length_t j = 0; j < N; ++j)
            p_Transform[i][j] *= p_Scale[j];
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::FlushState() noexcept
{
    KIT_ASSERT(m_RenderState.size() == 1, "For every push, there must be a pop");
    m_RenderState[0] = RenderState<N>{};
    if constexpr (N == 3)
        ApplyCoordinateSystem(m_RenderState[0].Axes, &m_RenderState[0].InverseAxes);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::FlushState(const Color &p_Color) noexcept
{
    FlushState();
    m_Window->BackgroundColor = p_Color;
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::FlushRenderData() noexcept
{
    m_Renderer.Flush();
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Flush() noexcept
{
    FlushRenderData();
    FlushState();
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Flush(const Color &p_Color) noexcept
{
    FlushRenderData();
    FlushState(p_Color);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Transform(const mat<N> &p_Transform) noexcept
{
    m_RenderState.back().Transform = p_Transform * m_RenderState.back().Transform;
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Transform(const vec<N> &p_Translation, const vec<N> &p_Scale, const rot<N> &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform<N>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Transform(const vec<N> &p_Translation, const f32 p_Scale, const rot<N> &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform<N>::ComputeTransform(p_Translation, vec<N>{p_Scale}, p_Rotation));
}
void RenderContext<3>::Transform(const vec3 &p_Translation, const vec3 &p_Scale, const vec3 &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform3D::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
void RenderContext<3>::Transform(const vec3 &p_Translation, const f32 p_Scale, const vec3 &p_Rotation) noexcept
{
    this->Transform(ONYX::Transform3D::ComputeTransform(p_Translation, vec3{p_Scale}, p_Rotation));
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::TransformAxes(const mat<N> &p_Axes) noexcept
{
    m_RenderState.back().Axes *= p_Axes;
    if constexpr (N == 3)
        m_RenderState.back().InverseAxes = glm::inverse(m_RenderState.back().Axes);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::TransformAxes(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                      const rot<N> &p_Rotation) noexcept
{
    m_RenderState.back().Axes *= ONYX::Transform<N>::ComputeReverseTransform(p_Translation, p_Scale, p_Rotation);
    if constexpr (N == 3)
        m_RenderState.back().InverseAxes =
            ONYX::Transform<N>::ComputeTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation)) *
            m_RenderState.back().InverseAxes;
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::TransformAxes(const vec<N> &p_Translation, const f32 p_Scale, const rot<N> &p_Rotation) noexcept
{
    TransformAxes(p_Translation, vec<N>{p_Scale}, p_Rotation);
}
void RenderContext<3>::TransformAxes(const vec3 &p_Translation, const vec3 &p_Scale, const vec3 &p_Rotation) noexcept
{
    TransformAxes(ONYX::Transform3D::ComputeReverseTransform(p_Translation, p_Scale, p_Rotation));
}
void RenderContext<3>::TransformAxes(const vec3 &p_Translation, const f32 p_Scale, const vec3 &p_Rotation) noexcept
{
    TransformAxes(ONYX::Transform3D::ComputeReverseTransform(p_Translation, vec3{p_Scale}, p_Rotation));
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Translate(const vec<N> &p_Translation) noexcept
{
    translateExtrinsic<N>(m_RenderState.back().Transform, p_Translation);
}
void RenderContext<2>::Translate(const f32 p_X, const f32 p_Y) noexcept
{
    Translate(vec2{p_X, p_Y});
}
void RenderContext<3>::Translate(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Translate(vec3{p_X, p_Y, p_Z});
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Scale(const vec<N> &p_Scale) noexcept
{
    scaleExtrinsic<N>(m_RenderState.back().Transform, p_Scale);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Scale(const f32 p_Scale) noexcept
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

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::TranslateX(const f32 p_X) noexcept
{
    translateExtrinsic<N>(m_RenderState.back().Transform, 0, p_X);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::TranslateY(const f32 p_Y) noexcept
{
    translateExtrinsic<N>(m_RenderState.back().Transform, 1, p_Y);
}
void RenderContext<3>::TranslateZ(const f32 p_Z) noexcept
{
    translateExtrinsic<3>(m_RenderState.back().Transform, 2, p_Z);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::ScaleX(const f32 p_X) noexcept
{
    scaleExtrinsic<N>(m_RenderState.back().Transform, 0, p_X);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::ScaleY(const f32 p_Y) noexcept
{
    scaleExtrinsic<N>(m_RenderState.back().Transform, 1, p_Y);
}
void RenderContext<3>::ScaleZ(const f32 p_Z) noexcept
{
    scaleExtrinsic<3>(m_RenderState.back().Transform, 2, p_Z);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::TranslateXAxis(const f32 p_X) noexcept
{
    translateIntrinsic<N>(m_RenderState.back().Axes, 0, p_X);
    if constexpr (N == 3)
        translateExtrinsic<N>(m_RenderState.back().InverseAxes, 0, -p_X);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::TranslateYAxis(const f32 p_Y) noexcept
{
    translateIntrinsic<N>(m_RenderState.back().Axes, 1, p_Y);
    if constexpr (N == 3)
        translateExtrinsic<N>(m_RenderState.back().InverseAxes, 1, -p_Y);
}
void RenderContext<3>::TranslateZAxis(const f32 p_Z) noexcept
{
    translateIntrinsic<3>(m_RenderState.back().Axes, 2, p_Z);
    translateExtrinsic<3>(m_RenderState.back().InverseAxes, 2, -p_Z);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::ScaleXAxis(const f32 p_X) noexcept
{
    scaleIntrinsic<N>(m_RenderState.back().Axes, 0, p_X);
    if constexpr (N == 3)
        scaleExtrinsic<N>(m_RenderState.back().InverseAxes, 0, 1.f / p_X);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::ScaleYAxis(const f32 p_Y) noexcept
{
    scaleIntrinsic<N>(m_RenderState.back().Axes, 1, p_Y);
    if constexpr (N == 3)
        scaleExtrinsic<N>(m_RenderState.back().InverseAxes, 1, 1.f / p_Y);
}
void RenderContext<3>::ScaleZAxis(const f32 p_Z) noexcept
{
    scaleIntrinsic<3>(m_RenderState.back().Axes, 2, p_Z);
    scaleExtrinsic<3>(m_RenderState.back().InverseAxes, 2, 1.f / p_Z);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::KeepWindowAspect() noexcept
{
    // Scaling down the axes means "enlarging" their extent, that is why the inverse is used
    const f32 aspect = 1.f / m_Window->GetScreenAspect();
    if constexpr (N == 2)
        ScaleAxes(vec2{aspect, 1.f});
    else
        ScaleAxes(vec3{aspect, 1.f, 1.f});
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::TranslateAxes(const vec<N> &p_Translation) noexcept
{
    translateIntrinsic<N>(m_RenderState.back().Axes, p_Translation);
    if constexpr (N == 3)
        translateExtrinsic<N>(m_RenderState.back().InverseAxes, -p_Translation);
}
void RenderContext<2>::TranslateAxes(const f32 p_X, const f32 p_Y) noexcept
{
    TranslateAxes(vec2{p_X, p_Y});
}
void RenderContext<3>::TranslateAxes(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    TranslateAxes(vec3{p_X, p_Y, p_Z});
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::ScaleAxes(const vec<N> &p_Scale) noexcept
{
    scaleIntrinsic<N>(m_RenderState.back().Axes, p_Scale);
    if constexpr (N == 3)
        scaleExtrinsic<N>(m_RenderState.back().InverseAxes, 1.f / p_Scale);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::ScaleAxes(const f32 p_Scale) noexcept
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
    const mat4 rotmat = Transform3D::ComputeRotationMatrix(p_Quaternion);
    m_RenderState.back().Axes *= rotmat;
    m_RenderState.back().InverseAxes = glm::transpose(rotmat) * m_RenderState.back().InverseAxes;
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

// This could be optimized a bit
void RenderContext<3>::RotateXAxis(const f32 p_Angle) noexcept
{
    RotateAxes(vec3{p_Angle, 0.f, 0.f});
}
void RenderContext<3>::RotateYAxis(const f32 p_Angle) noexcept
{
    RotateAxes(vec3{0.f, p_Angle, 0.f});
}
void RenderContext<3>::RotateZAxis(const f32 p_Angle) noexcept
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

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Triangle() noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetTriangleIndex(), m_RenderState.back().Transform);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Triangle(const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetTriangleIndex(), p_Transform * m_RenderState.back().Transform);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Square() noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), m_RenderState.back().Transform);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Square(const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), p_Transform * m_RenderState.back().Transform);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::NGon(const u32 p_Sides) noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetNGonIndex(p_Sides), m_RenderState.back().Transform);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::NGon(const u32 p_Sides, const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives<N>::GetNGonIndex(p_Sides), p_Transform * m_RenderState.back().Transform);
}

template <u32 N>
    requires(IsDim<N>())
template <typename... Vertices>
    requires(sizeof...(Vertices) >= 3 && (std::is_same_v<Vertices, vec<N>> && ...))
void IRenderContext<N>::Polygon(Vertices &&...p_Vertices) noexcept
{
    m_Renderer.DrawPolygon({std::forward<Vertices>(p_Vertices)...}, m_RenderState.back().Transform);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Polygon(const std::span<const vec<N>> p_Vertices) noexcept
{
    m_Renderer.DrawPolygon(p_Vertices, m_RenderState.back().Transform);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Polygon(const std::span<const vec<N>> p_Vertices, const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawPolygon(p_Vertices, p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Circle() noexcept
{
    m_Renderer.DrawCircle(m_RenderState.back().Transform);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Circle(const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawCircle(p_Transform * m_RenderState.back().Transform);
}

template <u32 N>
    requires(IsDim<N>())
static void drawIntrinsicCircle(Renderer<N> &p_Renderer, mat<N> p_Transform, const vec<N> &p_Position,
                                const f32 p_Diameter) noexcept
{
    translateIntrinsic<N>(p_Transform, p_Position);
    if constexpr (N == 2)
        scaleIntrinsic<N>(p_Transform, vec<N>{p_Diameter});
    else
        scaleIntrinsic<N>(p_Transform, vec<N>{p_Diameter, p_Diameter, 1.f});
    p_Renderer.DrawCircle(p_Transform);
}
template <u32 N>
    requires(IsDim<N>())
static void drawIntrinsicCircle(Renderer<N> &p_Renderer, mat<N> p_Transform, const vec<N> &p_Position) noexcept
{
    translateIntrinsic<N>(p_Transform, p_Position);
    p_Renderer.DrawCircle(p_Transform);
}

static void drawIntrinsicSphere(Renderer3D &p_Renderer, mat4 p_Transform, const vec3 &p_Position,
                                const f32 p_Diameter) noexcept
{
    translateIntrinsic<3>(p_Transform, p_Position);
    scaleIntrinsic<3>(p_Transform, vec3{p_Diameter});
    p_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), p_Transform);
}
static void drawIntrinsicSphere(Renderer3D &p_Renderer, mat4 p_Transform, const vec3 &p_Position) noexcept
{
    translateIntrinsic<3>(p_Transform, p_Position);
    p_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), p_Transform);
}

template <u32 N>
    requires(IsDim<N>())
static void drawStadium(Renderer<N> &p_Renderer, const mat<N> &p_Transform) noexcept
{
    p_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), p_Transform);

    vec<N> pos{0.f};
    pos.x = -0.5f;
    drawIntrinsicCircle<N>(p_Renderer, p_Transform, pos);
    pos.x = 0.5f;
    drawIntrinsicCircle<N>(p_Renderer, p_Transform, pos);
}
template <u32 N>
    requires(IsDim<N>())
static void drawStadium(Renderer<N> &p_Renderer, const mat<N> &p_Transform, const f32 p_Length,
                        const f32 p_Diameter) noexcept
{
    mat<N> transform = p_Transform;
    scaleIntrinsic<N>(transform, 0, p_Length);
    scaleIntrinsic<N>(transform, 1, p_Diameter);
    p_Renderer.DrawPrimitive(Primitives<N>::GetSquareIndex(), transform);

    vec<N> pos{0.f};
    pos.x = -0.5f * p_Length;
    drawIntrinsicCircle<N>(p_Renderer, p_Transform, pos, p_Diameter);
    pos.x = -pos.x;
    drawIntrinsicCircle<N>(p_Renderer, p_Transform, pos, p_Diameter);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Stadium() noexcept
{
    drawStadium(m_Renderer, m_RenderState.back().Transform);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Stadium(const mat<N> &p_Transform) noexcept
{
    drawStadium(m_Renderer, p_Transform * m_RenderState.back().Transform);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Stadium(const f32 p_Length, const f32 p_Radius) noexcept
{
    drawStadium(m_Renderer, m_RenderState.back().Transform, p_Length, 2.f * p_Radius);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Stadium(const f32 p_Length, const f32 p_Radius, const mat<N> &p_Transform) noexcept
{
    drawStadium(m_Renderer, p_Transform * m_RenderState.back().Transform, p_Length, 2.f * p_Radius);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Line(const vec<N> &p_Start, const vec<N> &p_End, const f32 p_Thickness) noexcept
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

    const mat<N> transform = m_RenderState.back().Transform * t.ComputeTransform();
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

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::LineStrip(std::span<const vec<N>> p_Points, const f32 p_Thickness) noexcept
{
    KIT_ASSERT(p_Points.size() > 1, "A line strip must have at least two points");
    for (u32 i = 0; i < p_Points.size() - 1; ++i)
        Line(p_Points[i], p_Points[i + 1], p_Thickness);
}

void RenderContext<2>::RoundedLine(const vec2 &p_Start, const vec2 &p_End, const f32 p_Thickness) noexcept
{
    Line(p_Start, p_End, p_Thickness);

    drawIntrinsicCircle<2>(m_Renderer, m_RenderState.back().Transform, p_Start, p_Thickness);
    drawIntrinsicCircle<2>(m_Renderer, m_RenderState.back().Transform, p_End, p_Thickness);
}
void RenderContext<3>::RoundedLine(const vec3 &p_Start, const vec3 &p_End, const f32 p_Thickness) noexcept
{
    Line(p_Start, p_End, p_Thickness);

    drawIntrinsicSphere(m_Renderer, m_RenderState.back().Transform, p_Start, p_Thickness);
    drawIntrinsicSphere(m_Renderer, m_RenderState.back().Transform, p_End, p_Thickness);
}

void RenderContext<2>::RoundedLineStrip(std::span<const vec2> p_Points, const f32 p_Thickness) noexcept
{
    LineStrip(p_Points, p_Thickness);

    for (const vec2 &point : p_Points)
        drawIntrinsicCircle<2>(m_Renderer, m_RenderState.back().Transform, point, p_Thickness);
}
void RenderContext<3>::RoundedLineStrip(std::span<const vec3> p_Points, const f32 p_Thickness) noexcept
{
    LineStrip(p_Points, p_Thickness);

    for (const vec3 &point : p_Points)
        drawIntrinsicSphere(m_Renderer, m_RenderState.back().Transform, point, p_Thickness);
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
void RenderContext<3>::Cube(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetCubeIndex(), p_Transform * m_RenderState.back().Transform);
}

void RenderContext<3>::Cylinder() noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), m_RenderState.back().Transform);
}
void RenderContext<3>::Cylinder(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), p_Transform * m_RenderState.back().Transform);
}

void RenderContext<3>::Sphere() noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), m_RenderState.back().Transform);
}
void RenderContext<3>::Sphere(const mat4 &p_Transform) noexcept
{
    m_Renderer.DrawPrimitive(Primitives3D::GetSphereIndex(), p_Transform * m_RenderState.back().Transform);
}

static void drawCapsule(Renderer3D &p_Renderer, const mat4 &p_Transform) noexcept
{
    p_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), p_Transform);

    vec3 pos{0.f};
    pos.x = -0.5f;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos);
    pos.x = 0.5f;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos);
}
static void drawCapsule(Renderer3D &p_Renderer, const mat4 &p_Transform, const f32 p_Length,
                        const f32 p_Diameter) noexcept
{
    mat4 transform = p_Transform;
    scaleIntrinsic<3>(transform, {p_Length, p_Diameter, p_Diameter});
    p_Renderer.DrawPrimitive(Primitives3D::GetCylinderIndex(), transform);

    vec3 pos{0.f};
    pos.x = -0.5f * p_Length;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Diameter);
    pos.x = -pos.x;
    drawIntrinsicSphere(p_Renderer, p_Transform, pos, p_Diameter);
}

void RenderContext<3>::Capsule() noexcept
{
    drawCapsule(m_Renderer, m_RenderState.back().Transform);
}
void RenderContext<3>::Capsule(const mat4 &p_Transform) noexcept
{
    drawCapsule(m_Renderer, p_Transform * m_RenderState.back().Transform);
}

void RenderContext<3>::Capsule(const f32 p_Length, const f32 p_Radius) noexcept
{
    drawCapsule(m_Renderer, m_RenderState.back().Transform, p_Length, 2.f * p_Radius);
}
void RenderContext<3>::Capsule(const f32 p_Length, const f32 p_Radius, const mat4 &p_Transform) noexcept
{
    drawCapsule(m_Renderer, p_Transform * m_RenderState.back().Transform, p_Length, 2.f * p_Radius);
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

void RenderContext<3>::DirectionalLight(ONYX::DirectionalLight p_Light) noexcept
{
    p_Light.DirectionAndIntensity =
        vec4{glm::normalize(vec3{p_Light.DirectionAndIntensity}), p_Light.DirectionAndIntensity.w};
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

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Mesh(const KIT::Ref<const Model<N>> &p_Model) noexcept
{
    m_Renderer.DrawMesh(p_Model, m_RenderState.back().Transform);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Mesh(const KIT::Ref<const Model<N>> &p_Model, const mat<N> &p_Transform) noexcept
{
    m_Renderer.DrawMesh(p_Model, p_Transform * m_RenderState.back().Transform);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Push() noexcept
{
    m_RenderState.push_back(m_RenderState.back());
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::PushAndClear() noexcept
{
    m_RenderState.push_back(RenderState<N>{});
    if constexpr (N == 3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Pop() noexcept
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

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Fill(const Color &p_Color) noexcept
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

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Material(const MaterialData<N> &p_Material) noexcept
{
    m_RenderState.back().Material = p_Material;
}

template <u32 N>
    requires(IsDim<N>())
const RenderState<N> &IRenderContext<N>::GetState() const noexcept
{
    return m_RenderState.back();
}
template <u32 N>
    requires(IsDim<N>())
RenderState<N> &IRenderContext<N>::GetState() noexcept
{
    return m_RenderState.back();
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::SetState(const RenderState<N> &p_State) noexcept
{
    m_RenderState.back() = p_State;
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::SetCurrentTransform(const mat<N> &p_Transform) noexcept
{
    m_RenderState.back().Transform = p_Transform;
}
template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::SetCurrentAxes(const mat<N> &p_Axes) noexcept
{
    m_RenderState.back().Axes = p_Axes;
    if constexpr (N == 3)
        ApplyCoordinateSystem(m_RenderState.back().Axes, &m_RenderState.back().InverseAxes);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Render(const VkCommandBuffer p_Commandbuffer) noexcept
{
    m_Renderer.Render(p_Commandbuffer);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderContext<N>::Axes(const f32 p_Thickness, const f32 p_Size) noexcept
{
    // TODO: Parametrize this
    Color &color = m_RenderState.back().Material.Color;
    const Color oldColor = color; // A cheap filthy push
    if constexpr (N == 2)
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