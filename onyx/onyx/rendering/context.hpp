#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/asset/handle.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/rendering/light.hpp"
#include "onyx/asset/font.hpp"
#include "vkit/resource/host_buffer.hpp"

namespace Onyx
{
template <Dimension D> struct RenderState
{
    TKIT_REFLECT_DECLARE(RenderState)
    TKIT_YAML_SERIALIZE_DECLARE(RenderState)

    f32m<D> Transform = f32m<D>::Identity();
    Color FillColor = Color::White;
    Color OutlineColor = Color::White;

    f32 OutlineWidth = 0.1f;
    f32 AmbientIntensity = 0.4f;
    Asset Material = NullHandle;
    Asset Font = NullHandle;
    Asset FontSampler = NullHandle;
    vec<Alignment, D> Alignment{Alignment_None};
    RenderMode Draw = RenderMode_Fill;
};

} // namespace Onyx

namespace Onyx::Detail
{
template <Dimension D> class alignas(TKIT_CACHE_LINE_SIZE) IRenderContext
{
    TKIT_NON_COPYABLE(IRenderContext)

  public:
    IRenderContext();
    ~IRenderContext();

    void Flush();

    void Align(const vec<Alignment, D> &alg)
    {
        m_Current->Alignment = alg;
    }
    void Align(const Alignment alg)
    {
        m_Current->Alignment = vec<Alignment, D>{alg};
    }
    void AlignX(const Alignment alg)
    {
        m_Current->Alignment[0] = alg;
    }
    void AlignY(const Alignment alg)
    {
        m_Current->Alignment[1] = alg;
    }

    void ResetTransform()
    {
        m_Current->Transform = f32m<D>::Identity();
    }

    void Transform(const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        m_Current->Transform =
            mode == Transform_Extrinsic ? (transform * m_Current->Transform) : (m_Current->Transform * transform);
    }

    void Transform(const f32v<D> &translation, const f32v<D> &scale, const rot<D> &rotation,
                   const TransformMode mode = Transform_Extrinsic)
    {
        this->Transform(Onyx::Transform<D>::ComputeTransform(translation, scale, rotation), mode);
    }
    void Transform(const f32v<D> &translation, const f32 scale, const rot<D> &rotation,
                   const TransformMode mode = Transform_Extrinsic)
    {
        this->Transform(Onyx::Transform<D>::ComputeTransform(translation, f32v<D>{scale}, rotation), mode);
    }

    void Translate(const f32v<D> &translation, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, translation);
        else
            Onyx::Transform<D>::TranslateIntrinsic(m_Current->Transform, translation);
    }

    void SetTranslation(const f32v<D> &translation)
    {
        m_Current->Transform[D][0] = translation[0];
        m_Current->Transform[D][1] = translation[1];
        if constexpr (D == D3)
            m_Current->Transform[D][2] = translation[2];
    }

    void Scale(const f32v<D> &scale, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, scale);
        else
            Onyx::Transform<D>::ScaleIntrinsic(m_Current->Transform, scale);
    }
    void Scale(const f32 scale, const TransformMode mode = Transform_Extrinsic)
    {
        Scale(f32v<D>{scale}, mode);
    }

    void TranslateX(const f32 x, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, 0, x);
        else
            Onyx::Transform<D>::TranslateIntrinsic(m_Current->Transform, 0, x);
    }

    void TranslateY(const f32 y, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, 1, y);
        else
            Onyx::Transform<D>::TranslateIntrinsic(m_Current->Transform, 1, y);
    }

    void SetTranslationX(const f32 x)
    {
        m_Current->Transform[D][0] = x;
    }

    void SetTranslationY(const f32 y)
    {
        m_Current->Transform[D][1] = y;
    }

    void ScaleX(const f32 x, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, 0, x);
        else
            Onyx::Transform<D>::ScaleIntrinsic(m_Current->Transform, 0, x);
    }

    void ScaleY(const f32 y, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, 1, y);
        else
            Onyx::Transform<D>::ScaleIntrinsic(m_Current->Transform, 1, y);
    }

    void Material(const Asset material)
    {
        m_Current->Material = material;
    }

    void Font(const Asset font)
    {
        m_Current->Font = font;
    }
    void FontSampler(const Asset sampler)
    {
        m_Current->FontSampler = sampler;
    }

    void StaticMesh(const Asset mesh)
    {
        addStaticData(mesh, m_Current->Transform);
    }
    void StaticMesh(const Asset mesh, const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        addStaticData(mesh, mode == Transform_Extrinsic ? (transform * m_Current->Transform)
                                                        : (m_Current->Transform * transform));
    }

    void ParametricMesh(const Asset mesh, const InstanceParameters &params)
    {
        addParametricData(mesh, m_Current->Transform, params);
    }
    void ParametricMesh(const Asset mesh, const InstanceParameters &params, const f32m<D> &transform,
                        const TransformMode mode = Transform_Extrinsic)
    {
        addParametricData(
            mesh, mode == Transform_Extrinsic ? (transform * m_Current->Transform) : (m_Current->Transform * transform),
            params);
    }

    template <typename T> void ParametricMesh(const Asset mesh, const T &params)
    {
        if constexpr (std::is_same_v<T, StadiumParameters>)
            ParametricMesh(mesh, InstanceParameters{.Stadium = params});
        else if constexpr (std::is_same_v<T, RoundedQuadParameters>)
            ParametricMesh(mesh, InstanceParameters{.RoundedQuad = params});
        else if constexpr (D == D3 && std::is_same_v<T, CapsuleParameters>)
            ParametricMesh(mesh, InstanceParameters{.Capsule = params});
        else if constexpr (D == D3 && std::is_same_v<T, RoundedBoxParameters>)
            ParametricMesh(mesh, InstanceParameters{.RoundedBox = params});
        else if constexpr (D == D3 && std::is_same_v<T, TorusParameters>)
            ParametricMesh(mesh, InstanceParameters{.Torus = params});
        else
            static_assert(false, "[ONYX][CONTEXT] Type T is not a valid instance parameters type");
    }
    template <typename T>
    void ParametricMesh(const Asset mesh, const T &params, const f32m<D> &transform,
                        const TransformMode mode = Transform_Extrinsic)
    {
        if constexpr (std::is_same_v<T, StadiumParameters>)
            ParametricMesh(mesh, InstanceParameters{.Stadium = params}, transform, mode);
        else if constexpr (std::is_same_v<T, RoundedQuadParameters>)
            ParametricMesh(mesh, InstanceParameters{.RoundedQuad = params}, transform, mode);
        else if constexpr (D == D3 && std::is_same_v<T, CapsuleParameters>)
            ParametricMesh(mesh, InstanceParameters{.Capsule = params}, transform, mode);
        else if constexpr (D == D3 && std::is_same_v<T, RoundedBoxParameters>)
            ParametricMesh(mesh, InstanceParameters{.RoundedBox = params}, transform, mode);
        else if constexpr (D == D3 && std::is_same_v<T, TorusParameters>)
            ParametricMesh(mesh, InstanceParameters{.Torus = params}, transform, mode);
        else
            static_assert(false, "[ONYX][CONTEXT] Type T is not a valid instance parameters type");
    }

    void Circle(const CircleParameters &params = {})
    {
        addCircleData(m_Current->Transform, params);
    }
    void Circle(const f32m<D> &transform, const CircleParameters &params = {},
                const TransformMode mode = Transform_Extrinsic)
    {
        addCircleData(mode == Transform_Extrinsic ? (transform * m_Current->Transform)
                                                  : (m_Current->Transform * transform),
                      params);
    }

    void Text(std::string_view text, const TextParameters &params = {})
    {
        addGlyphData(text, m_Current->Transform, params);
    }
    void Text(std::string_view text, const f32m<D> &transform, const TextParameters &params = {},
              const TransformMode mode = Transform_Extrinsic)
    {
        addGlyphData(
            text, mode == Transform_Extrinsic ? (transform * m_Current->Transform) : (m_Current->Transform * transform),
            params);
    }

    void Line(Asset staticMesh, const f32v<D> &start, const f32v<D> &end, f32 thickness = 0.1f);
    void Axes(Asset staticMesh, const AxesParameters &params = {});

    void Push(const RenderState<D> &state)
    {
        m_StateStack.Append(state);
        updateState();
    }
    void Push()
    {
        Push(*m_Current);
    }

    void Pop()
    {
        TKIT_ASSERT(m_StateStack.GetSize() > 1, "[ONYX][CONTEXT] For every Push(), there must be a Pop()");
        m_StateStack.Pop();
        updateState();
    }

    void Fill(const bool enable = true);
    void FillColor(const Color &color)
    {
        m_Current->FillColor = color;
    }

    void Outline(bool enable = true);
    void OutlineColor(const Color &color)
    {
        m_Current->OutlineColor = color;
    }
    void OutlineWidth(const f32 width)
    {
        m_Current->OutlineWidth = width;
    }

    void SetAmbientLight(const Color &color)
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

    template <typename... LightArgs> PointLight<D> *CreatePointLight(LightArgs &&...args)
    {
        TKit::TierAllocator *tier = TKit::GetTier();
        PointLight<D> *pl = tier->Create<PointLight<D>>(std::forward<LightArgs>(args)...);
        pl->SetViewMask(m_ViewMask);
        m_PointLights.Append(pl);
        return pl;
    }

    void DestroyPointLight(PointLight<D> *light);
    void DestroyAllPointLights();

    const TKit::TierArray<PointLight<D> *> &GetPointLights() const
    {
        return m_PointLights;
    }

    const RenderState<D> &GetState() const
    {
        return *m_Current;
    }
    RenderState<D> &GetState()
    {
        return *m_Current;
    }

    void SetState(const RenderState<D> &state);

    void Render(const Window *window);

    const auto &GetInstanceData() const
    {
        return m_InstanceData;
    }

    ViewMask GetViewMask() const
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

    u32 GetDepthCounter() const
    {
        return m_DepthCounter;
    }
    void SetDepthCounter(const u32 counter)
    {
        m_DepthCounter = counter;
    }

  protected:
    RenderState<D> *m_Current{};
    ViewMask m_ViewMask = 0;
    TKit::TierArray<PointLight<D> *> m_PointLights{};
    u64 m_Generation = 0;

  private:
    struct InstanceDataBuffer
    {
        VKit::HostBuffer Data{};
        u32 InstanceSize = 0;
        u32 Instances = 0;
        u32 Capacity = 0;
    };

    struct InstanceDataArrays
    {
        InstanceDataBuffer Circles{};
        TKit::FixedArray<TKit::FixedArray<TKit::TierArray<InstanceDataBuffer>, ONYX_MAX_ASSET_POOLS>, Asset_MeshCount>
            Meshes{};
    };

    void updateState()
    {
        m_Current = &m_StateStack.GetBack();
    }

    void resizeBuffer(InstanceDataBuffer &buffer);
    void resizeBufferArrays();

    template <typename T> void addInstanceData(InstanceDataBuffer &buffer, const T &data);

    void addCircleData(const f32m<D> &transform, const CircleParameters &params);
    void addStaticData(Asset mesh, const f32m<D> &transform);
    void addParametricData(Asset mesh, const f32m<D> &transform, const InstanceParameters &params);
    void addGlyphData(std::string_view text, const f32m<D> &transform, const TextParameters &params);
    void addGlyphData(const Glyph *glyph, const f32m<D> &transform);
#ifdef TKIT_ENABLE_ASSERTS
    void checkMaterial(Asset material);
#endif

    TKit::TierArray<RenderState<D>> m_StateStack{};
    TKit::FixedArray<InstanceDataArrays, RenderMode_Count> m_InstanceData{};
    Color m_AmbientLight = Color{Color::White, 0.4f};
    u32 m_DepthCounter = 0;
};
} // namespace Onyx::Detail

namespace Onyx
{

template <Dimension D> class RenderContext;

template <> class alignas(TKIT_CACHE_LINE_SIZE) RenderContext<D2> final : public Detail::IRenderContext<D2>
{
  public:
    using IRenderContext<D2>::IRenderContext;
    void Rotate(const f32 angle, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D2>::RotateExtrinsic(m_Current->Transform, angle);
        else
            Onyx::Transform<D2>::RotateIntrinsic(m_Current->Transform, angle);
    }

    void AddTarget(const ViewMask viewMask)
    {
        m_ViewMask |= viewMask;
        for (PointLight<D2> *pl : m_PointLights)
            pl->SetViewMask(m_ViewMask);
        ++m_Generation;
    }
    void RemoveTarget(const ViewMask viewMask)
    {
        m_ViewMask &= ~viewMask;
        for (PointLight<D2> *pl : m_PointLights)
            pl->SetViewMask(m_ViewMask);
        ++m_Generation;
    }

    void AddTarget(const Window *window)
    {
        AddTarget(window->GetViewBit());
    }
    void RemoveTarget(const Window *window)
    {
        RemoveTarget(window->GetViewBit());
    }
};

template <> class alignas(TKIT_CACHE_LINE_SIZE) RenderContext<D3> final : public Detail::IRenderContext<D3>
{
  public:
    using IRenderContext<D3>::IRenderContext;
    using IRenderContext<D3>::Transform;

    ~RenderContext();

    void AlignZ(const Alignment alg)
    {
        m_Current->Alignment[2] = alg;
    }

    void Transform(const f32v3 &translation, const f32v3 &scale, const f32v3 &rotation,
                   const TransformMode mode = Transform_Extrinsic)
    {
        this->Transform(Onyx::Transform<D3>::ComputeTransform(translation, scale, f32q{rotation}), mode);
    }

    void Transform(const f32v3 &translation, f32 scale, const f32v3 &rotation,
                   const TransformMode mode = Transform_Extrinsic)
    {
        this->Transform(Onyx::Transform<D3>::ComputeTransform(translation, f32v3{scale}, f32q{rotation}), mode);
    }

    void TranslateZ(const f32 z, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D3>::TranslateExtrinsic(m_Current->Transform, 2, z);
        else
            Onyx::Transform<D3>::TranslateIntrinsic(m_Current->Transform, 2, z);
    }

    void SetTranslationZ(const f32 z)
    {
        m_Current->Transform[D3][2] = z;
    }

    void ScaleZ(const f32 z, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D3>::ScaleExtrinsic(m_Current->Transform, 2, z);
        else
            Onyx::Transform<D3>::ScaleIntrinsic(m_Current->Transform, 2, z);
    }

    void Rotate(const f32q &quaternion, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D3>::RotateExtrinsic(m_Current->Transform, quaternion);
        else
            Onyx::Transform<D3>::RotateIntrinsic(m_Current->Transform, quaternion);
    }

    void Rotate(const f32v3 &angles, const TransformMode mode = Transform_Extrinsic)
    {
        Rotate(f32q(angles), mode);
    }

    void Rotate(const f32 angle, const f32v3 &axis, const TransformMode mode = Transform_Extrinsic)
    {
        Rotate(f32q::FromAngleAxis(angle, axis), mode);
    }

    void RotateX(const f32 angle, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D3>::RotateXExtrinsic(m_Current->Transform, angle);
        else
            Onyx::Transform<D3>::RotateXIntrinsic(m_Current->Transform, angle);
    }
    void RotateY(const f32 angle, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D3>::RotateYExtrinsic(m_Current->Transform, angle);
        else
            Onyx::Transform<D3>::RotateYIntrinsic(m_Current->Transform, angle);
    }
    void RotateZ(const f32 angle, const TransformMode mode = Transform_Extrinsic)
    {
        if (mode == Transform_Extrinsic)
            Onyx::Transform<D3>::RotateZExtrinsic(m_Current->Transform, angle);
        else
            Onyx::Transform<D3>::RotateZIntrinsic(m_Current->Transform, angle);
    }

    template <typename... LightArgs> DirectionalLight *CreateDirectionalLight(LightArgs &&...args)
    {
        TKit::TierAllocator *tier = TKit::GetTier();
        DirectionalLight *dl = tier->Create<DirectionalLight>(std::forward<LightArgs>(args)...);
        dl->SetViewMask(m_ViewMask);
        m_DirectionalLights.Append(dl);
        return dl;
    }

    void DestroyDirectionalLight(DirectionalLight *light);
    void DestroyAllDirectionalLights();

    const TKit::TierArray<DirectionalLight *> &GetDirectionalLights() const
    {
        return m_DirectionalLights;
    }
    void AddTarget(const ViewMask viewMask)
    {
        m_ViewMask |= viewMask;
        for (PointLight<D3> *pl : m_PointLights)
            pl->SetViewMask(m_ViewMask);
        for (DirectionalLight *dl : m_DirectionalLights)
            dl->SetViewMask(m_ViewMask);
        ++m_Generation;
    }
    void RemoveTarget(const ViewMask viewMask)
    {
        m_ViewMask &= ~viewMask;
        for (PointLight<D3> *pl : m_PointLights)
            pl->SetViewMask(m_ViewMask);
        for (DirectionalLight *dl : m_DirectionalLights)
            dl->SetViewMask(m_ViewMask);
        ++m_Generation;
    }

    void AddTarget(const Window *window)
    {
        AddTarget(window->GetViewBit());
    }
    void RemoveTarget(const Window *window)
    {
        RemoveTarget(window->GetViewBit());
    }

  private:
    TKit::TierArray<DirectionalLight *> m_DirectionalLights{};
};
} // namespace Onyx
