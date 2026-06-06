#include "pch.hpp"
#include "onyx/context.hpp"
#include "instance.hpp"
#include "buffer.hpp"
#include "tkit/math/math.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
using namespace Detail;

template <Dimension D> IRenderContext<D>::IRenderContext()
{
    m_Current = &m_StateStack.Append();

    TKit::TierAllocator *tier = GetTier();
    for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
        for (u32 rmode = 0; rmode < RenderMode_Count; ++rmode)
        {
            InstanceDataArrays *idata = tier->Create<InstanceDataArrays>();
            m_InstanceData[bpass][rmode] = idata;
            InstanceDataBuffer &buffer = idata->Circles;
            buffer.Data = VKit::HostBuffer::Create<CircleInstanceData<D>>(ONYX_BUFFER_INITIAL_CAPACITY);
            buffer.Capacity = ONYX_BUFFER_INITIAL_CAPACITY;
            buffer.InstanceSize = sizeof(CircleInstanceData<D>);
        }
    resizeInstanceData();
    m_DefaultResources = Resources::GetDefaultResources();

    m_Current->Font = m_DefaultResources.Font;
    m_Current->FontSampler = m_DefaultResources.FontSampler;
}
template <Dimension D> IRenderContext<D>::~IRenderContext()
{
    TKit::TierAllocator *tier = GetTier();
    for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
        for (InstanceDataArrays *instanceData : m_InstanceData[bpass])
        {
            instanceData->Circles.Data.Destroy();
            for (auto &meshes : instanceData->Meshes)
                for (auto &pools : meshes)
                    for (InstanceDataBuffer &buffer : pools.Instances)
                        buffer.Data.Destroy();
            tier->Destroy(instanceData);
        }
}

template <Dimension D> void IRenderContext<D>::Flush()
{
    TKIT_ASSERT(m_StateStack.GetSize() == 1,
                "[ONYX][CONTEXT] Mismatched Push() call found. For every Push(), there must be a Pop()");

    m_StateStack[0] = ContextState<D>{};
    m_Current = &m_StateStack.GetFront();
    m_DefaultResources = Resources::GetDefaultResources();

    m_Current->Font = m_DefaultResources.Font;
    m_Current->FontSampler = m_DefaultResources.FontSampler;

    NoClip();
    for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
        for (InstanceDataArrays *instanceData : m_InstanceData[bpass])
        {
            instanceData->Circles.Instances = 0;
            for (u32 j = 0; j < Resource_MeshCount; ++j)
            {
                const ResourceType rtype = ResourceType(j);
                const TKit::Span<const u32> poolIds = Resources::GetResourcePoolIds<D>(rtype);

                auto &ipools = instanceData->Meshes[j];
                for (const u32 pid : poolIds)
                {
                    InstanceResourceGroup &group = ipools[pid];
                    group.Registry.Clear();
                    for (InstanceDataBuffer &buffer : group.Instances)
                        buffer.Instances = 0;
                }
            }
        }

    resizeInstanceData();
    ++m_Generation;
    m_DepthCounter = 0;
    m_PointLightData.Clear();
    m_DirectionalLightData.Clear();
}

#define CHECK_HANDLE(handle, rtype, dim)                                                                               \
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);                                                                           \
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);                                                                      \
    ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(handle, rtype, dim);                                                    \
    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(handle, rtype, dim);

#ifdef TKIT_ENABLE_ASSERTS
template <Dimension D> void checkMaterial(const Resource material)
{
    TKIT_ASSERT(Resources::IsResourceNull(material) || Resources::IsResourceValid<D>(material, Resource_Material),
                "[ONYX][CONTEX] The material handle {:#010x} is invalid and is not an explicit null material",
                material);
    if (!Resources::IsResourceNull(material))
    {
        const MaterialData<D> &data = Resources::GetMaterialData<D>(material);
        if constexpr (D == D2)
        {
            TKIT_ASSERT(
                Resources::IsResourceNull(data.Sampler) ||
                    Resources::IsResourceValid<D>(data.Sampler, Resource_Sampler),
                "[ONYX][CONTEXT] The sampler handle {:#010x} from the material handle {:#010x} is invalid and is not "
                "an explicit null sampler",
                data.Sampler, material);
            TKIT_ASSERT(
                Resources::IsResourceNull(data.Texture) ||
                    Resources::IsResourceValid<D>(data.Texture, Resource_Texture),
                "[ONYX][CONTEXT] The texture handle {:#010x} from the material handle {:#010x} is invalid and is not "
                "an explicit null texture",
                data.Texture, material);
        }
        else
            for (u32 i = 0; i < TextureSlot_Count; ++i)
            {
                const TextureSlot slot = TextureSlot(i);
                const Resource sampler = data.Samplers[i];
                const Resource texture = data.Textures[i];

                TKIT_ASSERT(Resources::IsResourceNull(sampler) ||
                                Resources::IsResourceValid<D>(sampler, Resource_Sampler),
                            "[ONYX][CONTEXT] The sampler handle {:#010x} from the material handle {:#010x} at texture "
                            "slot '{}' is "
                            "invalid and is not an explicit null sampler",
                            sampler, material, ToString(slot));
                TKIT_ASSERT(Resources::IsResourceNull(texture) ||
                                Resources::IsResourceValid<D>(texture, Resource_Texture),
                            "[ONYX][CONTEXT] The texture handle {:#010x} from the material handle {:#010x} at texture "
                            "slot '{}' is "
                            "invalid and is not an explicit null texture",
                            texture, material, ToString(slot));
            }
    }
}
#endif

template <Dimension D>
static InstanceData<D> createInstanceData(const ContextState<D> *state, const f32m<D> &transform,
                                          const u32 depthCounter)
{
#ifdef TKIT_ENABLE_ASSERTS
    checkMaterial<D>(state->Material);
#endif
    InstanceData<D> instanceData;
    instanceData.Transform = CreateTransformData<D>(transform);
    instanceData.Rect = state->Rect;
    instanceData.MatHandle = state->Material;
    instanceData.FillColor = state->FillColor.ToLinear().Pack();
    instanceData.OutlineColor = state->OutlineColor.ToLinear().Pack();
    instanceData.OutlineWidth = state->OutlineWidth;
    if constexpr (D == D2)
    {
        instanceData.Alignment = u32(state->Alignment[1]) << 8 | u32(state->Alignment[0]);
        instanceData.DepthCounter = depthCounter;
    }
    else
        instanceData.Alignment =
            u32(state->Alignment[2]) << 16 | u32(state->Alignment[1]) << 8 | u32(state->Alignment[0]);

    return instanceData;
}

template <Dimension D>
static StaticInstanceData<D> createStaticInstanceData(const ContextState<D> *state, const f32m<D> &transform,
                                                      const Resource bounds, const u32 depthCounter)
{
    StaticInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.BoundsHandle = bounds;
    return instanceData;
}

template <Dimension D>
static CircleInstanceData<D> createCircleInstanceData(const ContextState<D> *state, const f32m<D> &transform,
                                                      const CircleParameters &params, const u32 depthCounter)
{
    // constexpr TKit::FixedArray<f32v2, 9> bounds = {f32v2{-0.5f, -0.5f}, f32v2{-0.5f, 0.f}, f32v2{-0.5f, 0.5f},
    //                                                f32v2{0.f, -0.5f},   f32v2{0.f, 0.f},   f32v2{0.f, 0.5f},
    //                                                f32v2{0.5f, -0.5f},  f32v2{0.5f, 0.f},  f32v2{0.5f, 0.5f}};
    //
    // const f32v2 &alignment = bounds[state->Alignment[0] * 3 + state->Alignment[1]];
    CircleInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);

    instanceData.Arc.LowerCos = Math::Cosine(params.LowerAngle);
    instanceData.Arc.LowerSin = Math::Sine(params.LowerAngle);
    instanceData.Arc.UpperCos = Math::Cosine(params.UpperAngle);
    instanceData.Arc.UpperSin = Math::Sine(params.UpperAngle);

    instanceData.Fade.AngleOverflow = Math::Absolute(params.UpperAngle - params.LowerAngle) > Math::Pi() ? 1 : 0;
    instanceData.Fade.Hollowness = params.Hollowness;
    instanceData.Fade.InnerFade = params.InnerFade;
    instanceData.Fade.OuterFade = params.OuterFade;

    return instanceData;
}

template <Dimension D>
static ParametricInstanceData<D> createParametricInstanceData(const ContextState<D> *state, const f32m<D> &transform,
                                                              const Resource bounds, const ParametricShape shape,
                                                              const InstanceParameters &params, const u32 depthCounter)
{
    ParametricInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.BoundsHandle = bounds;
    instanceData.Shape = shape;
    instanceData.Parameters = params;
    return instanceData;
}

template <Dimension D>
static GlyphInstanceData<D> createGlyphInstanceData(const ContextState<D> *state, const f32m<D> &transform,
                                                    const f32 unitRange, const u32 depthCounter)
{
    GlyphInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.BoundsHandle = NullHandle;
    instanceData.AtlasHandle = Resources::GetFontAtlas(state->Font);
    instanceData.SamplerHandle = state->FontSampler;
    instanceData.UnitRange = unitRange;
    return instanceData;
}

template <Dimension D> void IRenderContext<D>::resizeBuffer(InstanceDataBuffer &buffer)
{
    if (buffer.Instances > buffer.Capacity)
    {
        const u32 ninst = GrowCapacity(buffer.Instances);
        buffer.Data.Resize(ninst * buffer.InstanceSize);
        buffer.Capacity = ninst;
    }
}

static Geometry getGeometry(const ResourceType rtype)
{
    switch (rtype)
    {
    case Resource_StaticMesh:
        return Geometry_Static;
    case Resource_ParametricMesh:
        return Geometry_Parametric;
    case Resource_GlyphMesh:
        return Geometry_Glyph;
    default:
        TKIT_FATAL("[ONYX][RENDERER] The resource type '{}' does not have a geometry associated", ToString(rtype));
        return Geometry_Count;
    }
}

template <Dimension D> void IRenderContext<D>::resizeInstanceData()
{
    ForEachResourceGroup<D>([this](const u32 bpass, const u32 rmode, const u32 mtype, const u32 pid) {
        InstanceResourceGroup &group = m_InstanceData[bpass][rmode]->Meshes[mtype][pid];
        const ResourceType rtype = ResourceType(mtype);

        const u32 count = group.Instances.GetSize();
        const u32 ncount = Resources::GetResourceCount<D>(Resources::CreateResourcePoolHandle(rtype, pid));
        for (u32 k = count; k < ncount; ++k)
        {
            InstanceDataBuffer &buffer = group.Instances.Append();
            const u32 isize = GetInstanceSize<D>(getGeometry(rtype));
            buffer.Data = VKit::HostBuffer{isize * ONYX_BUFFER_INITIAL_CAPACITY};
            buffer.Capacity = ONYX_BUFFER_INITIAL_CAPACITY;
            buffer.InstanceSize = isize;
            buffer.Instances = 0;
        }
    });
}

template <Dimension D> WorldRect<D> IRenderContext<D>::computeWorldRect(const ClipRect<D> &clip)
{
    WorldRect<D> wrect;
    if (clip.Min[0] == TKIT_F32_LOWEST)
    {
        wrect.Min[0] = TKIT_F32_LOWEST;
        return wrect;
    }
    if constexpr (D == D2)
    {
        const f32m3 &t = m_Current->Transform;
        const f32v2 &mn = clip.Min;
        const f32v2 &mx = clip.Max;

        const f32v2 corner = f32v2{t * f32v3{mn[0], mx[1], 1.f}};
        const f32v2 wmn = f32v2{t * f32v3{mn, 1.f}};
        const f32v2 wmx = f32v2{t * f32v3{mx, 1.f}};

        wrect.Min = wmn;
        wrect.Edge0 = corner - wmn;
        wrect.Edge1 = wmx - corner;
    }
    else
    {
        const f32m4 &t = m_Current->Transform;
        const f32v3 &mn = clip.Min;
        const f32v3 &mx = clip.Max;

        const f32v3 corner0 = f32v3{t * f32v4{mn[0], mx[1], mn[2], 1.f}};
        const f32v3 corner1 = f32v3{t * f32v4{mx[0], mx[1], mn[2], 1.f}};
        const f32v3 wmn = f32v3{t * f32v4{mn, 1.f}};
        const f32v3 wmx = f32v3{t * f32v4{mx, 1.f}};

        wrect.Min = wmn;
        wrect.Edge0 = corner0 - wmn;
        wrect.Edge1 = corner1 - corner0;
        wrect.Edge2 = wmx - corner1;
    }

    return wrect;
}

template <Dimension D>
ClipRect<D> IRenderContext<D>::computeClipRect(const f32v<D> &position, const f32v<D> &dimensions)
{
    f32v<D> mn;
    f32v<D> mx;
    for (u32 i = 0; i < D; ++i)
    {
        const Alignment alg = m_Current->Alignment[i];
        const f32 p = position[i];
        const f32 d = dimensions[i];
        if (alg == Alignment_Canonical)
        {
            mn[i] = p;
            mx[i] = p + d;
        }
        else if (alg == Alignment_Mirrored)
        {
            mn[i] = p - d;
            mx[i] = p;
        }
        else
        {
            mn[i] = p - 0.5f * d;
            mx[i] = p + 0.5f * d;
        }
    }
    return ClipRect<D>{mn, mx};
}

template <Dimension D>
template <typename T>
void IRenderContext<D>::addInstanceData(InstanceDataBuffer &buffer, const T &data)
{
    const u32 index = buffer.Instances++;
    resizeBuffer(buffer);
    buffer.Data.Write(&data, {.srcOffset = 0, .dstOffset = index * sizeof(T), .size = sizeof(T)});
}

template <Dimension D> void IRenderContext<D>::addCircleData(const f32m<D> &transform, const CircleParameters &params)
{
    if (!m_Current->RenderFlags)
        return;
    const CircleInstanceData<D> idata = createCircleInstanceData(m_Current, transform, params, ++m_DepthCounter);
    InstanceDataBuffer &buffer = m_InstanceData[m_Current->Blend][GetRenderMode(m_Current->RenderFlags)]->Circles;
    addInstanceData(buffer, idata);
}

template <Dimension D> void IRenderContext<D>::addStaticData(const Resource mesh, const f32m<D> &transform)
{
    if (!m_Current->RenderFlags)
        return;
    CHECK_HANDLE(mesh, Resource_StaticMesh, D);

    const u32 pid = Resources::GetResourcePoolId(mesh);
    const u32 mid = Resources::GetResourceId(mesh);

    const StaticInstanceData<D> idata =
        createStaticInstanceData(m_Current, transform, Resources::GetMeshBounds<D>(mesh), ++m_DepthCounter);

    InstanceResourceGroup &group =
        m_InstanceData[m_Current->Blend][GetRenderMode(m_Current->RenderFlags)]->Meshes[Resource_StaticMesh][pid];

    group.Registry.RegisterResourceId(mid);
    addInstanceData(group.Instances[mid], idata);
}
template <Dimension D>
void IRenderContext<D>::addParametricData(const Resource mesh, const f32m<D> &transform,
                                          const InstanceParameters &params)
{
    if (!m_Current->RenderFlags)
        return;
    CHECK_HANDLE(mesh, Resource_ParametricMesh, D);

    const u32 pid = Resources::GetResourcePoolId(mesh);
    const u32 mid = Resources::GetResourceId(mesh);

    const ParametricShape shape = Resources::GetParametricShape<D>(mesh);

    const ParametricInstanceData<D> idata = createParametricInstanceData(
        m_Current, transform, Resources::GetMeshBounds<D>(mesh), shape, params, ++m_DepthCounter);

    InstanceResourceGroup &group =
        m_InstanceData[m_Current->Blend][GetRenderMode(m_Current->RenderFlags)]->Meshes[Resource_ParametricMesh][pid];

    group.Registry.RegisterResourceId(mid);
    addInstanceData(group.Instances[mid], idata);
}

struct Character
{
    Resource Glyph;
    f32 Advance;
};

struct CharLine
{
    u32 Start = 0;
    u32 End = 0;
    f32 Width = 0.f;
};

template <Dimension D>
void IRenderContext<D>::addGlyphData(TKit::StringView text, const f32m<D> &transform,
                                     const ContextTextParameters &params)
{
    if (!m_Current->RenderFlags || text.IsEmpty())
        return;

    TKIT_ASSERT(
        m_Current->FontSampler != NullHandle,
        "[ONYX][CONTEXT] To draw text, a valid font sampler must be provided first with the FontSampler() method");

    CHECK_HANDLE(m_Current->Font, Resource_Font, D);
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(m_Current->FontSampler);
    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(m_Current->FontSampler, Resource_Sampler, D);

    ++m_DepthCounter;
    const Alignment alg0 = m_Current->Alignment[0] == Alignment_None ? Alignment_Left : m_Current->Alignment[0];
    const Alignment alg1 = m_Current->Alignment[1] == Alignment_None ? Alignment_Top : m_Current->Alignment[1];

    const Resource font = m_Current->Font;
    const FontData &fdata = Resources::GetFontData(font);

    TKit::String wrapped;
    if (params.MaxWidth != TKIT_F32_MAX)
    {
        wrapped = fdata.WrapText(text, params.MaxWidth);
        text = wrapped;
    }

    const u32 size = text.GetSize();

    f32m<D> t = transform;

    TKit::StackArray<Character> chars{};
    chars.Reserve(size);

    TKit::StackArray<CharLine> lines{};
    lines.Reserve(size);

    CharLine line{};
    u32 lastCode = TKIT_U32_MAX;
    for (u32 i = 0; i < size;)
    {
        u32 byteCount;
        const CodePoint code = DecodeUTF8(&text[i], &byteCount);
        if (code == '\n')
        {
            if (line.Start < line.End)
            {
                lines.Append(line);
                line.Start = line.End;
                line.Width = 0.f;
            }
            i += byteCount;
            continue;
        }

        const Resource glyph = Resources::GetGlyph(font, code);
        if (glyph == NullHandle)
        {
            TKIT_LOG_ERROR("[ONYX][CONTEXT] The code U+{:04X} ({}) was not found as an available code point", code,
                           TKit::StringView{&text[i], byteCount});
            i += byteCount;
            continue;
        }

        const GlyphData &gdata = Resources::GetGlyphData(glyph);
        f32 advance = 0.f;
        if (line.Start < line.End)
            advance = fdata.GetKerning(lastCode, code);

        advance += gdata.Advance + params.Kerning;
        chars.Append(glyph, advance);

        ++line.End;
        line.Width += advance;

        i += byteCount;
        lastCode = code;
    }
    if (line.Start < line.End)
        lines.Append(line);
    if (lines.IsEmpty())
        return;

    f32v2 pos;
    constexpr f32 factors[3] = {0.f, 0.5f, 1.f};
    const f32 xfactor = factors[alg0];
    const f32 yfactor = factors[2 - alg1];

    const f32 dy = fdata.LineHeight + params.LineSpacing;
    pos[1] = dy * (yfactor * f32(lines.GetSize() - 1) - factors[alg1]);

    const auto updateTransform = [&] {
        f32v<D + 1> p = transform[D];
        for (u32 i = 0; i < 2; ++i)
            for (u32 j = 0; j < D; ++j)
            {
                p[j] += t[i][j] * pos[i];
                t[D][j] += t[i][j] * pos[i];
            }
        t[D] = p;
    };
    for (const CharLine &ln : lines)
    {
        pos[0] = -xfactor * ln.Width;
        for (u32 i = ln.Start; i < ln.End; ++i)
        {
            if (params.CharacterCallback)
                (*params.CharacterCallback)(i, text[i], pos);

            updateTransform();
            addGlyphData(chars[i].Glyph, fdata.UnitRange, t);
            pos[0] += chars[i].Advance;
        }
        pos[1] -= dy;
    }
}
template <Dimension D>
void IRenderContext<D>::addGlyphData(const Resource glyph, const f32 unitRange, const f32m<D> &transform)
{
    const u32 pid = Resources::GetResourcePoolId(glyph);
    const u32 gid = Resources::GetResourceId(glyph);

    const GlyphInstanceData<D> idata = createGlyphInstanceData(m_Current, transform, unitRange, m_DepthCounter);
    InstanceResourceGroup &group =
        m_InstanceData[m_Current->Blend][GetRenderMode(m_Current->RenderFlags)]->Meshes[Resource_GlyphMesh][pid];

    group.Registry.RegisterResourceId(gid);
    addInstanceData(group.Instances[gid], idata);
}
template <Dimension D> void IRenderContext<D>::addGlyphData(const Resource glyph, const f32m<D> &transform)
{
    ++m_DepthCounter;
    const Resource font = Resources::GetFont(glyph);
    const FontData &fdata = Resources::GetFontData(font);

    const Alignment al0 = m_Current->Alignment[0];
    const Alignment al1 = m_Current->Alignment[1];
    if ((al0 == Alignment_None || al0 == Alignment_Left) && (al1 == Alignment_None || al1 == Alignment_Bottom))
    {
        addGlyphData(glyph, fdata.UnitRange, transform);
        return;
    }
    const GlyphData &gdata = Resources::GetGlyphData(glyph);
    const f32 dx = gdata.Advance;
    const f32 dy = fdata.Ascender - fdata.Descender;

    f32v2 pos{0.f};
    if (al0 == Alignment_Right)
        pos[0] -= dx;
    else if (al0 == Alignment_Center)
        pos[0] -= 0.5f * dx;

    if (al1 == Alignment_Top)
        pos[1] -= dy;
    else if (al1 == Alignment_Center)
        pos[1] -= 0.5f * dy;

    f32m<D> t = transform;
    f32v<D + 1> p = transform[D];
    for (u32 i = 0; i < 2; ++i)
        for (u32 j = 0; j < D; ++j)
        {
            p[j] += t[i][j] * pos[i];
            t[D][j] += t[i][j] * pos[i];
        }
    t[D] = p;
    addGlyphData(glyph, fdata.UnitRange, t);
}

template <Dimension D>
void IRenderContext<D>::addPointLightData(const f32m<D> &transform, const PointLightParameters<D> &params)
{
    if constexpr (D == D2)
    {
        TKIT_ASSERT(params.Decay >= 0.f && params.Decay <= 1.f,
                    "[ONYX][CONTEXT] Point light decay must be between 0 and 1, but is {}", params.Decay);
        TKIT_ASSERT(params.Extent >= 0.f && params.Extent <= 1.f,
                    "[ONYX][CONTEXT] Point light extent must be between 0 and 1, but is {}", params.Extent);
        // TKIT_ASSERT(params.Angle >= -Math::Pi() && params.Angle <= Math::Pi(),
        //             "[ONYX][CONTEXT] Point light angle must be between -pi and pi, but is {}", params.Angle);
    }
    PointLightParameters<D> &p = m_PointLightData.Append(params);
    p.Position = f32v<D>{transform * f32v<D + 1>{p.Position, 1.f}};
}

void RenderContext<D3>::addSpotLightData(const f32m4 &transform, const SpotLightParameters &params)
{
    SpotLightParameters &p = m_SpotLightData.Append(params);
    p.Position = f32v3{transform * f32v4{p.Position, 1.f}};
}

template <Dimension D> void IRenderContext<D>::Layout(const Onyx::Layout &layout)
{
    u32 depth = 0;
    const auto translate = [this, &depth](const LayoutDrawInfo &info) {
        if constexpr (D == D2)
        {
            TKIT_UNUSED(depth);
            Translate(info.Position, Transform_Intrinsic);
        }
        else
        {
            const f32 z = 1.f - f32(++depth) / f32(1U << 24);
            Translate(f32v3{info.Position, z}, Transform_Intrinsic);
        }
    };
    const auto scale = [this](const LayoutDrawInfo &info) {
        if constexpr (D == D2)
            Scale(info.Size, Transform_Intrinsic);
        else
            Scale(f32v3{info.Size, 1.f}, Transform_Intrinsic);
    };
    Push();
    AlignX(Alignment_Left);
    AlignY(Alignment_Bottom);
    for (const LayoutDrawInfo &info : layout.GetDrawInfo())
    {
        Push();
        RenderFlags(info.RenderFlags);
        Material(info.Material);
        FillColor(info.FillColor);
        OutlineColor(info.OutlineColor);
        OutlineWidth(info.OutlineWidth);

        ClipRect<D> rect;
        if constexpr (D == D2)
        {
            rect.Min = info.ClipMin;
            rect.Max = info.ClipMax;
        }
        else
        {
            // NOTE(Isma): A reasonably large number to not clip in the z axis
            rect.Min = f32v3{info.ClipMin, -1e8};
            rect.Max = f32v3{info.ClipMax, 1e8};
        }
        Clip(rect);

        switch (info.ShapeType)
        {
        case LayoutShape_Circle:
            translate(info);
            scale(info);
            Circle();
            break;

        case LayoutShape_Rectangle:
            translate(info);
            scale(info);
            StaticMesh(info.Handle);
            break;

        case LayoutShape_RoundedRectangle:
            translate(info);
            ParametricMesh(info.Handle,
                           RoundedRectParameters{.Width = info.Size[0], .Height = info.Size[1], .Radius = info.Radius});
            break;
        case LayoutShape_Glyph:
            translate(info);
            scale(info);

            Glyph(info.Handle);
            break;
        case LayoutShape_Text:
            translate(info);
            scale(info);

            Font(info.Handle);
            Text(info.Text);
            break;

        case LayoutShape_Unicode:
            translate(info);
            scale(info);

            Font(info.Handle);
            Unicode(info.Unicode);
            break;

        default:
            break;
        }
        Pop();
    }
    Pop();
}

template <Dimension D> static rot<D> computeLineRotation(const f32v<D> &start, const f32v<D> &end)
{
    const f32v<D> delta = end - start;
    if constexpr (D == D2)
        return Math::AntiTangent(delta[1], delta[0]) - 0.5f * Math::Pi();
    else
    {
        const f32v3 dir = Math::Normalize(delta);
        const f32v3 r{dir[2], 0.f, -dir[0]};
        const f32 theta = 0.5f * Math::AntiCosine(dir[1]);
        if (!Math::ApproachesZero(Math::NormSquared(r)))
            return f32q{Math::Cosine(theta), Math::Normalize(r) * Math::Sine(theta)};
        if (dir[1] < 0.f)
            return f32q{0.f, 1.f, 0.f, 0.f};
        return RotType<D>::Identity;
    }
}
template <Dimension D>
void IRenderContext<D>::Line(const Resource mesh, const f32v<D> &start, const f32v<D> &end, const f32 thickness)
{
    const f32v<D> delta = end - start;

    f32m<D> transform = m_Current->Transform;
    Onyx::Transform<D>::TranslateIntrinsic(transform, 0.5f * (start + end));
    Onyx::Transform<D>::RotateIntrinsic(transform, computeLineRotation<D>(start, end));

    if constexpr (D == D2)
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v2{thickness, Math::Norm(delta)});
    else
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v3{thickness, Math::Norm(delta), thickness});

    addStaticData(mesh, transform);
}
template <Dimension D> void IRenderContext<D>::Axes(const Resource mesh, const AxesParameters &params)
{
    if constexpr (D == D2)
    {
        Color &color = m_Current->FillColor;
        const Color oldColor = color; // A cheap filthy push

        const f32v2 xLeft = {-params.Size, 0.f};
        const f32v2 xRight = {params.Size, 0.f};

        const f32v2 yDown = {0.f, -params.Size};
        const f32v2 yUp = {0.f, params.Size};

        color = Color{245u, 64u, 90u};
        Line(mesh, xLeft, xRight, params.Thickness);
        color = Color{65u, 135u, 245u};
        Line(mesh, yDown, yUp, params.Thickness);

        color = oldColor; // A cheap filthy pop
    }
    else
    {
        Color &color = m_Current->FillColor;
        const Color oldColor = color; // A cheap filthy push

        const f32v3 xLeft = {-params.Size, 0.f, 0.f};
        const f32v3 xRight = {params.Size, 0.f, 0.f};

        const f32v3 yDown = {0.f, -params.Size, 0.f};
        const f32v3 yUp = {0.f, params.Size, 0.f};

        const f32v3 zBack = {0.f, 0.f, -params.Size};
        const f32v3 zFront = {0.f, 0.f, params.Size};

        color = Color{245u, 64u, 90u};
        Line(mesh, xLeft, xRight, params.Thickness);
        color = Color{180u, 245u, 65u};
        Line(mesh, yDown, yUp, params.Thickness);
        color = Color{65u, 135u, 245u};
        Line(mesh, zBack, zFront, params.Thickness);
        color = oldColor; // A cheap filthy pop
    }
}

template class IRenderContext<D2>;
template class IRenderContext<D3>;

} // namespace Onyx
