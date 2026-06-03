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
        static bool anotherWindow = false;
        ui.CheckBox("Extra window", &anotherWindow);
        ui.CheckBox("Enable sliders", &sliders);

        if (sliders)
        {
            ui.Text("Here are some sliders!");
            static f32 fval = 4;
            ui.Slider("My slider float", &fval, 0.f, 10.f);

            static u32 uval = 7;
            ui.Slider("My slider uint", &uval, 3, 28);

            static u32 uval2 = 7;
            ui.Drag("My drag uint", &uval2, 1);
        }

        ui.EndWindow();

        if (anotherWindow)
        {
            ui.BeginWindow("Im another window!");
            ui.Text("Happy to be here!");
            ui.EndWindow();
        }

        Onyx::Transfer();
        Onyx::Render();
    }
}
