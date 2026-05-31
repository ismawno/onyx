#include "pch.hpp"
#include "onyx/ui.hpp"
#include "onyx/onyx.hpp"

namespace Onyx
{
UserInterface::UserInterface(Window *win, const LayoutSpecs &layoutSpecs) : m_LayoutSpecs(layoutSpecs), m_Window(win)
{
    m_Camera.Mode = CameraMode_Viewport;

    m_View = win->CreateRenderView<D2>(&m_Camera, RenderViewFlag_NormalizedCoordinates | RenderViewFlag_PostProcess |
                                                      RenderViewFlag_Outlines | RenderViewFlag_Transparency);
    m_View->ClearColor.rgba[3] = 0.f;

    m_Context = CreateRenderContext<D2>();

    m_Context->AddTarget(m_View);
}

void UserInterface::BeginWindow(const TKit::StringView title)
{
    TKIT_ASSERT(!m_Current, "[ONYX][UI] Cannot begin a new window when another one is being processed");

    const usz id = TKit::Hash(title);
    m_Current = getOrCreateLayout(id);

    if (m_Window->IsMousePressed(Mouse_Button1) && m_Current->Layout.IsHovered(id, m_MousePos))
        m_Current->Position += m_MouseDelta;

    Layout &ly = m_Current->Layout;
    ly.BeginPanel(id, LayoutPanelParameters{.FillColor = Color_Orange,
                                            .Direction = LayoutDirection_TopToBottom,
                                            .Alignment = {Alignment_Left, Alignment_Top},
                                            .Sizing = LayoutSizing::Absolute(240.f),
                                            .SelfOffset = {LayoutOffset::Absolute(m_Current->Position[0]),
                                                           LayoutOffset::Absolute(m_Current->Position[1])},
                                            .Padding = 2.f});
    ly.BeginPanel();
    ly.EndPanel();
    ly.BeginPanel(LayoutPanelParameters{.FillColor = Color_Green,
                                        .Alignment = {Alignment_Left, Alignment_Top},
                                        .Sizing = {LayoutSizing::Grow(), LayoutSizing::Fit()},
                                        .Padding = 4.f});
    ly.Text(title, LayoutTextParameters{.FontSize = 16.f,
                                        .Offset = {LayoutOffset::Absolute(0.f), LayoutOffset::Absolute(2.f)}});
    ly.EndPanel();
}
void UserInterface::EndWindow()
{
    TKIT_ASSERT(m_Current, "[ONYX][UI] Cannot end a window without having started one");
    m_Current->Layout.EndPanel();
    m_Current = nullptr;
}

void UserInterface::Draw()
{
    processEvents();
    m_Context->Flush();
    for (LayoutData &ldata : m_Layouts)
    {
        ldata.Layout.Compile();
        m_Context->Layout(ldata.Layout);
    }
}

void UserInterface::processEvents()
{
    const f32v2 mpos = getMousePos();
    m_MouseDelta = mpos - m_MousePos;
    m_MousePos = mpos;

    m_Flags &= ~UserInterfaceFlag_MousePressed;
    for (const Event &ev : m_Window->GetNewEvents())
        m_Flags |= (ev.Type == Event_MousePressed) * UserInterfaceFlag_MousePressed;
}

LayoutData *UserInterface::getOrCreateLayout(const u64 id)
{
    for (u32 i = 0; i < m_LayoutIds.GetSize(); ++i)
        if (id == m_LayoutIds[i])
            return &m_Layouts[i];

    m_LayoutIds.Append(id);
    return &m_Layouts.Append(m_LayoutSpecs);
}

f32v2 UserInterface::getMousePos() const
{
    return m_View->ScreenToWorld(m_Window->GetNormalizedMousePosition());
}

} // namespace Onyx
