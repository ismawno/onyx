#include "pch.hpp"
#include "onyx/ui.hpp"
#include "onyx/onyx.hpp"

namespace Onyx
{
enum OverlayResizeFlagBit : OverlayResizeFlags
{
    OverlayResizeFlag_Left = 1U << 0,
    OverlayResizeFlag_Right = 1U << 1,
    OverlayResizeFlag_Bottom = 1U << 2,
    OverlayResizeFlag_Top = 1U << 3,
};
enum OverlayWindowInternalFlagBit : OverlayWindowFlags
{
    OverlayWindowFlag_Collapsed = 1U << 0,
    OverlayWindowFlag_MousePressed = 1U << 1,
    OverlayWindowFlag_MouseReleased = 1U << 2,
    OverlayWindowFlag_Hovered = 1U << 3,
    OverlayWindowFlag_InputHovered = 1U << 4,
    OverlayWindowFlag_DeleteChar = 1U << 5,
    OverlayWindowFlagClear = (1U << 6) - 1U
};

UserInterface::UserInterface(Window *win, const UserInterfaceSpecs &specs)
    : Config(specs.Config), m_LayoutSpecs(specs.Layout), m_Window(win), m_Colors(specs.Colors)
{
    m_Camera.Mode = CameraMode_Viewport;
    m_View = win->CreateRenderView<D2>(&m_Camera, RenderViewFlag_NormalizedCoordinates | RenderViewFlag_Transparency);
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

    const vec2<LayoutSizing> hsizing = {sabs(Config.WindowBorderWidth), grow()};
    const vec2<LayoutSizing> vsizing = {grow(), sabs(Config.WindowBorderWidth)};

    const OverlayResizeEdge left = OverlayResizeEdge_Left;
    const OverlayResizeEdge right = OverlayResizeEdge_Right;
    const OverlayResizeEdge bottom = OverlayResizeEdge_Bottom;
    const OverlayResizeEdge top = OverlayResizeEdge_Top;

    const LayoutFloatingParameters fparams = {.Enable = true, .DrawOnTop = false};

    const auto drawLeftBorder = [&] {
        ly.BeginPanel(
            "Left border",
            LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight, .Sizing = snorm(1.f), .Floating = fparams});
        rinfo.Ids[left] =
            ly.Panel("Left", LayoutPanelParameters{.FillColor = l ? interaction : idle, .Sizing = hsizing});
        ly.EndPanel();
    };
    const auto drawRightBorder = [&] {
        ly.BeginPanel(
            "Right border",
            LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight, .Sizing = snorm(1.f), .Floating = fparams});
        ly.Panel(LayoutPanelParameters{.Sizing = grow()});
        rinfo.Ids[right] =
            ly.Panel("Right", LayoutPanelParameters{.FillColor = r ? interaction : idle, .Sizing = hsizing});
        ly.EndPanel();
    };
    const auto drawBottomBorder = [&] {
        ly.BeginPanel(
            "Bottom border",
            LayoutPanelParameters{.Direction = LayoutDirection_BottomToTop, .Sizing = snorm(1.f), .Floating = fparams});
        rinfo.Ids[bottom] =
            ly.Panel("Bottom", LayoutPanelParameters{.FillColor = b ? interaction : idle, .Sizing = vsizing});
        ly.EndPanel();
    };
    const auto drawTopBorder = [&] {
        ly.BeginPanel(
            "Top border",
            LayoutPanelParameters{.Direction = LayoutDirection_BottomToTop, .Sizing = snorm(1.f), .Floating = fparams});
        ly.Panel(LayoutPanelParameters{.Sizing = grow()});
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
    const f32 csize = contentArea->ChildrenSize[1];

    ScrollBarInfo &sinfo = m_Current->ScrollBar;
    if (csize > size)
    {
        const Color *col = &m_Colors[OverlayColor_ScrollBarIdle];

        const LayoutElement *scrollBar = ly.QueryElement("Scroll bar");
        if (scrollBar)
        {
            const DragInputInfo iinfo = getDragInputInfo(scrollBar);

            if (iinfo.Pressed)
            {
                col = &m_Colors[OverlayColor_ScrollBarPressed];
                sinfo.CursorOffset += m_MouseDelta[1];
            }
            else
                sinfo.CursorOffset = sinfo.BarOffset; // this indirectly saves the WheelOffset state

            if (!iinfo.Pressed && iinfo.Hovered)
                col = &m_Colors[OverlayColor_ScrollBarHovered];
        }
        else
            sinfo.CursorOffset = sinfo.BarOffset; // this indirectly saves the WheelOffset state

        const f32 barSize = size * size / csize;
        const f32 maxOffset = size - barSize;

        // TODO(Isma): Parametrize this
        const f32 elementFactor = (csize + Config.ScrollMargin) / size;

        const f32 unbounded = sinfo.CursorOffset + sinfo.WheelOffset / elementFactor;
        sinfo.BarOffset = Math::Clamp(unbounded, -maxOffset, 0.f);

        sinfo.ElementOffset = elementFactor * sinfo.BarOffset;

        const bool noScroll = m_Current->CheckFlags(OverlayWindowFlag_NoScrollBar);
        if (!noScroll)
            ly.Panel("Scroll bar", LayoutPanelParameters{.FillColor = *col,
                                                         .Sizing = sabs({Config.ScrollBarWidth, barSize}),
                                                         .SelfOffset = oabs({0.f, sinfo.BarOffset}),
                                                         .Shape = LayoutShape::Rectangle(Config.ScrollBarWidth)});
    }
    else
        sinfo.Reset();

    sinfo.WheelOffset = 0.f;
}

bool UserInterface::BeginWindow(const TKit::StringView title, const OverlayWindowFlags flags)
{
    TKIT_ASSERT(!m_Current, "[ONYX][Overlay] Cannot begin a new window when another one is being processed");

    const usz id = TKit::Hash(title);
    m_Current = getOrCreateOverlayWindow(id);
    m_Current->MinSize = computeWindowMinSize(Config.WindowPadding, Config.HeaderPadding, Config.FontSize);
    m_Current->Flags &= OverlayWindowFlagClear;
    m_Current->Flags |= flags;

    Layout &ly = m_Current->Layout;

    const bool noHeader = flags & OverlayWindowFlag_NoHeaderBar;
    const bool collapsed = !noHeader && m_Current->HeaderIcon == m_CollapsedHeaderIcon;
    const bool autoResize = flags & OverlayWindowFlag_AlwaysAutoResize;
    if (autoResize)
        m_Current->ScrollBar.Reset();

    // ugly
    const vec2<LayoutSizing> sizing = [&]() -> vec2<LayoutSizing> {
        if (autoResize)
            return collapsed ? vec2<LayoutSizing>{fit(), sabs(m_Current->Size[1])} : vec2<LayoutSizing>{fit()};

        return sabs(m_Current->Size);
    }();

    const vec2<Alignment> topLeft = {Alignment_Left, Alignment_Top};

    const bool noBckg = flags & OverlayWindowFlag_NoBackground;
    m_Current->Id =
        ly.BeginPanel(id, LayoutPanelParameters{.FillColor =
                                                    [&] {
                                                        if (noBckg)
                                                            return Color_Transparent;
                                                        if (collapsed)
                                                            return m_Colors[OverlayColor_WindowBackgroundCollapsed];
                                                        return m_Colors[OverlayColor_WindowBackgroundExpanded];
                                                    }(),
                                                .Direction = LayoutDirection_TopToBottom,
                                                .Alignment = topLeft,
                                                .Sizing = sizing,
                                                .SelfOffset = oabs(m_Current->Position),
                                                .Padding = Config.WindowPadding,
                                                .ChildGap = Config.ChildGap});

    drawWindowBorders();
    if (!noHeader)
    {
        ly.BeginPanel("Header", LayoutPanelParameters{
                                    .FillColor = collapsed ? m_Colors[OverlayColor_WindowHeaderBackgroundCollapsed]
                                                           : m_Colors[OverlayColor_WindowHeaderBackgroundExpanded],
                                    .Alignment = {Alignment_Left, Alignment_Center},
                                    .Sizing = {flex(), fit()},
                                    .Padding = Config.HeaderPadding,
                                    .ChildGap = Config.ChildGap});

        const bool noCollapse = flags & OverlayWindowFlag_NoCollapse;
        if (!noCollapse && collapseButton())
        {
            if (collapsed)
            {
                m_Current->Size[1] = m_Current->LastHeight;
                m_Current->ScrollBar.Reset();
            }
            else
            {
                m_Current->LastHeight = m_Current->Size[1];
                m_Current->Size[1] = m_Current->MinSize[1];
            }
        }

        ly.Text(title, getTextParams(OverlayColor_WindowHeader));

        ly.EndPanel();
    }

    ly.BeginPanel("Scroll area", LayoutPanelParameters{.Direction = LayoutDirection_RightToLeft,
                                                       .Alignment = topLeft,
                                                       .Sizing = autoResize ? fit() : grow(),
                                                       .ChildGap = 0.5f * Config.ChildGap});
    // must pass the id bc at this point, querying plainly with "Scroll area" will mix with the actual "Scroll area"
    // panel, giving a different id
    if (!collapsed && !autoResize)
        drawWindowScrollBar();
    ly.BeginPanel("Content area", LayoutPanelParameters{.Direction = LayoutDirection_TopToBottom,
                                                        .Alignment = topLeft,
                                                        .Sizing = autoResize ? fit() : grow(),
                                                        .ChildOffset = oabs({0.f, -m_Current->ScrollBar.ElementOffset}),
                                                        .Padding = 4.f,
                                                        .ChildGap = Config.ChildGap});
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

void UserInterface::HorizontalSeparator(const TKit::StringView label, const f32 textOffset, const f32 width)
{
    Layout &ly = m_Current->Layout;
    ly.BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight,
                                        .Alignment = {Alignment_Left, Alignment_Center},
                                        .Sizing = {grow(), fit()},
                                        .ChildGap = Config.ChildGap});

    ly.Panel(LayoutPanelParameters{.FillColor = m_Colors[OverlayColor_WindowHeaderBackgroundExpanded],
                                   .Sizing = sabs({textOffset, width})});
    ly.Text(label, getTextParams());
    ly.Panel(LayoutPanelParameters{.FillColor = m_Colors[OverlayColor_WindowHeaderBackgroundExpanded],
                                   .Sizing = {grow(), sabs(width)}});
    ly.EndPanel();
}

// TODO(Isma): Too much repetition between this and Button()
bool UserInterface::collapseButton()
{
    Layout &ly = m_Current->Layout;
    const ClickInputInfo info = getClickInputInfo(ly.QueryElement("Collapse button"));

    const Color *col = &Color_Transparent;
    if (info.Pressed)
        col = &m_Colors[OverlayColor_ButtonPressed];

    ly.BeginPanel("Collapse button", LayoutPanelParameters{.FillColor = *col,
                                                           .Alignment = Alignment_Center,
                                                           .Sizing = {fit(20.f, TKIT_F32_MAX), fit()}});

    ly.Unicode(m_Current->HeaderIcon, getUnicodeParams(OverlayColor_WindowHeader));

    ly.EndPanel();
    return info.Clicked;
}

ClickInputInfo UserInterface::getClickInputInfo(const LayoutElement *elm)
{
    if (!elm)
        return ClickInputInfo{};

    const bool canFocus = m_Current->CheckFlags(OverlayWindowFlag_Hovered) && !m_Grabbed;

    const bool canHover = canFocus && m_PressedDragger == NullLayoutId &&
                          (m_PressedClicker == NullLayoutId || m_PressedClicker == elm->Id);
    const bool hovered = canHover && elm->IsHovered(m_MousePos);
    const bool clicked = hovered && m_Current->CheckFlags(OverlayWindowFlag_MouseReleased);
    const bool pressed = hovered && m_Window->IsMousePressed(Mouse_Button1);

    if (pressed)
        m_PressedClicker = elm->Id;
    if (hovered)
        m_HoveredClicker = elm->Id;

    ClickInputInfo info;
    info.Clicked = clicked;
    info.Pressed = pressed;
    info.Hovered = hovered;
    return info;
}

DragInputInfo UserInterface::getDragInputInfo(const LayoutElement *elm)
{
    if (!elm)
        return DragInputInfo{};

    DragInputInfo info;
    if (m_PressedDragger == elm->Id)
    {
        info.Pressed = m_Window->IsMousePressed(Mouse_Button1);
        info.Hovered = true; // doesnt matter
        m_HoveredDragger = elm->Id;
    }
    else
    {
        const bool canFocus = m_Current->CheckFlags(OverlayWindowFlag_Hovered) && !m_Grabbed;
        const bool canHover = canFocus && m_PressedDragger == NullLayoutId && m_PressedClicker == NullLayoutId;
        const bool hovered = canHover && elm->IsHovered(m_MousePos);
        const bool pressed = hovered && m_Window->IsMousePressed(Mouse_Button1);

        info.Pressed = pressed;
        info.Hovered = hovered;
        if (pressed)
            m_PressedDragger = elm->Id;
        if (hovered)
            m_HoveredDragger = elm->Id;
    }
    return info;
}

bool UserInterface::isInputBoxFocused(const LayoutElement *elm)
{
    if (!elm)
        return false;

    const bool canFocus = m_Current->CheckFlags(OverlayWindowFlag_Hovered) && !m_Grabbed;
    const bool canHover = canFocus && m_PressedDragger == NullLayoutId && m_PressedClicker == NullLayoutId;
    const bool hovered = canHover && elm->IsHovered(m_MousePos);

    const bool pressing = m_Window->IsMousePressed(Mouse_Button1);

    if (hovered)
        m_Current->Flags |= OverlayWindowFlag_InputHovered;

    if (hovered && pressing)
        m_Current->TextHighlightSize += m_MouseDelta[0];

    if (m_FocusedInputter == elm->Id)
        return true;

    if (hovered && pressing)
    {
        m_FocusedInputter = elm->Id;
        m_Current->TextCursorPos = m_MousePos[0];
        return true;
    }
    return false;
}

bool UserInterface::Button(const TKit::StringView label)
{
    Layout &ly = m_Current->Layout;
    const ClickInputInfo info = getClickInputInfo(ly.QueryElement(label));

    const Color *col = &m_Colors[OverlayColor_ButtonIdle];
    if (info.Pressed)
        col = &m_Colors[OverlayColor_ButtonPressed];
    else if (info.Hovered)
        col = &m_Colors[OverlayColor_ButtonHovered];

    ly.BeginPanel(label, LayoutPanelParameters{
                             .FillColor = *col, .Alignment = Alignment_Center, .Sizing = fit(), .Padding = 8.f});

    ly.Text(label, getTextParams(OverlayColor_ButtonText));
    ly.EndPanel();
    return info.Clicked;
}

bool UserInterface::CheckBox(const TKit::StringView label, bool *enable)
{
    Layout &ly = m_Current->Layout;
    const ClickInputInfo info = getClickInputInfo(ly.QueryElement(label));

    const Color *col = &m_Colors[OverlayColor_CheckBoxIdle];
    if (info.Pressed)
        col = &m_Colors[OverlayColor_CheckBoxPressed];
    else if (info.Hovered)
        col = &m_Colors[OverlayColor_CheckBoxHovered];

    if (info.Clicked)
        *enable = !*enable;

    ly.BeginPanel(label, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                               .Sizing = fit(),
                                               .ChildGap = Config.ChildGap});

    ly.BeginPanel("Outer checkbox", LayoutPanelParameters{.FillColor = *col,
                                                          .Alignment = Alignment_Center,
                                                          .Sizing = sabs(Config.WidgetSize),
                                                          .Padding = 6.f});

    ly.Panel("Inner checkbox",
             LayoutPanelParameters{.FillColor = *enable ? m_Colors[OverlayColor_CheckBoxInner] : Color_Transparent,
                                   .Sizing = grow()});
    ly.EndPanel();

    ly.Text(label, getTextParams(OverlayColor_CheckBoxText));

    ly.EndPanel();
    return info.Clicked;
}

bool UserInterface::InputText(TKit::StringView label, char *buf, const u32 size)
{
    TKIT_ASSERT(size != 0, "[ONYX][UI] Buffer size for text input cannot be zero");
    Layout &ly = m_Current->Layout;
    ly.BeginPanel(label, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                               .Sizing = {grow(300.f), fit()},
                                               .ChildGap = Config.ChildGap});

    const u32 bufSize = u32(std::strlen(buf));
    TKIT_ASSERT(bufSize < size, "[ONYX][UI] The input character length ({}) exceeds buffer size ({})", bufSize, size);

    // lets just first store the buffer in a string for easy insertion
    TKit::StackString str;
    str.Reserve(size);
    str.Insert(str.end(), buf, buf + bufSize);

    const LayoutElement *outer = ly.QueryElement("Outer input");
    const bool focused = isInputBoxFocused(outer);
    // this is the actual input box. color selection is a bit random
    ly.BeginPanel("Outer input", LayoutPanelParameters{.FillColor = m_Colors[OverlayColor_WindowBackgroundCollapsed],
                                                       .Alignment = {Alignment_Left, Alignment_Center},
                                                       .Sizing = {snorm(0.6f), fit()},
                                                       .Padding = Config.WidgetPadding});

    ly.Text(str, getTextParams());

    bool written = false;
    // if we are not focused, we pretty much just print the text and thats it (focused just means the cursor is active
    // and awaiting input)
    if (focused)
    {
        const FontData &fdata = getFontData();
        const f32 outerPos = outer ? (outer->Position[0] + Config.WidgetPadding) : 0.f;

        f32 relCursorPos = m_Current->TextCursorPos - outerPos;
        const f32 fs = Config.FontSize;
        // overflow clicks means how many rapid succession clicks have happened without counting the first (aka, == 1 is
        // a double click)
        //
        // in general, advances are clamped to char borders, but are in pixel units as well
        if (m_OverflowClicks == 1)
        {
            f32 textAdvance = 0.f;
            u32 idx = 0;

            // this first walk is to just get the index of the word the cursor is in
            fdata.WalkText(str, [&](const u32 i, const u32, const CodePoint, const f32 w) {
                const f32 width = fs * w;
                const f32 hw = textAdvance + 0.5f * width;
                // if we reach the cursor position, we are done
                if (hw >= relCursorPos)
                    return false;

                textAdvance += width;
                idx = i + 1;
                return true;
            });

            // and from there, reach the word boundaries
            u32 wordBegin = idx;
            u32 wordEnd = idx;
            while (wordBegin > 0 && str[wordBegin - 1] != ' ')
                --wordBegin;
            while (wordEnd < str.GetSize() && str[wordEnd] != ' ')
                ++wordEnd;

            // we then walk again to get the advances (pixel positions) of both start and end
            f32 startAdvance = 0.f;
            f32 endAdvance = 0.f;
            fdata.WalkText(str, [&](const u32 i, const u32, const CodePoint, const f32 w) {
                if (i < wordBegin)
                    startAdvance += fs * w;
                if (i < wordEnd)
                    endAdvance += fs * w;
                return true;
            });

            // and then update the global positions/sizes. this is important bc all input is derived from these, so they
            // must remain consisten
            m_Current->TextCursorPos = startAdvance + outerPos;
            m_Current->TextHighlightSize = endAdvance - startAdvance;
        }
        // pretty straightforward, just grab all the text
        else if (m_OverflowClicks == 2)
        {
            m_Current->TextCursorPos = outerPos;
            m_Current->TextHighlightSize = fs * fdata.ComputeTextWidth(str);
        }

        // now it is time to, given the cursor and the text highlight size, to draw the former if there is not
        // highlight, or the latter otherwise, and then act on them
        const f32 hgh1 = relCursorPos;
        const f32 hgh2 = relCursorPos + m_Current->TextHighlightSize;

        // text highlight might be negative!! (user drags to the left)
        const bool negHsize = m_Current->TextHighlightSize < 0.f;

        // guaranteed hstartPos <= hendPos
        const f32 hstartPos = negHsize ? hgh2 : hgh1;
        const f32 hendPos = negHsize ? hgh1 : hgh2;

        // we now get the advances (pixel positions) of cursor, highlight start and highlight end, as well as the index
        // ranges (note that the cursor has a trivial range of 1 element, while highlight may have a large one)

        f32 cursorAdvance = 0.f;

        f32 hstartAdvance = 0.f;
        f32 hendAdvance = 0.f;

        // either cursorStart == cursorEnd or cursorStart + 1 == cursorEnd
        u32 cursorStart = 0;
        u32 cursorEnd = 0;

        u32 hstart = 0;
        u32 hend = 0;

        f32 textWidth = 0.f;

        fdata.WalkText(str, [&](const u32 i, const u32, const CodePoint, const f32 w) {
            const f32 width = fs * w;

            const f32 hw = textWidth + 0.5f * width;
            textWidth += width;
            if (hw < relCursorPos)
            {
                cursorStart = i;
                cursorEnd = i + 1;
                cursorAdvance = textWidth;
            }
            if (hw < hstartPos)
            {
                hstart = i + 1;
                hstartAdvance = textWidth;
            }
            if (hw < hendPos)
            {
                hend = i + 1;
                hendAdvance = textWidth;
            }

            return true;
        });

        const f32 hAdvance = hendAdvance - hstartAdvance;
        // bc of layout solving, cursor is gonna be offsetted by the text. we have to work out how much to "bring it
        // back", that is, if cursor is in front of the first char (advance == 0), we need to offset it by -textWidth.
        // same goes for highlight

        // the 0.1f is because some rounding errors clipping the cursor against the left edge of the input box
        const f32 offset = cursorAdvance - textWidth + 0.1f;

        // if there is any highlight, cursor is discarded
        const bool hasCursor = hAdvance == 0.f;
        if (hasCursor)
            ly.Panel("Cursor", LayoutPanelParameters{.FillColor = Color{m_Colors[OverlayColor_Text], 0.6f},
                                                     .Sizing = {sabs(Config.CursorWidth), grow()},
                                                     .SelfOffset = oabs({offset, 0.f})});
        else
        {
            // highlight may be negative, but we just removed that with hstartPos etc. this means that hAdvance is
            // guaranteed to be positive (which is needed, bc sizes must be). but... a positive size will grow to the
            // right, but if user is dragging to the left, the highlight must grow to the left. we counter that by
            // offsetting the offset (lol) again by the width of the highlight (hAdvance. the difference with the
            // original width is that this one is clamped to character borders)
            const f32 hoffset = negHsize ? (offset - hAdvance) : offset;
            ly.Panel("Highlight", LayoutPanelParameters{.FillColor = Color{m_Colors[OverlayColor_Pressed], 0.4f},
                                                        .Sizing = {sabs(hAdvance), grow()},
                                                        .SelfOffset = oabs({hoffset, 0.f})});
        }

        // we just then see how many spots are left and how many spots the user wants to write to. we take the min of
        // them
        const u32 spotsLeft = size - 1 - bufSize;
        const u32 spots = Math::Min(m_Current->TextInput.GetSize(), spotsLeft);

        // we take a range to remove characters by cursor or by highlight
        const u32 toRemoveBegin = hasCursor ? cursorStart : hstart;
        const u32 toRemoveEnd = hasCursor ? cursorEnd : hend;

        // and just apply it. for highlight, we want to remove even if user just tried to write
        if (toRemoveBegin != toRemoveEnd && !str.IsEmpty() &&
            (m_Current->CheckFlags(OverlayWindowFlag_DeleteChar) || (spots != 0 && !hasCursor)))
        {
            written = true;
            f32 toRemoveWidth = 0.f;
            for (u32 i = toRemoveBegin; i < toRemoveEnd; ++i)
            {
                const char c = str[i];

                const GlyphData *glyph = fdata.GetGlyph(c); // guaranteed to not be null bc we ve already written it

                toRemoveWidth += glyph->Advance;
                if (i != 0)
                    toRemoveWidth += fdata.GetKerning(str[i - 1], c);
            }

            str.RemoveOrdered(str.begin() + toRemoveBegin, str.begin() + toRemoveEnd);
            // importantly, we have to account for the text we have removed, and subtract it from the cursor pos only if
            // user uses cursor or highlight is negative (bc then the invisible cursor would be at the end of the
            // highlight, and we have to shift it)

            if (hasCursor || negHsize)
                m_Current->TextCursorPos -= fs * toRemoveWidth;

            // we can now reset the highlight
            m_Current->TextHighlightSize = 0.f;

            // this is important bc we are gonna insert now into the cursor position. the deletion might have
            // invalidated the position, so we adjust for that
            if (cursorEnd > toRemoveBegin)
                cursorEnd = toRemoveBegin;
        }

        written |= spots != 0;
        // and then insert!
        for (u32 i = 0; i < spots; ++i)
            str.Insert(str.begin() + cursorEnd, m_Current->TextInput[i]);

        // copy the new string into the buffer and we are done :)
        for (u32 i = 0; i < str.GetSize(); ++i)
            buf[i] = str[i];

        buf[str.GetSize()] = '\0';
        if (spots != 0)
            m_Current->TextCursorPos += fs * fdata.ComputeTextWidth(m_Current->TextInput);
    }

    ly.EndPanel();
    ly.Text(label, getTextParams());
    ly.EndPanel();
    return written;
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

    // if nothing is grabbed, we check mouse cursors here
    if (!m_Grabbed)
    {
        const bool hoveringWidget = m_HoveredClicker != NullLayoutId || m_HoveredDragger != NullLayoutId;
        MouseCursor cursor = MouseCursor_Default;
        iterateReverseWindows([&](OverlayWindow &win) {
            // if hovering a widget or window is not hovered (mouse is not on window) remove any hovering and skip
            const bool winHovered = win.CheckFlags(OverlayWindowFlag_Hovered);
            if (winHovered && win.CheckFlags(OverlayWindowFlag_InputHovered))
            {
                win.Resize.Flags = 0;
                cursor = MouseCursor_IBeam;
                return false;
            }
            if (!winHovered || hoveringWidget)
            {
                win.Resize.Flags = 0;
                return true;
            }
            if (win.CheckFlags(OverlayWindowFlag_NoResize | OverlayWindowFlag_AlwaysAutoResize))
                return true;
            // else, check if there is resize hover

            OverlayResizeFlags rflags = 0;
            OverlayResizeInfo &rinfo = win.Resize;
            for (u32 i = 0; i < rinfo.Ids.GetSize(); ++i)
            {
                const usz id = rinfo.Ids[i];
                if (win.Layout.IsHovered(id, m_MousePos, Config.BorderHoverPadding))
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
        win.RemoveFlags(OverlayWindowFlag_MousePressed | OverlayWindowFlag_MouseReleased | OverlayWindowFlag_Hovered |
                        OverlayWindowFlag_DeleteChar);
        win.TextInput.Clear();
    };

    // check for mouse events
    OverlayWindowFlags wflags = 0;
    f32 scroll = 0.f;
    TKit::String textInput{};
    for (const Event &ev : m_Window->GetNewEvents())
    {
        if (ev.Type == Event_MousePressed)
        {
            wflags |= OverlayWindowFlag_MousePressed;
            m_FocusedInputter = NullLayoutId;
        }
        else if (ev.Type == Event_MouseReleased)
        {
            wflags |= OverlayWindowFlag_MouseReleased;
            if (m_ClickClock.Restart().AsMilliseconds() <= Config.ClickMilliseconds)
                ++m_OverflowClicks;
        }
        else if (ev.Type == Event_Scrolled)
            scroll = Config.ScrollSensitivity * ev.ScrollOffset[1];
        else if (ev.Type == Event_CharInput)
        {
            char buf[4];
            const u32 count = EncodeUTF8(buf, ev.Character);
            textInput.Insert(textInput.end(), buf, buf + count);
        }
        else if ((ev.Type == Event_KeyPressed || ev.Type == Event_KeyRepeat) && ev.Key == Key_Backspace)
            wflags |= OverlayWindowFlag_DeleteChar;
    }

    if (m_ClickClock.GetElapsed().AsMilliseconds() > Config.ClickMilliseconds)
        m_OverflowClicks = 0;

    bool canAssignHover = true;

    u32 idx = m_OverlayWindows.GetSize();
    // go through the windows to 1. assign the mouse events to the uppermost hovered window and 2. assign such window as
    // grabbed if user pressed the mouse
    const bool pressingWidget = m_PressedClicker != NullLayoutId || m_PressedDragger != NullLayoutId;
    iterateReverseWindows([&](OverlayWindow &win) {
        --idx;
        const bool winHovered = canAssignHover && win.Layout.IsHovered(win.Id, m_MousePos, Config.BorderHoverPadding);
        const bool inputHovered = win.CheckFlags(OverlayWindowFlag_InputHovered);
        const bool pressed = wflags & OverlayWindowFlag_MousePressed;

        if (pressed)
            win.TextHighlightSize = 0.f;

        win.AddFlags((wflags & OverlayWindowFlag_MouseReleased) | (wflags & OverlayWindowFlag_DeleteChar));
        win.RemoveFlags(OverlayWindowFlag_InputHovered);
        win.TextInput = textInput;
        if (winHovered)
        {
            win.AddFlags(OverlayWindowFlag_Hovered);
            win.ScrollBar.WheelOffset += scroll;
            canAssignHover = false;
        }

        if (pressed && (win.Resize.Flags != 0 || winHovered))
        {
            const bool noFocus = win.Flags & OverlayWindowFlag_NoBringToFocus;
            if (!pressingWidget && !inputHovered)
            {
                win.Flags |= wflags;
                win.Resize.Position = win.Position;
                win.Resize.Size = win.Size;
                m_Grabbed = noFocus ? &win : &m_OverlayWindows.GetBack();
            }
            if (!noFocus)
                bringWindowToTop(idx);
            // because we just moved the current window to the front (back of the array)
            return false;
        }
        return true;
    });

    // now just handle grabbing, which is straightforward
    if (!m_Window->IsMousePressed(Mouse_Button1))
    {
        m_Grabbed = nullptr;
        m_PressedClicker = NullLayoutId;
        m_PressedDragger = NullLayoutId;
    }
    else if (m_Grabbed)
    {
        m_Grabbed->Resize.InteractionColor = &m_Colors[OverlayColor_WindowBorderPressed];
        OverlayResizeInfo &rinfo = m_Grabbed->Resize;
        f32v2 &p = rinfo.Position;
        f32v2 &s = rinfo.Size;
        const f32v2 &ms = m_Grabbed->MinSize;

        const f32v2 &md = m_MouseDelta;

        const bool noMove = m_Grabbed->Flags & OverlayWindowFlag_NoMove;
        const bool noResize = m_Grabbed->Flags & (OverlayWindowFlag_NoResize | OverlayWindowFlag_AlwaysAutoResize);

        if (rinfo.Flags == 0 && !noMove)
        {
            p += md;
            m_Grabbed->Position = p;
        }
        else if (!noResize)
        {
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
            handleResizeAxis(0, OverlayResizeFlag_Left, OverlayResizeFlag_Right);
            handleResizeAxis(1, OverlayResizeFlag_Top, OverlayResizeFlag_Bottom, -1.f);
        }
    }

    m_HoveredClicker = NullLayoutId;
    m_HoveredDragger = NullLayoutId;
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
    win.Position += m_WindowSpawnOffset;
    m_WindowSpawnOffset += Config.WindowSpawnDelta;
    return &win;
}

const FontData &UserInterface::getFontData() const
{
    const Resource font = m_Context->GetState().Font;
    return Resources::GetFontData(font);
}
f32 UserInterface::computeWindowMinSize(const f32 winPadding, const f32 headPadding, const f32 fontSize) const
{
    const FontData &fdata = getFontData();
    return fontSize * fdata.LineHeight + 2.f * (winPadding + headPadding);
}

f32v2 UserInterface::getMousePos() const
{
    return m_View->ScreenToWorld(m_Window->GetNormalizedMousePosition());
}

} // namespace Onyx
