#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/property/options.hpp"
#include "onyx/asset/mesh.hpp"
#include "onyx/core/limits.hpp"
#include "onyx/app/window.hpp"
#include "vkit/resource/host_buffer.hpp"

namespace Onyx
{
using RenderStateFlags = u8;
enum RenderStateFlagBit : RenderStateFlags
{
    RenderStateFlag_Fill = 1 << 0,
    RenderStateFlag_Outline = 1 << 1,
};

template <Dimension D> struct RenderState;

template <> struct RenderState<D2>
{
    TKIT_REFLECT_DECLARE(RenderState)
    TKIT_YAML_SERIALIZE_DECLARE(RenderState)

    f32m3 Transform = f32m3::Identity();
    Color FillColor = Color::WHITE;
    Color OutlineColor = Color::WHITE;
    f32 OutlineWidth = 0.1f;
    RenderStateFlags Flags = RenderStateFlag_Fill;
};

template <> struct RenderState<D3>
{
    TKIT_REFLECT_DECLARE(RenderState)
    TKIT_YAML_SERIALIZE_DECLARE(RenderState)

    f32m4 Transform = f32m4::Identity();
    Color FillColor = Color::WHITE;
    Color OutlineColor = Color::WHITE;
    Color LightColor = Color::WHITE;
    f32 OutlineWidth = 0.1f;
    RenderStateFlags Flags = RenderStateFlag_Fill;
};

} // namespace Onyx

namespace Onyx::Detail
{
enum BatchRanges : u32
{
    BatchRangeSize_StaticMesh = MaxBatches - 1,
    BatchRangeStart_StaticMesh = 0,
    BatchRangeEnd_StaticMesh = BatchRangeSize_StaticMesh + BatchRangeStart_StaticMesh,
    BatchRangeSize_Circle = 1,
    BatchRangeStart_Circle = BatchRangeEnd_StaticMesh,
    BatchRangeEnd_Circle = BatchRangeSize_Circle + BatchRangeStart_Circle,
};

TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4324) template <Dimension D> class alignas(TKIT_CACHE_LINE_SIZE) IRenderContext
{
    TKIT_NON_COPYABLE(IRenderContext)
  public:
    IRenderContext();

    void Flush();

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

    const RenderState<D> &GetState() const;
    RenderState<D> &GetState();

    void SetState(const RenderState<D> &p_State);

    void Render(const Window *p_Window);

    const auto &GetInstanceData() const
    {
        return m_InstanceData;
    }

    u64 GetViewMask() const
    {
        return m_ViewMask;
    }
    u64 GetGeneration() const
    {
        return m_Generation;
    }

    bool IsDirty(const u64 p_Generation) const
    {
        return m_Generation > p_Generation;
    }

    void AddTarget(const Window *p_Window)
    {
        m_ViewMask |= p_Window->GetViewBit();
    }
    void RemoveTarget(const Window *p_Window)
    {
        m_ViewMask &= ~p_Window->GetViewBit();
    }

  protected:
    RenderState<D> *m_Current{};

  private:
    void updateState();
    void addStaticMeshData(Mesh p_Mesh, const f32m<D> &p_Transform, StencilPass p_Pass);
    void addCircleData(const f32m<D> &p_Transform, const CircleOptions &p_Options, StencilPass p_Pass);
    struct InstanceBuffer
    {
        VKit::HostBuffer Data{};
        u32 Instances = 0;
    };

    TKit::TierArray<RenderState<D>> m_StateStack;
    TKit::FixedArray<TKit::FixedArray<InstanceBuffer, MaxBatches>, StencilPass_Count> m_InstanceData{};
    u64 m_ViewMask = 0;
    u64 m_Generation = 0;
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

template <> class alignas(TKIT_CACHE_LINE_SIZE) RenderContext<D2> final : public Detail::IRenderContext<D2>
{
  public:
    using IRenderContext<D2>::IRenderContext;
    void Rotate(f32 p_Angle);
};

template <> class alignas(TKIT_CACHE_LINE_SIZE) RenderContext<D3> final : public Detail::IRenderContext<D3>
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
};
TKIT_COMPILER_WARNING_IGNORE_POP()
} // namespace Onyx
