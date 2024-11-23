#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/transform.hpp"
#include <vulkan/vulkan.hpp>

// Translate() Rotate() etc is only valid for primitives, not lights

// Explain why InstanceData3D has view data for each instance (it's because of the immediate mode)

// Ambient intensity must be called AFTER ambient color

// 2D objects that are drawn later will always be on top of earlier ones. HOWEVER, blending will only work expectedly
// between objects of the same primitive

// Because of batch rendering, draw order is not guaranteed

namespace ONYX
{
class Window;
/**
 * @brief The RenderContext class is the primary way of communicating with the ONYX API.
 *
 * It is a high-level API that allows the user to draw shapes and meshes in a simple immediate mode
 * fashion.
 *
 * @note Operations such as Transform(), Translate(), Rotate(), etc. will not affect the lights in the scene.
 *
 */
template <Dimension D> class ONYX_API IRenderContext
{
    KIT_NON_COPYABLE(IRenderContext)
  public:
    IRenderContext(Window *p_Window, VkRenderPass p_RenderPass) noexcept;

    /**
     * @brief Clear all of the recorded draw data until this point.
     *
     * It is meant to be called at the beginning of the frame, but it is not required to do so in case you want to
     * persist ONYX draw calls made in older frames.
     *
     * It calls all of the renderer's Flush() methods.
     *
     */
    void FlushDrawData() noexcept;

    /**
     * @brief Reset the state of the RenderContext to its default values.
     *
     * This method is meant to be called at the beginning of the frame, but it is not required to do so in case you want
     * to persist the state of the RenderContext from older frames.
     *
     */
    void FlushState() noexcept;

    /**
     * @brief Reset the state of the RenderContext to its default values and sets the background color.
     *
     * @param p_Color The background color to set.
     */
    void FlushState(const Color &p_Color) noexcept;

    /**
     * @brief Reset the state of the RenderContext to its default values and sets the background color.
     *
     * @param p_ColorArgs Arguments to construct the background color.
     */
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void FlushState(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        FlushState(color);
    }

    /**
     * @brief Call both FlushDrawData() and FlushState().
     *
     */
    void Flush() noexcept;

    /**
     * @brief Call both FlushDrawData() and FlushState(), and sets the background color.
     *
     * @param p_Color The background color to set.
     */
    void Flush(const Color &p_Color) noexcept;

    /**
     * @brief Call both FlushDrawData() and FlushState(), and sets the background color.
     *
     * @param p_ColorArgs Arguments to construct the background color.
     */
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Flush(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Flush(color);
    }

    /**
     * @brief Transform subsequent shapes by the given transformation matrix.
     *
     * It corresponds to an extrinsic transformation directly applied to the shape.
     *
     * @param p_Transform The transformation matrix.
     */
    void Transform(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Transform subsequent shapes by the given translation, scale, and rotation.
     *
     * It corresponds to an extrinsic transformation directly applied to the shape.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scale vector.
     * @param p_Rotation The rotation object.
     */
    void Transform(const vec<D> &p_Translation, const vec<D> &p_Scale, const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Transform subsequent shapes by the given translation, uniform scale, and rotation.
     *
     * It corresponds to an extrinsic transformation directly applied to the shape.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scale value.
     * @param p_Rotation The rotation object.
     */
    void Transform(const vec<D> &p_Translation, f32 p_Scale, const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Transform subsequent shapes' coordinate system (axes) by the given transformation matrix.
     *
     * It corresponds to an intrinsic transformation applied to the coordinate system, which is then applied to all
     * subsequent shapes extrinsically.
     *
     * @param p_Transform The transformation matrix.
     */
    void TransformAxes(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Transform subsequent shapes' coordinate system (axes) by the given translation, scale, and rotation.
     *
     * It corresponds to an intrinsic transformation applied to the coordinate system, which is then applied to all
     * subsequent shapes extrinsically.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scale vector.
     * @param p_Rotation The rotation object.
     */
    void TransformAxes(const vec<D> &p_Translation, const vec<D> &p_Scale, const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Transform subsequent shapes' coordinate system (axes) by the given translation, uniform scale, and
     * rotation.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scale value.
     * @param p_Rotation The rotation object.
     */
    void TransformAxes(const vec<D> &p_Translation, f32 p_Scale, const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Translate subsequent shapes by the given vector.
     *
     * This applies a translation transformation to all subsequent draw calls.
     *
     * @param p_Translation The translation vector.
     */
    void Translate(const vec<D> &p_Translation) noexcept;

    /**
     * @brief Scale subsequent shapes by the given vector.
     *
     * This applies a scaling transformation to all subsequent draw calls.
     *
     * @param p_Scale The scaling vector.
     */
    void Scale(const vec<D> &p_Scale) noexcept;

    /**
     * @brief Scale subsequent shapes uniformly by the given factor.
     *
     * This applies a uniform scaling transformation to all subsequent draw calls.
     *
     * @param p_Scale The scaling factor.
     */
    void Scale(f32 p_Scale) noexcept;

    /**
     * @brief Translate subsequent shapes along the X-axis.
     *
     * Applies a translation along the X-axis to all subsequent draw calls.
     *
     * @param p_X The translation distance along the X-axis.
     */
    void TranslateX(f32 p_X) noexcept;

    /**
     * @brief Translate subsequent shapes along the Y-axis.
     *
     * Applies a translation along the Y-axis to all subsequent draw calls.
     *
     * @param p_Y The translation distance along the Y-axis.
     */
    void TranslateY(f32 p_Y) noexcept;

    /**
     * @brief Scale subsequent shapes along the X-axis.
     *
     * Applies a scaling transformation along the X-axis to all subsequent draw calls.
     *
     * @param p_X The scaling factor along the X-axis.
     */
    void ScaleX(f32 p_X) noexcept;

    /**
     * @brief Scale subsequent shapes along the Y-axis.
     *
     * Applies a scaling transformation along the Y-axis to all subsequent draw calls.
     *
     * @param p_Y The scaling factor along the Y-axis.
     */
    void ScaleY(f32 p_Y) noexcept;

    /**
     * @brief Translate the coordinate system (axes) by the given vector.
     *
     * This applies an intrinsic translation to the coordinate system, affecting all subsequent draw calls.
     *
     * @param p_Translation The translation vector.
     */
    void TranslateAxes(const vec<D> &p_Translation) noexcept;

    /**
     * @brief Scale the coordinate system (axes) by the given vector.
     *
     * This applies an intrinsic scaling to the coordinate system, affecting all subsequent draw calls.
     *
     * @param p_Scale The scaling vector.
     */
    void ScaleAxes(const vec<D> &p_Scale) noexcept;

    /**
     * @brief Scale the coordinate system (axes) uniformly by the given factor.
     *
     * @param p_Scale The uniform scaling factor.
     */
    void ScaleAxes(f32 p_Scale) noexcept;

    /**
     * @brief Translate the coordinate system along the X-axis.
     *
     * Applies an intrinsic translation along the X-axis to the coordinate system.
     *
     * @param p_X The translation distance along the X-axis.
     */
    void TranslateXAxis(f32 p_X) noexcept;

    /**
     * @brief Translate the coordinate system along the Y-axis.
     *
     * Applies an intrinsic translation along the Y-axis to the coordinate system.
     *
     * @param p_Y The translation distance along the Y-axis.
     */
    void TranslateYAxis(f32 p_Y) noexcept;

    /**
     * @brief Scale the coordinate system along the X-axis.
     *
     * Applies an intrinsic scaling along the X-axis to the coordinate system.
     *
     * @param p_X The scaling factor along the X-axis.
     */
    void ScaleXAxis(f32 p_X) noexcept;

    /**
     * @brief Scale the coordinate system along the Y-axis.
     *
     * Applies an intrinsic scaling along the Y-axis to the coordinate system.
     *
     * @param p_Y The scaling factor along the Y-axis.
     */
    void ScaleYAxis(f32 p_Y) noexcept;

    /**
     * @brief Update the view aspect ratio.
     *
     * @param p_Aspect The new aspect ratio.
     */
    void UpdateViewAspect(f32 p_Aspect) noexcept;

    /**
     * @brief Render the coordinate axes for visualization.
     *
     * Draw the X and Y axes (and Z axis if D == 3) with the specified thickness and size.
     *
     * @param p_Thickness The thickness of the axes lines.
     * @param p_Size The length of the axes.
     */
    void Axes(f32 p_Thickness = 0.01f, f32 p_Size = 50.f) noexcept;

    /**
     * @brief Draw a unit triangle centered at the origin.
     *
     * The triangle will be affected by the current transformation state.
     */
    void Triangle() noexcept;

    /**
     * @brief Draw a triangle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the triangle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Triangle(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a unit square centered at the origin.
     *
     * The square will be affected by the current transformation state.
     */
    void Square() noexcept;

    /**
     * @brief Draw a square with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Square(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a regular n-sided polygon centered at the origin.
     *
     * @param p_Sides The number of sides of the polygon.
     */
    void NGon(u32 p_Sides) noexcept;

    /**
     * @brief Draw a regular n-sided polygon with the specified transformation.
     *
     * @param p_Sides The number of sides of the polygon.
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void NGon(u32 p_Sides, const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a polygon defined by the given vertices.
     *
     * @param p_Vertices A span of vertices defining the polygon. Vertices are expected to be centered around zero.
     */
    void Polygon(std::span<const vec<D>> p_Vertices) noexcept;

    /**
     * @brief Draw a polygon defined by the given vertices with the specified transformation.
     *
     * @param p_Vertices A span of vertices defining the polygon.
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Polygon(std::span<const vec<D>> p_Vertices, const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a unit circle centered at the origin.
     *
     * The circle will be affected by the current transformation state.
     */
    void Circle() noexcept;

    /**
     * @brief Draw a circle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Circle(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a circular arc or annulus sector.
     *
     * @param p_LowerAngle The starting angle in radians.
     * @param p_UpperAngle The ending angle in radians.
     * @param p_Hollowness The inner radius of the circle (for hollow circles), default is 0 (solid).
     */
    void Circle(f32 p_LowerAngle, f32 p_UpperAngle, f32 p_Hollowness = 0.f) noexcept;

    /**
     * @brief Draw a circular arc with the specified transformation matrix.
     *
     * @param p_LowerAngle The starting angle in radians.
     * @param p_UpperAngle The ending angle in radians.
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Circle(f32 p_LowerAngle, f32 p_UpperAngle, const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a circular arc or annulus sector with the specified transformation matrix.
     *
     * @param p_LowerAngle The starting angle in radians.
     * @param p_UpperAngle The ending angle in radians.
     * @param p_Hollowness The inner radius of the circle.
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Circle(f32 p_LowerAngle, f32 p_UpperAngle, f32 p_Hollowness, const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a unit stadium shape (a rectangle with semicircular ends).
     *
     * The stadium will be affected by the current transformation state.
     */
    void Stadium() noexcept;

    /**
     * @brief Draw a stadium with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the stadium. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Stadium(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a stadium shape with the given length and radius.
     *
     * @param p_Length The length of the rectangular part of the stadium.
     * @param p_Radius The radius of the semicircular ends.
     */
    void Stadium(f32 p_Length, f32 p_Radius) noexcept;

    /**
     * @brief Draw a stadium shape with the given parameters and transformation.
     *
     * @param p_Length The length of the rectangular part of the stadium.
     * @param p_Radius The radius of the semicircular ends.
     * @param p_Transform The transformation matrix to apply to the stadium. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Stadium(f32 p_Length, f32 p_Radius, const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a unit rounded square centered at the origin.
     *
     * The rounded square will be affected by the current transformation state.
     */
    void RoundedSquare() noexcept;

    /**
     * @brief Draw a rounded square with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the rounded square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void RoundedSquare(const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a rounded square with given dimensions and corner radius.
     *
     * @param p_Dimensions The width and height of the square.
     * @param p_Radius The radius of the corners.
     */
    void RoundedSquare(const vec2 &p_Dimensions, f32 p_Radius) noexcept;

    /**
     * @brief Draw a rounded square with given dimensions, corner radius, and transformation.
     *
     * @param p_Dimensions The width and height of the square.
     * @param p_Radius The radius of the corners.
     * @param p_Transform The transformation matrix to apply to the rounded square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void RoundedSquare(const vec2 &p_Dimensions, f32 p_Radius, const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a rounded square with given dimensions and corner radius.
     *
     * @param p_Width The width of the square.
     * @param p_Height The height of the square.
     * @param p_Radius The radius of the corners.
     */
    void RoundedSquare(f32 p_Width, f32 p_Height, f32 p_Radius) noexcept;

    /**
     * @brief Draw a rounded square with given dimensions, corner radius, and transformation.
     *
     * @param p_Width The width of the square.
     * @param p_Height The height of the square.
     * @param p_Radius The radius of the corners.
     * @param p_Transform The transformation matrix to apply to the rounded square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void RoundedSquare(f32 p_Width, f32 p_Height, f32 p_Radius, const mat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void Line(const vec<D> &p_Start, const vec<D> &p_End, f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a line strip through the given points.
     *
     * @param p_Points A span of points defining the line strip.
     * @param p_Thickness The thickness of the line.
     */
    void LineStrip(std::span<const vec<D>> p_Points, f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a mesh model.
     *
     * @param p_Model A reference to the mesh model to draw.
     */
    void Mesh(const KIT::Ref<const Model<D>> &p_Model) noexcept;

    /**
     * @brief Draw a mesh model with the specified transformation.
     *
     * @param p_Model A reference to the mesh model to draw.
     * @param p_Transform The transformation matrix to apply to the model. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Mesh(const KIT::Ref<const Model<D>> &p_Model, const mat<D> &p_Transform) noexcept;

    /**
     * @brief Pushes the current transformation state onto the stack.
     *
     * Allows you to save the current state and restore it later with Pop().
     */
    void Push() noexcept;

    /**
     * @brief Pushes the current state onto the stack and resets the transformation state.
     *
     * Useful for temporarily changing the transformation state without affecting the saved state.
     */
    void PushAndClear() noexcept;

    /**
     * @brief Pops the last saved transformation state from the stack.
     *
     * Restores the transformation state to the last state saved with Push().
     */
    void Pop() noexcept;

    /**
     * @brief Enables or disables filling of subsequent shapes.
     *
     * @param p_Enable Set to true to enable filling, false to disable.
     */
    void Fill(bool p_Enable = true) noexcept;

    /**
     * @brief Set the fill color for subsequent shapes.
     *
     * Enables filling if it was previously disabled.
     *
     * @param p_Color The color to use for filling.
     */
    void Fill(const Color &p_Color) noexcept;

    /**
     * @brief Set the fill color for subsequent shapes.
     *
     * @param p_ColorArgs Arguments to construct the fill color.
     */
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Fill(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Fill(color);
    }

    /**
     * @brief Enables or disables outlining of subsequent shapes.
     *
     * @param p_Enable Set to true to enable outlining, false to disable.
     */
    void Outline(bool p_Enable = true) noexcept;

    /**
     * @brief Set the outline color for subsequent shapes.
     *
     * Enables outlining if it was previously disabled.
     *
     * @param p_Color The color to use for outlining.
     */
    void Outline(const Color &p_Color) noexcept;

    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Outline(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Outline(color);
    }

    /**
     * @brief Set the outline width for subsequent shapes.
     *
     * @param p_Width The width of the outline.
     */
    void OutlineWidth(f32 p_Width) noexcept;

    /**
     * @brief Set the material properties for subsequent shapes.
     *
     * @param p_Material The material data to use.
     */
    void Material(const MaterialData<D> &p_Material) noexcept;

    /**
     * @brief Control the global view's movement of the rendering context with user input.
     *
     * @param p_TranslationStep The step size for translation.
     * @param p_RotationStep The step size for rotation.
     */
    void ApplyCameraLikeMovementControls(const f32 p_TranslationStep, const f32 p_RotationStep) noexcept;

    /**
     * @brief Retrieve the coordinates of a point in the rendering context from a normalized position.
     *
     * @param p_NormalizedPos The normalized position to convert. Should be in the range [-1, 1]. If in 3D, the Z
     * axis must be between [0, 1].
     * @return The coordinates of the point in the rendering context.
     */
    vec<D> GetCoordinates(const vec<D> &p_NormalizedPos) const noexcept;

    /**
     * @brief Get the current rendering state.
     *
     * @return A constant reference to the current RenderState.
     */
    const RenderState<D> &GetCurrentState() const noexcept;

    /**
     * @brief Get the current rendering state.
     *
     * @return A reference to the current RenderState.
     */
    RenderState<D> &GetCurrentState() noexcept;

    /**
     * @brief Get the global context's projection view data.
     *
     * @return A constant reference to the global context's projection view data.
     */
    const ProjectionViewData<D> &GetProjectionViewData() const noexcept;

    /**
     * @brief Set the global context's view.
     *
     */
    void SetView(const ONYX::Transform<D> &p_View) noexcept;

    /**
     * @brief Render the recorded draw data using the provided command buffer.
     *
     * @param p_CommandBuffer The Vulkan command buffer to use for rendering.
     */
    void Render(VkCommandBuffer p_CommandBuffer) noexcept;

  protected:
    DynamicArray<RenderState<D>> m_RenderState;
    ProjectionViewData<D> m_ProjectionView;
    Renderer<D> m_Renderer;
    Window *m_Window;
};

template <Dimension D> class RenderContext;

/**
 * @brief The RenderContext class handles all primitive ONYX draw calls and allows the user to interact with most of the
 * ONYX API in 2D.
 *
 */
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

    /**
     * @brief Translate subsequent shapes by the given amounts along the X and Y axes.
     *
     * @param p_X The translation along the X-axis.
     * @param p_Y The translation along the Y-axis.
     */
    void Translate(f32 p_X, f32 p_Y) noexcept;

    /**
     * @brief Scale subsequent shapes by the given factors along the X and Y axes.
     *
     * @param p_X The scaling factor along the X-axis.
     * @param p_Y The scaling factor along the Y-axis.
     */
    void Scale(f32 p_X, f32 p_Y) noexcept;

    /**
     * @brief Rotates subsequent shapes by the given angle.
     *
     * @param p_Angle The rotation angle in radians.
     */
    void Rotate(f32 p_Angle) noexcept;

    /**
     * @brief Translate the coordinate system by the given amounts along the X and Y axes.
     *
     * @param p_X The translation along the X-axis.
     * @param p_Y The translation along the Y-axis.
     */
    void TranslateAxes(f32 p_X, f32 p_Y) noexcept;

    /**
     * @brief Scale the coordinate system by the given factors along the X and Y axes.
     *
     * @param p_X The scaling factor along the X-axis.
     * @param p_Y The scaling factor along the Y-axis.
     */
    void ScaleAxes(f32 p_X, f32 p_Y) noexcept;

    /**
     * @brief Rotates the coordinate system by the given angle.
     *
     * @param p_Angle The rotation angle in radians.
     */
    void RotateAxes(f32 p_Angle) noexcept;

    /**
     * @brief Draw a line between two points with the specified thickness.
     *
     * @param p_StartX The X-coordinate of the starting point.
     * @param p_StartY The Y-coordinate of the starting point.
     * @param p_EndX The X-coordinate of the ending point.
     * @param p_EndY The Y-coordinate of the ending point.
     * @param p_Thickness The thickness of the line.
     */
    void Line(f32 p_StartX, f32 p_StartY, f32 p_EndX, f32 p_EndY, f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a rounded line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void RoundedLine(const vec2 &p_Start, const vec2 &p_End, f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a rounded line between two points with the specified thickness.
     *
     * @param p_StartX The X-coordinate of the starting point.
     * @param p_StartY The Y-coordinate of the starting point.
     * @param p_EndX The X-coordinate of the ending point.
     * @param p_EndY The Y-coordinate of the ending point.
     * @param p_Thickness The thickness of the line.
     */
    void RoundedLine(f32 p_StartX, f32 p_StartY, f32 p_EndX, f32 p_EndY, f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Set the alpha (transparency) value for subsequent shapes.
     *
     * @param p_Alpha The alpha value between 0.0 (fully transparent) and 1.0 (fully opaque).
     */
    void Alpha(f32 p_Alpha) noexcept;

    /**
     * @brief Set the alpha (transparency) value for subsequent shapes.
     *
     * @param p_Alpha The alpha value between 0 and 255.
     */
    void Alpha(u8 p_Alpha) noexcept;

    /**
     * @brief Set the alpha (transparency) value for subsequent shapes.
     *
     * @param p_Alpha The alpha value between 0 and 255.
     */
    void Alpha(u32 p_Alpha) noexcept;

    /**
     * @brief Retrieve the current mouse coordinates in the rendering context.
     *
     * @return The mouse coordinates as a 2D vector.
     */
    vec2 GetMouseCoordinates() const noexcept;

    /**
     * @brief Control the global view's scale of the rendering context with user input.
     *
     * @param p_ScaleStep The step size for scaling.
     */
    void ApplyCameraLikeScalingControls(const f32 p_ScaleStep) noexcept;
};

/**
 * @brief The RenderContext class handles all primitive ONYX draw calls and allows the user to interact with most of the
 * ONYX API in 3D.
 *
 */
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

    /**
     * @brief Transforms subsequent shapes by the given translation, scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scaling vector.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void Transform(const vec3 &p_Translation, const vec3 &p_Scale, const vec3 &p_Rotation) noexcept;

    /**
     * @brief Transforms subsequent shapes by the given translation, uniform scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scaling factor.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void Transform(const vec3 &p_Translation, f32 p_Scale, const vec3 &p_Rotation) noexcept;

    /**
     * @brief Transforms the coordinate system by the given translation, scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scaling vector.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void TransformAxes(const vec3 &p_Translation, const vec3 &p_Scale, const vec3 &p_Rotation) noexcept;

    /**
     * @brief Transforms the coordinate system by the given translation, uniform scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scaling factor.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void TransformAxes(const vec3 &p_Translation, f32 p_Scale, const vec3 &p_Rotation) noexcept;

    /**
     * @brief Translate subsequent shapes by the given amounts along the X, Y, and Z axes.
     *
     * @param p_X The translation along the X-axis.
     * @param p_Y The translation along the Y-axis.
     * @param p_Z The translation along the Z-axis.
     */
    void Translate(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    /**
     * @brief Scale subsequent shapes by the given factors along the X, Y, and Z axes.
     *
     * @param p_X The scaling factor along the X-axis.
     * @param p_Y The scaling factor along the Y-axis.
     * @param p_Z The scaling factor along the Z-axis.
     */
    void Scale(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    /**
     * @brief Translate subsequent shapes along the Z-axis.
     *
     * @param p_Z The translation distance along the Z-axis.
     */
    void TranslateZ(f32 p_Z) noexcept;

    /**
     * @brief Scale subsequent shapes along the Z-axis.
     *
     * @param p_Z The scaling factor along the Z-axis.
     */
    void ScaleZ(f32 p_Z) noexcept;

    /**
     * @brief Translate the coordinate system by the given amounts along the X, Y, and Z axes.
     *
     * @param p_X The translation along the X-axis.
     * @param p_Y The translation along the Y-axis.
     * @param p_Z The translation along the Z-axis.
     */
    void TranslateAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    /**
     * @brief Scale the coordinate system by the given factors along the X, Y, and Z axes.
     *
     * @param p_X The scaling factor along the X-axis.
     * @param p_Y The scaling factor along the Y-axis.
     * @param p_Z The scaling factor along the Z-axis.
     */
    void ScaleAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    /**
     * @brief Translate the coordinate system along the Z-axis.
     *
     * @param p_Z The translation distance along the Z-axis.
     */
    void TranslateZAxis(f32 p_Z) noexcept;

    /**
     * @brief Scale the coordinate system along the Z-axis.
     *
     * @param p_Z The scaling factor along the Z-axis.
     */
    void ScaleZAxis(f32 p_Z) noexcept;

    /**
     * @brief Rotates subsequent shapes by the given quaternion.
     *
     * @param p_Quaternion The quaternion representing the rotation.
     */
    void Rotate(const quat &p_Quaternion) noexcept;

    /**
     * @brief Rotates subsequent shapes by the given Euler angles.
     *
     * @param p_Angles The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void Rotate(const vec3 &p_Angles) noexcept;

    /**
     * @brief Rotates subsequent shapes by the given angles around each axis.
     *
     * @param p_X The rotation angle around the X-axis.
     * @param p_Y The rotation angle around the Y-axis.
     * @param p_Z The rotation angle around the Z-axis.
     */
    void Rotate(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    /**
     * @brief Rotates subsequent shapes by the given angle around the specified axis.
     *
     * @param p_Angle The rotation angle in radians.
     * @param p_Axis The axis to rotate around.
     */
    void Rotate(f32 p_Angle, const vec3 &p_Axis) noexcept;

    /**
     * @brief Rotates subsequent shapes around the X-axis.
     *
     * @param p_X The rotation angle around the X-axis in radians.
     */
    void RotateX(f32 p_X) noexcept;

    /**
     * @brief Rotates subsequent shapes around the Y-axis.
     *
     * @param p_Y The rotation angle around the Y-axis in radians.
     */
    void RotateY(f32 p_Y) noexcept;

    /**
     * @brief Rotates subsequent shapes around the Z-axis.
     *
     * @param p_Z The rotation angle around the Z-axis in radians.
     */
    void RotateZ(f32 p_Z) noexcept;

    /**
     * @brief Rotates the coordinate system by the given quaternion.
     *
     * @param p_Quaternion The quaternion representing the rotation.
     */
    void RotateAxes(const quat &p_Quaternion) noexcept;

    /**
     * @brief Rotates the coordinate system by the given Euler angles.
     *
     * @param p_Angles The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void RotateAxes(const vec3 &p_Angles) noexcept;

    /**
     * @brief Rotates the coordinate system by the given angles around each axis.
     *
     * @param p_X The rotation angle around the X-axis.
     * @param p_Y The rotation angle around the Y-axis.
     * @param p_Z The rotation angle around the Z-axis.
     */
    void RotateAxes(f32 p_X, f32 p_Y, f32 p_Z) noexcept;

    /**
     * @brief Rotates the coordinate system by the given angle around the specified axis.
     *
     * @param p_Angle The rotation angle in radians.
     * @param p_Axis The axis to rotate around.
     */
    void RotateAxes(f32 p_Angle, const vec3 &p_Axis) noexcept;

    /**
     * @brief Rotates the coordinate system around the X-axis.
     *
     * @param p_X The rotation angle around the X-axis in radians.
     */
    void RotateXAxis(f32 p_X) noexcept;

    /**
     * @brief Rotates the coordinate system around the Y-axis.
     *
     * @param p_Y The rotation angle around the Y-axis in radians.
     */
    void RotateYAxis(f32 p_Y) noexcept;

    /**
     * @brief Rotates the coordinate system around the Z-axis.
     *
     * @param p_Z The rotation angle around the Z-axis in radians.
     */
    void RotateZAxis(f32 p_Z) noexcept;

    /**
     * @brief Draw a line between two points with the specified thickness.
     *
     * @param p_StartX The X-coordinate of the starting point.
     * @param p_StartY The Y-coordinate of the starting point.
     * @param p_StartZ The Z-coordinate of the starting point.
     * @param p_EndX The X-coordinate of the ending point.
     * @param p_EndY The Y-coordinate of the ending point.
     * @param p_EndZ The Z-coordinate of the ending point.
     * @param p_Thickness The thickness of the line.
     */
    void Line(f32 p_StartX, f32 p_StartY, f32 p_StartZ, f32 p_EndX, f32 p_EndY, f32 p_EndZ,
              f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a rounded line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void RoundedLine(const vec3 &p_Start, const vec3 &p_End, f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a rounded line between two points with the specified thickness.
     *
     * @param p_StartX The X-coordinate of the starting point.
     * @param p_StartY The Y-coordinate of the starting point.
     * @param p_StartZ The Z-coordinate of the starting point.
     * @param p_EndX The X-coordinate of the ending point.
     * @param p_EndY The Y-coordinate of the ending point.
     * @param p_EndZ The Z-coordinate of the ending point.
     * @param p_Thickness The thickness of the line.
     */
    void RoundedLine(f32 p_StartX, f32 p_StartY, f32 p_StartZ, f32 p_EndX, f32 p_EndY, f32 p_EndZ,
                     f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a unit cube centered at the origin.
     *
     * The cube will be affected by the current transformation state.
     */
    void Cube() noexcept;

    /**
     * @brief Draw a cube with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Cube(const mat4 &p_Transform) noexcept;

    /**
     * @brief Draw a unit cylinder centered at the origin.
     *
     * The cylinder will be affected by the current transformation state.
     */
    void Cylinder() noexcept;

    /**
     * @brief Draw a cylinder with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the cylinder. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Cylinder(const mat4 &p_Transform) noexcept;

    /**
     * @brief Draw a unit sphere centered at the origin.
     *
     * The sphere will be affected by the current transformation state.
     */
    void Sphere() noexcept;

    /**
     * @brief Draw a sphere with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the sphere. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Sphere(const mat4 &p_Transform) noexcept;

    /**
     * @brief Draw a unit capsule centered at the origin.
     *
     * The capsule will be affected by the current transformation state.
     */
    void Capsule() noexcept;

    /**
     * @brief Draw a capsule with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the capsule. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Capsule(const mat4 &p_Transform) noexcept;

    /**
     * @brief Draw a capsule shape with the given length and radius.
     *
     * @param p_Length The length of the cylindrical part of the capsule.
     * @param p_Radius The radius of the hemispherical ends.
     */
    void Capsule(f32 p_Length, f32 p_Radius) noexcept;

    /**
     * @brief Draw a capsule shape with the given parameters and transformation.
     *
     * @param p_Length The length of the cylindrical part of the capsule.
     * @param p_Radius The radius of the hemispherical ends.
     * @param p_Transform The transformation matrix to apply to the capsule. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Capsule(f32 p_Length, f32 p_Radius, const mat4 &p_Transform) noexcept;

    /**
     * @brief Draw a unit rounded cube centered at the origin.
     *
     * The rounded cube will be affected by the current transformation state.
     */
    void RoundedCube() noexcept;

    /**
     * @brief Draw a rounded cube with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void RoundedCube(const mat4 &p_Transform) noexcept;

    /**
     * @brief Draw a rounded cube with given dimensions and corner radius.
     *
     * @param p_Dimensions The width, height, and depth of the cube.
     * @param p_Radius The radius of the corners.
     */
    void RoundedCube(const vec3 &p_Dimensions, f32 p_Radius) noexcept;

    /**
     * @brief Draw a rounded cube with given dimensions, corner radius, and transformation.
     *
     * @param p_Dimensions The width, height, and depth of the cube.
     * @param p_Radius The radius of the corners.
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void RoundedCube(const vec3 &p_Dimensions, f32 p_Radius, const mat4 &p_Transform) noexcept;

    /**
     * @brief Draw a rounded cube with given dimensions and corner radius.
     *
     * @param p_Width The width of the cube.
     * @param p_Height The height of the cube.
     * @param p_Depth The depth of the cube.
     * @param p_Radius The radius of the corners.
     */
    void RoundedCube(f32 p_Width, f32 p_Height, f32 p_Depth, f32 p_Radius) noexcept;

    /**
     * @brief Draw a rounded cube with given dimensions, corner radius, and transformation.
     *
     * @param p_Width The width of the cube.
     * @param p_Height The height of the cube.
     * @param p_Depth The depth of the cube.
     * @param p_Radius The radius of the corners.
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void RoundedCube(f32 p_Width, f32 p_Height, f32 p_Depth, f32 p_Radius, const mat4 &p_Transform) noexcept;

    /**
     * @brief Set the color of the light for subsequent lighting calculations.
     *
     * @param p_Color The color of the light.
     */
    void LightColor(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void LightColor(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        LightColor(color);
    }

    /**
     * @brief Set the ambient light color.
     *
     * @param p_Color The ambient light color.
     */
    void AmbientColor(const Color &p_Color) noexcept;
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void AmbientColor(ColorArgs &&...p_ColorArgs) noexcept
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        AmbientColor(color);
    }

    /**
     * @brief Set the intensity of the ambient light.
     *
     * Must be called after setting the ambient color.
     *
     * @param p_Intensity The ambient light intensity.
     */
    void AmbientIntensity(f32 p_Intensity) noexcept;

    /**
     * @brief Adds a directional light to the scene.
     *
     * @param p_Light The directional light object.
     */
    void DirectionalLight(ONYX::DirectionalLight p_Light) noexcept;

    /**
     * @brief Adds a directional light to the scene with the specified direction and intensity.
     *
     * @param p_Direction The direction of the light.
     * @param p_Intensity The intensity of the light.
     */
    void DirectionalLight(const vec3 &p_Direction, f32 p_Intensity = 1.f) noexcept;

    /**
     * @brief Adds a directional light to the scene with the specified direction components and intensity.
     *
     * @param p_DX The X component of the direction.
     * @param p_DY The Y component of the direction.
     * @param p_DZ The Z component of the direction.
     * @param p_Intensity The intensity of the light.
     */
    void DirectionalLight(f32 p_DX, f32 p_DY, f32 p_DZ, f32 p_Intensity = 1.f) noexcept;

    /**
     * @brief Adds a point light to the scene.
     *
     * @param p_Light The point light object.
     */
    void PointLight(const ONYX::PointLight &p_Light) noexcept;

    /**
     * @brief Adds a point light to the scene at the current position.
     *
     * @param p_Radius The radius of the light's influence.
     * @param p_Intensity The intensity of the light.
     */
    void PointLight(f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;

    /**
     * @brief Adds a point light to the scene at the specified position.
     *
     * @param p_Position The position of the light.
     * @param p_Radius The radius of the light's influence.
     * @param p_Intensity The intensity of the light.
     */
    void PointLight(const vec3 &p_Position, f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;

    /**
     * @brief Adds a point light to the scene at the specified position.
     *
     * @param p_X The X-coordinate of the light's position.
     * @param p_Y The Y-coordinate of the light's position.
     * @param p_Z The Z-coordinate of the light's position.
     * @param p_Radius The radius of the light's influence.
     * @param p_Intensity The intensity of the light.
     */
    void PointLight(f32 p_X, f32 p_Y, f32 p_Z, f32 p_Radius = 1.f, f32 p_Intensity = 1.f) noexcept;

    /**
     * @brief Set the diffuse contribution factor for lighting calculations.
     *
     * @param p_Contribution The diffuse contribution factor.
     */
    void DiffuseContribution(f32 p_Contribution) noexcept;

    /**
     * @brief Set the specular contribution factor for lighting calculations.
     *
     * @param p_Contribution The specular contribution factor.
     */
    void SpecularContribution(f32 p_Contribution) noexcept;

    /**
     * @brief Set the specular sharpness for lighting calculations.
     *
     * @param p_Sharpness The specular sharpness factor.
     */
    void SpecularSharpness(f32 p_Sharpness) noexcept;

    /**
     * @brief Set the global projection matrix for the rendering context.
     *
     * @param p_Projection The projection matrix to set.
     */
    void SetProjection(const mat4 &p_Projection) noexcept;

    /**
     * @brief Set a global perspective projection with the given field of view and near/far planes.
     *
     * @param p_FieldOfView The field of view in radians.
     * @param p_Near The near clipping plane.
     * @param p_Far The far clipping plane.
     */
    void SetPerspectiveProjection(f32 p_FieldOfView, f32 p_Near, f32 p_Far) noexcept;

    /**
     * @brief Set a global orthographic projection for the rendering context.
     *
     */
    void SetOrthographicProjection() noexcept;

    /**
     * @brief Retrieve the mouse coordinates at the specified depth in the rendering context.
     *
     * @param p_Depth The depth at which to get the mouse coordinates.
     * @return The mouse coordinates as a 3D vector.
     */
    vec3 GetMouseCoordinates(f32 p_Depth) const noexcept;
};
} // namespace ONYX
