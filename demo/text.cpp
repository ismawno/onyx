#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/sanitizer_options.hpp"

using Onyx::D2;

int main()
{
    Onyx::Initialize();
    Onyx::Resources_CreateDefault();

    Onyx::Window *win = Onyx::OpenWindow();

    Onyx::Camera<D2> cam{};
    cam.OrthoParameters.Size = 15.f;

    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam, {.Flags = Onyx::RenderViewFlag_NormalizedCoordinates});

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
