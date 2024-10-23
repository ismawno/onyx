#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/renderer.hpp"

#include <vulkan/vulkan.hpp>

// GetMouseCoordinates depends on the Axes!!

// NoFill is an illusion. I could implement Stroke by drawing lines across the edges of the shape, then Implement NoFill
// by using a transparent color

// Many of the overloads could be specifically implmented to make some operations a bit more efficient, but for now they
// just rely on the general implementation. SPECIALLY RotateX, RotateY etcetera

// Translate() Rotate() etc is only valid for primitives, not lights

// Explain why TransformData3D has view data for each instance (it's because of the immediate mode)

namespace ONYX
{
class Window;

ONYX_DIMENSION_TEMPLATE class ONYX_API IRenderContext
{
  public:
    IRenderContext(Window *p_Window, VkRenderPass p_RenderPass) noexcept;

    void Flush() noexcept;
    void Flush(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Flush(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Flush(color);
    }

    void Transform(const mat<N> &p_Transform) noexcept;
    void Transform(const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept;
    void Transform(const vec<N> &p_Translation, const vec<N> &p_Scale, const rot<N> &p_Rotation) noexcept;
    void Transform(const vec<N> &p_Translation, f32 p_Scale) noexcept;
    void Transform(const vec<N> &p_Translation, f32 p_Scale, const rot<N> &p_Rotation) noexcept;

    void TransformAxes(const mat<N> &p_Transform) noexcept;
    void TransformAxes(const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept;
    void TransformAxes(const vec<N> &p_Translation, const vec<N> &p_Scale, const rot<N> &p_Rotation) noexcept;
    void TransformAxes(const vec<N> &p_Translation, f32 p_Scale) noexcept;
    void TransformAxes(const vec<N> &p_Translation, f32 p_Scale, const rot<N> &p_Rotation) noexcept;

    void Translate(const vec<N> &p_Translation) noexcept;
    void Scale(const vec<N> &p_Scale) noexcept;
    void Scale(f32 p_Scale) noexcept;

    void KeepWindowAspect() noexcept;
    void TranslateAxes(const vec<N> &p_Translation) noexcept;
    void ScaleAxes(const vec<N> &p_Scale) noexcept;
    void ScaleAxes(f32 p_Scale) noexcept;

    void Axes(f32 p_Thickness = 0.01f, f32 p_Size = 50.f) noexcept;

    void Triangle() noexcept;
    void Triangle(const mat<N> &p_Transform) noexcept;
    void Triangle(f32 p_Size) noexcept;
    void Triangle(const vec<N> &p_Position) noexcept;
    void Triangle(const vec<N> &p_Position, f32 p_Size) noexcept;
    void Triangle(const vec<N> &p_Position, f32 p_Size, const rot<N> &p_Rotation) noexcept;

    void Square() noexcept;
    void Square(f32 p_Size) noexcept;
    void Square(const vec<N> &p_Position) noexcept;
    void Square(const vec<N> &p_Position, f32 p_Size) noexcept;
    void Square(const vec<N> &p_Position, f32 p_Size, const rot<N> &p_Rotation) noexcept;

    void Rect(f32 p_XDim, f32 p_YDim) noexcept;
    void Rect(const mat<N> &p_Transform) noexcept;
    void Rect(const vec2 &p_Dimensions) noexcept;
    void Rect(const vec<N> &p_Position, const vec2 &p_Dimensions) noexcept;
    void Rect(const vec<N> &p_Position, const vec2 &p_Dimensions, const rot<N> &p_Rotation) noexcept;

    void NGon(u32 p_Sides) noexcept;
    void NGon(u32 p_Sides, f32 p_radius) noexcept;
    void NGon(u32 p_Sides, const mat<N> &p_Transform) noexcept;
    void NGon(u32 p_Sides, const vec<N> &p_Position) noexcept;
    void NGon(u32 p_Sides, const vec<N> &p_Position, f32 p_Radius) noexcept;
    void NGon(u32 p_Sides, const vec<N> &p_Position, f32 p_Radius, const rot<N> &p_Rotation) noexcept;

    template <typename... Vertices>
        requires(sizeof...(Vertices) >= 3 && (std::is_same_v<Vertices, vec<N>> && ...))
    void Polygon(Vertices &&...p_Vertices) noexcept;
    void Polygon(std::span<const vec<N>> p_Vertices) noexcept;
    void Polygon(std::span<const vec<N>> p_Vertices, const mat<N> &p_Transform) noexcept;
    void Polygon(std::span<const vec<N>> p_Vertices, const vec<N> &p_Translation) noexcept;
    void Polygon(std::span<const vec<N>> p_Vertices, const vec<N> &p_Translation, const rot<N> &p_Rotation) noexcept;

    void Circle() noexcept;
    void Circle(f32 p_Radius) noexcept;
    void Circle(const vec<N> &p_Position) noexcept;
    void Circle(const vec<N> &p_Position, f32 p_Radius) noexcept;

    void Ellipse(f32 p_XDim, f32 p_YDim) noexcept;
    void Ellipse(const mat<N> &p_Transform) noexcept;
    void Ellipse(const vec2 &p_Dimensions) noexcept;
    void Ellipse(const vec<N> &p_Position, const vec2 &p_Dimensions) noexcept;
    void Ellipse(const vec<N> &p_Position, const vec2 &p_Dimensions, const rot<N> &p_Rotation) noexcept;

    void Line(const vec<N> &p_Start, const vec<N> &p_End, f32 p_Thickness = 0.01f) noexcept;
    void LineStrip(std::span<const vec<N>> p_Points, f32 p_Thickness = 0.01f) noexcept;

    void Mesh(const KIT::Ref<const Model<N>> &p_Model) noexcept;
    void Mesh(const KIT::Ref<const Model<N>> &p_Model, const mat<N> &p_Transform) noexcept;
    void Mesh(const KIT::Ref<const Model<N>> &p_Model, const vec<N> &p_Translation) noexcept;
    void Mesh(const KIT::Ref<const Model<N>> &p_Model, const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept;
    void Mesh(const KIT::Ref<const Model<N>> &p_Model, const vec<N> &p_Translation, const vec<N> &p_Scale,
              const rot<N> &p_Rotation) noexcept;

    void Push() noexcept;
    void PushAndClear() noexcept;

    void Pop() noexcept;

    void Fill(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Fill(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Fill(color);
    }

    const RenderState<N> &GetState() const noexcept;
    RenderState<N> &GetState() noexcept;

    void SetState(const RenderState<N> &p_State) noexcept;

    void SetCurrentTransform(const mat<N> &p_Transform) noexcept;
    void SetCurrentAxes(const mat<N> &p_Axes) noexcept;

    void Reset() noexcept;
    void Render(VkCommandBuffer p_CommandBuffer) noexcept;

  protected:
    DynamicArray<RenderState<N>> m_RenderState;
    Renderer<N> m_Renderer;
    Window *m_Window;
};

ONYX_DIMENSION_TEMPLATE class RenderContext;

template <> class ONYX_API RenderContext<2> final : public IRenderContext<2>
{
  public:
    using IRenderContext<2>::IRenderContext;
    using IRenderContext<2>::Transform;
    using IRenderContext<2>::TransformAxes;
    using IRenderContext<2>::Translate;
    using IRenderContext<2>::Scale;
    using IRenderContext<2>::TranslateAxes;
    using IRenderContext<2>::ScaleAxes;
    using IRenderContext<2>::Triangle;
    using IRenderContext<2>::Square;
    using IRenderContext<2>::Rect;
    using IRenderContext<2>::NGon;
    using IRenderContext<2>::Polygon;
    using IRenderContext<2>::Circle;
    using IRenderContext<2>::Ellipse;
    using IRenderContext<2>::Line;
    using IRenderContext<2>::Fill;
    using IRenderContext<2>::Mesh;

    void Transform(f32 p_X, f32 p_Y, f32 p_Scale) noexcept;
    void Transform(f32 p_X, f32 p_Y, f32 p_XS, f32 p_YS) noexcept;
    void Transform(f32 p_X, f32 p_Y, f32 p_XS, f32 p_YS, f32 p_Rotation) noexcept;

    void TransformAxes(f32 p_X, f32 p_Y, f32 p_Scale) noexcept;
    void TransformAxes(f32 p_X, f32 p_Y, f32 p_XS, f32 p_YS) noexcept;
    void TransformAxes(f32 p_X, f32 p_Y, f32 p_XS, f32 p_YS, f32 p_Rotation) noexcept;

    void Translate(f32 p_X, f32 p_Y) noexcept;
    void Scale(f32 p_X, f32 p_Y) noexcept;
    void Rotate(f32 p_Angle) noexcept;

    void TranslateAxes(f32 p_X, f32 p_Y) noexcept;
    void ScaleAxes(f32 p_X, f32 p_Y) noexcept;
    void RotateAxes(f32 p_Angle) noexcept;

    void Triangle(f32 p_X, f32 p_Y) noexcept;
    void Triangle(f32 p_X, f32 p_Y, f32 p_Size) noexcept;
    void Triangle(f32 p_X, f32 p_Y, f32 p_Size, f32 p_Rotation) noexcept;

    void Square(f32 p_X, f32 p_Y) noexcept;
    void Square(f32 p_X, f32 p_Y, f32 p_Size) noexcept;
    void Square(f32 p_X, f32 p_Y, f32 p_Size, f32 p_Rotation) noexcept;

    void Rect(f32 p_X, f32 p_Y, f32 p_XDim, f32 p_YDim) noexcept;
    void Rect(f32 p_X, f32 p_Y, f32 p_XDim, f32 p_YDim, f32 p_Rotation) noexcept;

    void NGon(u32 p_Sides, f32 p_X, f32 p_Y) noexcept;
    void NGon(u32 p_Sides, f32 p_X, f32 p_Y, f32 p_Radius) noexcept;
    void NGon(u32 p_Sides, f32 p_X, f32 p_Y, f32 p_Radius, f32 p_Rotation) noexcept;

    void Polygon(std::span<const vec2> p_Vertices, f32 p_X, f32 p_Y) noexcept;
    void Polygon(std::span<const vec2> p_Vertices, f32 p_X, f32 p_Y, f32 p_Rotation) noexcept;

    void Circle(f32 p_X, f32 p_Y) noexcept;
    void Circle(f32 p_X, f32 p_Y, f32 p_Radius) noexcept;

    void Ellipse(f32 p_X, f32 p_Y, f32 p_XDim, f32 p_YDim) noexcept;
    void Ellipse(f32 p_X, f32 p_Y, f32 p_XDim, f32 p_YDim, f32 p_Rotation) noexcept;

    void Line(f32 p_StartX, f32 p_StartY, f32 p_EndX, f32 p_EndY, f32 p_Thickness = 0.01f) noexcept;

    void RoundedLine(const vec2 &p_Start, const vec2 &p_End, f32 p_Thickness = 0.01f) noexcept;
    void RoundedLine(f32 p_StartX, f32 p_StartY, f32 p_EndX, f32 p_EndY, f32 p_Thickness = 0.01f) noexcept;

    void RoundedLineStrip(std::span<const vec2> p_Points, f32 p_Thickness = 0.01f) noexcept;

    void Mesh(const KIT::Ref<const Model2D> &p_Model, f32 p_X, f32 p_Y) noexcept;
    void Mesh(const KIT::Ref<const Model2D> &p_Model, f32 p_X, f32 p_Y, f32 p_Scale) noexcept;
    void Mesh(const KIT::Ref<const Model2D> &p_Model, f32 p_X, f32 p_Y, f32 p_XS, f32 p_YS) noexcept;
    void Mesh(const KIT::Ref<const Model2D> &p_Model, f32 p_X, f32 p_Y, f32 p_XS, f32 p_YS, f32 p_Rotation) noexcept;

    void Fill() noexcept;
    void NoFill() noexcept;

    void Alpha(f32 p_Alpha) noexcept;
    void Alpha(u8 p_Alpha) noexcept;
    void Alpha(u32 p_Alpha) noexcept;

    void Stroke() noexcept;
    void Stroke(const Color &p_Color) noexcept;

    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Stroke(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Stroke(color);
    }

    void StrokeWidth(f32 p_Width) noexcept;
    void NoStroke() noexcept;

    vec2 GetMouseCoordinates() const noexcept;
};
template <> class ONYX_API RenderContext<3> final : public IRenderContext<3>
{
  public:
    using IRenderContext<3>::IRenderContext;
    using IRenderContext<3>::Transform;
    using IRenderContext<3>::TransformAxes;
    using IRenderContext<3>::Translate;
    using IRenderContext<3>::Scale;
    using IRenderContext<3>::TranslateAxes;
    using IRenderContext<3>::ScaleAxes;
    using IRenderContext<3>::Triangle;
    using IRenderContext<3>::Square;
    using IRenderContext<3>::Rect;
    using IRenderContext<3>::NGon;
    using IRenderContext<3>::Polygon;
    using IRenderContext<3>::Circle;
    using IRenderContext<3>::Ellipse;
    using IRenderContext<3>::Line;
    using IRenderContext<3>::Mesh;

    void Transform(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Scale) noexcept;
    void Transform(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS) noexcept;

    void Transform(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Scale, const quat &p_Quaternion) noexcept;
    void Transform(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS, const quat &p_Quaternion) noexcept;

    void Transform(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Scale, const vec3 &p_Angles) noexcept;
    void Transform(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS, const vec3 &p_Angles) noexcept;
    void Transform(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS, f32 p_XRot, f32 p_YRot,
                   f32 p_ZRot) noexcept;

    void TransformAxes(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Scale) noexcept;
    void TransformAxes(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS) noexcept;

    void TransformAxes(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Scale, const quat &p_Quaternion) noexcept;
    void TransformAxes(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS, const quat &p_Quaternion) noexcept;

    void TransformAxes(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Scale, const vec3 &p_Angles) noexcept;
    void TransformAxes(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS, const vec3 &p_Angles) noexcept;
    void TransformAxes(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS, f32 p_XRot, f32 p_YRot,
                       f32 p_ZRot) noexcept;

    void Translate(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Scale(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    // Quaternions need to be normalized

    void Rotate(const quat &p_Quaternion) noexcept;
    void Rotate(const vec3 &p_Angles) noexcept;
    void Rotate(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Rotate(f32 p_Angle, const vec3 &p_Axis) noexcept;

    void RotateX(f32 p_X) noexcept;
    void RotateY(f32 p_Y) noexcept;
    void RotateZ(f32 p_Z) noexcept;

    void TranslateAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void ScaleAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    void RotateAxes(const quat &p_Quaternion) noexcept;
    void RotateAxes(const vec3 &p_Angles) noexcept;
    void RotateAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void RotateAxes(f32 p_Angle, const vec3 &p_Axis) noexcept;

    void RotateXAxes(f32 p_X) noexcept;
    void RotateYAxes(f32 p_Y) noexcept;
    void RotateZAxes(f32 p_Z) noexcept;

    void Triangle(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Triangle(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size) noexcept;
    void Triangle(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size, const quat &p_Quaternion) noexcept;
    void Triangle(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;
    void Triangle(const vec3 &p_Position, f32 p_Size, const vec3 &p_Angles) noexcept;

    void Square(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Square(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size) noexcept;
    void Square(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size, const quat &p_Quaternion) noexcept;
    void Square(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;
    void Square(const vec3 &p_Position, f32 p_Size, const vec3 &p_Angles) noexcept;

    void Rect(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim) noexcept;
    void Rect(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, const quat &p_Quaternion) noexcept;
    void Rect(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;
    void Rect(const vec3 &p_Position, const vec2 &p_Dimensions, const vec3 &p_Angles) noexcept;

    void NGon(u32 p_Sides, f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void NGon(u32 p_Sides, f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius) noexcept;
    void NGon(u32 p_Sides, f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius, const quat &p_Quaternion) noexcept;
    void NGon(u32 p_Sides, f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;
    void NGon(u32 p_Sides, const vec3 &p_Position, f32 p_Radius, const vec3 &p_Angles) noexcept;

    void Polygon(std::span<const vec3> p_Vertices, f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Polygon(std::span<const vec3> p_Vertices, f32 p_X, f32 p_Y, f32 p_Z, const quat &p_Quaternion) noexcept;
    void Polygon(std::span<const vec3> p_Vertices, f32 p_X, f32 p_Y, f32 p_Z, f32 p_XRot, f32 p_YRot,
                 f32 p_ZRot) noexcept;
    void Polygon(std::span<const vec3> p_Vertices, const vec3 &p_Position, const vec3 &p_Angles) noexcept;

    void Circle(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Circle(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius) noexcept;

    void Ellipse(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim) noexcept;
    void Ellipse(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, const quat &p_Quaternion) noexcept;
    void Ellipse(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;
    void Ellipse(const vec3 &p_Position, const vec2 &p_Dimensions, const vec3 &p_Angles) noexcept;

    void Line(f32 p_StartX, f32 p_StartY, f32 p_StartZ, f32 p_EndX, f32 p_EndY, f32 p_EndZ,
              f32 p_Thickness = 0.01f) noexcept;

    void RoundedLine(const vec3 &p_Start, const vec3 &p_End, f32 p_Thickness = 0.01f) noexcept;
    void RoundedLine(f32 p_StartX, f32 p_StartY, f32 p_StartZ, f32 p_EndX, f32 p_EndY, f32 p_EndZ,
                     f32 p_Thickness = 0.01f) noexcept;

    void RoundedLineStrip(std::span<const vec3> p_Points, f32 p_Thickness = 0.01f) noexcept;

    void Mesh(const KIT::Ref<const Model3D> &p_Model, f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Mesh(const KIT::Ref<const Model3D> &p_Model, f32 p_X, f32 p_Y, f32 p_Z, f32 p_Scale) noexcept;
    void Mesh(const KIT::Ref<const Model3D> &p_Model, f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS) noexcept;
    void Mesh(const KIT::Ref<const Model3D> &p_Model, f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS,
              const quat &p_Quaternion) noexcept;
    void Mesh(const KIT::Ref<const Model3D> &p_Model, f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS,
              const vec3 &p_Angles) noexcept;
    void Mesh(const KIT::Ref<const Model3D> &p_Model, f32 p_X, f32 p_Y, f32 p_Z, f32 p_XS, f32 p_YS, f32 p_ZS,
              f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;

    void Cube() noexcept;
    void Cube(f32 p_Size) noexcept;

    void Cube(const vec3 &p_Position) noexcept;
    void Cube(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    void Cube(const vec3 &p_Position, f32 p_Size) noexcept;
    void Cube(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size) noexcept;

    void Cube(const vec3 &p_Position, f32 p_Size, const quat &p_Quaternion) noexcept;
    void Cube(const vec3 &p_Position, f32 p_Size, const vec3 &p_Angles) noexcept;
    void Cube(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size, const quat &p_Quaternion) noexcept;
    void Cube(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Size, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;

    void Cuboid(const mat4 &p_Transform) noexcept;
    void Cuboid(const vec3 &p_Dimensions) noexcept;
    void Cuboid(f32 p_XDim, f32 p_YDim, f32 p_ZDim) noexcept;

    void Cuboid(const vec3 &p_Position, const vec3 &p_Dimensions) noexcept;
    void Cuboid(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_ZDim) noexcept;

    void Cuboid(const vec3 &p_Position, const vec3 &p_Dimensions, const quat &p_Quaternion) noexcept;
    void Cuboid(const vec3 &p_Position, const vec3 &p_Dimensions, const vec3 &p_Angles) noexcept;
    void Cuboid(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_ZDim, const quat &p_Quaternion) noexcept;
    void Cuboid(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_ZDim, f32 p_XRot, f32 p_YRot,
                f32 p_ZRot) noexcept;

    void Cylinder() noexcept;
    void Cylinder(f32 p_Radius, f32 p_Length) noexcept;

    void Cylinder(const mat4 &p_Transform) noexcept;
    void Cylinder(const vec2 &p_Dimensions) noexcept;

    void Cylinder(const vec3 &p_Position, const vec2 &p_Dimensions) noexcept;
    void Cylinder(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius, f32 p_Length) noexcept;

    void Cylinder(const vec3 &p_Position, const vec2 &p_Dimensions, const quat &p_Quaternion) noexcept;
    void Cylinder(const vec3 &p_Position, const vec2 &p_Dimensions, const vec3 &p_Angles) noexcept;
    void Cylinder(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius, f32 p_Length, const quat &p_Quaternion) noexcept;
    void Cylinder(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius, f32 p_Length, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;

    void Sphere() noexcept;
    void Sphere(f32 p_Radius) noexcept;
    void Sphere(const vec3 &p_Position) noexcept;
    void Sphere(const vec3 &p_Position, f32 p_Radius) noexcept;

    void Sphere(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Sphere(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius) noexcept;

    void Ellipsoid(f32 p_XDim, f32 p_YDim, f32 p_ZDim) noexcept;
    void Ellipsoid(const mat4 &p_Transform) noexcept;
    void Ellipsoid(const vec3 &p_Dimensions) noexcept;

    void Ellipsoid(const vec3 &p_Position, const vec3 &p_Dimensions) noexcept;
    void Ellipsoid(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_ZDim) noexcept;

    void Ellipsoid(const vec3 &p_Position, const vec3 &p_Dimensions, const quat &p_Quaternion) noexcept;
    void Ellipsoid(const vec3 &p_Position, const vec3 &p_Dimensions, const vec3 &p_Angles) noexcept;
    void Ellipsoid(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_ZDim, const quat &p_Quaternion) noexcept;
    void Ellipsoid(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_ZDim, f32 p_XRot, f32 p_YRot,
                   f32 p_ZRot) noexcept;

    void LightColor(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void LightColor(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        LightColor(color);
    }

    void AmbientColor(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void AmbientColor(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        AmbientColor(color);
    }
    void AmbientIntensity(f32 p_Intensity) noexcept;

    void DirectionalLight(const vec3 &p_Direction, f32 p_Intensity = 1.f) noexcept;
    void DirectionalLight(f32 p_DX, f32 p_DY, f32 p_DZ, f32 p_Intensity = 1.f) noexcept;

    void PointLight(f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;
    void PointLight(const vec3 &p_Position, f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;
    void PointLight(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;

    void Projection(const mat4 &p_Projection) noexcept;
    void Perspective(f32 p_FieldOfView, f32 p_Near, f32 p_Far) noexcept;
    void Orthographic() noexcept;

    vec3 GetMouseCoordinates(f32 p_Depth) const noexcept;
};

using RenderContext2D = RenderContext<2>;
using RenderContext3D = RenderContext<3>;
} // namespace ONYX