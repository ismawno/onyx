#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"
#include "onyx/sanitizer_options.hpp"

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

    // ui->Flags |= Onyx::OverlayFlag_AutoSerialize | Onyx::OverlayFlag_Docking | Onyx::OverlayFlag_WindowPromotions;
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
    while (Onyx::Running())
    {
        ui->ShowDemo();
        if (ui->BeginWindow("Overlay demo"))
        {
            ui->PushDirection(Onyx::LayoutDirection_LeftToRight);
            if (ui->Button("Quit"))
                Onyx::Quit(Onyx::QuitFlag_DestroyWindows | Onyx::QuitFlag_DestroyFloatingOverlays);
            if (ui->Button(floating ? "Restart using a main window" : "Restart using floating mode"))
            {
                restart = true;
                Onyx::Quit(Onyx::QuitFlag_DestroyWindows | Onyx::QuitFlag_DestroyFloatingOverlays);
            }
            ui->PopDirection();
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
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    bool floating = false;
    for (;;)
    {
        if (!runDemo(floating))
            break;
        floating = !floating;
    }

    Onyx::Terminate();
}
