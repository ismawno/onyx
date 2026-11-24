#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/hook.hpp"
#include "onyx/sanitizer_options.hpp"

using Onyx::D2;
using Onyx::D3;
using namespace TKit::Alias;

int main()
{
    Onyx::Specs specs{};
    specs.Specs2 = Onyx::CreateEmptySpecs<D2>();
    specs.Specs2.MaxInstances = 16;
    specs.Specs2.MaxDrawCommands = 16;
    specs.Specs2.MaxBounds = 4;
    specs.Specs3 = Onyx::CreateEmptySpecs<D3>();
    Onyx::Initialize(specs);
    Onyx::Resources_CreateDefault<D2>();

    Onyx::Window *win = Onyx::OpenWindow();

    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(view);

    const Onyx::Resource hook = Onyx::Hook_FromSource<D2>(R"(
                                                          )");
    ONYX_CHECK_RESULT(Onyx::Hook_Precompile<D2>(hook));

    f32 time = 0.f;
    while (Onyx::Running())
    {
        time += Onyx::GetDeltaTime(win).AsSeconds();
        ctx->Flush();
        ctx->FragmentHook(hook);
        ctx->Rotate(time);
        ctx->Triangle();

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
