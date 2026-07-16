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

    Onyx::Window *win = Onyx::OpenWindow();
    Onyx::Overlay *ui = win->CreateOverlay({.Flags = Onyx::OverlayFlag_WindowPromotions});

    u32v2 rdims1 = {800, 600};
    u32v2 rdims2 = {800, 600};

    Onyx::RenderTexture *rt1 = Onyx::CreateRenderTexture(rdims1);
    Onyx::RenderTexture *rt2 = Onyx::CreateRenderTexture(rdims2);

    Onyx::Camera<D2> cam{};

    Onyx::RenderView<D2> *wview = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);
    Onyx::RenderView<D2> *rview1 = rt1->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    Onyx::RenderView<D2> *rview2 =
        rt2->CreateRenderView<D2>(&ui->GetCamera(), Onyx::RenderViewFlag_NormalizedCoordinates);

    // main context targets window view -> sample shapes will render to main window
    // main context targets render texture view -> sample shapes will render to render texture
    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(wview);
    ctx->AddTarget(rview1);

    // overlay's context targets its internal view (by default, not explicitly set here) and we also make it target the
    // second render texture's view
    // this means rt2 will only show the ui being drawn, NOT the shapes directly. for that, another view must be created
    // for rt2 with `cam` and make ctx target that one as well
    const Onyx::NativeWindow *nw = ui->GetMainNativeWindow();
    nw->Context->AddTarget(rview2);

    ctx->Flush();

    ctx->FillColor(Onyx::Color_Green);
    ctx->Quad();

    ctx->TranslateX(-1.5f);
    ctx->FillColor(Onyx::Color_Red);
    ctx->Triangle();

    ctx->TranslateX(3.f);
    ctx->FillColor(Onyx::Color_Blue);
    ctx->Circle();

    win->BringToTop(nw->View);
    const auto editDimensions = [&](Onyx::RenderTexture *rt, u32v2 &dims) {
        static bool useHor = false;
        if (!useHor && ui->IsCurrentWindowPromoted())
        {
            ui->BeginDisabled();
            ui->SetNextTextId("Help");
            ui->TextRaw("(?)");
            ui->EndDisabled();

            ui->SetItemTooltip(
                "When promoted, window resizes tend to be a bit unstable. Horizontal sliders depend on width, so "
                "modifying the width with a horizontal slider may cause the window to go crazy");

            ui->CheckBox("Use horizontal slider anyways to see what happens", &useHor);

            if (ui->VerticalSlider("Image width", &dims[0], 50.f, 1600.f))
                rt->Resize(dims);
            if (ui->HorizontalSlider("Image height", &dims[1], 50.f, 1600.f))
                rt->Resize(dims);
        }
        else
        {
            if (ui->IsCurrentWindowPromoted() && ui->Button("Go back"))
                useHor = false;

            if (ui->HorizontalSlider("Image dimensions", &dims, 50.f, 1600.f))
                rt->Resize(dims);
        }
    };
    while (Onyx::Running())
    {
        if (ui->BeginWindow("Main viewport", Onyx::OverlayWindowFlag_AutoResize))
        {
            editDimensions(rt1, rdims1);
            ui->Image(*rt1, 0.5f * f32v2{rdims1});
            ui->EndWindow();
        }

        if (ui->BeginWindow("Screen sharing", Onyx::OverlayWindowFlag_AutoResize))
        {
            editDimensions(rt2, rdims2);

            ui->Image(*rt2, 0.5f * f32v2{rdims2});
            ui->EndWindow();
        }

        ui->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
    Onyx::Terminate();
}
