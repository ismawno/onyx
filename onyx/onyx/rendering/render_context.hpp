#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/rendering/camera.hpp"
#include "onyx/data/options.hpp"
#include <vulkan/vulkan.h>

// 2D objects that are drawn later will always be on top of earlier ones. HOWEVER, blending will only work expectedly
// between objects of the same primitive

// Because of batch rendering, draw order is not guaranteed

#ifndef ONYX_MAX_RENDER_STATE_DEPTH
#    define ONYX_MAX_RENDER_STATE_DEPTH 8
#endif

namespace Onyx
{
template <Dimension D> using RenderStateStack = TKit::StaticArray<RenderState<D>, ONYX_MAX_RENDER_STATE_DEPTH>;

class Window;

namespace Detail
{
template <Dimension D> class IRenderContext
{
    TKIT_NON_COPYABLE(IRenderContext)
  public:
    IRenderContext(Window *p_Window, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept;

    /**
     * @brief Clear all of the recorded draw data until this point.
     *
     * It is meant to be called at the beginning of the frame, but it is not required to do so in case you want to
     * persist Onyx draw calls made in older frames.
     *
     * It calls all of the renderer's `Flush()` methods.
     *
     */
    void FlushDrawData() noexcept;

    /**
     * @brief Reset the state of the `RenderContext` to its default values.
     *
     * This method is meant to be called at the beginning of the frame, but it is not required to do so in case you want
     * to persist the state of the `RenderContext` from older frames.
     *
     */
    void FlushState() noexcept;

    /**
     * @brief Reset the state of the `RenderContext` to its default values and sets the background color.
     *
     * @param p_Color The background color to set.
     */
    void FlushState(const Color &p_Color) noexcept;

    /**
     * @brief Reset the state of the `RenderContext` to its default values and sets the background color.
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
     * @brief Call both `FlushDrawData()` and `FlushState()`.
     *
     */
    void Flush() noexcept;

    /**
     * @brief Call both `FlushDrawData()` and `FlushState()`, and sets the background color.
     *
     * @param p_Color The background color to set.
     */
    void Flush(const Color &p_Color) noexcept;

    /**
     * @brief Call both `FlushDrawData()` and `FlushState()`, and sets the background color.
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
    void Transform(const fmat<D> &p_Transform) noexcept;

    /**
     * @brief Transform subsequent shapes by the given translation, scale, and rotation.
     *
     * It corresponds to an extrinsic transformation directly applied to the shape.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scale vector.
     * @param p_Rotation The rotation object.
     */
    void Transform(const fvec<D> &p_Translation, const fvec<D> &p_Scale, const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Transform subsequent shapes by the given translation, uniform scale, and rotation.
     *
     * It corresponds to an extrinsic transformation directly applied to the shape.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scale value.
     * @param p_Rotation The rotation object.
     */
    void Transform(const fvec<D> &p_Translation, f32 p_Scale, const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Transform subsequent shapes' coordinate system (axes) by the given transformation matrix.
     *
     * It corresponds to an intrinsic transformation applied to the coordinate system, which is then applied to all
     * subsequent shapes extrinsically.
     *
     * @param p_Transform The transformation matrix.
     */
    void TransformAxes(const fmat<D> &p_Transform) noexcept;

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
    void TransformAxes(const fvec<D> &p_Translation, const fvec<D> &p_Scale, const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Transform subsequent shapes' coordinate system (axes) by the given translation, uniform scale, and
     * rotation.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scale value.
     * @param p_Rotation The rotation object.
     */
    void TransformAxes(const fvec<D> &p_Translation, f32 p_Scale, const rot<D> &p_Rotation) noexcept;

    /**
     * @brief Translate subsequent shapes by the given vector.
     *
     * This applies a translation transformation to all subsequent draw calls.
     *
     * @param p_Translation The translation vector.
     */
    void Translate(const fvec<D> &p_Translation) noexcept;

    /**
     * @brief Scale subsequent shapes by the given vector.
     *
     * This applies a scaling transformation to all subsequent draw calls.
     *
     * @param p_Scale The scaling vector.
     */
    void Scale(const fvec<D> &p_Scale) noexcept;

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
    void TranslateAxes(const fvec<D> &p_Translation) noexcept;

    /**
     * @brief Scale the coordinate system (axes) by the given vector.
     *
     * This applies an intrinsic scaling to the coordinate system, affecting all subsequent draw calls.
     *
     * @param p_Scale The scaling vector.
     */
    void ScaleAxes(const fvec<D> &p_Scale) noexcept;

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
    void Triangle(const fmat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a unit triangle centered at the origin.
     *
     * The triangle will be affected by the current transformation state.
     *
     * @param p_Dimensions The dimensions of the triangle.
     */
    void Triangle(const fvec2 &p_Dimensions) noexcept;

    /**
     * @brief Draw a triangle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the triangle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The dimensions of the triangle.
     */
    void Triangle(const fmat<D> &p_Transform, const fvec2 &p_Dimensions) noexcept;

    /**
     * @brief Draw a unit triangle centered at the origin.
     *
     * The triangle will be affected by the current transformation state.
     *
     * @param p_Size The size of the triangle.
     */
    void Triangle(f32 p_Size) noexcept;

    /**
     * @brief Draw a triangle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the triangle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Size The size of the triangle.
     */
    void Triangle(const fmat<D> &p_Transform, f32 p_Size) noexcept;

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
    void Square(const fmat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a square with the specified dimensions.
     *
     * @param p_Dimensions The dimensions of the square.
     */
    void Square(const fvec2 &p_Dimensions) noexcept;

    /**
     * @brief Draw a square with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The dimensions of the square.
     */
    void Square(const fmat<D> &p_Transform, const fvec2 &p_Dimensions) noexcept;

    /**
     * @brief Draw a square with the specified dimensions.
     *
     * @param p_Size The size of the square.
     */
    void Square(f32 p_Size) noexcept;

    /**
     * @brief Draw a square with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Size The size of the square.
     */
    void Square(const fmat<D> &p_Transform, f32 p_Size) noexcept;

    /**
     * @brief Draw a regular n-sided polygon centered at the origin.
     *
     * @param p_Sides The number of sides of the polygon.
     */
    void NGon(u32 p_Sides) noexcept;

    /**
     * @brief Draw a regular n-sided polygon with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Sides The number of sides of the polygon.
     */
    void NGon(const fmat<D> &p_Transform, u32 p_Sides) noexcept;

    /**
     * @brief Draw a regular n-sided polygon centered at the origin.
     *
     * @param p_Sides The number of sides of the polygon.
     * @param p_Dimensions The dimensions of the polygon.
     */
    void NGon(u32 p_Sides, const fvec2 &p_Dimensions) noexcept;

    /**
     * @brief Draw a regular n-sided polygon with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Sides The number of sides of the polygon.
     * @param p_Dimensions The dimensions of the polygon.
     */
    void NGon(const fmat<D> &p_Transform, u32 p_Sides, const fvec2 &p_Dimensions) noexcept;

    /**
     * @brief Draw a regular n-sided polygon centered at the origin.
     *
     * @param p_Sides The number of sides of the polygon.
     * @param p_Size The size of the polygon.
     */
    void NGon(u32 p_Sides, f32 p_Size) noexcept;

    /**
     * @brief Draw a regular n-sided polygon with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Sides The number of sides of the polygon.
     * @param p_Size The size of the polygon.
     */
    void NGon(const fmat<D> &p_Transform, u32 p_Sides, f32 p_Size) noexcept;

    /**
     * @brief Draw a polygon defined by the given vertices.
     *
     * The vertices must be in counter-clockwise order, otherwise outlines will not be drawn correctly.
     * The polygon will be affected by the current transformation state.
     * The polygon's origin is its transform's position (with respect to the current axes, of course), and so the
     * vertices are expected to be with respect that position.
     *
     * In 3D, to define a correct 2D polygon, all vertices must lie on the same plane. That is quite hard to
     * achieve, so the way to go is to use 2D vectors for the vertices in 3D, and if you want another orientation, just
     * rotate the polygon. The default plane for 3D polygons is the XY plane.
     *
     * @param p_Vertices A span of vertices defining the polygon. Vertices are expected to be centered around zero.
     */
    void Polygon(TKit::Span<const fvec2> p_Vertices) noexcept;

    /**
     * @brief Draw a polygon defined by the given vertices with the specified transformation.
     *
     * The vertices must be in counter-clockwise order, otherwise outlines will not be drawn correctly.
     * The polygon will be affected by the current transformation state.
     * The polygon's origin is its transform's position (with respect to the current axes, of course).
     *
     * In 3D, to define a correct 2D polygon, all vertices must lie on the same plane. That is quite hard to
     * achieve, so the way to go is to use 2D vectors for the vertices in 3D, and if you want another orientation, just
     * rotate the polygon. The default plane for 3D polygons is the XY plane.
     *
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Vertices A span of vertices defining the polygon.
     */
    void Polygon(const fmat<D> &p_Transform, TKit::Span<const fvec2> p_Vertices) noexcept;

    /**
     * @brief Draw a unit circle centered at the origin.
     *
     * The circle will be affected by the current transformation state.
     *
     * The following is encoded in the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const CircleOptions &p_Options = {}) noexcept;

    /**
     * @brief Draw a circle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     *
     * The following is encoded in the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const fmat<D> &p_Transform, const CircleOptions &p_Options = {}) noexcept;

    /**
     * @brief Draw a unit circle centered at the origin.
     *
     * The circle will be affected by the current transformation state.
     *
     * @param p_Dimensions The dimensions of the circle.
     *
     * The following is encoded in the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const fvec2 &p_Dimensions, const CircleOptions &p_Options = {}) noexcept;

    /**
     * @brief Draw a circle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The dimensions of the circle.
     *
     * The following is encoded in the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const fmat<D> &p_Transform, const fvec2 &p_Dimensions, const CircleOptions &p_Options = {}) noexcept;

    /**
     * @brief Draw a unit circle centered at the origin.
     *
     * The circle will be affected by the current transformation state.
     *
     * @param p_Diameter The diameter of the circle.
     *
     * The following is encoded in the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(f32 p_Diameter, const CircleOptions &p_Options = {}) noexcept;

    /**
     * @brief Draw a circle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Diameter The diameter of the circle.
     *
     * The following is encoded in the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const fmat<D> &p_Transform, f32 p_Diameter, const CircleOptions &p_Options = {}) noexcept;

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
    void Stadium(const fmat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a stadium shape with the given length and diameter.
     *
     * @param p_Length The length of the rectangular part of the stadium.
     * @param p_Diameter The diameter of the semicircular ends.
     */
    void Stadium(f32 p_Length, f32 p_Diameter) noexcept;

    /**
     * @brief Draw a stadium shape with the given parameters and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the stadium. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Length The length of the rectangular part of the stadium.
     * @param p_Diameter The diameter of the semicircular ends.
     */
    void Stadium(const fmat<D> &p_Transform, f32 p_Length, f32 p_Diameter) noexcept;

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
    void RoundedSquare(const fmat<D> &p_Transform) noexcept;

    /**
     * @brief Draw a rounded square with given dimensions and corner diameter.
     *
     * @param p_Dimensions The width and height of the square.
     * @param p_Diameter The diameter of the corners.
     */
    void RoundedSquare(const fvec2 &p_Dimensions, f32 p_Diameter) noexcept;

    /**
     * @brief Draw a rounded square with given dimensions, corner diameter, and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the rounded square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The width and height of the square.
     * @param p_Diameter The diameter of the corners.
     */
    void RoundedSquare(const fmat<D> &p_Transform, const fvec2 &p_Dimensions, f32 p_Diameter) noexcept;

    /**
     * @brief Draw a rounded square with given dimensions and corner diameter.
     *
     * @param p_Size The size of the square.
     * @param p_Diameter The diameter of the corners.
     */
    void RoundedSquare(f32 p_Size, f32 p_Diameter) noexcept;

    /**
     * @brief Draw a rounded square with given dimensions, corner diameter, and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the rounded square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Size The size of the square.
     * @param p_Diameter The diameter of the corners.
     */
    void RoundedSquare(const fmat<D> &p_Transform, f32 p_Size, f32 p_Diameter) noexcept;

    // Actually, 2D meshes could be used in 3D as well. This feature is not implemented yet. If you want a 2D mesh in a
    // 3D context, you must load such mesh as a 3D mesh

    /**
     * @brief Draw a mesh model.
     *
     * @param p_Mesh The mesh model to draw.
     */
    void Mesh(const Onyx::Mesh<D> &p_Mesh) noexcept;

    /**
     * @brief Draw a mesh model with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the model. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Mesh The mesh model to draw.
     */
    void Mesh(const fmat<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh) noexcept;

    /**
     * @brief Draw a mesh model.
     *
     * @param p_Mesh The mesh model to draw.
     */
    void Mesh(const Onyx::Mesh<D> &p_Mesh, const fvec<D> &p_Dimensions) noexcept;

    /**
     * @brief Draw a mesh model with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the mesh. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Mesh The mesh model to draw.
     */
    void Mesh(const fmat<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh, const fvec<D> &p_Dimensions) noexcept;

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
     * @brief Get the current rendering state.
     *
     * @return A constant reference to the current `RenderState`.
     */
    const RenderState<D> &GetCurrentState() const noexcept;

    /**
     * @brief Get the current rendering state.
     *
     * @return A reference to the current `RenderState`.
     */
    RenderState<D> &GetCurrentState() noexcept;

    /**
     * @brief Grow all device buffers to fit host data.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void GrowToFit(u32 p_FrameIndex) noexcept;

    /**
     * @brief Send all stored host data to the device.
     *
     * @param p_FrameIndex The index of the frame to send data for.
     */
    void SendToDevice(u32 p_FrameIndex) noexcept;

    /**
     * @brief Record vulkan copy commands to send data to a device local buffer.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_GraphicsCommand The graphics command buffer.
     * @param p_TransferCommand The transfer command buffer.
     */
    void RecordCopyCommands(u32 p_FrameIndex, VkCommandBuffer p_GraphicsCommand,
                            VkCommandBuffer p_TransferCommand) noexcept;

    /**
     * @brief Render the recorded draw data using the provided command buffer.
     *
     * @param p_FrameIndex The index of the frame to render.
     * @param p_CommandBuffer The Vulkan command buffer to use for rendering.
     * @param p_Cameras The cameras from which to render the scene.
     */
    void Render(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer, TKit::Span<const CameraInfo> p_Cameras) noexcept;

  protected:
    template <typename F1, typename F2> void resolveDrawFlagsWithState(F1 &&p_FillDraw, F2 &&p_OutlineDraw) noexcept;

    void updateState() noexcept;

    template <Dimension PDim>
    void drawPrimitive(const RenderState<D> &p_State, const fmat<D> &p_Transform, u32 p_PrimitiveIndex) noexcept;
    template <Dimension PDim>
    void drawPrimitive(const RenderState<D> &p_State, const fmat<D> &p_Transform, u32 p_PrimitiveIndex,
                       const fvec<PDim> &p_Dimensions) noexcept;

    void drawPolygon(const RenderState<D> &p_State, const fmat<D> &p_Transform,
                     TKit::Span<const fvec2> p_Vertices) noexcept;

    void drawCircle(const RenderState<D> &p_State, const fmat<D> &p_Transform, const CircleOptions &p_Options) noexcept;
    void drawCircle(const RenderState<D> &p_State, const fmat<D> &p_Transform, const CircleOptions &p_Options,
                    const fvec2 &p_Dimensions) noexcept;

    void drawChildCircle(const RenderState<D> &p_State, fmat<D> p_Transform, const fvec<D> &p_Position,
                         const CircleOptions &p_Options, Detail::DrawFlags p_Flags) noexcept;
    void drawChildCircle(const RenderState<D> &p_State, fmat<D> p_Transform, const fvec<D> &p_Position, f32 p_Diameter,
                         const CircleOptions &p_Options, Detail::DrawFlags p_Flags) noexcept;

    void drawStadiumMoons(const RenderState<D> &p_State, const fmat<D> &p_Transform,
                          Detail::DrawFlags p_Flags) noexcept;
    void drawStadiumMoons(const RenderState<D> &p_State, const fmat<D> &p_Transform, f32 p_Length, f32 p_Diameter,
                          Detail::DrawFlags p_Flags) noexcept;

    void drawStadium(const RenderState<D> &p_State, const fmat<D> &p_Transform) noexcept;
    void drawStadium(const RenderState<D> &p_State, const fmat<D> &p_Transform, f32 p_Length, f32 p_Diameter) noexcept;

    void drawRoundedSquareMoons(const RenderState<D> &p_State, const fmat<D> &p_Transform, const fvec2 &p_Dimension,
                                f32 p_Diameter, Detail::DrawFlags p_Flags) noexcept;

    void drawRoundedSquare(const RenderState<D> &p_State, const fmat<D> &p_Transform) noexcept;
    void drawRoundedSquare(const RenderState<D> &p_State, const fmat<D> &p_Transform, const fvec2 &p_Dimension,
                           f32 p_Diameter) noexcept;

    void drawMesh(const RenderState<D> &p_State, const fmat<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh) noexcept;
    void drawMesh(const RenderState<D> &p_State, const fmat<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh,
                  const fvec<D> &p_Dimensions) noexcept;

    RenderState<D> *m_State;
    Detail::Renderer<D> m_Renderer;
    Window *m_Window;

  private:
    RenderStateStack<D> m_StateStack;
};
} // namespace Detail

/**
 * @brief The `RenderContext` class is the primary way of communicating with the Onyx API.
 *
 * It is a high-level API that allows the user to draw shapes and meshes in a simple immediate mode
 * fashion. The draw calls are recorded, sent to the gpu and translated to vulkan draw calls when appropiate. The
 * following is a set of properties of the `RenderContext` you must take into account when using it:
 *
 * - You may use the `RenderContext` at almost any moment. Do not forget to call `Flush()` at the beginning of your loop
 * to not to persist data from the last frame.
 *
 * - The `RenderContext` is mostly immediate mode. All mutations to its state can be reset with the `Flush()`
 * method, which is recommended to be called at the beginning of each frame.
 *
 * - Keep in mind that outlines are affected by the scaling of the shapes they outline. This means you may get weird
 * outlines with scaled shapes, especially if the scaling is not uniform. To avoid this issue when using outlines,
 * always try to modify the shape's dimensions explicitly through function parameters, instead of trying to apply
 * scaling transformations directly. Note that all shapes have a way to set their dimensions directly. That particular
 * way will work well with outlines.
 *
 * - Onyx renderers use batch rendering to optimize draw calls. This means that in some cases, the order in which shapes
 * are drawn may not be respected.
 *
 * - All entities that can be added to the scene (shapes, meshes, lights) will always have their position, scale and
 * rotation relative to the current axes transform, which can be modified as well.
 *
 * - Please note that objects drawn in the scene inherit the state the axes were in when the draw command was issued.
 * This means that every object drawn will have its own, dedicated parent axes transform. Because of this, the view's
 * transform found in all cameras (which are persisted) cannot be bound to the axes, as these are not unique and well
 * defined. If you want to query the view's transform with respect to the current axes, you must use the
 * `GetViewTransform()` camera method. Otherwise, you may query the view's transform from the
 * `GetProjectionViewData()` camera method, which will not have any axes transform applied to it.
 *
 */
template <Dimension D> class RenderContext;

template <> class ONYX_API RenderContext<D2> final : public Detail::IRenderContext<D2>
{
  public:
    using IRenderContext<D2>::IRenderContext;

    /**
     * @brief Render the coordinate axes for visualization.
     *
     * Draw the X and Y axes with the specified thickness and size.
     *
     * The following is encoded in the `AxesOptions` struct:
     *
     * @param p_Thickness The thickness of the axes lines.
     * @param p_Size The length of the axes.
     */
    void Axes(const AxesOptions<D2> &p_Options = {}) noexcept;

    /**
     * @brief Rotates subsequent shapes by the given angle.
     *
     * @param p_Angle The rotation angle in radians.
     */
    void Rotate(f32 p_Angle) noexcept;

    /**
     * @brief Rotates the coordinate system by the given angle.
     *
     * @param p_Angle The rotation angle in radians.
     */
    void RotateAxes(f32 p_Angle) noexcept;

    /**
     * @brief Draw a line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void Line(const fvec2 &p_Start, const fvec2 &p_End, f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a line strip through the given points.
     *
     * @param p_Points A span of points defining the line strip.
     * @param p_Thickness The thickness of the line.
     */
    void LineStrip(TKit::Span<const fvec2> p_Points, f32 p_Thickness = 0.01f) noexcept;

    /**
     * @brief Draw a rounded line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void RoundedLine(const fvec2 &p_Start, const fvec2 &p_End, f32 p_Thickness = 0.01f) noexcept;
};

template <> class ONYX_API RenderContext<D3> final : public Detail::IRenderContext<D3>
{
  public:
    using IRenderContext<D3>::IRenderContext;
    using IRenderContext<D3>::Transform;
    using IRenderContext<D3>::TransformAxes;

    /**
     * @brief Render the coordinate axes for visualization.
     *
     * Draw the X, Y and Z axes with the specified thickness and size.
     *
     * The following is encoded in the `AxesOptions` struct:
     *
     * @param p_Thickness The thickness of the axes lines.
     * @param p_Size The length of the axes.
     */
    void Axes(const AxesOptions<D3> &p_Options = {}) noexcept;

    /**
     * @brief Transforms subsequent shapes by the given translation, scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scaling vector.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void Transform(const fvec3 &p_Translation, const fvec3 &p_Scale, const fvec3 &p_Rotation) noexcept;

    /**
     * @brief Transforms subsequent shapes by the given translation, uniform scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scaling factor.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void Transform(const fvec3 &p_Translation, f32 p_Scale, const fvec3 &p_Rotation) noexcept;

    /**
     * @brief Transforms the coordinate system by the given translation, scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scaling vector.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void TransformAxes(const fvec3 &p_Translation, const fvec3 &p_Scale, const fvec3 &p_Rotation) noexcept;

    /**
     * @brief Transforms the coordinate system by the given translation, uniform scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scaling factor.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void TransformAxes(const fvec3 &p_Translation, f32 p_Scale, const fvec3 &p_Rotation) noexcept;

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
    void Rotate(const fvec3 &p_Angles) noexcept;

    /**
     * @brief Rotates subsequent shapes by the given angle around the specified axis.
     *
     * @param p_Angle The rotation angle in radians.
     * @param p_Axis The axis to rotate around.
     */
    void Rotate(f32 p_Angle, const fvec3 &p_Axis) noexcept;

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
    void RotateAxes(const fvec3 &p_Angles) noexcept;

    /**
     * @brief Rotates the coordinate system by the given angle around the specified axis.
     *
     * @param p_Angle The rotation angle in radians.
     * @param p_Axis The axis to rotate around.
     */
    void RotateAxes(f32 p_Angle, const fvec3 &p_Axis) noexcept;

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
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void Line(const fvec3 &p_Start, const fvec3 &p_End, const LineOptions &p_Options = {}) noexcept;

    /**
     * @brief Draw a line strip through the given points.
     *
     * @param p_Points A span of points defining the line strip.
     * @param p_Thickness The thickness of the line.
     */
    void LineStrip(TKit::Span<const fvec3> p_Points, const LineOptions &p_Options = {}) noexcept;

    /**
     * @brief Draw a rounded line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void RoundedLine(const fvec3 &p_Start, const fvec3 &p_End, const LineOptions &p_Options = {}) noexcept;

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
    void Cube(const fmat4 &p_Transform) noexcept;

    /**
     * @brief Draw a cube with the specified dimensions.
     *
     * @param p_Dimensions The width, height, and depth of the cube.
     */
    void Cube(const fvec3 &p_Dimensions) noexcept;

    /**
     * @brief Draw a cube with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The width, height, and depth of the cube.
     */
    void Cube(const fmat4 &p_Transform, const fvec3 &p_Dimensions) noexcept;

    /**
     * @brief Draw a cube with the specified dimensions.
     *
     * @param p_Size The size of the cube.
     */
    void Cube(f32 p_Size) noexcept;

    /**
     * @brief Draw a cube with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Size The size of the cube.
     */
    void Cube(const fmat4 &p_Transform, f32 p_Size) noexcept;

    /**
     * @brief Draw a unit cylinder centered at the origin.
     *
     * The cylinder will be affected by the current transformation state.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a cylinder with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the cylinder. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(const fmat4 &p_Transform, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a cylinder with the specified dimensions.
     *
     * @param p_Dimensions The width, height, and depth of the cylinder.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(const fvec3 &p_Dimensions, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a cylinder with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the cylinder. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The width, height, and depth of the cylinder.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(const fmat4 &p_Transform, const fvec3 &p_Dimensions, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a cylinder shape with the given length and diameter.
     *
     * @param p_Length The length of the cylindrical part of the cylinder.
     * @param p_Diameter The diameter of the hemispherical ends.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(f32 p_Length, f32 p_Diameter, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a cylinder shape with the given parameters and transformation.
     *
     * @param p_Length The length of the cylindrical part of the cylinder.
     * @param p_Diameter The diameter of the hemispherical ends.
     * @param p_Transform The transformation matrix to apply to the cylinder. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(const fmat4 &p_Transform, f32 p_Length, f32 p_Diameter,
                  Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a unit sphere centered at the origin.
     *
     * The sphere will be affected by the current transformation state.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a sphere with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the sphere. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(const fmat4 &p_Transform, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a sphere shape with the given dimensions.
     *
     * @param p_Dimensions The width, height, and depth of the sphere.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(const fvec3 &p_Dimensions, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a sphere shape with the given dimensions and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the sphere. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The width, height, and depth of the sphere.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(const fmat4 &p_Transform, const fvec3 &p_Dimensions, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a sphere shape with the given dimensions.
     *
     * @param p_Diameter The diameter of the sphere.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(f32 p_Diameter, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a sphere shape with the given dimensions and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the sphere. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Diameter The diameter of the sphere.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(const fmat4 &p_Transform, f32 p_Diameter, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a unit capsule centered at the origin.
     *
     * The capsule will be affected by the current transformation state.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Capsule(Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a capsule with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the capsule. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Capsule(const fmat4 &p_Transform, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a capsule shape with the given length and diameter.
     *
     * @param p_Length The length of the cylindrical part of the capsule.
     * @param p_Diameter The diameter of the hemispherical ends.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Capsule(f32 p_Length, f32 p_Diameter, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a capsule shape with the given parameters and transformation.
     *
     * @param p_Length The length of the cylindrical part of the capsule.
     * @param p_Diameter The diameter of the hemispherical ends.
     * @param p_Transform The transformation matrix to apply to the capsule. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Capsule(const fmat4 &p_Transform, f32 p_Length, f32 p_Diameter,
                 Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a unit rounded cube centered at the origin.
     *
     * The rounded cube will be affected by the current transformation state.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a rounded cube with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(const fmat4 &p_Transform, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a rounded cube with given dimensions and corner diameter.
     *
     * @param p_Dimensions The width, height, and depth of the cube.
     * @param p_Diameter The diameter of the corners.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(const fvec3 &p_Dimensions, f32 p_Diameter, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a rounded cube with given dimensions, corner diameter, and transformation.
     *
     * @param p_Dimensions The width, height, and depth of the cube.
     * @param p_Diameter The diameter of the corners.
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(const fmat4 &p_Transform, const fvec3 &p_Dimensions, f32 p_Diameter,
                     Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a rounded cube with given dimensions and corner diameter.
     *
     * @param p_Size The size of the cube.
     * @param p_Diameter The diameter of the corners.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(f32 p_Size, f32 p_Diameter, Resolution p_Res = Resolution::Medium) noexcept;

    /**
     * @brief Draw a rounded cube with given dimensions, corner diameter, and transformation.
     *
     * @param p_Size The size of the cube.
     * @param p_Diameter The diameter of the corners.
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(const fmat4 &p_Transform, f32 p_Size, f32 p_Diameter,
                     Resolution p_Res = Resolution::Medium) noexcept;

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
    void DirectionalLight(Onyx::DirectionalLight p_Light) noexcept;

    /**
     * @brief Adds a directional light to the scene with the specified direction and intensity.
     *
     * @param p_Direction The direction of the light.
     * @param p_Intensity The intensity of the light.
     */
    void DirectionalLight(const fvec3 &p_Direction, f32 p_Intensity = 1.f) noexcept;

    /**
     * @brief Adds a point light to the scene.
     *
     * @param p_Light The point light object.
     */
    void PointLight(Onyx::PointLight p_Light) noexcept;

    /**
     * @brief Adds a point light to the scene at the current position.
     *
     * @param p_Diameter The diameter of the light's influence.
     * @param p_Intensity The intensity of the light.
     */
    void PointLight(f32 p_Diameter = 1.f, f32 p_Intensity = 1.f) noexcept;

    /**
     * @brief Adds a point light to the scene at the specified position.
     *
     * @param p_Position The position of the light.
     * @param p_Diameter The diameter of the light's influence.
     * @param p_Intensity The intensity of the light.
     */
    void PointLight(const fvec3 &p_Position, f32 p_Diameter = 1.f, f32 p_Intensity = 1.f) noexcept;

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

  private:
    void drawChildSphere(const RenderState<D3> &p_State, fmat4 p_Transform, const fvec3 &p_Position, Resolution p_Res,
                         Detail::DrawFlags p_Flags) noexcept;
    void drawChildSphere(const RenderState<D3> &p_State, fmat4 p_Transform, const fvec3 &p_Position, f32 p_Diameter,
                         Resolution p_Res, Detail::DrawFlags p_Flags) noexcept;

    void drawCapsule(const RenderState<D3> &p_State, const fmat4 &p_Transform, Resolution p_Res) noexcept;
    void drawCapsule(const RenderState<D3> &p_State, const fmat4 &p_Transform, f32 p_Length, f32 p_Diameter,
                     Resolution p_Res) noexcept;

    void drawRoundedCubeMoons(const RenderState<D3> &p_State, const fmat4 &p_Transform, const fvec3 &p_Dimensions,
                              f32 p_Diameter, Resolution p_Res, Detail::DrawFlags p_Flags) noexcept;

    void drawRoundedCube(const RenderState<D3> &p_State, const fmat4 &p_Transform, Resolution p_Res) noexcept;
    void drawRoundedCube(const RenderState<D3> &p_State, const fmat4 &p_Transform, const fvec3 &p_Dimensions,
                         f32 p_Diameter, Resolution p_Res) noexcept;
};
} // namespace Onyx
