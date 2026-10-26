#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/sanitizer_options.hpp"

using Onyx::D2;
using Onyx::D3;

int main()
{
    Onyx::Specs specs{};
    specs.Specs2 = Onyx::CreateEmptySpecs<D2>();
    specs.Specs2.MaxInstances = 64;
    specs.Specs2.MaxDrawCommands = 64;
    specs.Specs2.MaxVerticesPerResourcePool = 1024;
    specs.Specs2.MaxIndicesPerResourcePool = 2048;
    specs.Specs2.MaxBounds = 4;
    specs.Specs3 = Onyx::CreateEmptySpecs<D3>();
    Onyx::Initialize(specs);
    Onyx::Resources_CreateDefault<D2>();

    Onyx::Window *win = Onyx::OpenWindow();

    Onyx::Camera<D2> cam{};
    cam.OrthoParameters.Size = 15.f;

    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(view);

    while (Onyx::Running())
    {
        ctx->Flush();
        ctx->RenderFlags(Onyx::RenderModeFlag_Flat);

        ctx->Align(Onyx::Alignment_Center);
        ctx->Text("Hello world!");
        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
