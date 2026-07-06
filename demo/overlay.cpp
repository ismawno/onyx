#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"

using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});
    Onyx::Overlay *ui = win->CreateOverlay();
    while (Onyx::Running())
    {
        ui->ShowDemo();
        ui->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
    Onyx::Terminate();
}
