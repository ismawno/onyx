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
    Onyx::Overlay ui{win};

    Onyx::RenderContext<D2> *ctx = ui.GetContext();

    const f32v2 rdims = {800, 600};
    Onyx::RenderTexture *rt = Onyx::CreateRenderTexture(rdims);
    ctx->AddTarget(rt->CreateRenderView(&ui.GetCamera(), ui.GetRenderViewFlags()));

    while (Onyx::Running())
    {
        static Onyx::OverlayWindowFlags flags = 0;
        static bool enableSettings = false;
        if (ui.BeginWindow("User interface demo", flags))
        {
            if (ui.PushTree("General", Onyx::OverlayTreeFlag_DrawLines))
            {
                const f32 ftime = Onyx::GetDeltaTime(win).AsMilliseconds();
                ui.Text("Delta time: {:.2f} ms", ftime);
                if (ui.BeginItemTooltip())
                {
                    ui.Text("Im a tooltip!");
                    ui.Text("And this is the time that passes between frames");
                    ui.EndTooltip();
                }
                ui.TextWrapped(
                    "This is some text that should wrap because it is too long to fit into the width of the window");

                ui.CheckBox("Open widow settings", &enableSettings);

                static bool helloText = false;
                if (ui.Button("This is a button"))
                    helloText = !helloText;

                if (helloText)
                    ui.Text("Hi!");

                ui.Button("I have a twin##Cant see me");
                ui.Button("I have a twin##Cant see me eiter");

                ui.PushDirection(Onyx::LayoutDirection_LeftToRight);
                static u32 radio = 0;
                ui.RadioButton("Im enabled!", &radio, 0);
                ui.RadioButton("Im not :(", &radio, 1);

                ui.Pop();
                ui.PopTree();
            }

            if (ui.PushTree("Images", Onyx::OverlayTreeFlag_DrawLines))
            {
                ui.Text("May get a bit trippy...");
                ui.Image(*rt, 0.25f * rdims);
                ui.PopTree();
            }

            if (ui.PushTree("Inputs", Onyx::OverlayTreeFlag_DrawLines))
            {
                static Onyx::OverlayInputFlags iflags = 0;
                ui.CheckBoxFlags("OverlayInputFlag_EnterReturnsTrue", &iflags, Onyx::OverlayInputFlag_EnterReturnsTrue);
                ui.CheckBoxFlags("OverlayInputFlag_EnterCommitsBuffer", &iflags,
                                 Onyx::OverlayInputFlag_EnterCommitsBuffer);
                ui.CheckBoxFlags("OverlayInputFlag_EscapeClearsAll", &iflags, Onyx::OverlayInputFlag_EscapeClearsAll);
                ui.CheckBoxFlags("OverlayInputFlag_AutoSelectAll", &iflags, Onyx::OverlayInputFlag_AutoSelectAll);
                ui.CheckBoxFlags("OverlayInputFlag_NoHorizontalScroll", &iflags,
                                 Onyx::OverlayInputFlag_NoHorizontalScroll);
                ui.CheckBoxFlags("OverlayInputFlag_ElideLeft", &iflags, Onyx::OverlayInputFlag_ElideLeft);

                static char buf1[32] = "This is some nice text";
                ui.InputText("Text 1", buf1, 32, "Im a little hint", iflags);

                static i32 iival = 4;
                ui.InputNumeric("Some integer", &iival, "{}", "Add a number!", iflags);

                static f32 ifval = 8.f;
                ui.InputNumeric("Some float", &ifval, "{:.3f}", nullptr, iflags);
                ui.PopTree();
            }

            if (ui.PushTree("Sliders/Drags", Onyx::OverlayTreeFlag_DrawLines))
            {
                static f32 fval[2] = {4, 7};
                ui.HorizontalSlider("My slider float", fval, 0.f, 10.f, "Value: {:.1f}", 2);

                static i32 ival = 7;
                ui.HorizontalSlider("My slider int", &ival, -3, 28);

                static u32 uval2[3] = {7, 2, 5};
                ui.HorizontalDrag("My drag uint", uval2, 1, 1, 87, nullptr, 3);
                ui.PopTree();
            }

            if (ui.PushTree("Tooltips", Onyx::OverlayTreeFlag_DrawLines))
            {
                ui.Button("Im an instant tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui.IsItemHovered())
                    ui.SetTooltip("Im instant!");

                ui.Button("Im a short-delayed tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui.IsItemHovered(Onyx::OverlayHoveredFlag_ShortDelay))
                    ui.SetTooltip("Im a bit delayed!");

                ui.Button("Im a normal-delayed tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui.IsItemHovered(Onyx::OverlayHoveredFlag_ShortDelay))
                    ui.SetTooltip("Im delayed!");

                ui.Button("Im a stationary tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui.IsItemHovered(Onyx::OverlayHoveredFlag_Stationary | Onyx::OverlayHoveredFlag_NormalDelay))
                    ui.SetTooltip("Im stationary!");
                ui.PopTree();
            }

            if (ui.PushTree("Trees", Onyx::OverlayTreeFlag_DrawLines))
            {
                static Onyx::OverlayTreeFlags tflags = 0;
                ui.CheckBoxFlags("OverlayTreeFlag_DrawLines", &tflags, Onyx::OverlayTreeFlag_DrawLines);
                ui.CheckBoxFlags("OverlayTreeFlag_OpenOnArrow", &tflags, Onyx::OverlayTreeFlag_OpenOnArrow);
                ui.CheckBoxFlags("OverlayTreeFlag_OpenOnDoubleClick", &tflags, Onyx::OverlayTreeFlag_OpenOnDoubleClick);
                ui.CheckBoxFlags("OverlayTreeFlag_SpanLabelWidth", &tflags, Onyx::OverlayTreeFlag_SpanLabelWidth);
                ui.CheckBoxFlags("OverlayTreeFlag_Framed", &tflags, Onyx::OverlayTreeFlag_Framed);
                ui.CheckBoxFlags("OverlayTreeFlag_NoIndent", &tflags, Onyx::OverlayTreeFlag_NoIndent);

                if (ui.PushTree("Click me", tflags))
                {
                    if (ui.PushTree("Simple example", tflags))
                    {
                        ui.Button("Hello");
                        ui.PopTree();
                    }
                    if (ui.PushTree("Im open", tflags | Onyx::OverlayTreeFlag_StartOpen))
                    {
                        ui.Text("You can see me");
                        ui.PopTree();
                    }
                    ui.PopTree();
                }
                ui.PopTree();
            }
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
