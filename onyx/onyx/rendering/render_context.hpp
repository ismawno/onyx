#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/data/options.hpp"
#include <vulkan/vulkan.h>

// 2D objects that are drawn later will always be on top of earlier ones. HOWEVER, blending will only work expectedly
// between objects of the same primitive

// Because of batch rendering, draw order is not guaranteed

#ifndef ONYX_MAX_RENDER_STATE_DEPTH
#    define ONYX_MAX_RENDER_STATE_DEPTH 8
#endif

TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4324)

namespace Onyx
{

class Window;

namespace Detail
{
template <Dimension D> using RenderStateStack = TKit::StaticArray<RenderState<D>, ONYX_MAX_RENDER_STATE_DEPTH>;
template <Dimension D> class IRenderContext
{
    TKIT_NON_COPYABLE(IRenderContext)
  public:
    IRenderContext(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

    /**
     * @brief Flushes all cpu-stored draw data, signaling the context to draw from scratch, and resets the state for all
     *threads.
     *
     * At the end of the frame, all cpu-stored data will be sent to the device and drawn. If this method is not called
     * in the next frame, no new data will be sent and the device will keep rendering what was recorded last frame.
     *
     * New calls to `RenderContext` will have no effect and will be lost if at the start of a new frame `Flush()` is not
     * called and draw commands are issued.
     *
     * This method is not thread-safe and should be called once from a single thread.
     *
     * @param p_ThreadCount The number of threads affected. The application may not be using all of the threads given by
     * `ONYX_MAX_THREADS`. This number may reflect how many threads are actually in use by your application to avoid
     * extra work. The calling thread's index is thus expected to be lower than the count, although it is not required.
     *
     */
    void Flush(const u32 p_ThreadCount = ONYX_MAX_THREADS);

    /**
     * @brief Transform subsequent shapes by the given transformation matrix.
     *
     * It corresponds to an extrinsic transformation directly applied to the shape.
     *
     * @param p_Transform The transformation matrix.
     */
    void Transform(const f32m<D> &p_Transform);

    /**
     * @brief Transform subsequent shapes by the given translation, scale, and rotation.
     *
     * It corresponds to an extrinsic transformation directly applied to the shape.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scale vector.
     * @param p_Rotation The rotation object.
     */
    void Transform(const f32v<D> &p_Translation, const f32v<D> &p_Scale, const rot<D> &p_Rotation);

    /**
     * @brief Transform subsequent shapes by the given translation, uniform scale, and rotation.
     *
     * It corresponds to an extrinsic transformation directly applied to the shape.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scale value.
     * @param p_Rotation The rotation object.
     */
    void Transform(const f32v<D> &p_Translation, f32 p_Scale, const rot<D> &p_Rotation);

    /**
     * @brief Translate subsequent shapes by the given vector.
     *
     * This applies a translation transformation to all subsequent draw calls.
     *
     * @param p_Translation The translation vector.
     */
    void Translate(const f32v<D> &p_Translation);

    /**
     * @brief Set the translation of subsequent shapes by the given vector.
     *
     * This sets a translation transformation to all subsequent draw calls.
     *
     * @param p_Translation The translation vector.
     */
    void SetTranslation(const f32v<D> &p_Translation);

    /**
     * @brief Scale subsequent shapes by the given vector.
     *
     * This applies a scaling transformation to all subsequent draw calls.
     *
     * @param p_Scale The scaling vector.
     */
    void Scale(const f32v<D> &p_Scale);

    /**
     * @brief Scale subsequent shapes uniformly by the given factor.
     *
     * This applies a uniform scaling transformation to all subsequent draw calls.
     *
     * @param p_Scale The scaling factor.
     */
    void Scale(f32 p_Scale);

    /**
     * @brief Translate subsequent shapes along the X-axis.
     *
     * Applies a translation along the X-axis to all subsequent draw calls.
     *
     * @param p_X The translation distance along the X-axis.
     */
    void TranslateX(f32 p_X);

    /**
     * @brief Translate subsequent shapes along the Y-axis.
     *
     * Applies a translation along the Y-axis to all subsequent draw calls.
     *
     * @param p_Y The translation distance along the Y-axis.
     */
    void TranslateY(f32 p_Y);

    /**
     * @brief Set the translation of subsequent shapes along the X-axis.
     *
     * This sets a translation along the X-axis to all subsequent draw calls.
     *
     * @param p_X The translation distance along the X-axis
     */
    void SetTranslationX(f32 p_X);

    /**
     * @brief Set the translation of subsequent shapes along the Y-axis.
     *
     * This sets a translation along the Y-axis to all subsequent draw calls.
     *
     * @param p_Y The translation distance along the Y-axis
     */
    void SetTranslationY(f32 p_Y);

    /**
     * @brief Scale subsequent shapes along the X-axis.
     *
     * Applies a scaling transformation along the X-axis to all subsequent draw calls.
     *
     * @param p_X The scaling factor along the X-axis.
     */
    void ScaleX(f32 p_X);

    /**
     * @brief Scale subsequent shapes along the Y-axis.
     *
     * Applies a scaling transformation along the Y-axis to all subsequent draw calls.
     *
     * @param p_Y The scaling factor along the Y-axis.
     */
    void ScaleY(f32 p_Y);

    /**
     * @brief Draw a unit triangle centered at the origin.
     *
     * The triangle will be affected by the current transformation state.
     */
    void Triangle();

    /**
     * @brief Draw a triangle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the triangle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Triangle(const f32m<D> &p_Transform);

    /**
     * @brief Draw a unit triangle centered at the origin.
     *
     * The triangle will be affected by the current transformation state.
     *
     * @param p_Dimensions The dimensions of the triangle.
     */
    void Triangle(const f32v2 &p_Dimensions);

    /**
     * @brief Draw a triangle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the triangle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The dimensions of the triangle.
     */
    void Triangle(const f32m<D> &p_Transform, const f32v2 &p_Dimensions);

    /**
     * @brief Draw a unit triangle centered at the origin.
     *
     * The triangle will be affected by the current transformation state.
     *
     * @param p_Size The size of the triangle.
     */
    void Triangle(f32 p_Size);

    /**
     * @brief Draw a triangle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the triangle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Size The size of the triangle.
     */
    void Triangle(const f32m<D> &p_Transform, f32 p_Size);

    /**
     * @brief Draw a unit square centered at the origin.
     *
     * The square will be affected by the current transformation state.
     */
    void Square();

    /**
     * @brief Draw a square with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Square(const f32m<D> &p_Transform);

    /**
     * @brief Draw a square with the specified dimensions.
     *
     * @param p_Dimensions The dimensions of the square.
     */
    void Square(const f32v2 &p_Dimensions);

    /**
     * @brief Draw a square with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The dimensions of the square.
     */
    void Square(const f32m<D> &p_Transform, const f32v2 &p_Dimensions);

    /**
     * @brief Draw a square with the specified dimensions.
     *
     * @param p_Size The size of the square.
     */
    void Square(f32 p_Size);

    /**
     * @brief Draw a square with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Size The size of the square.
     */
    void Square(const f32m<D> &p_Transform, f32 p_Size);

    /**
     * @brief Draw a regular n-sided polygon centered at the origin.
     *
     * @param p_Sides The number of sides of the polygon.
     */
    void NGon(u32 p_Sides);

    /**
     * @brief Draw a regular n-sided polygon with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Sides The number of sides of the polygon.
     */
    void NGon(const f32m<D> &p_Transform, u32 p_Sides);

    /**
     * @brief Draw a regular n-sided polygon centered at the origin.
     *
     * @param p_Sides The number of sides of the polygon.
     * @param p_Dimensions The dimensions of the polygon.
     */
    void NGon(u32 p_Sides, const f32v2 &p_Dimensions);

    /**
     * @brief Draw a regular n-sided polygon with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Sides The number of sides of the polygon.
     * @param p_Dimensions The dimensions of the polygon.
     */
    void NGon(const f32m<D> &p_Transform, u32 p_Sides, const f32v2 &p_Dimensions);

    /**
     * @brief Draw a regular n-sided polygon centered at the origin.
     *
     * @param p_Sides The number of sides of the polygon.
     * @param p_Size The size of the polygon.
     */
    void NGon(u32 p_Sides, f32 p_Size);

    /**
     * @brief Draw a regular n-sided polygon with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the polygon. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Sides The number of sides of the polygon.
     * @param p_Size The size of the polygon.
     */
    void NGon(const f32m<D> &p_Transform, u32 p_Sides, f32 p_Size);

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
    void Polygon(TKit::Span<const f32v2> p_Vertices);

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
    void Polygon(const f32m<D> &p_Transform, TKit::Span<const f32v2> p_Vertices);

    /**
     * @brief Draw a unit circle centered at the origin.
     *
     * The circle will be affected by the current transformation state.
     *
     * The following is specified through the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const CircleOptions &p_Options = {});

    /**
     * @brief Draw a circle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     *
     * The following is specified through the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const f32m<D> &p_Transform, const CircleOptions &p_Options = {});

    /**
     * @brief Draw a unit circle centered at the origin.
     *
     * The circle will be affected by the current transformation state.
     *
     * @param p_Dimensions The dimensions of the circle.
     *
     * The following is specified through the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const f32v2 &p_Dimensions, const CircleOptions &p_Options = {});

    /**
     * @brief Draw a circle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The dimensions of the circle.
     *
     * The following is specified through the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const f32m<D> &p_Transform, const f32v2 &p_Dimensions, const CircleOptions &p_Options = {});

    /**
     * @brief Draw a unit circle centered at the origin.
     *
     * The circle will be affected by the current transformation state.
     *
     * @param p_Diameter The diameter of the circle.
     *
     * The following is specified through the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(f32 p_Diameter, const CircleOptions &p_Options = {});

    /**
     * @brief Draw a circle with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the circle. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Diameter The diameter of the circle.
     *
     * The following is specified through the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     */
    void Circle(const f32m<D> &p_Transform, f32 p_Diameter, const CircleOptions &p_Options = {});

    /**
     * @brief Draw a unit stadium shape (a rectangle with semicircular ends).
     *
     * The stadium will be affected by the current transformation state.
     */
    void Stadium();

    /**
     * @brief Draw a stadium with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the stadium. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Stadium(const f32m<D> &p_Transform);

    /**
     * @brief Draw a stadium shape with the given length and diameter.
     *
     * @param p_Length The length of the rectangular part of the stadium.
     * @param p_Diameter The diameter of the semicircular ends.
     */
    void Stadium(f32 p_Length, f32 p_Diameter);

    /**
     * @brief Draw a stadium shape with the given parameters and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the stadium. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Length The length of the rectangular part of the stadium.
     * @param p_Diameter The diameter of the semicircular ends.
     */
    void Stadium(const f32m<D> &p_Transform, f32 p_Length, f32 p_Diameter);

    /**
     * @brief Draw a unit rounded square centered at the origin.
     *
     * The rounded square will be affected by the current transformation state.
     */
    void RoundedSquare();

    /**
     * @brief Draw a rounded square with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the rounded square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void RoundedSquare(const f32m<D> &p_Transform);

    /**
     * @brief Draw a rounded square with given dimensions and corner diameter.
     *
     * @param p_Dimensions The width and height of the square.
     * @param p_Diameter The diameter of the corners.
     */
    void RoundedSquare(const f32v2 &p_Dimensions, f32 p_Diameter);

    /**
     * @brief Draw a rounded square with given dimensions, corner diameter, and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the rounded square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The width and height of the square.
     * @param p_Diameter The diameter of the corners.
     */
    void RoundedSquare(const f32m<D> &p_Transform, const f32v2 &p_Dimensions, f32 p_Diameter);

    /**
     * @brief Draw a rounded square with given dimensions and corner diameter.
     *
     * @param p_Size The size of the square.
     * @param p_Diameter The diameter of the corners.
     */
    void RoundedSquare(f32 p_Size, f32 p_Diameter);

    /**
     * @brief Draw a rounded square with given dimensions, corner diameter, and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the rounded square. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Size The size of the square.
     * @param p_Diameter The diameter of the corners.
     */
    void RoundedSquare(const f32m<D> &p_Transform, f32 p_Size, f32 p_Diameter);

    // Actually, 2D meshes could be used in 3D as well. This feature is not implemented yet. If you want a 2D mesh in a
    // 3D context, you must load such mesh as a 3D mesh

    /**
     * @brief Draw a mesh model.
     *
     * @param p_Mesh The mesh model to draw.
     */
    void Mesh(const Onyx::Mesh<D> &p_Mesh);

    /**
     * @brief Draw a mesh model with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the model. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Mesh The mesh model to draw.
     */
    void Mesh(const f32m<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh);

    /**
     * @brief Draw a mesh model.
     *
     * @param p_Mesh The mesh model to draw.
     */
    void Mesh(const Onyx::Mesh<D> &p_Mesh, const f32v<D> &p_Dimensions);

    /**
     * @brief Draw a mesh model with the specified transformation.
     *
     * @param p_Transform The transformation matrix to apply to the mesh. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Mesh The mesh model to draw.
     */
    void Mesh(const f32m<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh, const f32v<D> &p_Dimensions);

    /**
     * @brief Pushes the current context state onto the stack.
     *
     * Allows you to save the current state and restore it later with `Pop()`.
     */
    void Push();

    /**
     * @brief Pushes the specified context state onto the stack.
     *
     * Allows you to save the current state and restore it later with `Pop()`.
     */
    void Push(const RenderState<D> &p_State);

    /**
     * @brief Pops the last saved context state from the stack.
     *
     * Restores the context state to the last state saved with `Push()`.
     */
    void Pop();

    void AddFlags(RenderStateFlags p_Flags);
    void RemoveFlags(RenderStateFlags p_Flags);

    /**
     * @brief Enables or disables filling of subsequent shapes.
     *
     * @param p_Enable Set to true to enable filling, false to disable.
     */
    void Fill(bool p_Enable = true);

    /**
     * @brief Set the fill color for subsequent shapes.
     *
     * Enables filling if it was previously disabled.
     *
     * @param p_Color The color to use for filling.
     */
    void Fill(const Color &p_Color);

    /**
     * @brief Set the fill color for subsequent shapes.
     *
     * @param p_ColorArgs Arguments to construct the fill color.
     */
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Fill(ColorArgs &&...p_ColorArgs)
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Fill(color);
    }

    /**
     * @brief Set the alpha (transparency) value for subsequent shapes.
     *
     * @param p_Alpha The alpha value between 0.0 (fully transparent) and 1.0 (fully opaque).
     */
    void Alpha(f32 p_Alpha);

    /**
     * @brief Set the alpha (transparency) value for subsequent shapes.
     *
     * @param p_Alpha The alpha value between 0 and 255.
     */
    void Alpha(u8 p_Alpha);

    /**
     * @brief Set the alpha (transparency) value for subsequent shapes.
     *
     * @param p_Alpha The alpha value between 0 and 255.
     */
    void Alpha(u32 p_Alpha);

    /**
     * @brief Enables or disables outlining of subsequent shapes.
     *
     * @param p_Enable Set to true to enable outlining, false to disable.
     */
    void Outline(bool p_Enable = true);

    /**
     * @brief Set the outline color for subsequent shapes.
     *
     * Enables outlining if it was previously disabled.
     *
     * @param p_Color The color to use for outlining.
     */
    void Outline(const Color &p_Color);

    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Outline(ColorArgs &&...p_ColorArgs)
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Outline(color);
    }

    /**
     * @brief Set the outline width for subsequent shapes.
     *
     * @param p_Width The width of the outline.
     */
    void OutlineWidth(f32 p_Width);

    /**
     * @brief Set the material properties for subsequent shapes.
     *
     * @param p_Material The material data to use.
     */
    void Material(const MaterialData<D> &p_Material);

    /**
     * @brief Share this thread's state stack to all other threads.
     *
     * Useful when a global state is set from the main thread and is expected to be inherited by all threads. If
     * `Push()` was called n times, all threads will have to `Pop()` n times as well. Take into account this will
     * override previous `Pus()`/`Pop()` calls for the other threads. This method is not thread-safe.
     *
     * @param p_ThreadCount The number of threads affected. The application may not be using all of the threads given by
     * `ONYX_MAX_THREADS`. Because this method shares the whole stack, this number should reflect how many threads are
     * actually in use by your application. Failing to do so may result in assert errors from other threads state. The
     * calling thread's index is thus expected to be lower than the count, although it is not required.
     */
    void ShareStateStack(u32 p_ThreadCount = ONYX_MAX_THREADS);

    /**
     * @brief Share this thread's current state to all other threads.
     *
     * Useful when a global state is set from the main thread and is expected to be inherited by all threads. The other
     * threads will not inherit this thread's `Push()`/`Pop()` calls. This method is not thread-safe.
     *
     * @param p_ThreadCount The number of threads affected. The application may not be using all of the threads given by
     * `ONYX_MAX_THREADS`. This number may reflect how many threads are actually in use by your application to avoid
     * extra work. The calling thread's index is thus expected to be lower than the count, although it is not required.
     */
    void ShareCurrentState(u32 p_ThreadCount = ONYX_MAX_THREADS);

    /**
     * @brief Sets the specified state to all threads.
     *
     * Useful when a global state is set from the main thread and is expected to be inherited by all threads. The other
     * threads will not inherit this thread's `Push()`/`Pop()` calls. This method is not thread-safe.
     *
     * @param p_State The state all threads will have.
     * @param p_ThreadCount The number of threads affected. The application may not be using all of the threads given by
     * `ONYX_MAX_THREADS`. This number may reflect how many threads are actually in use by your application to avoid
     * extra work. The calling thread's index is thus expected to be lower than the count, although it is not required.
     *
     */
    void ShareState(const RenderState<D> &p_State, u32 p_ThreadCount = ONYX_MAX_THREADS);

    /**
     * @brief Get the current rendering state.
     *
     * @return A constant reference to the current `RenderState`.
     */
    const RenderState<D> &GetCurrentState() const;

    /**
     * @brief Get the current rendering state.
     *
     * @return A reference to the current `RenderState`.
     */
    RenderState<D> &GetCurrentState();

    void SetCurrentState(const RenderState<D> &p_State);

    const Renderer<D> &GetRenderer() const
    {
        return m_Renderer;
    }
    Renderer<D> &GetRenderer()
    {
        return m_Renderer;
    }

  protected:
    void updateState();
    RenderState<D> *getState();

    template <Dimension PDim>
    void drawPrimitive(const RenderState<D> &p_State, const f32m<D> &p_Transform, u32 p_PrimitiveIndex);
    template <Dimension PDim>
    void drawPrimitive(const RenderState<D> &p_State, const f32m<D> &p_Transform, u32 p_PrimitiveIndex,
                       const f32v<PDim> &p_Dimensions);

    void drawPolygon(const RenderState<D> &p_State, const f32m<D> &p_Transform, TKit::Span<const f32v2> p_Vertices);

    void drawCircle(const RenderState<D> &p_State, const f32m<D> &p_Transform, const CircleOptions &p_Options);
    void drawCircle(const RenderState<D> &p_State, const f32m<D> &p_Transform, const CircleOptions &p_Options,
                    const f32v2 &p_Dimensions);

    void drawChildCircle(const RenderState<D> &p_State, f32m<D> p_Transform, const f32v<D> &p_Position,
                         const CircleOptions &p_Options, Detail::DrawFlags p_Flags);
    void drawChildCircle(const RenderState<D> &p_State, f32m<D> p_Transform, const f32v<D> &p_Position, f32 p_Diameter,
                         const CircleOptions &p_Options, Detail::DrawFlags p_Flags);

    void drawStadiumMoons(const RenderState<D> &p_State, const f32m<D> &p_Transform, Detail::DrawFlags p_Flags);
    void drawStadiumMoons(const RenderState<D> &p_State, const f32m<D> &p_Transform, f32 p_Length, f32 p_Diameter,
                          Detail::DrawFlags p_Flags);

    void drawStadium(const RenderState<D> &p_State, const f32m<D> &p_Transform);
    void drawStadium(const RenderState<D> &p_State, const f32m<D> &p_Transform, f32 p_Length, f32 p_Diameter);

    void drawRoundedSquareMoons(const RenderState<D> &p_State, const f32m<D> &p_Transform, const f32v2 &p_Dimension,
                                f32 p_Diameter, Detail::DrawFlags p_Flags);

    void drawRoundedSquare(const RenderState<D> &p_State, const f32m<D> &p_Transform);
    void drawRoundedSquare(const RenderState<D> &p_State, const f32m<D> &p_Transform, const f32v2 &p_Dimension,
                           f32 p_Diameter);

    void drawMesh(const RenderState<D> &p_State, const f32m<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh);
    void drawMesh(const RenderState<D> &p_State, const f32m<D> &p_Transform, const Onyx::Mesh<D> &p_Mesh,
                  const f32v<D> &p_Dimensions);

    struct alignas(TKIT_CACHE_LINE_SIZE) Stack
    {
        RenderStateStack<D> States;
        RenderState<D> *Current;
    };

    TKit::Array<Stack, ONYX_MAX_THREADS> m_StateStack;
    Detail::Renderer<D> m_Renderer;
};
} // namespace Detail

/**
 * @brief The `RenderContext` class is the primary way of communicating with the Onyx API.
 *
 * It is a high-level API that allows the user to draw shapes and meshes in a simple immediate mode
 * fashion. The draw calls are recorded, sent to the gpu and translated to vulkan draw calls when appropiate.
 *
 * All public interface is thread-safe unless the documentation says otherwise. Each thread has a separate state, with
 * support for up to `ONYX_MAX_THREADS`. Thread-unsafe methods to synchronize state between threads are available as
 * well. Multi-threaded use is encouraged, as the API was designed with core-friendliness in mind.
 *
 * The following is a set of properties of the `RenderContext` you must take into account when using it:
 *
 * - `RenderContext`s have their own coordinate system, defined by the axes transform that can be found in the context's
 * state and which can be modified through its API to affect the coordinates in which subsequent shapes are drawn. You
 * must take this into account when communicating to other systems unaware of these coordinates, such as cameras. All
 * world related camera methods have an optional parameter where you can specify the axes transform of (potentially) a
 * context, so that you get accurate coordinates when querying for the world mouse position in a `RenderContext`, for
 * instance.
 *
 * - While it is possible to use `RenderContext` in pretty much any callback, it is recommended to use it in the
 * `OnUpdate()` callbacks, if using an application, or inside the body of the while loop if using a simple window. You
 * should also be consistent with which callback you use, and stick to calling the API from that callback only. Failing
 * to do so may result in only some of the things you draw popping on the screen, or worse.
 *
 * - The `RenderContext` is mostly immediate mode. All mutations to its state can be reset with the `Flush()`
 * method, which is recommended to be called at the beginning of each frame in case your scene consists of moving
 * objects. If `Flush()` is not called, the context will keep its state and the device will keep drawing the same
 * geometry every frame. The context will make sure not to re-upload the data to the gpu in case it is to re-use its
 * state.
 *
 * - Windows support multiple `RenderContext` objects, and it is advised to group your objects by frequency of update,
 * and have a `RenderContext` per group. Sending data to the device can be a time consuming operation and a real
 * bottleneck. If your data does not change, use a static `RenderContext` to render it, by calling `Flush()` once and
 * submitting draw commands.
 *
 * - Once recorded and submitted (this step happens automatically once the `RenderContext` sends the data to the
 * device), to re-draw the contents of a `RenderContext` it is necessary to flush it and re-record the commands.
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
 * - Note that objects drawn in the scene inherit the state the axes were in when the draw command was issued.
 * This means that every object drawn will have its own, dedicated parent axes transform.
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
     * Axes are the global coordinates of the `RenderContext`, which can be transformed at will to affect the whole
     * scene (from where the transformation happened) when enabled.
     *
     * Draw the X and Y axes with the specified thickness and size.
     *
     * The following is specified through the `AxesOptions` struct:
     *
     * @param p_Thickness The thickness of the axes lines.
     * @param p_Size The length of the axes.
     */
    void Axes(const AxesOptions<D2> &p_Options = {});

    /**
     * @brief Rotates subsequent shapes by the given angle.
     *
     * @param p_Angle The rotation angle in radians.
     */
    void Rotate(f32 p_Angle);

    /**
     * @brief Draw a line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void Line(const f32v2 &p_Start, const f32v2 &p_End, f32 p_Thickness = 0.01f);

    /**
     * @brief Draw a line strip through the given points.
     *
     * @param p_Points A span of points defining the line strip.
     * @param p_Thickness The thickness of the line.
     */
    void LineStrip(TKit::Span<const f32v2> p_Points, f32 p_Thickness = 0.01f);

    /**
     * @brief Draw a rounded line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void RoundedLine(const f32v2 &p_Start, const f32v2 &p_End, f32 p_Thickness = 0.01f);
};

template <> class ONYX_API RenderContext<D3> final : public Detail::IRenderContext<D3>
{
  public:
    using IRenderContext<D3>::IRenderContext;
    using IRenderContext<D3>::Transform;

    /**
     * @brief Render the coordinate axes for visualization.
     *
     * Axes are the global coordinates of the `RenderContext`, which can be transformed at will to affect the whole
     * scene (from where the transformation happened) when enabled.
     *
     * Draw the X, Y and Z axes with the specified thickness and size.
     *
     * The following is specified through the `AxesOptions` struct:
     *
     * @param p_Thickness The thickness of the axes lines.
     * @param p_Size The length of the axes.
     */
    void Axes(const AxesOptions<D3> &p_Options = {});

    /**
     * @brief Transforms subsequent shapes by the given translation, scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The scaling vector.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void Transform(const f32v3 &p_Translation, const f32v3 &p_Scale, const f32v3 &p_Rotation);

    /**
     * @brief Transforms subsequent shapes by the given translation, uniform scale, and rotation angles.
     *
     * @param p_Translation The translation vector.
     * @param p_Scale The uniform scaling factor.
     * @param p_Rotation The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void Transform(const f32v3 &p_Translation, f32 p_Scale, const f32v3 &p_Rotation);

    /**
     * @brief Translate subsequent shapes along the Z-axis.
     *
     * @param p_Z The translation distance along the Z-axis.
     */
    void TranslateZ(f32 p_Z);

    /**
     * @brief Set the translation of subsequent shapes along the Z-axis.
     *
     * This sets a translation along the Z-axis to all subsequent draw calls.
     *
     * @param p_Z The translation distance along the Z-axis
     */
    void SetTranslationZ(f32 p_Z);

    /**
     * @brief Scale subsequent shapes along the Z-axis.
     *
     * @param p_Z The scaling factor along the Z-axis.
     */
    void ScaleZ(f32 p_Z);

    /**
     * @brief Rotates subsequent shapes by the given quaternion.
     *
     * @param p_Quaternion The quaternion representing the rotation.
     */
    void Rotate(const f32q &p_Quaternion);

    /**
     * @brief Rotates subsequent shapes by the given Euler angles.
     *
     * @param p_Angles The rotation angles (in radians) around the X, Y, and Z axes.
     */
    void Rotate(const f32v3 &p_Angles);

    /**
     * @brief Rotates subsequent shapes by the given angle around the specified axis.
     *
     * @param p_Angle The rotation angle in radians.
     * @param p_Axis The axis to rotate around.
     */
    void Rotate(f32 p_Angle, const f32v3 &p_Axis);

    /**
     * @brief Rotates subsequent shapes around the X-axis.
     *
     * @param p_X The rotation angle around the X-axis in radians.
     */
    void RotateX(f32 p_X);

    /**
     * @brief Rotates subsequent shapes around the Y-axis.
     *
     * @param p_Y The rotation angle around the Y-axis in radians.
     */
    void RotateY(f32 p_Y);

    /**
     * @brief Rotates subsequent shapes around the Z-axis.
     *
     * @param p_Z The rotation angle around the Z-axis in radians.
     */
    void RotateZ(f32 p_Z);

    /**
     * @brief Draw a line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void Line(const f32v3 &p_Start, const f32v3 &p_End, const LineOptions &p_Options = {});

    /**
     * @brief Draw a line strip through the given points.
     *
     * @param p_Points A span of points defining the line strip.
     * @param p_Thickness The thickness of the line.
     */
    void LineStrip(TKit::Span<const f32v3> p_Points, const LineOptions &p_Options = {});

    /**
     * @brief Draw a rounded line between two points with the specified thickness.
     *
     * @param p_Start The starting point of the line.
     * @param p_End The ending point of the line.
     * @param p_Thickness The thickness of the line.
     */
    void RoundedLine(const f32v3 &p_Start, const f32v3 &p_End, const LineOptions &p_Options = {});

    /**
     * @brief Draw a unit cube centered at the origin.
     *
     * The cube will be affected by the current transformation state.
     */
    void Cube();

    /**
     * @brief Draw a cube with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     */
    void Cube(const f32m4 &p_Transform);

    /**
     * @brief Draw a cube with the specified dimensions.
     *
     * @param p_Dimensions The width, height, and depth of the cube.
     */
    void Cube(const f32v3 &p_Dimensions);

    /**
     * @brief Draw a cube with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The width, height, and depth of the cube.
     */
    void Cube(const f32m4 &p_Transform, const f32v3 &p_Dimensions);

    /**
     * @brief Draw a cube with the specified dimensions.
     *
     * @param p_Size The size of the cube.
     */
    void Cube(f32 p_Size);

    /**
     * @brief Draw a cube with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Size The size of the cube.
     */
    void Cube(const f32m4 &p_Transform, f32 p_Size);

    /**
     * @brief Draw a unit cylinder centered at the origin.
     *
     * The cylinder will be affected by the current transformation state.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a cylinder with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the cylinder. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(const f32m4 &p_Transform, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a cylinder with the specified dimensions.
     *
     * @param p_Dimensions The width, height, and depth of the cylinder.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(const f32v3 &p_Dimensions, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a cylinder with the specified transformation matrix and dimensions.
     *
     * @param p_Transform The transformation matrix to apply to the cylinder. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The width, height, and depth of the cylinder.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(const f32m4 &p_Transform, const f32v3 &p_Dimensions, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a cylinder shape with the given length and diameter.
     *
     * @param p_Length The length of the cylindrical part of the cylinder.
     * @param p_Diameter The diameter of the hemispherical ends.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(f32 p_Length, f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a cylinder shape with the given parameters and transformation.
     *
     * @param p_Length The length of the cylindrical part of the cylinder.
     * @param p_Diameter The diameter of the hemispherical ends.
     * @param p_Transform The transformation matrix to apply to the cylinder. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Cylinder(const f32m4 &p_Transform, f32 p_Length, f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a unit sphere centered at the origin.
     *
     * The sphere will be affected by the current transformation state.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a sphere with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the sphere. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(const f32m4 &p_Transform, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a sphere shape with the given dimensions.
     *
     * @param p_Dimensions The width, height, and depth of the sphere.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(const f32v3 &p_Dimensions, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a sphere shape with the given dimensions and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the sphere. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Dimensions The width, height, and depth of the sphere.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(const f32m4 &p_Transform, const f32v3 &p_Dimensions, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a sphere shape with the given dimensions.
     *
     * @param p_Diameter The diameter of the sphere.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a sphere shape with the given dimensions and transformation.
     *
     * @param p_Transform The transformation matrix to apply to the sphere. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Diameter The diameter of the sphere.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Sphere(const f32m4 &p_Transform, f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a unit capsule centered at the origin.
     *
     * The capsule will be affected by the current transformation state.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Capsule(Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a capsule with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the capsule. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Capsule(const f32m4 &p_Transform, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a capsule shape with the given length and diameter.
     *
     * @param p_Length The length of the cylindrical part of the capsule.
     * @param p_Diameter The diameter of the hemispherical ends.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Capsule(f32 p_Length, f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a capsule shape with the given parameters and transformation.
     *
     * @param p_Length The length of the cylindrical part of the capsule.
     * @param p_Diameter The diameter of the hemispherical ends.
     * @param p_Transform The transformation matrix to apply to the capsule. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void Capsule(const f32m4 &p_Transform, f32 p_Length, f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a unit rounded cube centered at the origin.
     *
     * The rounded cube will be affected by the current transformation state.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a rounded cube with the specified transformation matrix.
     *
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(const f32m4 &p_Transform, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a rounded cube with given dimensions and corner diameter.
     *
     * @param p_Dimensions The width, height, and depth of the cube.
     * @param p_Diameter The diameter of the corners.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(const f32v3 &p_Dimensions, f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a rounded cube with given dimensions, corner diameter, and transformation.
     *
     * @param p_Dimensions The width, height, and depth of the cube.
     * @param p_Diameter The diameter of the corners.
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(const f32m4 &p_Transform, const f32v3 &p_Dimensions, f32 p_Diameter,
                     Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a rounded cube with given dimensions and corner diameter.
     *
     * @param p_Size The size of the cube.
     * @param p_Diameter The diameter of the corners.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(f32 p_Size, f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Draw a rounded cube with given dimensions, corner diameter, and transformation.
     *
     * @param p_Size The size of the cube.
     * @param p_Diameter The diameter of the corners.
     * @param p_Transform The transformation matrix to apply to the rounded cube. This transformation will be applied
     * extrinsically, on top of the current cummulated transformations.
     * @param p_Res The shape vertex resolution. Can be low, medium or high. Higher resolutions are more expensive.
     */
    void RoundedCube(const f32m4 &p_Transform, f32 p_Size, f32 p_Diameter, Resolution p_Res = Resolution::Medium);

    /**
     * @brief Set the color of the light for subsequent lighting calculations.
     *
     * @param p_Color The color of the light.
     */
    void LightColor(const Color &p_Color);
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void LightColor(ColorArgs &&...p_ColorArgs)
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        LightColor(color);
    }

    /**
     * @brief Set the ambient light color.
     *
     * @param p_Color The ambient light color.
     */
    void AmbientColor(const Color &p_Color);
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void AmbientColor(ColorArgs &&...p_ColorArgs)
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
    void AmbientIntensity(f32 p_Intensity);

    /**
     * @brief Adds a directional light to the scene.
     *
     * @param p_Light The directional light object.
     */
    void DirectionalLight(Onyx::DirectionalLight p_Light);

    /**
     * @brief Adds a directional light to the scene with the specified direction and intensity.
     *
     * @param p_Direction The direction of the light.
     * @param p_Intensity The intensity of the light.
     */
    void DirectionalLight(const f32v3 &p_Direction, f32 p_Intensity = 1.f);

    /**
     * @brief Adds a point light to the scene.
     *
     * @param p_Light The point light object.
     */
    void PointLight(Onyx::PointLight p_Light);

    /**
     * @brief Adds a point light to the scene at the current position.
     *
     * @param p_Radius The radius of the light's influence.
     * @param p_Intensity The intensity of the light.
     */
    void PointLight(f32 p_Radius = 1.f, f32 p_Intensity = 1.f);

    /**
     * @brief Adds a point light to the scene at the specified position.
     *
     * @param p_Position The position of the light.
     * @param p_Radius The radius of the light's influence.
     * @param p_Intensity The intensity of the light.
     */
    void PointLight(const f32v3 &p_Position, f32 p_Radius = 1.f, f32 p_Intensity = 1.f);

    /**
     * @brief Set the diffuse contribution factor for lighting calculations.
     *
     * @param p_Contribution The diffuse contribution factor.
     */
    void DiffuseContribution(f32 p_Contribution);

    /**
     * @brief Set the specular contribution factor for lighting calculations.
     *
     * @param p_Contribution The specular contribution factor.
     */
    void SpecularContribution(f32 p_Contribution);

    /**
     * @brief Set the specular sharpness for lighting calculations.
     *
     * @param p_Sharpness The specular sharpness factor.
     */
    void SpecularSharpness(f32 p_Sharpness);

  private:
    void drawChildSphere(const RenderState<D3> &p_State, f32m4 p_Transform, const f32v3 &p_Position, Resolution p_Res,
                         Detail::DrawFlags p_Flags);
    void drawChildSphere(const RenderState<D3> &p_State, f32m4 p_Transform, const f32v3 &p_Position, f32 p_Diameter,
                         Resolution p_Res, Detail::DrawFlags p_Flags);

    void drawCapsule(const RenderState<D3> &p_State, const f32m4 &p_Transform, Resolution p_Res);
    void drawCapsule(const RenderState<D3> &p_State, const f32m4 &p_Transform, f32 p_Length, f32 p_Diameter,
                     Resolution p_Res);

    void drawRoundedCubeMoons(const RenderState<D3> &p_State, const f32m4 &p_Transform, const f32v3 &p_Dimensions,
                              f32 p_Diameter, Resolution p_Res, Detail::DrawFlags p_Flags);

    void drawRoundedCube(const RenderState<D3> &p_State, const f32m4 &p_Transform, Resolution p_Res);
    void drawRoundedCube(const RenderState<D3> &p_State, const f32m4 &p_Transform, const f32v3 &p_Dimensions,
                         f32 p_Diameter, Resolution p_Res);
};
TKIT_COMPILER_WARNING_IGNORE_POP()
} // namespace Onyx
