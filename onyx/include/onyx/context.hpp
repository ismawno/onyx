#pragma once

#include "onyx/dimension.hpp"
#include "onyx/instance.hpp"
#include "onyx/resources.hpp"
#include "onyx/font.hpp"
#include "onyx/view.hpp"
#include "onyx/layout.hpp"

namespace Onyx
{
template <Dimension D> struct PointLightData;
template <Dimension D> struct DirectionalLightData;
struct SpotLightData;

template <Dimension D> struct PointLightParameters;
template <> struct PointLightParameters<D3>
{
    using InstanceData = PointLightData<D3>;
    f32v3 Position = f32v3{0.f};
    Color Tint = Color_White;
    f32 LightSize = 0.01f;
    f32 LightRadius = 1.f;
    f32 ShadowRadius = 4.f;
    f32 Intensity = 0.8f;
    f32 DepthBias = 0.001f;
    LightFlags Flags = 0;
};

template <> struct PointLightParameters<D2>
{
    using InstanceData = PointLightData<D2>;
    f32v2 Position = f32v2{0.f};
    Color Tint = Color_White;
    f32 LightSize = 0.01f;
    f32 LightRadius = 1.f;
    f32 ShadowRadius = 4.f;
    f32 Angle = 0.f;  // from -pi to pi, otherwise shadows wont work
    f32 Decay = 0.f;  // must be between 0 and 1
    f32 Extent = 1.f; // must be between 0 and 1
    f32 Intensity = 0.8f;
    f32 DepthBias = 0.001f;
    LightFlags Flags = 0;
};

constexpr TKit::FixedArray<f32, ONYX_MAX_CASCADES> CreateDepthBias(const f32 minBias, const f32 maxBias)
{
    const f32 dbias = (maxBias - minBias) / (ONYX_MAX_CASCADES > 1 ? (ONYX_MAX_CASCADES - 1) : ONYX_MAX_CASCADES);
    TKit::FixedArray<f32, ONYX_MAX_CASCADES> bias;
    for (u32 i = 0; i < ONYX_MAX_CASCADES; ++i)
        bias[i] = minBias + dbias * i;
    return bias;
}

struct FixedCascadeParameters
{
    f32v3 ViewPosition{1.f};
    f32 MinSize = 6.f;
    f32 MaxSize = 12.f;
    f32 Near = -3.f;
    f32 Far = 3.f;
};

struct FittedCascadeParameters
{
    f32 ZMul = 3.f;
};

struct ShadowCascadeParameters
{
    const RenderView<D3> *View = nullptr; // if null, will use fixed cascades. if populated, will use fitted
    TKit::FixedArray<f32, ONYX_MAX_CASCADES> DepthBias = CreateDepthBias(0.001f, 0.007f);
    FixedCascadeParameters FixedParameters{};
    FittedCascadeParameters FittedParameters{};
    f32 Overlap = 0.f;
    f32 Lambda = 0.8f;
    u32 Count = ONYX_MAX_CASCADES;
};

template <Dimension D> struct DirectionalLightParameters;

template <> struct DirectionalLightParameters<D2>
{
    using InstanceData = DirectionalLightData<D2>;
    Color Tint = Color_White;
    f32 LightOffset = 0.f;   // offsets the ray along the normal direction
    f32 LightExtent = 10.f;  // extends the light along the normal direction
    f32 ShadowOffset = 0.f;  // offsets the shadow along the light's direction
    f32 ShadowExtent = 10.f; // extends the shadow range along the light's direction
    f32 Decay = 0.f;
    f32 Angle = 0.f;
    f32 AngleSize = 0.f;
    f32 DepthBias = 0.001f;
    f32 Intensity = 0.8f;
    LightFlags Flags = 0;
};

template <> struct DirectionalLightParameters<D3>
{
    using InstanceData = DirectionalLightData<D3>;
    f32v3 Direction = f32v3{-1.f};
    f32v2 Offset = f32v2{0.f};
    f32v2 Extent = f32v2{100.f};
    Color Tint = Color_White;
    ShadowCascadeParameters Cascades{};
    f32 Decay = 0.f;
    f32 AngleSize = 0.f;
    f32 Intensity = 0.8f;
    LightFlags Flags = 0;
};

struct SpotLightParameters
{
    using InstanceData = SpotLightData;
    f32v3 Position{0.f};
    f32v3 Direction{-1.f, 0.f, 0.f};
    Color Tint = Color_White;
    f32 LightSize = 0.04f;
    f32 DepthBias = 0.001f;
    f32 FieldOfView = Math::Radians(75.f);
    f32 LightRange = 5.f;
    f32 ShadowRange = 20.f;
    f32 Decay = 0.f;
    f32 Intensity = 0.8f;
    LightFlags Flags = 0;
};

struct ContextTextParameters
{
    std::function<void(u32, char, f32v2 &)> *CharacterCallback = nullptr;
    f32 Kerning = 0.f;
    f32 LineSpacing = 0.f;
    f32 MaxWidth = TKIT_F32_MAX;
};
struct AxesParameters
{
    f32 Thickness = 0.1f;
    f32 Size = 50.f;
};
struct CircleParameters
{
    f32 InnerFade = 0.f;
    f32 OuterFade = 0.f;
    f32 Hollowness = 0.f;
    f32 LowerAngle = 0.f;
    f32 UpperAngle = 2.f * Math::Pi();
};

template <Dimension D> struct ClipRect
{
    f32v<D> Min;
    f32v<D> Max;

    void Union(const ClipRect<D> &other)
    {
        Min = Math::Max(Min, other.Min);
        Max = Math::Min(Max, other.Max);
    }
};

template <Dimension D> struct RectPair
{
    ClipRect<D> Clip;
    WorldRect<D> World;
};

template <Dimension D> struct ContextState
{
    f32m<D> Transform = f32m<D>::Identity();
    Color FillColor = Color_White;
    Color OutlineColor = Color_White;
    WorldRect<D> Rect{};

    f32 OutlineWidth = 0.1f;
    f32 AmbientIntensity = 0.4f;

    f32v2 TexOffset{0.f};
    f32v2 TexScale{1.f};

    Resource Material = NullHandle;
    Resource Sampler = NullHandle;
    Resource Texture = NullHandle;
    Resource Font = NullHandle;
    vec<Alignment, D> Alignment{Alignment_None};
    RenderModeFlags RenderFlags = RenderModeFlag_Flat;
    BlendPass Blend = BlendPass_Opaque;
};

struct InstanceDataArrays;
struct InstanceDataBuffer;

template <Dimension D> class alignas(TKIT_CACHE_LINE_SIZE) IRenderContext
{
    TKIT_NON_COPYABLE(IRenderContext)

  public:
    // parameter specifying how many dynamic mesh slots to preallocate. by default zero. if users
    // exceed (or the value was zero) when calling dynamic mesh, trigger an assert warning them. this is only a concern
    // for the most immediate API that lets the user to just provide some vertices/indices, because it requires a fresh
    // new dynamic mesh handle. precisely these "fresh" ones are the ones the context allocated beforehand. it cant
    // allocate more bc contexts must remain independent, and that operation is not thread safe. if users provide their
    // own handles, that is just fine and wont interfere with anything. for the immediate approach, store a counter that
    // increases that picks a different dynamic mesh
    IRenderContext(u32 immediateDynamicMeshCapacity);
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

    void Material(const Resource material)
    {
        m_Current->Material = material;
    }

    void Font(const Resource font)
    {
        m_Current->Font = font;
    }
    void Sampler(const Resource sampler)
    {
        m_Current->Sampler = sampler;
    }

    void Texture(const Resource tex)
    {
        m_Current->Texture = tex;
    }
    void Texture(const Resource tex, const f32v2 &offset, const f32v2 &scale = f32v2{1.f})
    {
        m_Current->Texture = tex;
        TextureCoordinates(offset, scale);
    }
    void TextureCoordinates(const f32v2 &offset, const f32v2 &scale = f32v2{1.f})
    {
        m_Current->TexOffset = offset;
        m_Current->TexScale = scale;
    }

    void Image(const Resource tex)
    {
        const Resource oldTex = m_Current->Texture;
        Texture(tex);
        Quad();
        Texture(oldTex);
    }
    void Image(const Resource tex, const f32v2 &offset, const f32v2 &scale = f32v2{1.f})
    {
        const Resource oldTex = m_Current->Texture;
        const f32v2 oldOffset = m_Current->TexOffset;
        const f32v2 oldScale = m_Current->TexScale;
        Texture(tex, offset, scale);
        Quad();
        Texture(oldTex, oldOffset, oldScale);
    }
    void Image(const Resource tex, const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        const Resource oldTex = m_Current->Texture;
        Texture(tex);
        Quad(transform, mode);
        Texture(oldTex);
    }
    void Image(const Resource tex, const f32m<D> &transform, const f32v2 &offset, const f32v2 &scale = f32v2{1.f},
               const TransformMode mode = Transform_Extrinsic)
    {
        const Resource oldTex = m_Current->Texture;
        const f32v2 oldOffset = m_Current->TexOffset;
        const f32v2 oldScale = m_Current->TexScale;
        Texture(tex, offset, scale);
        Quad(transform, mode);
        Texture(oldTex, oldOffset, oldScale);
    }

    void StaticMesh(const Resource mesh)
    {
        addStaticData(mesh, m_Current->Transform);
    }
    void StaticMesh(const Resource mesh, const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        addStaticData(mesh, mode == Transform_Extrinsic ? (transform * m_Current->Transform)
                                                        : (m_Current->Transform * transform));
    }

    void DynamicMesh(const Resource mesh)
    {
        addDynamicData(mesh, m_Current->Transform);
    }
    void DynamicMesh(const Resource mesh, const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        addDynamicData(mesh, mode == Transform_Extrinsic ? (transform * m_Current->Transform)
                                                         : (m_Current->Transform * transform));
    }
    void DynamicMesh(const DynamicMeshData<D> *data)
    {
        DynamicMesh(getDynamicMeshHandle(data));
    }
    void DynamicMesh(const DynamicMeshData<D> *data, const f32m<D> &transform,
                     const TransformMode mode = Transform_Extrinsic)
    {
        DynamicMesh(getDynamicMeshHandle(data), transform, mode);
    }

    template <typename... DynMeshArgs> void DynamicMesh(DynMeshArgs &&...args)
    {
        DynamicMesh(getDynamicMeshHandle(std::forward<DynMeshArgs>(args)...));
    }
    template <typename... DynMeshArgs>
    void DynamicMesh(DynMeshArgs &&...args, const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        DynamicMesh(getDynamicMeshHandle(std::forward<DynMeshArgs>(args)...), transform, mode);
    }

    void Triangle()
    {
        StaticMesh(m_DefaultResources.GetTriangle<D>());
    }
    void Triangle(const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        StaticMesh(m_DefaultResources.GetTriangle<D>(), transform, mode);
    }

    void Quad()
    {
        StaticMesh(m_DefaultResources.GetQuad<D>());
    }
    void Quad(const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        StaticMesh(m_DefaultResources.GetQuad<D>(), transform, mode);
    }

    void ParametricMesh(const Resource mesh, const InstanceParameters &params)
    {
        addParametricData(mesh, m_Current->Transform, params);
    }
    void ParametricMesh(const Resource mesh, const InstanceParameters &params, const f32m<D> &transform,
                        const TransformMode mode = Transform_Extrinsic)
    {
        addParametricData(
            mesh, mode == Transform_Extrinsic ? (transform * m_Current->Transform) : (m_Current->Transform * transform),
            params);
    }

    template <typename T> void ParametricMesh(const Resource mesh, const T &params)
    {
        if constexpr (std::is_same_v<T, StadiumParameters>)
            ParametricMesh(mesh, InstanceParameters{.Stadium = params});
        else if constexpr (std::is_same_v<T, RoundedRectParameters>)
            ParametricMesh(mesh, InstanceParameters{.RoundedRect = params});
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
    void ParametricMesh(const Resource mesh, const T &params, const f32m<D> &transform,
                        const TransformMode mode = Transform_Extrinsic)
    {
        if constexpr (std::is_same_v<T, StadiumParameters>)
            ParametricMesh(mesh, InstanceParameters{.Stadium = params}, transform, mode);
        else if constexpr (std::is_same_v<T, RoundedRectParameters>)
            ParametricMesh(mesh, InstanceParameters{.RoundedRect = params}, transform, mode);
        else if constexpr (D == D3 && std::is_same_v<T, CapsuleParameters>)
            ParametricMesh(mesh, InstanceParameters{.Capsule = params}, transform, mode);
        else if constexpr (D == D3 && std::is_same_v<T, RoundedBoxParameters>)
            ParametricMesh(mesh, InstanceParameters{.RoundedBox = params}, transform, mode);
        else if constexpr (D == D3 && std::is_same_v<T, TorusParameters>)
            ParametricMesh(mesh, InstanceParameters{.Torus = params}, transform, mode);
        else
            static_assert(false, "[ONYX][CONTEXT] Type T is not a valid instance parameters type");
    }

    void Stadium(const StadiumParameters &params = {1.f, 0.5f})
    {
        ParametricMesh(m_DefaultResources.GetStadium<D>(), params);
    }
    void Stadium(const StadiumParameters &params, const f32m<D> &transform,
                 const TransformMode mode = Transform_Extrinsic)
    {
        ParametricMesh(m_DefaultResources.GetStadium<D>(), params, transform, mode);
    }

    void RoundedRect(const RoundedRectParameters &params = {1.f, 1.f, 0.5f})
    {
        ParametricMesh(m_DefaultResources.GetRoundedRect<D>(), params);
    }
    void RoundedRect(const RoundedRectParameters &params, const f32m<D> &transform,
                     const TransformMode mode = Transform_Extrinsic)
    {
        ParametricMesh(m_DefaultResources.GetRoundedRect<D>(), params, transform, mode);
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

    void Glyph(const Resource glyph)
    {
        addGlyphData(glyph, m_Current->Transform);
    }
    void Glyph(const Resource glyph, const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        addGlyphData(glyph, mode == Transform_Extrinsic ? (transform * m_Current->Transform)
                                                        : (m_Current->Transform * transform));
    }
    void Unicode(const CodePoint code)
    {
        Glyph(Resources::GetGlyph(m_Current->Font, code));
    }
    void Unicode(const CodePoint code, const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        Glyph(Resources::GetGlyph(m_Current->Font, code), transform, mode);
    }
    void Unicode(const TKit::StringView code)
    {
        Unicode(DecodeUTF8(code.GetData()));
    }
    void Unicode(const TKit::StringView code, const f32m<D> &transform, const TransformMode mode = Transform_Extrinsic)
    {
        Unicode(DecodeUTF8(code.GetData()), transform, mode);
    }

    void Text(const TKit::StringView text, const ContextTextParameters &params = {})
    {
        addGlyphData(text, m_Current->Transform, params);
    }
    void Text(const TKit::StringView text, const f32m<D> &transform, const ContextTextParameters &params = {},
              const TransformMode mode = Transform_Extrinsic)
    {
        addGlyphData(
            text, mode == Transform_Extrinsic ? (transform * m_Current->Transform) : (m_Current->Transform * transform),
            params);
    }

    void Layout(const Onyx::Layout &layout);

    void Line(Resource staticMesh, const f32v<D> &start, const f32v<D> &end, f32 thickness = 0.1f);
    void Axes(Resource staticMesh, const AxesParameters &params = {});

    void Line(const f32v<D> &start, const f32v<D> &end, f32 thickness = 0.1f)
    {
        if constexpr (D == D2)
            Line(m_DefaultResources.Quad2, start, end, thickness);
        else
            Line(m_DefaultResources.Box, start, end, thickness);
    }
    void RoundedLine(const f32v<D> &start, const f32v<D> &end, f32 thickness = 0.1f)
    {
        if constexpr (D == D2)
            Line(m_DefaultResources.Stadium2, start, end, thickness);
        else
            Line(m_DefaultResources.Capsule, start, end, thickness);
    }

    void Axes(const AxesParameters &params = {})
    {
        if constexpr (D == D2)
            Axes(m_DefaultResources.Quad2, params);
        else
            Axes(m_DefaultResources.Cylinder, params);
    }

    void Push(const ContextState<D> &state)
    {
        m_StateStack.Append(state);
        updateState();
    }
    void Push()
    {
        Push(*m_Current);
    }
    void PushClear()
    {
        Push(ContextState<D>{});
    }

    void Clip(const ClipRect<D> &rect)
    {
        m_Current->Rect = computeWorldRect(rect);
    }
    void Clip(const f32v<D> &position, const f32v<D> &dimensions)
    {
        Clip(computeClipRect(position, dimensions));
    }
    void Clip(const f32v<D> &dimensions)
    {
        Clip(computeClipRect(f32v<D>{0.f}, dimensions));
    }
    void NoClip()
    {
        ClipRect<D> crect{f32v<D>{TKIT_F32_MIN}, f32v<D>{TKIT_F32_MAX}};
        m_Current->Rect = computeWorldRect(crect);
    }

    f32v<D> WorldToLocal(const f32v<D> &world)
    {
        return f32v<D>{Math::Inverse(m_Current->Transform) * f32v<D + 1>{world, 1.f}};
    }
    f32v<D> LocalToWorld(const f32v<D> &local)
    {
        return f32v<D>{m_Current->Transform * f32v<D + 1>{local, 1.f}};
    }

    void Pop()
    {
        m_StateStack.Pop();
        TKIT_ASSERT(!m_StateStack.IsEmpty(), "[ONYX][CONTEXT] For every Push(), there must be a Pop()");
        updateState();
    }

    void RenderFlags(const RenderModeFlags flags)
    {
        m_Current->RenderFlags = flags;
    }
    void AddRenderFlags(const RenderModeFlags flags)
    {
        m_Current->RenderFlags |= flags;
    }
    void RemoveRenderFlags(const RenderModeFlags flags)
    {
        m_Current->RenderFlags &= ~flags;
    }
    void FillColor(const Color &color)
    {
        m_Current->FillColor = color;
        m_Current->Blend = Math::Approximately(color.rgba[3], 1.f) ? BlendPass_Opaque : BlendPass_Transparent;
    }
    // required when material has a color factor with alpha < 1 or circles have fading
    void Blend(const bool enable = true)
    {
        m_Current->Blend = BlendPass(enable);
    }
    void Alpha(const f32 alpha)
    {
        m_Current->FillColor.rgba[3] = alpha;
        m_Current->Blend = Math::Approximately(alpha, 1.f) ? BlendPass_Opaque : BlendPass_Transparent;
    }
    // there is no support for alpha channel in outlines
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
    void PointLight(const PointLightParameters<D> &params = {})
    {
        addPointLightData(m_Current->Transform, params);
    }
    void DirectionalLight(const DirectionalLightParameters<D> &params = {})
    {
        m_DirectionalLightData.Append(params);
    }

    const ContextState<D> &GetState() const
    {
        return *m_Current;
    }
    void SetState(const ContextState<D> &state)
    {
        *m_Current = state;
        m_Current->Blend =
            Math::Approximately(m_Current->FillColor.rgba[3], 1.f) ? BlendPass_Opaque : BlendPass_Transparent;
    }

    const InstanceDataGrouping<InstanceDataArrays *> &GetInstanceData() const
    {
        return m_InstanceData;
    }

    const TKit::TierArray<PointLightParameters<D>> &GetPointLightData() const
    {
        return m_PointLightData;
    }
    const TKit::TierArray<DirectionalLightParameters<D>> &GetDirectionalLightData() const
    {
        return m_DirectionalLightData;
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

    void AddTarget(const ViewMask viewMask)
    {
        m_ViewMask |= viewMask;
        ++m_Generation;
    }

    void RemoveTarget(const ViewMask viewMask)
    {
        m_ViewMask &= ~viewMask;
        ++m_Generation;
    }

    void AddTarget(const RenderView<D> *view)
    {
        AddTarget(view->GetViewBit());
    }
    void RemoveTarget(const RenderView<D> *view)
    {
        RemoveTarget(view->GetViewBit());
    }

  protected:
    ContextState<D> *m_Current{};
    DefaultResources m_DefaultResources{};

  private:
    void updateState()
    {
        m_Current = &m_StateStack.GetBack();
    }

    void resizeBuffer(InstanceDataBuffer &buffer);
    void resizeInstanceData();
    WorldRect<D> computeWorldRect(const ClipRect<D> &clip);
    ClipRect<D> computeClipRect(const f32v<D> &position, const f32v<D> &dimensions);

    template <typename T> void addInstanceData(InstanceDataBuffer &buffer, const T &data);

    void addCircleData(const f32m<D> &transform, const CircleParameters &params);
    void addStaticData(Resource mesh, const f32m<D> &transform);
    void addParametricData(Resource mesh, const f32m<D> &transform, const InstanceParameters &params);
    void addGlyphData(TKit::StringView text, const f32m<D> &transform, const ContextTextParameters &params);
    void addGlyphData(TKit::StringView text, const f32m<D> &transform);
    void addGlyphData(Resource glyph, f32 unitRange, const f32m<D> &transform);
    void addGlyphData(Resource glyph, const f32m<D> &transform);
    void addDynamicData(Resource mesh, const f32m<D> &transform);
    void addPointLightData(const f32m<D> &transform, const PointLightParameters<D> &params);
#ifdef TKIT_ENABLE_ASSERTS
    void checkMaterial(Resource material);
#endif

    Resource getDynamicMeshHandle(const DynamicMeshData<D> *data)
    {
        TKIT_ASSERT(!m_ImmediateDynamicMeshes.IsEmpty(),
                    "[ONYX][CONTEXT] Trying to draw an immediate dynamic mesh but initial capacity for immediate "
                    "dynamic meshes was zero. Consider setting the immediateDynamicMeshCapacity to a non-zero value at "
                    "context creation");
        TKIT_ASSERT(m_DynamicMeshCounter < m_ImmediateDynamicMeshes.GetSize(),
                    "[ONYX][CONTEXT] The maximum amount of {} immediate dynamic meshes has been exceeded! Consider "
                    "increasing the parameter immediateDynamicMeshCapacity at context creation",
                    m_ImmediateDynamicMeshes.GetSize());
        const DynamicMeshInfo<D> &info = m_ImmediateDynamicMeshes[m_DynamicMeshCounter++];
        *info.Data = *data;
        return info.Handle;
    }

    template <typename... Args> Resource getDynamicMeshHandle(Args &&...args)
    {
        const DynamicMeshData<D> data = CreateDynamicMeshData<D>(std::forward<Args>(args)...);
        return getDynamicMeshHandle(&data);
    }

    TKit::TierArray<ContextState<D>> m_StateStack{};
    InstanceDataGrouping<InstanceDataArrays *> m_InstanceData{};

    TKit::TierArray<PointLightParameters<D>> m_PointLightData{};
    TKit::TierArray<DirectionalLightParameters<D>> m_DirectionalLightData{};

    TKit::TierArray<DynamicMeshInfo<D>> m_ImmediateDynamicMeshes{};

    u64 m_Generation = 0;
    Color m_AmbientLight = Color{Color_White, 0.4f};
    ViewMask m_ViewMask = 0;
    u32 m_DepthCounter = 0;
    u32 m_DynamicMeshCounter = 0;
};

template <Dimension D> class RenderContext;

template <> class alignas(TKIT_CACHE_LINE_SIZE) RenderContext<D2> final : public IRenderContext<D2>
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
};

template <> class alignas(TKIT_CACHE_LINE_SIZE) RenderContext<D3> final : public IRenderContext<D3>
{
  public:
    using IRenderContext<D3>::IRenderContext;
    using IRenderContext<D3>::Transform;

    void Flush()
    {
        IRenderContext<D3>::Flush();
        m_SpotLightData.Clear();
    }

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

    void Box()
    {
        StaticMesh(m_DefaultResources.Box);
    }
    void Box(const f32m4 &transform, const TransformMode mode = Transform_Extrinsic)
    {
        StaticMesh(m_DefaultResources.Box, transform, mode);
    }

    void Sphere()
    {
        StaticMesh(m_DefaultResources.Sphere);
    }
    void Sphere(const f32m4 &transform, const TransformMode mode = Transform_Extrinsic)
    {
        StaticMesh(m_DefaultResources.Sphere, transform, mode);
    }

    void Cylinder()
    {
        StaticMesh(m_DefaultResources.Cylinder);
    }
    void Cylinder(const f32m4 &transform, const TransformMode mode = Transform_Extrinsic)
    {
        StaticMesh(m_DefaultResources.Cylinder, transform, mode);
    }

    void Capsule(const CapsuleParameters &params = {1.f, 0.5f})
    {
        ParametricMesh(m_DefaultResources.Capsule, params);
    }
    void Capsule(const CapsuleParameters &params, const f32m4 &transform,
                 const TransformMode mode = Transform_Extrinsic)
    {
        ParametricMesh(m_DefaultResources.Capsule, params, transform, mode);
    }

    void RoundedBox(const RoundedBoxParameters &params = {1.f, 1.f, 1.f, 0.5f})
    {
        ParametricMesh(m_DefaultResources.RoundedBox, params);
    }
    void RoundedBox(const RoundedBoxParameters &params, const f32m4 &transform,
                    const TransformMode mode = Transform_Extrinsic)
    {
        ParametricMesh(m_DefaultResources.RoundedBox, params, transform, mode);
    }

    void Torus(const TorusParameters &params = {0.5f, 0.5f})
    {
        ParametricMesh(m_DefaultResources.Torus, params);
    }
    void Torus(const TorusParameters &params, const f32m4 &transform, const TransformMode mode = Transform_Extrinsic)
    {
        ParametricMesh(m_DefaultResources.Torus, params, transform, mode);
    }

    void SpotLight(const SpotLightParameters &params = {})
    {
        addSpotLightData(m_Current->Transform, params);
    }

    const TKit::TierArray<SpotLightParameters> &GetSpotLightData() const
    {
        return m_SpotLightData;
    }

  private:
    void addSpotLightData(const f32m4 &transform, const SpotLightParameters &params);

    TKit::TierArray<SpotLightParameters> m_SpotLightData{};
};
} // namespace Onyx
