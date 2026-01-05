#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/property/options.hpp"
#include "onyx/state/state.hpp"
#include "onyx/rendering/renderer.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
class Window;
} // namespace Onyx

namespace Onyx::Detail
{
template <Dimension D> using RenderStateStack = TKit::Array<RenderState<D>, MaxRenderStateDepth>;
template <Dimension D> class IRenderContext
{
    TKIT_NON_COPYABLE(IRenderContext)
  public:
    IRenderContext(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

    void Flush(const u32 p_ThreadCount = MaxThreads);

    void Transform(const f32m<D> &p_Transform);
    void Transform(const f32v<D> &p_Translation, const f32v<D> &p_Scale, const rot<D> &p_Rotation);
    void Transform(const f32v<D> &p_Translation, f32 p_Scale, const rot<D> &p_Rotation);

    void Translate(const f32v<D> &p_Translation);
    void SetTranslation(const f32v<D> &p_Translation);

    void Scale(const f32v<D> &p_Scale);
    void Scale(f32 p_Scale);

    void TranslateX(f32 p_X);
    void TranslateY(f32 p_Y);

    void SetTranslationX(f32 p_X);
    void SetTranslationY(f32 p_Y);

    void ScaleX(f32 p_X);
    void ScaleY(f32 p_Y);

    void StaticMesh(Mesh p_Mesh);
    void StaticMesh(Mesh p_Mesh, const f32m<D> &p_Transform);

    void Circle(const CircleOptions &p_Options = {});
    void Circle(const f32m<D> &p_Transform, const CircleOptions &p_Options = {});

    void Line(Mesh p_Mesh, const f32v<D> &p_Start, const f32v<D> &p_End, f32 p_Thickness = 0.1f);
    void Axes(Mesh p_Mesh, const AxesOptions &p_Options = {});

    void Push();
    void Push(const RenderState<D> &p_State);

    void Pop();

    void AddFlags(RenderStateFlags p_Flags);
    void RemoveFlags(RenderStateFlags p_Flags);

    void Fill(bool p_Enable = true);
    void Fill(const Color &p_Color);

    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Fill(ColorArgs &&...p_ColorArgs)
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Fill(color);
    }

    void Alpha(f32 p_Alpha);
    void Alpha(u8 p_Alpha);
    void Alpha(u32 p_Alpha);

    void Outline(bool p_Enable = true);
    void Outline(const Color &p_Color);

    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void Outline(ColorArgs &&...p_ColorArgs)
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        Outline(color);
    }

    void OutlineWidth(f32 p_Width);
    void Material(const MaterialData<D> &p_Material);

    /**
     * @brief Share this thread's state stack to all other threads.
     *
     * Useful when a global state is set from the main thread and is expected to be inherited by all threads. If
     * `Push()` was called n times, all threads will have to `Pop()` n times as well. Take into account this will
     * override previous `Push()`/`Pop()` calls for the other threads. This method is not thread-safe.
     *
     * @param p_ThreadCount The number of threads affected. The application may not be using all of the threads given by
     * `MaxThreads`. Because this method shares the whole stack, this number should reflect how many threads are
     * actually in use by your application. Failing to do so may result in assert errors from other threads state. The
     * calling thread's index is thus expected to be lower than the count, although it is not required.
     */
    void ShareStateStack(u32 p_ThreadCount = MaxThreads);

    /**
     * @brief Share this thread's current state to all other threads.
     *
     * Useful when a global state is set from the main thread and is expected to be inherited by all threads. The other
     * threads will not inherit this thread's `Push()`/`Pop()` calls. This method is not thread-safe.
     *
     * @param p_ThreadCount The number of threads affected. The application may not be using all of the threads given by
     * `MaxThreads`. This number may reflect how many threads are actually in use by your application to avoid
     * extra work. The calling thread's index is thus expected to be lower than the count, although it is not required.
     */
    void ShareCurrentState(u32 p_ThreadCount = MaxThreads);

    /**
     * @brief Sets the specified state to all threads.
     *
     * Useful when a global state is set from the main thread and is expected to be inherited by all threads. The other
     * threads will not inherit this thread's `Push()`/`Pop()` calls. This method is not thread-safe.
     *
     * @param p_State The state all threads will have.
     * @param p_ThreadCount The number of threads affected. The application may not be using all of the threads given by
     * `MaxThreads`. This number may reflect how many threads are actually in use by your application to avoid
     * extra work. The calling thread's index is thus expected to be lower than the count, although it is not required.
     *
     */
    void ShareState(const RenderState<D> &p_State, u32 p_ThreadCount = MaxThreads);

    const RenderState<D> &GetCurrentState() const;

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

    struct alignas(TKIT_CACHE_LINE_SIZE) Stack
    {
        RenderStateStack<D> States{};
        RenderState<D> *Current{};
    };

    TKit::FixedArray<Stack, ONYX_MAX_THREADS> m_StateStack{};
    Detail::Renderer<D> m_Renderer;
};
} // namespace Onyx::Detail

namespace Onyx
{
/**
 * @brief The `RenderContext` class is the primary way of communicating with the Onyx API.
 *
 * It is a high-level API that allows the user to draw shapes and meshes in a simple immediate mode
 * fashion. The draw calls are recorded, sent to the gpu and translated to vulkan draw calls when appropiate.
 *
 * All public interface is thread-safe unless the documentation says otherwise. Each thread has a separate state, with
 * support for up to `MaxThreads`. Thread-unsafe methods to synchronize state between threads are available as
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
 * - Onyx renderer uses batch rendering to optimize draw calls. This means that in some cases, the order in which shapes
 * are drawn may not be respected.
 *
 * - State changes to the context affect subsequent shapes. Calling `Transform()`, `Scale()` or a similar method will
 * affect all entities drawn from that point on. Transform matrices passed directly when drawing an entity are not
 * persisted.
 *
 */
template <Dimension D> class RenderContext;

template <> class RenderContext<D2> final : public Detail::IRenderContext<D2>
{
  public:
    using IRenderContext<D2>::IRenderContext;
    void Rotate(f32 p_Angle);
};

template <> class RenderContext<D3> final : public Detail::IRenderContext<D3>
{
  public:
    using IRenderContext<D3>::IRenderContext;
    using IRenderContext<D3>::Transform;

    void Transform(const f32v3 &p_Translation, const f32v3 &p_Scale, const f32v3 &p_Rotation);
    void Transform(const f32v3 &p_Translation, f32 p_Scale, const f32v3 &p_Rotation);
    void TranslateZ(f32 p_Z);

    void SetTranslationZ(f32 p_Z);
    void ScaleZ(f32 p_Z);

    void Rotate(const f32q &p_Quaternion);
    void Rotate(const f32v3 &p_Angles);
    void Rotate(f32 p_Angle, const f32v3 &p_Axis);

    void RotateX(f32 p_X);
    void RotateY(f32 p_Y);
    void RotateZ(f32 p_Z);

    void LightColor(const Color &p_Color);
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void LightColor(ColorArgs &&...p_ColorArgs)
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        LightColor(color);
    }

    void AmbientColor(const Color &p_Color);
    template <typename... ColorArgs>
        requires std::constructible_from<Color, ColorArgs...>
    void AmbientColor(ColorArgs &&...p_ColorArgs)
    {
        const Color color(std::forward<ColorArgs>(p_ColorArgs)...);
        AmbientColor(color);
    }

    void AmbientIntensity(f32 p_Intensity);

    void DirectionalLight(Onyx::DirectionalLight p_Light);
    void DirectionalLight(const f32v3 &p_Direction, f32 p_Intensity = 1.f);

    void PointLight(Onyx::PointLight p_Light);
    void PointLight(f32 p_Radius = 1.f, f32 p_Intensity = 1.f);
    void PointLight(const f32v3 &p_Position, f32 p_Radius = 1.f, f32 p_Intensity = 1.f);

    void DiffuseContribution(f32 p_Contribution);
    void SpecularContribution(f32 p_Contribution);
    void SpecularSharpness(f32 p_Sharpness);
};
} // namespace Onyx
