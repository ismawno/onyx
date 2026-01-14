#include "onyx/core/pch.hpp"
#include "onyx/rendering/context.hpp"

#include "tkit/math/math.hpp"

namespace Onyx
{
using namespace Detail;

template <Dimension D> IRenderContext<D>::IRenderContext()
{
    m_Current = &m_StateStack.Append();
}

template <Dimension D> void IRenderContext<D>::Flush()
{
    TKIT_ASSERT(m_StateStack.GetSize() == 1,
                "[ONYX] Mismatched Push() call found. For every Push(), there must be a Pop()");

    m_StateStack[0] = RenderState<D>{};
    m_Current = &m_StateStack.GetFront();
    for (u32 i = 0; i < Primitive_Count; ++i)
        for (u32 j = 0; j < StencilPass_Count; ++j)
            m_InstanceData[i][j].Instances = 0;
    ++m_Generation;
}

template <Dimension D> void IRenderContext<D>::Transform(const f32m<D> &p_Transform)
{
    m_Current->Transform = p_Transform * m_Current->Transform;
}
template <Dimension D>
void IRenderContext<D>::Transform(const f32v<D> &p_Translation, const f32v<D> &p_Scale, const rot<D> &p_Rotation)
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(p_Translation, p_Scale, p_Rotation));
}
template <Dimension D>
void IRenderContext<D>::Transform(const f32v<D> &p_Translation, const f32 p_Scale, const rot<D> &p_Rotation)
{
    this->Transform(Onyx::Transform<D>::ComputeTransform(p_Translation, f32v<D>{p_Scale}, p_Rotation));
}
void RenderContext<D3>::Transform(const f32v3 &p_Translation, const f32v3 &p_Scale, const f32v3 &p_Rotation)
{
    this->Transform(Onyx::Transform<D3>::ComputeTransform(p_Translation, p_Scale, f32q{p_Rotation}));
}
void RenderContext<D3>::Transform(const f32v3 &p_Translation, const f32 p_Scale, const f32v3 &p_Rotation)
{
    this->Transform(Onyx::Transform<D3>::ComputeTransform(p_Translation, f32v3{p_Scale}, f32q{p_Rotation}));
}

template <Dimension D> void IRenderContext<D>::Translate(const f32v<D> &p_Translation)
{
    Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, p_Translation);
}
template <Dimension D> void IRenderContext<D>::SetTranslation(const f32v<D> &p_Translation)
{
    m_Current->Transform[D][0] = p_Translation[0];
    m_Current->Transform[D][1] = p_Translation[1];
    if constexpr (D == D3)
        m_Current->Transform[D][2] = p_Translation[2];
}

template <Dimension D> void IRenderContext<D>::Scale(const f32v<D> &p_Scale)
{
    Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, p_Scale);
}
template <Dimension D> void IRenderContext<D>::Scale(const f32 p_Scale)
{
    Scale(f32v<D>{p_Scale});
}

template <Dimension D> void IRenderContext<D>::TranslateX(const f32 p_X)
{
    Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::TranslateY(const f32 p_Y)
{
    Onyx::Transform<D>::TranslateExtrinsic(m_Current->Transform, 1, p_Y);
}
void RenderContext<D3>::TranslateZ(const f32 p_Z)
{
    Onyx::Transform<D3>::TranslateExtrinsic(m_Current->Transform, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::SetTranslationX(const f32 p_X)
{
    m_Current->Transform[D][0] = p_X;
}
template <Dimension D> void IRenderContext<D>::SetTranslationY(const f32 p_Y)
{
    m_Current->Transform[D][1] = p_Y;
}
void RenderContext<D3>::SetTranslationZ(const f32 p_Z)
{
    m_Current->Transform[D3][2] = p_Z;
}
template <Dimension D> void IRenderContext<D>::ScaleX(const f32 p_X)
{
    Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::ScaleY(const f32 p_Y)
{
    Onyx::Transform<D>::ScaleExtrinsic(m_Current->Transform, 1, p_Y);
}
void RenderContext<D3>::ScaleZ(const f32 p_Z)
{
    Onyx::Transform<D3>::ScaleExtrinsic(m_Current->Transform, 2, p_Z);
}

void RenderContext<D2>::Rotate(const f32 p_Angle)
{
    Onyx::Transform<D2>::RotateExtrinsic(m_Current->Transform, p_Angle);
}

void RenderContext<D3>::Rotate(const f32q &p_Quaternion)
{
    Onyx::Transform<D3>::RotateExtrinsic(m_Current->Transform, p_Quaternion);
}
void RenderContext<D3>::Rotate(const f32 p_Angle, const f32v3 &p_Axis)
{
    Rotate(f32q::FromAngleAxis(p_Angle, p_Axis));
}
void RenderContext<D3>::Rotate(const f32v3 &p_Angles)
{
    Rotate(f32q(p_Angles));
}

// This could be optimized a bit
void RenderContext<D3>::RotateX(const f32 p_Angle)
{
    Rotate(f32v3{p_Angle, 0.f, 0.f});
}
void RenderContext<D3>::RotateY(const f32 p_Angle)
{
    Rotate(f32v3{0.f, p_Angle, 0.f});
}
void RenderContext<D3>::RotateZ(const f32 p_Angle)
{
    Rotate(f32v3{0.f, 0.f, p_Angle});
}

template <Dimension D, typename F> static void resolveStencilPassWithState(const RenderState<D> *p_State, F &&p_Draw)
{
    if (p_State->Flags & RenderStateFlag_Fill)
    {
        if (p_State->Flags & RenderStateFlag_Outline)
        {
            std::forward<F>(p_Draw)(StencilPass_DoStencilWriteDoFill);
            std::forward<F>(p_Draw)(StencilPass_DoStencilTestNoFill);
        }
        else
            std::forward<F>(p_Draw)(StencilPass_NoStencilWriteDoFill);
    }
    else if (p_State->Flags & RenderStateFlag_Outline)
    {
        std::forward<F>(p_Draw)(StencilPass_DoStencilWriteNoFill);
        std::forward<F>(p_Draw)(StencilPass_DoStencilTestNoFill);
    }
}

template <Dimension D> void IRenderContext<D>::updateState()
{
    m_Current = &m_StateStack.GetBack();
}

template <Dimension D> void IRenderContext<D>::StaticMesh(const Mesh p_Mesh)
{
    const auto draw = [&, p_Mesh](const StencilPass p_Pass) {
        addStaticMeshData(p_Mesh, m_Current->Transform, p_Pass);
    };
    resolveStencilPassWithState(m_Current, draw);
}
template <Dimension D> void IRenderContext<D>::StaticMesh(const Mesh p_Mesh, const f32m<D> &p_Transform)
{
    const auto draw = [&, p_Mesh](const StencilPass p_Pass) {
        addStaticMeshData(p_Mesh, p_Transform * m_Current->Transform, p_Pass);
    };
    resolveStencilPassWithState(m_Current, draw);
}

template <Dimension D> void IRenderContext<D>::Circle(const CircleOptions &p_Options)
{
    const auto draw = [&](const StencilPass p_Pass) { addCircleData(m_Current->Transform, p_Options, p_Pass); };
    resolveStencilPassWithState(m_Current, draw);
}
template <Dimension D> void IRenderContext<D>::Circle(const f32m<D> &p_Transform, const CircleOptions &p_Options)
{
    const auto draw = [&](const StencilPass p_Pass) {
        addCircleData(p_Transform * m_Current->Transform, p_Options, p_Pass);
    };
    resolveStencilPassWithState(m_Current, draw);
}

template <Dimension D>
static InstanceData<D> createInstanceData(const RenderState<D> *p_State, const f32m<D> &p_Transform,
                                          const StencilPass p_Pass)
{
    if constexpr (D == D2)
    {
        InstanceData<D2> instanceData{};
        instanceData.Basis1 = f32v2{p_Transform[0]};
        instanceData.Basis2 = f32v2{p_Transform[1]};
        instanceData.Basis3 = f32v2{p_Transform[2]};
        if (p_Pass == StencilPass_NoStencilWriteDoFill || p_Pass == StencilPass_DoStencilWriteDoFill)
        {
            instanceData.BaseColor = p_State->FillColor.Pack();
            instanceData.TexIndex = TKIT_U32_MAX;
        }
        else
        {
            instanceData.BaseColor = p_State->OutlineColor.Pack();
            instanceData.OutlineWidth = p_State->OutlineWidth;
        }
        return instanceData;
    }
    else
    {
        InstanceData<D3> instanceData{};
        instanceData.Basis1 = f32v4{p_Transform[0][0], p_Transform[1][0], p_Transform[2][0], p_Transform[3][0]};
        instanceData.Basis2 = f32v4{p_Transform[0][1], p_Transform[1][1], p_Transform[2][1], p_Transform[3][1]};
        instanceData.Basis3 = f32v4{p_Transform[0][2], p_Transform[1][2], p_Transform[2][2], p_Transform[3][2]};
        if (p_Pass == StencilPass_NoStencilWriteDoFill || p_Pass == StencilPass_DoStencilWriteDoFill)
        {
            instanceData.BaseColor = p_State->FillColor.Pack();
            instanceData.MatIndex = TKIT_U32_MAX;
        }
        else
        {
            instanceData.BaseColor = p_State->OutlineColor.Pack();
            instanceData.OutlineWidth = p_State->OutlineWidth;
        }
        return instanceData;
    }
}

template <Dimension D>
static CircleInstanceData<D> createCircleInstanceData(const RenderState<D> *p_State, const f32m<D> &p_Transform,
                                                      const CircleOptions &p_Options, const StencilPass p_Pass)
{
    CircleInstanceData<D> instanceData;
    instanceData.BaseData = createInstanceData(p_State, p_Transform, p_Pass);
    instanceData.LowerCos = Math::Cosine(p_Options.LowerAngle);
    instanceData.LowerSin = Math::Sine(p_Options.LowerAngle);
    instanceData.UpperCos = Math::Cosine(p_Options.UpperAngle);
    instanceData.UpperSin = Math::Sine(p_Options.UpperAngle);

    instanceData.AngleOverflow = Math::Absolute(p_Options.UpperAngle - p_Options.LowerAngle) > Math::Pi<f32>() ? 1 : 0;
    instanceData.Hollowness = p_Options.Hollowness;
    instanceData.InnerFade = p_Options.InnerFade;
    instanceData.OuterFade = p_Options.OuterFade;

    return instanceData;
}

static void resizeBuffer(VKit::HostBuffer &p_Buffer, const u32 p_Instances)
{
    if (p_Instances > p_Buffer.GetInstanceCount())
    {
        const u32 ninst = static_cast<u32>(1.5f * static_cast<f32>(p_Instances));
        p_Buffer.Resize(ninst);
    }
}

template <Dimension D>
void IRenderContext<D>::addStaticMeshData(const Mesh p_Mesh, const f32m<D> &p_Transform, const StencilPass p_Pass)
{
    TKIT_ASSERT(p_Mesh < BatchRangeSize_StaticMesh,
                "[ONYX] Instance overflow. The amount of static mesh types exceeds maximum of {}. Consider increasing "
                "ONYX_MAX_BATCHES to a larger number",
                u32(BatchRangeSize_StaticMesh));

    const InstanceData<D> idata = createInstanceData(m_Current, p_Transform, p_Pass);
    InstanceBuffer &buffer = m_InstanceData[p_Pass][p_Mesh + BatchRangeStart_StaticMesh];

    const u32 index = buffer.Instances++;
    resizeBuffer(buffer.Data, buffer.Instances);
    buffer.Data.WriteAt(index, &idata);
}

template <Dimension D>
void IRenderContext<D>::addCircleData(const f32m<D> &p_Transform, const CircleOptions &p_Options,
                                      const StencilPass p_Pass)
{
    const CircleInstanceData<D> idata = createCircleInstanceData(m_Current, p_Transform, p_Options, p_Pass);
    InstanceBuffer &buffer = m_InstanceData[p_Pass][BatchRangeStart_Circle];
    const u32 index = buffer.Instances++;
    resizeBuffer(buffer.Data, buffer.Instances);
    buffer.Data.WriteAt(index, &idata);
}

template <Dimension D> static rot<D> computeLineRotation(const f32v<D> &p_Start, const f32v<D> &p_End)
{
    const f32v<D> delta = p_End - p_Start;
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
void IRenderContext<D>::Line(const Mesh p_Mesh, const f32v<D> &p_Start, const f32v<D> &p_End, const f32 p_Thickness)
{
    const f32v<D> delta = p_End - p_Start;

    f32m<D> transform = m_Current->Transform;
    Onyx::Transform<D>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D>::RotateIntrinsic(transform, computeLineRotation<D>(p_Start, p_End));
    if constexpr (D == D2)
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v2{Math::Norm(delta), p_Thickness});
    else
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v3{Math::Norm(delta), p_Thickness, p_Thickness});
    const auto draw = [&, p_Mesh](const StencilPass p_Pass) { addStaticMeshData(p_Mesh, transform, p_Pass); };
    resolveStencilPassWithState(m_Current, draw);
}
template <Dimension D> void IRenderContext<D>::Axes(const Mesh p_Mesh, const AxesOptions &p_Options)
{
    if constexpr (D == D2)
    {
        Color &color = m_Current->FillColor;
        const Color oldColor = color; // A cheap filthy push

        const f32v2 xLeft = f32v2{-p_Options.Size, 0.f};
        const f32v2 xRight = f32v2{p_Options.Size, 0.f};

        const f32v2 yDown = f32v2{0.f, -p_Options.Size};
        const f32v2 yUp = f32v2{0.f, p_Options.Size};

        color = Color{245u, 64u, 90u};
        Line(p_Mesh, xLeft, xRight, p_Options.Thickness);
        color = Color{65u, 135u, 245u};
        Line(p_Mesh, yDown, yUp, p_Options.Thickness);

        color = oldColor; // A cheap filthy pop
    }
    else
    {
        Color &color = m_Current->FillColor;
        const Color oldColor = color; // A cheap filthy push

        const f32v3 xLeft = f32v3{-p_Options.Size, 0.f, 0.f};
        const f32v3 xRight = f32v3{p_Options.Size, 0.f, 0.f};

        const f32v3 yDown = f32v3{0.f, -p_Options.Size, 0.f};
        const f32v3 yUp = f32v3{0.f, p_Options.Size, 0.f};

        const f32v3 zBack = f32v3{0.f, 0.f, -p_Options.Size};
        const f32v3 zFront = f32v3{0.f, 0.f, p_Options.Size};

        color = Color{245u, 64u, 90u};
        Line(p_Mesh, xLeft, xRight, p_Options.Thickness);
        color = Color{180u, 245u, 65u};
        Line(p_Mesh, yDown, yUp, p_Options.Thickness);
        color = Color{65u, 135u, 245u};
        Line(p_Mesh, zBack, zFront, p_Options.Thickness);
        color = oldColor; // A cheap filthy pop
    }
}

void RenderContext<D3>::LightColor(const Color &p_Color)
{
    m_Current->LightColor = p_Color;
}
// void RenderContext<D3>::AmbientColor(const Color &p_Color)
// {
//     m_Renderer.AmbientColor = p_Color;
// }
// void RenderContext<D3>::AmbientIntensity(const f32 p_Intensity)
// {
//     m_Renderer.AmbientColor.RGBA[3] = p_Intensity;
// }
//
// void RenderContext<D3>::DirectionalLight(Onyx::DirectionalLight p_Light)
// {
//     p_Light.Direction = Math::Normalize(m_Current->Transform * f32v4{p_Light.Direction, 0.f});
//     m_Renderer.AddDirectionalLight(p_Light);
// }
// void RenderContext<D3>::DirectionalLight(const f32v3 &p_Direction, const f32 p_Intensity)
// {
//     Onyx::DirectionalLight light;
//     light.Direction = p_Direction;
//     light.Intensity = p_Intensity;
//     light.Color = m_Current->LightColor.Pack();
//     DirectionalLight(light);
// }

// void RenderContext<D3>::PointLight(Onyx::PointLight p_Light)
// {
//     p_Light.Position = m_Current->Transform * f32v4{p_Light.Position, 1.f};
//     m_Renderer.AddPointLight(p_Light);
// }
// void RenderContext<D3>::PointLight(const f32v3 &p_Position, const f32 p_Radius, const f32 p_Intensity)
// {
//     Onyx::PointLight light;
//     light.Position = p_Position;
//     light.Radius = p_Radius;
//     light.Intensity = p_Intensity;
//     light.Color = m_Current->LightColor.Pack();
//     PointLight(light);
// }
// void RenderContext<D3>::PointLight(const f32 p_Radius, const f32 p_Intensity)
// {
//     PointLight(f32v3{0.f}, p_Radius, p_Intensity);
// }

template <Dimension D> void IRenderContext<D>::AddFlags(const RenderStateFlags p_Flags)
{
    m_Current->Flags |= p_Flags;
}
template <Dimension D> void IRenderContext<D>::RemoveFlags(const RenderStateFlags p_Flags)
{
    m_Current->Flags &= ~p_Flags;
}

template <Dimension D> void IRenderContext<D>::Fill(const bool p_Enabled)
{
    if (p_Enabled)
        AddFlags(RenderStateFlag_Fill);
    else
        RemoveFlags(RenderStateFlag_Fill);
}
template <Dimension D> void IRenderContext<D>::Push(const RenderState<D> &p_State)
{
    m_StateStack.Append(p_State);
    updateState();
}
template <Dimension D> void IRenderContext<D>::Push()
{
    Push(*m_Current);
}
template <Dimension D> void IRenderContext<D>::Pop()
{
    TKIT_ASSERT(m_StateStack.GetSize() > 1, "[ONYX] For every Push(), there must be a Pop()");
    m_StateStack.Pop();
    updateState();
}

template <Dimension D> void IRenderContext<D>::Alpha(const f32 p_Alpha)
{
    m_Current->FillColor.RGBA[3] = p_Alpha;
}
template <Dimension D> void IRenderContext<D>::Alpha(const u8 p_Alpha)
{
    m_Current->FillColor.RGBA[3] = static_cast<f32>(p_Alpha) / 255.f;
}
template <Dimension D> void IRenderContext<D>::Alpha(const u32 p_Alpha)
{
    m_Current->FillColor.RGBA[3] = static_cast<f32>(p_Alpha) / 255.f;
}

template <Dimension D> void IRenderContext<D>::Fill(const Color &p_Color)
{
    Fill(true);
    m_Current->FillColor = p_Color;
}
template <Dimension D> void IRenderContext<D>::Outline(const bool p_Enabled)
{
    if (p_Enabled)
        AddFlags(RenderStateFlag_Outline);
    else
        RemoveFlags(RenderStateFlag_Outline);
}
template <Dimension D> void IRenderContext<D>::Outline(const Color &p_Color)
{
    Outline(true);
    m_Current->OutlineColor = p_Color;
}
template <Dimension D> void IRenderContext<D>::OutlineWidth(const f32 p_Width)
{
    Outline(true);
    m_Current->OutlineWidth = p_Width;
}

template class Detail::IRenderContext<D2>;
template class Detail::IRenderContext<D3>;

} // namespace Onyx
