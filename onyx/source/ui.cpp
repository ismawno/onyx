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

void UserInterface::drawWindowBorders()
{
    Layout &ly = m_Current->Layout;
    const Color &interaction = *m_Current->Resize.InteractionColor;
    const Color &idle = m_Colors[OverlayColor_WindowBorderIdle];

    OverlayResizeInfo &rinfo = m_Current->Resize;

    const bool l = rinfo.Flags & OverlayResizeFlag_Left;
    const bool r = rinfo.Flags & OverlayResizeFlag_Right;
    const bool b = rinfo.Flags & OverlayResizeFlag_Bottom;
    const bool t = rinfo.Flags & OverlayResizeFlag_Top;

    const vec2<LayoutSizing> sizing = LayoutSizing::Normalized(1.f);
    const vec2<LayoutSizing> hsizing = {LayoutSizing::Absolute(rinfo.BarWidth), LayoutSizing::Grow()};
    const vec2<LayoutSizing> vsizing = {LayoutSizing::Grow(), LayoutSizing::Absolute(rinfo.BarWidth)};
    const vec2<LayoutSizing> grow = LayoutSizing::Grow();

    const OverlayResizeEdge left = OverlayResizeEdge_Left;
    const OverlayResizeEdge right = OverlayResizeEdge_Right;
    const OverlayResizeEdge bottom = OverlayResizeEdge_Bottom;
    const OverlayResizeEdge top = OverlayResizeEdge_Top;

    const LayoutFloatingParameters fparams = {.Enable = true, .DrawOnTop = false};

    const auto drawLeftBorder = [&] {
        ly.BeginPanel(
            "Left border",
            LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight, .Sizing = sizing, .Floating = fparams});
        rinfo.Ids[left] =
            ly.Panel("Left", LayoutPanelParameters{.FillColor = l ? interaction : idle, .Sizing = hsizing});
        ly.EndPanel();
    };
    const auto drawRightBorder = [&] {
        ly.BeginPanel(
            "Right border",
            LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight, .Sizing = sizing, .Floating = fparams});
        ly.Panel(LayoutPanelParameters{.Sizing = grow});
        rinfo.Ids[right] =
            ly.Panel("Right", LayoutPanelParameters{.FillColor = r ? interaction : idle, .Sizing = hsizing});
        ly.EndPanel();
    };
    const auto drawBottomBorder = [&] {
        ly.BeginPanel(
            "Bottom border",
            LayoutPanelParameters{.Direction = LayoutDirection_BottomToTop, .Sizing = sizing, .Floating = fparams});
        rinfo.Ids[bottom] =
            ly.Panel("Bottom", LayoutPanelParameters{.FillColor = b ? interaction : idle, .Sizing = vsizing});
        ly.EndPanel();
    };
    const auto drawTopBorder = [&] {
        ly.BeginPanel(
            "Top border",
            LayoutPanelParameters{.Direction = LayoutDirection_BottomToTop, .Sizing = sizing, .Floating = fparams});
        ly.Panel(LayoutPanelParameters{.Sizing = grow});
        rinfo.Ids[top] = ly.Panel("Top", LayoutPanelParameters{.FillColor = t ? interaction : idle, .Sizing = vsizing});
        ly.EndPanel();
    };

    for (u32 pass = 0; pass < 2; ++pass)
    {
        const bool wantHovered = pass == 1;
        if (l == wantHovered)
            drawLeftBorder();
        if (r == wantHovered)
            drawRightBorder();
        if (b == wantHovered)
            drawBottomBorder();
        if (t == wantHovered)
            drawTopBorder();
    }
}

void UserInterface::drawWindowScrollBar()
{
    Layout &ly = m_Current->Layout;
    const LayoutElement *contentArea = ly.QueryElement("Content area");
    if (!contentArea)
        return;

    const f32 size = contentArea->Size[1];
    const f32 csize = contentArea->ChildSize[1];

    ScrollBarInfo &sinfo = m_Current->ScrollBar;
    if (csize > size)
    {
        const Color *col = &m_Colors[OverlayColor_ScrollBarIdle];

        const LayoutElement *scrollBar = ly.QueryElement("Scroll bar");
        if (scrollBar)
        {
            const ScrollBarInputInfo iinfo = getScrollBarInputInfo(scrollBar, sinfo.Pressed);

            m_Current->Flags |= iinfo.FlagsToAdd;
            sinfo.Pressed = iinfo.Pressed;

            if (iinfo.Pressed)
            {
                col = &m_Colors[OverlayColor_ScrollBarPressed];
                sinfo.CursorOffset += m_MouseDelta[1];
            }
            else
                sinfo.CursorOffset = sinfo.BarOffset; // this indirectly saves the WheelOffset state

            if (!iinfo.Pressed && iinfo.Hovered)
                col = &m_Colors[OverlayColor_ScrollBarHovered];

            const f32 barSize = scrollBar->Size[1]; // could just query the scroll bar as well and get its Size
            const f32 maxOffset = size - barSize;

            const f32 unbounded = sinfo.CursorOffset + sinfo.WheelOffset;
            sinfo.BarOffset = Math::Clamp(unbounded, -maxOffset, 0.f);

            const f32 margin = 24.f;
            sinfo.ElementOffset = (csize + margin) * sinfo.BarOffset / size;
        }

        ly.Panel("Scroll bar",
                 LayoutPanelParameters{
                     .FillColor = *col,
                     .Sizing = {LayoutSizing::Absolute(m_ScrollBarWidth), LayoutSizing::Normalized(size / csize)},
                     .SelfOffset = {LayoutOffset::Absolute(0.f), LayoutOffset::Absolute(sinfo.BarOffset)},
                     .Shape = LayoutShape::Rectangle(m_ScrollBarWidth)});
    }
    else
    {
        sinfo.BarOffset = 0.f;
        sinfo.CursorOffset = 0.f;
    }
    sinfo.WheelOffset = 0.f;
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
    const vec2<Alignment> topLeft = {Alignment_Left, Alignment_Top};
    m_Current->Id = ly.BeginPanel(
        id, LayoutPanelParameters{.FillColor = collapsed ? m_Colors[OverlayColor_WindowBackgroundCollapsed]
                                                         : m_Colors[OverlayColor_WindowBackgroundExpanded],
                                  .Direction = LayoutDirection_TopToBottom,
                                  .Alignment = topLeft,
                                  .Sizing = sizing,
                                  .SelfOffset = LayoutOffset::Absolute(m_Current->Position),
                                  .Padding = m_WindowPadding,
                                  .ChildGap = 8.f});

    drawWindowBorders();
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

    ly.BeginPanel("Scroll area", LayoutPanelParameters{.Direction = LayoutDirection_RightToLeft,
                                                       .Alignment = topLeft,
                                                       .Sizing = LayoutSizing::Grow(),
                                                       .ChildGap = 4.f});
    // must pass the id bc at this point, querying plainly with "Scroll area" will mix with the actual "Scroll area"
    // panel, giving a different id
    drawWindowScrollBar();
    ly.BeginPanel("Content area",
                  LayoutPanelParameters{.Direction = LayoutDirection_TopToBottom,
                                        .Alignment = topLeft,
                                        .Sizing = LayoutSizing::Grow(),
                                        .ChildOffset = {LayoutOffset::Absolute(0.f),
                                                        LayoutOffset::Absolute(-m_Current->ScrollBar.ElementOffset)},
                                        .Padding = 4.f,
                                        .ChildGap = 8.f});
    return !collapsed;
}

void UserInterface::EndWindow()
{
    TKIT_ASSERT(m_Current, "[ONYX][Overlay] Cannot end a window without having started one");
    m_Current->Layout.EndPanel();
    m_Current->Layout.EndPanel();
    m_Current->Layout.EndPanel();
    m_Current = nullptr;
}

// TODO(Isma): Too much repetition between this and Button()
bool UserInterface::collapseButton()
{
    Layout &ly = m_Current->Layout;
    const ButtonInputInfo info = getButtonInputInfo("Collapse button");
    m_Current->Flags |= info.FlagsToAdd;

    const Color *col = &Color_Transparent;
    if (info.Pressed)
        col = &m_Colors[OverlayColor_ButtonPressed];

    ly.BeginPanel("Collapse button",
                  LayoutPanelParameters{.FillColor = *col,
                                        .Alignment = Alignment_Center,
                                        .Sizing = {LayoutSizing::Fit(20.f, TKIT_F32_MAX, 0.f), LayoutSizing::Fit(0.f)},
                                        .Padding = 0.f});

    ly.Unicode(m_Current->HeaderIcon, LayoutUnicodeParameters{.FillColor = m_Colors[OverlayColor_WindowHeader],
                                                              .Size = m_FontSize,
                                                              .Offset = m_TextOffset});

    ly.EndPanel();
    return info.Clicked;
}

ButtonInputInfo UserInterface::getButtonInputInfo(const TKit::StringView label) const
{
    Layout &ly = m_Current->Layout;
    const bool canFocus = (m_Current->Flags & OverlayWindowFlag_Hovered) && !m_Grabbed;
    const bool hovered = canFocus && ly.IsHovered(label, m_MousePos);
    const bool clicked = hovered && (m_Current->Flags & OverlayWindowFlag_MouseReleased);
    const bool pressed = hovered && m_Window->IsMousePressed(Mouse_Button1);

    ButtonInputInfo info;
    info.Clicked = clicked;
    info.Pressed = pressed;
    info.Hovered = hovered;
    info.FlagsToAdd = hovered * OverlayWindowFlag_HoveringWidget;
    return info;
}

ScrollBarInputInfo UserInterface::getScrollBarInputInfo(const LayoutElement *elm, const bool wasPressed) const
{
    ScrollBarInputInfo info;
    if (wasPressed)
    {
        info.Pressed = m_Window->IsMousePressed(Mouse_Button1);
        info.Hovered = true; // doesnt matter
        info.FlagsToAdd = OverlayWindowFlag_HoveringWidget;
    }
    else
    {
        const bool canFocus = (m_Current->Flags & OverlayWindowFlag_Hovered) && !m_Grabbed;
        const bool hovered = canFocus && elm->IsHovered(m_MousePos);
        const bool pressed = hovered && m_Window->IsMousePressed(Mouse_Button1);

        info.Pressed = pressed;
        info.Hovered = hovered;
        info.FlagsToAdd = hovered * OverlayWindowFlag_HoveringWidget;
    }
    return info;
}

bool UserInterface::Button(const TKit::StringView label)
{
    Layout &ly = m_Current->Layout;
    const ButtonInputInfo info = getButtonInputInfo(label);
    m_Current->Flags |= info.FlagsToAdd;

    const Color *col = &m_Colors[OverlayColor_ButtonIdle];
    if (info.Pressed)
        col = &m_Colors[OverlayColor_ButtonPressed];
    else if (info.Hovered)
        col = &m_Colors[OverlayColor_ButtonHovered];

    ly.BeginPanel(
        label, LayoutPanelParameters{
                   .FillColor = *col, .Alignment = Alignment_Center, .Sizing = LayoutSizing::Fit(0.f), .Padding = 8.f});

    ly.Text(label, LayoutTextParameters{
                       .FillColor = m_Colors[OverlayColor_ButtonText], .FontSize = m_FontSize, .Offset = m_TextOffset});
    ly.EndPanel();
    return info.Clicked;
}

void UserInterface::Draw()
{
    processWindows();
    m_Context->Flush();
    for (OverlayWindow &win : m_OverlayWindows)
    {
        win.Layout.Compile();
        m_Context->Layout(win.Layout);
    }
}

template <typename F> void UserInterface::iterateReverseWindows(const F func)
{
    for (u32 i = m_OverlayWindows.GetSize() - 1; i < m_OverlayWindows.GetSize(); --i)
        if (!func(m_OverlayWindows[i]))
            return;
}

void UserInterface::processWindows()
{
    const f32v2 mpos = getMousePos();
    m_MouseDelta = mpos - m_MousePos;
    m_MousePos = mpos;

    // if nothing is grabbed, we check resize hovers here
    if (!m_Grabbed)
    {
        MouseCursor cursor = MouseCursor_Default;
        iterateReverseWindows([&](OverlayWindow &win) {
            // if hovering a widget or window is not hovered (mouse is not on window) remove any hovering and skip
            if (win.CheckFlags(OverlayWindowFlag_HoveringWidget) || !win.CheckFlags(OverlayWindowFlag_Hovered))
            {
                win.Resize.Flags = 0;
                return true;
            }
            // else, check if there is resize hover

            OverlayResizeFlags rflags = 0;
            OverlayResizeInfo &rinfo = win.Resize;
            for (u32 i = 0; i < rinfo.Ids.GetSize(); ++i)
            {
                const usz id = rinfo.Ids[i];
                if (win.Layout.IsHovered(id, m_MousePos, m_BorderHoverPadding))
                {
                    win.Resize.InteractionColor = &m_Colors[OverlayColor_WindowBorderHovered];
                    rflags |= 1U << i;
                }
            }
            rinfo.Flags = rflags;

            const auto cf = [&](const OverlayResizeFlags f) { return f & rflags; };
            if ((cf(OverlayResizeFlag_Left) && cf(OverlayResizeFlag_Bottom)) ||
                (cf(OverlayResizeFlag_Right) && cf(OverlayResizeFlag_Top)))
                cursor = MouseCursor_NESW;

            else if ((cf(OverlayResizeFlag_Right) && cf(OverlayResizeFlag_Bottom)) ||
                     (cf(OverlayResizeFlag_Left) && cf(OverlayResizeFlag_Top)))
                cursor = MouseCursor_NWSE;

            else if (cf(OverlayResizeFlag_Left | OverlayResizeFlag_Right))
                cursor = MouseCursor_EW;
            else if (cf(OverlayResizeFlag_Bottom | OverlayResizeFlag_Top))
                cursor = MouseCursor_NS;

            return true;
        });
        m_Window->SetMouseCursor(cursor);
    }

    // remove some state flags and check whether the window is collapsed
    for (OverlayWindow &win : m_OverlayWindows)
    {
        const bool collapsed = Math::Approximately(win.Size[1], win.MinSize[1], 1.f);
        win.HeaderIcon = collapsed ? m_CollapsedHeaderIcon : m_ExpandedHeaderIcon;
        win.RemoveFlags(OverlayWindowFlag_MousePressed | OverlayWindowFlag_MouseReleased | OverlayWindowFlag_Hovered);
    };

    // check for mouse events
    OverlayWindowFlags wflags = 0;
    f32 scroll = 0.f;
    for (const Event &ev : m_Window->GetNewEvents())
    {
        wflags |= OverlayWindowFlag_MousePressed * (ev.Type == Event_MousePressed) +
                  OverlayWindowFlag_MouseReleased * (ev.Type == Event_MouseReleased);
        if (ev.Type == Event_Scrolled)
        {
            wflags |= OverlayWindowFlag_Scrolled;
            scroll = m_ScrollSensitivity * ev.ScrollOffset[1];
        }
    }

    bool canAssignHover = true;

    u32 idx = m_OverlayWindows.GetSize();
    // go through the windows to 1. assign the mouse events to the uppermost hovered window and 2. assign such window as
    // grabbed if user pressed the mouse
    iterateReverseWindows([&](OverlayWindow &win) {
        --idx;
        const bool widgHovering = win.CheckFlags(OverlayWindowFlag_HoveringWidget);
        const bool winHovered = canAssignHover && win.Layout.IsHovered(win.Id, m_MousePos, m_BorderHoverPadding);
        const bool pressed = wflags & OverlayWindowFlag_MousePressed;

        win.RemoveFlags(OverlayWindowFlag_HoveringWidget);
        win.AddFlags(wflags & OverlayWindowFlag_MouseReleased);
        if (winHovered)
        {
            win.AddFlags(OverlayWindowFlag_Hovered);
            win.ScrollBar.WheelOffset += scroll;
            canAssignHover = false;
        }

        if (pressed && (win.Resize.Flags != 0 || winHovered))
        {
            if (!widgHovering)
            {
                win.Flags = wflags;
                win.Resize.Position = win.Position;
                win.Resize.Size = win.Size;
                m_Grabbed = &m_OverlayWindows.GetBack();
            }
            bringWindowToTop(idx);
            // because we just moved the current window to the front (back of the array)
            return false;
        }
        return true;
    });

    // now just handle grabbing, which is straightforward
    if (!m_Window->IsMousePressed(Mouse_Button1))
        m_Grabbed = nullptr;
    else if (m_Grabbed)
    {
        m_Grabbed->Resize.InteractionColor = &m_Colors[OverlayColor_WindowBorderPressed];
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

void UserInterface::bringWindowToTop(const u32 idx)
{
    const OverlayWindow target = std::move(m_OverlayWindows[idx]);
    m_OverlayWindows.RemoveOrdered(m_OverlayWindows.begin() + idx);
    m_OverlayWindows.Append(target);

    const usz id = m_WindowIds[idx];
    m_WindowIds.RemoveOrdered(m_WindowIds.begin() + idx);
    m_WindowIds.Append(id);
}

OverlayWindow *UserInterface::getOrCreateOverlayWindow(const usz id)
{
    for (u32 i = 0; i < m_WindowIds.GetSize(); ++i)
        if (id == m_WindowIds[i])
            return &m_OverlayWindows[i];

    m_WindowIds.Append(id);
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
