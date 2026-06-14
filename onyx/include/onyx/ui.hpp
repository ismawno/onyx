#pragma once

#include "onyx/layout.hpp"
#include "onyx/context.hpp"
#include "onyx/window.hpp"
#include "tkit/container/bitset.hpp"

#ifndef ONYX_INPUT_NUMERIC_BUFFER_SIZE
#    define ONYX_INPUT_NUMERIC_BUFFER_SIZE 128
#endif

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
using OverlayEventFlags = u8;
using OverlayWindowFlags = u16;

enum OverlayWindowFlagBit : OverlayWindowFlags
{
    OverlayWindowFlag_NoResize = 1U << 8,
    OverlayWindowFlag_NoMove = 1U << 9,
    OverlayWindowFlag_NoCollapse = 1U << 10,
    OverlayWindowFlag_NoScrollBar = 1U << 11,
    OverlayWindowFlag_NoHeaderBar = 1U << 12,
    OverlayWindowFlag_NoBackground = 1U << 13,
    OverlayWindowFlag_NoBringToFocus = 1U << 14,
    OverlayWindowFlag_AlwaysAutoResize = 1U << 15,
};

using OverlayInputFlags = u8;
enum OverlayInputFlagBit : OverlayInputFlags
{
    OverlayInputFlag_EnterReturnsTrue = 1U << 0,
    OverlayInputFlag_EnterCommitsBuffer = 1U << 1,
    OverlayInputFlag_EscapeClearsAll = 1U << 2,
    OverlayInputFlag_AutoSelectAll = 1U << 3,
    OverlayInputFlag_NoHorizontalScroll = 1U << 4,
    OverlayInputFlag_ElideLeft = 1U << 5,
};

using OverlayHoveredFlags = u8;
enum OverlayHoveredFlagBit : OverlayHoveredFlags
{
    OverlayHoveredFlag_ShortDelay = 1U << 0,
    OverlayHoveredFlag_NormalDelay = 1U << 1,
    OverlayHoveredFlag_Stationary = 1U << 2,
    OverlayHoveredFlag_AllowBlockedByWindow = 1U << 3,
    OverlayHoveredFlag_NoSharedDelay = 1U << 4,
};

using OverlayButtonFlags = u8;
enum OverlayButtonFlagBit : OverlayButtonFlags
{
    OverlayButtonFlag_SpanFullWidth = 1U << 0,
};

using OverlayTreeFlags = u8;
enum OverlayTreeFlagBit : OverlayTreeFlags
{
    OverlayTreeFlag_DrawLines = 1U << 0,
};

using OverlayWidgetStateFlags = u8;
// NOTE(Isma): Could move to .cpp and hide this?
enum OverlayWidgetStateFlagBit : OverlayWidgetStateFlags
{
    OverlayWidgetStateFlag_TreeOpened = 1U << 0,
};

struct OverlayResizeInfo
{
    TKit::FixedArray<usz, OverlayResizeEdge_Count> Ids{NullLayoutId, NullLayoutId, NullLayoutId, NullLayoutId};
    const Color *InteractionColor = nullptr; // Whether hovered or pressed
    f32v2 Position;
    f32v2 Size;
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

struct OverlayTooltip
{
    OverlayTooltip(const LayoutSpecs &spc) : Layout(spc)
    {
    }
    Layout Layout;
    f32v2 Position{0.f};
    bool Drawn = false;
    bool Active = false;
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

    TKit::String TextInput{};
    f32 TextHighlightSize = 0.f;
    f32 TextCursorPos = 0.f;

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

    OverlayColor_TreeIdle,
    OverlayColor_TreeHovered,
    OverlayColor_TreePressed,

    OverlayColor_WindowBackgroundExpanded,
    OverlayColor_WindowBackgroundCollapsed,

    OverlayColor_WindowHeaderBackgroundExpanded,
    OverlayColor_WindowHeaderBackgroundCollapsed,

    OverlayColor_ScrollBarIdle,
    OverlayColor_ScrollBarHovered,
    OverlayColor_Count,

    OverlayColor_ScrollBarPressed = OverlayColor_Inner,

    OverlayColor_TreeText = OverlayColor_Text,

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

    Color TreeIdle;
    Color TreeHovered;
    Color TreePressed;

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

              .TreeIdle = Color_Transparent,
              .TreeHovered = Color::FromHexadecimal("3A4A60"),
              .TreePressed = Color::FromHexadecimal("4A5568"),

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

struct ClickFocusInfo
{
    bool Clicked = false;
    bool Pressed = false;
    bool Hovered = false;
};

struct DragFocusInfo
{
    bool Pressed = false;
    bool Hovered = false;
};

struct InputFocusInfo
{
    bool Focused = false;
    bool EnteredFocus = false;
};

struct InputConvertInfo
{
    bool MustConvert = false;
    bool MustOverrideHighlight = false;
};

struct LayoutConfig
{
    f32 FontSize = 14.f;
    f32 ChildGap = 8.f;

    f32 TooltipOffset = 12.f;
    f32 TooltipPadding = 4.f;

    f32 WindowPadding = 8.f;
    f32 WindowBorderWidth = 4.f;
    f32 WindowSpawnDelta = 32.f;
    f32 HeaderPadding = 4.f;
    f32 HeaderButtonWidth = 20.f;
    f32 BorderHoverPadding = 8.f;
    f32 ContentAreaPadding = 4.f;

    f32 ScrollBarWidth = 8.f;
    f32 ScrollSensitivity = 16.f;
    f32 ScrollMargin = 24.f;

    f32 WidgetSize = 24.f;
    f32 WidgetPadding = 6.f;

    f32 ClickMilliseconds = 200.f;
    f32 CursorWidth = 2.f;

    f32 HoverDelayShort = 0.15f;
    f32 HoverDelayNormal = 0.40f;
    f32 HoverStationaryThreshold = 5.f;

    f32 BoxInputHintAlpha = 0.4f;
    f32 BoxInputCursorAlpha = 0.6f;
    Key BoxInputTrigger = Key_LeftControl;
};

struct UserInterfaceSpecs
{
    LayoutSpecs Layout{.RootAlignment = {Alignment_Left, Alignment_Top}};
    OverlayColorRegistry Colors{};
    LayoutConfig Config{};
};

// TODO(Isma): Implement bitset with pressed keys for control etc. as well for mouse
// TODO(Isma): Have a function, eventKeyPressed/Released(), eventMousePressed() that takes a key/mouse enum and checks
// if it was pressed
// TODO(Isma): Implement little +/- buttons in input numeric (should be easy)
// TODO(Isma): Implement arrow cursor movement with keyboard
// TODO(Isma): Implement a way to push/pop colors
class Overlay
{
    TKIT_NON_COPYABLE(Overlay)

  public:
    Overlay(Window *win, const UserInterfaceSpecs &specs = {});

    // TODO(Isma): Should return id
    bool BeginWindow(TKit::StringView title, OverlayWindowFlags flags = 0);
    void EndWindow();

    // TODO(Isma): Create unicode overload
    bool Button(TKit::StringView label, OverlayButtonFlags flags = 0);
    bool RadioButton(TKit::StringView label, bool active);
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool RadioButton(const TKit::StringView label, T *value, const U reference)
    {
        if (RadioButton(label, *value == T(reference)))
        {
            *value = T(reference);
            return true;
        }
        return false;
    }
    bool CheckBox(TKit::StringView label, bool *enable);

    void BeginTooltip();
    void EndTooltip();
    template <typename... Args> void SetTooltip(const fmt::format_string<Args...> str, Args &&...args)
    {
        BeginTooltip();
        Text(str, std::forward<Args>(args)...);
        EndTooltip();
    }

    bool BeginItemTooltip();
    template <typename... Args> bool SetItemTooltip(const fmt::format_string<Args...> str, Args &&...args)
    {
        if (!BeginItemTooltip())
            return false;

        Text(str, std::forward<Args>(args)...);
        EndTooltip();
        return true;
    }

    // TODO(Isma): Implement hint
    bool InputText(TKit::StringView label, char *buf, u32 size, TKit::StringView hint = {},
                   OverlayInputFlags flags = 0);
    template <TKit::Numeric T>
    bool InputNumeric(const TKit::StringView label, T *value, const char *format, const TKit::StringView hint = {},
                      const OverlayInputFlags flags = 0)
    {
        Layout &ly = getCurrentLayout();
        ly.BeginPanel(label, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                   .Sizing = {grow(300.f), fit()},
                                                   .ChildGap = Config.ChildGap});
        m_LastWidget = ly.BeginPanel("Container", LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                                        .Sizing = {snorm(0.6f), fit()},
                                                                        .ChildGap = Config.ChildGap});

        const bool updated = inputNumericBox(value, format, hint, flags);
        ly.EndPanel();
        ly.Text(label, getTextParams());
        ly.EndPanel();
        return updated;
    }

    bool IsItemHovered(const OverlayHoveredFlags flags = 0);

    void Separator(const f32 width = 4.f)
    {
        const LayoutElement &elm = getCurrentLayout().GetCurrentElement();
        if (elm.Direction == LayoutDirection_LeftToRight || elm.Direction == LayoutDirection_RightToLeft)
            VerticalLine(width);
        else
            HorizontalLine(width);
    }
    void HorizontalSeparator(TKit::StringView label, f32 textOffset = 20.f, f32 width = 4.f);
    void HorizontalLine(const f32 width = 4.f)
    {
        getCurrentLayout().Panel(LayoutPanelParameters{
            .FillColor = m_Colors[OverlayColor_WindowHeaderBackgroundExpanded], .Sizing = {grow(), sabs(width)}});
    }
    void VerticalLine(const f32 width = 4.f)
    {
        getCurrentLayout().Panel(LayoutPanelParameters{
            .FillColor = m_Colors[OverlayColor_WindowHeaderBackgroundExpanded], .Sizing = {sabs(width), grow()}});
    }

    Layout *BeginPanel(const LayoutPanelParameters &params = {})
    {
        Layout &ly = getCurrentLayout();
        ly.BeginPanel(params);
        return &ly;
    }
    void EndPanel()
    {
        getCurrentLayout().EndPanel();
    }
    void Pop()
    {
        EndPanel();
    }

    bool PushTree(LayoutId id, TKit::StringView label, OverlayTreeFlags flags = 0);
    bool PushTree(TKit::StringView label, const OverlayTreeFlags flags = 0)
    {
        return PushTree(label, label, flags);
    }
    template <typename... Args> bool PushTree(const LayoutId id, const fmt::format_string<Args...> str, Args &&...args)
    {
        const TKit::StackString txt = TKit::StackString::Format(str, std::forward<Args>(args)...);
        return PushTree(id, txt);
    }
    void PopTree()
    {
        Pop();
        Pop();
    }

    void PushDirection(const LayoutDirection dir)
    {
        BeginPanel({.Direction = dir,
                    .Alignment = {Alignment_Left, Alignment_Top},
                    .Sizing = fit(),
                    .ChildGap = Config.ChildGap});
    }
    void PushIndent(const f32 indent)
    {
        BeginPanel({.Direction = LayoutDirection_TopToBottom,
                    .Alignment = {Alignment_Left, Alignment_Top},
                    .Sizing = fit(),
                    .ChildOffset = oabs({indent, 0.f}),
                    .ChildGap = Config.ChildGap});
    }

    Layout *GetCurrentLayout()
    {
        return m_Current ? &m_Current->Layout : nullptr;
    }

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

    template <typename... Args> void Text(const fmt::format_string<Args...> str, Args &&...args)
    {
        const TKit::StackString txt = TKit::StackString::Format(str, std::forward<Args>(args)...);
        const LayoutTextParameters params = getTextParams(OverlayColor_Text);
        Layout &ly = getCurrentLayout();
        // a very mid solution to unstable ids when text changes every frame (e.g, printing delta times/performance)
        if constexpr (sizeof...(Args) != 0)
            m_LastWidget = ly.Text(++m_TextId, txt, params);
        else
            m_LastWidget = ly.Text(txt, params);
    }

    // TODO(Isma): Implement format rounding
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool HorizontalSlider(const TKit::StringView label, T *value, const U mn, const U mx, const char *format = nullptr,
                          const u32 count = 1)
    {
        TKIT_ASSERT(mn < mx, "[ONYX][UI] Maximum slider value ({}), must be greater than minimum ({})", mx, mn);
        Layout &ly = getCurrentLayout();
        ly.BeginPanel(label, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                   .Sizing = {grow(300.f), fit()},
                                                   .ChildGap = Config.ChildGap});

        bool pressed = false;
        m_LastWidget = ly.BeginPanel("Container", LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                                        .Sizing = {snorm(0.6f), fit()},
                                                                        .ChildGap = Config.ChildGap});
        for (u32 i = 0; i < count; ++i)
        {
            T &val = value[i];
            ly.PushId(&val);
            pressed |= horizontalSliderBox(&val, mn, mx, format);
            ly.PopId();
        }
        ly.EndPanel();
        ly.Text(label, getTextParams(OverlayColor_SliderText));
        ly.EndPanel();
        return pressed;
    }

    template <TKit::Numeric T, std::convertible_to<T> U = T>
    bool HorizontalDrag(const TKit::StringView label, T *value, const f32 speed = 1.f, const U mn = T(0),
                        const U mx = T(0), const char *format = nullptr, const u32 count = 1)
    {
        Layout &ly = getCurrentLayout();
        ly.BeginPanel(label, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                   .Sizing = {grow(300.f), fit()},
                                                   .ChildGap = Config.ChildGap});

        m_LastWidget = ly.BeginPanel("Container", LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                                        .Sizing = {snorm(0.6f), fit()},
                                                                        .ChildGap = Config.ChildGap});
        bool pressed = false;
        for (u32 i = 0; i < count; ++i)
        {
            T &val = value[i];
            ly.PushId(&val);
            pressed |= horizontalDragBox(&val, speed, mn, mx, format);
            ly.PopId();
        }

        ly.EndPanel();
        ly.Text(label, getTextParams(OverlayColor_DragText));
        ly.EndPanel();
        return pressed;
    }

    void Draw();

    LayoutConfig Config;

  private:
    bool checkFlags(const OverlayWindowFlags flags) const
    {
        return flags & m_EventFlags;
    }
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool horizontalSliderBox(T *value, const U mn, const U mx, const char *format)
    {
        Layout &ly = getCurrentLayout();
        const LayoutElement *elm = ly.QueryElement("Slider box");
        format = getFormat<T>(format);

        const Color *col = &m_Colors[OverlayColor_SliderIdle];
        DragFocusInfo info{};
        if (elm)
        {
            info = getDragFocusInfo(elm);

            if (info.Pressed)
                col = &m_Colors[OverlayColor_SliderPressed];
            else if (info.Hovered)
                col = &m_Colors[OverlayColor_SliderHovered];
            info.Pressed = info.Pressed;
        }

        const InputConvertInfo cinfo = getInputConvertInfo(info.Hovered);
        if (cinfo.MustConvert)
            return inputNumericBox(value, nullptr, nullptr, OverlayInputFlag_AutoSelectAll,
                                   cinfo.MustOverrideHighlight);

        const f32 length = elm ? elm->Size[0] : 0.f;
        const f32 w = 0.5f * Config.WidgetSize;

        const f32 maxOffset = 0.5f * (length - w) - Config.WidgetPadding;

        f32 offset = 0.f;
        const f32 normalized = Math::Map(f32(*value), f32(mn), f32(mx), -1.f, 1.f);
        if (info.Pressed && !m_Window->IsKeyPressed(Config.BoxInputTrigger))
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

        ly.BeginPanel("Slider box", LayoutPanelParameters{.FillColor = *col,
                                                          .Alignment = {Alignment_Left, Alignment_Center},
                                                          .Sizing = {grow(), fit()},
                                                          .Padding = Config.WidgetPadding});

        // the next 2 children will serve as slots for the slider button and the text. this is required bc text
        // length cannot interfere with slider button positioning in layout calculation
        //
        // we actually need both containers to overlap. but bc of layout calculation, slider slot will be placed
        // correctly (just overlapping inner slider) but text slot will be "offscreen" (clipped by outer slider).
        // so, we offset text slot by 1 parent to align it correctly

        ly.BeginPanel("Slider slot",
                      LayoutPanelParameters{.Alignment = Alignment_Center, .Sizing = {snorm(1.f), grow()}});

        ly.Panel("Slider button", LayoutPanelParameters{.FillColor = m_Colors[OverlayColor_SliderInner],
                                                        .Sizing = {sabs(w), grow()},
                                                        .SelfOffset = oabs({offset, 0.f})});

        ly.EndPanel();
        ly.BeginPanel("Text slot", LayoutPanelParameters{.Alignment = Alignment_Center,
                                                         .Sizing = {snorm(1.f), fit()},
                                                         .SelfOffset = onorm({-1.f, 0.f})});

        const TKit::StackString text = TKit::StackString::Format(TKit::RuntimeFormatString(format), *value);
        ly.Text(text, getTextParams(OverlayColor_SliderText));

        ly.EndPanel();
        ly.EndPanel();
        return info.Pressed;
    }
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool horizontalDragBox(T *value, const f32 speed, const U mn, const U mx, const char *format)
    {
        Layout &ly = getCurrentLayout();
        const bool hasLimits = mn < mx;
        format = getFormat<T>(format);

        const LayoutElement *elm = ly.QueryElement("Drag box");
        const Color *col = &m_Colors[OverlayColor_DragIdle];
        DragFocusInfo info{};
        if (elm)
        {
            info = getDragFocusInfo(elm);

            if (info.Pressed)
                col = &m_Colors[OverlayColor_DragPressed];
            else if (info.Hovered)
                col = &m_Colors[OverlayColor_DragHovered];
        }

        const InputConvertInfo cinfo = getInputConvertInfo(info.Hovered, true);
        if (cinfo.MustConvert)
            return inputNumericBox(value, nullptr, nullptr, OverlayInputFlag_AutoSelectAll,
                                   cinfo.MustOverrideHighlight);

        if (info.Pressed)
        {
            const f32 md = m_MouseDelta[0];
            const T nval = *value + T(speed * md);
            if ((md > 0.f && nval > *value) || (md < 0.f && nval < *value))
                *value = nval;
        }
        if (hasLimits)
            *value = Math::Clamp(*value, T(mn), T(mx));

        ly.BeginPanel("Drag box", LayoutPanelParameters{.FillColor = *col,
                                                        .Alignment = Alignment_Center,
                                                        .Sizing = {grow(), fit()},
                                                        .Padding = Config.WidgetPadding});

        const TKit::StackString text = TKit::StackString::Format(TKit::RuntimeFormatString(format), *value);
        ly.Text(text, getTextParams(OverlayColor_DragText));

        ly.EndPanel();
        return info.Pressed;
    }

    bool inputTextBox(char *buf, u32 size, TKit::StringView hint, OverlayInputFlags flags,
                      bool overrideEnterFocus = false);

    template <TKit::Numeric T>
    bool inputNumericBox(T *value, const char *format, const TKit::StringView hint, const OverlayInputFlags flags,
                         const bool overrideEnterFocus = false)
    {
        constexpr u32 bsize = ONYX_INPUT_NUMERIC_BUFFER_SIZE;

        format = getFormat<T>(format);
        TKit::StaticString<bsize> str = TKit::StaticString<bsize>::Format(TKit::RuntimeFormatString(format), *value);
        char *buf = str.CString();

        if (inputTextBox(buf, bsize, hint, flags, overrideEnterFocus))
        {
            char *end;
            if constexpr (TKit::Float<T>)
            {
                const f64 v = std::strtod(buf, &end);
                if (end != buf)
                    *value = T(v);
            }
            else
            {
                const u64 v = u64(std::strtoll(buf, &end, 10));
                if (end != buf)
                    *value = T(v);
            }
            return true;
        }
        return false;
    }

    template <TKit::Numeric T> const char *getFormat(const char *format)
    {
        if constexpr (TKit::Float<T>)
        {
            if (!format)
                format = "{:.3f}";
        }
        else
        {
            if (!format)
                format = "{}";
        }
        return format;
    }

    Layout &getCurrentLayout()
    {
        return m_Tooltip.Active ? m_Tooltip.Layout : m_Current->Layout;
    }

    InputConvertInfo getInputConvertInfo(bool hovered, bool allowDoubleClick = false);

    // TODO(Isma): Standardize this a bit more. Maybe a prameter struct
    bool collapseButton();
    template <typename F> void iterateReverseWindows(F func);

    f32v2 getMousePos() const;
    void processWindows();
    void bringWindowToTop(u32 idx);
    void drawWindowBorders();
    void drawWindowScrollBar();

    OverlayWidgetStateFlags getWidgetState(const usz id)
    {
        const auto it = m_WidgetStates.Find(id);
        return it == m_WidgetStates.end() ? 0 : it->Value;
    }
    bool getWidgetState(const usz id, const OverlayWidgetStateFlags flags)
    {
        return getWidgetState(id) & flags;
    }
    void toggleWidgetState(const usz id, const OverlayWidgetStateFlagBit bit)
    {
        const OverlayWidgetStateFlags flags = getWidgetState(id);
        if (flags & bit)
            m_WidgetStates[id] &= ~bit;
        else
            m_WidgetStates[id] |= bit;
    }

    ClickFocusInfo getClickFocusInfo(const LayoutElement *elm);
    DragFocusInfo getDragFocusInfo(const LayoutElement *elm);
    InputFocusInfo getInputFocusInfo(const LayoutElement *elm);

    // TODO(Isma): Replace with hash map [] operator
    OverlayWindow *getOrCreateOverlayWindow(usz id);
    const FontData &getFontData() const;
    f32 computeWindowMinSize(f32 winPadding, f32 headPadding, f32 fontSize) const;

    LayoutTextParameters getTextParams(const OverlayColor color = OverlayColor_Text) const
    {
        return {.FillColor = m_Colors[color], .FontSize = Config.FontSize};
    }
    LayoutUnicodeParameters getUnicodeParams(const OverlayColor color) const
    {
        return {.FillColor = m_Colors[color], .Size = Config.FontSize};
    }

    static constexpr LayoutSizing fit(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return LayoutSizing::Fit(min, max);
    }
    static constexpr LayoutSizing grow(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return LayoutSizing::Grow(min, max);
    }
    static constexpr LayoutSizing flex(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return LayoutSizing::Flex(min, max);
    }
    static constexpr LayoutSizing sabs(const f32 size)
    {
        return LayoutSizing::Absolute(size);
    }
    static constexpr vec2<LayoutSizing> sabs(const f32v2 &size)
    {
        return LayoutSizing::Absolute(size);
    }
    static constexpr LayoutSizing snorm(const f32 size)
    {
        return LayoutSizing::Normalized(size);
    }
    static constexpr vec2<LayoutSizing> snorm(const f32v2 &size)
    {
        return LayoutSizing::Normalized(size);
    }
    static constexpr LayoutOffset oabs(const f32 size)
    {
        return LayoutOffset::Absolute(size);
    }
    static constexpr vec2<LayoutOffset> oabs(const f32v2 &size)
    {
        return LayoutOffset::Absolute(size);
    }
    static constexpr LayoutOffset onorm(const f32 size)
    {
        return LayoutOffset::Normalized(size);
    }
    static constexpr vec2<LayoutOffset> onorm(const f32v2 &size)
    {
        return LayoutOffset::Normalized(size);
    }

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

    OverlayEventFlags m_EventFlags = 0;

    // interaction info
    usz m_HoveredClicker = NullLayoutId;
    usz m_HoveredDragger = NullLayoutId;

    usz m_PressedClicker = NullLayoutId;
    usz m_DelayedPressedClicker = NullLayoutId;
    usz m_PressedDragger = NullLayoutId;

    usz m_FocusedInputter = NullLayoutId;
    usz m_DelayedFocusedInputter = NullLayoutId;
    //

    usz m_LastWidget = NullLayoutId;
    usz m_HoveredWidgetCandidate = NullLayoutId;
    const Layout *m_CandidateLayout = nullptr;
    TKit::Clock m_WidgetHoverClock{};

    // automatic id for texts
    usz m_TextId = 0;

    // overflow clicks means how many rapid succession clicks have happened without counting the first (aka, == 1 is
    // a double click)
    u32 m_OverflowClicks = 0;

    TKit::StaticBitSet<Key_Count> m_EventKeys{Key_Count};
    TKit::String m_InputWidgetBuffer{};

    TKit::Clock m_ClickClock{};

    // TODO(Isma): Should be a hash map
    TKit::TierArray<OverlayWindow> m_OverlayWindows{};
    TKit::TierArray<usz> m_WindowIds{};
    TKit::TierHashMap<usz, OverlayWidgetStateFlags> m_WidgetStates{};
    OverlayTooltip m_Tooltip;
};
} // namespace Onyx
