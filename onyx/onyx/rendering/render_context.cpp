#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "tkit/multiprocessing/topology.hpp"

#include "tkit/math/math.hpp"

namespace Onyx
{
using namespace Detail;

template <Dimension D>
IRenderContext<D>::IRenderContext(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) : m_Renderer(p_RenderInfo)
{
    for (u32 i = 0; i < MaxThreads; ++i)
    {
        RenderState<D> &state = m_StateStack[i].States.Append();
        m_StateStack[i].Current = &state;
    }
}

static u32 getThreadIndex()
{
    thread_local u32 tindex = TKit::Topology::GetThreadIndex();
    return tindex;
}

template <Dimension D> void IRenderContext<D>::Flush(const u32 p_ThreadCount)
{
    TKIT_ASSERT(p_ThreadCount <= MaxThreads, "[ONYX] Thread count is greater than the maximum threads allowed");
    m_Renderer.Flush();
    for (u32 i = 0; i < p_ThreadCount; ++i)
    {
        Stack &stack = m_StateStack[i];
        TKIT_ASSERT(stack.States.GetSize() == 1,
                    "[ONYX] Mismatched Push() call found in thread {}. For every Push(), there must be a Pop()", i);
        stack.States[0] = RenderState<D>{};
    }
}

template <Dimension D> void IRenderContext<D>::Transform(const f32m<D> &p_Transform)
{
    RenderState<D> *state = getState();
    state->Transform = p_Transform * state->Transform;
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
    Onyx::Transform<D>::TranslateExtrinsic(getState()->Transform, p_Translation);
}
template <Dimension D> void IRenderContext<D>::SetTranslation(const f32v<D> &p_Translation)
{
    RenderState<D> *state = getState();
    state->Transform[D][0] = p_Translation[0];
    state->Transform[D][1] = p_Translation[1];
    if constexpr (D == D3)
        state->Transform[D][2] = p_Translation[2];
}

template <Dimension D> void IRenderContext<D>::Scale(const f32v<D> &p_Scale)
{
    Onyx::Transform<D>::ScaleExtrinsic(getState()->Transform, p_Scale);
}
template <Dimension D> void IRenderContext<D>::Scale(const f32 p_Scale)
{
    Scale(f32v<D>{p_Scale});
}

template <Dimension D> void IRenderContext<D>::TranslateX(const f32 p_X)
{
    Onyx::Transform<D>::TranslateExtrinsic(getState()->Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::TranslateY(const f32 p_Y)
{
    Onyx::Transform<D>::TranslateExtrinsic(getState()->Transform, 1, p_Y);
}
void RenderContext<D3>::TranslateZ(const f32 p_Z)
{
    Onyx::Transform<D3>::TranslateExtrinsic(getState()->Transform, 2, p_Z);
}

template <Dimension D> void IRenderContext<D>::SetTranslationX(const f32 p_X)
{
    getState()->Transform[D][0] = p_X;
}
template <Dimension D> void IRenderContext<D>::SetTranslationY(const f32 p_Y)
{
    getState()->Transform[D][1] = p_Y;
}
void RenderContext<D3>::SetTranslationZ(const f32 p_Z)
{
    getState()->Transform[D3][2] = p_Z;
}
template <Dimension D> void IRenderContext<D>::ScaleX(const f32 p_X)
{
    Onyx::Transform<D>::ScaleExtrinsic(getState()->Transform, 0, p_X);
}
template <Dimension D> void IRenderContext<D>::ScaleY(const f32 p_Y)
{
    Onyx::Transform<D>::ScaleExtrinsic(getState()->Transform, 1, p_Y);
}
void RenderContext<D3>::ScaleZ(const f32 p_Z)
{
    Onyx::Transform<D3>::ScaleExtrinsic(getState()->Transform, 2, p_Z);
}

void RenderContext<D2>::Rotate(const f32 p_Angle)
{
    Onyx::Transform<D2>::RotateExtrinsic(getState()->Transform, p_Angle);
}

void RenderContext<D3>::Rotate(const f32q &p_Quaternion)
{
    Onyx::Transform<D3>::RotateExtrinsic(getState()->Transform, p_Quaternion);
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

template <Dimension D, typename F> static void resolveDrawFlagsWithState(const RenderState<D> &p_State, F &&p_Draw)
{
    if (p_State.Flags & RenderStateFlag_Fill)
    {
        if (p_State.Flags & RenderStateFlag_Outline)
        {
            std::forward<F>(p_Draw)(DrawFlag_DoStencilWriteDoFill);
            std::forward<F>(p_Draw)(DrawFlag_DoStencilTestNoFill);
        }
        else
            std::forward<F>(p_Draw)(DrawFlag_NoStencilWriteDoFill);
    }
    else if (p_State.Flags & RenderStateFlag_Outline)
    {
        std::forward<F>(p_Draw)(DrawFlag_DoStencilWriteNoFill);
        std::forward<F>(p_Draw)(DrawFlag_DoStencilTestNoFill);
    }
}

template <Dimension D> void IRenderContext<D>::updateState()
{
    Stack &stack = m_StateStack[getThreadIndex()];
    stack.Current = &stack.States.GetBack();
}
template <Dimension D> RenderState<D> *IRenderContext<D>::getState()
{
    return m_StateStack[getThreadIndex()].Current;
}

template <Dimension D> void IRenderContext<D>::StaticMesh(const Assets::Mesh p_Mesh)
{
    const RenderState<D> &state = *getState();
    const auto draw = [&, p_Mesh](const DrawFlags p_Flags) {
        m_Renderer.DrawStaticMesh(state, state.Transform, p_Mesh, p_Flags);
    };
    resolveDrawFlagsWithState(state, draw);
}
template <Dimension D> void IRenderContext<D>::StaticMesh(const Assets::Mesh p_Mesh, const f32m<D> &p_Transform)
{
    const RenderState<D> &state = *getState();
    const auto draw = [&, p_Mesh](const DrawFlags p_Flags) {
        m_Renderer.DrawStaticMesh(state, state.Transform * p_Transform, p_Mesh, p_Flags);
    };
    resolveDrawFlagsWithState(state, draw);
}

template <Dimension D> void IRenderContext<D>::Circle(const CircleOptions &p_Options)
{
    const RenderState<D> &state = *getState();
    const auto draw = [&](const DrawFlags p_Flags) {
        m_Renderer.DrawCircle(state, state.Transform, p_Options, p_Flags);
    };
    resolveDrawFlagsWithState(state, draw);
}
template <Dimension D> void IRenderContext<D>::Circle(const f32m<D> &p_Transform, const CircleOptions &p_Options)
{
    const RenderState<D> &state = *getState();
    const auto draw = [&](const DrawFlags p_Flags) {
        m_Renderer.DrawCircle(state, state.Transform * p_Transform, p_Options, p_Flags);
    };
    resolveDrawFlagsWithState(state, draw);
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
void IRenderContext<D>::Line(const Assets::Mesh p_Mesh, const f32v<D> &p_Start, const f32v<D> &p_End,
                             const f32 p_Thickness)
{
    const f32v<D> delta = p_End - p_Start;
    const RenderState<D> &state = *getState();
    f32m<D> transform = state.Transform;
    Onyx::Transform<D>::TranslateIntrinsic(transform, 0.5f * (p_Start + p_End));
    Onyx::Transform<D>::RotateIntrinsic(transform, computeLineRotation<D>(p_Start, p_End));
    if constexpr (D == D2)
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v2{Math::Norm(delta), p_Thickness});
    else
        Onyx::Transform<D>::ScaleIntrinsic(transform, f32v3{Math::Norm(delta), p_Thickness, p_Thickness});
    const auto draw = [&, p_Mesh](const DrawFlags p_Flags) {
        m_Renderer.DrawStaticMesh(state, transform, p_Mesh, p_Flags);
    };
    resolveDrawFlagsWithState(state, draw);
}
template <Dimension D> void IRenderContext<D>::Axes(const Assets::Mesh p_Mesh, const AxesOptions &p_Options)
{
    if constexpr (D == D2)
    {
        Color &color = getState()->Material.Color;
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
        Color &color = getState()->Material.Color;
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
    getState()->LightColor = p_Color;
}
void RenderContext<D3>::AmbientColor(const Color &p_Color)
{
    m_Renderer.AmbientColor = p_Color;
}
void RenderContext<D3>::AmbientIntensity(const f32 p_Intensity)
{
    m_Renderer.AmbientColor.RGBA[3] = p_Intensity;
}

void RenderContext<D3>::DirectionalLight(Onyx::DirectionalLight p_Light)
{
    const RenderState<D3> *state = getState();
    p_Light.Direction = Math::Normalize(state->Transform * f32v4{p_Light.Direction, 0.f});
    m_Renderer.AddDirectionalLight(p_Light);
}
void RenderContext<D3>::DirectionalLight(const f32v3 &p_Direction, const f32 p_Intensity)
{
    Onyx::DirectionalLight light;
    light.Direction = p_Direction;
    light.Intensity = p_Intensity;
    light.Color = getState()->LightColor.Pack();
    DirectionalLight(light);
}

void RenderContext<D3>::PointLight(Onyx::PointLight p_Light)
{
    const RenderState<D3> *state = getState();
    p_Light.Position = state->Transform * f32v4{p_Light.Position, 1.f};
    m_Renderer.AddPointLight(p_Light);
}
void RenderContext<D3>::PointLight(const f32v3 &p_Position, const f32 p_Radius, const f32 p_Intensity)
{
    Onyx::PointLight light;
    light.Position = p_Position;
    light.Radius = p_Radius;
    light.Intensity = p_Intensity;
    light.Color = getState()->LightColor.Pack();
    PointLight(light);
}
void RenderContext<D3>::PointLight(const f32 p_Radius, const f32 p_Intensity)
{
    PointLight(f32v3{0.f}, p_Radius, p_Intensity);
}

void RenderContext<D3>::DiffuseContribution(const f32 p_Contribution)
{
    getState()->Material.DiffuseContribution = p_Contribution;
}
void RenderContext<D3>::SpecularContribution(const f32 p_Contribution)
{
    getState()->Material.SpecularContribution = p_Contribution;
}
void RenderContext<D3>::SpecularSharpness(const f32 p_Sharpness)
{
    getState()->Material.SpecularSharpness = p_Sharpness;
}
template <Dimension D> void IRenderContext<D>::AddFlags(const RenderStateFlags p_Flags)
{
    getState()->Flags |= p_Flags;
}
template <Dimension D> void IRenderContext<D>::RemoveFlags(const RenderStateFlags p_Flags)
{
    getState()->Flags &= ~p_Flags;
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
    Stack &stack = m_StateStack[getThreadIndex()];
    stack.States.Append(p_State);
    updateState();
}
template <Dimension D> void IRenderContext<D>::Push()
{
    Push(m_StateStack[getThreadIndex()].States.GetBack());
}
template <Dimension D> void IRenderContext<D>::Pop()
{
    TKIT_ASSERT(m_StateStack.GetSize() > 1, "[ONYX] For every Push(), there must be a Pop()");
    Stack &stack = m_StateStack[getThreadIndex()];
    stack.States.Pop();
    updateState();
}

template <Dimension D> void IRenderContext<D>::Alpha(const f32 p_Alpha)
{
    getState()->Material.Color.RGBA[3] = p_Alpha;
}
template <Dimension D> void IRenderContext<D>::Alpha(const u8 p_Alpha)
{
    getState()->Material.Color.RGBA[3] = static_cast<f32>(p_Alpha) / 255.f;
}
template <Dimension D> void IRenderContext<D>::Alpha(const u32 p_Alpha)
{
    getState()->Material.Color.RGBA[3] = static_cast<f32>(p_Alpha) / 255.f;
}

template <Dimension D> void IRenderContext<D>::Fill(const Color &p_Color)
{
    Fill(true);
    getState()->Material.Color = p_Color;
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
    getState()->OutlineColor = p_Color;
}
template <Dimension D> void IRenderContext<D>::OutlineWidth(const f32 p_Width)
{
    Outline(true);
    getState()->OutlineWidth = p_Width;
}

template <Dimension D> void IRenderContext<D>::Material(const MaterialData<D> &p_Material)
{
    getState()->Material = p_Material;
}

template <Dimension D> void IRenderContext<D>::ShareStateStack(const u32 p_ThreadCount)
{
    TKIT_ASSERT(p_ThreadCount <= MaxThreads, "[ONYX] Thread count is greater than the maximum threads allowed");
    const u32 tindex = getThreadIndex();
    for (u32 i = 0; i < p_ThreadCount; ++i)
        if (i != tindex)
            m_StateStack[i] = m_StateStack[tindex];
}

template <Dimension D> void IRenderContext<D>::ShareCurrentState(const u32 p_ThreadCount)
{
    TKIT_ASSERT(p_ThreadCount <= MaxThreads, "[ONYX] Thread count is greater than the maximum threads allowed");
    const u32 tindex = getThreadIndex();
    for (u32 i = 0; i < p_ThreadCount; ++i)
        if (i != tindex)
            *m_StateStack[i].Current = *m_StateStack[tindex].Current;
}

template <Dimension D> void IRenderContext<D>::ShareState(const RenderState<D> &p_State, const u32 p_ThreadCount)
{
    TKIT_ASSERT(p_ThreadCount <= MaxThreads, "[ONYX] Thread count is greater than the maximum threads allowed");
    for (u32 i = 0; i < p_ThreadCount; ++i)
        *m_StateStack[i].Current = p_State;
}

template <Dimension D> const RenderState<D> &IRenderContext<D>::GetCurrentState() const
{
    return *m_StateStack[getThreadIndex()].Current;
}
template <Dimension D> RenderState<D> &IRenderContext<D>::GetCurrentState()
{
    return *m_StateStack[getThreadIndex()].Current;
}
template <Dimension D> void IRenderContext<D>::SetCurrentState(const RenderState<D> &p_State)
{
    *m_StateStack[getThreadIndex()].Current = p_State;
}

template class ONYX_API Detail::IRenderContext<D2>;
template class ONYX_API Detail::IRenderContext<D3>;

} // namespace Onyx
