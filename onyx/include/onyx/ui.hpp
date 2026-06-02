#pragma once

#include "onyx/layout.hpp"
#include "onyx/context.hpp"
#include "onyx/window.hpp"

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
enum OverlayResizeFlagBit : OverlayResizeFlags
{
    OverlayResizeFlag_Left = 1U << 0,
    OverlayResizeFlag_Right = 1U << 1,
    OverlayResizeFlag_Bottom = 1U << 2,
    OverlayResizeFlag_Top = 1U << 3,
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
    bool Pressed = false;
};

using OverlayWindowFlags = u8;
enum OverlayWindowFlagBit : OverlayWindowFlags
{
    OverlayWindowFlag_Collapsed = 1U << 0,
    OverlayWindowFlag_MousePressed = 1U << 1,
    OverlayWindowFlag_MouseReleased = 1U << 2,
    OverlayWindowFlag_HoveringWidget = 1U << 3,
    OverlayWindowFlag_Hovered = 1U << 4,
    OverlayWindowFlag_Scrolled = 1U << 5,
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
    OverlayColor_WindowBackgroundExpanded,
    OverlayColor_WindowBackgroundCollapsed,

    OverlayColor_WindowHeaderBackgroundExpanded,
    OverlayColor_WindowHeaderBackgroundCollapsed,
    OverlayColor_WindowHeader,

    OverlayColor_WindowBorderIdle,
    OverlayColor_WindowBorderHovered,
    OverlayColor_WindowBorderPressed,

    OverlayColor_ScrollBarIdle,
    OverlayColor_ScrollBarHovered,
    OverlayColor_ScrollBarPressed,

    OverlayColor_ButtonIdle,
    OverlayColor_ButtonHovered,
    OverlayColor_ButtonPressed,
    OverlayColor_ButtonText,

    OverlayColor_Count,
};

struct OverlayColors
{
    Color WindowBackgroundExpanded;
    Color WindowBackgroundCollapsed;

    Color WindowHeaderBackgroundExpanded;
    Color WindowHeaderBackgroundCollapsed;
    Color WindowHeader;

    Color WindowBorderIdle;
    Color WindowBorderHovered;
    Color WindowBorderPressed;

    Color ScrollBarIdle;
    Color ScrollBarHovered;
    Color ScrollBarPressed;

    Color ButtonIdle;
    Color ButtonHovered;
    Color ButtonPressed;
    Color ButtonText;
};

struct OverlayColorRegistry
{
    OverlayColorRegistry()
        : Named{
              .WindowBackgroundExpanded = Color::FromHexadecimal("2A3F5F"),
              .WindowBackgroundCollapsed = Color::FromHexadecimal("1E2D45D9"),

              .WindowHeaderBackgroundExpanded = Color::FromHexadecimal("344E6E"),
              .WindowHeaderBackgroundCollapsed = Color::FromHexadecimal("2A3F5FD9"),
              .WindowHeader = Color::FromHexadecimal("E2E8F0"),

              .WindowBorderIdle = Color::FromHexadecimal("2D3748"),
              .WindowBorderHovered = Color::FromHexadecimal("4A5568"),
              .WindowBorderPressed = Color::FromHexadecimal("718096"),

              .ScrollBarIdle = Color::FromHexadecimal("3A4F6F"),
              .ScrollBarHovered = Color::FromHexadecimal("5A7A9E"),
              .ScrollBarPressed = Color::FromHexadecimal("63B3ED"),

              .ButtonIdle = Color::FromHexadecimal("2D3748"),
              .ButtonHovered = Color::FromHexadecimal("4A5568"),
              .ButtonPressed = Color::FromHexadecimal("63B3ED"),
              .ButtonText = Color::FromHexadecimal("E2E8F0"),
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

struct ButtonInputInfo
{
    bool Clicked;
    bool Pressed;
    bool Hovered;
    OverlayWindowFlags FlagsToAdd;
};

struct ScrollBarInputInfo
{
    bool Pressed;
    bool Hovered;
    OverlayWindowFlags FlagsToAdd;
};

struct UserInterfaceSpecs
{
    LayoutSpecs Layout{.RootAlignment = {Alignment_Left, Alignment_Top}};
    OverlayColorRegistry Colors{};
};

// TODO(Isma): Add ui specs and color array
class UserInterface
{
    TKIT_NON_COPYABLE(UserInterface)

  public:
    UserInterface(Window *win, const UserInterfaceSpecs &specs = {});

    // TODO(Isma): Should return id
    bool BeginWindow(TKit::StringView title);
    void EndWindow();

    // TODO(Isma): Implement slider
    bool Button(TKit::StringView label);

    void Draw();

  private:
    // TODO(Isma): Standardize this a bit more. Maybe a prameter struct
    bool collapseButton();
    template <typename F> void iterateReverseWindows(F func);

    f32v2 getMousePos() const;
    void processWindows();
    void bringWindowToTop(u32 idx);
    void drawWindowBorders();
    void drawWindowScrollBar();

    ButtonInputInfo getButtonInputInfo(const TKit::StringView label) const;
    ScrollBarInputInfo getScrollBarInputInfo(const LayoutElement *elm, bool wasPressed) const;

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

    OverlayColorRegistry m_Colors{};

    f32 m_FontSize = 16.f;
    f32 m_WindowPadding = 8.f;
    f32 m_HeaderPadding = 4.f;
    f32 m_ScrollBarWidth = 8.f;
    f32 m_BorderHoverPadding = 8.f;
    f32 m_ScrollSensitivity = 8.f;

    CodePoint m_ExpandedHeaderIcon = 0x25BC;
    CodePoint m_CollapsedHeaderIcon = 0x25B6;

    vec2<LayoutOffset> m_TextOffset = LayoutOffset::Absolute(f32v2{0.f, 2.f});

    // TODO(Isma): Should be a hash map
    TKit::TierArray<OverlayWindow> m_OverlayWindows{};
    TKit::TierArray<usz> m_WindowIds{};
};
} // namespace Onyx
