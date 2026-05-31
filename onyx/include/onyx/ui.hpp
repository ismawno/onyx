#pragma once

#include "onyx/layout.hpp"
#include "onyx/context.hpp"
#include "onyx/window.hpp"

namespace Onyx
{
enum UIResizeEdge : u8
{
    UIResizeEdge_Left,
    UIResizeEdge_Right,
    UIResizeEdge_Bottom,
    UIResizeEdge_Top,
    UIResizeEdge_Count
};
using UIResizeRectFlags = u8;
enum UIResizeRectFlagBit : UIResizeRectFlags
{
    UIResizeRectFlag_Left = 1U << 0,
    UIResizeRectFlag_Right = 1U << 1,
    UIResizeRectFlag_Bottom = 1U << 2,
    UIResizeRectFlag_Top = 1U << 3,
};

struct UIResizeInfo
{
    TKit::FixedArray<usz, UIResizeEdge_Count> Ids{NullLayoutId, NullLayoutId, NullLayoutId, NullLayoutId};
    Color Color = Color::FromHexadecimal("7288AE");
    f32v2 Position{0.f};
    f32v2 Size{240.f};
    UIResizeRectFlags Flags = 0;
};

struct UIWindow
{
    UIWindow(const LayoutSpecs &spc) : Layout(spc)
    {
    }

    usz Id = NullLayoutId;
    UIResizeInfo Resize{};

    Layout Layout;
    f32v2 Position{0.f};
    f32v2 Size{240.f};
    f32v2 MinSize{40.f};
};

// TODO(Isma): Add ui specs and color array
class UserInterface
{
    TKIT_NON_COPYABLE(UserInterface)

  public:
    UserInterface(Window *win, const LayoutSpecs &layoutSpecs = {});

    // TODO(Isma): Should return id
    void BeginWindow(TKit::StringView title);
    void EndWindow();

    void Draw();

  private:
    f32v2 getMousePos() const;
    void processEvents();

    // TODO(Isma): Replace with hash map [] operator
    UIWindow *getOrCreateLayoutWindow(usz id);

    LayoutSpecs m_LayoutSpecs{};
    Window *m_Window;
    RenderView<D2> *m_View;
    Camera<D2> m_Camera;
    RenderContext<D2> *m_Context;

    UIWindow *m_Current = nullptr;
    UIWindow *m_Grabbed = nullptr;

    f32v2 m_MousePos{0.f};
    f32v2 m_MouseDelta{0.f};

    // TODO(Isma): Should be a hash map
    TKit::TierArray<UIWindow> m_LayoutWindows{};
    TKit::TierArray<usz> m_LayoutIds{};
};
} // namespace Onyx
