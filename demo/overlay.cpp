#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"

using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow();
    Onyx::Overlay *ui = win->CreateOverlay();
    while (Onyx::Running())
    {
        ui->ShowDemo();
        if (ui->BeginWindow("Overlay demo"))
        {
            if (ui->Button("Quit"))
                Onyx::Quit();
            ui->EndWindow();
        }

        ui->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
    Onyx::Terminate();
}
