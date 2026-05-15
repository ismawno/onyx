#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/font.hpp"

using Onyx::D2;

int main()
{
    Onyx::Initialize();

    const Onyx::Resource sampler = Onyx::Resources::CreateSampler();
    const Onyx::FontData fdata = ONYX_CHECK_RESULT(Onyx::LoadDefaultFont());
    const Onyx::ResourcePool fpool = Onyx::Resources::CreateFontPool();
    const Onyx::Resource font = Onyx::Resources::RegisterFont(fpool, fdata);
    Onyx::Resources::Sync(Onyx::SyncFlag_Fonts);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});

    Onyx::Camera<D2> cam{};
    cam.OrthoParameters.Size = 15.f;

    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    ctx->AddTarget(view);

    while (Onyx::Running())
    {
        win->ControlCamera(Onyx::GetDeltaTime(win), &cam);
        ctx->Flush();
        ctx->RenderFlags(Onyx::RenderModeFlag_Flat);

        ctx->FontSampler(sampler);
        ctx->Font(font);
        ctx->Align(Onyx::Alignment_Center);
        ctx->Text("Hello world!");

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
