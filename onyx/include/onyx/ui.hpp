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
using OverlayResizeRectFlags = u8;
enum OverlayResizeRectFlagBit : OverlayResizeRectFlags
{
    OverlayResizeRectFlag_Left = 1U << 0,
    OverlayResizeRectFlag_Right = 1U << 1,
    OverlayResizeRectFlag_Bottom = 1U << 2,
    OverlayResizeRectFlag_Top = 1U << 3,
};

struct OverlayResizeInfo
{
    TKit::FixedArray<usz, OverlayResizeEdge_Count> Ids{NullLayoutId, NullLayoutId, NullLayoutId, NullLayoutId};
    const Color *Color = nullptr;
    f32v2 Position;
    f32v2 Size;
    f32 BarWidth = 4.f;
    OverlayResizeRectFlags Flags = 0;
};

using OverlayWindowFlags = u8;
enum OverlayWindowFlagBit : OverlayWindowFlags
{
    OverlayWindowFlag_Collapsed = 1U << 0,
    OverlayWindowFlag_Pressed = 1U << 1,
};

struct OverlayWindow
{
    OverlayWindow(const LayoutSpecs &spc) : Layout(spc)
    {
    }

    usz Id = NullLayoutId;
    OverlayResizeInfo Resize{};

    Layout Layout;
    f32v2 Position{0.f};
    f32v2 Size{240.f};
    f32v2 MinSize;
    OverlayWindowFlags Flags = 0;
};

enum OverlayColor : u8
{
    OverlayColor_WindowBackground,
    OverlayColor_WindowHeaderBackground,
    OverlayColor_WindowHeader,

    OverlayColor_WindowResizeHovered,
    OverlayColor_WindowResizePressed,

    OverlayColor_ButtonIdle,
    OverlayColor_ButtonHovered,
    OverlayColor_ButtonPressed,
    OverlayColor_ButtonText,

    OverlayColor_Count,
};

struct OverlayColors
{
    Color WindowBackground;
    Color WindowHeaderBackground;
    Color WindowHeader;

    Color WindowResizeHovered;
    Color WindowResizePressed;

    Color ButtonIdle;
    Color ButtonHovered;
    Color ButtonPressed;
    Color ButtonText;
};

struct OverlayColorRegistry
{
    OverlayColorRegistry()
        : Named{
              .WindowBackground = Color::FromHexadecimal("2A3F5F"),
              .WindowHeaderBackground = Color::FromHexadecimal("344E6E"),
              .WindowHeader = Color::FromHexadecimal("E2E8F0"),
              .WindowResizeHovered = Color::FromHexadecimal("4A5568"),
              .WindowResizePressed = Color::FromHexadecimal("718096"),
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

struct UserInterfaceSpecs
{
    LayoutSpecs Layout{.RootAlignment = {Alignment_Left, Alignment_Top}};
};

// TODO(Isma): Add ui specs and color array
class UserInterface
{
    TKIT_NON_COPYABLE(UserInterface)

  public:
    UserInterface(Window *win, const LayoutSpecs &layoutSpecs = {.RootAlignment = {Alignment_Left, Alignment_Top}});

    // TODO(Isma): Should return id
    bool BeginWindow(TKit::StringView title);
    void EndWindow();

    bool Button(TKit::StringView label);

    void Draw();

  private:
    f32v2 getMousePos() const;
    void processEvents();

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
    u32 m_HeaderIcon = 0x25BC;
    vec2<LayoutOffset> m_TextOffset = LayoutOffset::Absolute(f32v2{0.f, 2.f});

    // TODO(Isma): Should be a hash map
    TKit::TierArray<OverlayWindow> m_OverlayWindows{};
    TKit::TierArray<usz> m_LayoutIds{};
};
} // namespace Onyx
