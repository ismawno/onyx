#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/hook.hpp"
#include "onyx/onyx.hpp"
#include "onyx/sanitizer_options.hpp"

using Onyx::D2;
using Onyx::D3;
using namespace TKit::Alias;

// a fragment hook receives the color onyx computed for a pixel and returns the one to write
static constexpr const char *FragmentHookSource = R"(
export FragmentHookOutput FragmentHook(const FragmentHookInput input)
{
    const float time = input.Globals.LoadF32(0);
    const float stripes = 0.5 + 0.5 * sin(40.0 * input.TexCoord.x - 6.0 * time);

    FragmentHookOutput output;
    output.Color = float4(input.Color.rgb * stripes, input.Color.a);
    return output;
}
)";

// a vertex hook receives the world position onyx computed for a vertex and returns the one to project
static constexpr const char *VertexHookSource = R"(
export VertexHookOutput VertexHook(const VertexHookInput input)
{
    const float time = input.Globals.LoadF32(0);

    VertexHookOutput output;
    output.WorldPosition = input.WorldPosition;
    output.WorldPosition.y += 0.25 * sin(3.0 * time + 6.0 * input.LocalPosition.x);
    return output;
}
)";

int main()
{
    Onyx::Specs specs{};
    specs.Specs2 = Onyx::CreateEmptySpecs<D2>();
    specs.Specs2.MaxInstances = 16;
    specs.Specs2.MaxDrawCommands = 16;
    specs.Specs2.MaxBounds = 4;
    specs.Specs2.HookGlobalsSize = sizeof(f32);
    specs.Specs3 = Onyx::CreateEmptySpecs<D3>();
    Onyx::Initialize(specs);
    Onyx::Resources_CreateDefault<D2>();

    const Onyx::Resource fragment = Onyx::Hook_FromSource<D2>(Onyx::HookStage_Fragment, FragmentHookSource);
    const Onyx::Resource vertex = Onyx::Hook_FromSource<D2>(Onyx::HookStage_Vertex, VertexHookSource);

    // optional: hooks also compile on first use. compiling them up front reports errors here instead
    // ONYX_CHECK_RESULT(Onyx::Hook_Precompile<D2>(fragment));
    // ONYX_CHECK_RESULT(Onyx::Hook_Precompile<D2>(vertex));

    Onyx::Window *win = Onyx::OpenWindow();

    Onyx::Camera<D2> cam{};
    cam.OrthoParameters.Size = 5.f;
    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(view);

    f32 time = 0.f;
    while (Onyx::Running())
    {
        time += Onyx::GetDeltaTime(win).AsSeconds();
        // read at Transfer(), so it must stay alive until then
        Onyx::SetHookGlobalData<D2>(&time);

        ctx->Flush();

        // no hooks
        ctx->Push();
        ctx->TranslateX(-1.5f);
        ctx->FillColor(Onyx::Color_Coral);
        ctx->Quad();
        ctx->Pop();

        // a fragment hook. hooks are part of the context state, so Pop() takes them off again
        ctx->Push();
        ctx->FragmentHook(fragment);
        ctx->FillColor(Onyx::Color_Sky);
        ctx->Quad();
        ctx->Pop();

        // a vertex and a fragment hook together
        ctx->Push();
        ctx->TranslateX(1.5f);
        ctx->VertexHook(vertex);
        ctx->FragmentHook(fragment);
        ctx->FillColor(Onyx::Color_Mint);
        ctx->Quad();
        ctx->Pop();

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
