#pragma once

#include "onyx/layout.hpp"
#include "onyx/context.hpp"
#include "onyx/window.hpp"

// ============================================================
// TODO(Isma): Overlay widget roadmap
// ============================================================
//
// --- Missing widgets ---
//
// [ ] Separator
// [ ] Tooltip
// [ ] SameLine
// [ ] Indent / Unindent
// [ ] TextInput (single line)
// [ ] ColorDisplay
// [ ] RadioButton
// [ ] ProgressBar
// [ ] Combo / Dropdown
// [ ] TreeNode
// [ ] Tabs
// [ ] TextInput (multiline)
// [ ] ColorPicker
// [ ] Table
// [ ] Docking
// [ ] MenuBar
//
// --- Global ---
//
// [ ] BeginDisabled / EndDisabled
//
// --- Per-widget flags ---
//
// Window:
//   [ ] NoResize
//   [ ] NoMove
//   [ ] NoCollapse
//   [ ] NoScrollBar
//   [ ] NoBackground
//   [ ] NoTitleBar
//   [ ] AlwaysAutoResize
//   [ ] NoBringToFrontOnFocus
//
// Button:
//   [ ] Small
//
// Slider:
//   [ ] NoLabel
//   [ ] Logarithmic
//   [ ] Vertical
//
// Drag:
//   [ ] NoLabel
//
// Text:
//   [ ] Bullet
//
// TextInput:
//   [ ] ReadOnly
//   [ ] Password (mask characters)
//   [ ] EnterReturnsTrue (confirm on enter only)
//   [ ] Hint (placeholder text)
//
// Combo:
//   [ ] NoPreview (hide current selection)
//   [ ] HeightSmall / HeightLarge (popup size)
//
// TreeNode:
//   [ ] DefaultOpen
//   [ ] Leaf (no arrow, not expandable)
//   [ ] Bullet (bullet instead of arrow)
//
// Tabs:
//   [ ] Reorderable
//   [ ] NoCloseButton
//
// Table:
//   [ ] Sortable
//   [ ] Resizable
//   [ ] NoBorders
//   [ ] RowBackground (alternating row colors)
//
// ColorPicker:
//   [ ] NoAlpha
//   [ ] NoLabel
//   [ ] NoInputs (hide text input fields)
//
// ============================================================

namespace Onyx
{
enum OverlayResizeEdge : u8
{
    OverlayResizeEdge_Left,
    OverlayResizeEdge_Right,
    OverlayResizeEdge_Bottom,
    OverlayResizeEdge_Top,
    OverlayResizeEdge_Count
};
using OverlayResizeFlags = u8;
using OverlayWindowFlags = u16;

enum OverlayWindowFlagBit : OverlayWindowFlags
{
    OverlayWindowFlag_NoResize = 1U << 4,
    OverlayWindowFlag_NoMove = 1U << 5,
    OverlayWindowFlag_NoCollapse = 1U << 6,
    OverlayWindowFlag_NoScrollBar = 1U << 7,
    OverlayWindowFlag_NoHeaderBar = 1U << 8,
    OverlayWindowFlag_NoBackground = 1U << 9,
    OverlayWindowFlag_NoBringToFocus = 1U << 10,
    OverlayWindowFlag_AlwaysAutoResize = 1U << 11,
};

struct OverlayResizeInfo
{
    TKit::FixedArray<usz, OverlayResizeEdge_Count> Ids{NullLayoutId, NullLayoutId, NullLayoutId, NullLayoutId};
    const Color *InteractionColor = nullptr; // Whether hovered or pressed
    f32v2 Position;
    f32v2 Size;
    f32 BarWidth = 4.f;
    OverlayResizeFlags Flags = 0;
};

struct ScrollBarInfo
{
    f32 BarOffset = 0.f;
    f32 ElementOffset = 0.f;
    f32 CursorOffset = 0.f;
    f32 WheelOffset = 0.f;

    void Reset()
    {
        BarOffset = 0.f;
        ElementOffset = 0.f;
        CursorOffset = 0.f;
        WheelOffset = 0.f;
    }
};

struct OverlayWindow
{
    OverlayWindow(const LayoutSpecs &spc) : Layout(spc)
    {
    }

    usz Id = NullLayoutId;

    OverlayResizeInfo Resize{};
    ScrollBarInfo ScrollBar{};

    Layout Layout;
    f32v2 Position{0.f};
    f32v2 Size{240.f};
    f32v2 MinSize;
    f32 LastHeight = 240.f;
    CodePoint HeaderIcon;
    OverlayWindowFlags Flags = 0;

    bool CheckFlags(const OverlayWindowFlags flags) const
    {
        return flags & Flags;
    }
    void AddFlags(const OverlayWindowFlags flags)
    {
        Flags |= flags;
    }
    void RemoveFlags(const OverlayWindowFlags flags)
    {
        Flags &= ~flags;
    }
};

enum OverlayColor : u8
{
    OverlayColor_Idle,
    OverlayColor_Hovered,
    OverlayColor_Pressed,
    OverlayColor_Text,
    OverlayColor_Inner,

    OverlayColor_WindowBackgroundExpanded,
    OverlayColor_WindowBackgroundCollapsed,

    OverlayColor_WindowHeaderBackgroundExpanded,
    OverlayColor_WindowHeaderBackgroundCollapsed,

    OverlayColor_ScrollBarIdle,
    OverlayColor_ScrollBarHovered,
    OverlayColor_Count,

    OverlayColor_ScrollBarPressed = OverlayColor_Inner,

    OverlayColor_WindowHeader = OverlayColor_Text,

    OverlayColor_WindowBorderIdle = OverlayColor_Idle,
    OverlayColor_WindowBorderHovered = OverlayColor_Hovered,
    OverlayColor_WindowBorderPressed = OverlayColor_Pressed,

    OverlayColor_ButtonIdle = OverlayColor_Idle,
    OverlayColor_ButtonHovered = OverlayColor_Hovered,
    OverlayColor_ButtonPressed = OverlayColor_Pressed,
    OverlayColor_ButtonText = OverlayColor_Text,

    OverlayColor_CheckBoxIdle = OverlayColor_Idle,
    OverlayColor_CheckBoxHovered = OverlayColor_Hovered,
    OverlayColor_CheckBoxPressed = OverlayColor_Pressed,
    OverlayColor_CheckBoxText = OverlayColor_Text,
    OverlayColor_CheckBoxInner = OverlayColor_Inner,

    OverlayColor_SliderIdle = OverlayColor_Idle,
    OverlayColor_SliderHovered = OverlayColor_Hovered,
    OverlayColor_SliderPressed = OverlayColor_Pressed,
    OverlayColor_SliderText = OverlayColor_Text,
    OverlayColor_SliderInner = OverlayColor_Inner,

    OverlayColor_DragIdle = OverlayColor_Idle,
    OverlayColor_DragHovered = OverlayColor_Hovered,
    OverlayColor_DragPressed = OverlayColor_Pressed,
    OverlayColor_DragText = OverlayColor_Text,
};

struct OverlayColors
{
    Color Idle;
    Color Hovered;
    Color Pressed;
    Color Text;
    Color Inner;

    Color WindowBackgroundExpanded;
    Color WindowBackgroundCollapsed;

    Color WindowHeaderBackgroundExpanded;
    Color WindowHeaderBackgroundCollapsed;

    Color ScrollBarIdle;
    Color ScrollBarHovered;
};

struct OverlayColorRegistry
{
    OverlayColorRegistry()
        : Named{

              .Idle = Color::FromHexadecimal("2D3748"),
              .Hovered = Color::FromHexadecimal("4A5568"),
              .Pressed = Color::FromHexadecimal("5A6A7E"),
              .Text = Color::FromHexadecimal("E2E8F0"),
              .Inner = Color::FromHexadecimal("4A8EC2"),

              .WindowBackgroundExpanded = Color::FromHexadecimal("2A3F5F"),
              .WindowBackgroundCollapsed = Color::FromHexadecimal("1E2D45D9"),

              .WindowHeaderBackgroundExpanded = Color::FromHexadecimal("344E6E"),
              .WindowHeaderBackgroundCollapsed = Color::FromHexadecimal("2A3F5FD9"),

              .ScrollBarIdle = Color::FromHexadecimal("3A4F6F"),
              .ScrollBarHovered = Color::FromHexadecimal("5A7A9E"),
          }
    {
    }

    union {
        Color Flat[OverlayColor_Count];
        OverlayColors Named;
    };

    constexpr const Color &operator[](const u32 idx) const
    {
        return Flat[idx];
    }
};

struct ClickInputInfo
{
    bool Clicked = false;
    bool Pressed = false;
    bool Hovered = false;
};

struct DragInputInfo
{
    bool Pressed = false;
    bool Hovered = false;
};

struct LayoutConfig
{
    f32 FontSize = 14.f;
    f32 WindowPadding = 8.f;
    f32 HeaderPadding = 4.f;
    f32 ScrollBarWidth = 8.f;
    f32 BorderHoverPadding = 8.f;
    f32 ScrollSensitivity = 16.f;
    f32 WidgetWidth = 24.f;
    f32 WindowSpawnDelta = 32.f;
};

struct UserInterfaceSpecs
{
    LayoutSpecs Layout{.RootAlignment = {Alignment_Left, Alignment_Top}};
    OverlayColorRegistry Colors{};
    LayoutConfig Config{};
};

// TODO(Isma): Add ui specs and color array
class UserInterface
{
    TKIT_NON_COPYABLE(UserInterface)

  public:
    UserInterface(Window *win, const UserInterfaceSpecs &specs = {});

    // TODO(Isma): Should return id
    bool BeginWindow(TKit::StringView title, OverlayWindowFlags flags = 0);
    void EndWindow();

    // TODO(Isma): Create unicode overload
    bool Button(TKit::StringView label);
    bool CheckBox(TKit::StringView label, bool *enable);

    template <TKit::Integer T, std::convertible_to<T> U>
    bool CheckBoxFlags(TKit::StringView label, T *flags, const U flag)
    {
        bool enabled = *flags & T(flag);
        if (CheckBox(label, &enabled))
        {
            if (enabled)
                *flags |= T(flag);
            else
                *flags &= ~T(flag);
            return true;
        }
        return false;
    }

    void Text(TKit::StringView text);

    // TODO(Isma): Implement flags
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool Slider(const TKit::StringView label, T *value, const U mn, const U mx)
    {
        TKIT_ASSERT(mn < mx, "[ONYX][UI] Maximum slider value ({}), must be greater than minimum ({})", mx, mn);
        Layout &ly = m_Current->Layout;
        ly.BeginPanel(label, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                   .Sizing = {LayoutSizing::Grow(300.f), LayoutSizing::Fit()},
                                                   .ChildGap = 8.f});

        const LayoutElement *elm = ly.QueryElement("Outer slider");
        const Color *col = &m_Colors[OverlayColor_SliderIdle];
        bool pressed = false;
        if (elm)
        {
            const DragInputInfo info = getDragInputInfo(elm);

            if (info.Pressed)
                col = &m_Colors[OverlayColor_SliderPressed];
            else if (info.Hovered)
                col = &m_Colors[OverlayColor_SliderHovered];
            pressed = info.Pressed;
        }

        const f32 padding = 6.f;
        const f32 length = elm ? elm->Size[0] : 0.f;

        const f32 maxOffset = 0.5f * (length - Config.WidgetWidth) - padding;

        f32 offset = 0.f;
        const f32 normalized = Math::Map(f32(*value), f32(mn), f32(mx), -1.f, 1.f);
        if (pressed)
        {
            const f32 relPos = m_MousePos[0] - elm->Position[0] - 0.5f * length;
            offset = Math::Clamp(relPos, -maxOffset, maxOffset);
            *value = T(Math::Map(offset, -maxOffset, maxOffset, f32(mn), f32(mx)));
            if constexpr (Integer<T>)
                // snapping
                offset = normalized * maxOffset;
        }
        else if (elm)
        {
            *value = Math::Clamp(*value, T(mn), T(mx));
            offset = normalized * maxOffset;
        }

        // heres how this works. outer slider is the first visible bit. then, 2 children
        // come

        ly.BeginPanel("Outer slider",
                      LayoutPanelParameters{.FillColor = *col,
                                            .Alignment = {Alignment_Left, Alignment_Center},
                                            .Sizing = {LayoutSizing::Normalized(0.6f), LayoutSizing::Fit()},
                                            .Padding = padding});

        // the next 2 children will serve as slots for the slider button and the text. this is required bc text length
        // cannot interfere with slider button positioning in layout calculation
        //
        // we actually need both containers to overlap. but bc of layout calculation, slider slot will be placed
        // correctly (just overlapping inner slider) but text slot will be "offscreen" (clipped by outer slider). so, we
        // offset text slot by 1 parent to align it correctly

        ly.BeginPanel("Slider slot",
                      LayoutPanelParameters{.Alignment = Alignment_Center,
                                            .Sizing = {LayoutSizing::Normalized(1.f), LayoutSizing::Fit()}});

        ly.Panel("Slider button",
                 LayoutPanelParameters{.FillColor = m_Colors[OverlayColor_SliderInner],
                                       .Sizing = LayoutSizing::Absolute(Config.WidgetWidth),
                                       .SelfOffset = {LayoutOffset::Absolute(offset), LayoutOffset::Absolute(0.f)}});

        ly.EndPanel();
        ly.BeginPanel("Text slot", LayoutPanelParameters{
                                       .Alignment = Alignment_Center,
                                       .Sizing = {LayoutSizing::Normalized(1.f), LayoutSizing::Fit()},
                                       .SelfOffset = {LayoutOffset::Normalized(-1.f), LayoutOffset::Absolute(0.f)}});

        const TKit::StackString text = [&] {
            // TODO(Isma): Pass formatting as a parameter
            if constexpr (Float<T>)
                return TKit::StackString::Format("{:.3f}", *value);
            else
                return TKit::StackString::Format("{}", *value);
        }();

        ly.Text(text, LayoutTextParameters{.FillColor = m_Colors[OverlayColor_SliderText],
                                           .FontSize = Config.FontSize,
                                           .Offset = m_TextOffset});

        ly.EndPanel();
        ly.EndPanel();
        ly.Text(label, LayoutTextParameters{.FillColor = m_Colors[OverlayColor_SliderText],
                                            .FontSize = Config.FontSize,
                                            .Offset = m_TextOffset});
        ly.EndPanel();
        return pressed;
    }

    // TODO(Isma): Implement flags
    template <TKit::Numeric T, std::convertible_to<T> U = T>
    bool Drag(const TKit::StringView label, T *value, const f32 speed = 1.f, U mn = T(0), U mx = T(0))
    {
        const bool hasLimits = mn < mx;
        Layout &ly = m_Current->Layout;
        ly.BeginPanel(label, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                   .Sizing = {LayoutSizing::Grow(300.f), LayoutSizing::Fit()},
                                                   .ChildGap = 8.f});

        const LayoutElement *elm = ly.QueryElement("Outer drag");
        const Color *col = &m_Colors[OverlayColor_DragIdle];
        bool pressed = false;
        if (elm)
        {
            const DragInputInfo info = getDragInputInfo(elm);

            if (info.Pressed)
                col = &m_Colors[OverlayColor_DragPressed];
            else if (info.Hovered)
                col = &m_Colors[OverlayColor_DragHovered];
            pressed = info.Pressed;
        }

        if (pressed)
        {
            const f32 md = m_MouseDelta[0];
            const T nval = *value + T(speed * md);
            if ((md > 0.f && nval > *value) || (md < 0.f && nval < *value))
                *value = nval;
        }
        if (hasLimits)
            *value = Math::Clamp(*value, T(mn), T(mx));

        const f32 padding = 6.f;
        ly.BeginPanel("Outer drag",
                      LayoutPanelParameters{.FillColor = *col,
                                            .Alignment = Alignment_Center,
                                            .Sizing = {LayoutSizing::Normalized(0.6f), LayoutSizing::Fit()},
                                            .Padding = padding});

        const TKit::StackString text = [&] {
            // TODO(Isma): Pass formatting as a parameter
            if constexpr (Float<T>)
                return TKit::StackString::Format("{:.3f}", *value);
            else
                return TKit::StackString::Format("{}", *value);
        }();

        ly.Text(text, LayoutTextParameters{.FillColor = m_Colors[OverlayColor_DragText], .FontSize = Config.FontSize});

        ly.EndPanel();
        ly.Text(label, LayoutTextParameters{.FillColor = m_Colors[OverlayColor_DragText], .FontSize = Config.FontSize});
        ly.EndPanel();
        return pressed;
    }

    void Draw();

    LayoutConfig Config;

  private:
    // TODO(Isma): Standardize this a bit more. Maybe a prameter struct
    bool collapseButton();
    template <typename F> void iterateReverseWindows(F func);

    f32v2 getMousePos() const;
    void processWindows();
    void bringWindowToTop(u32 idx);
    void drawWindowBorders();
    void drawWindowScrollBar();

    ClickInputInfo getClickInputInfo(const LayoutElement *elm);
    DragInputInfo getDragInputInfo(const LayoutElement *elm);

    // TODO(Isma): Replace with hash map [] operator
    OverlayWindow *getOrCreateOverlayWindow(usz id);
    f32 computeWindowMinSize(f32 winPadding, f32 headPadding, f32 fontSize) const;

    LayoutSpecs m_LayoutSpecs{};
    Window *m_Window;
    RenderView<D2> *m_View;
    Camera<D2> m_Camera;
    RenderContext<D2> *m_Context;

    OverlayWindow *m_Current = nullptr;
    OverlayWindow *m_Grabbed = nullptr;

    f32v2 m_MousePos{0.f};
    f32v2 m_MouseDelta{0.f};
    f32 m_WindowSpawnOffset = 0.f;

    OverlayColorRegistry m_Colors{};

    CodePoint m_ExpandedHeaderIcon = 0x25BC;
    CodePoint m_CollapsedHeaderIcon = 0x25B6;

    vec2<LayoutOffset> m_TextOffset = LayoutOffset::Absolute(f32v2{0.f, 0.f});

    usz m_HoveredClicker = NullLayoutId;
    usz m_HoveredDragger = NullLayoutId;

    usz m_PressedClicker = NullLayoutId;
    usz m_PressedDragger = NullLayoutId;

    // TODO(Isma): Should be a hash map
    TKit::TierArray<OverlayWindow> m_OverlayWindows{};
    TKit::TierArray<usz> m_WindowIds{};
};
} // namespace Onyx
