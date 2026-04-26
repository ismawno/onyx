#include "onyx/core/pch.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/resource/resources.hpp"
#include "onyx/asset/assets.hpp"
#include "tkit/math/math.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
using namespace Detail;

template <Dimension D> IRenderContext<D>::IRenderContext()
{
    m_Current = &m_StateStack.Append();
    for (InstanceDataArrays &instanceData : m_InstanceData)
    {
        InstanceDataBuffer &buffer = instanceData.Circles;
        buffer.Data = VKit::HostBuffer::Create<CircleInstanceData<D>>(ONYX_BUFFER_INITIAL_CAPACITY);
        buffer.Capacity = ONYX_BUFFER_INITIAL_CAPACITY;
        buffer.InstanceSize = sizeof(CircleInstanceData<D>);
    }
    resizeBufferArrays();
}
template <Dimension D> IRenderContext<D>::~IRenderContext()
{
    for (InstanceDataArrays &instanceData : m_InstanceData)
    {
        instanceData.Circles.Data.Destroy();
        for (auto &meshes : instanceData.Meshes)
            for (auto &pools : meshes)
                for (InstanceDataBuffer &buffer : pools)
                    buffer.Data.Destroy();
    }
}

template <Dimension D> void IRenderContext<D>::Flush()
{
    TKIT_ASSERT(m_StateStack.GetSize() == 1,
                "[ONYX][CONTEXT] Mismatched Push() call found. For every Push(), there must be a Pop()");

    m_StateStack[0] = RenderState<D>{};
    m_Current = &m_StateStack.GetFront();
    for (InstanceDataArrays &instanceData : m_InstanceData)
    {
        instanceData.Circles.Instances = 0;
        for (u32 j = 0; j < Asset_MeshCount; ++j)
        {
            const AssetType atype = AssetType(j);
            const TKit::Span<const u32> poolIds = Assets::GetAssetPoolIds<D>(atype);

            auto &ipools = instanceData.Meshes[j];
            for (const u32 pid : poolIds)
                for (InstanceDataBuffer &buffer : ipools[pid])
                    buffer.Instances = 0;
        }
    }
    resizeBufferArrays();
    ++m_Generation;
    m_DepthCounter = 0;
    m_PointLightData.Clear();
}

#define CHECK_HANDLE(handle, atype, dim)                                                                               \
    ONYX_CHECK_ASSET_IS_NOT_NULL(handle);                                                                              \
    ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(handle);                                                                         \
    ONYX_CHECK_ASSET_POOL_IS_VALID_WITH_DIM(handle, atype, dim);                                                       \
    ONYX_CHECK_ASSET_IS_VALID_WITH_DIM(handle, atype, dim);

#ifdef TKIT_ENABLE_ASSERTS
template <Dimension D> void checkMaterial(const Asset material)
{
    TKIT_ASSERT(Assets::IsAssetNull(material) || Assets::IsAssetValid<D>(material, Asset_Material),
                "[ONYX][CONTEX] The material handle {:#010x} is invalid and is not an explicit null material",
                material);
    if (!Assets::IsAssetNull(material))
    {
        const MaterialData<D> &data = Assets::GetMaterialData<D>(material);
        if constexpr (D == D2)
        {
            TKIT_ASSERT(
                Assets::IsAssetNull(data.Sampler) || Assets::IsAssetValid<D>(data.Sampler, Asset_Sampler),
                "[ONYX][CONTEXT] The sampler handle {:#010x} from the material handle {:#010x} is invalid and is not "
                "an explicit null sampler",
                data.Sampler, material);
            TKIT_ASSERT(
                Assets::IsAssetNull(data.Texture) || Assets::IsAssetValid<D>(data.Texture, Asset_Texture),
                "[ONYX][CONTEXT] The texture handle {:#010x} from the material handle {:#010x} is invalid and is not "
                "an explicit null texture",
                data.Texture, material);
        }
        else
            for (u32 i = 0; i < TextureSlot_Count; ++i)
            {
                const TextureSlot slot = TextureSlot(i);
                const Asset sampler = data.Samplers[i];
                const Asset texture = data.Textures[i];

                TKIT_ASSERT(Assets::IsAssetNull(sampler) || Assets::IsAssetValid<D>(sampler, Asset_Sampler),
                            "[ONYX][CONTEXT] The sampler handle {:#010x} from the material handle {:#010x} at texture "
                            "slot '{}' is "
                            "invalid and is not an explicit null sampler",
                            sampler, material, ToString(slot));
                TKIT_ASSERT(Assets::IsAssetNull(texture) || Assets::IsAssetValid<D>(texture, Asset_Texture),
                            "[ONYX][CONTEXT] The texture handle {:#010x} from the material handle {:#010x} at texture "
                            "slot '{}' is "
                            "invalid and is not an explicit null texture",
                            texture, material, ToString(slot));
            }
    }
}
#endif

template <Dimension D>
static InstanceData<D> createInstanceData(const RenderState<D> *state, const f32m<D> &transform, const u32 depthCounter)
{
#ifdef TKIT_ENABLE_ASSERTS
    checkMaterial<D>(state->Material);
#endif
    InstanceData<D> instanceData;
    instanceData.Transform = CreateTransformData<D>(transform);
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
static StaticInstanceData<D> createStaticInstanceData(const RenderState<D> *state, const f32m<D> &transform,
                                                      const Asset bounds, const u32 depthCounter)
{
    StaticInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.BoundsHandle = bounds;
    return instanceData;
}

template <Dimension D>
static CircleInstanceData<D> createCircleInstanceData(const RenderState<D> *state, const f32m<D> &transform,
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

    instanceData.Fade.AngleOverflow = Math::Absolute(params.UpperAngle - params.LowerAngle) > Math::Pi<f32>() ? 1 : 0;
    instanceData.Fade.Hollowness = params.Hollowness;
    instanceData.Fade.InnerFade = params.InnerFade;
    instanceData.Fade.OuterFade = params.OuterFade;

    return instanceData;
}

template <Dimension D>
static ParametricInstanceData<D> createParametricInstanceData(const RenderState<D> *state, const f32m<D> &transform,
                                                              const Asset bounds, const ParametricShape shape,
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
static GlyphInstanceData<D> createGlyphInstanceData(const RenderState<D> *state, const f32m<D> &transform,
                                                    const u32 depthCounter)
{
    GlyphInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.BoundsHandle = NullHandle;
    instanceData.AtlasHandle = Assets::GetFontAtlas(state->Font);
    instanceData.SamplerHandle = state->FontSampler;
    return instanceData;
}

template <Dimension D> void IRenderContext<D>::resizeBuffer(InstanceDataBuffer &buffer)
{
    if (buffer.Instances > buffer.Capacity)
    {
        const u32 ninst = Resources::GrowCapacity(buffer.Instances);
        buffer.Data.Resize(ninst * buffer.InstanceSize);
        buffer.Capacity = ninst;
    }
}

static Geometry getGeometry(const AssetType atype)
{
    switch (atype)
    {
    case Asset_StaticMesh:
        return Geometry_Static;
    case Asset_ParametricMesh:
        return Geometry_Parametric;
    case Asset_GlyphMesh:
        return Geometry_Glyph;
    default:
        return Geometry_Count;
        TKIT_FATAL("[ONYX][RENDERER] The asset type '{}' does not have a geometry associated", ToString(atype));
    }
}

template <Dimension D> void IRenderContext<D>::resizeBufferArrays()
{
    for (InstanceDataArrays &instanceData : m_InstanceData)
        for (u32 j = 0; j < Asset_MeshCount; ++j)
        {
            const AssetType atype = AssetType(j);
            const auto poolIds = Assets::GetAssetPoolIds<D>(atype);

            auto &ipools = instanceData.Meshes[j];
            for (const u32 pid : poolIds)
            {
                auto &buffers = ipools[pid];
                const u32 count = buffers.GetSize();
                const u32 ncount = Assets::GetAssetCount<D>(Assets::CreateAssetPoolHandle(atype, pid));
                for (u32 k = count; k < ncount; ++k)
                {
                    InstanceDataBuffer &buffer = buffers.Append();
                    const u32 isize = GetInstanceSize<D>(getGeometry(atype));
                    buffer.Data = VKit::HostBuffer{isize * ONYX_BUFFER_INITIAL_CAPACITY};
                    buffer.Capacity = ONYX_BUFFER_INITIAL_CAPACITY;
                    buffer.InstanceSize = isize;
                    buffer.Instances = 0;
                }
            }
        }
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
    InstanceDataBuffer &buffer = m_InstanceData[GetRenderMode(m_Current->RenderFlags)].Circles;
    addInstanceData(buffer, idata);
}

template <Dimension D> void IRenderContext<D>::addStaticData(const Asset mesh, const f32m<D> &transform)
{
    if (!m_Current->RenderFlags)
        return;
    CHECK_HANDLE(mesh, Asset_StaticMesh, D);
    const u32 pid = Assets::GetAssetPoolId(mesh);
    const u32 mid = Assets::GetAssetId(mesh);

    const StaticInstanceData<D> idata =
        createStaticInstanceData(m_Current, transform, Assets::GetMeshBounds<D>(mesh), ++m_DepthCounter);
    InstanceDataBuffer &buffer =
        m_InstanceData[GetRenderMode(m_Current->RenderFlags)].Meshes[Asset_StaticMesh][pid][mid];
    addInstanceData(buffer, idata);
}
template <Dimension D>
void IRenderContext<D>::addParametricData(const Asset mesh, const f32m<D> &transform, const InstanceParameters &params)
{
    if (!m_Current->RenderFlags)
        return;
    CHECK_HANDLE(mesh, Asset_ParametricMesh, D);
    const u32 pid = Assets::GetAssetPoolId(mesh);
    const u32 mid = Assets::GetAssetId(mesh);

    const ParametricShape shape = Assets::GetParametricShape<D>(mesh);

    const ParametricInstanceData<D> idata = createParametricInstanceData(
        m_Current, transform, Assets::GetMeshBounds<D>(mesh), shape, params, ++m_DepthCounter);

    InstanceDataBuffer &buffer =
        m_InstanceData[GetRenderMode(m_Current->RenderFlags)].Meshes[Asset_ParametricMesh][pid][mid];
    addInstanceData(buffer, idata);
}

struct Character
{
    const Glyph *Glyph;
    f32 Advance;
};

struct CharLine
{
    u32 Start = 0;
    u32 End = 0;
    f32 Width = 0.f;
};

template <Dimension D>
void IRenderContext<D>::addGlyphData(const std::string_view text, const f32m<D> &transform,
                                     const TextParameters &params)
{
    if (!m_Current->RenderFlags || text.empty())
        return;

    CHECK_HANDLE(m_Current->Font, Asset_Font, D);
    ONYX_CHECK_ASSET_IS_NOT_NULL(m_Current->FontSampler);
    ONYX_CHECK_ASSET_IS_VALID_WITH_DIM(m_Current->FontSampler, Asset_Sampler, D);

    ++m_DepthCounter;
    const Alignment alg0 = m_Current->Alignment[0] == Alignment_None ? Alignment_Left : m_Current->Alignment[0];
    const Alignment alg1 = m_Current->Alignment[1] == Alignment_None ? Alignment_Top : m_Current->Alignment[1];

    const Asset font = m_Current->Font;
    const FontData &fdata = Assets::GetFontData(font);
    const u32 size = u32(text.size());
    const f32 maxWidth = params.Width;

    f32m<D> t = transform;

    TKit::StackArray<Character> chars{};
    chars.Reserve(size);

    TKit::StackArray<CharLine> lines{};
    lines.Reserve(size);

    CharLine line{};
    u32 wordEnd = 0;
    f32 wordWidth = 0.f;

    for (u32 i = 0; i < size; ++i)
    {
        const char c = text[i];
        if (c == '\n')
        {
            if (line.Start < line.End)
            {
                lines.Append(line);
                line.Start = line.End;
                line.Width = 0.f;
                wordEnd = line.End;
            }
            continue;
        }
        if (c == ' ')
        {
            wordEnd = line.End + 1;
            wordWidth = line.Width;
        }

        f32 advance = 0.f;
        if (line.Start < line.End)
            advance = fdata.GetKerning(text[i - 1], c);

        const Glyph *glyph = Assets::GetGlyph(font, c);
        advance += glyph->Advance + params.Kerning;
        chars.Append(glyph, advance);
        ++line.End;
        line.Width += advance;
        if (line.Width + advance > maxWidth && line.Start < wordEnd)
        {
            lines.Append(line.Start, wordEnd, wordWidth);
            line.Start = wordEnd;
            line.Width -= wordWidth;
        }
    }
    if (line.Start < line.End)
        lines.Append(line);

    f32v2 pos;
    constexpr f32 factors[3] = {0.f, 0.5f, 1.f};
    const f32 xfactor = factors[alg0];
    const f32 yfactor = factors[2 - alg1];
    const f32 yscale = 1.f / (fdata.Ascender - fdata.Descender);

    const f32 dy = fdata.LineHeight * yscale + params.LineSpacing;
    pos[1] = yfactor * (f32(lines.GetSize()) - 1.f) * dy - 0.5f * factors[alg1] * dy;

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
    for (const CharLine &line : lines)
    {
        pos[0] = -xfactor * line.Width;
        for (u32 i = line.Start; i < line.End; ++i)
        {
            if (params.CharacterCallback)
                (*params.CharacterCallback)(i, text[i], pos);

            updateTransform();
            addGlyphData(chars[i].Glyph, t);
            pos[0] += chars[i].Advance;
        }
        pos[1] -= dy;
    }
}
template <Dimension D> void IRenderContext<D>::addGlyphData(const Glyph *glyph, const f32m<D> &transform)
{
    const u32 pid = Assets::GetAssetPoolId(m_Current->Font);

    const GlyphInstanceData<D> idata = createGlyphInstanceData(m_Current, transform, m_DepthCounter);
    InstanceDataBuffer &buffer =
        m_InstanceData[GetRenderMode(m_Current->RenderFlags)].Meshes[Asset_GlyphMesh][pid][glyph->Id];
    addInstanceData(buffer, idata);
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
        // TKIT_ASSERT(params.Angle >= -Math::Pi<f32>() && params.Angle <= Math::Pi<f32>(),
        //             "[ONYX][CONTEXT] Point light angle must be between -pi and pi, but is {}", params.Angle);
    }
    PointLightParameters<D> &p = m_PointLightData.Append(params);
    p.Position = f32v<D>{transform * f32v<D + 1>{p.Position, 1.f}};
}

template <Dimension D> static rot<D> computeLineRotation(const f32v<D> &start, const f32v<D> &end)
{
    const f32v<D> delta = end - start;
    if constexpr (D == D2)
        return Math::AntiTangent(delta[1], delta[0]) - 0.5f * Math::Pi<f32>();
    else
    {
        const f32v3 dir = Math::Normalize(delta);
        const f32v3 r{dir[2], 0.f, -dir[0]};
        const f32 theta = 0.5f * Math::AntiCosine(dir[1]);
        if (!TKit::ApproachesZero(Math::NormSquared(r)))
            return f32q{Math::Cosine(theta), Math::Normalize(r) * Math::Sine(theta)};
        if (dir[1] < 0.f)
            return f32q{0.f, 1.f, 0.f, 0.f};
        return Detail::RotType<D>::Identity;
    }
}
template <Dimension D>
void IRenderContext<D>::Line(const Asset mesh, const f32v<D> &start, const f32v<D> &end, const f32 thickness)
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
template <Dimension D> void IRenderContext<D>::Axes(const Asset mesh, const AxesParameters &params)
{
    if constexpr (D == D2)
    {
        Color &color = m_Current->FillColor;
        const Color oldColor = color; // A cheap filthy push

        const f32v2 xLeft = f32v2{-params.Size, 0.f};
        const f32v2 xRight = f32v2{params.Size, 0.f};

        const f32v2 yDown = f32v2{0.f, -params.Size};
        const f32v2 yUp = f32v2{0.f, params.Size};

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

        const f32v3 xLeft = f32v3{-params.Size, 0.f, 0.f};
        const f32v3 xRight = f32v3{params.Size, 0.f, 0.f};

        const f32v3 yDown = f32v3{0.f, -params.Size, 0.f};
        const f32v3 yUp = f32v3{0.f, params.Size, 0.f};

        const f32v3 zBack = f32v3{0.f, 0.f, -params.Size};
        const f32v3 zFront = f32v3{0.f, 0.f, params.Size};

        color = Color{245u, 64u, 90u};
        Line(mesh, xLeft, xRight, params.Thickness);
        color = Color{180u, 245u, 65u};
        Line(mesh, yDown, yUp, params.Thickness);
        color = Color{65u, 135u, 245u};
        Line(mesh, zBack, zFront, params.Thickness);
        color = oldColor; // A cheap filthy pop
    }
}

template class Detail::IRenderContext<D2>;
template class Detail::IRenderContext<D3>;

} // namespace Onyx
