#include "onyx/core/pch.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/asset/assets.hpp"
#include "tkit/math/math.hpp"

namespace Onyx
{
using namespace Detail;

template <Dimension D> IRenderContext<D>::IRenderContext()
{
    m_Current = &m_StateStack.Append();
    for (u32 i = 0; i < m_InstanceData.GetSize(); ++i)
    {
        TKit::TierArray<InstanceBuffer> &data = m_InstanceData[i];
        data.Resize(Assets::GetBatchCount());

        for (u32 j = Assets::GetBatchStart(Geometry_Circle); j < Assets::GetBatchEnd(Geometry_Circle); ++j)
        {
            InstanceBuffer &buffer = data[j];
            buffer.Data = VKit::HostBuffer::Create<CircleInstanceData<D>>(ONYX_BUFFER_INITIAL_CAPACITY);
        }
        for (u32 j = Assets::GetBatchStart(Geometry_StaticMesh); j < Assets::GetBatchEnd(Geometry_StaticMesh); ++j)
        {
            InstanceBuffer &buffer = data[j];
            buffer.Data = VKit::HostBuffer::Create<InstanceData<D>>(ONYX_BUFFER_INITIAL_CAPACITY);
        }
    }
}

template <Dimension D> void IRenderContext<D>::Flush()
{
    TKIT_ASSERT(m_StateStack.GetSize() == 1,
                "[ONYX][CONTEXT] Mismatched Push() call found. For every Push(), there must be a Pop()");

    m_StateStack[0] = RenderState<D>{};
    m_Current = &m_StateStack.GetFront();
    for (u32 i = 0; i < m_InstanceData.GetSize(); ++i)
        for (u32 j = 0; j < m_InstanceData[i].GetSize(); ++j)
            m_InstanceData[i][j].Instances = 0;
    ++m_Generation;
}

template <Dimension D> void IRenderContext<D>::Transform(const f32m<D> &transform)
{
    m_Current->Transform = transform * m_Current->Transform;
}
template <Dimension D>
void IRenderContext<D>::Transform(const f32v<D> &translation, const f32v<D> &scale, const rot<D> &rotation)
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(translation, scale, rotation));
}
template <Dimension D>
void IRenderContext<D>::Transform(const f32v<D> &translation, const f32 scale, const rot<D> &rotation)
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(translation, f32v<D>{scale}, rotation));
}
void RenderContext<D3>::Transform(const f32v3 &translation, const f32v3 &scale, const f32v3 &rotation)
{
    this->Transform(Onyx::Transform<D3>::ComputeTransform(translation, scale, f32q{rotation}));
}
void RenderContext<D3>::Transform(const f32v3 &translation, const f32 scale, const f32v3 &rotation)
{
    this->Transform(Onyx::Transform<D3>::ComputeTransform(translation, f32v3{scale}, f32q{rotation}));
}

template <Dimension D> void IRenderContext<D>::Translate(const f32v<D> &translation)
{
    Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, translation);
}
template <Dimension D> void IRenderContext<D>::SetTranslation(const f32v<D> &translation)
{
    m_Current->Transform[D][0] = translation[0];
    m_Current->Transform[D][1] = translation[1];
    if constexpr (D == D3)
        m_Current->Transform[D][2] = translation[2];
}

template <Dimension D> void IRenderContext<D>::Scale(const f32v<D> &scale)
{
    Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, scale);
}
template <Dimension D> void IRenderContext<D>::Scale(const f32 scale)
{
    Scale(f32v<D>{scale});
}

template <Dimension D> void IRenderContext<D>::TranslateX(const f32 x)
{
    Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, 0, x);
}
template <Dimension D> void IRenderContext<D>::TranslateY(const f32 y)
{
    Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, 1, y);
}
void RenderContext<D3>::TranslateZ(const f32 z)
{
    Onyx::Transform<D3>::TranslateExtrinsic(m_Current->Transform, 2, z);
}

template <Dimension D> void IRenderContext<D>::SetTranslationX(const f32 x)
{
    m_Current->Transform[D][0] = x;
}
template <Dimension D> void IRenderContext<D>::SetTranslationY(const f32 y)
{
    m_Current->Transform[D][1] = y;
}
void RenderContext<D3>::SetTranslationZ(const f32 z)
{
    m_Current->Transform[D3][2] = z;
}
template <Dimension D> void IRenderContext<D>::ScaleX(const f32 x)
{
    Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, 0, x);
}
template <Dimension D> void IRenderContext<D>::ScaleY(const f32 y)
{
    Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, 1, y);
}
void RenderContext<D3>::ScaleZ(const f32 z)
{
    Onyx::Transform<D3>::ScaleExtrinsic(m_Current->Transform, 2, z);
}

void RenderContext<D2>::Rotate(const f32 angle)
{
    Onyx::Transform<D2>::RotateExtrinsic(m_Current->Transform, angle);
}

void RenderContext<D3>::Rotate(const f32q &quaternion)
{
    Onyx::Transform<D3>::RotateExtrinsic(m_Current->Transform, quaternion);
}
void RenderContext<D3>::Rotate(const f32 angle, const f32v3 &axis)
{
    Rotate(f32q::FromAngleAxis(angle, axis));
}
void RenderContext<D3>::Rotate(const f32v3 &angles)
{
    Rotate(f32q(angles));
}

// This could be optimized a bit
void RenderContext<D3>::RotateX(const f32 angle)
{
    Rotate(f32v3{angle, 0.f, 0.f});
}
void RenderContext<D3>::RotateY(const f32 angle)
{
    Rotate(f32v3{0.f, angle, 0.f});
}
void RenderContext<D3>::RotateZ(const f32 angle)
{
    Rotate(f32v3{0.f, 0.f, angle});
}

template <Dimension D, typename F> static void resolveStencilPassWithState(const RenderState<D> *state, F &&draw)
{
    if (state->Flags & RenderStateFlag_Fill)
    {
        if (state->Flags & RenderStateFlag_Outline)
        {
            std::forward<F>(draw)(StencilPass_DoStencilWriteDoFill);
            std::forward<F>(draw)(StencilPass_DoStencilTestNoFill);
        }
        else
            std::forward<F>(draw)(StencilPass_NoStencilWriteDoFill);
    }
    else if (state->Flags & RenderStateFlag_Outline)
    {
        std::forward<F>(draw)(StencilPass_DoStencilWriteNoFill);
        std::forward<F>(draw)(StencilPass_DoStencilTestNoFill);
    }
}

template <Dimension D> void IRenderContext<D>::updateState()
{
    m_Current = &m_StateStack.GetBack();
}

template <Dimension D> void IRenderContext<D>::StaticMesh(const Mesh mesh)
{
    const auto draw = [&, mesh](const StencilPass pass) { addStaticMeshData(mesh, m_Current->Transform, pass); };
    resolveStencilPassWithState(m_Current, draw);
}
template <Dimension D> void IRenderContext<D>::StaticMesh(const Mesh mesh, const f32m<D> &transform)
{
    const auto draw = [&, mesh](const StencilPass pass) {
        addStaticMeshData(mesh, transform * m_Current->Transform, pass);
    };
    resolveStencilPassWithState(m_Current, draw);
}

template <Dimension D> void IRenderContext<D>::Circle(const CircleOptions &options)
{
    const auto draw = [&](const StencilPass pass) { addCircleData(m_Current->Transform, options, pass); };
    resolveStencilPassWithState(m_Current, draw);
}
template <Dimension D> void IRenderContext<D>::Circle(const f32m<D> &transform, const CircleOptions &options)
{
    const auto draw = [&](const StencilPass pass) { addCircleData(transform * m_Current->Transform, options, pass); };
    resolveStencilPassWithState(m_Current, draw);
}

template <Dimension D>
static InstanceData<D> createInstanceData(const RenderState<D> *state, const f32m<D> &transform, const StencilPass pass)
{
    if constexpr (D == D2)
    {
        InstanceData<D2> instanceData{};
        instanceData.Basis1 = f32v2{transform[0]};
        instanceData.Basis2 = f32v2{transform[1]};
        instanceData.Basis3 = f32v2{transform[2]};
        instanceData.TexIndex = TKIT_U32_MAX;
        if (pass == StencilPass_NoStencilWriteDoFill || pass == StencilPass_DoStencilWriteDoFill)
            instanceData.BaseColor = state->FillColor.Pack();
        else
        {
            instanceData.BaseColor = state->OutlineColor.Pack();
            instanceData.OutlineWidth = state->OutlineWidth;
        }
        return instanceData;
    }
    else
    {
        InstanceData<D3> instanceData{};
        instanceData.Basis1 = f32v4{transform[0][0], transform[1][0], transform[2][0], transform[3][0]};
        instanceData.Basis2 = f32v4{transform[0][1], transform[1][1], transform[2][1], transform[3][1]};
        instanceData.Basis3 = f32v4{transform[0][2], transform[1][2], transform[2][2], transform[3][2]};
        if (pass == StencilPass_NoStencilWriteDoFill || pass == StencilPass_DoStencilWriteDoFill)
        {
            instanceData.BaseColor = state->FillColor.Pack();
            instanceData.MatIndex = TKIT_U32_MAX;
        }
        else
        {
            instanceData.BaseColor = state->OutlineColor.Pack();
            instanceData.OutlineWidth = state->OutlineWidth;
        }
        return instanceData;
    }
}

template <Dimension D>
static CircleInstanceData<D> createCircleInstanceData(const RenderState<D> *state, const f32m<D> &transform,
                                                      const CircleOptions &options, const StencilPass pass)
{
    CircleInstanceData<D> instanceData;
    instanceData.BaseData = createInstanceData(state, transform, pass);
    instanceData.LowerCos = Math::Cosine(options.LowerAngle);
    instanceData.LowerSin = Math::Sine(options.LowerAngle);
    instanceData.UpperCos = Math::Cosine(options.UpperAngle);
    instanceData.UpperSin = Math::Sine(options.UpperAngle);

    instanceData.AngleOverflow = Math::Absolute(options.UpperAngle - options.LowerAngle) > Math::Pi<f32>() ? 1 : 0;
    instanceData.Hollowness = options.Hollowness;
    instanceData.InnerFade = options.InnerFade;
    instanceData.OuterFade = options.OuterFade;

    return instanceData;
}

template <Dimension D> void IRenderContext<D>::resizeBuffer(InstanceBuffer &buffer)
{
    if (buffer.Instances > buffer.Data.GetInstanceCount())
    {
        const u32 ninst = static_cast<u32>(1.5f * static_cast<f32>(buffer.Instances));
        buffer.Data.Resize(ninst);
    }
}

template <Dimension D>
template <typename T>
void IRenderContext<D>::addInstanceData(InstanceBuffer &buffer, const T &data)
{
    const u32 index = buffer.Instances++;
    resizeBuffer(buffer);
    buffer.Data.WriteAt(index, &data);
}

template <Dimension D>
void IRenderContext<D>::addCircleData(const f32m<D> &transform, const CircleOptions &options, const StencilPass pass)
{
    const CircleInstanceData<D> idata = createCircleInstanceData(m_Current, transform, options, pass);
    InstanceBuffer &buffer = m_InstanceData[pass][Assets::GetCircleBatchIndex()];
    addInstanceData(buffer, idata);
}

template <Dimension D>
void IRenderContext<D>::addStaticMeshData(const Mesh mesh, const f32m<D> &transform, const StencilPass pass)
{
    const InstanceData<D> idata = createInstanceData(m_Current, transform, pass);
    InstanceBuffer &buffer = m_InstanceData[pass][Assets::GetStaticMeshBatchIndex(mesh)];
    addInstanceData(buffer, idata);
}

template <Dimension D> static rot<D> computeLineRotation(const f32v<D> &start, const f32v<D> &end)
{
    const f32v<D> delta = end - start;
    if constexpr (D == D2)
        return Math::AntiTangent(delta[1], delta[0]);
    else
    {
        const f32v3 dir = Math::Normalize(delta);
        const f32v3 r{0.f, -dir[2], dir[1]};
        const f32 theta = 0.5f * Math::AntiCosine(dir[0]);
        if (!TKit::ApproachesZero(Math::NormSquared(r)))
            return f32q{Math::Cosine(theta), Math::Normalize(r) * Math::Sine(theta)};
        if (dir[0] < 0.f)
            return f32q{0.f, 0.f, 1.f, 0.f};
        return Detail::RotType<D>::Identity;
    }
}

template <Dimension D>
void IRenderContext<D>::Line(const Mesh mesh, const f32v<D> &start, const f32v<D> &end, const f32 thickness)
{
    const f32v<D> delta = end - start;

    f32m<D> transform = m_Current->Transform;
    Onyx::Transform<D>::TranslateIntrinsic(transform, 0.5f * (start + end));
    Onyx::Transform<D>::RotateIntrinsic(transform, computeLineRotation<D>(start, end));
    if constexpr (D == D2)
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v2{Math::Norm(delta), thickness});
    else
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v3{Math::Norm(delta), thickness, thickness});
    const auto draw = [&, mesh](const StencilPass pass) { addStaticMeshData(mesh, transform, pass); };
    resolveStencilPassWithState(m_Current, draw);
}
template <Dimension D> void IRenderContext<D>::Axes(const Mesh mesh, const AxesOptions &options)
{
    if constexpr (D == D2)
    {
        Color &color = m_Current->FillColor;
        const Color oldColor = color; // A cheap filthy push

        const f32v2 xLeft = f32v2{-options.Size, 0.f};
        const f32v2 xRight = f32v2{options.Size, 0.f};

        const f32v2 yDown = f32v2{0.f, -options.Size};
        const f32v2 yUp = f32v2{0.f, options.Size};

        color = Color{245u, 64u, 90u};
        Line(mesh, xLeft, xRight, options.Thickness);
        color = Color{65u, 135u, 245u};
        Line(mesh, yDown, yUp, options.Thickness);

        color = oldColor; // A cheap filthy pop
    }
    else
    {
        Color &color = m_Current->FillColor;
        const Color oldColor = color; // A cheap filthy push

        const f32v3 xLeft = f32v3{-options.Size, 0.f, 0.f};
        const f32v3 xRight = f32v3{options.Size, 0.f, 0.f};

        const f32v3 yDown = f32v3{0.f, -options.Size, 0.f};
        const f32v3 yUp = f32v3{0.f, options.Size, 0.f};

        const f32v3 zBack = f32v3{0.f, 0.f, -options.Size};
        const f32v3 zFront = f32v3{0.f, 0.f, options.Size};

        color = Color{245u, 64u, 90u};
        Line(mesh, xLeft, xRight, options.Thickness);
        color = Color{180u, 245u, 65u};
        Line(mesh, yDown, yUp, options.Thickness);
        color = Color{65u, 135u, 245u};
        Line(mesh, zBack, zFront, options.Thickness);
        color = oldColor; // A cheap filthy pop
    }
}

template <Dimension D> void IRenderContext<D>::AddFlags(const RenderStateFlags flags)
{
    m_Current->Flags |= flags;
}
template <Dimension D> void IRenderContext<D>::RemoveFlags(const RenderStateFlags flags)
{
    m_Current->Flags &= ~flags;
}

template <Dimension D> void IRenderContext<D>::Fill(const bool enabled)
{
    if (enabled)
        AddFlags(RenderStateFlag_Fill);
    else
        RemoveFlags(RenderStateFlag_Fill);
}
template <Dimension D> void IRenderContext<D>::Push(const RenderState<D> &state)
{
    m_StateStack.Append(state);
    updateState();
}
template <Dimension D> void IRenderContext<D>::Push()
{
    Push(*m_Current);
}
template <Dimension D> void IRenderContext<D>::Pop()
{
    TKIT_ASSERT(m_StateStack.GetSize() > 1, "[ONYX][CONTEXT] For every Push(), there must be a Pop()");
    m_StateStack.Pop();
    updateState();
}

template <Dimension D> void IRenderContext<D>::FillColor(const Color &color)
{
    m_Current->FillColor = color;
}
template <Dimension D> void IRenderContext<D>::Outline(const bool enabled)
{
    if (enabled)
        AddFlags(RenderStateFlag_Outline);
    else
        RemoveFlags(RenderStateFlag_Outline);
}
template <Dimension D> void IRenderContext<D>::OutlineColor(const Color &color)
{
    m_Current->OutlineColor = color;
}
template <Dimension D> void IRenderContext<D>::OutlineWidth(const f32 width)
{
    m_Current->OutlineWidth = width;
}

template <Dimension D> void IRenderContext<D>::RemovePointLight(PointLight<D> *light)
{
    for (u32 i = 0; i < m_PointLights.GetSize(); ++i)
        if (m_PointLights[i] == light)
        {
            TKit::TierAllocator *tier = TKit::GetTier();
            tier->Destroy(light);
            m_PointLights.RemoveUnordered(m_PointLights.begin() + i);
            m_NeedToUpdateLights |= LightFlag_Point;
            return;
        }
    TKIT_FATAL("[ONYX][CONTEXT] Point light '{}' not found", static_cast<void *>(light));
}

void RenderContext<D3>::RemoveDirectionalLight(DirectionalLight *light)
{
    for (u32 i = 0; i < m_DirectionalLights.GetSize(); ++i)
        if (m_DirectionalLights[i] == light)
        {
            TKit::TierAllocator *tier = TKit::GetTier();
            tier->Destroy(light);
            m_DirectionalLights.RemoveUnordered(m_DirectionalLights.begin() + i);
            m_NeedToUpdateLights |= LightFlag_Directional;
            return;
        }
    TKIT_FATAL("[ONYX][CONTEXT] Directional light '{}' not found", static_cast<void *>(light));
}

template <Dimension D> void IRenderContext<D>::RemoveAllPointLights()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (PointLight<D> *light : m_PointLights)
        tier->Destroy(light);

    m_NeedToUpdateLights |= LightFlag_Point;
    m_PointLights.Clear();
}

void RenderContext<D3>::RemoveAllDirectionalLights()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (DirectionalLight *light : m_DirectionalLights)
        tier->Destroy(light);

    m_NeedToUpdateLights |= LightFlag_Directional;
    m_DirectionalLights.Clear();
}

template class Detail::IRenderContext<D2>;
template class Detail::IRenderContext<D3>;

} // namespace Onyx
