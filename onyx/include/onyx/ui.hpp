#pragma once

#include "onyx/layout.hpp"
#include "onyx/context.hpp"
#include "onyx/window.hpp"

namespace Onyx
{
using UserInterfaceFlags = u8;
enum UserInterfaceFlagBit : UserInterfaceFlags
{
    UserInterfaceFlag_MousePressed = 1U << 0,
};

struct LayoutData
{
    LayoutData(const LayoutSpecs &spc) : Layout(spc)
    {
    }
    Layout Layout;
    f32v2 Position;
};

class UserInterface
{
    TKIT_NON_COPYABLE(UserInterface)

  public:
    UserInterface(Window *win, const LayoutSpecs &layoutSpecs = {});

    void BeginWindow(TKit::StringView title);
    void EndWindow();

    void Draw();

  private:
    f32v2 getMousePos() const;
    void processEvents();
    LayoutData *getOrCreateLayout(usz id);

    LayoutSpecs m_LayoutSpecs{};
    Window *m_Window;
    RenderView<D2> *m_View;
    Camera<D2> m_Camera;
    RenderContext<D2> *m_Context;
    LayoutData *m_Current = nullptr;

    f32v2 m_MousePos{0.f};
    f32v2 m_MouseDelta{0.f};

    TKit::TierArray<LayoutData> m_Layouts{};
    TKit::TierArray<usz> m_LayoutIds{};

    UserInterfaceFlags m_Flags = 0;
};
} // namespace Onyx
