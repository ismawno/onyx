#include "onyx/resources.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"

using Onyx::D2;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});
    Onyx::Overlay ui{win};

    Onyx::RenderContext<D2> *ctx = ui.GetContext();

    f32v2 rdims = {800, 600};
    Onyx::RenderTexture *rt = Onyx::CreateRenderTexture(rdims);
    ctx->AddTarget(rt->CreateRenderView(&ui.GetCamera(), ui.GetRenderViewFlags()));

    while (Onyx::Running())
    {
        static Onyx::OverlayWindowFlags flags = 0;
        static bool enableSettings = false;
        const Onyx::OverlayTreeFlags drawLines = Onyx::OverlayTreeFlag_DrawLines;
        if (ui.BeginWindow("User interface demo", flags))
        {
            const f32 ftime = Onyx::GetDeltaTime(win).AsMilliseconds();
            if (ui.PushTree("General", drawLines))
            {
                ui.Text("Delta time: {:.2f} ms", ftime);
                if (ui.BeginItemTooltip())
                {
                    ui.Text("Im a tooltip!");
                    ui.Text("And this is the time that passes between frames");
                    ui.EndTooltip();
                }

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

            // if (ui.PushTree("Dropdowns", drawLines))
            // {
            //     const TKit::FixedArray<TKit::StringView, 4> elements{"Element 1", "Element 2", "Element 3",
            //                                                          "Element 4"};
            //     static u32 idx = 0;
            //     ui.DropDown("Dropdown", &idx, elements);
            //     ui.PopTree();
            // }

            if (ui.PushTree("Images", drawLines))
            {
                ui.Text("May get a bit trippy...");
                if (ui.HorizontalSlider("Image dimensions", &rdims, 50.f, 1600.f, "{:.0f}"))
                    rt->Resize(rdims);
                ui.Image(*rt, 0.25f * rdims);
                ui.PopTree();
            }

            if (ui.PushTree("Inputs", drawLines))
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

            if (ui.PushTree("Queries", drawLines))
            {
                static Onyx::OverlayHoveredFlags hflags = 0;
                ui.CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByWindow", &hflags,
                                 Onyx::OverlayHoveredFlag_AllowBlockedByWindow);
                ui.CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByResize", &hflags,
                                 Onyx::OverlayHoveredFlag_AllowBlockedByResize);
                ui.CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByPressedItem", &hflags,
                                 Onyx::OverlayHoveredFlag_AllowBlockedByPressedItem);
                ui.CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByActiveItem", &hflags,
                                 Onyx::OverlayHoveredFlag_AllowBlockedByActiveItem);
                ui.CheckBoxFlags("OverlayHoveredFlag_NoSharedDelay", &hflags, Onyx::OverlayHoveredFlag_NoSharedDelay);
                ui.CheckBoxFlags("OverlayHoveredFlag_ShortDelay", &hflags, Onyx::OverlayHoveredFlag_ShortDelay);
                ui.CheckBoxFlags("OverlayHoveredFlag_NormalDelay", &hflags, Onyx::OverlayHoveredFlag_NormalDelay);
                ui.CheckBoxFlags("OverlayHoveredFlag_Stationary", &hflags, Onyx::OverlayHoveredFlag_Stationary);

                if (ui.PushTree("I am to be queried"))
                    ui.PopTree();

                const bool hovered = ui.IsItemHovered(hflags);
                const bool active = ui.IsItemActive();
                const bool opened = ui.IsItemOpened();
                const Onyx::OverlayHoverQueryFlags qflags = ui.QueryItemHoverStatus();

                ui.Text("Hovered: {}", hovered);
                ui.Text("Active: {}", active);
                ui.Text("Opened: {}", opened);

                ui.Text("Blocked by window: {}", bool(qflags & Onyx::OverlayHoverQueryFlag_BlockedByWindow));
                ui.Text("Blocked by resize: {}", bool(qflags & Onyx::OverlayHoverQueryFlag_BlockedByResize));
                ui.Text("Blocked by pressed item: {}", bool(qflags & Onyx::OverlayHoverQueryFlag_BlockedByPressedItem));
                ui.Text("Blocked by active item: {}", bool(qflags & Onyx::OverlayHoverQueryFlag_BlockedByActiveItem));
                ui.Text("Natively hovered: {}", bool(qflags & Onyx::OverlayHoverQueryFlag_Hovered));

                ui.PopTree();
            }

            if (ui.PushTree("Scroll area", drawLines))
            {
                static f32v2 dimensions = {400.f, 200.f};
                static bool xunlim = true;
                static Onyx::OverlayScrollFlags sflags = 0;
                ui.CheckBoxFlags("OverlayScrollFlag_Borders", &sflags, Onyx::OverlayScrollFlag_Borders);
                ui.CheckBoxFlags("OverlayScrollFlag_Title", &sflags, Onyx::OverlayScrollFlag_Title);
                ui.CheckBoxFlags("OverlayScrollFlag_NoScrollBar", &sflags, Onyx::OverlayScrollFlag_NoScrollBar);
                ui.CheckBoxFlags("OverlayScrollFlag_NoVerticalScroll", &sflags,
                                 Onyx::OverlayScrollFlag_NoVerticalScroll);
                ui.CheckBoxFlags("OverlayScrollFlag_HorizontalScroll", &sflags,
                                 Onyx::OverlayScrollFlag_HorizontalScroll);

                ui.CheckBox("Unlimited width", &xunlim);
                if (xunlim)
                    ui.HorizontalSlider("Maximum height", &dimensions[1], 50.f, 800.f, "{:.0f}");
                else
                    ui.HorizontalSlider("Maximum dimensions", &dimensions, 50.f, 800.f, "{:.0f}");

                const bool focused =
                    ui.BeginScroll("Title", dimensions[1], xunlim ? TKIT_F32_MAX : dimensions[0], sflags);

                if (focused)
                    ui.TextRaw("Im focused!");
                else
                    ui.TextRaw("Im not focused");

                ui.TextRaw("Im a long text that will require you to scroll horizontally to read fully, allowing me to "
                           "showcase the feature");
                ui.Button("Im a useless button");
                if (ui.PushTree("Some content", Onyx::OverlayTreeFlag_StartOpen))
                {
                    for (u32 i = 0; i < 10; ++i)
                        ui.Text("Bla bla");
                    ui.PopTree();
                }

                ui.BeginScroll("I am yet another scroll", 200.f, 200.f, sflags);
                if (ui.PushTree("Some more content"))
                {
                    for (u32 i = 0; i < 10; ++i)
                        ui.Text("Bla bla");
                    ui.PopTree();
                }
                ui.EndScroll();

                ui.EndScroll();
                ui.PopTree();
            }

            if (ui.PushTree("Sliders/Drags", drawLines))
            {
                static Onyx::OverlaySliderFlags sflags = 0;
                ui.CheckBoxFlags("OverlaySliderFlag_ClampOnInput", &sflags, Onyx::OverlaySliderFlag_ClampOnInput);
                ui.CheckBoxFlags("OverlaySliderFlag_Logarithmic", &sflags, Onyx::OverlaySliderFlag_Logarithmic);
                ui.CheckBoxFlags("OverlaySliderFlag_NoRoundToFormat", &sflags, Onyx::OverlaySliderFlag_NoRoundToFormat);
                ui.CheckBoxFlags("OverlaySliderFlag_NoInput", &sflags, Onyx::OverlaySliderFlag_NoInput);

                ui.HorizontalSeparator("Sliders");
                static f32 fval[2] = {4, 7};
                ui.Text("Underlying values: {:.2f}, {:.2f}", fval[0], fval[1]);

                ui.HorizontalSlider("My slider float", fval, 0.f, 10.f, "Value: {:.1f}", 2, sflags);
                ui.HorizontalSlider("My other slider float", fval, -10.f, 20.f, "{:.2f}", 1, sflags);

                static i32 ival = 7;
                ui.Text("Underlying value: {}", ival);
                ui.HorizontalSlider("My slider int", &ival, -3, 28, nullptr, 1, sflags);
                ui.HorizontalSlider("My small slider int", &ival, 0, 2, nullptr, 1, sflags);

                ui.HorizontalSeparator("Drags");
                static f32 speed = 0.1f;
                ui.HorizontalSlider("Drag speed", &speed, 1e-2f, 10.f, "{:.2f}", 1,
                                    Onyx::OverlaySliderFlag_Logarithmic);

                ui.HorizontalDrag("My drag float", fval, speed, 0.f, 10.f, "Value: {:.1f}", 2, sflags);
                ui.HorizontalDrag("My other drag float", fval, speed, -10.f, 20.f, "{:.2f}", 1, sflags);

                ui.HorizontalDrag("My drag int", &ival, speed, -3, 28, nullptr, 1, sflags);
                ui.HorizontalDrag("My small drag int", &ival, speed, 0, 2, nullptr, 1, sflags);

                static u32 uval2[3] = {7, 2, 5};
                ui.HorizontalDrag("My drag uint", uval2, speed, 1, 87, nullptr, 3, sflags);
                ui.PopTree();
            }

            if (ui.PushTree("Text", drawLines))
            {
                ui.TextRaw("This is some raw text");
                ui.TextRaw(
                    Onyx::TextMode_Wrapped,
                    "This is some text that should wrap because it is too long to fit into the width of the window");
                ui.TextIconRaw(Onyx::BulletIcon, "A bullet!");
                ui.TextIcon(Onyx::ArrowRightIcon, "Here is the delta time again: {:.2f} ms", ftime);
                ui.PopTree();
            }

            if (ui.PushTree("Tooltips", drawLines))
            {
                ui.Button("Im an instant tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui.IsItemHovered())
                    ui.SetTooltip("Im instant!");

                ui.Button("Im a short-delayed tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui.IsItemHovered(Onyx::OverlayHoveredFlag_ShortDelay))
                    ui.SetTooltip("Im a bit delayed!");

                ui.Button("Im a normal-delayed tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui.IsItemHovered(Onyx::OverlayHoveredFlag_NormalDelay))
                    ui.SetTooltip("Im delayed!");

                ui.Button("Im a stationary tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui.IsItemHovered(Onyx::OverlayHoveredFlag_Stationary | Onyx::OverlayHoveredFlag_NormalDelay))
                    ui.SetTooltip("Im stationary!");
                ui.PopTree();
            }

            if (ui.PushTree("Trees", drawLines))
            {
                static Onyx::OverlayTreeFlags tflags = 0;
                ui.CheckBoxFlags("OverlayTreeFlag_DrawLines", &tflags, drawLines);
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
            ui.EndWindow();
        }

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
                ui.CheckBoxFlags("OverlayWindowFlag_AutoResize", &flags, Onyx::OverlayWindowFlag_AutoResize);
                ui.CheckBoxFlags("OverlayWindowFlag_NoVerticalScroll", &flags,
                                 Onyx::OverlayWindowFlag_NoVerticalScroll);
                ui.CheckBoxFlags("OverlayWindowFlag_HorizontalScroll", &flags,
                                 Onyx::OverlayWindowFlag_HorizontalScroll);
                ui.EndWindow();
            }
        }

        ui.Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
}
