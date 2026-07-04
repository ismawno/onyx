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
    Onyx::Overlay *ui = win->CreateOverlay();

    Onyx::RenderContext<D2> *ctx = ui->GetContext();

    f32v2 rdims = {800, 600};
    Onyx::RenderTexture *rt = Onyx::CreateRenderTexture(rdims);

    ctx->AddTarget(rt->CreateRenderView(&ui->GetCamera(), ui->GetRenderViewFlags()));
    while (Onyx::Running())
    {
        static Onyx::OverlayWindowFlags wflags = 0;
        static bool enableSettings = false;
        static bool enableMainMenu = false;

        const Onyx::OverlayTreeFlags drawLines = Onyx::OverlayTreeFlag_DrawLines;

        const auto editWindowFlags = [&](Onyx::OverlayWindowFlags *flags) {
            ui->CheckBoxFlags("OverlayWindowFlag_NoResize", flags, Onyx::OverlayWindowFlag_NoResize);
            ui->CheckBoxFlags("OverlayWindowFlag_NoMove", flags, Onyx::OverlayWindowFlag_NoMove);
            ui->CheckBoxFlags("OverlayWindowFlag_NoCollapse", flags, Onyx::OverlayWindowFlag_NoCollapse);
            ui->CheckBoxFlags("OverlayWindowFlag_NoScrollBar", flags, Onyx::OverlayWindowFlag_NoScrollBar);
            ui->CheckBoxFlags("OverlayWindowFlag_NoHeaderBar", flags, Onyx::OverlayWindowFlag_NoHeaderBar);
            ui->CheckBoxFlags("OverlayWindowFlag_NoBringToFocus", flags, Onyx::OverlayWindowFlag_NoBringToFocus);
            ui->CheckBoxFlags("OverlayWindowFlag_AutoResize", flags, Onyx::OverlayWindowFlag_AutoResize);
            ui->CheckBoxFlags("OverlayWindowFlag_NoVerticalScroll", flags, Onyx::OverlayWindowFlag_NoVerticalScroll);
            ui->CheckBoxFlags("OverlayWindowFlag_HorizontalScroll", flags, Onyx::OverlayWindowFlag_HorizontalScroll);
            ui->CheckBoxFlags("OverlayWindowFlag_BringToTop", flags, Onyx::OverlayWindowFlag_BringToTop);
            ui->CheckBoxFlags("OverlayWindowFlag_Modal", flags, Onyx::OverlayWindowFlag_Modal);
            ui->CheckBoxFlags("OverlayWindowFlag_NoCloseButton", flags, Onyx::OverlayWindowFlag_NoCloseButton);
            ui->CheckBoxFlags("OverlayWindowFlag_MenuBar", flags, Onyx::OverlayWindowFlag_MenuBar);
        };
        const auto drawMenus = [&] {
            if (ui->BeginMenu("Options"))
            {
                ui->MenuItem("Window settings", &enableSettings);
                ui->MenuItem("Main menu bar", &enableMainMenu);
                ui->EndMenu();
            }
            if (ui->BeginMenu("Menu"))
            {
                static bool selected = false;
                ui->HorizontalSeparator("This is a demo menu");
                ui->MenuItem("New");
                ui->MenuItem("Open");
                if (ui->BeginMenu("Open as..."))
                {
                    ui->MenuItem("File 1");
                    ui->MenuItem("File 2");
                    ui->MenuItem("File 3");
                    ui->MenuItem("File 4");
                    if (ui->BeginMenu("More..."))
                    {
                        ui->MenuItem("Nothing to see here");
                        ui->EndMenu();
                    }
                    ui->EndMenu();
                }
                if (ui->BeginMenu("Options"))
                {
                    static f32 val = 3.f;
                    ui->HorizontalSlider("Slider", &val, -10.f, 10.f);
                    ui->Button("Press me");

                    ui->BeginScroll("Scroll", 100.f, Onyx::OverlayScrollFlag_Borders);
                    for (u32 i = 0; i < 10; ++i)
                        ui->Text("Bla bla");
                    ui->EndScroll();

                    static u32 element = 0;
                    ui->DropDown("Drop down", &element, "Hello 1#Hello 2#Hello 3");
                    ui->EndMenu();
                }
                ui->MenuItem("Select", &selected);
                ui->EndMenu();
            }
        };

        if (enableMainMenu)
        {
            ui->BeginMainMenuBar();
            drawMenus();
            ui->EndMainMenuBar();
        }

        static bool disableGlobal = false;
        if (ui->BeginWindow("Overlay demo", wflags | Onyx::OverlayWindowFlag_MenuBar))
        {
            const f32 ftime = Onyx::GetDeltaTime(win).AsMilliseconds();
            if (ui->PushTree("General", drawLines))
            {
                ui->Text("Delta time: {:.2f} ms", ftime);
                if (ui->BeginItemTooltip())
                {
                    ui->Text("I am a tooltip!");
                    ui->Text("And this is the time that passes between frames");
                    ui->EndTooltip();
                }
                static bool disableLocal = false;
                static bool dummy = false;

                ui->CheckBox("Disable other sections", &disableGlobal);
                ui->CheckBox("Disable items below", &disableLocal);

                ui->BeginDisabled(disableLocal);
                ui->Text("I can be disabled");
                ui->CheckBox("I can be disabled##CB", &dummy);
                ui->Button("I can be disabled##Button");
                ui->EndDisabled();

                ui->PopTree();
            }

            if (ui->BeginMenuBar())
            {
                drawMenus();
                ui->EndMenuBar();
            }

            ui->BeginDisabled(disableGlobal);
            if (ui->PushTree("Buttons", drawLines))
            {
                static bool helloText = false;
                if (ui->Button("This is a button"))
                    helloText = !helloText;

                if (helloText)
                    ui->Text("Hi!");

                ui->Button("I have a twin##Cant see me");
                ui->Button("I have a twin##Cant see me eiter");
                ui->Button("I am a long button", Onyx::OverlayButtonFlag_SpanFullWidth);

                ui->PushDirection(Onyx::LayoutDirection_LeftToRight, 0.f);
                ui->TextRaw("A small button can be easily ");
                ui->Button("embedded", Onyx::OverlayButtonFlag_Small);
                ui->TextRaw(" in text");
                ui->PopDirection();

                ui->PushDirection(Onyx::LayoutDirection_LeftToRight);
                static u32 radio = 0;
                ui->RadioButton("I am enabled!", &radio, 0);
                ui->RadioButton("I am not :(", &radio, 1);

                ui->PopDirection();
                ui->PopTree();
            }

            if (ui->PushTree("Color editor", drawLines))
            {
                static Onyx::OverlayColorFlags cflags = 0;
                ui->CheckBoxFlags("OverlayColorFlag_NoAlpha", &cflags, Onyx::OverlayColorFlag_NoAlpha);
                ui->CheckBoxFlags("OverlayColorFlag_NoInput", &cflags, Onyx::OverlayColorFlag_NoInput);
                ui->CheckBoxFlags("OverlayColorFlag_NoColorMarkers", &cflags, Onyx::OverlayColorFlag_NoColorMarkers);
                ui->CheckBoxFlags("OverlayColorFlag_NoPicker", &cflags, Onyx::OverlayColorFlag_NoPicker);
                ui->CheckBoxFlags("OverlayColorFlag_NoTooltip", &cflags, Onyx::OverlayColorFlag_NoTooltip);
                ui->CheckBoxFlags("OverlayColorFlag_NoPreview", &cflags, Onyx::OverlayColorFlag_NoPreview);
                ui->CheckBoxFlags("OverlayColorFlag_NoTooltipLabel", &cflags, Onyx::OverlayColorFlag_NoTooltipLabel);
                ui->CheckBoxFlags("OverlayColorFlag_HSV", &cflags, Onyx::OverlayColorFlag_HSV);
                ui->CheckBoxFlags("OverlayColorFlag_Hex", &cflags, Onyx::OverlayColorFlag_Hex);
                ui->CheckBoxFlags("OverlayColorFlag_Float", &cflags, Onyx::OverlayColorFlag_Float);

                static Onyx::Color col = Onyx::Color_Red;
                ui->ColorEditor("Color", &col, cflags);
                ui->ColorPreview("Preview", col, cflags);
                ui->ColorButton("Button", &col, cflags);
                ui->ColorPicker("Picker", &col, cflags);
                ui->PopTree();
            }

            if (ui->PushTree("Dropdowns", drawLines))
            {
                static Onyx::OverlayDropDownFlags dflags = 0;
                ui->CheckBoxFlags("OverlayDropDownFlag_NoArrowButton", &dflags,
                                  Onyx::OverlayDropDownFlag_NoArrowButton);
                ui->CheckBoxFlags("OverlayDropDownFlag_NoPreview", &dflags, Onyx::OverlayDropDownFlag_NoPreview);
                ui->CheckBoxFlags("OverlayDropDownFlag_HeightSmall", &dflags, Onyx::OverlayDropDownFlag_HeightSmall);
                ui->CheckBoxFlags("OverlayDropDownFlag_HeightRegular", &dflags,
                                  Onyx::OverlayDropDownFlag_HeightRegular);
                ui->CheckBoxFlags("OverlayDropDownFlag_HeightLargest", &dflags,
                                  Onyx::OverlayDropDownFlag_HeightLargest);
                ui->CheckBoxFlags("OverlayDropDownFlag_Tight", &dflags, Onyx::OverlayDropDownFlag_Tight);
                if (ui->BeginDropDown("Hello", "Some preview", dflags))
                {
                    static bool dummy = false;
                    ui->TextRaw("Some text");
                    ui->Button("I am a button in a drop down!");
                    ui->CheckBox("You can pretty much put whatever you want in here...", &dummy);

                    if (ui->BeginDropDown("Even another dropdown!", "I am another preview", dflags))
                    {
                        for (u32 i = 0; i < 10; ++i)
                            ui->TextRaw("Bla bla");
                        ui->EndDropDown();
                    }

                    ui->EndDropDown();
                }
                const TKit::FixedArray<TKit::StringView, 8> elements{"Element 1", "Element 2", "Element 3",
                                                                     "Element 4", "Element 5", "Element 6",
                                                                     "Element 7", "Element 8"};
                static u32 idx = 0;
                ui->DropDown("One-liner 1", &idx, elements, dflags);
                ui->DropDown("One-liner 2##You should not see this", &idx, "I am#part of#the same#string", dflags);
                ui->PopTree();
            }

            if (ui->PushTree("Images", drawLines))
            {
                ui->Text("May get a bit trippy...");
                if (ui->HorizontalSlider("Image dimensions", &rdims, 50.f, 1600.f, "{:.0f}"))
                    rt->Resize(rdims);
                ui->Image(*rt, 0.25f * rdims);
                ui->PopTree();
            }

            if (ui->PushTree("Inputs", drawLines))
            {
                static Onyx::OverlayInputFlags iflags = 0;
                ui->CheckBoxFlags("OverlayInputFlag_EnterReturnsTrue", &iflags,
                                  Onyx::OverlayInputFlag_EnterReturnsTrue);
                ui->CheckBoxFlags("OverlayInputFlag_EnterCommitsBuffer", &iflags,
                                  Onyx::OverlayInputFlag_EnterCommitsBuffer);
                ui->CheckBoxFlags("OverlayInputFlag_EscapeClearsAll", &iflags, Onyx::OverlayInputFlag_EscapeClearsAll);
                ui->CheckBoxFlags("OverlayInputFlag_AutoSelectAll", &iflags, Onyx::OverlayInputFlag_AutoSelectAll);
                ui->CheckBoxFlags("OverlayInputFlag_NoHorizontalScroll", &iflags,
                                  Onyx::OverlayInputFlag_NoHorizontalScroll);
                ui->CheckBoxFlags("OverlayInputFlag_ElideLeft", &iflags, Onyx::OverlayInputFlag_ElideLeft);
                ui->CheckBoxFlags("OverlayInputFlag_StepButtons", &iflags, Onyx::OverlayInputFlag_StepButtons);

                static char buf1[32] = "This is some nice text";
                ui->InputText("Text 1", buf1, 32, "I am a little hint", iflags);

                static i32 iival = 4;
                ui->InputNumeric("Some integer", &iival, "{}", "Add a number!", iflags);

                static f32 ifval = 8.f;
                ui->InputNumeric("Some float", &ifval, "{:.3f}", nullptr, iflags);
                ui->PopTree();
            }

            if (ui->PushTree("Popups", drawLines))
            {
                static Onyx::OverlayWindowFlags pflags =
                    Onyx::OverlayWindowFlag_BringToTop | Onyx::OverlayWindowFlag_AutoResize;
                editWindowFlags(&pflags);

                if (ui->Button("Open popup"))
                    ui->OpenPopup("Popup example");

                if (ui->BeginPopup("Popup example", pflags))
                {
                    ui->TextRaw("I am a popup");
                    if (ui->Button("Another one?"))
                        ui->OpenPopup("Yes, another one");

                    ui->SetItemTooltip("This will open another popup!");

                    if (ui->BeginPopup("Yes, another one", pflags))
                    {
                        ui->TextRaw("Hi!");

                        static u32 value = 3;
                        ui->SetNextTextId("Text id");
                        ui->Text("Right click me and change the value!: {}", value);
                        if (ui->BeginPopupContextItem("Value edit", Onyx::OverlayWindowFlag_AutoResize |
                                                                        Onyx::OverlayWindowFlag_BringToTop))
                        {
                            ui->InputNumeric("Value", &value);
                            ui->EndPopup();
                        }

                        if (ui->Button("Close##Two"))
                            ui->CloseCurrentPopup();
                        ui->EndPopup();
                    }

                    if (ui->Button("Close##One"))
                        ui->CloseCurrentPopup();
                    ui->EndPopup();
                }
                ui->PopTree();
            }

            if (ui->PushTree("Queries", drawLines))
            {
                ui->HorizontalSeparator("Hover flags");
                static Onyx::OverlayHoveredFlags hflags = 0;

                ui->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByWindow", &hflags,
                                  Onyx::OverlayHoveredFlag_AllowBlockedByWindow);
                ui->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByWindowGrab", &hflags,
                                  Onyx::OverlayHoveredFlag_AllowBlockedByWindowGrab);
                ui->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByPressedItem", &hflags,
                                  Onyx::OverlayHoveredFlag_AllowBlockedByPressedItem);
                ui->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByActiveItem", &hflags,
                                  Onyx::OverlayHoveredFlag_AllowBlockedByActiveItem);
                ui->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByPopup", &hflags,
                                  Onyx::OverlayHoveredFlag_AllowBlockedByPopup);
                ui->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByPopupCollapse", &hflags,
                                  Onyx::OverlayHoveredFlag_AllowBlockedByPopupCollapse);
                ui->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByDisabled", &hflags,
                                  Onyx::OverlayHoveredFlag_AllowBlockedByDisabled);
                ui->CheckBoxFlags("OverlayHoveredFlag_NoSharedDelay", &hflags, Onyx::OverlayHoveredFlag_NoSharedDelay);

                ui->BeginDisabled(hflags & Onyx::OverlayHoveredFlag_NormalDelay);
                ui->CheckBoxFlags("OverlayHoveredFlag_ShortDelay", &hflags, Onyx::OverlayHoveredFlag_ShortDelay);
                ui->EndDisabled();

                ui->BeginDisabled(hflags & Onyx::OverlayHoveredFlag_ShortDelay);
                ui->CheckBoxFlags("OverlayHoveredFlag_NormalDelay", &hflags, Onyx::OverlayHoveredFlag_NormalDelay);
                ui->EndDisabled();

                ui->CheckBoxFlags("OverlayHoveredFlag_Stationary", &hflags, Onyx::OverlayHoveredFlag_Stationary);

                ui->HorizontalSeparator("Focus flags");
                static Onyx::OverlayFocusFlags fflags = 0;
                ui->CheckBoxFlags("OverlayFocusFlag_PressedEvenWhenAwayFromHover", &fflags,
                                  Onyx::OverlayFocusFlag_PressedEvenWhenAwayFromHover);

                ui->HorizontalSeparator("The experiment");
                if (ui->PushTree("I am to be queried"))
                    ui->PopTree();

                const bool hovered = ui->IsItemHovered(hflags);
                const bool opened = ui->IsItemOpened();
                const Onyx::OverlayHoverQueryFlags hqflags = ui->QueryItemHoverStatus();
                const Onyx::OverlayFocusQueryFlags fqflags = ui->QueryItemFocusStatus(fflags);

                ui->HorizontalSeparator("Hovering info");
                ui->Text("Hovered: {}", hovered);
                ui->Text("Blocked by window: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByWindow));
                ui->Text("Blocked by window grab: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByWindowGrab));
                ui->Text("Blocked by pressed item: {}",
                         bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPressedItem));
                ui->Text("Blocked by active item: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByActiveItem));
                ui->Text("Blocked by popup : {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPopup));
                ui->Text("Blocked by popup collapse : {}",
                         bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPopupCollapse));
                ui->Text("Blocked by disabled : {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByDisabled));
                ui->Text("Natively hovered: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_Hovered));

                ui->HorizontalSeparator("Focus info");

                static TKit::Clock lclickClock{};
                static TKit::Clock rclickClock{};
                static TKit::Clock dclickClock{};

                if (fqflags & Onyx::OverlayFocusQueryFlag_LeftClicked)
                    lclickClock.Restart();
                if (fqflags & Onyx::OverlayFocusQueryFlag_RightClicked)
                    rclickClock.Restart();
                if (fqflags & Onyx::OverlayFocusQueryFlag_DoubleClicked)
                    dclickClock.Restart();

                ui->Text("Hovered: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_Hovered));
                ui->Text("Pressed: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_Pressed));
                ui->Text("Left clicked: {:.1f} seconds ago", lclickClock.GetElapsed().AsSeconds());
                ui->Text("Right clicked: {:.1f} seconds ago", rclickClock.GetElapsed().AsSeconds());
                ui->Text("Double clicked: {:.1f} seconds ago", dclickClock.GetElapsed().AsSeconds());
                ui->Text("Active: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_Active));
                ui->Text("Just active: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_JustActive));
                ui->Text("Popup open: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_PopupOpen));

                ui->HorizontalSeparator("State info");
                ui->Text("Opened: {}", opened);

                ui->HorizontalSeparator("Focus info");
                ui->Text("Want capture mouse: {}", ui->WantCaptureMouse());
                ui->Text("Want capture keyboard: {}", ui->WantCaptureKeyboard());

                ui->PopTree();
            }

            if (ui->PushTree("Selectables", drawLines))
            {
                static Onyx::OverlaySelectableFlags sflags = 0;

                ui->CheckBoxFlags("OverlaySelectableFlag_SpanLabelWidth", &sflags,
                                  Onyx::OverlaySelectableFlag_SpanLabelWidth);
                ui->CheckBoxFlags("OverlaySelectableFlag_SelectOnDoubleClick", &sflags,
                                  Onyx::OverlaySelectableFlag_SelectOnDoubleClick);
                ui->CheckBoxFlags("OverlaySelectableFlag_Highlight", &sflags, Onyx::OverlaySelectableFlag_Highlight);
                ui->CheckBoxFlags("OverlaySelectableFlag_CheckBox", &sflags, Onyx::OverlaySelectableFlag_CheckBox);

                ui->Selectable("I am not selected at all##One", false, sflags);
                ui->Selectable("I am permanently selected", true, sflags);
                ui->Selectable("I am not selected at all##Two", false, sflags);

                static bool enabled[3] = {false, false, false};
                ui->Selectable("I can be toggled on and off##One", &enabled[0], sflags);
                ui->Selectable("I can be toggled on and off##Two", &enabled[1], sflags);

                ui->BeginSelectable(&enabled[2], sflags);
                ui->TextRaw("I am a fancy selectable");
                ui->TextRaw("I even have multiple lines");
                ui->EndSelectable();

                ui->PopTree();
            }

            if (ui->PushTree("Scroll area", drawLines))
            {
                static f32v2 dimensions = {400.f, 200.f};
                static bool xunlim = true;
                static Onyx::OverlayScrollFlags sflags = 0;
                ui->CheckBoxFlags("OverlayScrollFlag_Borders", &sflags, Onyx::OverlayScrollFlag_Borders);
                ui->CheckBoxFlags("OverlayScrollFlag_Title", &sflags, Onyx::OverlayScrollFlag_Title);
                ui->CheckBoxFlags("OverlayScrollFlag_NoBackground", &sflags, Onyx::OverlayScrollFlag_NoBackground);
                ui->CheckBoxFlags("OverlayScrollFlag_NoScrollBar", &sflags, Onyx::OverlayScrollFlag_NoScrollBar);
                ui->CheckBoxFlags("OverlayScrollFlag_NoVerticalScroll", &sflags,
                                  Onyx::OverlayScrollFlag_NoVerticalScroll);
                ui->CheckBoxFlags("OverlayScrollFlag_HorizontalScroll", &sflags,
                                  Onyx::OverlayScrollFlag_HorizontalScroll);

                ui->CheckBox("Unlimited width", &xunlim);
                if (xunlim)
                    ui->HorizontalSlider("Maximum height", &dimensions[1], 50.f, 800.f, "{:.0f}");
                else
                    ui->HorizontalSlider("Maximum dimensions", &dimensions, 50.f, 800.f, "{:.0f}");

                const bool focused =
                    ui->BeginScroll("Title", dimensions[1], xunlim ? TKIT_F32_MAX : dimensions[0], sflags);

                if (focused)
                    ui->TextRaw("I am focused!");
                else
                    ui->TextRaw("I am not focused");

                ui->TextRaw(
                    "I am a long text that will require you to scroll horizontally to read fully, allowing me to "
                    "showcase the feature");
                ui->Button("I am a useless button");
                if (ui->PushTree("Some content", Onyx::OverlayTreeFlag_StartOpen))
                {
                    for (u32 i = 0; i < 10; ++i)
                        ui->Text("Bla bla");
                    ui->PopTree();
                }

                ui->BeginScroll("I am yet another scroll", 200.f, 200.f, sflags);
                if (ui->PushTree("Some more content"))
                {
                    for (u32 i = 0; i < 10; ++i)
                        ui->Text("Bla bla");
                    ui->PopTree();
                }
                ui->EndScroll();

                ui->EndScroll();
                ui->PopTree();
            }

            if (ui->PushTree("Sliders/Drags", drawLines))
            {
                static Onyx::OverlaySliderFlags sflags = 0;
                ui->CheckBoxFlags("OverlaySliderFlag_ClampOnInput", &sflags, Onyx::OverlaySliderFlag_ClampOnInput);
                ui->CheckBoxFlags("OverlaySliderFlag_Logarithmic", &sflags, Onyx::OverlaySliderFlag_Logarithmic);
                ui->CheckBoxFlags("OverlaySliderFlag_NoRoundToFormat", &sflags,
                                  Onyx::OverlaySliderFlag_NoRoundToFormat);
                ui->CheckBoxFlags("OverlaySliderFlag_NoInput", &sflags, Onyx::OverlaySliderFlag_NoInput);

                ui->HorizontalSeparator("Sliders");
                static f32 fval[2] = {4, 7};
                ui->Text("Underlying values: {:.2f}, {:.2f}", fval[0], fval[1]);

                ui->HorizontalSlider("My slider float", fval, 0.f, 10.f, "Value: {:.1f}", 2, sflags);
                ui->HorizontalSlider("My other slider float", fval, -10.f, 20.f, "{:.2f}", 1, sflags);

                static i32 ival = 7;
                ui->Text("Underlying value: {}", ival);
                ui->HorizontalSlider("My slider int", &ival, -3, 28, nullptr, 1, sflags);
                ui->HorizontalSlider("My small slider int", &ival, 0, 2, nullptr, 1, sflags);

                ui->HorizontalSeparator("Drags");
                static f32 speed = 0.1f;
                ui->HorizontalSlider("Drag speed", &speed, 1e-2f, 10.f, "{:.2f}", 1,
                                     Onyx::OverlaySliderFlag_Logarithmic);

                ui->HorizontalDrag("My drag float", fval, speed, 0.f, 10.f, "Value: {:.1f}", 2, sflags);
                ui->HorizontalDrag("My other drag float", fval, speed, -10.f, 20.f, "{:.2f}", 1, sflags);

                ui->HorizontalDrag("My drag int", &ival, speed, -3, 28, nullptr, 1, sflags);
                ui->HorizontalDrag("My small drag int", &ival, speed, 0, 2, nullptr, 1, sflags);

                static u32 uval2[3] = {7, 2, 5};
                ui->HorizontalDrag("My drag uint", uval2, speed, 1, 87, nullptr, 3, sflags);
                ui->PopTree();
            }

            if (ui->PushTree("Text", drawLines))
            {
                ui->TextRaw("This is some raw text");
                ui->TextRaw(
                    Onyx::TextMode_Wrapped,
                    "This is some text that should wrap because it is too long to fit into the width of the window");
                ui->TextIconRaw(Onyx::BulletIcon, "A bullet!");
                ui->TextIcon(Onyx::ArrowRightIcon, "Here is the delta time again: {:.2f} ms", ftime);
                ui->PopTree();
            }

            if (ui->PushTree("Tooltips", drawLines))
            {
                ui->Button("I am an instant tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui->IsItemHovered())
                    ui->SetTooltip("I am instant!");

                ui->Button("I am a short-delayed tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui->IsItemHovered(Onyx::OverlayHoveredFlag_ShortDelay))
                    ui->SetTooltip("I am a bit delayed!");

                ui->Button("I am a normal-delayed tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui->IsItemHovered(Onyx::OverlayHoveredFlag_NormalDelay))
                    ui->SetTooltip("I am delayed!");

                ui->Button("I am a stationary tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
                if (ui->IsItemHovered(Onyx::OverlayHoveredFlag_Stationary | Onyx::OverlayHoveredFlag_NormalDelay))
                    ui->SetTooltip("I am stationary!");
                ui->PopTree();
            }

            if (ui->PushTree("Trees", drawLines))
            {
                static Onyx::OverlayTreeFlags tflags = 0;
                ui->CheckBoxFlags("OverlayTreeFlag_DrawLines", &tflags, drawLines);
                ui->CheckBoxFlags("OverlayTreeFlag_ToggleOnArrow", &tflags, Onyx::OverlayTreeFlag_ToggleOnArrow);
                ui->CheckBoxFlags("OverlayTreeFlag_OpenOnDoubleClick", &tflags,
                                  Onyx::OverlayTreeFlag_OpenOnDoubleClick);
                ui->CheckBoxFlags("OverlayTreeFlag_SpanLabelWidth", &tflags, Onyx::OverlayTreeFlag_SpanLabelWidth);
                ui->CheckBoxFlags("OverlayTreeFlag_Framed", &tflags, Onyx::OverlayTreeFlag_Framed);
                ui->CheckBoxFlags("OverlayTreeFlag_NoIndent", &tflags, Onyx::OverlayTreeFlag_NoIndent);

                if (ui->PushTree("Click me", tflags))
                {
                    if (ui->PushTree("Simple example", tflags))
                    {
                        ui->Button("Hello");
                        ui->PopTree();
                    }
                    if (ui->PushTree("I am open", tflags | Onyx::OverlayTreeFlag_StartOpen))
                    {
                        ui->Text("You can see me");
                        ui->PopTree();
                    }
                    ui->PopTree();
                }
                ui->PopTree();
            }
            ui->EndDisabled();
            ui->EndWindow();
        }

        if (ui->BeginWindow("Window settings", &enableSettings, wflags))
        {
            editWindowFlags(&wflags);
            ui->EndWindow();
        }

        if (ui->BeginWindow("Overlay demo"))
        {
            if (ui->PushTree("Miscellaneous", drawLines))
            {
                ui->TextRaw(Onyx::TextMode_Wrapped, "Nothing to see here! This is extra content to demonstrate "
                                                    "appending multiple times to a window after it has "
                                                    "been closed");
                if (ui->BeginMenu("A rogue menu item"))
                {
                    drawMenus();
                    ui->EndMenu();
                }
                ui->PopTree();
            }
            ui->EndWindow();
        }

        ui->Draw();

        Onyx::Transfer();
        Onyx::Render();
    }
    Onyx::Terminate();
}
