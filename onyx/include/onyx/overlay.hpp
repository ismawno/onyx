#pragma once

#include "onyx/layout.hpp"
#include "onyx/context.hpp"
#include "onyx/window.hpp"
#include "tkit/container/bitset.hpp"

// in general, elements beginning with OverlayXXX are to be used by the public

#ifndef ONYX_INPUT_NUMERIC_BUFFER_SIZE
#    define ONYX_INPUT_NUMERIC_BUFFER_SIZE 128
#endif

namespace Onyx
{
enum ResizeEdge : u8
{
    ResizeEdge_Left,
    ResizeEdge_Right,
    ResizeEdge_Bottom,
    ResizeEdge_Top,
    ResizeEdge_Count
};
using ResizeFlags = u8;
using EventFlags = u16;

// NOTE(Isma, 25/06/06): Consider exposing these through a function QueryWidgetState or something like this
using WidgetStateFlags = u8;
enum WidgetStateFlagBit : WidgetStateFlags
{
    WidgetStateFlag_Opened = 1U << 0,
    WidgetStateFlag_ConvertedToInput = 1U << 1,
};

// public focus query flags -> these flags are given to the user/dev by queries
using OverlayFocusQueryFlags = u8;
enum OverlayFocusQueryFlagBit : OverlayFocusQueryFlags
{
    OverlayFocusQueryFlag_Hovered = 1U << 0,
    OverlayFocusQueryFlag_Pressed = 1U << 1,
    OverlayFocusQueryFlag_LeftClicked = 1U << 2,
    OverlayFocusQueryFlag_RightClicked = 1U << 3,
    OverlayFocusQueryFlag_DoubleClicked = 1U << 4,
    OverlayFocusQueryFlag_Active = 1U << 5,
    OverlayFocusQueryFlag_JustActive = 1U << 6,
    OverlayFocusQueryFlag_PopupOpen = 1U << 7,
};

// internal focus flags -> these configure how querying focus behave
using FocusFlags = u32;
enum FocusFlagBit : FocusFlags
{
    FocusFlag_PressedEvenWhenAwayFromHover = 1U << 8,
    FocusFlag_ClickedOnMousePress = 1U << 9,
    FocusFlag_KeepActiveOnPressed = 1U << 10,
    FocusFlag_KeepActiveOnRelease = 1U << 11,
    FocusFlag_SetActiveOnRelease = 1U << 12,
    FocusFlag_ToggleActiveOnRelease = 1U << 13,
    FocusFlag_ActiveAllowsInteraction = 1U << 14,
    FocusFlag_LeftClickOpensPopup = 1U << 15,
    FocusFlag_RightClickOpensPopup = 1U << 16,
    FocusFlag_DoNotSetHoveredId = 1U << 17,
    FocusFlag_DoNotSetPressedId = 1U << 18,
    FocusFlag_DoNotSetActiveId = 1U << 19,
    FocusFlag_DoNotProtectPopup = 1U << 20,
    FocusFlag_ReadOnly = FocusFlag_DoNotSetHoveredId | FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetActiveId |
                         FocusFlag_DoNotProtectPopup
};

// same as above, but public
using OverlayFocusFlags = u32;
enum OverlayFocusFlagBit : OverlayFocusFlags
{
    OverlayFocusFlag_PressedEvenWhenAwayFromHover = FocusFlag_PressedEvenWhenAwayFromHover
};

using InputConvertInfoFlags = u8;
enum InputConvertFlagBit : InputConvertInfoFlags
{
    InputConvertFlag_Hovered = OverlayFocusQueryFlag_Hovered,
    InputConvertFlag_MustConvert = 1U << 1,
    InputConvertFlag_MustOverrideHighlight = 1U << 2,
    InputConvertFlag_AllowDoubleClick = 1U << 3,
};

using OverlayWindowFlags = u16;
enum OverlayWindowFlagBit : OverlayWindowFlags
{
    OverlayWindowFlag_NoScrollBar = 1U << 3,
    OverlayWindowFlag_NoVerticalScroll = 1U << 4,
    OverlayWindowFlag_HorizontalScroll = 1U << 5,
    OverlayWindowFlag_NoResize = 1U << 6,
    OverlayWindowFlag_NoMove = 1U << 7,
    OverlayWindowFlag_NoCollapse = 1U << 8,
    OverlayWindowFlag_NoHeaderBar = 1U << 9,
    OverlayWindowFlag_NoBringToFocus = 1U << 10,
    OverlayWindowFlag_AutoResize = 1U << 11,
    OverlayWindowFlag_BringToTop = 1U << 12,
    OverlayWindowFlag_Modal = 1U << 13,
    WindowInternalFlag_ClosePopupButton = 1U << 14,
};

using OverlayScrollFlags = u16;
enum OverlayScrollFlagBit : OverlayScrollFlags
{
    OverlayScrollFlag_Borders = 1U << 0,
    OverlayScrollFlag_Title = 1U << 1,
    OverlayScrollFlag_NoBackground = 1U << 2,
    OverlayScrollFlag_NoScrollBar = OverlayWindowFlag_NoScrollBar,
    OverlayScrollFlag_NoVerticalScroll = OverlayWindowFlag_NoVerticalScroll,
    OverlayScrollFlag_HorizontalScroll = OverlayWindowFlag_HorizontalScroll,
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

using OverlaySelectableFlags = u8;
enum OverlaySelectableFlagBit : OverlaySelectableFlags
{
    OverlaySelectableFlag_SpanLabelWidth = 1U << 0,
    OverlaySelectableFlag_SelectOnDoubleClick = 1U << 1,
    OverlaySelectableFlag_Highlight = 1U << 2,
};

using OverlayDropDownFlags = u8;
enum OverlayDropDownFlagBit : OverlayDropDownFlags
{
    OverlayDropDownFlag_NoArrowButton = 1U << 0,
    OverlayDropDownFlag_NoPreview = 1U << 1,
    OverlayDropDownFlag_HeightSmall = 1U << 2,
    OverlayDropDownFlag_HeightRegular = 1U << 3,
    OverlayDropDownFlag_HeightLargest = 1U << 4,
    OverlayDropDownFlag_Tight = 1U << 5,
};

using OverlayHoveredFlags = u16;
enum OverlayHoveredFlagBit : OverlayHoveredFlags
{
    OverlayHoveredFlag_AllowBlockedByWindow = 1U << 0,
    OverlayHoveredFlag_AllowBlockedByWindowGrab = 1U << 1,
    OverlayHoveredFlag_AllowBlockedByPressedItem = 1U << 2,
    OverlayHoveredFlag_AllowBlockedByActiveItem = 1U << 3,
    OverlayHoveredFlag_AllowBlockedByPopup = 1U << 4,
    OverlayHoveredFlag_AllowBlockedByPopupCollapse = 1U << 5,
    OverlayHoveredFlag_NoSharedDelay = 1U << 6,
    OverlayHoveredFlag_ShortDelay = 1U << 7,
    OverlayHoveredFlag_NormalDelay = 1U << 8,
    OverlayHoveredFlag_Stationary = 1U << 9,
};

using OverlayHoverQueryFlags = u16;
enum OverlayHoverQueryFlagBit : OverlayHoverQueryFlags
{
    OverlayHoverQueryFlag_BlockedByWindow = OverlayHoveredFlag_AllowBlockedByWindow,
    OverlayHoverQueryFlag_BlockedByWindowGrab = OverlayHoveredFlag_AllowBlockedByWindowGrab,
    OverlayHoverQueryFlag_BlockedByPressedItem = OverlayHoveredFlag_AllowBlockedByPressedItem,
    OverlayHoverQueryFlag_BlockedByActiveItem = OverlayHoveredFlag_AllowBlockedByActiveItem,
    OverlayHoverQueryFlag_BlockedByPopup = OverlayHoveredFlag_AllowBlockedByPopup,
    OverlayHoverQueryFlag_BlockedByPopupCollapse = OverlayHoveredFlag_AllowBlockedByPopupCollapse,
    OverlayHoverQueryFlag_Hovered = 1U << 15,
};

using OverlayButtonFlags = u8;
enum OverlayButtonFlagBit : OverlayButtonFlags
{
    OverlayButtonFlag_SpanFullWidth = 1U << 0,
    OverlayButtonFlag_Small = 1U << 1,
};

using OverlayTreeFlags = u8;
enum OverlayTreeFlagBit : OverlayTreeFlags
{
    OverlayTreeFlag_DrawLines = 1U << 0,
    OverlayTreeFlag_ToggleOnArrow = 1U << 1,
    // TODO(Isma): Make this only on open. Add CloseOnDoubleClick
    OverlayTreeFlag_OpenOnDoubleClick = 1U << 2,
    OverlayTreeFlag_SpanLabelWidth = 1U << 3,
    OverlayTreeFlag_Framed = 1U << 4,
    OverlayTreeFlag_NoIndent = 1U << 5,
    OverlayTreeFlag_StartOpen = 1U << 6,
};

using OverlaySliderFlags = u8;
enum OverlaySliderFlagBit : OverlaySliderFlags
{
    OverlaySliderFlag_ClampOnInput = 1U << 0,
    OverlaySliderFlag_Logarithmic = 1U << 1,
    OverlaySliderFlag_NoRoundToFormat = 1U << 2,
    OverlaySliderFlag_NoInput = 1U << 3,
};

using OverlayPopupFlags = u8;
enum OverlayPopupFlagBit : OverlayPopupFlags
{
    OverlayPopupFlag_LeftClick = OverlayFocusQueryFlag_LeftClicked,
    OverlayPopupFlag_RightClick = OverlayFocusQueryFlag_RightClicked
};

using NextWindowFlags = u8;
enum NextWindowFlagBit : NextWindowFlags
{
    NextWindowFlag_Position = 1U << 0,
    NextWindowFlag_Size = 1U << 1,
};

struct NextWindowData
{
    f32v2 Position;
    f32v2 Size;
    NextWindowFlags Flags = 0;
};

struct ResizeInfo
{
    TKit::FixedArray<usz, ResizeEdge_Count> Ids{NullLayoutId, NullLayoutId, NullLayoutId, NullLayoutId};
    const Color *InteractionColor = nullptr; // Whether hovered or pressed
    f32v2 Position;
    f32v2 Size;
    ResizeFlags Flags = 0;
};

struct ScrollBarInfo
{
    f32 BarOffset = 0.f;
    f32 ElementOffset = 0.f;
    f32 CursorOffset = 0.f;
    f32 WheelOffset = 0.f;
};

struct ScrollInfo
{
    ScrollBarInfo Vertical{};
    ScrollBarInfo Horizontal{};
    OverlayScrollFlags Flags = 0;
};

struct ScrollParameterSpecs
{
    LayoutId Id;
    vec2<LayoutSizing> OuterSizing;
    vec2<LayoutSizing> ContentSizing;
    f32 ContentPadding;
    f32 ChildGap;
    OverlayScrollFlags Flags;
};

struct Tooltip
{
    Tooltip(const LayoutSpecs &spc) : Layout(spc)
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
    u64 Layer;

    ResizeInfo Resize{};

    Layout Layout;
    f32v2 Position{0.f};
    f32v2 Size{240.f};
    f32v2 MinSize;
    f32 LastHeight = 240.f;
    u32 PopupDepth = 0;
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

enum OverlayPaletteType : u8
{
    OverlayPalette_Idle0,
    OverlayPalette_Idle1,
    OverlayPalette_Idle2,

    OverlayPalette_Hovered0,
    OverlayPalette_Hovered1,
    OverlayPalette_Hovered2,
    OverlayPalette_Hovered3,

    OverlayPalette_Pressed0,
    OverlayPalette_Pressed1,
    OverlayPalette_Pressed2,

    OverlayPalette_Text0,
    OverlayPalette_Text1,

    OverlayPalette_Inner0,
    OverlayPalette_Inner1,

    OverlayPalette_Background0,
    OverlayPalette_Background1,
    OverlayPalette_Background2,

    OverlayPalette_Count,
};

enum OverlayColor : u8
{
    OverlayColor_Text,
    OverlayColor_Line,

    OverlayColor_InputText,
    OverlayColor_InputCursor,
    OverlayColor_InputHighlight,

    OverlayColor_WindowBorderIdle,
    OverlayColor_WindowBorderHovered,
    OverlayColor_WindowBorderPressed,

    OverlayColor_Header,

    OverlayColor_ButtonIdle,
    OverlayColor_ButtonHovered,
    OverlayColor_ButtonPressed,
    OverlayColor_ButtonText,

    OverlayColor_CheckBoxIdle,
    OverlayColor_CheckBoxHovered,
    OverlayColor_CheckBoxPressed,
    OverlayColor_CheckBoxText,
    OverlayColor_CheckBoxInner,

    OverlayColor_SliderIdle,
    OverlayColor_SliderHovered,
    OverlayColor_SliderPressed,
    OverlayColor_SliderText,
    OverlayColor_SliderInner,

    OverlayColor_DragIdle,
    OverlayColor_DragHovered,
    OverlayColor_DragPressed,
    OverlayColor_DragText,

    OverlayColor_TreeIdle,
    OverlayColor_TreeHovered,
    OverlayColor_TreePressed,
    OverlayColor_TreeText,

    OverlayColor_DropDownIdle,
    OverlayColor_DropDownHovered,
    OverlayColor_DropDownPressed,
    OverlayColor_DropDownText,
    OverlayColor_DropDownButton,

    OverlayColor_SelectableIdle,
    OverlayColor_SelectableHovered,
    OverlayColor_SelectablePressed,
    OverlayColor_SelectableText,

    OverlayColor_ScrollBarIdle,
    OverlayColor_ScrollBarHovered,
    OverlayColor_ScrollBarPressed,
    OverlayColor_ScrollAreaBorders,

    OverlayColor_PopupBackground,

    OverlayColor_WindowBackgroundExpanded,
    OverlayColor_WindowBackgroundCollapsed,

    OverlayColor_HeaderBackgroundExpanded,
    OverlayColor_HeaderBackgroundCollapsed,

    OverlayColor_Count,
};

enum OverlayStyleType : u8
{
    OverlayStyle_FontSize,
    OverlayStyle_ChildGap,

    OverlayStyle_TooltipOffset,
    OverlayStyle_TooltipPadding,

    OverlayStyle_WindowPadding,
    OverlayStyle_WindowBorderWidth,
    OverlayStyle_WindowSpawnDelta,

    OverlayStyle_HeaderPadding,
    OverlayStyle_IconWidth,

    OverlayStyle_BorderHoverPadding,
    OverlayStyle_ContentAreaPadding,

    OverlayStyle_ScrollBarWidth,
    OverlayStyle_ScrollBarGap,
    OverlayStyle_ScrollSensitivity,

    OverlayStyle_WidgetSize,
    OverlayStyle_WidgetPadding,
    OverlayStyle_WidgetMinimumWidth,
    OverlayStyle_SmallButtonPadding,

    OverlayStyle_TreeLineWidth,

    OverlayStyle_ClickMilliseconds,
    OverlayStyle_CursorWidth,

    OverlayStyle_HoverDelayShort,
    OverlayStyle_HoverDelayNormal,
    OverlayStyle_HoverStationaryThreshold,

    OverlayStyle_DropDownHeightSmall,
    OverlayStyle_DropDownHeightRegular,

    OverlayStyle_InputHintAlpha,
    OverlayStyle_InputCursorAlpha,

    OverlayStyle_Count
};

using OverlayPalette = TKit::FixedArray<Color, OverlayPalette_Count>;
using OverlayColors = TKit::FixedArray<Color, OverlayColor_Count>;
using OverlayStyleVariables = TKit::FixedArray<f32, OverlayStyle_Count>;

OverlayStyleVariables CreateDefaultOverlayVariables();
OverlayPalette CreateSlateOverlayPalette();
OverlayPalette CreateEmberOverlayPalette();

OverlayColors CreateOverlayColorsFromPalette(const OverlayPalette &palette);

struct ColorBackup
{
    Color Old;
    OverlayColor Index;
};

struct StyleBackup
{
    f32 Old;
    OverlayStyleType Index;
};

struct OverlayStyle
{
    OverlayStyleVariables Variables = CreateDefaultOverlayVariables();
    OverlayColors Colors = CreateOverlayColorsFromPalette(CreateEmberOverlayPalette());

    constexpr f32 operator[](const OverlayStyleType idx) const
    {
        return Variables[idx];
    }
    constexpr f32 &operator[](const OverlayStyleType idx)
    {
        return Variables[idx];
    }

    constexpr const Color &operator[](const OverlayColor idx) const
    {
        return Colors[idx];
    }
    constexpr Color &operator[](const OverlayColor idx)
    {
        return Colors[idx];
    }
};

struct UserInterfaceSpecs
{
    LayoutSpecs Layout{.RootAlignment = {Alignment_Left, Alignment_Top}};
    OverlayStyle Style{};
};

// TODO(Isma): Implement little +/- buttons in input numeric (should be easy)
class Overlay
{
    TKIT_NON_COPYABLE(Overlay)

  public:
    Overlay(Window *win, const UserInterfaceSpecs &specs = {});

    void SetNextWindowPosition(const f32v2 &pos)
    {
        m_NextWindow.Position = pos;
        m_NextWindow.Flags |= NextWindowFlag_Position;
    }
    void SetNextWindowSize(const f32v2 &size)
    {
        m_NextWindow.Size = size;
        m_NextWindow.Flags |= NextWindowFlag_Size;
    }
    void SetNextTextId(const LayoutId id)
    {
        m_TextId = id;
    }

    // windows //
    // TODO(Isma): Should return id
    bool BeginWindow(TKit::StringView title, bool *opened, OverlayWindowFlags flags = 0);
    bool BeginWindow(const TKit::StringView title, const OverlayWindowFlags flags = 0)
    {
        return BeginWindow(title, nullptr, flags);
    }
    void EndWindow();

    // /windows //

    // widgets //

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

    bool BeginSelectable(LayoutId id, bool enabled = false, OverlaySelectableFlags flags = 0);
    bool BeginSelectable(LayoutId id, bool *enabled, OverlaySelectableFlags flags = 0);

    bool BeginSelectable(const bool enabled = false, const OverlaySelectableFlags flags = 0)
    {
        return BeginSelectable(getCurrentLayout().GenerateNextId(), enabled, flags);
    }
    bool BeginSelectable(bool *enabled, const OverlaySelectableFlags flags = 0)
    {
        return BeginSelectable(getCurrentLayout().GenerateNextId(), enabled, flags);
    }

    void EndSelectable();

    bool Selectable(TKit::StringView label, bool enabled = false, OverlaySelectableFlags flags = 0);
    bool Selectable(TKit::StringView label, bool *enabled, OverlaySelectableFlags flags = 0);

    // TODO(Isma): Implement hint
    bool InputText(TKit::StringView label, char *buf, u32 size, TKit::StringView hint = {},
                   OverlayInputFlags flags = 0);
    template <TKit::Numeric T>
    bool InputNumeric(const TKit::StringView label, T *value, const char *format = nullptr,
                      const TKit::StringView hint = {}, const OverlayInputFlags flags = 0)
    {
        beginHorizontalWidget(PushId(label));
        const bool updated = inputNumericBox(value, format, hint, flags);
        endHorizontalWidget(label, OverlayColor_InputText);
        PopId();
        return updated;
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

    // TODO(Isma): Implement format rounding
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool HorizontalSlider(const TKit::StringView label, T *value, const U mn, const U mx, const char *format = nullptr,
                          const u32 count = 1, const OverlaySliderFlags flags = 0)
    {
        TKIT_ASSERT(mn < mx, "[ONYX][OVERLAY] Maximum slider value ({}), must be greater than minimum ({})", mx, mn);
        beginHorizontalWidget(PushId(label));
        bool pressed = false;
        for (u32 i = 0; i < count; ++i)
        {
            T &val = value[i];
            PushId(&val);
            pressed |= horizontalSliderBox(&val, mn, mx, format, flags);
            PopId();
        }
        endHorizontalWidget(label, OverlayColor_SliderText);
        PopId();
        return pressed;
    }

    template <TKit::Numeric T, u32 N, std::convertible_to<T> U>
    bool HorizontalSlider(const TKit::StringView label, vec<T, N> *value, const U mn, const U mx,
                          const char *format = nullptr, const OverlaySliderFlags flags = 0)
    {
        return HorizontalSlider(label, Math::AsPointer(*value), mn, mx, format, N, flags);
    }

    template <TKit::Numeric T, std::convertible_to<T> U = T>
    bool HorizontalDrag(const TKit::StringView label, T *value, const f32 speed = 1.f, const U mn = T(0),
                        const U mx = T(0), const char *format = nullptr, const u32 count = 1,
                        const OverlaySliderFlags flags = 0)
    {
        beginHorizontalWidget(PushId(label));

        bool pressed = false;
        for (u32 i = 0; i < count; ++i)
        {
            T &val = value[i];
            PushId(&val);
            pressed |= horizontalDragBox(&val, speed, mn, mx, format, flags);
            PopId();
        }

        endHorizontalWidget(label, OverlayColor_DragText);
        PopId();
        return pressed;
    }

    template <TKit::Numeric T, u32 N, std::convertible_to<T> U = T>
    bool HorizontalDrag(const TKit::StringView label, vec<T, N> *value, const f32 speed = 1.f, const U mn = T(0),
                        const U mx = T(0), const char *format = nullptr, const OverlaySliderFlags flags = 0)
    {
        return HorizontalDrag(label, Math::AsPointer(*value), speed, mn, mx, format, N, flags);
    }

    // /widgets //

    // display //

    void TextRaw(LayoutTextMode mode, TKit::StringView text);
    void TextRaw(const TKit::StringView text)
    {
        TextRaw(TextMode_Unbounded, text);
    }

    void TextIconRaw(CodePoint icon, LayoutTextMode mode, TKit::StringView text);
    void TextIconRaw(const CodePoint icon, const TKit::StringView text)
    {
        TextIconRaw(icon, TextMode_Unbounded, text);
    }

    template <typename... Args>
    void Text(const LayoutTextMode mode, const fmt::format_string<Args...> str, Args &&...args)
    {
        const TKit::StackString txt = TKit::StackString::Format(str, std::forward<Args>(args)...);
        TextRaw(mode, txt);
    }
    template <typename... Args> void Text(const fmt::format_string<Args...> str, Args &&...args)
    {
        Text(TextMode_Unbounded, str, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void TextIcon(const CodePoint icon, const LayoutTextMode mode, const fmt::format_string<Args...> str,
                  Args &&...args)
    {
        const TKit::StackString txt = TKit::StackString::Format(str, std::forward<Args>(args)...);
        TextIconRaw(icon, mode, txt);
    }
    template <typename... Args>
    void TextIcon(const CodePoint icon, const fmt::format_string<Args...> str, Args &&...args)
    {
        TextIcon(icon, TextMode_Unbounded, str, std::forward<Args>(args)...);
    }

    void Image(const Resource texture, const f32v2 &size, const f32v2 &offset = f32v2{0.f},
               const f32v2 &scale = f32v2{1.f})
    {
        getCurrentLayout().Panel(LayoutPanelParameters{.FillColor = Color_White,
                                                       .Sizing = sabs(size),
                                                       .Texture = texture,
                                                       .TexOffset = offset,
                                                       .TexScale = scale});
    }

    // /display //

    // popups

    void OpenPopup(const LayoutId id)
    {
        m_PopupStack.Append(id);
    }
    void CloseCurrentPopup();

    bool BeginPopup(TKit::StringView title, OverlayWindowFlags flags = 0);
    void EndPopup();

    bool BeginPopupContextItem(const TKit::StringView title, const OverlayFocusFlags wflags = 0,
                               const OverlayPopupFlags flags = OverlayPopupFlag_RightClick)
    {
        if (QueryItemFocusStatus() & flags)
            OpenPopup(title);

        return BeginPopup(title, wflags);
    }

    bool BeginDropDown(TKit::StringView label, TKit::StringView preview, OverlayDropDownFlags flags = 0);
    void EndDropDown()
    {
        endScroll();
        PopStyleVar();
        Layout &ly = getCurrentLayout();
        ly.EndPanel();
        ly.EndPanel();
        PopId();
        --m_CurrentPopupDepth;
    }

    template <TKit::Integer T>
    bool DropDown(const TKit::StringView label, T *current, const TKit::Span<const TKit::StringView> elements,
                  const OverlayDropDownFlags flags = 0)
    {
        const T val = *current;
        if (BeginDropDown(label, val < elements.GetSize() ? elements[val] : "", flags | OverlayDropDownFlag_Tight))
        {
            for (u32 i = 0; i < elements.GetSize(); ++i)
                if (Selectable(elements[i], val == i))
                {
                    *current = i;
                    CloseCurrentPopup();
                }
            EndDropDown();
        }
        return val != *current;
    }
    template <TKit::Integer T>
    bool DropDown(const TKit::StringView label, T *current, const TKit::StringView elements,
                  const OverlayDropDownFlags flags = 0)
    {
        const TKit::StackString str{elements.GetData(), elements.GetSize()};
        const TKit::StackArray<TKit::StackString> splits = str.Split("#");

        TKit::StackArray<TKit::StringView> views{};
        views.Reserve(splits.GetSize());
        for (const TKit::StackString &elm : splits)
            views.Append(elm);
        return DropDown(label, current, views, flags);
    }

    // /popups

    // tooltips //
    void BeginTooltip();
    void EndTooltip();
    template <typename... Args> void SetTooltip(const fmt::format_string<Args...> str, Args &&...args)
    {
        BeginTooltip();
        Text(str, std::forward<Args>(args)...);
        EndTooltip();
    }

    bool BeginItemTooltip(OverlayHoveredFlags flags = 0);

    template <typename... Args>
    bool SetItemTooltip(const OverlayHoveredFlags flags, const fmt::format_string<Args...> str, Args &&...args)
    {
        if (!BeginItemTooltip(flags))
            return false;

        Text(str, std::forward<Args>(args)...);
        EndTooltip();
        return true;
    }
    template <typename... Args> bool SetItemTooltip(const fmt::format_string<Args...> str, Args &&...args)
    {
        return SetItemTooltip(0, str, std::forward<Args>(args)...);
    }

    // /tooltips //

    // layout //

    bool BeginScroll(TKit::StringView label, f32 maxHeight, f32 maxWidth = TKIT_F32_MAX, OverlayScrollFlags flags = 0);
    void EndScroll()
    {
        PopId();
        EndPanel();
        endScroll();
    }

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
        getCurrentLayout().Panel(
            LayoutPanelParameters{.FillColor = m_Style[OverlayColor_Line], .Sizing = {grow(), sabs(width)}});
    }
    void VerticalLine(const f32 width = 4.f)
    {
        getCurrentLayout().Panel(
            LayoutPanelParameters{.FillColor = m_Style[OverlayColor_Line], .Sizing = {sabs(width), grow()}});
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
        Layout &ly = getCurrentLayout();
        ly.EndPanel();
        ly.EndPanel();
        PopId();
    }

    void PushDirection(const LayoutDirection dir, const f32 childGap)
    {
        BeginPanel(
            {.Direction = dir, .Alignment = {Alignment_Left, Alignment_Top}, .Sizing = fit(), .ChildGap = childGap});
    }
    void PushDirection(const LayoutDirection dir)
    {
        PushDirection(dir, m_Style[OverlayStyle_ChildGap]);
    }
    void PopDirection()
    {
        EndPanel();
    }
    void PushIndent(const f32 indent)
    {
        BeginPanel({.Direction = LayoutDirection_TopToBottom,
                    .Alignment = {Alignment_Left, Alignment_Top},
                    .Sizing = fit(),
                    .ChildOffset = oabs({indent, 0.f}),
                    .ChildGap = m_Style[OverlayStyle_ChildGap]});
    }
    void PopIndent()
    {
        EndPanel();
    }

    Layout *GetCurrentLayout()
    {
        return m_Current ? &m_Current->Layout : nullptr;
    }

    LayoutId AsStackedId(LayoutId id)
    {
        if (m_IdStack.IsEmpty())
            return id;
        id.Id = TKit::Hash(m_IdStack.GetBack(), id.Id);
        return id;
    }

    LayoutId PushId(const LayoutId id)
    {
        return m_IdStack.Append(AsStackedId(id));
    }
    void PopId()
    {
        m_IdStack.Pop();
    }

    // /layout //

    // style //
    void PushStyleVar(const OverlayStyleType var, const f32 val)
    {
        m_StyleStack.Append(m_Style[var], var);
        m_Style[var] = val;
    }

    void PopStyleVar()
    {
        const StyleBackup &b = m_StyleStack.GetBack();
        m_Style[b.Index] = b.Old;
    }

    void PushStyleColor(const OverlayColor color, const Color &col)
    {
        m_ColorStack.Append(m_Style[color], color);
        m_Style[color] = col;
    }

    void PopStyleColor()
    {
        const ColorBackup &b = m_ColorStack.GetBack();
        m_Style[b.Index] = b.Old;
    }

    // /style //

    // query //
    OverlayHoverQueryFlags QueryItemHoverStatus() const
    {
        return queryHoverStatus(getCurrentLayout().QueryElement(m_LastItem));
    }
    OverlayFocusQueryFlags QueryItemFocusStatus(const OverlayFocusFlags flags = 0)
    {
        return queryAndSetFocusStatus(getCurrentLayout().QueryElement(m_LastItem), flags | FocusFlag_ReadOnly);
    }
    bool IsItemHovered(const OverlayHoveredFlags flags = 0)
    {
        return isElementHovered(getCurrentLayout().QueryElement(m_LastItem), flags);
    }
    bool IsItemPressed(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_Pressed;
    }
    bool IsItemLeftClicked(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_LeftClicked;
    }
    bool IsItemRightClicked(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_RightClicked;
    }
    bool IsItemDoubleClicked(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_DoubleClicked;
    }
    bool IsItemActive(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_Active;
    }
    bool IsItemJustActive(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_JustActive;
    }
    bool HasItemAnOpenPopup(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_PopupOpen;
    }
    bool IsItemOpened() const
    {
        const auto it = m_WidgetStates.Find(m_LastItem);
        return it != m_WidgetStates.end() && (it->Value & WidgetStateFlag_Opened);
    }
    // /query //

    void Draw();

    // NOTE(Isma): Consider adding const getters?
    Camera<D2> &GetCamera()
    {
        return m_Camera;
    }
    Window *GetWindow()
    {
        return m_Window;
    }
    RenderContext<D2> *GetContext()
    {
        return m_Context;
    }
    RenderViewFlags GetRenderViewFlags() const
    {
        return RenderViewFlag_NormalizedCoordinates | RenderViewFlag_Transparency;
    }

  private:
    bool checkFlags(const OverlayWindowFlags flags) const
    {
        return flags & m_EventFlags;
    }

    static TKit::StringView trimLabel(TKit::StringView label);

    bool beginScroll(const ScrollParameterSpecs &specs);
    void endScroll();

    void beginHorizontalWidget(usz id, f32 normSize = 0.7f);
    void endHorizontalWidget(TKit::StringView label, OverlayColor labelColor);

    template <TKit::Numeric T, std::convertible_to<T> U>
    bool horizontalSliderBox(T *value, const U mn, const U mx, const char *format, const OverlaySliderFlags flags)
    {
        Layout &ly = getCurrentLayout();
        const LayoutId id = AsStackedId("Drag/Slider box");

        const LayoutElement *elm = ly.QueryElement(id);

        const T pval = *value;
        if (!(flags & OverlaySliderFlag_NoInput))
        {
            const InputConvertInfoFlags cflags =
                mustConvertToInputBox(isElementHovered(elm) ? OverlayFocusQueryFlag_Hovered : 0);

            if (cflags & InputConvertFlag_MustConvert)
            {
                inputNumericBox(value, nullptr, nullptr, OverlayInputFlag_AutoSelectAll, cflags);
                if (flags & OverlaySliderFlag_ClampOnInput)
                    *value = Math::Clamp(*value, T(mn), T(mx));
                return *value != pval;
            }
        }

        const T clamped = Math::Clamp(pval, T(mn), T(mx));
        format = getFormat<T>(format);

        const Color *col = &m_Style[OverlayColor_SliderIdle];
        const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_PressedEvenWhenAwayFromHover);

        if (focusFlags & OverlayFocusQueryFlag_Pressed)
            col = &m_Style[OverlayColor_SliderPressed];
        else if (focusFlags & OverlayFocusQueryFlag_Hovered)
            col = &m_Style[OverlayColor_SliderHovered];

        const f32 length = elm ? elm->Size[0] : 0.f;

        f32 baseWidth = 0.f;
        if constexpr (TKit::Integer<T>)
            baseWidth = length / f32(mx - mn + U(1));

        const f32 padding = m_Style[OverlayStyle_WidgetPadding];
        const f32 innerWidth = Math::Max(baseWidth, 0.5f * m_Style[OverlayStyle_WidgetSize]);
        const f32 maxOffset = 0.5f * (length - innerWidth) - padding;
        const bool log = flags & OverlaySliderFlag_Logarithmic;

        const auto map = [log](const f32 v, const f32 mn0, const f32 mx0, const f32 mn1, const f32 mx1) {
            return log ? Math::LogMap(v, mn0, mx0, mn1, mx1) : Math::LinearMap(v, mn0, mx0, mn1, mx1);
        };
        const auto imap = [log](const f32 v, const f32 mn0, const f32 mx0, const f32 mn1, const f32 mx1) {
            return log ? Math::InverseLogMap(v, mn0, mx0, mn1, mx1) : Math::LinearMap(v, mn0, mx0, mn1, mx1);
        };

        f32 offset = 0.f;
        const f32 normalized = imap(f32(clamped), f32(mn), f32(mx), -1.f, 1.f);
        if ((focusFlags & OverlayFocusQueryFlag_Pressed) && !m_Window->IsKeyPressed(Key_LeftControl))
        {
            f32 relPos = m_MousePos[0] - elm->Position[0] - 0.5f * length;
            if constexpr (TKit::Integer<T>)
                relPos += 0.5f * innerWidth;

            offset = Math::Clamp(relPos, -maxOffset, maxOffset);
            *value = T(map(offset, -maxOffset, maxOffset, f32(mn), f32(mx)));

            if (!(flags & OverlaySliderFlag_NoRoundToFormat))
                *value = roundToFormat(*value, format);

            if constexpr (TKit::Integer<T>)
                offset = normalized * maxOffset;
        }
        else
            offset = normalized * maxOffset;

        // heres how this works. outer slider is the first visible bit. then, 2 children
        // come

        ly.BeginPanel(id, LayoutPanelParameters{.FillColor = *col,
                                                .Alignment = {Alignment_Left, Alignment_Center},
                                                .Sizing = {grow(), fit()},
                                                .Padding = padding});

        // the next 2 children will serve as slots for the slider button and the text. this is required bc text
        // length cannot interfere with slider button positioning in layout calculation
        //
        // we actually need both containers to overlap. but bc of layout calculation, slider slot will be placed
        // correctly (just overlapping inner slider) but text slot will be "offscreen" (clipped by outer slider).
        // so, we offset text slot by 1 parent to align it correctly

        ly.BeginPanel(AsStackedId("Slider slot"),
                      LayoutPanelParameters{.Alignment = Alignment_Center, .Sizing = {snorm(1.f), grow()}});

        ly.Panel(AsStackedId("Slider button"), LayoutPanelParameters{.FillColor = m_Style[OverlayColor_SliderInner],
                                                                     .Sizing = {sabs(innerWidth), grow()},
                                                                     .SelfOffset = oabs({offset, 0.f})});

        ly.EndPanel();
        ly.BeginPanel(AsStackedId("Text slot"), LayoutPanelParameters{.Alignment = Alignment_Center,
                                                                      .Sizing = {snorm(1.f), fit()},
                                                                      .SelfOffset = onorm({-1.f, 0.f})});

        const TKit::StackString text = TKit::StackString::Format(TKit::RuntimeFormatString(format), *value);
        ly.Text(ly.GenerateNextId(), text, getTextParams(OverlayColor_SliderText));

        ly.EndPanel();
        ly.EndPanel();
        return *value != pval;
    }
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool horizontalDragBox(T *value, const f32 speed, const U mn, const U mx, const char *format,
                           const OverlaySliderFlags flags)
    {
        Layout &ly = getCurrentLayout();
        const LayoutId id = AsStackedId("Drag/Slider box");

        const LayoutElement *elm = ly.QueryElement(id);
        const T pval = *value;
        if (!(flags & OverlaySliderFlag_NoInput))
        {
            const InputConvertInfoFlags cflags = mustConvertToInputBox(
                (isElementHovered(elm) ? OverlayFocusQueryFlag_Hovered : 0) | InputConvertFlag_AllowDoubleClick);

            if (cflags & InputConvertFlag_MustConvert)
            {
                inputNumericBox(value, nullptr, nullptr, OverlayInputFlag_AutoSelectAll, cflags);
                if (flags & OverlaySliderFlag_ClampOnInput)
                    *value = Math::Clamp(*value, T(mn), T(mx));
                return *value != pval;
            }
        }

        const bool hasLimits = mn < mx;
        format = getFormat<T>(format);

        const Color *col = &m_Style[OverlayColor_DragIdle];
        const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_PressedEvenWhenAwayFromHover);

        if (focusFlags & OverlayFocusQueryFlag_Pressed)
            col = &m_Style[OverlayColor_SliderPressed];
        else if (focusFlags & OverlayFocusQueryFlag_Hovered)
            col = &m_Style[OverlayColor_SliderHovered];

        if (focusFlags & OverlayFocusQueryFlag_JustActive)
            m_DragValue = f64(*value);

        if (focusFlags & OverlayFocusQueryFlag_Pressed)
        {
            const u32 decimals = getFormatDecimals(format);
            const bool log = flags & OverlaySliderFlag_Logarithmic;

            const f32 drag = m_MousePos[0] - m_MousePosOnPress[0];
            const f32 effectiveSpeed =
                log ? (speed * Math::Max(Math::Absolute(f32(m_DragValue + drag)),
                                         decimals == 0 ? 1e-4f : Math::Power(10.f, -f32(decimals))))
                    : speed;

            f64 nval = m_DragValue + f64(effectiveSpeed * drag);
            if (hasLimits)
                nval = Math::Clamp(nval, f64(mn), f64(mx));

            *value = roundToFormat(T(nval), decimals);
        }

        ly.BeginPanel(id, LayoutPanelParameters{.FillColor = *col,
                                                .Alignment = Alignment_Center,
                                                .Sizing = {flex(), fit()},
                                                .Padding = m_Style[OverlayStyle_WidgetPadding]});

        const TKit::StackString text = TKit::StackString::Format(TKit::RuntimeFormatString(format), *value);
        ly.Text(ly.GenerateNextId(), text, getTextParams(OverlayColor_DragText));

        ly.EndPanel();
        return *value != pval;
    }

    bool inputTextBox(char *buf, u32 size, TKit::StringView hint, OverlayInputFlags flags,
                      InputConvertInfoFlags cflags = 0);

    template <TKit::Numeric T>
    bool inputNumericBox(T *value, const char *format, const TKit::StringView hint, const OverlayInputFlags flags,
                         const InputConvertInfoFlags cflags = 0)
    {
        constexpr u32 bsize = ONYX_INPUT_NUMERIC_BUFFER_SIZE;

        format = getFormat<T>(format);
        TKit::StaticString<bsize> str = TKit::StaticString<bsize>::Format(TKit::RuntimeFormatString(format), *value);
        char *buf = str.CString();

        if (inputTextBox(buf, bsize, hint, flags, cflags))
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

    template <TKit::Numeric T> static const char *getFormat(const char *format)
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
    static u32 getFormatDecimals(const char *format);
    template <TKit::Numeric T> static T roundToFormat(const T value, const u32 decimals)
    {
        if constexpr (TKit::Integer<T>)
            return value;
        else
            return decimals == 0 ? value : Math::Round(value, decimals);
    }
    template <TKit::Numeric T> static T roundToFormat(const T value, const char *format)
    {
        return roundToFormat(value, getFormatDecimals(format));
    }

    Layout &getCurrentLayout()
    {
        return m_Tooltip.Active ? m_Tooltip.Layout : m_Current->Layout;
    }
    const Layout &getCurrentLayout() const
    {
        return m_Tooltip.Active ? m_Tooltip.Layout : m_Current->Layout;
    }

    bool headerButton(LayoutId id, CodePoint code);
    template <typename F> void iterateReverseWindows(F func);

    f32v2 getMousePos() const;
    f32v2 computeMouseAlignedPosition(const f32v2 &size) const;
    u32 processWindows();

    void drawWindowBorders();
    void performScroll(LayoutId contentAreaId, ScrollBarInfo &sinfo, LayoutAxis axis, f32 contentPadding, bool drawBar);
    bool addActiveWindow(OverlayWindow *win);
    u64 toTop()
    {
        return m_LayerCount++;
    }

    OverlayHoverQueryFlags queryHoverStatus(const LayoutElement *elm) const;
    bool isElementHovered(const OverlayHoverQueryFlags qflags, const OverlayHoveredFlags flags = 0)
    {
        return (qflags & ~flags) == OverlayHoverQueryFlag_Hovered;
    }
    bool isElementHovered(const LayoutElement *elm, OverlayHoveredFlags flags = 0);

    WidgetStateFlags getWidgetState(const usz id, const WidgetStateFlags fallback = 0)
    {
        return m_WidgetStates.TryInsert(id, fallback);
    }
    bool checkWidgetState(const usz id, const WidgetStateFlags flags, const WidgetStateFlags fallback = 0)
    {
        return getWidgetState(id, fallback) & flags;
    }
    void toggleWidgetState(const usz id, const WidgetStateFlags bit)
    {
        const WidgetStateFlags flags = getWidgetState(id);
        if (flags & bit)
            m_WidgetStates[id] &= ~bit;
        else
            m_WidgetStates[id] |= bit;
    }

    OverlayFocusQueryFlags queryAndSetFocusStatus(const LayoutElement *elm, FocusFlags flags = 0);
    InputConvertInfoFlags mustConvertToInputBox(InputConvertInfoFlags flags = 0);

    // TODO(Isma): Replace with hash map [] operator
    OverlayWindow *getOrCreateOverlayWindow(LayoutId id);
    const FontData &getFontData() const;
    f32 computeWindowMinSize() const;

    LayoutTextParameters getTextParams(const OverlayColor color) const
    {
        return {.FillColor = m_Style[color], .FontSize = m_Style[OverlayStyle_FontSize]};
    }
    LayoutUnicodeParameters getUnicodeParams(const OverlayColor color) const
    {
        return {.FillColor = m_Style[color], .Size = m_Style[OverlayStyle_FontSize]};
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
    f32v2 m_MousePosOnPress{0.f};
    f32v2 m_MouseDelta{0.f};
    f32 m_WindowSpawnOffset = 0.f;

    OverlayStyle m_Style{};
    TKit::TierArray<ColorBackup> m_ColorStack{};
    TKit::TierArray<StyleBackup> m_StyleStack{};

    EventFlags m_EventFlags = 0;

    // interaction info
    usz m_HoveredId = NullLayoutId;
    usz m_PressedId = NullLayoutId;
    usz m_ActiveId = NullLayoutId;
    usz m_ActiveIdLastFrame = NullLayoutId;

    TKit::TierArray<usz> m_ScrollStack{};

    TKit::TierArray<usz> m_PopupStack{};
    // TKit::TierArray<
    u32 m_CurrentPopupDepth = 0;
    u32 m_PopupCollapseDepth = 0;
    // so that modals only collapse manually
    u32 m_ModalCollapseDepth = 0;

    f64 m_DragValue = 0.;

    // required bc immediate queries to the window cause widgets to see the mouse pressed before the actual mouse
    // pressed event. this is important for elements that if they are active think they are currently pressed,
    // causing the firs mouse click outside their bounding box to still qualify as pressed
    bool m_PressingLeftMouse = false;
    //

    // text input info
    TKit::String m_TextInput{};
    u32 m_CursorStart = 0;
    u32 m_CursorEnd = 0;
    //

    usz m_LastItem = NullLayoutId;
    usz m_HoveredWidgetCandidate = NullLayoutId;
    const Layout *m_CandidateLayout = nullptr;
    TKit::Clock m_WidgetHoverClock{};

    // overflow clicks means how many rapid succession clicks have happened without counting the first (aka, == 1 is
    // a double click)
    u32 m_OverflowClicks = 0;

    TKit::StaticBitSet<Key_Count> m_EventKeys{Key_Count};
    TKit::String m_InputWidgetBuffer{};

    TKit::Clock m_ClickClock{};

    // NOTE(Isma, 25/06/06): Could be a hash map, but assuming the window count will be small enough that a linear
    // search is overall better

    // NOTE(Isma, 25/06/06): Applying a hard cap right now because we use direct pointer references to array elements,
    // and so we just avoid stale references on resizes. I dont really expect more than a handful of these at the same
    // time, so 256 should be plenty
    TKit::StaticArray256<OverlayWindow> m_OverlayWindows{};
    TKit::TierArray<OverlayWindow *> m_ActiveWindows{};
    TKit::TierArray<OverlayWindow *> m_WindowStack{};

    TKit::TierArray<usz> m_WindowIds{};
    TKit::TierArray<usz> m_IdStack{};
    TKit::TierHashMap<usz, WidgetStateFlags> m_WidgetStates{};
    TKit::TierHashMap<usz, ScrollInfo> m_Scrollables{};

    NextWindowData m_NextWindow{};
    LayoutId m_TextId = NullLayoutId;

    u64 m_LayerCount = 0;

    Tooltip m_Tooltip;
};
} // namespace Onyx
