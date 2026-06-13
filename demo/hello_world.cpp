#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"

using Onyx::D2;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});

    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(view);

    const Onyx::ImageData idata =
        ONYX_CHECK_RESULT(Onyx::LoadImageDataFromFile("/home/ismawno/screenshot.png", Onyx::ImageComponent_RGBA));
    const Onyx::Resource img = Onyx::Resources::CreateImage(idata);
    const Onyx::Resource tex = Onyx::Resources::CreateTexture(img);

    f32 time = 0.f;
    while (Onyx::Running())
    {
        time += Onyx::GetDeltaTime(win).AsSeconds();
        ctx->Flush();
        ctx->Rotate(time);
        ctx->Scale(4.f);
        ctx->Image(tex);

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
