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
        static Onyx::OverlayWindowFlags flags = 0;
        if (ui.BeginWindow("Window settings", flags))
        {
            ui.CheckBoxFlags("No resize", &flags, Onyx::OverlayWindowFlag_NoResize);
            ui.CheckBoxFlags("No move", &flags, Onyx::OverlayWindowFlag_NoMove);
            ui.CheckBoxFlags("No collapse", &flags, Onyx::OverlayWindowFlag_NoCollapse);
            ui.CheckBoxFlags("No scroll bar", &flags, Onyx::OverlayWindowFlag_NoScrollBar);
            ui.CheckBoxFlags("No background", &flags, Onyx::OverlayWindowFlag_NoBackground);
            ui.CheckBoxFlags("No header bar", &flags, Onyx::OverlayWindowFlag_NoHeaderBar);
            ui.CheckBoxFlags("No bring to focus", &flags, Onyx::OverlayWindowFlag_NoBringToFocus);
            ui.CheckBoxFlags("Always auto resize", &flags, Onyx::OverlayWindowFlag_AlwaysAutoResize);
        }
        ui.EndWindow();

        ui.BeginWindow("Test", flags);
        const f32 ftime = Onyx::GetDeltaTime(win).AsMilliseconds();
        ui.Text("Delta time: {:.2f} ms", ftime);

        ui.Button("Wow!");
        ui.Button("Yes!");
        ui.Button("No!");
        if (ui.Button("A!"))
            TKit::PrintLine("Hello! ive been pressed");

        static char buf1[32] = "Some long wooords tooo test";
        ui.InputText("Text 1", buf1, 32);
        static char buf2[32] = "Some wooords test... again!";
        ui.InputText("Text 2", buf2, 32);

        static u32 val = 4;
        ui.InputNumeric("Some number", &val);

        static bool sliders = false;
        static bool anotherWindow = false;
        ui.PushDirection(Onyx::LayoutDirection_LeftToRight);
        ui.CheckBox("Extra window", &anotherWindow);
        ui.Separator();
        ui.CheckBox("Enable sliders", &sliders);
        ui.Pop();

        if (sliders)
        {
            ui.HorizontalSeparator("Here are some sliders!");
            static f32 fval[2] = {4, 7};
            ui.HorizontalSlider("My slider float", fval, 0.f, 10.f, 2);

            static u32 uval = 7;
            ui.HorizontalSlider("My slider uint", &uval, 3, 28);

            static u32 uval2[3] = {7, 2, 5};
            ui.HorizontalDrag("My drag uint", uval2, 1, 1, 87, 3);
        }

        ui.EndWindow();

        if (anotherWindow)
        {
            ui.BeginWindow("Im another window!", flags);
            ui.Text("Happy to be here!");
            ui.EndWindow();
        }

        ui.Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
}
