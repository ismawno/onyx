#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/property/options.hpp"
#include "onyx/asset/mesh.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/rendering/light.hpp"
#include "vkit/resource/host_buffer.hpp"

namespace Onyx
{
using RenderStateFlags = u8;
enum RenderStateFlagBit : RenderStateFlags
{
    RenderStateFlag_Fill = 1 << 0,
    RenderStateFlag_Outline = 1 << 1,
};

using LightFlags = u8;
enum LightFlagBit : LightFlags
{
    LightFlag_Point = 1 << 0,
    LightFlag_Directional = 1 << 1,
};

template <Dimension D> struct RenderState
{
    TKIT_REFLECT_DECLARE(RenderState)
    TKIT_YAML_SERIALIZE_DECLARE(RenderState)

    f32m<D> Transform = f32m<D>::Identity();
    Color FillColor = Color::White;
    Color OutlineColor = Color::White;

    f32 OutlineWidth = 0.1f;
    f32 AmbientIntensity = 0.4f;
    RenderStateFlags Flags = RenderStateFlag_Fill;
};

} // namespace Onyx

namespace Onyx::Detail
{

TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4324)

template <Dimension D> class alignas(TKIT_CACHE_LINE_SIZE) IRenderContext
{
    TKIT_NON_COPYABLE(IRenderContext)

  public:
    IRenderContext();

    void Flush();

    void Transform(const f32m<D> &transform);
    void Transform(const f32v<D> &translation, const f32v<D> &scale, const rot<D> &rotation);
    void Transform(const f32v<D> &translation, f32 scale, const rot<D> &rotation);

    void Translate(const f32v<D> &translation);
    void SetTranslation(const f32v<D> &translation);

    void Scale(const f32v<D> &scale);
    void Scale(f32 scale);

    void TranslateX(f32 x);
    void TranslateY(f32 y);

    void SetTranslationX(f32 x);
    void SetTranslationY(f32 y);

    void ScaleX(f32 x);
    void ScaleY(f32 y);

    void StaticMesh(Mesh mesh);
    void StaticMesh(Mesh mesh, const f32m<D> &transform);

    void Circle(const CircleOptions &options = {});
    void Circle(const f32m<D> &transform, const CircleOptions &options = {});

    void Line(Mesh mesh, const f32v<D> &start, const f32v<D> &end, f32 thickness = 0.1f);
    void Axes(Mesh mesh, const AxesOptions &options = {});

    void Push();
    void Push(const RenderState<D> &state);

    void Pop();

    void AddFlags(RenderStateFlags flags);
    void RemoveFlags(RenderStateFlags flags);

    void Fill(bool enable = true);
    void FillColor(const Color &color);

    void Outline(bool enable = true);
    void OutlineColor(const Color &color);
    void OutlineWidth(f32 width);

    void SetAmbientColor(const Color &color)
    {
        m_AmbientLight = color;
    }
    void SetAmbientIntensity(const f32 intensity)
    {
        m_AmbientLight.rgba[3] = intensity;
    }

    const Color &GetAmbientLight() const
    {
        return m_AmbientLight;
    }

    template <typename... LightArgs> PointLight<D> *AddPointLight(LightArgs &&...args)
    {
        TKit::TierAllocator *tier = TKit::GetTier();
        PointLight<D> *pl = tier->Create<PointLight<D>>(std::forward<LightArgs>(args)...);
        pl->SetViewMask(m_ViewMask);
        m_PointLights.Append(pl);
        m_NeedToUpdateLights |= LightFlag_Point;
        return pl;
    }

    void RemovePointLight(PointLight<D> *light);

    const TKit::TierArray<PointLight<D> *> &GetPointLights() const
    {
        return m_PointLights;
    }
    bool UpdateLightData();

    const RenderState<D> &GetState() const;
    RenderState<D> &GetState();

    void SetState(const RenderState<D> &state);

    void Render(const Window *window);

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

    bool IsDirty(const u64 generation) const
    {
        return m_Generation > generation;
    }

    void AddTarget(const ViewMask viewMask)
    {
        m_ViewMask |= viewMask;
        Renderer::UpdateViewMask(static_cast<RenderContext<D> *>(this)); //:(
    }
    void RemoveTarget(const ViewMask viewMask)
    {
        m_ViewMask &= ~viewMask;
        Renderer::UpdateViewMask(static_cast<RenderContext<D> *>(this));
    }

    void AddTarget(const Window *window)
    {
        AddTarget(window->GetViewBit());
    }
    void RemoveTarget(const Window *window)
    {
        RemoveTarget(window->GetViewBit());
    }

    LightFlags GetUpdateLightFlags() const
    {
        return m_NeedToUpdateLights;
    }
    void MarkLightsUpdated()
    {
        m_NeedToUpdateLights = 0;
    }

  protected:
    RenderState<D> *m_Current{};
    LightFlags m_NeedToUpdateLights = 0;
    ViewMask m_ViewMask = 0;

  private:
    struct InstanceBuffer
    {
        VKit::HostBuffer Data{};
        u32 Instances = 0;
    };

    void updateState();

    void resizeBuffer(InstanceBuffer &buffer);
    template <typename T> void addInstanceData(InstanceBuffer &buffer, const T &data);

    void addCircleData(const f32m<D> &transform, const CircleOptions &options, StencilPass pass);
    void addStaticMeshData(Mesh mesh, const f32m<D> &transform, StencilPass pass);

    TKit::TierArray<RenderState<D>> m_StateStack{};
    TKit::FixedArray<TKit::TierArray<InstanceBuffer>, StencilPass_Count> m_InstanceData{};
    TKit::TierArray<PointLight<D> *> m_PointLights{};
    Color m_AmbientLight = Color::White;
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
    void Rotate(f32 angle);
};

template <> class alignas(TKIT_CACHE_LINE_SIZE) RenderContext<D3> final : public Detail::IRenderContext<D3>
{
  public:
    using IRenderContext<D3>::IRenderContext;
    using IRenderContext<D3>::Transform;

    void Transform(const f32v3 &translation, const f32v3 &scale, const f32v3 &rotation);
    void Transform(const f32v3 &translation, f32 scale, const f32v3 &rotation);
    void TranslateZ(f32 z);

    void SetTranslationZ(f32 z);
    void ScaleZ(f32 z);

    void Rotate(const f32q &quaternion);
    void Rotate(const f32v3 &angles);
    void Rotate(f32 angle, const f32v3 &axis);

    void RotateX(f32 x);
    void RotateY(f32 y);
    void RotateZ(f32 z);

    template <typename... LightArgs> DirectionalLight *AddDirectionalLight(LightArgs &&...args)
    {
        TKit::TierAllocator *tier = TKit::GetTier();
        DirectionalLight *dl = tier->Create<DirectionalLight>(std::forward<LightArgs>(args)...);
        dl->SetViewMask(m_ViewMask);
        m_DirectionalLights.Append(dl);
        m_NeedToUpdateLights |= LightFlag_Directional;
        return dl;
    }

    void RemoveDirectionalLight(DirectionalLight *light);

    const TKit::TierArray<DirectionalLight *> &GetDirectionalLights() const
    {
        return m_DirectionalLights;
    }

  private:
    TKit::TierArray<DirectionalLight *> m_DirectionalLights{};
};
TKIT_COMPILER_WARNING_IGNORE_POP()
} // namespace Onyx
