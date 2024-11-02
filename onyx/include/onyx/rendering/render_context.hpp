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

// Explain why InstanceData3D has view data for each instance (it's because of the immediate mode)

// Ambient intensity must be called AFTER ambient color

// 2D objects that are drawn later will always be on top of earlier ones. HOWEVER, blending will only work expectedly
// between objects of the same primitive

// Because of batch rendering, draw order is not guaranteed

namespace ONYX
{
class Window;

template <Dimension D> class ONYX_API IRenderContext
{
  public:
    IRenderContext(Window *p_Window, VkRenderPass p_RenderPass) noexcept;

    void FlushDrawData() noexcept;

    void FlushState() noexcept;
    void FlushState(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void FlushState(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        FlushState(color);
    }

    void Flush() noexcept;
    void Flush(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Flush(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Flush(color);
    }

    void Transform(const mat<D> &p_Transform) noexcept;
    void Transform(const vec<D> &p_Translation, const vec<D> &p_Scale, const rot<D> &p_Rotation) noexcept;
    void Transform(const vec<D> &p_Translation, f32 p_Scale, const rot<D> &p_Rotation) noexcept;

    void TransformAxes(const mat<D> &p_Transform) noexcept;
    void TransformAxes(const vec<D> &p_Translation, const vec<D> &p_Scale, const rot<D> &p_Rotation) noexcept;
    void TransformAxes(const vec<D> &p_Translation, f32 p_Scale, const rot<D> &p_Rotation) noexcept;

    void Translate(const vec<D> &p_Translation) noexcept;
    void Scale(const vec<D> &p_Scale) noexcept;
    void Scale(f32 p_Scale) noexcept;

    void TranslateX(f32 p_X) noexcept;
    void TranslateY(f32 p_Y) noexcept;

    void ScaleX(f32 p_X) noexcept;
    void ScaleY(f32 p_Y) noexcept;

    void TranslateAxes(const vec<D> &p_Translation) noexcept;
    void ScaleAxes(const vec<D> &p_Scale) noexcept;
    void ScaleAxes(f32 p_Scale) noexcept;

    void TranslateXAxis(f32 p_X) noexcept;
    void TranslateYAxis(f32 p_Y) noexcept;

    void ScaleXAxis(f32 p_X) noexcept;
    void ScaleYAxis(f32 p_Y) noexcept;

    void KeepWindowAspect() noexcept;

    void Axes(f32 p_Thickness = 0.01f, f32 p_Size = 50.f) noexcept;

    void Triangle() noexcept;
    void Triangle(const mat<D> &p_Transform) noexcept;

    void Square() noexcept;
    void Square(const mat<D> &p_Transform) noexcept;

    void NGon(u32 p_Sides) noexcept;
    void NGon(u32 p_Sides, const mat<D> &p_Transform) noexcept;

    template <typename... Vertices>
        requires(sizeof...(Vertices) >= 3 && (std::is_same_v<Vertices, vec<D>> && ...))
    void Polygon(Vertices &&...p_Vertices) noexcept;
    void Polygon(std::span<const vec<D>> p_Vertices) noexcept;
    void Polygon(std::span<const vec<D>> p_Vertices, const mat<D> &p_Transform) noexcept;

    void Circle() noexcept;
    void Circle(const mat<D> &p_Transform) noexcept;

    void Circle(f32 p_LowerAngle, f32 p_UpperAngle, f32 p_Hollowness = 0.f) noexcept;
    void Circle(f32 p_LowerAngle, f32 p_UpperAngle, const mat<D> &p_Transform) noexcept;
    void Circle(f32 p_LowerAngle, f32 p_UpperAngle, f32 p_Hollowness, const mat<D> &p_Transform) noexcept;

    void Stadium() noexcept;
    void Stadium(const mat<D> &p_Transform) noexcept;

    void Stadium(f32 p_Length, f32 p_Radius) noexcept;
    void Stadium(f32 p_Length, f32 p_Radius, const mat<D> &p_Transform) noexcept;

    void RoundedSquare() noexcept;
    void RoundedSquare(const mat<D> &p_Transform) noexcept;

    void RoundedSquare(const vec2 &p_Dimensions, f32 p_Radius) noexcept;
    void RoundedSquare(const vec2 &p_Dimensions, f32 p_Radius, const mat<D> &p_Transform) noexcept;

    void RoundedSquare(f32 p_Width, f32 p_Height, f32 p_Radius) noexcept;
    void RoundedSquare(f32 p_Width, f32 p_Height, f32 p_Radius, const mat<D> &p_Transform) noexcept;

    void Line(const vec<D> &p_Start, const vec<D> &p_End, f32 p_Thickness = 0.01f) noexcept;
    void LineStrip(std::span<const vec<D>> p_Points, f32 p_Thickness = 0.01f) noexcept;

    void Mesh(const KIT::Ref<const Model<D>> &p_Model) noexcept;
    void Mesh(const KIT::Ref<const Model<D>> &p_Model, const mat<D> &p_Transform) noexcept;

    void Push() noexcept;
    void PushAndClear() noexcept;

    void Pop() noexcept;

    void Fill(bool p_Enable = true) noexcept;
    void Fill(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Fill(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Fill(color);
    }

    void Outline(bool p_Enable = true) noexcept;
    void Outline(const Color &p_Color) noexcept;

    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Outline(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Outline(color);
    }

    void OutlineWidth(f32 p_Width) noexcept;
    void Material(const MaterialData<D> &p_Material) noexcept;

    const RenderState<D> &GetState() const noexcept;
    RenderState<D> &GetState() noexcept;

    void SetState(const RenderState<D> &p_State) noexcept;

    void SetCurrentTransform(const mat<D> &p_Transform) noexcept;
    void SetCurrentAxes(const mat<D> &p_Axes) noexcept;

    void Render(VkCommandBuffer p_CommandBuffer) noexcept;

  protected:
    DynamicArray<RenderState<D>> m_RenderState;
    Renderer<D> m_Renderer;
    Window *m_Window;
};

template <Dimension D> class RenderContext;

template <> class ONYX_API RenderContext<D2> final : public IRenderContext<D2>
{
  public:
    using IRenderContext<D2>::IRenderContext;
    using IRenderContext<D2>::Transform;
    using IRenderContext<D2>::TransformAxes;
    using IRenderContext<D2>::Translate;
    using IRenderContext<D2>::Scale;
    using IRenderContext<D2>::TranslateAxes;
    using IRenderContext<D2>::ScaleAxes;
    using IRenderContext<D2>::Line;

    void Translate(f32 p_X, f32 p_Y) noexcept;
    void Scale(f32 p_X, f32 p_Y) noexcept;
    void Rotate(f32 p_Angle) noexcept;

    void TranslateAxes(f32 p_X, f32 p_Y) noexcept;
    void ScaleAxes(f32 p_X, f32 p_Y) noexcept;
    void RotateAxes(f32 p_Angle) noexcept;

    void Line(f32 p_StartX, f32 p_StartY, f32 p_EndX, f32 p_EndY, f32 p_Thickness = 0.01f) noexcept;

    void RoundedLine(const vec2 &p_Start, const vec2 &p_End, f32 p_Thickness = 0.01f) noexcept;
    void RoundedLine(f32 p_StartX, f32 p_StartY, f32 p_EndX, f32 p_EndY, f32 p_Thickness = 0.01f) noexcept;

    void RoundedLineStrip(std::span<const vec2> p_Points, f32 p_Thickness = 0.01f) noexcept;

    void Alpha(f32 p_Alpha) noexcept;
    void Alpha(u8 p_Alpha) noexcept;
    void Alpha(u32 p_Alpha) noexcept;

    vec2 GetMouseCoordinates() const noexcept;
};
template <> class ONYX_API RenderContext<D3> final : public IRenderContext<D3>
{
  public:
    using IRenderContext<D3>::IRenderContext;
    using IRenderContext<D3>::Transform;
    using IRenderContext<D3>::TransformAxes;
    using IRenderContext<D3>::Translate;
    using IRenderContext<D3>::Scale;
    using IRenderContext<D3>::TranslateAxes;
    using IRenderContext<D3>::ScaleAxes;
    using IRenderContext<D3>::Line;

    void Transform(const vec3 &p_Translation, const vec3 &p_Scale, const vec3 &p_Rotation) noexcept;
    void Transform(const vec3 &p_Translation, f32 p_Scale, const vec3 &p_Rotation) noexcept;

    void TransformAxes(const vec3 &p_Translation, const vec3 &p_Scale, const vec3 &p_Rotation) noexcept;
    void TransformAxes(const vec3 &p_Translation, f32 p_Scale, const vec3 &p_Rotation) noexcept;

    void Translate(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Scale(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    void TranslateZ(f32 p_Z) noexcept;
    void ScaleZ(f32 p_Z) noexcept;

    void TranslateAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void ScaleAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    void TranslateZAxis(f32 p_Z) noexcept;
    void ScaleZAxis(f32 p_Z) noexcept;

    // Quaternions need to be normalized

    void Rotate(const quat &p_Quaternion) noexcept;
    void Rotate(const vec3 &p_Angles) noexcept;
    void Rotate(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Rotate(f32 p_Angle, const vec3 &p_Axis) noexcept;

    void RotateX(f32 p_X) noexcept;
    void RotateY(f32 p_Y) noexcept;
    void RotateZ(f32 p_Z) noexcept;

    void RotateAxes(const quat &p_Quaternion) noexcept;
    void RotateAxes(const vec3 &p_Angles) noexcept;
    void RotateAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void RotateAxes(f32 p_Angle, const vec3 &p_Axis) noexcept;

    void RotateXAxis(f32 p_X) noexcept;
    void RotateYAxis(f32 p_Y) noexcept;
    void RotateZAxis(f32 p_Z) noexcept;

    void Line(f32 p_StartX, f32 p_StartY, f32 p_StartZ, f32 p_EndX, f32 p_EndY, f32 p_EndZ,
              f32 p_Thickness = 0.01f) noexcept;

    void RoundedLine(const vec3 &p_Start, const vec3 &p_End, f32 p_Thickness = 0.01f) noexcept;
    void RoundedLine(f32 p_StartX, f32 p_StartY, f32 p_StartZ, f32 p_EndX, f32 p_EndY, f32 p_EndZ,
                     f32 p_Thickness = 0.01f) noexcept;

    void RoundedLineStrip(std::span<const vec3> p_Points, f32 p_Thickness = 0.01f) noexcept;

    void Cube() noexcept;
    void Cube(const mat4 &p_Transform) noexcept;

    void Cylinder() noexcept;
    void Cylinder(const mat4 &p_Transform) noexcept;

    void Sphere() noexcept;
    void Sphere(const mat4 &p_Transform) noexcept;

    void Capsule() noexcept;
    void Capsule(const mat4 &p_Transform) noexcept;

    void Capsule(f32 p_Length, f32 p_Radius) noexcept;
    void Capsule(f32 p_Length, f32 p_Radius, const mat4 &p_Transform) noexcept;

    void RoundedCube() noexcept;
    void RoundedCube(const mat4 &p_Transform) noexcept;

    void RoundedCube(const vec3 &p_Dimensions, f32 p_Radius) noexcept;
    void RoundedCube(const vec3 &p_Dimensions, f32 p_Radius, const mat4 &p_Transform) noexcept;

    void RoundedCube(f32 p_Width, f32 p_Height, f32 p_Depth, f32 p_Radius) noexcept;
    void RoundedCube(f32 p_Width, f32 p_Height, f32 p_Depth, f32 p_Radius, const mat4 &p_Transform) noexcept;

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

    void DirectionalLight(ONYX::DirectionalLight p_Light) noexcept;
    void DirectionalLight(const vec3 &p_Direction, f32 p_Intensity = 1.f) noexcept;
    void DirectionalLight(f32 p_DX, f32 p_DY, f32 p_DZ, f32 p_Intensity = 1.f) noexcept;

    void PointLight(const ONYX::PointLight &p_Light) noexcept;
    void PointLight(f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;
    void PointLight(const vec3 &p_Position, f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;
    void PointLight(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;

    void DiffuseContribution(f32 p_Contribution) noexcept;
    void SpecularContribution(f32 p_Contribution) noexcept;
    void SpecularSharpness(f32 p_Sharpness) noexcept;

    void Projection(const mat4 &p_Projection) noexcept;
    void Perspective(f32 p_FieldOfView, f32 p_Near, f32 p_Far) noexcept;
    void Orthographic() noexcept;

    vec3 GetMouseCoordinates(f32 p_Depth) const noexcept;
};
} // namespace ONYX