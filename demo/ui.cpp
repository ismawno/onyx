#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/ui.hpp"

using Onyx::D2;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});
    Onyx::UserInterface ui{win};

    while (Onyx::Running())
    {
        ui.Draw();

        ui.BeginWindow("Test");
        ui.Button("Hey!");
        ui.Button("Wow!");
        ui.Button("Yes!");
        ui.Button("No!");
        ui.EndWindow();

        Onyx::Transfer();
        Onyx::Render();
    }
}
