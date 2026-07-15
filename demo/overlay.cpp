#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"

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
