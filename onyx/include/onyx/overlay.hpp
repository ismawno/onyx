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
/////////////////////////////////////////////
/// FLAG DEFINITIONS
/////////////////////////////////////////////

using OverlayButtonFlags = u8;
using OverlayColorFlags = u16;
using OverlayDragDropFlags = u8;
using OverlayDropDownFlags = u8;
using OverlayFlags = u8;
using OverlayFocusQueryFlags = u16;
using OverlayFocusFlags = u32;
using OverlayHoveredFlags = u16;
using OverlayHoverQueryFlags = OverlayHoveredFlags;
using OverlayInputFlags = u8;
using OverlayPopupFlags = OverlayFocusQueryFlags;
using OverlayScrollFlags = u32;
using OverlaySelectableFlags = u8;
using OverlaySliderFlags = u8;
using OverlayTabBarFlags = u8;
using OverlayTabFlags = u8;
using OverlayTreeFlags = u8;
using OverlayTooltipFlags = u8;
using OverlayWindowFlags = OverlayScrollFlags;

using FocusFlags = OverlayFocusFlags;
using InputConvertInfoFlags = u8;
using NativeWindowFlags = u16;
using NextWindowFlags = u8;
using ResizeFlags = u8;
using StateFlags = u32;
using WidgetStateFlags = u8;

/////////////////////////////////////////////
/// END FLAG DEFINITIONS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// GENERAL
/////////////////////////////////////////////

enum OverlayFlagBit : OverlayFlags
{
    OverlayFlag_WindowPromotions = 1U << 0,

    // internal
    OverlayFlag_FloatingMode = 1U << 1,
};

/////////////////////////////////////////////
/// END GENERAL
/////////////////////////////////////////////

/////////////////////////////////////////////
/// STYLING
/////////////////////////////////////////////

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

    OverlayPalette_Inner0,
    OverlayPalette_Inner1,

    OverlayPalette_Background0,
    OverlayPalette_Background1,
    OverlayPalette_Background2,

    OverlayPalette_Count,
};

enum OverlayColor : u8
{
    OverlayColor_None,
    OverlayColor_Text,
    OverlayColor_Line,

    OverlayColor_DragOutline,

    OverlayColor_InputCursor,
    OverlayColor_InputHighlight,
    OverlayColor_InputBackground,

    OverlayColor_WindowBorderIdle,
    OverlayColor_WindowBorderHovered,
    OverlayColor_WindowBorderPressed,

    OverlayColor_Header,

    OverlayColor_ButtonIdle,
    OverlayColor_ButtonHovered,
    OverlayColor_ButtonPressed,

    OverlayColor_CheckBoxIdle,
    OverlayColor_CheckBoxHovered,
    OverlayColor_CheckBoxPressed,
    OverlayColor_CheckBoxInner,

    OverlayColor_SliderIdle,
    OverlayColor_SliderHovered,
    OverlayColor_SliderPressed,
    OverlayColor_SliderInner,

    OverlayColor_DragIdle,
    OverlayColor_DragHovered,
    OverlayColor_DragPressed,

    OverlayColor_TreeIdle,
    OverlayColor_TreeHovered,
    OverlayColor_TreePressed,

    OverlayColor_DropDownIdle,
    OverlayColor_DropDownHovered,
    OverlayColor_DropDownPressed,
    OverlayColor_DropDownButton,

    OverlayColor_SelectableIdle,
    OverlayColor_SelectableHovered,
    OverlayColor_SelectablePressed,

    OverlayColor_MenuItemIdle,
    OverlayColor_MenuItemHovered,
    OverlayColor_MenuItemPressed,
    OverlayColor_MenuBoxBackground,

    OverlayColor_ScrollBarIdle,
    OverlayColor_ScrollBarHovered,
    OverlayColor_ScrollBarPressed,
    OverlayColor_ScrollAreaBorders,

    OverlayColor_ProgressBarBackground,
    OverlayColor_ProgressBarInner,

    OverlayColor_PopupBackground,

    OverlayColor_WindowBackgroundExpanded,
    OverlayColor_WindowBackgroundCollapsed,

    OverlayColor_HeaderBackgroundExpanded,
    OverlayColor_HeaderBackgroundCollapsed,

    OverlayColor_MenuBarBackground,

    OverlayColor_Count,
};

enum OverlayStyleVariable : u8
{
    OverlayStyle_FontSize,
    OverlayStyle_ChildGap,

    OverlayStyle_HeaderRadius,
    OverlayStyle_MenuBarRadius,

    OverlayStyle_DropDownRadius,
    OverlayStyle_DropDownPopupRadius,

    OverlayStyle_DragThreshold,
    OverlayStyle_DragOutlineWidth,

    OverlayStyle_ScrollAreaBorderRadius,
    OverlayStyle_TreeRadius,
    OverlayStyle_InputBoxRadius,
    OverlayStyle_ButtonRadius,
    OverlayStyle_CheckBoxRadius,
    OverlayStyle_SelectableRadius,
    OverlayStyle_SelectableCheckBoxRadius,
    OverlayStyle_TooltipRadius,
    OverlayStyle_ImageRadius,
    OverlayStyle_TabRadius,

    OverlayStyle_TabPadding,
    OverlayStyle_TabGap,

    OverlayStyle_LineRadius,
    OverlayStyle_LineWidth,
    OverlayStyle_SeparatorTextOffset,

    OverlayStyle_SliderRadius,
    OverlayStyle_SliderInnerRadius,

    OverlayStyle_VerticalSliderWidth,
    OverlayStyle_VerticalSliderHeight,

    OverlayStyle_Alpha,
    OverlayStyle_DisabledAlpha,

    OverlayStyle_ListBoxMaxHeight,

    OverlayStyle_TooltipOffset,
    OverlayStyle_TooltipPadding,

    OverlayStyle_MainMenuBarPadding,
    OverlayStyle_MinimumMenuWidth,

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

    OverlayStyle_MenuPadding,

    OverlayStyle_TreeLineWidth,

    OverlayStyle_ClickMilliseconds,
    OverlayStyle_CursorWidth,

    OverlayStyle_HoverDelayShort,
    OverlayStyle_HoverDelayNormal,
    OverlayStyle_HoverStationaryThreshold,

    OverlayStyle_DropDownHeightSmall,
    OverlayStyle_DropDownHeightRegular,

    OverlayStyle_HintOpacity,
    OverlayStyle_CursorOpacity,

    OverlayStyle_ColorPreviewSize,
    OverlayStyle_ColorTooltipSize,
    OverlayStyle_ColorDragTooltipSize,

    OverlayStyle_ColorPickerSize,
    OverlayStyle_ColorPickerPreviewSize,
    OverlayStyle_ColorPickerTooltipSize,

    OverlayStyle_Count,

    OverlayStyle_DragRadius = OverlayStyle_SliderRadius,
    OverlayStyle_VerticalDragWidth = OverlayStyle_VerticalSliderWidth,
    OverlayStyle_VerticalDragHeight = OverlayStyle_VerticalSliderHeight,
};

using OverlayPalette = TKit::FixedArray<Color, OverlayPalette_Count>;
using OverlayColors = TKit::FixedArray<Color, OverlayColor_Count>;
using OverlayStyleVariables = TKit::FixedArray<f32, OverlayStyle_Count>;

OverlayStyleVariables CreateDefaultOverlayVariables();
OverlayPalette CreateSlateOverlayPalette();
OverlayPalette CreateEmberOverlayPalette();
OverlayPalette CreateBabyBlueOverlayPalette();

OverlayColors CreateOverlayColorsFromPalette(const OverlayPalette &palette);

struct ColorBackup
{
    Color Old;
    OverlayColor Index;
};

struct StyleBackup
{
    f32 Old;
    OverlayStyleVariable Index;
};

struct OverlayStyle
{
    OverlayStyleVariables Variables = CreateDefaultOverlayVariables();
    OverlayColors Colors = CreateOverlayColorsFromPalette(CreateBabyBlueOverlayPalette());

    constexpr f32 operator[](const OverlayStyleVariable idx) const
    {
        return Variables[idx];
    }

    Color operator[](const OverlayColor idx) const
    {
        const Color &col = Colors[idx];
        return Color{col, col.rgba[3] * Variables[OverlayStyle_Alpha]};
    }
};

struct OverlayColorHandle
{
    OverlayColorHandle() = default;
    OverlayColorHandle(f32 *data) : Data(data)
    {
    }
    OverlayColorHandle(f32v3 *data) : Data(&data->At(0))
    {
#ifdef TKIT_ENABLE_ENSURE
        Size = 3;
#endif
    }
    OverlayColorHandle(f32v4 *data) : Data(&data->At(0))
    {
    }
    OverlayColorHandle(Color *data) : Data(&data->rgba.At(0))
    {
    }
    f32 *Data;
#ifdef TKIT_ENABLE_ENSURE
    u32 Size = 4;
#endif
};

/////////////////////////////////////////////
/// END STYLING
/////////////////////////////////////////////

/////////////////////////////////////////////
/// WINDOWS
/////////////////////////////////////////////

enum ResizeEdge : u8
{
    ResizeEdge_Left,
    ResizeEdge_Right,
    ResizeEdge_Bottom,
    ResizeEdge_Top,
    ResizeEdge_Count
};

enum NextWindowFlagBit : NextWindowFlags
{
    NextWindowFlag_Position = 1U << 0,
    NextWindowFlag_Size = 1U << 1,
};

enum OverlayWindowFlagBit : OverlayWindowFlags
{
    OverlayWindowFlag_NoScrollBar = 1U << 8,
    OverlayWindowFlag_NoVerticalScroll = 1U << 9,
    OverlayWindowFlag_HorizontalScroll = 1U << 10,
    OverlayWindowFlag_NoResize = 1U << 11,
    OverlayWindowFlag_NoMove = 1U << 12,
    OverlayWindowFlag_NoCollapse = 1U << 13,
    OverlayWindowFlag_NoHeaderBar = 1U << 14,
    OverlayWindowFlag_NoBringToFocus = 1U << 15,
    OverlayWindowFlag_NoPromotion = 1U << 16,
    OverlayWindowFlag_AutoResize = 1U << 17,
    OverlayWindowFlag_BringToTop = 1U << 18,
    OverlayWindowFlag_Modal = 1U << 19,
    OverlayWindowFlag_NoCloseButton = 1U << 20,
    OverlayWindowFlag_MenuBar = 1U << 21,
};

struct GrabInfo
{
    TKit::FixedArray<usz, ResizeEdge_Count> Ids{NullLayoutId, NullLayoutId, NullLayoutId, NullLayoutId};
    OverlayColor InteractionColor = OverlayColor_None; // Whether hovered or pressed
    f32v2 ScreenPos;
    f32v2 Size;
    ResizeFlags Flags = 0;
};

struct NextWindowData
{
    f32v2 ScreenPos;
    f32v2 Size;
    NextWindowFlags Flags = 0;
};

struct OverlayWindow;
struct NativeWindow
{
    Window *Window;
    RenderView<D2> *View;
    RenderContext<D2> *Context;
    NativeWindow *Parent = nullptr;

    OverlayWindow *Owner = nullptr;

    u64 Layer = 0;

    f32v2 ScreenPos{0.f};
    // only uses when os actively resizes window, so that child overlay window can adapt if it is a promoted window (has
    // its own surface) OR auto resize is on, which means this size and the reported size of the overlay window may
    // differ
    f32v2 Size{0.f};

    f32v2 ScreenMousePos{0.f};
    f32v2 ScreenMouseDelta{0.f};

    f32v2 WorldMouse{0.f};
    f32v2 WorldMouseOnPress{0.f};
    f32v2 WorldMouseDelta{0.f};

    f32v2 ScreenTopLeftBorder;
    f32v2 ScreenBottomRightBorder;

    f32v2 WorldTopLeftBorder;
    f32v2 WorldBottomRightBorder;

    // overflow clicks means how many rapid succession clicks have happened without counting the first (aka, == 1 is
    // a double click)
    u32 OverflowClicks = 0;
    TKit::StaticBitSet<Key_Count> EventKeys{Key_Count};
    TKit::String TextInput{};

    TKit::String InputWidgetBuffer{};
    TKit::Clock ClickClock{};

    NativeWindowFlags Flags = 0;

    void UpdateBorders()
    {
        WorldTopLeftBorder = View->ScreenToWorld(f32v2{0.f});
        WorldBottomRightBorder = View->ScreenToWorld(f32v2{1.f});
    }

    f32v2 ToScreen(const f32v2 &world) const
    {
        return ScreenPos + View->ToAbsolute(View->WorldToScreen(world));
    }
    f32v2 ToWorld(const f32v2 &screen) const
    {
        return View->ScreenToWorld(View->ToNormalized(screen - ScreenPos));
    }

    f32v2 GetWorldTopLeft() const
    {
        return WorldTopLeftBorder;
    }
    f32v2 GetWorldTopRight() const
    {
        return {WorldBottomRightBorder[0], WorldTopLeftBorder[1]};
    }
    f32v2 GetWorldBottomLeft() const
    {
        return {WorldTopLeftBorder[0], WorldBottomRightBorder[1]};
    }
    f32v2 GetWorldBottomRight() const
    {
        return WorldBottomRightBorder;
    }
    f32 GetWorldLeft() const
    {
        return WorldTopLeftBorder[0];
    }
    f32 GetWorldRight() const
    {
        return WorldBottomRightBorder[0];
    }
    f32 GetWorldTop() const
    {
        return WorldTopLeftBorder[1];
    }
    f32 GetWorldBottom() const
    {
        return WorldBottomRightBorder[1];
    }
    f32v2 GetDimensions() const
    {
        return {GetWorldRight() - GetWorldLeft(), GetWorldTop() - GetWorldBottom()};
    }
};

struct OverlayWindow
{
    OverlayWindow(const LayoutSpecs &spc) : Layout(spc)
    {
    }

    usz Id = NullLayoutId;
    u64 Layer;
    NativeWindow *Native;

    GrabInfo Grab{};

    Layout Layout;

    f32v2 ScreenPos{120.f};

    f32v2 Size{240.f};
    // its nice when disabling auto resize to recover old size
    f32v2 SizeBeforeAutoResize{240.f};
    f32v2 MinSize;

    f32 LastHeight = 240.f;
    u32 PopupDepth = 0;
    CodePoint HeaderIcon;
    OverlayWindowFlags Flags = 0;

    const f32v2 &GetActivePosition() const;
    f32v2 &GetActivePosition();
    void SetActivePosition(const f32v2 &pos);

    void ClampToNative();
    void SyncNativeSize();

    bool IsFullscreenBlocked() const;
    bool CanResize() const;
    bool CanMove() const;
    bool CanCollapse() const;

    f32v2 ToScreen(const f32v2 &world) const;
    f32v2 ToWorld(const f32v2 &screen) const;
};

/////////////////////////////////////////////
/// END WINDOWS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// QUERY FLAGS
/////////////////////////////////////////////

// public focus query flags -> these flags are given to the user/dev by queries
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
    OverlayFocusQueryFlag_DragSource = 1U << 8,
    OverlayFocusQueryFlag_DragTarget = 1U << 9,
    OverlayFocusQueryFlag_DragPayloadDropped = 1U << 10,
};

// internal focus flags -> these configure how querying focus behave
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
    FocusFlag_HoverOpensPopup = 1U << 17,
    FocusFlag_HoverRequestsPopupCollapse = 1U << 18,
    FocusFlag_DoNotSetHoveredId = 1U << 19,
    FocusFlag_DoNotSetPressedId = 1U << 20,
    FocusFlag_DoNotSetDraggedId = 1U << 21,
    FocusFlag_DoNotSetActiveId = 1U << 22,
    FocusFlag_DoNotProtectPopup = 1U << 23,
    FocusFlag_AllowPressedOnEnter = 1U << 24,
    FocusFlag_ReadOnly = FocusFlag_DoNotSetHoveredId | FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetDraggedId |
                         FocusFlag_DoNotSetActiveId | FocusFlag_DoNotProtectPopup
};

// same as above, but public
enum OverlayFocusFlagBit : OverlayFocusFlags
{
    OverlayFocusFlag_PressedEvenWhenAwayFromHover = FocusFlag_PressedEvenWhenAwayFromHover
};

enum OverlayHoveredFlagBit : OverlayHoveredFlags
{
    OverlayHoveredFlag_AllowBlockedByWindow = 1U << 0,
    OverlayHoveredFlag_AllowBlockedByWindowGrab = 1U << 1,
    OverlayHoveredFlag_AllowBlockedByPressedItem = 1U << 2,
    OverlayHoveredFlag_AllowBlockedByActiveItem = 1U << 3,
    OverlayHoveredFlag_AllowBlockedByPopup = 1U << 4,
    OverlayHoveredFlag_AllowBlockedByPopupCollapse = 1U << 5,
    OverlayHoveredFlag_AllowBlockedByDisabled = 1U << 6,
    OverlayHoveredFlag_AllowBlockedByDrag = 1U << 7,
    OverlayHoveredFlag_NoSharedDelay = 1U << 8,
    OverlayHoveredFlag_ShortDelay = 1U << 9,
    OverlayHoveredFlag_NormalDelay = 1U << 10,
    OverlayHoveredFlag_Stationary = 1U << 11,
};

enum OverlayHoverQueryFlagBit : OverlayHoverQueryFlags
{
    OverlayHoverQueryFlag_BlockedByWindow = OverlayHoveredFlag_AllowBlockedByWindow,
    OverlayHoverQueryFlag_BlockedByWindowGrab = OverlayHoveredFlag_AllowBlockedByWindowGrab,
    OverlayHoverQueryFlag_BlockedByPressedItem = OverlayHoveredFlag_AllowBlockedByPressedItem,
    OverlayHoverQueryFlag_BlockedByActiveItem = OverlayHoveredFlag_AllowBlockedByActiveItem,
    OverlayHoverQueryFlag_BlockedByPopup = OverlayHoveredFlag_AllowBlockedByPopup,
    OverlayHoverQueryFlag_BlockedByPopupCollapse = OverlayHoveredFlag_AllowBlockedByPopupCollapse,
    OverlayHoverQueryFlag_BlockedByDisabled = OverlayHoveredFlag_AllowBlockedByDisabled,
    OverlayHoverQueryFlag_BlockedByDrag = OverlayHoveredFlag_AllowBlockedByDrag,
    OverlayHoverQueryFlag_Hovered = 1U << 15,
};

/////////////////////////////////////////////
/// END QUERY FLAGS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// WIDGET FLAGS
/////////////////////////////////////////////

enum OverlayButtonFlagBit : OverlayButtonFlags
{
    OverlayButtonFlag_SpanFullWidth = 1U << 0,
    OverlayButtonFlag_Small = 1U << 1,
    OverlayButtonFlag_TryKeepSquare = 1U << 2,
};

enum OverlayColorFlagBit : OverlayColorFlags
{
    OverlayColorFlag_NoAlpha = 1U << 0,
    OverlayColorFlag_NoInput = 1U << 1,
    OverlayColorFlag_NoColorMarkers = 1U << 2,
    OverlayColorFlag_NoPicker = 1U << 3,
    OverlayColorFlag_NoTooltip = 1U << 4,
    OverlayColorFlag_NoPreview = 1U << 5,
    OverlayColorFlag_NoTooltipLabel = 1U << 6,
    OverlayColorFlag_NoTooltipColorInfo = 1U << 7,
    OverlayColorFlag_NoDragDrop = 1U << 8,
    OverlayColorFlag_HSV = 1U << 9,
    OverlayColorFlag_Hex = 1U << 10,
    OverlayColorFlag_Float = 1U << 11,
};

enum OverlayDragDropFlagBit : OverlayDragDropFlags
{
    OverlayDragDropFlag_SourceNoTooltip = 1U << 0,

    OverlayDragDropFlag_TargetNoTooltip = 1U << 1,
    OverlayDragDropFlag_TargetNoOutline = 1U << 2,
    OverlayDragDropFlag_TargetNoNotAllowedCursor = 1U << 3,
    OverlayDragDropFlag_TargetAcceptOnHover = 1U << 4,

    DragDropFlag_PayloadDropped = 1U << 5,
    DragDropFlag_MustClearTooltip = 1U << 6,
};

enum OverlayDropDownFlagBit : OverlayDropDownFlags
{
    OverlayDropDownFlag_NoArrowButton = 1U << 0,
    OverlayDropDownFlag_NoPreview = 1U << 1,
    OverlayDropDownFlag_HeightSmall = 1U << 2,
    OverlayDropDownFlag_HeightRegular = 1U << 3,
    OverlayDropDownFlag_HeightLargest = 1U << 4,
    OverlayDropDownFlag_Tight = 1U << 5,
};

enum OverlayInputFlagBit : OverlayInputFlags
{
    OverlayInputFlag_EnterReturnsTrue = 1U << 0,
    OverlayInputFlag_EnterCommitsBuffer = 1U << 1,
    OverlayInputFlag_EscapeClearsAll = 1U << 2,
    OverlayInputFlag_AutoSelectAll = 1U << 3,
    OverlayInputFlag_NoHorizontalScroll = 1U << 4,
    OverlayInputFlag_ElideLeft = 1U << 5,
    OverlayInputFlag_StepButtons = 1U << 6,
    OverlayInputFlag_NoUndoRedo = 1U << 7,
};

enum OverlayScrollFlagBit : OverlayScrollFlags
{
    OverlayScrollFlag_Borders = 1U << 0,
    OverlayScrollFlag_Title = 1U << 1,
    OverlayScrollFlag_NoBackground = 1U << 2,
    OverlayScrollFlag_Tight = 1U << 3,
    OverlayScrollFlag_NoScrollBar = OverlayWindowFlag_NoScrollBar,
    OverlayScrollFlag_NoVerticalScroll = OverlayWindowFlag_NoVerticalScroll,
    OverlayScrollFlag_HorizontalScroll = OverlayWindowFlag_HorizontalScroll,
};

enum OverlayPopupFlagBit : OverlayPopupFlags
{
    OverlayPopupFlag_LeftClick = OverlayFocusQueryFlag_LeftClicked,
    OverlayPopupFlag_RightClick = OverlayFocusQueryFlag_RightClicked
};

enum OverlaySelectableFlagBit : OverlaySelectableFlags
{
    OverlaySelectableFlag_SpanLabelWidth = 1U << 0,
    OverlaySelectableFlag_SelectOnDoubleClick = 1U << 1,
    OverlaySelectableFlag_Highlight = 1U << 2,
    OverlaySelectableFlag_CheckBox = 1U << 3,
    OverlaySelectableFlag_FlexWidth = 1U << 4,
    OverlaySelectableFlag_LeftToRight = 1U << 5,
};

enum OverlaySliderFlagBit : OverlaySliderFlags
{
    OverlaySliderFlag_ClampOnInput = 1U << 0,
    OverlaySliderFlag_Logarithmic = 1U << 1,
    OverlaySliderFlag_NoRoundToFormat = 1U << 2,
    OverlaySliderFlag_NoInput = 1U << 3,
};

enum OverlayTabBarFlagBit : OverlayTabBarFlags
{
    OverlayTabBarFlag_Reorderable = 1U << 0,
};

enum OverlayTabFlagBit : OverlayTabFlags
{
    OverlayTabFlag_StartOpen = 1U << 0,

    TabFlag_Enabled = 1U << 1,
    TabFlag_DrawCloseButton = 1U << 2,
    TabFlag_RequestClose = 1U << 3,
    TabFlag_JustPermuted = 1U << 4,
};

enum OverlayTreeFlagBit : OverlayTreeFlags
{
    OverlayTreeFlag_DrawLines = 1U << 0,
    OverlayTreeFlag_ToggleOnArrow = 1U << 1,
    OverlayTreeFlag_OpenOnDoubleClick = 1U << 2,
    OverlayTreeFlag_SpanLabelWidth = 1U << 3,
    OverlayTreeFlag_Framed = 1U << 4,
    OverlayTreeFlag_NoIndent = 1U << 5,
    OverlayTreeFlag_StartOpen = 1U << 6,
};

enum OverlayTooltipFlagBit : OverlayTooltipFlags
{
    OverlayTooltipFlag_Reset = 1U << 0,
    OverlayTooltipFlag_NoPromotion = 1U << 1
};

enum InputConvertFlagBit : InputConvertInfoFlags
{
    InputConvertFlag_Hovered = OverlayFocusQueryFlag_Hovered,
    InputConvertFlag_MustConvert = 1U << 1,
    InputConvertFlag_MustOverrideHighlight = 1U << 2,
    InputConvertFlag_AllowDoubleClick = 1U << 3,
};

// NOTE(Isma, 25/06/06): Consider exposing these through a function QueryWidgetState or something like this
enum WidgetStateFlagBit : WidgetStateFlags
{
    WidgetStateFlag_Opened = 1U << 0,
    // only used if OnHover focus flags are used
    WidgetStateFlag_Hovering = 1U << 1,
};

/////////////////////////////////////////////
/// END WIDGET FLAGS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// WIDGET STATE
/////////////////////////////////////////////

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
    LayoutDirection Direction = LayoutDirection_TopToBottom; // useful for tabs
    vec2<LayoutSizing> OuterSizing;
    vec2<LayoutSizing> ContentSizing;
    f32 ContentPadding;
    f32 ChildGap;
    f32 VerticalOffset = 0.f; // useful for tabs
    OverlayScrollFlags Flags;
};

struct PickerData
{
    const DynamicMeshInfo<D2> *PickerQuad;
    const DynamicMeshInfo<D2> *HueBar;
    const DynamicMeshInfo<D2> *AlphaBar;
    f32v2 CirclePos{0.f};
    f32v3 Hsv{0.f};
    f32v3 Rgb{0.f};
    f32 HueRodPos = 0.f;
    f32 AlphaRodPos = 0.f;
};

struct Tab
{
    usz Id;
    TKit::String Label;
    OverlayTabFlags Flags = 0;
};

struct TabBarData
{
    usz Id;
    usz OpenId = NullLayoutId;
    TKit::TierArray<Tab> Tabs{};
    TKit::TierArray<u32> Order{};
    OverlayTabBarFlags Flags = 0;

    u32 GetTabById(const LayoutId id) const
    {
        for (u32 i = 0; i < Tabs.GetSize(); ++i)
            if (Tabs[i].Id == id)
                return i;
        return TKIT_U32_MAX;
    }
};

struct OverlayDragDropPayload
{
    TKit::StringView Identifier{};
    void *Data = nullptr;
    u32 Size = 0;

    operator bool() const
    {
        return Size != 0;
    }
};

/////////////////////////////////////////////
/// END WIDGET STATE
/////////////////////////////////////////////

/////////////////////////////////////////////
/// IMPLEMENTATION
/////////////////////////////////////////////

struct OverlaySpecs
{
    LayoutSpecs Layout{.RootAlignment = {Alignment_Left, Alignment_Top}};
    OverlayStyle Style{};
    OverlayFlags Flags = 0;
};
class Overlay
{
    /////////////////////////////////////////////
    /// INITIALIZATION
    /////////////////////////////////////////////

    TKIT_NON_COPYABLE(Overlay)

  public:
    using LyPnPar = LayoutPanelParameters;
    using LyTxPar = LayoutTextParameters;
    using LyUnPar = LayoutUnicodeParameters;

    using LySz = LayoutSizing;
    using LyOf = LayoutOffset;
    using LyAtt = LayoutAttachment;
    using LyAlg = Alignment;

    using LySz2 = vec2<LySz>;
    using LyOf2 = vec2<LyOf>;
    using LyAtt2 = vec2<LyAtt>;
    using LyAlg2 = vec2<LyAlg>;

    static constexpr vec2<Alignment> TopLeft = {Alignment_Left, Alignment_Top};
    static constexpr vec2<Alignment> BottomLeft = {Alignment_Left, Alignment_Bottom};

    static constexpr vec2<Alignment> TopRight = {Alignment_Right, Alignment_Top};
    static constexpr vec2<Alignment> CenterLeft = {Alignment_Left, Alignment_Center};

    static constexpr vec2<Alignment> TopCenter = {Alignment_Center, Alignment_Left};
    static constexpr vec2<Alignment> Center = Alignment_Center;

    Overlay(Window *win, const OverlaySpecs &specs);
    ~Overlay();

    /////////////////////////////////////////////
    /// END INITIALIZATION
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// WINDOWS/MENUS PUBLIC
    /////////////////////////////////////////////

    bool BeginWindow(TKit::StringView title, bool *opened, OverlayWindowFlags flags = 0);
    bool BeginWindow(const TKit::StringView title, const OverlayWindowFlags flags = 0)
    {
        return BeginWindow(title, nullptr, flags);
    }
    void EndWindow();

    bool BeginMenuBar();
    void EndMenuBar();

    bool BeginMainMenuBar();
    void EndMainMenuBar();

    bool BeginMenu(TKit::StringView label);
    void EndMenu();

    void SetNextWindowPosition(const f32v2 &pos)
    {
        m_NextWindow.ScreenPos = pos;
        m_NextWindow.Flags |= NextWindowFlag_Position;
    }
    void SetNextWindowSize(const f32v2 &size)
    {
        m_NextWindow.Size = size;
        m_NextWindow.Flags |= NextWindowFlag_Size;
    }

    bool MenuItem(const TKit::StringView label, const bool enabled = false)
    {
        PushStyleColor(OverlayColor_SelectableIdle, Color_Transparent);
        if (Selectable(label, enabled, OverlaySelectableFlag_CheckBox | OverlaySelectableFlag_FlexWidth))
        {
            CollapsePopups();
            PopStyleColor();
            return true;
        }
        PopStyleColor();
        return false;
    }
    bool MenuItem(const TKit::StringView label, bool *enabled)
    {
        if (MenuItem(label, *enabled))
        {
            *enabled = !*enabled;
            return true;
        }
        return false;
    }
    const NativeWindow *GetMainNativeWindow() const
    {
        return (m_Flags & OverlayFlag_FloatingMode) ? nullptr : m_NativeWindows[0];
    }
    bool IsCurrentWindowPromoted() const;

    /////////////////////////////////////////////
    /// END WINDOWS/MENUS PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// WIDGETS PUBLIC
    /////////////////////////////////////////////

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
        return BeginSelectable(GetCurrentLayout().GenerateNextId(), enabled, flags);
    }
    bool BeginSelectable(bool *enabled, const OverlaySelectableFlags flags = 0)
    {
        return BeginSelectable(GetCurrentLayout().GenerateNextId(), enabled, flags);
    }

    void EndSelectable();

    bool Selectable(TKit::StringView label, bool enabled = false, OverlaySelectableFlags flags = 0);
    bool Selectable(TKit::StringView label, bool *enabled, OverlaySelectableFlags flags = 0);

    void ProgressBar(TKit::StringView label, TKit::StringView text, f32 pct);
    void ProgressBar(const TKit::StringView label, const f32 pct)
    {
        ProgressBar(label, {}, pct);
    }
    template <typename... Args>
    void ProgressBar(const TKit::StringView label, const f32 pct, const fmt::format_string<Args...> str, Args &&...args)
    {
        const TKit::StackString txt = TKit::StackString::Format(str, std::forward<Args>(args)...);
        ProgressBar(label, txt, pct);
    }

    void BeginTabBar(LayoutId id, OverlayTabBarFlags flags = 0);
    void EndTabBar();

    bool BeginTab(TKit::StringView label, bool *enabled, OverlayTabFlags flags = 0);
    bool BeginTab(const TKit::StringView label, OverlayTabFlags flags = 0)
    {
        return BeginTab(label, nullptr, flags);
    }
    void EndTab()
    {
        PopId();
        GetCurrentLayout().EndPanel();
    }

    bool InputText(TKit::StringView label, char *buf, u32 size, TKit::StringView hint = {},
                   OverlayInputFlags flags = 0);
    template <TKit::Numeric T>
    bool InputNumeric(const TKit::StringView label, T *value, const char *format = nullptr,
                      const TKit::StringView hint = {}, const OverlayInputFlags flags = 0)
    {
        beginHorizontalWidget(PushId(label));
        const bool updated = inputNumericBox(value, format, hint, flags);
        if (flags & OverlayInputFlag_StepButtons)
        {
            if (Button("-", OverlayButtonFlag_TryKeepSquare))
                --(*value);
            if (Button("+", OverlayButtonFlag_TryKeepSquare))
                ++(*value);
        }
        endHorizontalWidget(label);
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

    template <TKit::Numeric T, std::convertible_to<T> U>
    bool HorizontalSlider(const TKit::StringView label, T *value, const U mn, const U mx, const char *format = nullptr,
                          const u32 count = 1, const OverlaySliderFlags flags = 0)
    {
        TKIT_ASSERT(mn < mx, "[ONYX][OVERLAY] Maximum slider value ({}), must be greater than minimum ({})", mx, mn);
        beginHorizontalWidget(PushId(label));
        bool changed = false;
        for (u32 i = 0; i < count; ++i)
        {
            T &val = value[i];
            PushId(&val);
            changed |= horizontalSliderBox(&val, mn, mx, format, flags);
            PopId();
        }
        endHorizontalWidget(label);
        PopId();
        return changed;
    }

    template <TKit::Numeric T, u32 N, std::convertible_to<T> U>
    bool HorizontalSlider(const TKit::StringView label, vec<T, N> *value, const U mn, const U mx,
                          const char *format = nullptr, const OverlaySliderFlags flags = 0)
    {
        return HorizontalSlider(label, Math::AsPointer(*value), mn, mx, format, N, flags);
    }
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool VerticalSlider(const TKit::StringView label, T *value, const U mn, const U mx, const char *format = nullptr,
                        const u32 count = 1, const OverlaySliderFlags flags = 0)
    {
        TKIT_ASSERT(mn < mx, "[ONYX][OVERLAY] Maximum slider value ({}), must be greater than minimum ({})", mx, mn);

        Layout &ly = GetCurrentLayout();
        const LayoutId id = PushId(label);
        ly.BeginPanel(id, LyPnPar{.Direction = LayoutDirection_TopToBottom,
                                  .Sizing = {fit(), sabs(m_Style[OverlayStyle_VerticalSliderHeight])},
                                  .ChildGap = m_Style[OverlayStyle_ChildGap]});
        bool changed = false;
        const TKit::StringView tlabel = trimLabel(label);
        for (u32 i = 0; i < count; ++i)
        {
            T &val = value[i];
            PushId(i);
            changed |= verticalSliderBox(tlabel, &val, mn, mx, format, flags);
            PopId();
        }

        ly.EndPanel();
        PopId();
        return changed;
    }
    template <TKit::Numeric T, u32 N, std::convertible_to<T> U>
    bool VerticalSlider(const TKit::StringView label, vec<T, N> *value, const U mn, const U mx,
                        const char *format = nullptr, const OverlaySliderFlags flags = 0)
    {
        return VerticalSlider(label, Math::AsPointer(*value), mn, mx, format, N, flags);
    }

    template <TKit::Numeric T, std::convertible_to<T> U = T>
    bool HorizontalDrag(const TKit::StringView label, T *value, const f32 speed = 1.f, const U mn = T(0),
                        const U mx = T(0), const char *format = nullptr, const u32 count = 1,
                        const OverlaySliderFlags flags = 0)
    {
        beginHorizontalWidget(PushId(label));
        bool changed = false;
        for (u32 i = 0; i < count; ++i)
        {
            T &val = value[i];
            PushId(i);
            changed |= horizontalDragBox(&val, speed, mn, mx, format, flags);
            PopId();
        }

        endHorizontalWidget(label);
        PopId();
        return changed;
    }

    template <TKit::Numeric T, u32 N, std::convertible_to<T> U = T>
    bool HorizontalDrag(const TKit::StringView label, vec<T, N> *value, const f32 speed = 1.f, const U mn = T(0),
                        const U mx = T(0), const char *format = nullptr, const OverlaySliderFlags flags = 0)
    {
        return HorizontalDrag(label, Math::AsPointer(*value), speed, mn, mx, format, N, flags);
    }

    template <TKit::Numeric T, std::convertible_to<T> U = T>
    bool VerticalDrag(const TKit::StringView label, T *value, const f32 speed = 1.f, const U mn = T(0),
                      const U mx = T(0), const char *format = nullptr, const u32 count = 1,
                      const OverlaySliderFlags flags = 0)
    {
        Layout &ly = GetCurrentLayout();
        const LayoutId id = PushId(label);

        ly.BeginPanel(id, LyPnPar{.Direction = LayoutDirection_TopToBottom,
                                  .Sizing = {fit(), sabs(m_Style[OverlayStyle_VerticalDragHeight])},
                                  .ChildGap = m_Style[OverlayStyle_ChildGap]});
        bool changed = false;
        const TKit::StringView tlabel = trimLabel(label);
        for (u32 i = 0; i < count; ++i)
        {
            T &val = value[i];
            PushId(i);
            changed |= verticalDragBox(tlabel, &val, speed, mn, mx, format, flags);
            PopId();
        }
        ly.EndPanel();
        PopId();
        return changed;
    }
    template <TKit::Numeric T, u32 N, std::convertible_to<T> U = T>
    bool VerticalDrag(const TKit::StringView label, vec<T, N> *value, const f32 speed = 1.f, const U mn = T(0),
                      const U mx = T(0), const char *format = nullptr, const OverlaySliderFlags flags = 0)
    {
        return VerticalDrag(label, Math::AsPointer(*value), speed, mn, mx, format, N, flags);
    }

    void ColorPreviewTooltip(TKit::StringView label, const Color &col, OverlayColorFlags flags = 0);
    void ColorPreview(TKit::StringView label, const Color &col, OverlayColorFlags flags = 0);

    bool ColorPicker(TKit::StringView label, OverlayColorHandle color, const Color *original, f32 pickerSize,
                     OverlayColorFlags flags = 0);
    bool ColorPicker(const TKit::StringView label, const OverlayColorHandle color, const Color *original,
                     OverlayColorFlags flags = 0)
    {
        return ColorPicker(label, color, original, 0.6f * m_Current->Size[0], flags);
    }
    bool ColorPicker(const TKit::StringView label, const OverlayColorHandle color, const f32 size,
                     OverlayColorFlags flags = 0)
    {
        return ColorPicker(label, color, nullptr, size, flags);
    }
    bool ColorPicker(const TKit::StringView label, const OverlayColorHandle color, OverlayColorFlags flags = 0)
    {
        return ColorPicker(label, color, nullptr, 0.6f * m_Current->Size[0], flags);
    }

    bool ColorButton(TKit::StringView label, OverlayColorHandle color, OverlayColorFlags flags = 0);
    bool ColorEditor(TKit::StringView label, OverlayColorHandle color, OverlayColorFlags flags = 0);

    template <TKit::Integer T>
    bool ListBox(const TKit::StringView label, T *current, const TKit::Span<const TKit::StringView> elements,
                 const OverlaySelectableFlags flags = 0)
    {
        const T val = *current;
        BeginScroll(label, m_Style[OverlayStyle_ListBoxMaxHeight],
                    OverlayScrollFlag_Tight | OverlayScrollFlag_Borders | OverlayScrollFlag_Title);

        for (u32 i = 0; i < elements.GetSize(); ++i)
            if (Selectable(elements[i], val == i, flags))
                *current = i;

        EndScroll();
        return val != *current;
    }
    template <TKit::Integer T>
    bool ListBox(const TKit::StringView label, T *current, const TKit::StringView elements,
                 const OverlaySelectableFlags flags = 0)
    {
        const TKit::StackString str{elements.GetData(), elements.GetSize()};
        const TKit::StackArray<TKit::StackString> splits = str.Split("#");

        TKit::StackArray<TKit::StringView> views{};
        views.Reserve(splits.GetSize());
        for (const TKit::StackString &elm : splits)
            views.Append(elm);
        return ListBox(label, current, views, flags);
    }

    /////////////////////////////////////////////
    /// END WIDGETS PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// DISPLAY PUBLIC
    /////////////////////////////////////////////

    void SetNextTextId(const LayoutId id)
    {
        m_TextId = id;
    }

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
        GetCurrentLayout().Panel(LyPnPar{.FillColor = Color_White,
                                         .Sizing = sabs(size),
                                         .Shape = rect(m_Style[OverlayStyle_ImageRadius]),
                                         .Texture = texture,
                                         .TexOffset = offset,
                                         .TexScale = scale});
    }

    void BeginDisabled(bool enabled = true);
    void EndDisabled();

    /////////////////////////////////////////////
    /// END DISPLAY PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// POPUPS PUBLIC
    /////////////////////////////////////////////

    void OpenPopup(const LayoutId id)
    {
        m_PopupStack.Append(id);
    }
    void CloseCurrentPopup();
    void CloseChildPopup();
    void CollapsePopups();

    bool BeginPopup(LayoutId id, TKit::StringView title, OverlayWindowFlags flags = 0);
    bool BeginPopup(const TKit::StringView title, const OverlayWindowFlags flags = 0)
    {
        return BeginPopup(title, title, flags);
    }
    void EndPopup();

    bool BeginPopupContextItem(const LayoutId id, const TKit::StringView title, const OverlayWindowFlags wflags = 0,
                               const OverlayPopupFlags flags = OverlayPopupFlag_RightClick)
    {
        if (QueryItemFocusStatus() & flags)
            OpenPopup(id);

        return BeginPopup(id, title, wflags);
    }
    bool BeginPopupContextItem(const TKit::StringView title, const OverlayWindowFlags wflags = 0,
                               const OverlayPopupFlags flags = OverlayPopupFlag_RightClick)
    {
        return BeginPopupContextItem(title, title, wflags, flags);
    }

    bool BeginDropDown(TKit::StringView label, TKit::StringView preview, OverlayDropDownFlags flags = 0);
    void EndDropDown()
    {
        endScroll();
        PopStyleVar();
        Layout &ly = GetCurrentLayout();
        ly.EndPanel();
        ly.EndPanel();
        PopId();
        --m_CurrentPopupDepth;
    }

    template <TKit::Integer T>
    bool DropDown(const TKit::StringView label, T *current, const TKit::Span<const TKit::StringView> elements,
                  const OverlayDropDownFlags flags = 0, const OverlaySelectableFlags sflags = 0)
    {
        const T val = *current;
        if (BeginDropDown(label, val < elements.GetSize() ? elements[val] : "", flags | OverlayDropDownFlag_Tight))
        {
            for (u32 i = 0; i < elements.GetSize(); ++i)
                if (Selectable(elements[i], val == i, sflags | OverlaySelectableFlag_FlexWidth))
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

    /////////////////////////////////////////////
    /// END POPUPS PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// TOOLTIPS PUBLIC
    /////////////////////////////////////////////

    void BeginTooltip(OverlayTooltipFlags flags = 0);
    void EndTooltip();
    template <typename... Args>
    void SetTooltip(const OverlayTooltipFlags flags, const fmt::format_string<Args...> str, Args &&...args)
    {
        BeginTooltip(flags | OverlayTooltipFlag_Reset);
        Text(str, std::forward<Args>(args)...);
        EndTooltip();
    }
    template <typename... Args> void SetTooltip(const fmt::format_string<Args...> str, Args &&...args)
    {
        SetTooltip(0, str, std::forward<Args>(args)...);
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

    /////////////////////////////////////////////
    /// END TOOLTIPS PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// LAYOUT PUBLIC
    /////////////////////////////////////////////

    bool BeginScroll(TKit::StringView label, f32 maxHeight, f32 maxWidth, OverlayScrollFlags flags = 0);
    bool BeginScroll(TKit::StringView label, f32 maxHeight, OverlayScrollFlags flags = 0)
    {
        return BeginScroll(label, maxHeight, TKIT_F32_MAX, flags);
    }
    void EndScroll()
    {
        PopId();
        EndPanel();
        endScroll();
    }

    void Separator()
    {
        const LayoutElement &elm = GetCurrentLayout().GetCurrentElement();
        if (elm.Direction == LayoutDirection_LeftToRight || elm.Direction == LayoutDirection_RightToLeft)
            VerticalLine();
        else
            HorizontalLine();
    }
    void HorizontalSeparator(TKit::StringView label);
    void HorizontalLine()
    {
        GetCurrentLayout().Panel(LyPnPar{.FillColor = m_Style[OverlayColor_Line],
                                         .Sizing = {grow(), sabs(m_Style[OverlayStyle_LineWidth])},
                                         .Shape = rect(m_Style[OverlayStyle_LineRadius])});
    }
    void VerticalLine()
    {
        GetCurrentLayout().Panel(LyPnPar{.FillColor = m_Style[OverlayColor_Line],
                                         .Sizing = {sabs(m_Style[OverlayStyle_LineWidth]), grow()},
                                         .Shape = rect(m_Style[OverlayStyle_LineRadius])});
    }

    usz BeginPanel(const LayoutId id, const LyPnPar &params = {})
    {
        m_LastItem = GetCurrentLayout().BeginPanel(id, params);
        return m_LastItem;
    }
    void BeginPanel(const LyPnPar &params = {})
    {
        GetCurrentLayout().BeginPanel(params);
    }
    void EndPanel()
    {
        GetCurrentLayout().EndPanel();
    }

    usz Panel(const LayoutId id, const LyPnPar &params = {})
    {
        m_LastItem = GetCurrentLayout().Panel(id, params);
        return m_LastItem;
    }
    void Panel(const LyPnPar &params = {})
    {
        GetCurrentLayout().Panel(params);
    }

    bool PushTreeRaw(LayoutId id, TKit::StringView label, OverlayTreeFlags flags = 0);
    bool PushTree(TKit::StringView label, const OverlayTreeFlags flags = 0)
    {
        return PushTreeRaw(label, label, flags);
    }
    template <typename... Args> bool PushTree(const LayoutId id, const fmt::format_string<Args...> str, Args &&...args)
    {
        return PushTree(id, 0, str, std::forward<Args>(args)...);
    }
    template <typename... Args>
    bool PushTree(const LayoutId id, const OverlayTreeFlags flags, const fmt::format_string<Args...> str,
                  Args &&...args)
    {
        const TKit::StackString txt = TKit::StackString::Format(str, std::forward<Args>(args)...);
        return PushTreeRaw(id, txt, flags);
    }
    void PopTree()
    {
        Layout &ly = GetCurrentLayout();
        ly.EndPanel();
        ly.EndPanel();
        PopId();
    }

    void PushDirection(const LayoutDirection dir, const f32 childGap)
    {
        BeginPanel({.Direction = dir, .Alignment = TopLeft, .Sizing = fit(), .ChildGap = childGap});
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
                    .Alignment = TopLeft,
                    .Sizing = fit(),
                    .ChildOffset = oabs({indent, 0.f}),
                    .ChildGap = m_Style[OverlayStyle_ChildGap]});
    }
    void PopIndent()
    {
        EndPanel();
    }

    const Layout &GetCurrentLayout() const
    {
        return m_Current->Layout;
    }
    Layout &GetCurrentLayout()
    {
        return m_Current->Layout;
    }

    const OverlayStyle &GetStyle() const
    {
        return m_Style;
    }
    OverlayStyle &GetStyle()
    {
        return m_Style;
    }
    const OverlayStyle &GetDefaultStyle() const
    {
        return m_DefaultStyle;
    }

    LayoutId IdFromStack(LayoutId id)
    {
        if (m_IdStack.IsEmpty())
            return id;
        id.Id = TKit::Hash(m_IdStack.GetBack(), id.Id);
        return id;
    }

    LayoutId PushId(LayoutId id)
    {
        id = IdFromStack(id);
        m_IdStack.Append(id);
        return id;
    }
    void PopId()
    {
        m_IdStack.Pop();
    }

    /////////////////////////////////////////////
    /// END LAYOUT PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// STYLE PUBLIC
    /////////////////////////////////////////////

    void PushStyleVar(const OverlayStyleVariable var, const f32 val)
    {
        m_StyleStack.Append(m_Style[var], var);
        m_Style.Variables[var] = val;
    }

    void PopStyleVar()
    {
        const StyleBackup &b = m_StyleStack.GetBack();
        m_Style.Variables[b.Index] = b.Old;
        m_StyleStack.Pop();
    }
    void PopStyleVar(const u32 count)
    {
        for (u32 i = 0; i < count; ++i)
            PopStyleVar();
    }

    void PushStyleColor(const OverlayColor color, const Color &col)
    {
        m_ColorStack.Append(m_Style[color], color);
        m_Style.Colors[color] = col;
    }

    void PopStyleColor()
    {
        const ColorBackup &b = m_ColorStack.GetBack();
        m_Style.Colors[b.Index] = b.Old;
        m_ColorStack.Pop();
    }

    /////////////////////////////////////////////
    /// END STYLE PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// INTERACTION/INPUT PUBLIC
    /////////////////////////////////////////////

    bool BeginDragDropSource(OverlayDragDropFlags flags = 0);
    void EndDragDropSource();

    bool BeginDragDropTarget(OverlayDragDropFlags flags = 0);
    void EndDragDropTarget()
    {
        if (m_DragDropFlags & DragDropFlag_PayloadDropped)
        {
            m_DragDropPayload = {};
            m_DragDropFlags = 0;
        }
    }

    // NOTE(Isma, 08/07/26) right now identifiers' memory is owned by user. may consider copying the actual string
    // contents into payload
    void SetDragDropPayload(const TKit::StringView identifier, void *data, const u32 size)
    {
        m_DragDropPayload = {identifier, data, size};
    }
    template <typename T> void SetDragDropPayload(const TKit::StringView identifier, T *data)
    {
        SetDragDropPayload(identifier, data, sizeof(T));
    }
    OverlayDragDropPayload AcceptDragDropPayload(TKit::StringView identifier);

    OverlayHoverQueryFlags QueryItemHoverStatus(const f32v2 &hoverPadding = f32v2{0.f}) const
    {
        return queryHoverStatus(GetCurrentLayout().QueryElement(m_LastItem), hoverPadding);
    }
    OverlayFocusQueryFlags QueryItemFocusStatus(const OverlayFocusFlags flags = 0)
    {
        // return queryAndSetFocusStatus(GetCurrentLayout().QueryElement(m_LastItem), flags | FocusFlag_ReadOnly);
        return queryAndSetFocusStatus(GetCurrentLayout().QueryElement(m_LastItem), flags);
    }
    bool IsItemHovered(const OverlayHoveredFlags flags = 0, const f32v2 &hoverPadding = f32v2{0.f})
    {
        return isElementHovered(GetCurrentLayout().QueryElement(m_LastItem), flags, hoverPadding);
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
    bool IsItemDragged(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_DragSource;
    }
    bool IsItemDragHovered(const OverlayFocusFlags flags = 0)
    {
        return QueryItemFocusStatus(flags) & OverlayFocusQueryFlag_DragTarget;
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
    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

    /////////////////////////////////////////////
    /// END INTERACTION/INPUT PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// RENDERING PUBLIC
    /////////////////////////////////////////////

    void Draw();

    // NOTE(Isma): Consider adding const getters?
    Camera<D2> &GetCamera()
    {
        return m_Camera;
    }
    static constexpr RenderViewFlags GetRenderViewFlags()
    {
        return RenderViewFlag_NormalizedCoordinates | RenderViewFlag_Transparency | RenderViewFlag_PostProcess |
               RenderViewFlag_Outlines;
    }

    /////////////////////////////////////////////
    /// END RENDERING PUBLIC
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// DEMO
    /////////////////////////////////////////////

    void ShowDemo();
    void ShowStyleEditor();
    void ShowRendererStatistics();

    /////////////////////////////////////////////
    /// END DEMO
    /////////////////////////////////////////////

  private:
    StateFlags m_StateFlags = 0;
    OverlayFlags m_Flags = 0;

    /////////////////////////////////////////////
    /// WINDOWS/MENUS PRIVATE
    /////////////////////////////////////////////

    OverlayWindow *getOrCreateOverlayWindow(LayoutId id);
    void assignNativeWindowSomehow(OverlayWindow *win);

    void addActiveWindow(OverlayWindow *win);
    void drawWindowBorders();
    bool iconButton(LayoutId id, CodePoint code, LySz ysizing = LySz::Fit(), OverlayColor idle = OverlayColor_None);
    void popWindowStack();
    u32 processWindows();

    NativeWindow *createNativeWindow(Window *win);
    NativeWindow *createNativeWindow(const f32v2 &pos, const f32v2 &dims, WindowFlags flags = 0);

    void destroyNativeWindow(NativeWindow *win);
    void removeNativeWindow(NativeWindow *win);

    NativeWindow *promoteWindow(OverlayWindow *win, const f32v2 &pos, const f32v2 &dims);
    void demoteWindow(OverlayWindow *win);

    void demoteAllWindows();
    void removeAllFloatWindows();
    void manageWindowPromotions();

    template <typename F> void iterateReverseWindows(F func);

    NativeWindow *getMainNativeWindow()
    {
        return (m_Flags & OverlayFlag_FloatingMode) ? nullptr : m_NativeWindows[0];
    }

    u64 toTop()
    {
        return m_LayerCount++;
    }

    // NOTE(Isma, 25/06/06): Could be a hash map, but assuming the window count will be small enough that a linear
    // search is overall better
    // NOTE(Isma, 25/06/06): Applying a hard cap right now because we use direct pointer references to array elements,
    // and so we just avoid stale references on resizes. I dont really expect more than a handful of these at the same
    // time, so 32 should be plenty
    TKit::StaticArray32<OverlayWindow> m_OverlayWindows{};
    TKit::StaticArray<NativeWindow *, ONYX_MAX_VIEWS> m_NativeWindows{};
    TKit::StaticHashMap<usz, NativeWindow *, 4 * ONYX_MAX_VIEWS> m_FloatWindows{};

    TKit::TierArray<OverlayWindow *> m_ActiveWindows{};
    TKit::TierArray<OverlayWindow *> m_WindowStack{};
    TKit::TierArray<usz> m_WindowIds{};

    NextWindowData m_NextWindow{};

    OverlayWindow *m_Current = nullptr;
    OverlayWindow *m_Grabbed = nullptr;

    u64 m_LayerCount = 0;
    f32 m_WindowSpawnOffset = 0.f;

    /////////////////////////////////////////////
    /// END WINDOWS/MENUS PRIVATE
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// WIDGETS PRIVATE
    /////////////////////////////////////////////

    static TKit::StringView trimLabel(TKit::StringView label);

    void beginHorizontalWidget(usz id, const LySz2 &outerSizing, const LySz2 &innerSizing);
    void beginHorizontalWidget(usz id, f32 normSize = 0.5f);
    void endHorizontalWidget(TKit::StringView label = {});

    bool inputTextBox(char *buf, u32 size, TKit::StringView hint, OverlayInputFlags flags,
                      InputConvertInfoFlags cflags = 0);

    bool colorHexInput(f32 *colPtr, const Color &col, OverlayColorFlags flags);
    bool colorDrag(f32 *colPtr, const Color &col, OverlayColorFlags flags);
    bool colorPicker(TKit::StringView label, f32 *colPtr, const Color &col, const Color *original,
                     OverlayColorFlags flags, f32 size);
    usz drawColorPreview(const Color &col, f32 size, bool alpha);

    struct SliderBoxInfo
    {
        const char *Format;
        f32 InnerWidth;
        f32 Offset;
        OverlayColor Color;
        FocusFlags Flags;
    };

    template <TKit::Numeric T, std::convertible_to<T> U>
    SliderBoxInfo getSliderBoxInfo(const u32 idx, const T pval, T *value, const U mn, const U mx, const char *format,
                                   const LayoutElement *elm, const OverlaySliderFlags flags)
    {
        const T clamped = Math::Clamp(pval, T(mn), T(mx));
        format = getFormat<T>(format);

        OverlayColor col = OverlayColor_SliderIdle;
        const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_PressedEvenWhenAwayFromHover);

        if (focusFlags & OverlayFocusQueryFlag_Pressed)
            col = OverlayColor_SliderPressed;
        else if (focusFlags & OverlayFocusQueryFlag_Hovered)
            col = OverlayColor_SliderHovered;

        const f32 length = elm ? elm->Size[idx] : 0.f;

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
        const NativeWindow *nw = m_Current->Native;
        const f32 normalized = imap(f32(clamped), f32(mn), f32(mx), -1.f, 1.f);
        if ((focusFlags & OverlayFocusQueryFlag_Pressed) && !nw->Window->IsKeyPressed(Key_LeftControl))
        {
            f32 relPos = nw->WorldMouse[idx] - elm->Position[idx] - 0.5f * length;
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

        return {format, innerWidth, offset, col, focusFlags};
    }

    template <TKit::Numeric T, std::convertible_to<T> U>
    bool horizontalSliderBox(T *value, const U mn, const U mx, const char *format, const OverlaySliderFlags flags)
    {
        Layout &ly = GetCurrentLayout();
        const LayoutId id = IdFromStack("__onyx_id_Drag/Slider_hbox");
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

        const SliderBoxInfo sinfo = getSliderBoxInfo(0, pval, value, mn, mx, format, elm, flags);

        // heres how this works. outer slider is the first visible bit. then, 2 children
        // come

        const f32 padding = m_Style[OverlayStyle_WidgetPadding];
        ly.BeginPanel(id, LyPnPar{.FillColor = m_Style[sinfo.Color],
                                  .Alignment = CenterLeft,
                                  .Sizing = {flex(), fit()},
                                  .Shape = rect(m_Style[OverlayStyle_SliderRadius]),
                                  .Padding = padding});

        const f32 boxWidth = elm ? (elm->Size[0] - 2.f * padding) : 0.f;

        // the next 2 children will serve as slots for the slider button and the text. this is required bc text
        // length cannot interfere with slider button positioning in layout calculation
        //
        // we actually need both containers to overlap. but bc of layout calculation, slider slot will be placed
        // correctly (just overlapping inner slider) but text slot will be "offscreen" (clipped by outer slider).
        // so, we offset text slot by 1 parent to align it correctly

        ly.BeginPanel(LyPnPar{.Alignment = Center, .Sizing = {srel(1.f), grow()}});

        ly.Panel(LyPnPar{.FillColor = m_Style[OverlayColor_SliderInner],
                         .Sizing = {sabs(sinfo.InnerWidth), grow()},
                         .SelfOffset = oabs({sinfo.Offset, 0.f}),
                         .Shape = rect(m_Style[OverlayStyle_SliderInnerRadius])});

        const LayoutId txtId = IdFromStack("__onyx_id_Text_slot");
        const LayoutElement *txtElm = ly.QueryElement(txtId);
        const f32 txtOffset = txtElm ? (-0.5f * (txtElm->Size[0] + boxWidth)) : 0.f;
        const bool hasAccurateFlex = elm && txtElm;

        ly.EndPanel();
        ly.BeginPanel(txtId, LyPnPar{.Alignment = Center,
                                     .Sizing = {hasAccurateFlex ? flex() : srel(1.f), fit()},
                                     .SelfOffset = hasAccurateFlex ? oabs({txtOffset, 0.f}) : onorm({-1.0f, 0.f})});

        const TKit::StackString text = TKit::StackString::Format(TKit::RuntimeFormatString(sinfo.Format), *value);
        ly.Text(ly.GenerateNextId(), text, getTextParams());

        ly.EndPanel();
        ly.EndPanel();
        return *value != pval;
    }
    template <TKit::Numeric T, std::convertible_to<T> U>
    bool verticalSliderBox(const TKit::StringView label, T *value, const U mn, const U mx, const char *format,
                           const OverlaySliderFlags flags)
    {
        Layout &ly = GetCurrentLayout();
        const LayoutId id = IdFromStack("__onyx_id_Drag/Slider_vbox");
        const LayoutElement *elm = ly.QueryElement(id);

        const T pval = *value;
        const SliderBoxInfo sinfo = getSliderBoxInfo(1, pval, value, mn, mx, format, elm, flags);

        const f32 padding = m_Style[OverlayStyle_WidgetPadding];
        ly.BeginPanel(id, LyPnPar{.FillColor = m_Style[sinfo.Color],
                                  .Alignment = Center,
                                  .Sizing = {sabs(m_Style[OverlayStyle_VerticalSliderWidth]), grow()},
                                  .Shape = rect(m_Style[OverlayStyle_SliderRadius]),
                                  .Padding = padding});

        ly.Panel(LyPnPar{.FillColor = m_Style[OverlayColor_SliderInner],
                         .Sizing = {grow(), sabs(sinfo.InnerWidth)},
                         .SelfOffset = oabs({0.f, sinfo.Offset}),
                         .Shape = rect(m_Style[OverlayStyle_SliderInnerRadius])});

        ly.EndPanel();

        if (sinfo.Flags & (OverlayFocusQueryFlag_Hovered | OverlayFocusQueryFlag_Pressed))
        {
            BeginTooltip(OverlayTooltipFlag_Reset);
            Layout &tly = GetCurrentLayout();

            if (!label.IsEmpty())
                tly.Text(tly.GenerateNextId(), label, getTextParams());
            const TKit::StackString text = TKit::StackString::Format(TKit::RuntimeFormatString(sinfo.Format), *value);
            tly.Text(tly.GenerateNextId(), text, getTextParams());

            EndTooltip();
        }

        return *value != pval;
    }

    struct DragBoxInfo
    {
        const char *Format;
        OverlayColor Color;
        FocusFlags Flags;
    };

    template <TKit::Numeric T, std::convertible_to<T> U>
    DragBoxInfo getDragBoxInfo(const u32 idx, T *value, const f32 speed, const U mn, const U mx, const char *format,
                               const LayoutElement *elm, const OverlaySliderFlags flags)
    {
        const bool hasLimits = mn < mx;
        format = getFormat<T>(format);

        OverlayColor col = OverlayColor_DragIdle;
        const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_PressedEvenWhenAwayFromHover);

        if (focusFlags & OverlayFocusQueryFlag_Pressed)
            col = OverlayColor_SliderPressed;
        else if (focusFlags & OverlayFocusQueryFlag_Hovered)
            col = OverlayColor_SliderHovered;

        if (focusFlags & OverlayFocusQueryFlag_JustActive)
            m_DragValue = f64(*value);

        if (focusFlags & OverlayFocusQueryFlag_Pressed)
        {
            const u32 decimals = getFormatDecimals(format);
            const bool log = flags & OverlaySliderFlag_Logarithmic;

            const NativeWindow *nw = m_Current->Native;
            const f32 drag = nw->WorldMouse[idx] - nw->WorldMouseOnPress[idx];
            const f32 effectiveSpeed =
                log ? (speed * Math::Max(Math::Absolute(f32(m_DragValue + drag)),
                                         decimals == 0 ? 1e-4f : Math::Power(10.f, -f32(decimals))))
                    : speed;

            f64 nval = m_DragValue + f64(effectiveSpeed * drag);
            if (hasLimits)
                nval = Math::Clamp(nval, f64(mn), f64(mx));

            *value = roundToFormat(T(nval), decimals);
        }
        return {format, col, focusFlags};
    }

    template <TKit::Numeric T, std::convertible_to<T> U>
    bool horizontalDragBox(T *value, const f32 speed, const U mn, const U mx, const char *format,
                           const OverlaySliderFlags flags)
    {
        Layout &ly = GetCurrentLayout();
        const LayoutId id = IdFromStack("__onyx_id_Drag/Slider_hbox");

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

        const DragBoxInfo dinfo = getDragBoxInfo(0, value, speed, mn, mx, format, elm, flags);

        ly.BeginPanel(id, LyPnPar{.FillColor = m_Style[dinfo.Color],
                                  .Alignment = Center,
                                  .Sizing = {flex(), fit()},
                                  .Shape = rect(m_Style[OverlayStyle_DragRadius]),
                                  .Padding = m_Style[OverlayStyle_WidgetPadding]});

        const TKit::StackString text = TKit::StackString::Format(TKit::RuntimeFormatString(dinfo.Format), *value);
        ly.Text(ly.GenerateNextId(), text, getTextParams());

        ly.EndPanel();
        return *value != pval;
    }

    template <TKit::Numeric T, std::convertible_to<T> U>
    bool verticalDragBox(const TKit::StringView label, T *value, const f32 speed, const U mn, const U mx,
                         const char *format, const OverlaySliderFlags flags)
    {
        Layout &ly = GetCurrentLayout();
        const LayoutId id = IdFromStack("__onyx_id_Drag/Slider_vbox");

        const LayoutElement *elm = ly.QueryElement(id);
        const T pval = *value;

        const DragBoxInfo dinfo = getDragBoxInfo(1, value, speed, mn, mx, format, elm, flags);

        ly.Panel(id, LyPnPar{.FillColor = m_Style[dinfo.Color],
                             .Sizing = {sabs(m_Style[OverlayStyle_VerticalDragWidth]), grow()},
                             .Shape = rect(m_Style[OverlayStyle_DragRadius])});

        if (dinfo.Flags & (OverlayFocusQueryFlag_Hovered | OverlayFocusQueryFlag_Pressed))
        {
            BeginTooltip(OverlayTooltipFlag_Reset);
            Layout &tly = GetCurrentLayout();

            if (!label.IsEmpty())
                tly.Text(tly.GenerateNextId(), label, getTextParams());
            const TKit::StackString text = TKit::StackString::Format(TKit::RuntimeFormatString(dinfo.Format), *value);
            tly.Text(tly.GenerateNextId(), text, getTextParams());

            EndTooltip();
        }

        return *value != pval;
    }

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
    WidgetStateFlags getWidgetState(const LayoutId id, const WidgetStateFlags fallback = 0)
    {
        return m_WidgetStates.TryInsert(id, fallback);
    }
    bool checkWidgetState(const LayoutId id, const WidgetStateFlags flags, const WidgetStateFlags fallback = 0)
    {
        return getWidgetState(id, fallback) & flags;
    }
    void toggleWidgetState(const LayoutId id, const WidgetStateFlags bit)
    {
        const WidgetStateFlags flags = getWidgetState(id);
        if (flags & bit)
            m_WidgetStates[id] &= ~bit;
        else
            m_WidgetStates[id] |= bit;
    }

    TKit::String m_InputWidgetBuffer{};
    u32 m_CursorStart = 0;
    u32 m_CursorEnd = 0;

    usz m_LastItem = NullLayoutId;

    f64 m_DragValue = 0.;

    TKit::TierHashMap<usz, WidgetStateFlags> m_WidgetStates{};
    TKit::TierHashMap<usz, PickerData> m_PickerMeshes{};
    TKit::TierHashMap<usz, TabBarData> m_TabBarData{};

    Color m_PickerOriginal{};

    TabBarData *m_CurrentTabBar = nullptr;

    struct TextInputStateInfo
    {
        u32 Cursor;
        TKit::String Text;
    };

    TKit::TierArray<TextInputStateInfo> m_UndoStack{};
    TKit::TierArray<TextInputStateInfo> m_RedoStack{};

    // NOTE(Isma, 07/07/26): Persistently saving a LayoutId object may be dangerous bc of the debug name stored:
    // underlying string may become stale. In practice, this is a throwaway id that gets discarded once used, so its not
    // that persistent. should be fine
    LayoutId m_TextId = NullLayoutId;

    /////////////////////////////////////////////
    /// END WIDGETS PRIVATE
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// POPUPS PRIVATE
    /////////////////////////////////////////////

    void closePopup(u32 depth);
    void requestCollapsePopups();
    f32v2 computeMouseAlignedPosition(const NativeWindow *win, const f32v2 &size, bool allowPromotions) const;

    TKit::TierArray<usz> m_PopupStack{};
    u32 m_CurrentPopupDepth = 0;
    u32 m_PopupCollapseDepth = 0;
    // so that modals only collapse manually
    u32 m_ModalCollapseDepth = 0;

    /////////////////////////////////////////////
    /// END POPUPS PRIVATE
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// TOOLTIPS PRIVATE
    /////////////////////////////////////////////

    void trashTooltip();
    void resetTooltip();

    OverlayWindow *m_Tooltip = nullptr;
    usz m_LastItemTooltipBackup = NullLayoutId;

    /////////////////////////////////////////////
    /// END TOOLTIPS PRIVATE
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// LAYOUT PRIVATE
    /////////////////////////////////////////////

    bool beginScroll(const ScrollParameterSpecs &specs);
    void endScroll();
    bool performScroll(LayoutId contentAreaId, ScrollBarInfo &sinfo, LayoutAxis axis, f32 contentPadding, bool drawBar);

    LayoutSpecs m_LayoutSpecs{};
    TKit::TierArray<usz> m_IdStack{};
    TKit::TierArray<f32> m_DisabledStack{};

    TKit::TierHashMap<usz, ScrollInfo> m_Scrollables{};
    TKit::TierArray<usz> m_ScrollStack{};

    /////////////////////////////////////////////
    /// END LAYOUT PRIVATE
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// STYLE PRIVATE
    /////////////////////////////////////////////

    OverlayStyle m_Style;
    OverlayStyle m_DefaultStyle;
    TKit::TierArray<ColorBackup> m_ColorStack{};
    TKit::TierArray<StyleBackup> m_StyleStack{};

    /////////////////////////////////////////////
    /// END STYLE PRIVATE
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// INTERACTION/INPUT PRIVATE
    /////////////////////////////////////////////

    OverlayHoverQueryFlags queryHoverStatus(const LayoutElement *elm, const f32v2 &padding) const;
    bool isElementHovered(const OverlayHoverQueryFlags qflags, const OverlayHoveredFlags flags = 0)
    {
        return (qflags & ~flags) == OverlayHoverQueryFlag_Hovered;
    }
    bool isElementHovered(const LayoutElement *elm, OverlayHoveredFlags flags = 0, const f32v2 &padding = f32v2{0.f});

    OverlayFocusQueryFlags queryAndSetFocusStatus(const LayoutElement *elm, FocusFlags flags = 0,
                                                  const f32v2 &padding = f32v2{0.f});
    InputConvertInfoFlags mustConvertToInputBox(InputConvertInfoFlags flags = 0);

    usz m_HoveredId = NullLayoutId;
    usz m_PressedId = NullLayoutId;
    usz m_DraggedId = NullLayoutId;
    usz m_DragDropId = NullLayoutId;
    usz m_ActiveId = NullLayoutId;
    usz m_ActiveIdLastFrame = NullLayoutId;

    usz m_HoveredWidgetCandidate = NullLayoutId;
    const Layout *m_HoveredLayoutCandidate = nullptr;
    TKit::Clock m_WidgetHoverClock{};

    OverlayDragDropPayload m_DragDropPayload{};
    OverlayDragDropFlags m_DragDropFlags = 0;

    /////////////////////////////////////////////
    /// END INTERACTION/INPUT PRIVATE
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// RENDERING PRIVATE
    /////////////////////////////////////////////

    Camera<D2> m_Camera;
    TKit::FixedArray<DynamicMeshInfo<D2>, 3 * 32> m_DynamicMeshes{};
    u32 m_DynamicMeshIndex = 0;

    /////////////////////////////////////////////
    /// END RENDERING PRIVATE
    /////////////////////////////////////////////

    /////////////////////////////////////////////
    /// HELPERS PRIVATE
    /////////////////////////////////////////////

    const FontData &getFontData() const;
    f32 getLineHeight() const;
    bool isAutoResize() const;

    f32v4 getWorldEffectiveBorders() const;

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

    LyTxPar getTextParams() const
    {
        return {.FillColor = m_Style[OverlayColor_Text], .FontSize = m_Style[OverlayStyle_FontSize]};
    }
    LyUnPar getUnicodeParams() const
    {
        return {.FillColor = m_Style[OverlayColor_Text], .Size = m_Style[OverlayStyle_FontSize]};
    }

    static constexpr LySz fit(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return LySz::Fit(min, max);
    }
    static constexpr LySz grow(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return LySz::Grow(min, max);
    }
    static constexpr LySz flex(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return LySz::Flex(min, max);
    }
    static constexpr LySz sabs(const f32 size)
    {
        return LySz::Absolute(size);
    }
    static constexpr LySz2 sabs(const f32v2 &size)
    {
        return LySz::Absolute(size);
    }
    static constexpr LySz snorm(const f32 size)
    {
        return LySz::Normalized(size);
    }
    static constexpr LySz2 snorm(const f32v2 &size)
    {
        return LySz::Normalized(size);
    }
    static constexpr LySz srel(const f32 size)
    {
        return LySz::Relative(size);
    }
    static constexpr LySz2 srel(const f32v2 &size)
    {
        return LySz::Relative(size);
    }
    static constexpr LyOf oabs(const f32 size)
    {
        return LyOf::Absolute(size);
    }
    static constexpr LyOf2 oabs(const f32v2 &size)
    {
        return LyOf::Absolute(size);
    }
    static constexpr LyOf onorm(const f32 size)
    {
        return LyOf::Normalized(size);
    }
    static constexpr LyOf2 onorm(const f32v2 &size)
    {
        return LyOf::Normalized(size);
    }
    static constexpr LyOf orel(const f32 size)
    {
        return LyOf::Relative(size);
    }
    static constexpr LyOf2 orel(const f32v2 &size)
    {
        return LyOf::Relative(size);
    }
    static constexpr LayoutShape rect(const f32 radius = 0.f)
    {
        return LayoutShape::Rectangle(radius);
    }
    static constexpr LayoutShape circle()
    {
        return LayoutShape::Circle();
    }
    static constexpr LayoutShape dynamic(const Resource handle)
    {
        return LayoutShape::Dynamic(handle);
    }

    /////////////////////////////////////////////
    /// END HELPERS PRIVATE
    /////////////////////////////////////////////
};
/////////////////////////////////////////////
/// END IMPLEMENTATION
/////////////////////////////////////////////
} // namespace Onyx
