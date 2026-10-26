#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"
#include "onyx/sanitizer_options.hpp"

using Onyx::D2;
using Onyx::D3;
using namespace TKit::Alias;

static bool runDemo(const bool floating)
{
    Onyx::Overlay *ui;
    if (floating)
        ui = Onyx::CreateFloatingOverlay();
    else
    {
        Onyx::Window *win = Onyx::OpenWindow();
        ui = win->CreateOverlay();
    }

    // ui->Flags |= Onyx::OverlayFlag_AutoSerialize | Onyx::OverlayFlag_Docking |
    //              Onyx::OverlayFlag_WindowPromotionInitialFrameCooldown;
    // ui->Deserialize(".");

    // a simple example using the docking api. docking must be enabled when creating the ui!!
    // const Onyx::OverlayDockNode *root =
    //     Onyx::DockSplit(Onyx::LayoutAxis_Vertical, 0.5f,
    //                     Onyx::DockSplit(Onyx::LayoutAxis_Horizontal, 0.5f, Onyx::DockTabBar("Overlay demo"),
    //                                     Onyx::DockTabBar("Window settings")),
    //                     Onyx::DockTabBar({"Style editor", "Renderer statistics"}));
    //
    // ui->SubmitDockTree("Dock tree", root);

    bool restart = false;
    const Graph::PlatformBackendInfo binfo = Graph::GetPlatformBackendInfo();
    while (Onyx::Running())
    {
        ui->ShowDemo();
        if (ui->BeginWindow("Overlay demo"))
        {
            ui->PushPanel(Onyx::LayoutDirection_LeftToRight);
            if (ui->Button("Quit"))
                Onyx::Quit(Onyx::QuitFlag_DestroyWindows | Onyx::QuitFlag_DestroyFloatingOverlays);
            if (binfo.Platform != Graph::Platform_Wayland &&
                ui->Button(floating ? "Restart using a main window" : "Restart using floating mode"))
            {
                restart = true;
                Onyx::Quit(Onyx::QuitFlag_DestroyWindows | Onyx::QuitFlag_DestroyFloatingOverlays);
            }
            ui->PopPanel();
            ui->EndWindow();
        }

        ui->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
    return restart;
}

int main()
{
    Onyx::Specs specs{};
    specs.Specs2 = Onyx::CreateEmptySpecs<D2>();
    specs.Specs2.MaxInstances = 16384;
    specs.Specs2.MaxDrawCommands = 2048;
    specs.Specs2.MaxVerticesPerResourcePool = 1024;
    specs.Specs2.MaxIndicesPerResourcePool = 2048;
    specs.Specs2.MaxDynamicVertices = 8192;
    specs.Specs2.MaxDynamicIndices = 32768;
    specs.Specs2.MaxDynamicMeshes = 96; // registered by the overlay
    specs.Specs2.MaxBounds = 4;
    specs.Specs3 = Onyx::CreateEmptySpecs<D3>();
    Onyx::Initialize(specs);
    Onyx::Resources_CreateDefault<D2>();

    bool floating = false;
    for (;;)
    {
        if (!runDemo(floating))
            break;
        floating = !floating;
    }

    Onyx::Terminate();
}
