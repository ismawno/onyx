#include "pch.hpp"
#include "onyx/ui.hpp"
#include "onyx/onyx.hpp"

namespace Onyx
{
UserInterface::UserInterface(Window *win, const UserInterfaceSpecs &specs)
    : m_LayoutSpecs(specs.Layout), m_Window(win), m_Colors(specs.Colors)
{
    m_Camera.Mode = CameraMode_Viewport;
    m_View = win->CreateRenderView<D2>(&m_Camera, RenderViewFlag_NormalizedCoordinates | RenderViewFlag_PostProcess |
                                                      RenderViewFlag_Outlines | RenderViewFlag_Transparency);
    m_View->ClearColor.rgba[3] = 0.f;

    m_Context = CreateRenderContext<D2>();
    m_Context->AddTarget(m_View);
}

static void drawResizes(OverlayWindow &win, const vec2<LayoutSizing> &sizing)
{
    Layout &ly = win.Layout;
    const Color &col = *win.Resize.Color;
    const Color trp = Color_Transparent;

    OverlayResizeInfo &rinfo = win.Resize;

    const bool l = rinfo.Flags & OverlayResizeFlag_Left;
    const bool r = rinfo.Flags & OverlayResizeFlag_Right;
    const bool b = rinfo.Flags & OverlayResizeFlag_Bottom;
    const bool t = rinfo.Flags & OverlayResizeFlag_Top;

    const vec2<LayoutSizing> hsizing = {LayoutSizing::Absolute(rinfo.BarWidth), LayoutSizing::Grow()};
    const vec2<LayoutSizing> vsizing = {LayoutSizing::Grow(), LayoutSizing::Absolute(rinfo.BarWidth)};
    const vec2<LayoutSizing> grow = LayoutSizing::Grow();

    const OverlayResizeEdge left = OverlayResizeEdge_Left;
    const OverlayResizeEdge right = OverlayResizeEdge_Right;
    const OverlayResizeEdge bottom = OverlayResizeEdge_Bottom;
    const OverlayResizeEdge top = OverlayResizeEdge_Top;

    const LayoutFloatingParameters fparams = {.Enable = true, .DrawOnTop = false};
    ly.BeginPanel(
        LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight, .Sizing = sizing, .Floating = fparams});

    rinfo.Ids[left] = ly.Panel("Left resize", LayoutPanelParameters{.FillColor = l ? col : trp, .Sizing = hsizing});
    ly.Panel(LayoutPanelParameters{.Sizing = grow});
    rinfo.Ids[right] = ly.Panel("Right resize", LayoutPanelParameters{.FillColor = r ? col : trp, .Sizing = hsizing});

    ly.EndPanel();

    ly.BeginPanel(
        LayoutPanelParameters{.Direction = LayoutDirection_BottomToTop, .Sizing = sizing, .Floating = fparams});

    rinfo.Ids[bottom] = ly.Panel("Bottom resize", LayoutPanelParameters{.FillColor = b ? col : trp, .Sizing = vsizing});
    ly.Panel(LayoutPanelParameters{.Sizing = grow});
    rinfo.Ids[top] = ly.Panel("Top resize", LayoutPanelParameters{.FillColor = t ? col : trp, .Sizing = vsizing});

    ly.EndPanel();
}

bool UserInterface::BeginWindow(const TKit::StringView title)
{
    TKIT_ASSERT(!m_Current, "[ONYX][Overlay] Cannot begin a new window when another one is being processed");

    const usz id = TKit::Hash(title);
    m_Current = getOrCreateOverlayWindow(id);

    m_Current->MinSize = computeWindowMinSize(m_WindowPadding, m_HeaderPadding, m_FontSize);

    Layout &ly = m_Current->Layout;
    const bool collapsed = m_Current->HeaderIcon == m_CollapsedHeaderIcon;

    const vec2<LayoutSizing> sizing = LayoutSizing::Absolute(m_Current->Size);
    m_Current->Id = ly.BeginPanel(
        id, LayoutPanelParameters{.FillColor = collapsed ? m_Colors[OverlayColor_WindowBackgroundCollapsed]
                                                         : m_Colors[OverlayColor_WindowBackgroundExpanded],
                                  .Direction = LayoutDirection_TopToBottom,
                                  .Alignment = {Alignment_Left, Alignment_Top},
                                  .Sizing = sizing,
                                  .SelfOffset = LayoutOffset::Absolute(m_Current->Position),
                                  .Padding = m_WindowPadding,
                                  .ChildGap = 8.f});

    drawResizes(*m_Current, sizing);
    ly.BeginPanel("Header",
                  LayoutPanelParameters{.FillColor = collapsed ? m_Colors[OverlayColor_WindowHeaderBackgroundCollapsed]
                                                               : m_Colors[OverlayColor_WindowHeaderBackgroundExpanded],
                                        .Alignment = {Alignment_Left, Alignment_Center},
                                        .Sizing = {LayoutSizing::Grow(), LayoutSizing::Fit(0.f)},
                                        .Padding = m_HeaderPadding,
                                        .ChildGap = 8.f});

    if (collapseButton())
    {
        if (collapsed)
            m_Current->Size[1] = m_Current->LastHeight;
        else
        {
            m_Current->LastHeight = m_Current->Size[1];
            m_Current->Size[1] = m_Current->MinSize[1];
        }
    }

    ly.Text(title, LayoutTextParameters{.FillColor = m_Colors[OverlayColor_WindowHeader],
                                        .FontSize = m_FontSize,
                                        .Offset = m_TextOffset});

    ly.EndPanel();
    ly.BeginPanel("Content area", LayoutPanelParameters{.Direction = LayoutDirection_TopToBottom,
                                                        .Alignment = {Alignment_Left, Alignment_Top},
                                                        .Sizing = LayoutSizing::Grow(),
                                                        .Padding = 4.f,
                                                        .ChildGap = 8.f});
    return true;
}

bool UserInterface::collapseButton()
{
    Layout &ly = m_Current->Layout;
    const bool hovered = ly.IsHovered("Collapse button", m_MousePos);
    const bool clicked = m_Current->Flags & OverlayWindowFlag_MouseReleased;
    const bool pressed = m_Window->IsMousePressed(Mouse_Button1);

    m_Current->Flags |= hovered * OverlayWindowFlag_HoveringWidget;

    const Color *col = &Color_Transparent;
    if (pressed && hovered)
        col = &m_Colors[OverlayColor_ButtonPressed];
    // else if (hovered)
    //     col = &m_Colors[OverlayColor_ButtonHovered];

    ly.BeginPanel("Collapse button",
                  LayoutPanelParameters{.FillColor = *col,
                                        .Alignment = Alignment_Center,
                                        .Sizing = {LayoutSizing::Fit(20.f, TKIT_F32_MAX, 0.f), LayoutSizing::Fit(0.f)},
                                        .Padding = 0.f});

    ly.Unicode(m_Current->HeaderIcon, LayoutUnicodeParameters{.FillColor = m_Colors[OverlayColor_WindowHeader],
                                                              .Size = m_FontSize,
                                                              .Offset = m_TextOffset});

    ly.EndPanel();
    return hovered && clicked;
}

bool UserInterface::Button(const TKit::StringView label)
{
    Layout &ly = m_Current->Layout;
    const bool hovered = ly.IsHovered(label, m_MousePos);
    const bool clicked = m_Current->Flags & OverlayWindowFlag_MouseReleased;
    const bool pressed = m_Window->IsMousePressed(Mouse_Button1);

    m_Current->Flags |= hovered * OverlayWindowFlag_HoveringWidget;

    const Color *col = &m_Colors[OverlayColor_ButtonIdle];
    if (pressed && hovered)
        col = &m_Colors[OverlayColor_ButtonPressed];
    else if (hovered)
        col = &m_Colors[OverlayColor_ButtonHovered];

    ly.BeginPanel(
        label, LayoutPanelParameters{
                   .FillColor = *col, .Alignment = Alignment_Center, .Sizing = LayoutSizing::Fit(0.f), .Padding = 8.f});

    ly.Text(label, LayoutTextParameters{
                       .FillColor = m_Colors[OverlayColor_ButtonText], .FontSize = m_FontSize, .Offset = m_TextOffset});
    ly.EndPanel();
    return hovered && clicked;
}

void UserInterface::EndWindow()
{
    TKIT_ASSERT(m_Current, "[ONYX][Overlay] Cannot end a window without having started one");
    m_Current->Layout.EndPanel();
    m_Current->Layout.EndPanel();
    m_Current = nullptr;
}

void UserInterface::Draw()
{
    processEvents();
    m_Context->Flush();
    for (OverlayWindow &win : m_OverlayWindows)
    {
        win.Layout.Compile();
        m_Context->Layout(win.Layout);
    }
}

void UserInterface::processEvents()
{
    const f32v2 mpos = getMousePos();
    m_MouseDelta = mpos - m_MousePos;
    m_MousePos = mpos;

    for (OverlayWindow &win : m_OverlayWindows)
    {
        const bool collapsed = Math::Approximately(win.Size[1], win.MinSize[1], 0.5f);
        win.HeaderIcon = collapsed ? m_CollapsedHeaderIcon : m_ExpandedHeaderIcon;

        win.RemoveFlags(OverlayWindowFlag_MousePressed | OverlayWindowFlag_MouseReleased);
        if (m_Grabbed == &win)
        {
            win.Resize.Color = &m_Colors[OverlayColor_WindowResizePressed];
            continue;
        }
        if (win.Flags & OverlayWindowFlag_HoveringWidget)
            continue;

        OverlayResizeInfo &rinfo = win.Resize;
        OverlayResizeFlags rflags = 0;
        for (u32 i = 0; i < rinfo.Ids.GetSize(); ++i)
        {
            const usz id = rinfo.Ids[i];
            if (win.Layout.IsHovered(id, m_MousePos, 16.f))
            {
                win.Resize.Color = &m_Colors[OverlayColor_WindowResizeHovered];
                rflags |= 1U << i;
            }
        }
        const auto cf = [&rflags](const OverlayResizeFlags f) { return f & rflags; };

        // TODO(Isma): Handle resize and grab resize
        rinfo.Flags = rflags;
        if (rflags == 0)
            m_Window->SetMouseCursor(MouseCursor_Default);

        else if ((cf(OverlayResizeFlag_Left) && cf(OverlayResizeFlag_Bottom)) ||
                 (cf(OverlayResizeFlag_Right) && cf(OverlayResizeFlag_Top)))
            m_Window->SetMouseCursor(MouseCursor_NESW);

        else if ((cf(OverlayResizeFlag_Right) && cf(OverlayResizeFlag_Bottom)) ||
                 (cf(OverlayResizeFlag_Left) && cf(OverlayResizeFlag_Top)))
            m_Window->SetMouseCursor(MouseCursor_NWSE);

        else if (cf(OverlayResizeFlag_Left | OverlayResizeFlag_Right))
            m_Window->SetMouseCursor(MouseCursor_EW);
        else
            m_Window->SetMouseCursor(MouseCursor_NS);
    }

    OverlayWindowFlags wflags = 0;
    for (const Event &ev : m_Window->GetNewEvents())
        wflags |= OverlayWindowFlag_MousePressed * (ev.Type == Event_MousePressed) +
                  OverlayWindowFlag_MouseReleased * (ev.Type == Event_MouseReleased);

    for (OverlayWindow &win : m_OverlayWindows)
    {
        const bool hovering = win.CheckFlags(OverlayWindowFlag_HoveringWidget);
        win.Flags = wflags;
        const bool pressed = win.CheckFlags(OverlayWindowFlag_MousePressed);
        if (pressed && !hovering && (win.Resize.Flags != 0 || win.Layout.IsHovered(win.Id, m_MousePos)))
        {
            win.Resize.Position = win.Position;
            win.Resize.Size = win.Size;
            m_Grabbed = &win;
            break;
        }
    }

    if (!m_Window->IsMousePressed(Mouse_Button1))
        m_Grabbed = nullptr;
    else if (m_Grabbed)
    {
        OverlayResizeInfo &rinfo = m_Grabbed->Resize;
        f32v2 &p = rinfo.Position;
        f32v2 &s = rinfo.Size;
        const f32v2 &ms = m_Grabbed->MinSize;

        const f32v2 &md = m_MouseDelta;

        const auto handleResizeAxis = [&](const u32 idx, const OverlayResizeFlags canonical,
                                          const OverlayResizeFlags mirrored, const f32 sign = 1.f) {
            const f32 step = md[idx];
            if (rinfo.Flags & canonical)
            {
                s[idx] -= sign * step;
                p[idx] += step;
            }
            else if (rinfo.Flags & mirrored)
                s[idx] += sign * step;

            if (s[idx] >= ms[idx])
            {
                m_Grabbed->Position[idx] = p[idx];
                m_Grabbed->Size[idx] = s[idx];
            }
        };

        if (rinfo.Flags == 0)
        {
            p += md;
            m_Grabbed->Position = p;
        }
        else
        {
            handleResizeAxis(0, OverlayResizeFlag_Left, OverlayResizeFlag_Right);
            handleResizeAxis(1, OverlayResizeFlag_Top, OverlayResizeFlag_Bottom, -1.f);
        }
    }
}

OverlayWindow *UserInterface::getOrCreateOverlayWindow(const u64 id)
{
    for (u32 i = 0; i < m_LayoutIds.GetSize(); ++i)
        if (id == m_LayoutIds[i])
            return &m_OverlayWindows[i];

    m_LayoutIds.Append(id);
    OverlayWindow &win = m_OverlayWindows.Append(m_LayoutSpecs);
    win.HeaderIcon = m_ExpandedHeaderIcon;
    return &win;
}

f32 UserInterface::computeWindowMinSize(const f32 winPadding, const f32 headPadding, const f32 fontSize) const
{
    const Resource font = m_Context->GetState().Font;
    const FontData &fdata = Resources::GetFontData(font);

    return fontSize * fdata.LineHeight + 2.f * (winPadding + headPadding);
}

f32v2 UserInterface::getMousePos() const
{
    return m_View->ScreenToWorld(m_Window->GetNormalizedMousePosition());
}

} // namespace Onyx
