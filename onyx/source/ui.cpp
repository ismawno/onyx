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

static void drawResizes(UIWindow &uwin, const vec2<LayoutSizing> &sizing)
{
    Layout &ly = uwin.Layout;
    const Color &col = uwin.Resize.Color;
    const Color trp = Color_Transparent;

    const bool l = uwin.Resize.Flags & UIResizeRectFlag_Left;
    const bool r = uwin.Resize.Flags & UIResizeRectFlag_Right;
    const bool b = uwin.Resize.Flags & UIResizeRectFlag_Bottom;
    const bool t = uwin.Resize.Flags & UIResizeRectFlag_Top;

    const vec2<LayoutSizing> hsizing = {LayoutSizing::Absolute(4.f), LayoutSizing::Grow()};
    const vec2<LayoutSizing> vsizing = {LayoutSizing::Grow(), LayoutSizing::Absolute(4.f)};
    const vec2<LayoutSizing> grow = LayoutSizing::Grow();

    const LayoutFloatingParameters fparams = {.Enable = true, .DrawOnTop = false};
    ly.BeginPanel(
        LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight, .Sizing = sizing, .Floating = fparams});

    uwin.Resize.Ids[UIResizeEdge_Left] =
        ly.Panel("Left resize", LayoutPanelParameters{.FillColor = l ? col : trp, .Sizing = hsizing});
    ly.Panel(LayoutPanelParameters{.Sizing = grow});
    uwin.Resize.Ids[UIResizeEdge_Right] =
        ly.Panel("Right resize", LayoutPanelParameters{.FillColor = r ? col : trp, .Sizing = hsizing});

    ly.EndPanel();

    ly.BeginPanel(
        LayoutPanelParameters{.Direction = LayoutDirection_BottomToTop, .Sizing = sizing, .Floating = fparams});

    uwin.Resize.Ids[UIResizeEdge_Bottom] =
        ly.Panel("Bottom resize", LayoutPanelParameters{.FillColor = b ? col : trp, .Sizing = vsizing});
    ly.Panel(LayoutPanelParameters{.Sizing = grow});
    uwin.Resize.Ids[UIResizeEdge_Top] =
        ly.Panel("Top resize", LayoutPanelParameters{.FillColor = t ? col : trp, .Sizing = vsizing});

    ly.EndPanel();
}

void UserInterface::BeginWindow(const TKit::StringView title)
{
    TKIT_ASSERT(!m_Current, "[ONYX][UI] Cannot begin a new window when another one is being processed");

    const usz id = TKit::Hash(title);
    m_Current = getOrCreateLayoutWindow(id);

    Layout &ly = m_Current->Layout;

    const vec2<LayoutSizing> sizing = LayoutSizing::Absolute(m_Current->Size);
    m_Current->Id = ly.BeginPanel(id, LayoutPanelParameters{.FillColor = Color::FromHexadecimal("4B5694"),
                                                            .Alignment = {Alignment_Left, Alignment_Top},
                                                            .Sizing = sizing,
                                                            .SelfOffset = LayoutOffset::Absolute(m_Current->Position)});

    drawResizes(*m_Current, sizing);
    ly.BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_TopToBottom,
                                        .Alignment = {Alignment_Left, Alignment_Top},
                                        .Sizing = sizing,
                                        .Floating = {.Enable = true, .DrawOnTop = false},
                                        .Padding = 4.f});

    ly.BeginPanel(LayoutPanelParameters{.FillColor = Color::FromHexadecimal("7288AE"),
                                        .Alignment = {Alignment_Left, Alignment_Top},
                                        .Sizing = {LayoutSizing::Grow(), LayoutSizing::Fit()},
                                        .Padding = 4.f,
                                        .ChildGap = 8.f});

    const vec2<LayoutOffset> offset = {LayoutOffset::Absolute(0.f), LayoutOffset::Absolute(2.f)};

    ly.Unicode(0x25BC,
               LayoutUnicodeParameters{.FillColor = Color::FromHexadecimal("EAE0CF"), .Size = 16.f, .Offset = offset});
    ly.Text(title,
            LayoutTextParameters{.FillColor = Color::FromHexadecimal("EAE0CF"), .FontSize = 16.f, .Offset = offset});

    ly.EndPanel();
}
void UserInterface::EndWindow()
{
    TKIT_ASSERT(m_Current, "[ONYX][UI] Cannot end a window without having started one");
    m_Current->Layout.EndPanel();
    m_Current->Layout.EndPanel();
    m_Current = nullptr;
}

void UserInterface::Draw()
{
    processEvents();
    m_Context->Flush();
    for (UIWindow &uwin : m_LayoutWindows)
    {
        uwin.Layout.Compile();
        m_Context->Layout(uwin.Layout);
    }
}

void UserInterface::processEvents()
{
    const f32v2 mpos = getMousePos();
    m_MouseDelta = mpos - m_MousePos;
    m_MousePos = mpos;

    bool mpressed = false;
    for (const Event &ev : m_Window->GetNewEvents())
        mpressed |= (ev.Type == Event_MousePressed);

    for (UIWindow &uwin : m_LayoutWindows)
    {
        if (m_Grabbed == &uwin)
            continue;
        UIResizeInfo &rinfo = uwin.Resize;
        UIResizeRectFlags flags = 0;
        for (u32 i = 0; i < rinfo.Ids.GetSize(); ++i)
        {
            const usz id = rinfo.Ids[i];
            if (uwin.Layout.IsHovered(id, m_MousePos, 16.f))
            {
                uwin.Resize.Color = Color::FromHexadecimal("7288AE");
                flags |= 1U << i;
            }
        }
        const auto cf = [&flags](const UIResizeRectFlags f) { return f & flags; };

        // TODO(Isma): Handle resize and grab resize
        rinfo.Flags = flags;
        if (flags == 0)
            m_Window->SetMouseCursor(MouseCursor_Default);

        else if ((cf(UIResizeRectFlag_Left) && cf(UIResizeRectFlag_Bottom)) ||
                 (cf(UIResizeRectFlag_Right) && cf(UIResizeRectFlag_Top)))
            m_Window->SetMouseCursor(MouseCursor_NESW);

        else if ((cf(UIResizeRectFlag_Right) && cf(UIResizeRectFlag_Bottom)) ||
                 (cf(UIResizeRectFlag_Left) && cf(UIResizeRectFlag_Top)))
            m_Window->SetMouseCursor(MouseCursor_NWSE);

        else if (flags & (UIResizeRectFlag_Left | UIResizeRectFlag_Right))
            m_Window->SetMouseCursor(MouseCursor_EW);
        else
            m_Window->SetMouseCursor(MouseCursor_NS);
    }

    if (mpressed)
    {
        for (UIWindow &uwin : m_LayoutWindows)
            if (uwin.Resize.Flags != 0 || uwin.Layout.IsHovered(uwin.Id, m_MousePos))
            {
                m_Grabbed = &uwin;
                break;
            }
    }
    else if (!m_Window->IsMousePressed(Mouse_Button1))
        m_Grabbed = nullptr;
    else if (m_Grabbed)
    {
        UIResizeInfo &rinfo = m_Grabbed->Resize;
        f32v2 &p = rinfo.Position;
        f32v2 &s = rinfo.Size;
        const f32v2 &ms = m_Grabbed->MinSize;

        const f32v2 &md = m_MouseDelta;

        const auto handleResizeAxis = [&](const u32 idx, const UIResizeRectFlags canonical,
                                          const UIResizeRectFlags mirrored) {
            if (rinfo.Flags & canonical)
            {
                s[idx] -= md[idx];
                p[idx] += md[idx];
            }
            else if (rinfo.Flags & mirrored)
                s[idx] += md[idx];
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
            handleResizeAxis(0, UIResizeRectFlag_Left, UIResizeRectFlag_Right);
            handleResizeAxis(1, UIResizeRectFlag_Bottom, UIResizeRectFlag_Top);
        }
    }
}

UIWindow *UserInterface::getOrCreateLayoutWindow(const u64 id)
{
    for (u32 i = 0; i < m_LayoutIds.GetSize(); ++i)
        if (id == m_LayoutIds[i])
            return &m_LayoutWindows[i];

    m_LayoutIds.Append(id);
    return &m_LayoutWindows.Append(m_LayoutSpecs);
}

f32v2 UserInterface::getMousePos() const
{
    return m_View->ScreenToWorld(m_Window->GetNormalizedMousePosition());
}

} // namespace Onyx
