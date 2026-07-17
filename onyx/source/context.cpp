#include "pch.hpp"
#include "onyx/context.hpp"
#include "instance.hpp"
#include "buffer.hpp"
#include "resources.hpp"
#include "tkit/math/math.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx
{
using namespace Detail;

template <Dimension D> IRenderContext<D>::IRenderContext(const u32 immediateDynamicMeshCapacity)
{
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

    m_State.Font = m_DefaultResources.Font;
    m_State.Sampler = m_DefaultResources.Sampler;

    m_ImmediateDynamicMeshes.Reserve(immediateDynamicMeshCapacity);

    for (u32 i = 0; i < immediateDynamicMeshCapacity; ++i)
        m_ImmediateDynamicMeshes.Append(Resources::RegisterDynamicMesh<D>());

    if (immediateDynamicMeshCapacity != 0)
        Resources::Sync(SyncFlag_DynamicMeshes); // have a close look at this. idk why i feel it may cause issues
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
    for (const DynamicMeshInfo<D> &info : m_ImmediateDynamicMeshes)
        // we perform this chech bc resources might have been destroyed already if this context is being destroyed on
        // teardown
        if (Resources::IsResourceValid(info.Handle))
            Resources::DestroyDynamicMesh<D>(info.Handle);
}

template <Dimension D> void IRenderContext<D>::Flush()
{
    TKIT_ASSERT(
        m_StateStack.IsEmpty(),
        "[ONYX][CONTEXT] Mismatched Push()/Pop() pair found. For every Push(), there must be a Pop(). Stack has {} "
        "entries left",
        m_StateStack.GetSize());

    m_State = ContextState<D>{};
    m_DefaultResources = Resources::GetDefaultResources();

    m_State.Font = m_DefaultResources.Font;
    m_State.Sampler = m_DefaultResources.Sampler;

    NoClip();
    for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
        for (InstanceDataArrays *instanceData : m_InstanceData[bpass])
        {
            instanceData->Circles.Instances = 0;

            instanceData->DynamicMeshes.Registry.Clear();
            for (InstanceDataBuffer &dynInstances : instanceData->DynamicMeshes.Instances)
                dynInstances.Instances = 0;

            for (u32 j = 0; j < Resource_MeshPoolCount; ++j)
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
    m_DynamicMeshCounter = 0;
    m_PointLightData.Clear();
    m_DirectionalLightData.Clear();
}

#define CHECK_HANDLE(handle, rtype, dim)                                                                               \
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);                                                                           \
    ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(handle);                                                                      \
    ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(handle, rtype, dim);                                                    \
    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(handle, rtype, dim);

#ifdef TKIT_ENABLE_ENSURE
template <Dimension D>
void checkSampler(const Resource sampler, const Resource material = NullHandle,
                  const TextureSlot slot = TextureSlot_Count)
{
    TKIT_COMPILER_WARNING_IGNORE_PUSH()
    TKIT_MSVC_WARNING_IGNORE(4127)
    if (material == NullHandle)
    {
        TKIT_ENSURE(IsResourceNull(sampler) || Resources::IsResourceValid<D>(sampler, Resource_Sampler),
                    "[ONYX][CONTEXT] The sampler handle {:#010x} is invalid and is not "
                    "an explicit null sampler",
                    sampler);
    }
    else if (D == D3 && slot != TextureSlot_Count)
    {
        TKIT_ENSURE(IsResourceNull(sampler) || Resources::IsResourceValid<D>(sampler, Resource_Sampler),
                    "[ONYX][CONTEXT] The sampler handle {:#010x} from the material handle {:#010x} at texture "
                    "slot '{}' is "
                    "invalid and is not an explicit null sampler",
                    sampler, material, ToString(slot));
    }
    else
    {
        TKIT_ENSURE(
            IsResourceNull(sampler) || Resources::IsResourceValid<D>(sampler, Resource_Sampler),
            "[ONYX][CONTEXT] The sampler handle {:#010x} from the material handle {:#010x} is invalid and is not "
            "an explicit null sampler",
            sampler, material);
    }
    TKIT_COMPILER_WARNING_IGNORE_POP()
}
template <Dimension D>
void checkTexture(const Resource tex, const Resource material = NullHandle, const TextureSlot slot = TextureSlot_Count)
{
    TKIT_COMPILER_WARNING_IGNORE_PUSH()
    TKIT_MSVC_WARNING_IGNORE(4127)
    if (material == NullHandle)
    {
        TKIT_ENSURE(IsResourceNull(tex) || Resources::IsResourceValid<D>(tex, Resource_Texture),
                    "[ONYX][CONTEXT] The texture handle {:#010x} is invalid and is not "
                    "an explicit null texture",
                    tex);
    }
    else if (D == D3 && slot != TextureSlot_Count)
    {
        TKIT_ENSURE(IsResourceNull(tex) || Resources::IsResourceValid<D>(tex, Resource_Texture),
                    "[ONYX][CONTEXT] The texture handle {:#010x} from the material handle {:#010x} at texture "
                    "slot '{}' is "
                    "invalid and is not an explicit null texture",
                    tex, material, ToString(slot));
    }
    else
    {
        TKIT_ENSURE(
            IsResourceNull(tex) || Resources::IsResourceValid<D>(tex, Resource_Texture),
            "[ONYX][CONTEXT] The texture handle {:#010x} from the material handle {:#010x} is invalid and is not "
            "an explicit null texture",
            tex, material);
    }
    TKIT_COMPILER_WARNING_IGNORE_POP()
}
template <Dimension D> void checkMaterial(const Resource material)
{
    TKIT_ENSURE(IsResourceNull(material) || Resources::IsResourceValid<D>(material, Resource_Material),
                "[ONYX][CONTEX] The material handle {:#010x} is invalid and is not an explicit null material",
                material);
    if (!IsResourceNull(material))
    {
        const MaterialData<D> &data = Resources::GetMaterialData<D>(material);
        if constexpr (D == D2)
        {
            checkSampler<D>(data.Sampler, material);
            checkTexture<D>(data.Texture, material);
        }
        else
            for (u32 i = 0; i < TextureSlot_Count; ++i)
            {
                const TextureSlot slot = TextureSlot(i);
                checkSampler<D>(data.Samplers[i], material, slot);
                checkTexture<D>(data.Textures[i], material, slot);
            }
    }
}
#endif

u16 F32ToF16(const f32 f)
{
    u32 bits;
    std::memcpy(&bits, &f, 4);

    i32 s = (bits >> 16) & 0x8000;
    i32 e = ((bits >> 23) & 0xFF) - 112; // 127 - 15 = 112
    i32 m = bits & 0x007FFFFF;

    if (e <= 0)
    {
        if (e < -10)
            return u16(s);

        m = (m | 0x00800000) >> (1 - e);
        if (m & 0x00001000)
            m += 0x00002000;

        return u16(s | (m >> 13));
    }

    if (e == 143) // 0xFF - 112
    {
        if (m == 0)
            return u16(s | 0x7C00);

        m >>= 13;
        return u16(s | 0x7C00 | m | (m == 0));
    }

    if (m & 0x00001000)
    {
        m += 0x00002000;
        if (m & 0x00800000)
        {
            m = 0;
            e += 1;
        }
    }

    if (e > 30)
        return u16(s | 0x7C00);

    return u16(s | (e << 10) | (m >> 13));
}

u32 PackHalf2x16(const f32v2 &x)
{
    return u32(F32ToF16(x[0])) | (u32(F32ToF16(x[1])) << 16);
}

template <Dimension D> u32 packAlignment(const vec<Alignment, D> alg)
{
    if constexpr (D == D2)
        return u32(alg[1]) << 8 | u32(alg[0]);
    else
        return u32(alg[2]) << 16 | u32(alg[1]) << 8 | u32(alg[0]);
}

template <Dimension D>
static InstanceData<D> createInstanceData(const ContextState<D> &state, const f32m<D> &transform,
                                          const u32 depthCounter)
{
#ifdef TKIT_ENABLE_ENSURE
    checkMaterial<D>(state.Material);
    checkSampler<D>(state.Sampler);
    checkTexture<D>(state.Texture);
#endif
    const bool flat = state.RenderFlags & RenderModeFlag_Flat;
    InstanceData<D> instanceData;
    instanceData.Transform = CreateTransformData<D>(transform);
    instanceData.Rect = state.Rect;
    instanceData.MatOrSamplerTex =
        flat ? Resources::CombineSamplerTexIntoId(state.Sampler, state.Texture) : GetResourceId(state.Material);
    instanceData.TexOffset = PackHalf2x16(state.TexOffset);
    instanceData.TexScale = PackHalf2x16(state.TexScale);
    instanceData.FillColor = state.FillColor.ToLinear().Pack();
    instanceData.OutlineColor = state.OutlineColor.ToLinear().Pack();
    instanceData.OutlineWidth = state.OutlineWidth;
    if constexpr (D == D2)
        instanceData.DepthCounter = depthCounter;

    return instanceData;
}

template <Dimension D>
static StaticInstanceData<D> createStaticInstanceData(const ContextState<D> &state, const f32m<D> &transform,
                                                      const Resource bounds, const u32 depthCounter)
{
    StaticInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.Alignment = packAlignment<D>(state.Alignment);
    instanceData.BoundsId = GetResourceId(bounds);
    return instanceData;
}

template <Dimension D>
static CircleInstanceData<D> createCircleInstanceData(const ContextState<D> &state, const f32m<D> &transform,
                                                      const CircleParameters &params, const u32 depthCounter)
{
    // constexpr TKit::FixedArray<f32v2, 9> bounds = {f32v2{-0.5f, -0.5f}, f32v2{-0.5f, 0.f}, f32v2{-0.5f, 0.5f},
    //                                                f32v2{0.f, -0.5f},   f32v2{0.f, 0.f},   f32v2{0.f, 0.5f},
    //                                                f32v2{0.5f, -0.5f},  f32v2{0.5f, 0.f},  f32v2{0.5f, 0.5f}};
    //
    // const f32v2 &alignment = bounds[state.Alignment[0] * 3 + state.Alignment[1]];
    CircleInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.Alignment = packAlignment<D>(state.Alignment);

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
static ParametricInstanceData<D> createParametricInstanceData(const ContextState<D> &state, const f32m<D> &transform,
                                                              const Resource bounds, const ParametricShape shape,
                                                              const InstanceParameters &params, const u32 depthCounter)
{
    ParametricInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.Alignment = packAlignment<D>(state.Alignment);
    instanceData.BoundsId = GetResourceId(bounds);
    instanceData.Shape = shape;
    instanceData.Parameters = params;
    return instanceData;
}

template <Dimension D>
static GlyphInstanceData<D> createGlyphInstanceData(const ContextState<D> &state, const f32m<D> &transform,
                                                    const f32 unitRange, const u32 depthCounter)
{
    GlyphInstanceData<D> instanceData;
    instanceData.Data = createInstanceData(state, transform, depthCounter);
    instanceData.SamplerAtlasId =
        Resources::CombineSamplerTexIntoId(state.Sampler, Resources::GetFontAtlas(state.Font));
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
    case Resource_DynamicMesh:
        return Geometry_Dynamic;
    default:
        TKIT_FATAL("[ONYX][RENDERER] The resource type '{}' does not have a geometry associated", ToString(rtype));
        return Geometry_Count;
    }
}

// NOTE(Isma): For vertex geometry it is important that *creating new vertex/index arrays* triggers this function
// through a FlushAllContexts. meaning creating them requires a Sync() call.
template <Dimension D> void IRenderContext<D>::resizeInstanceData()
{
    const auto resize = [](const ResourceType rtype, InstanceResourceGroup &group, const u32 count, const u32 ncount) {
        for (u32 k = count; k < ncount; ++k)
        {
            InstanceDataBuffer &buffer = group.Instances.Append();
            const u32 isize = GetInstanceSize<D>(getGeometry(rtype));
            buffer.Data = VKit::HostBuffer{isize * ONYX_BUFFER_INITIAL_CAPACITY};
            buffer.Capacity = ONYX_BUFFER_INITIAL_CAPACITY;
            buffer.InstanceSize = isize;
            buffer.Instances = 0;
        }
    };

    ForEachResourceGroup<D>([&](const u32 bpass, const u32 rmode, const u32 mtype, const u32 pid) {
        InstanceResourceGroup &group = m_InstanceData[bpass][rmode]->Meshes[mtype][pid];
        const ResourceType rtype = ResourceType(mtype);

        const u32 count = group.Instances.GetSize();
        const u32 ncount = Resources::GetResourceCount<D>(CreateResourcePoolHandle(rtype, pid));
        resize(rtype, group, count, ncount);
    });

    ForEachDynamicMeshResourceGroup<D>([&](const u32 bpass, const u32 rmode) {
        InstanceResourceGroup &group = m_InstanceData[bpass][rmode]->DynamicMeshes;
        const u32 count = group.Instances.GetSize();
        const u32 ncount = Resources::GetDynamicMeshCount<D>();
        resize(Resource_DynamicMesh, group, count, ncount);
    });
}

template <Dimension D> WorldRect<D> IRenderContext<D>::computeWorldRect(const ClipRect<D> &clip)
{
    WorldRect<D> wrect;
    if (clip.Min[0] == TKIT_F32_MIN)
    {
        wrect.Min[0] = TKIT_F32_MIN;
        return wrect;
    }
    if constexpr (D == D2)
    {
        const f32m3 &t = m_State.Transform;
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
        const f32m4 &t = m_State.Transform;
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
        const Alignment alg = m_State.Alignment[i];
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
    if (!m_State.RenderFlags)
        return;
    const CircleInstanceData<D> idata = createCircleInstanceData(m_State, transform, params, ++m_DepthCounter);
    InstanceDataBuffer &buffer = m_InstanceData[m_State.Blend][GetRenderMode(m_State.RenderFlags)]->Circles;
    addInstanceData(buffer, idata);
}

template <Dimension D> void IRenderContext<D>::addStaticData(const Resource mesh, const f32m<D> &transform)
{
    if (!m_State.RenderFlags)
        return;
    CHECK_HANDLE(mesh, Resource_StaticMesh, D);

    const u32 pid = GetResourcePoolId(mesh);
    const u32 mid = GetResourceId(mesh);

    const StaticInstanceData<D> idata =
        createStaticInstanceData(m_State, transform, Resources::GetMeshBounds<D>(mesh), ++m_DepthCounter);

    InstanceResourceGroup &group =
        m_InstanceData[m_State.Blend][GetRenderMode(m_State.RenderFlags)]->Meshes[Resource_StaticMesh][pid];

    group.Registry.RegisterResourceId(mid);
    addInstanceData(group.Instances[mid], idata);
}
template <Dimension D> void IRenderContext<D>::addDynamicData(const Resource mesh, const f32m<D> &transform)
{
    if (!m_State.RenderFlags)
        return;
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(mesh);
    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(mesh, Resource_DynamicMesh, D);

    const u32 mid = GetResourceId(mesh);
    const DynamicInstanceData<D> idata = createInstanceData(m_State, transform, ++m_DepthCounter);

    InstanceResourceGroup &group = m_InstanceData[m_State.Blend][GetRenderMode(m_State.RenderFlags)]->DynamicMeshes;
    group.Registry.RegisterResourceId(mid);
    addInstanceData(group.Instances[mid], idata);
}
template <Dimension D>
void IRenderContext<D>::addParametricData(const Resource mesh, const f32m<D> &transform,
                                          const InstanceParameters &params)
{
    if (!m_State.RenderFlags)
        return;
    CHECK_HANDLE(mesh, Resource_ParametricMesh, D);

    const u32 pid = GetResourcePoolId(mesh);
    const u32 mid = GetResourceId(mesh);

    const ParametricShape shape = Resources::GetParametricShape<D>(mesh);

    const ParametricInstanceData<D> idata = createParametricInstanceData(
        m_State, transform, Resources::GetMeshBounds<D>(mesh), shape, params, ++m_DepthCounter);

    InstanceResourceGroup &group =
        m_InstanceData[m_State.Blend][GetRenderMode(m_State.RenderFlags)]->Meshes[Resource_ParametricMesh][pid];

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
    if (!m_State.RenderFlags || text.IsEmpty())
        return;

    TKIT_ASSERT(m_State.Sampler != NullHandle,
                "[ONYX][CONTEXT] To draw text, a valid sampler must be provided first with the Font() method");

    CHECK_HANDLE(m_State.Font, Resource_Font, D);
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(m_State.Sampler);
    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(m_State.Sampler, Resource_Sampler, D);

    ++m_DepthCounter;
    const Alignment alg0 = m_State.Alignment[0] == Alignment_None ? Alignment_Left : m_State.Alignment[0];
    const Alignment alg1 = m_State.Alignment[1] == Alignment_None ? Alignment_Top : m_State.Alignment[1];

    const Resource font = m_State.Font;
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
    const u32 pid = GetResourcePoolId(glyph);
    const u32 gid = GetResourceId(glyph);

    const GlyphInstanceData<D> idata = createGlyphInstanceData(m_State, transform, unitRange, m_DepthCounter);
    InstanceResourceGroup &group =
        m_InstanceData[m_State.Blend][GetRenderMode(m_State.RenderFlags)]->Meshes[Resource_GlyphMesh][pid];

    group.Registry.RegisterResourceId(gid);
    addInstanceData(group.Instances[gid], idata);
}
template <Dimension D> void IRenderContext<D>::addGlyphData(const Resource glyph, const f32m<D> &transform)
{
    TKIT_ASSERT(m_State.Sampler != NullHandle,
                "[ONYX][CONTEXT] To draw text, a valid sampler must be provided first with the Font() method");

    ONYX_CHECK_RESOURCE_IS_NOT_NULL(m_State.Sampler);
    ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(m_State.Sampler, Resource_Sampler, D);

    ++m_DepthCounter;
    const Resource font = Resources::GetFont(glyph);
    const FontData &fdata = Resources::GetFontData(font);

    const Alignment al0 = m_State.Alignment[0];
    const Alignment al1 = m_State.Alignment[1];
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

template <Dimension D>
void IRenderContext<D>::translateLayoutElement(const LayoutDrawInfo &info, [[maybe_unused]] u32 *depth)
{
    if constexpr (D == D2)
        Translate(info.Position, Transform_Intrinsic);
    else
    {
        const f32 z = depth ? (1.f - f32(++(*depth)) / f32(1U << 24)) : 0.f;
        Translate(f32v3{info.Position, z}, Transform_Intrinsic);
    }
}
template <Dimension D> void IRenderContext<D>::scaleLayoutElement(const LayoutDrawInfo &info)
{
    if constexpr (D == D2)
        Scale(info.Size, Transform_Intrinsic);
    else
        Scale(f32v3{info.Size, 1.f}, Transform_Intrinsic);
}

template <Dimension D> void IRenderContext<D>::Layout(const Onyx::Layout &layout)
{
    TKIT_PROFILE_NSCOPE("Onyx::Context::Layout");
    u32 depth = 0;
    BeginLayoutElements();
    for (const LayoutDrawInfo &info : layout.GetDrawInfo())
        if (info.Flags & LayoutElementFlag_Drawable)
            LayoutElement(info, &depth);
    EndLayoutElements();
}
template <Dimension D> void IRenderContext<D>::LayoutElement(const LayoutDrawInfo &element, u32 *depthCounter3D)
{
    Push();
    RenderFlags(element.RenderFlags);
    Texture(element.Texture, element.TexOffset, element.TexScale);
    Material(element.Material);
    FillColor(element.FillColor);
    OutlineColor(element.OutlineColor);
    OutlineWidth(element.OutlineWidth);
    if (element.Flags & LayoutElementFlag_ForceBlend)
        Blend();

    ClipRect<D> rect;
    if constexpr (D == D2)
    {
        rect.Min = element.ClipMin;
        rect.Max = element.ClipMax;
    }
    else
    {
        // NOTE(Isma): A reasonably large number to not clip in the z axis
        rect.Min = f32v3{element.ClipMin, -1e8};
        rect.Max = f32v3{element.ClipMax, 1e8};
    }
    Clip(rect);

    switch (element.ShapeType)
    {
    case LayoutShape_Circle:
        translateLayoutElement(element, depthCounter3D);
        scaleLayoutElement(element);
        Circle();
        break;

    case LayoutShape_Static:
    case LayoutShape_Rectangle:
        translateLayoutElement(element, depthCounter3D);
        scaleLayoutElement(element);
        StaticMesh(element.Handle);
        break;

    case LayoutShape_Dynamic:
        translateLayoutElement(element, depthCounter3D);
        scaleLayoutElement(element);
        DynamicMesh(element.Handle);
        break;

    case LayoutShape_RoundedRectangle:
        translateLayoutElement(element, depthCounter3D);
        ParametricMesh(
            element.Handle,
            RoundedRectParameters{.Width = element.Size[0], .Height = element.Size[1], .Radius = element.Radius});
        break;
    case LayoutShape_Glyph:
        translateLayoutElement(element, depthCounter3D);
        scaleLayoutElement(element);

        Glyph(element.Handle);
        break;
    case LayoutShape_Text:
        translateLayoutElement(element, depthCounter3D);
        scaleLayoutElement(element);

        Font(element.Handle);
        Text(element.Text);
        break;

    case LayoutShape_Unicode:
        translateLayoutElement(element, depthCounter3D);
        scaleLayoutElement(element);

        Font(element.Handle);
        Unicode(element.Unicode);
        break;

    default:
        break;
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

    f32m<D> transform = m_State.Transform;
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
        Color &color = m_State.FillColor;
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
        Color &color = m_State.FillColor;
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
