#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"
#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/rendering/swap_chain.hpp"

#include <vulkan/vulkan.hpp>

// GetMouseCoordinates depends on the Axes!!
// NoFill is an illusion. I could implement Stroke by drawing lines across the edges of the shape, then Implement NoFill
// by using a transparent color

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct RenderState;
class Window;

template <> struct RenderState<2>
{
    mat3 Transform{1.f};
    mat3 Axes{1.f};
    Color FillColor = Color::WHITE;
    Color StrokeColor = Color::WHITE;
    f32 StrokeWidth = 0.f;
    bool NoStroke = true;
    bool NoFill = false;
};

template <> struct RenderState<3>
{
    mat4 Transform{1.f};
    mat4 Axes{1.f};
    Color FillColor = Color::WHITE;
};

ONYX_DIMENSION_TEMPLATE class ONYX_API IRenderContext
{
  public:
    IRenderContext(Window *p_Window) noexcept;

    void Background(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Background(ColorArgs &&...p_ColorArgs) noexcept
    {
        Background(Color(std::forward<ColorArgs>(p_ColorArgs)...));
    }

    void Translate(const vec<N> &p_Translation) noexcept;
    void Scale(const vec<N> &p_Scale) noexcept;
    void Scale(f32 p_Scale) noexcept;

    void KeepWindowAspect() noexcept;
    void TranslateAxes(const vec<N> &p_Translation) noexcept;
    void ScaleAxes(const vec<N> &p_Scale) noexcept;
    void ScaleAxes(f32 p_Scale) noexcept;

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

    void Circle() noexcept;
    void Circle(f32 p_Radius) noexcept;
    void Circle(const vec<N> &p_Position) noexcept;
    void Circle(const vec<N> &p_Position, f32 p_Radius) noexcept;

    void Ellipse(f32 p_XDim, f32 p_YDim) noexcept;
    void Ellipse(const mat<N> &p_Transform) noexcept;
    void Ellipse(const vec2 &p_Dimensions) noexcept;
    void Ellipse(const vec<N> &p_Position, const vec2 &p_Dimensions) noexcept;
    void Ellipse(const vec<N> &p_Position, const vec2 &p_Dimensions, const rot<N> &p_Rotation) noexcept;

    void Line(const vec<N> &p_Start, const vec<N> &p_End, f32 p_Thickness = 0.1f) noexcept;
    void LineStrip(std::span<const vec<N>> p_Points, f32 p_Thickness = 0.1f) noexcept;

    void RoundedLine(const vec<N> &p_Start, const vec<N> &p_End, f32 p_Thickness = 0.1f) noexcept;
    void RoundedLineStrip(std::span<const vec<N>> p_Points, f32 p_Thickness = 0.1f) noexcept;

    void Mesh(const Model *p_Model, const mat<N> &p_Transform) noexcept;
    void Mesh(const Model *p_Model, const vec<N> &p_Translation) noexcept;
    void Mesh(const Model *p_Model, const vec<N> &p_Translation, const vec<N> &p_Scale) noexcept;
    void Mesh(const Model *p_Model, const vec<N> &p_Translation, const vec<N> &p_Scale,
              const rot<N> &p_Rotation) noexcept;
    void Mesh(const Model *p_Model) noexcept;

    void Push() noexcept;
    void PushAndClear() noexcept;

    void Pop() noexcept;

    void Fill(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Fill(ColorArgs &&...p_ColorArgs) noexcept
    {
        Fill(Color(std::forward<ColorArgs>(p_ColorArgs)...));
    }

    const mat<N> &GetCurrentTransform() const noexcept;
    const mat<N> &GetCurrentAxes() const noexcept;

    mat<N> &GetCurrentTransform() noexcept;
    mat<N> &GetCurrentAxes() noexcept;

    void SetCurrentTransform(const mat<N> &p_Transform) noexcept;
    void SetCurrentAxes(const mat<N> &p_Axes) noexcept;

  protected:
    // this method MUST be called externally (ie by a derived class). it wont be called automatically
    void initializeRenderers(VkRenderPass p_RenderPass, VkDescriptorSetLayout p_Layout) noexcept;
    void resetRenderState() noexcept;
    void circleMesh(const mat<N> &p_Transform) noexcept;

    KIT::Storage<MeshRenderer<N>> m_MeshRenderer;
    KIT::Storage<CircleRenderer<N>> m_CircleRenderer;

    DynamicArray<RenderState<N>> m_RenderState;
    Window *m_Window;
};

ONYX_DIMENSION_TEMPLATE class RenderContext;

template <> class ONYX_API RenderContext<2> final : public IRenderContext<2>
{
  public:
    RenderContext(Window *p_Window, VkRenderPass p_RenderPass) noexcept;

    using IRenderContext<2>::Translate;
    using IRenderContext<2>::Scale;
    using IRenderContext<2>::TranslateAxes;
    using IRenderContext<2>::ScaleAxes;
    using IRenderContext<2>::Square;
    using IRenderContext<2>::Rect;
    using IRenderContext<2>::NGon;
    using IRenderContext<2>::Circle;
    using IRenderContext<2>::Ellipse;
    using IRenderContext<2>::Line;
    using IRenderContext<2>::RoundedLine;
    using IRenderContext<2>::Fill;

    void Translate(f32 p_X, f32 p_Y) noexcept;
    void Scale(f32 p_X, f32 p_Y) noexcept;
    void Rotate(f32 p_Angle) noexcept;

    void TranslateAxes(f32 p_X, f32 p_Y) noexcept;
    void ScaleAxes(f32 p_X, f32 p_Y) noexcept;
    void RotateAxes(f32 p_Angle) noexcept;

    void Square(f32 p_X, f32 p_Y) noexcept;
    void Square(f32 p_X, f32 p_Y, f32 p_Size) noexcept;
    void Square(f32 p_X, f32 p_Y, f32 p_Size, f32 p_Rotation) noexcept;

    void Rect(f32 p_X, f32 p_Y, f32 p_XDim, f32 p_YDim) noexcept;
    void Rect(f32 p_X, f32 p_Y, f32 p_XDim, f32 p_YDim, f32 p_Rotation) noexcept;

    void NGon(u32 p_Sides, f32 p_X, f32 p_Y) noexcept;
    void NGon(u32 p_Sides, f32 p_X, f32 p_Y, f32 p_Radius) noexcept;
    void NGon(u32 p_Sides, f32 p_X, f32 p_Y, f32 p_Radius, f32 p_Rotation) noexcept;

    void Circle(f32 p_X, f32 p_Y) noexcept;
    void Circle(f32 p_X, f32 p_Y, f32 p_Radius) noexcept;

    void Ellipse(f32 p_X, f32 p_Y, f32 p_XDim, f32 p_YDim) noexcept;
    void Ellipse(f32 p_X, f32 p_Y, f32 p_XDim, f32 p_YDim, f32 p_Rotation) noexcept;

    void Line(f32 p_StartX, f32 p_StartY, f32 p_EndX, f32 p_EndY, f32 p_Thickness = 0.1f) noexcept;
    void RoundedLine(f32 p_StartX, f32 p_StartY, f32 p_EndX, f32 p_EndY, f32 p_Thickness = 0.1f) noexcept;

    void Fill() noexcept;
    void NoFill() noexcept;

    void Stroke() noexcept;
    void Stroke(const Color &p_Color) noexcept;

    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Stroke(ColorArgs &&...p_ColorArgs) noexcept
    {
        Stroke(Color(std::forward<ColorArgs>(p_ColorArgs)...));
    }

    void StrokeWidth(f32 p_Width) noexcept;
    void NoStroke() noexcept;

    vec2 GetMouseCoordinates() const noexcept;

    void Render(VkCommandBuffer p_CommandBuffer) noexcept;
};
template <> class ONYX_API RenderContext<3> final : public IRenderContext<3>
{
  public:
    RenderContext(Window *p_Window, VkRenderPass p_RenderPass) noexcept;
    ~RenderContext() noexcept;

    using IRenderContext<3>::Translate;
    using IRenderContext<3>::Scale;
    using IRenderContext<3>::TranslateAxes;
    using IRenderContext<3>::ScaleAxes;
    using IRenderContext<3>::Square;
    using IRenderContext<3>::Rect;
    using IRenderContext<3>::NGon;
    using IRenderContext<3>::Circle;
    using IRenderContext<3>::Ellipse;
    using IRenderContext<3>::Line;
    using IRenderContext<3>::RoundedLine;

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

    void RotateAxesX(f32 p_X) noexcept;
    void RotateAxesY(f32 p_Y) noexcept;
    void RotateAxesZ(f32 p_Z) noexcept;

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

    void Circle(f32 p_X, f32 p_Y, f32 p_Z) noexcept;
    void Circle(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius) noexcept;

    void Ellipse(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim) noexcept;
    void Ellipse(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, const quat &p_Quaternion) noexcept;
    void Ellipse(f32 p_X, f32 p_Y, f32 p_Z, f32 p_XDim, f32 p_YDim, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;
    void Ellipse(const vec3 &p_Position, const vec2 &p_Dimensions, const vec3 &p_Angles) noexcept;

    void Line(f32 p_StartX, f32 p_StartY, f32 p_StartZ, f32 p_EndX, f32 p_EndY, f32 p_EndZ,
              f32 p_Thickness = 0.1f) noexcept;
    void RoundedLine(f32 p_StartX, f32 p_StartY, f32 p_StartZ, f32 p_EndX, f32 p_EndY, f32 p_EndZ,
                     f32 p_Thickness = 0.1f) noexcept;

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
    void Cuboid(f32 p_X, f32 p_Y, f32 p_Z, const vec3 &p_Dimensions, const quat &p_Quaternion) noexcept;
    void Cuboid(f32 p_X, f32 p_Y, f32 p_Z, const vec3 &p_Dimensions, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;

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
    void Ellipsoid(f32 p_X, f32 p_Y, f32 p_Z, const vec3 &p_Dimensions, const quat &p_Quaternion) noexcept;
    void Ellipsoid(f32 p_X, f32 p_Y, f32 p_Z, const vec3 &p_Dimensions, f32 p_XRot, f32 p_YRot, f32 p_ZRot) noexcept;

    void SetPerspectiveProjection(f32 p_FieldOfAxes, f32 p_Aspect, f32 p_Near, f32 p_Far) noexcept;
    void SetOrthographicProjection() noexcept;

    vec3 GetMouseCoordinates(f32 p_Depth) const noexcept;

    void Render(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer) noexcept;

    // TODO: Handle this in a better way
    vec3 LightDirection{0.f, -1.f, 0.f};
    f32 LightIntensity = 0.9f;
    f32 AmbientIntensity = 0.1f;

  private:
    struct GlobalUniformHelper
    {
        GlobalUniformHelper(const DescriptorPool::Specs &p_PoolSpecs,
                            const std::span<const VkDescriptorSetLayoutBinding> p_Bindings,
                            const Buffer::Specs &p_BufferSpecs) noexcept
            : Pool(p_PoolSpecs), Layout(p_Bindings), UniformBuffer(p_BufferSpecs)
        {
        }
        DescriptorPool Pool;
        DescriptorSetLayout Layout;
        Buffer UniformBuffer;
        std::array<VkDescriptorSet, SwapChain::MFIF> DescriptorSets;
    };

    void createGlobalUniformHelper() noexcept;

    KIT::Storage<GlobalUniformHelper> m_GlobalUniformHelper;
    KIT::Ref<Device> m_Device;
    mat4 m_Projection{1.f};
};

using RenderContext2D = RenderContext<2>;
using RenderContext3D = RenderContext<3>;
} // namespace ONYX