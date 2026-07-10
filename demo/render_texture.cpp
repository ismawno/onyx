#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"

using namespace TKit::Alias;

using Onyx::D2;
int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});
    Onyx::Overlay *ui = win->CreateOverlay();

    f32v2 rdims1 = {800, 600};
    f32v2 rdims2 = {800, 600};

    Onyx::RenderTexture *rt1 = Onyx::CreateRenderTexture(rdims1);
    Onyx::RenderTexture *rt2 = Onyx::CreateRenderTexture(rdims2);

    Onyx::Camera<D2> cam{};

    Onyx::RenderView<D2> *wview = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);
    Onyx::RenderView<D2> *rview1 = rt1->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    Onyx::RenderView<D2> *rview2 =
        rt2->CreateRenderView<D2>(&ui->GetCamera(), Onyx::RenderViewFlag_NormalizedCoordinates);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(wview);
    ctx->AddTarget(rview1);

    const Onyx::NativeWindow &nw = ui->GetMainNativeWindow();
    nw.Context->AddTarget(rview2);

    ctx->Flush();

    ctx->FillColor(Onyx::Color_Green);
    ctx->Quad();

    ctx->TranslateX(-1.5f);
    ctx->FillColor(Onyx::Color_Red);
    ctx->Triangle();

    ctx->TranslateX(3.f);
    ctx->FillColor(Onyx::Color_Blue);
    ctx->Circle();

    win->BringToTop(nw.View);
    while (Onyx::Running())
    {
        if (ui->BeginWindow("Main viewport", Onyx::OverlayWindowFlag_AutoResize))
        {
            if (ui->HorizontalSlider("Image dimensions", &rdims1, 50.f, 1600.f, "{:.0f}"))
                rt1->Resize(rdims1);

            ui->Image(*rt1, 0.5f * rdims1);
            ui->EndWindow();
        }

        if (ui->BeginWindow("Screen sharing", Onyx::OverlayWindowFlag_AutoResize))
        {
            if (ui->HorizontalSlider("Image dimensions", &rdims2, 50.f, 1600.f, "{:.0f}"))
                rt2->Resize(rdims2);

            ui->Image(*rt2, 0.5f * rdims2);
            ui->EndWindow();
        }

        ui->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
    Onyx::Terminate();
}
