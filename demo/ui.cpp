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
        static bool enableSettings = false;
        if (ui.BeginWindow("User interface demo", flags))
        {
            ui.HorizontalSeparator("General");
            const f32 ftime = Onyx::GetDeltaTime(win).AsMilliseconds();
            ui.Text("Delta time: {:.2f} ms", ftime);
            ui.CheckBox("Open widow settings", &enableSettings);

            static bool helloText = false;
            if (ui.Button("This is a button"))
                helloText = !helloText;

            if (helloText)
                ui.Text("Hi!");

            ui.HorizontalSeparator("Sliders/Drags");
            static f32 fval[2] = {4, 7};
            ui.HorizontalSlider("My slider float", fval, 0.f, 10.f, "Value: {:.1f}", 2);

            static i32 ival = 7;
            ui.HorizontalSlider("My slider int", &ival, -3, 28);

            static u32 uval2[3] = {7, 2, 5};
            ui.HorizontalDrag("My drag uint", uval2, 1, 1, 87, nullptr, 3);

            ui.HorizontalSeparator("Inputs");

            static Onyx::OverlayInputFlags iflags = 0;
            ui.CheckBoxFlags("OverlayInputFlag_EnterReturnsTrue", &iflags, Onyx::OverlayInputFlag_EnterReturnsTrue);
            ui.CheckBoxFlags("OverlayInputFlag_EnterCommitsBuffer", &iflags, Onyx::OverlayInputFlag_EnterCommitsBuffer);
            ui.CheckBoxFlags("OverlayInputFlag_EscapeClearsAll", &iflags, Onyx::OverlayInputFlag_EscapeClearsAll);
            ui.CheckBoxFlags("OverlayInputFlag_AutoSelectAll", &iflags, Onyx::OverlayInputFlag_AutoSelectAll);
            ui.CheckBoxFlags("OverlayInputFlag_NoHorizontalScroll", &iflags, Onyx::OverlayInputFlag_NoHorizontalScroll);
            ui.CheckBoxFlags("OverlayInputFlag_ElideLeft", &iflags, Onyx::OverlayInputFlag_ElideLeft);

            static char buf1[32] = "This is some nice text";
            ui.InputText("Text 1", buf1, 32, iflags);

            static i32 iival = 4;
            ui.InputNumeric("Some integer", &iival, "{}", iflags);

            static f32 ifval = 8.f;
            ui.InputNumeric("Some float", &ifval, "{:.3f}", iflags);
        }
        ui.EndWindow();

        if (enableSettings)
        {
            if (ui.BeginWindow("Window settings", flags))
            {
                ui.CheckBoxFlags("OverlayWindowFlag_NoResize", &flags, Onyx::OverlayWindowFlag_NoResize);
                ui.CheckBoxFlags("OverlayWindowFlag_NoMove", &flags, Onyx::OverlayWindowFlag_NoMove);
                ui.CheckBoxFlags("OverlayWindowFlag_NoCollapse", &flags, Onyx::OverlayWindowFlag_NoCollapse);
                ui.CheckBoxFlags("OverlayWindowFlag_NoScrollBar", &flags, Onyx::OverlayWindowFlag_NoScrollBar);
                ui.CheckBoxFlags("OverlayWindowFlag_NoBackground", &flags, Onyx::OverlayWindowFlag_NoBackground);
                ui.CheckBoxFlags("OverlayWindowFlag_NoHeaderBar", &flags, Onyx::OverlayWindowFlag_NoHeaderBar);
                ui.CheckBoxFlags("OverlayWindowFlag_NoBringToFocus", &flags, Onyx::OverlayWindowFlag_NoBringToFocus);
                ui.CheckBoxFlags("OverlayWindowFlag_AlwaysAutoResize", &flags,
                                 Onyx::OverlayWindowFlag_AlwaysAutoResize);
            }
            ui.EndWindow();
        }

        ui.Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
}
