#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/ui.hpp"

using Onyx::D2;
using namespace TKit::Alias;

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
        ui.Button("Wow!");
        ui.Button("Yes!");
        ui.Button("No!");
        ui.Button("A!");
        static bool sliders = false;
        ui.CheckBox("Enable sliders", &sliders);

        if (sliders)
        {
            static f32 fval = 4;
            ui.Slider("My slider float", &fval, 0.f, 10.f);

            static u32 uval = 7;
            ui.Slider("My slider uint", &uval, 3, 28);
        }

        ui.EndWindow();

        ui.BeginWindow("Another");
        ui.EndWindow();

        Onyx::Transfer();
        Onyx::Render();
    }
}
