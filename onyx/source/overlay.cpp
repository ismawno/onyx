#include "pch.hpp"
#include "onyx/overlay.hpp"
#include "onyx/onyx.hpp"

namespace Onyx
{
enum ResizeFlagBit : ResizeFlags
{
    ResizeFlag_Left = 1U << 0,
    ResizeFlag_Right = 1U << 1,
    ResizeFlag_Bottom = 1U << 2,
    ResizeFlag_Top = 1U << 3,
};
enum EventFlagBit : EventFlags
{
    EventFlag_LeftMousePressed = 1U << 0,
    EventFlag_LeftMouseReleased = 1U << 1,

    EventFlag_RightMousePressed = 1U << 2,
    EventFlag_RightMouseReleased = 1U << 3,

    EventFlag_ActiveIdMustPersist = 1U << 4,
    EventFlag_PressedIdMustPersist = 1U << 5,
    EventFlag_ActiveAllowsInteraction = 1U << 6,

    EventFlag_MustCollapsePopups = 1U << 7,
    EventFlag_FocusBlockByPopupCollapse = 1U << 8,
    EventFlag_PopupProtectionForbidden = 1U << 9,
    // we include all flags except for the active allows interaction. that one is only cleared when active id is cleared
    EventFlagPersist = EventFlag_ActiveAllowsInteraction | EventFlag_FocusBlockByPopupCollapse
};
enum WindowInternalFlagBit : OverlayWindowFlags
{
    WindowInternalFlag_Hovered = 1U << 0,
    WindowInternalFlag_Focused = 1U << 1,
    WindowInternalFlag_InputHovered = 1U << 2,
    // we clear only user flags. internal flags persist
    WindowInternalFlagPersist = (1U << 3) - 1U
};

OverlayStyleVariables CreateDefaultOverlayVariables()
{
    OverlayStyleVariables vars;
    vars[OverlayStyle_FontSize] = 14.f;
    vars[OverlayStyle_ChildGap] = 8.f;

    vars[OverlayStyle_TooltipOffset] = 12.f;
    vars[OverlayStyle_TooltipPadding] = 4.f;

    vars[OverlayStyle_WindowPadding] = 8.f;
    vars[OverlayStyle_WindowBorderWidth] = 4.f;
    vars[OverlayStyle_WindowSpawnDelta] = 32.f;

    vars[OverlayStyle_HeaderPadding] = 4.f;
    vars[OverlayStyle_IconWidth] = 20.f;

    vars[OverlayStyle_BorderHoverPadding] = 8.f;
    vars[OverlayStyle_ContentAreaPadding] = 4.f;

    vars[OverlayStyle_ScrollBarWidth] = 8.f;
    vars[OverlayStyle_ScrollBarGap] = 4.f;
    vars[OverlayStyle_ScrollSensitivity] = 64.f;

    vars[OverlayStyle_WidgetSize] = 24.f;
    vars[OverlayStyle_WidgetPadding] = 6.f;
    vars[OverlayStyle_WidgetMinimumWidth] = 300.f;
    vars[OverlayStyle_SmallButtonPadding] = 1.f;

    vars[OverlayStyle_TreeLineWidth] = 4.f;

    vars[OverlayStyle_ClickMilliseconds] = 200.f;
    vars[OverlayStyle_CursorWidth] = 2.f;

    vars[OverlayStyle_DropDownHeightSmall] = 80.f;
    vars[OverlayStyle_DropDownHeightRegular] = 200.f;

    vars[OverlayStyle_HoverDelayShort] = 0.15f;
    vars[OverlayStyle_HoverDelayNormal] = 0.40f;
    vars[OverlayStyle_HoverStationaryThreshold] = 5.f;

    vars[OverlayStyle_InputHintAlpha] = 0.4f;
    vars[OverlayStyle_InputCursorAlpha] = 0.6f;
    return vars;
}

static Color hex(const TKit::StringView h)
{
    return Color::FromHexadecimal(h);
}

OverlayPalette CreateSlateOverlayPalette()
{
    OverlayPalette palette;
    palette[OverlayPalette_Idle0] = hex("2D3748");
    palette[OverlayPalette_Idle1] = hex("3A4F6F");
    palette[OverlayPalette_Idle2] = hex("384A64");

    palette[OverlayPalette_Hovered0] = hex("4A5568");
    palette[OverlayPalette_Hovered1] = hex("5A7A9E");
    palette[OverlayPalette_Hovered2] = hex("3A4A60");
    palette[OverlayPalette_Hovered3] = hex("4E6888");

    palette[OverlayPalette_Pressed0] = hex("5A6A7E");
    palette[OverlayPalette_Pressed1] = hex("4A5A72");
    palette[OverlayPalette_Pressed2] = hex("6A7E96");

    palette[OverlayPalette_Text0] = hex("E2E8F0");
    palette[OverlayPalette_Text1] = hex("8899AA");

    palette[OverlayPalette_Inner0] = hex("4A8EC2");
    palette[OverlayPalette_Inner1] = hex("5BA0D4");

    palette[OverlayPalette_Background0] = hex("2A3F5F");
    palette[OverlayPalette_Background1] = hex("344E6E");
    palette[OverlayPalette_Background2] = hex("161F2E");

    return palette;
}

OverlayPalette CreateEmberOverlayPalette()
{
    OverlayPalette palette;
    palette[OverlayPalette_Idle0] = hex("4D3636");
    palette[OverlayPalette_Idle1] = hex("6E4A4A");
    palette[OverlayPalette_Idle2] = hex("5A4040");

    palette[OverlayPalette_Hovered0] = hex("704E4E");
    palette[OverlayPalette_Hovered1] = hex("9E6E6E");
    palette[OverlayPalette_Hovered2] = hex("604848");
    palette[OverlayPalette_Hovered3] = hex("806060");

    palette[OverlayPalette_Pressed0] = hex("8E6464");
    palette[OverlayPalette_Pressed1] = hex("7C5555");
    palette[OverlayPalette_Pressed2] = hex("A07878");

    palette[OverlayPalette_Text0] = hex("F5EDE6");
    palette[OverlayPalette_Text1] = hex("B8A498");

    palette[OverlayPalette_Inner0] = hex("D4885B");
    palette[OverlayPalette_Inner1] = hex("E09A6C");

    palette[OverlayPalette_Background0] = hex("3A2828");
    palette[OverlayPalette_Background1] = hex("4A3535");
    palette[OverlayPalette_Background2] = hex("2A1C1C");

    return palette;
}

OverlayColors CreateOverlayColorsFromPalette(const OverlayPalette &palette)
{
    OverlayColors colors;

    colors[OverlayColor_Text] = palette[OverlayPalette_Text0];
    colors[OverlayColor_Line] = palette[OverlayPalette_Background1];

    colors[OverlayColor_InputText] = palette[OverlayPalette_Text0];
    colors[OverlayColor_InputCursor] = palette[OverlayPalette_Text0];
    colors[OverlayColor_InputHighlight] = palette[OverlayPalette_Pressed0];

    colors[OverlayColor_WindowBorderIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_WindowBorderHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_WindowBorderPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_Header] = palette[OverlayPalette_Text0];

    colors[OverlayColor_ButtonIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_ButtonHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_ButtonPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_ButtonText] = palette[OverlayPalette_Text0];

    colors[OverlayColor_CheckBoxIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_CheckBoxHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_CheckBoxPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_CheckBoxText] = palette[OverlayPalette_Text0];
    colors[OverlayColor_CheckBoxInner] = palette[OverlayPalette_Inner0];

    colors[OverlayColor_SliderIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_SliderHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_SliderPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_SliderText] = palette[OverlayPalette_Text0];
    colors[OverlayColor_SliderInner] = palette[OverlayPalette_Inner0];

    colors[OverlayColor_DragIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_DragHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_DragPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_DragText] = palette[OverlayPalette_Text0];

    colors[OverlayColor_ScrollBarIdle] = palette[OverlayPalette_Idle1];
    colors[OverlayColor_ScrollBarHovered] = palette[OverlayPalette_Hovered1];
    colors[OverlayColor_ScrollBarPressed] = palette[OverlayPalette_Inner0];
    colors[OverlayColor_ScrollAreaBorders] = palette[OverlayPalette_Background1];

    colors[OverlayColor_TreeIdle] = palette[OverlayPalette_Background1];
    colors[OverlayColor_TreeHovered] = palette[OverlayPalette_Hovered2];
    colors[OverlayColor_TreePressed] = palette[OverlayPalette_Pressed1];
    colors[OverlayColor_TreeText] = palette[OverlayPalette_Text0];

    colors[OverlayColor_DropDownIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_DropDownHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_DropDownPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_DropDownText] = palette[OverlayPalette_Text0];
    colors[OverlayColor_DropDownButton] = palette[OverlayPalette_Inner0];

    colors[OverlayColor_SelectableIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_SelectableHovered] = palette[OverlayPalette_Hovered3];
    colors[OverlayColor_SelectablePressed] = palette[OverlayPalette_Pressed2];
    colors[OverlayColor_SelectableText] = palette[OverlayPalette_Text0];

    colors[OverlayColor_PopupBackground] = palette[OverlayPalette_Background2];

    colors[OverlayColor_WindowBackgroundExpanded] = palette[OverlayPalette_Background0];
    colors[OverlayColor_WindowBackgroundCollapsed] = palette[OverlayPalette_Background0];
    colors[OverlayColor_HeaderBackgroundExpanded] = palette[OverlayPalette_Background1];
    colors[OverlayColor_HeaderBackgroundCollapsed] = palette[OverlayPalette_Background1];

    return colors;
}

Overlay::Overlay(Window *win, const UserInterfaceSpecs &specs)
    : m_LayoutSpecs(specs.Layout), m_Window(win), m_Style(specs.Style), m_Tooltip(specs.Layout)
{
    TKIT_ASSERT(
        specs.Layout.RootAlignment[0] == Alignment_Left && specs.Layout.RootAlignment[1] == Alignment_Top,
        "[ONYX][OVERLAY] Root alignment for layouts must be Top Left. Other alignments are not supported for root");

    m_Camera.Mode = CameraMode_Viewport;
    m_View = win->CreateRenderView<D2>(&m_Camera, RenderViewFlag_NormalizedCoordinates | RenderViewFlag_Transparency);

    m_Context = CreateRenderContext<D2>();
    m_Context->AddTarget(m_View);
}

void Overlay::drawWindowBorders()
{
    Layout &ly = getCurrentLayout();
    const Color &interaction = *m_Current->Resize.InteractionColor;
    const Color &idle = m_Style[OverlayColor_WindowBorderIdle];

    ResizeInfo &rinfo = m_Current->Resize;

    const bool l = rinfo.Flags & ResizeFlag_Left;
    const bool r = rinfo.Flags & ResizeFlag_Right;
    const bool b = rinfo.Flags & ResizeFlag_Bottom;
    const bool t = rinfo.Flags & ResizeFlag_Top;

    const vec2<LayoutSizing> hsizing = {sabs(m_Style[OverlayStyle_WindowBorderWidth]), grow()};
    const vec2<LayoutSizing> vsizing = {grow(), sabs(m_Style[OverlayStyle_WindowBorderWidth])};

    const ResizeEdge left = ResizeEdge_Left;
    const ResizeEdge right = ResizeEdge_Right;
    const ResizeEdge bottom = ResizeEdge_Bottom;
    const ResizeEdge top = ResizeEdge_Top;

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

void Overlay::performScroll(const LayoutId contentAreaId, ScrollBarInfo &sinfo, const LayoutAxis axis,
                            const f32 contentPadding, const bool drawBar)
{
    Layout &ly = getCurrentLayout();
    const LayoutElement *contentArea = ly.QueryElement(contentAreaId);

    if (contentArea && contentArea->ChildrenSize[axis] > contentArea->Size[axis])
    {
        const f32 size = contentArea->Size[axis];
        const f32 csize = contentArea->ChildrenSize[axis] + 2.f * contentPadding;
        const Color *col = &m_Style[OverlayColor_ScrollBarIdle];

        const char *name = axis == LayoutAxis_Horizontal ? "Horizontal scroll bar" : "Vertical scroll bar";
        const LayoutId scrollId = AsStackedId(name);

        const LayoutElement *scrollBar = ly.QueryElement(scrollId);
        const f32 sign = axis == LayoutAxis_Horizontal ? -1.f : 1.f;

        const f32 sw = m_Style[OverlayStyle_ScrollBarWidth];
        const f32 mw = 2.f * sw;

        const f32 barSize = Math::Max(mw, size * size / csize);

        const f32 maxBarTravel = size - barSize;
        const f32 maxElementTravel = csize - size;
        const f32 barToElement = maxBarTravel > TKIT_F32_EPSILON ? (maxElementTravel / maxBarTravel) : 0.f;

        if (scrollBar)
        {
            const OverlayFocusQueryFlags focusFlags =
                queryAndSetFocusStatus(scrollBar, FocusFlag_PressedEvenWhenAwayFromHover);

            const bool pressed = focusFlags & OverlayFocusQueryFlag_Pressed;
            const bool hovered = focusFlags & OverlayFocusQueryFlag_Hovered;
            if (pressed || Math::Absolute(sinfo.WheelOffset) > 0.f)
            {
                if (pressed)
                    col = &m_Style[OverlayColor_ScrollBarPressed];

                const f32 wheel = barToElement > TKIT_F32_EPSILON ? (sinfo.WheelOffset / barToElement) : 0.f;
                const f32 unbounded = sinfo.CursorOffset + wheel;

                sinfo.CursorOffset += sign * m_MouseDelta[axis];
                sinfo.BarOffset = sign * Math::Clamp(unbounded, -maxBarTravel, 0.f);
                sinfo.ElementOffset = barToElement * sinfo.BarOffset;
            }
            else
            {
                sinfo.ElementOffset = sign * Math::Clamp(sign * sinfo.ElementOffset, -maxElementTravel, 0.f);
                sinfo.BarOffset = barToElement > TKIT_F32_EPSILON ? sinfo.ElementOffset / barToElement : 0.f;
                sinfo.CursorOffset = sign * sinfo.BarOffset; // this indirectly saves the WheelOffset state
            }
            if (!pressed && hovered)
                col = &m_Style[OverlayColor_ScrollBarHovered];
        }
        else
            sinfo.CursorOffset = sign * sinfo.BarOffset; // this indirectly saves the WheelOffset state

        if (drawBar)
        {
            vec2<LayoutSizing> sizing;
            sizing[axis] = sabs(barSize);
            sizing[1 - axis] = sabs(sw);

            vec2<LayoutOffset> offset;
            offset[axis] = oabs(sinfo.BarOffset);
            offset[1 - axis] = oabs(0.f);

            ly.Panel(scrollId,
                     LayoutPanelParameters{.FillColor = *col,
                                           .Sizing = sizing,
                                           .SelfOffset = offset,
                                           .Shape = LayoutShape::Rectangle(m_Style[OverlayStyle_ScrollBarWidth])});
        }
    }
    else
        sinfo = ScrollBarInfo{};

    sinfo.WheelOffset = 0.f;
}

bool Overlay::addActiveWindow(OverlayWindow *win)
{
    for (const OverlayWindow *w : m_ActiveWindows)
        if (w == win)
            return false;
    for (u32 i = 0; i < m_ActiveWindows.GetSize(); ++i)
        if (win->Layer <= m_ActiveWindows[i]->Layer)
        {
            m_ActiveWindows.Insert(m_ActiveWindows.begin() + i, win);
            return true;
        }
    m_ActiveWindows.Append(win);
    return true;
}

OverlayHoverQueryFlags Overlay::queryHoverStatus(const LayoutElement *elm) const
{
    OverlayHoverQueryFlags flags = 0;
    const usz id = elm ? elm->Id : NullLayoutId;

    const bool hovered = elm && elm->IsHovered(m_MousePos);
    const bool windowBlock =
        !m_Current->CheckFlags(WindowInternalFlag_Focused) && !m_Current->CheckFlags(WindowInternalFlag_Hovered);
    const bool grabBlock = m_Grabbed;
    const bool pressBlock = m_PressedId != NullLayoutId && m_PressedId != id;
    const bool activeBlocked =
        !(m_EventFlags & EventFlag_ActiveAllowsInteraction) && m_ActiveId != NullLayoutId && m_ActiveId != id;
    const bool popupBlocked = m_CurrentPopupDepth != m_PopupStack.GetSize();
    const bool popupCollapseBlocked = m_EventFlags & EventFlag_FocusBlockByPopupCollapse;

    flags |= OverlayHoverQueryFlag_Hovered * hovered;
    flags |= OverlayHoverQueryFlag_BlockedByWindow * windowBlock;
    flags |= OverlayHoverQueryFlag_BlockedByWindowGrab * grabBlock;
    flags |= OverlayHoverQueryFlag_BlockedByPressedItem * pressBlock;
    flags |= OverlayHoverQueryFlag_BlockedByActiveItem * activeBlocked;
    flags |= OverlayHoverQueryFlag_BlockedByPopup * popupBlocked;
    flags |= OverlayHoverQueryFlag_BlockedByPopupCollapse * popupCollapseBlocked;

    return flags;
}

bool Overlay::isElementHovered(const LayoutElement *elm, const OverlayHoveredFlags flags)
{
    const OverlayHoverQueryFlags qflags = queryHoverStatus(elm);
    const bool candidate = isElementHovered(qflags, flags);

    const bool shortDelay = flags & OverlayHoveredFlag_ShortDelay;
    const bool normalDelay = flags & OverlayHoveredFlag_NormalDelay;
    const bool stationary = flags & OverlayHoveredFlag_Stationary;
    TKIT_ASSERT(normalDelay + shortDelay != 2,
                "[ONYX][OVERLAY] Cannot have short delay and normal delay at the same time in tooltip");

    const Layout &ly = getCurrentLayout();
    if (candidate)
    {
        const f32 statThres = stationary ? m_Style[OverlayStyle_HoverStationaryThreshold] : TKIT_F32_MAX;
        if (Math::NormSquared(m_MouseDelta) > statThres)
        {
            m_WidgetHoverClock.Restart();
            return false;
        }
        if (!shortDelay && !normalDelay)
            return true;

        const f32 delay = shortDelay ? m_Style[OverlayStyle_HoverDelayShort] : m_Style[OverlayStyle_HoverDelayNormal];
        const bool wasCandidate = m_HoveredWidgetCandidate == m_LastItem;
        const bool noShared = flags & OverlayHoveredFlag_NoSharedDelay;

        m_HoveredWidgetCandidate = m_LastItem;
        m_CandidateLayout = &ly;

        if (noShared && !wasCandidate)
            m_WidgetHoverClock.Restart();

        const f32 seconds = m_WidgetHoverClock.GetElapsed().AsSeconds();
        return seconds >= delay;
    }
    return false;
}

TKit::StringView Overlay::trimLabel(const TKit::StringView label)
{
    const u32 idx = label.FindFirstOf("##");
    return label.SubString(0, idx);
}

bool Overlay::BeginWindow(const TKit::StringView title, const OverlayWindowFlags flags)
{
    const LayoutId id = title; /* forcing titles to be unique for now */ // PushId(title);
    const LayoutId nid = PushId(id);

    m_Current = getOrCreateOverlayWindow(id);
    m_Current->PopupDepth = m_CurrentPopupDepth;
    if (flags & OverlayWindowFlag_BringToTop)
        m_Current->Layer = toTop();
    if (flags & OverlayWindowFlag_Modal)
        m_ModalCollapseDepth = Math::Max(m_ModalCollapseDepth, m_CurrentPopupDepth);

    const bool noCollapse = flags & OverlayWindowFlag_NoCollapse;
    const bool noHeader = flags & OverlayWindowFlag_NoHeaderBar;
    const bool collapsed = !noCollapse && !noHeader && m_Current->HeaderIcon == ArrowRightIcon;

    m_WindowStack.Append(m_Current);
    if (!addActiveWindow(m_Current))
        // window is still active. user can still append to it
        return !collapsed;

    if (m_NextWindow.Flags & NextWindowFlag_Position)
        m_Current->Position = m_NextWindow.Position;
    if (m_NextWindow.Flags & NextWindowFlag_Size)
        m_Current->Size = m_NextWindow.Size;
    m_NextWindow.Flags = 0;

    m_Current->Flags &= WindowInternalFlagPersist;
    m_Current->MinSize = computeWindowMinSize();
    m_Current->Flags |= flags;

    Layout &ly = getCurrentLayout();

    const bool autoResize = flags & OverlayWindowFlag_AutoResize;

    const usz scrollId = AsStackedId(nid);
    ScrollInfo &sinfo = m_Scrollables[scrollId];
    if (autoResize)
        sinfo = ScrollInfo{};

    // ugly
    const vec2<LayoutSizing> sizing = [&]() -> vec2<LayoutSizing> {
        if (autoResize)
            return collapsed ? vec2<LayoutSizing>{fit(), sabs(m_Current->Size[1])} : vec2<LayoutSizing>{fit()};

        return sabs(m_Current->Size);
    }();

    const vec2<Alignment> topLeft = {Alignment_Left, Alignment_Top};

    m_Current->Id = ly.BeginPanel(
        id, LayoutPanelParameters{
                .FillColor =
                    m_Style[collapsed ? OverlayColor_WindowBackgroundCollapsed : OverlayColor_WindowBackgroundExpanded],
                .Direction = LayoutDirection_TopToBottom,
                .Alignment = topLeft,
                .Sizing = sizing,
                .SelfOffset = oabs(m_Current->Position),
                .Padding = m_Style[OverlayStyle_WindowPadding],
                .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const LayoutElement *elm = ly.QueryElement(id);
    queryAndSetFocusStatus(elm, FocusFlag_DoNotSetHoveredId | FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetActiveId);
    m_LastItem = id;

    drawWindowBorders();
    if (!noHeader)
    {
        ly.BeginPanel(
            AsStackedId("Header"),
            LayoutPanelParameters{
                .FillColor =
                    m_Style[collapsed ? OverlayColor_HeaderBackgroundCollapsed : OverlayColor_HeaderBackgroundExpanded],
                .Alignment = {Alignment_Left, Alignment_Center},
                .Sizing = {flex(), fit()},
                .Padding = m_Style[OverlayStyle_HeaderPadding],
                .ChildGap = m_Style[OverlayStyle_ChildGap]});

        if (!noCollapse && collapseButton(collapsed))
        {
            if (collapsed)
            {
                m_Current->Size[1] = m_Current->LastHeight;
                sinfo = ScrollInfo{};
            }
            else
            {
                m_Current->LastHeight = m_Current->Size[1];
                m_Current->Size[1] = m_Current->MinSize[1];
            }
        }

        LayoutTextParameters tparams = getTextParams(OverlayColor_Header);
        if (collapsed)
            tparams.FillColor.rgba[3] = 0.8f;

        ly.Text(ly.GenerateNextId(), trimLabel(title), getTextParams(OverlayColor_Header));
        ly.EndPanel();
    }

    const vec2<LayoutSizing> scrollSizing = autoResize ? flex() : grow();
    beginScroll({.Id = scrollId,
                 .OuterSizing = scrollSizing,
                 .ContentSizing = scrollSizing,
                 .ContentPadding = m_Style[OverlayStyle_ContentAreaPadding],
                 .ChildGap = m_Style[OverlayStyle_ChildGap],
                 .Flags = flags});

    if (collapsed)
    {
        EndWindow();
        return false;
    }
    return true;
}

void Overlay::EndWindow()
{
    TKIT_ASSERT(m_Current, "[ONYX][OVERLAY] Cannot end a window without having started one");
    m_WindowStack.Pop();
    m_Current = m_WindowStack.IsEmpty() ? nullptr : m_WindowStack.GetBack();
    PopId();
}

void Overlay::CloseCurrentPopup()
{
    TKIT_ASSERT(m_CurrentPopupDepth != 0,
                "[ONYX][OVERLAY] CloseCurrentPopup() can only be called inside an active popup");
    m_EventFlags |= EventFlag_PopupProtectionForbidden | EventFlag_MustCollapsePopups;
    m_PopupCollapseDepth = m_CurrentPopupDepth - 1;
    // this means only the topmost modal can be collapsed
    if (m_ModalCollapseDepth == m_CurrentPopupDepth)
        m_ModalCollapseDepth = m_PopupCollapseDepth;
}

bool Overlay::BeginPopup(const TKit::StringView title, const OverlayWindowFlags flags)
{
    // we cannot apply the id stack here because user must unequivocally reference this from anywhere
    const LayoutId id = title;
    if (m_CurrentPopupDepth == m_PopupStack.GetSize() || m_PopupStack[m_CurrentPopupDepth] != id)
    {
        m_WidgetStates[id] = 0;
        return false;
    }

    ++m_CurrentPopupDepth;
    if (getWidgetState(id) == 0)
    {
        OverlayWindow *win = getOrCreateOverlayWindow(id);
        const LayoutElement *elm = win->Layout.QueryElement(id);
        const f32v2 size = elm ? elm->Size : win->Size;

        win->Position = computeMouseAlignedPosition(size);
    }
    m_WidgetStates[id] = WidgetStateFlag_Opened;

    return BeginWindow(title, flags | OverlayWindowFlag_NoCollapse);
}

void Overlay::EndPopup()
{
    --m_CurrentPopupDepth;
    EndWindow();
}

bool Overlay::BeginDropDown(const TKit::StringView label, const TKit::StringView preview,
                            const OverlayDropDownFlags flags)
{
    Layout &ly = getCurrentLayout();
    beginHorizontalWidget(PushId(label), 1.f);

    const LayoutId id = AsStackedId("Drop down box");
    const LayoutElement *elm = ly.QueryElement(id);
    const Color *boxCol = &m_Style[OverlayColor_DropDownIdle];

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_LeftClickOpensPopup);

    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        boxCol = &m_Style[OverlayColor_DropDownPressed];
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        boxCol = &m_Style[OverlayColor_DropDownHovered];

    const bool hasPreview = !(flags & OverlayDropDownFlag_NoPreview);
    const f32 padding = m_Style[OverlayStyle_WidgetPadding];
    ly.BeginPanel(id, LayoutPanelParameters{
                          .FillColor = *boxCol,
                          .Alignment = {Alignment_Left, Alignment_Center},
                          .Sizing = {m_CurrentPopupDepth == 0 ? snorm(0.7f) : flex(),
                                     hasPreview ? fit()
                                                : sabs(getFontData().LineHeight * m_Style[OverlayStyle_FontSize] +
                                                       2.f * padding)}});

    if (hasPreview)
    {
        ly.BeginPanel(AsStackedId("Parent"), LayoutPanelParameters{.Alignment = Alignment_Center,
                                                                   .Sizing = fit(),
                                                                   .Padding = m_Style[OverlayStyle_WidgetPadding]});

        ly.Text(ly.GenerateNextId(), preview, getTextParams(OverlayColor_DropDownText));
        ly.EndPanel();
    }

    ly.Panel(AsStackedId("Push"), LayoutPanelParameters{.Sizing = grow()});

    const bool dropDownActive = focusFlags & OverlayFocusQueryFlag_PopupOpen;
    if (!(flags & OverlayDropDownFlag_NoArrowButton))
    {
        const Color *buttonCol =
            dropDownActive ? &m_Style[OverlayColor_DropDownButton] : &m_Style[OverlayColor_DropDownHovered];
        ly.BeginPanel(AsStackedId("Button box"),
                      LayoutPanelParameters{.FillColor = *buttonCol,
                                            .Alignment = Alignment_Center,
                                            .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), flex()}});
        ly.Unicode(AsStackedId("Icon"), ArrowDownIcon, getUnicodeParams(OverlayColor_DropDownText));
        ly.EndPanel();
    }

    if (dropDownActive)
    {
        endHorizontalWidget(label, OverlayColor_Text);

        const f32 size = elm ? elm->Size[0] : 0.f;
        const usz did = AsStackedId("Drop down");
        const bool tight = flags & OverlayDropDownFlag_Tight;
        ly.BeginPanel(did,
                      LayoutPanelParameters{.FillColor = m_Style[OverlayColor_PopupBackground],
                                            .Direction = LayoutDirection_TopToBottom,
                                            .Alignment = {Alignment_Left, Alignment_Top},
                                            .Sizing = {fit(size), fit()},
                                            .Floating = {.Enable = true,
                                                         .Attachment = {LayoutAttachment_Left, LayoutAttachment_Bottom},
                                                         .Alignment = {Alignment_Left, Alignment_Top}},
                                            .Padding = tight ? 0.f : m_Style[OverlayStyle_WidgetPadding]});

        const f32 height = (flags & OverlayDropDownFlag_HeightSmall) ? m_Style[OverlayStyle_DropDownHeightSmall]
                                                                     : m_Style[OverlayStyle_DropDownHeightRegular];
        const bool largest = flags & OverlayDropDownFlag_HeightLargest;

        const usz sid = AsStackedId("Scroll");
        const vec2<LayoutSizing> osizing = flex();
        const vec2<LayoutSizing> csizing = {flex(), largest ? fit() : fit(0.f, height)};
        const f32 cgap = tight ? 0.f : m_Style[OverlayStyle_ChildGap];

        ++m_CurrentPopupDepth;
        PushStyleVar(OverlayStyle_ScrollBarGap, 0.f);
        beginScroll({.Id = sid,
                     .OuterSizing = osizing,
                     .ContentSizing = csizing,
                     .ContentPadding = 0.f,
                     .ChildGap = cgap,
                     .Flags = OverlayScrollFlag_NoBackground});

        queryAndSetFocusStatus(ly.QueryElement(did), FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetActiveId);
        return true;
    }
    endHorizontalWidget(label, OverlayColor_Text);
    ly.EndPanel();
    PopId();
    return false;
}

bool Overlay::BeginScroll(const TKit::StringView label, const f32 maxHeight, const f32 maxWidth,
                          const OverlayScrollFlags flags)
{
    const usz id = PushId(label);
    const bool autoResize = m_Current->Flags & OverlayWindowFlag_AutoResize;

    const f32 padding = m_Style[OverlayStyle_ContentAreaPadding];
    const bool borders = flags & OverlayScrollFlag_Borders;

    const f32 omw = maxWidth + 2.f * padding;
    const vec2<LayoutSizing> outer = {autoResize ? fit() : grow(0.f, omw), fit()};
    const vec2<LayoutSizing> content = {autoResize ? fit(0.f, maxWidth) : grow(0.f, maxWidth), fit(0.f, maxHeight)};

    Layout &ly = getCurrentLayout();
    ly.BeginPanel(
        LayoutPanelParameters{.FillColor = borders ? m_Style[OverlayColor_ScrollAreaBorders] : Color_Transparent,
                              .Direction = LayoutDirection_TopToBottom,
                              .Alignment = {Alignment_Left, Alignment_Center},
                              .Sizing = outer,
                              .Padding = borders ? padding : 0.f});

    if (flags & OverlayScrollFlag_Title)
        ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams(OverlayColor_Text));

    return beginScroll({.Id = id,
                        .OuterSizing = outer,
                        .ContentSizing = content,
                        .ContentPadding = padding,
                        .ChildGap = m_Style[OverlayStyle_ChildGap],
                        .Flags = flags});
}

bool Overlay::beginScroll(const ScrollParameterSpecs &specs)
{
    Layout &ly = getCurrentLayout();
    ScrollInfo &sinfo = m_Scrollables[specs.Id];
    sinfo.Flags = specs.Flags;

    const bool noHeader = m_Current->Flags & OverlayWindowFlag_NoHeaderBar;
    const bool collapsed = !noHeader && m_Current->HeaderIcon == ArrowRightIcon;
    const bool drawBar = !(specs.Flags & OverlayScrollFlag_NoScrollBar);
    const vec2<Alignment> topLeft = {Alignment_Left, Alignment_Top};

    const bool borders = specs.Flags & OverlayScrollFlag_Borders;
    const f32 cpadding = m_Style[OverlayStyle_ContentAreaPadding];
    const bool bckg = !(specs.Flags & OverlayScrollFlag_NoBackground);
    m_LastItem =
        ly.BeginPanel(specs.Id, LayoutPanelParameters{.FillColor = bckg ? m_Style[OverlayColor_WindowBackgroundExpanded]
                                                                        : Color_Transparent,
                                                      .Direction = LayoutDirection_BottomToTop,
                                                      .Alignment = topLeft,
                                                      .Sizing = specs.OuterSizing,
                                                      .Padding = borders ? cpadding : 0.f,
                                                      .ChildGap = m_Style[OverlayStyle_ScrollBarGap]});

    const LayoutId contentId = AsStackedId("Content area");
    if (!collapsed && (specs.Flags & OverlayScrollFlag_HorizontalScroll))
        performScroll(contentId, sinfo.Horizontal, LayoutAxis_Horizontal, specs.ContentPadding, drawBar);

    ly.BeginPanel(AsStackedId("Vertical scroll area"),
                  LayoutPanelParameters{.Direction = LayoutDirection_RightToLeft,
                                        .Alignment = topLeft,
                                        .Sizing = specs.OuterSizing,
                                        .ChildGap = m_Style[OverlayStyle_ScrollBarGap]});

    if (!collapsed && !(specs.Flags & OverlayScrollFlag_NoVerticalScroll))
        performScroll(contentId, sinfo.Vertical, LayoutAxis_Vertical, specs.ContentPadding, drawBar);

    ly.BeginPanel(contentId, LayoutPanelParameters{
                                 .Direction = LayoutDirection_TopToBottom,
                                 .Alignment = topLeft,
                                 .Sizing = specs.ContentSizing,
                                 .ChildOffset = oabs({-sinfo.Horizontal.ElementOffset, -sinfo.Vertical.ElementOffset}),
                                 .Padding = specs.ContentPadding,
                                 .ChildGap = specs.ChildGap});

    if (isElementHovered(ly.QueryElement(specs.Id)))
    {
        m_ScrollStack.Append(specs.Id);
        return true;
    }
    return false;
}

void Overlay::endScroll()
{
    Layout &ly = getCurrentLayout();
    ly.EndPanel();
    ly.EndPanel();
    ly.EndPanel();
}

void Overlay::beginHorizontalWidget(const usz id, const f32 normSize)
{
    Layout &ly = getCurrentLayout();
    const bool autoResize = m_Current->Flags & OverlayWindowFlag_AutoResize;
    const f32 mw = m_Style[OverlayStyle_WidgetMinimumWidth];
    m_LastItem = ly.BeginPanel(id, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                         .Sizing = {autoResize ? fit(mw) : flex(mw), fit()},
                                                         .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.BeginPanel(AsStackedId("Container"),
                  LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                        .Sizing = {autoResize ? fit(mw) : snorm(normSize), fit()},
                                        .ChildGap = m_Style[OverlayStyle_ChildGap]});
}
void Overlay::endHorizontalWidget(const TKit::StringView label, const OverlayColor labelColor)
{
    Layout &ly = getCurrentLayout();
    ly.EndPanel();
    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams(labelColor));
    ly.EndPanel();
}

void Overlay::HorizontalSeparator(const TKit::StringView label, const f32 textOffset, const f32 width)
{
    Layout &ly = getCurrentLayout();
    ly.BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight,
                                        .Alignment = {Alignment_Left, Alignment_Center},
                                        .Sizing = {grow(), fit()},
                                        .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.Panel(LayoutPanelParameters{.FillColor = m_Style[OverlayColor_HeaderBackgroundExpanded],
                                   .Sizing = sabs({textOffset, width})});
    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams(OverlayColor_Text));
    ly.Panel(LayoutPanelParameters{.FillColor = m_Style[OverlayColor_HeaderBackgroundExpanded],
                                   .Sizing = {grow(), sabs(width)}});
    ly.EndPanel();
}

bool Overlay::PushTree(LayoutId id, const TKit::StringView label, const OverlayTreeFlags flags)
{
    id = PushId(id);
    Layout &ly = getCurrentLayout();
    const bool framed = flags & OverlayTreeFlag_Framed;

    const Color *col = framed ? &m_Style[OverlayColor_TreeIdle] : &Color_Transparent;

    OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = &m_Style[OverlayColor_TreePressed];
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = &m_Style[OverlayColor_TreeHovered];

    const bool horScroll = m_Current->Flags & OverlayWindowFlag_HorizontalScroll;
    const bool autoResize = m_Current->Flags & OverlayWindowFlag_AutoResize;
    const LayoutSizing growOrFlex = (horScroll || autoResize) ? flex() : grow();

    const bool spanLabel = flags & OverlayTreeFlag_SpanLabelWidth;
    const vec2<LayoutSizing> sizing = {spanLabel ? fit() : growOrFlex, fit()};

    m_LastItem = ly.BeginPanel(id, LayoutPanelParameters{.FillColor = *col,
                                                         .Alignment = {Alignment_Left, Alignment_Center},
                                                         .Sizing = sizing,
                                                         .Padding = m_Style[OverlayStyle_HeaderPadding],
                                                         .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const bool startOpen = flags & OverlayTreeFlag_StartOpen;
    const bool opened = checkWidgetState(id, WidgetStateFlag_Opened, startOpen ? WidgetStateFlag_Opened : 0);

    const usz buttonId = ly.BeginPanel(
        AsStackedId("Tree collapse"),
        LayoutPanelParameters{.Alignment = Alignment_Center, .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), fit()}});

    bool toggleOpen = focusFlags & OverlayFocusQueryFlag_LeftClicked;
    if (toggleOpen)
    {
        const bool onArrow = flags & OverlayTreeFlag_ToggleOnArrow;
        const bool onDoubleClick = !opened && (flags & OverlayTreeFlag_OpenOnDoubleClick);
        const bool doubleClicked = focusFlags & OverlayFocusQueryFlag_DoubleClicked;
        if (onArrow)
            toggleOpen = ly.IsHovered(buttonId, m_MousePos) || (onDoubleClick && doubleClicked);
        else if (onDoubleClick)
            toggleOpen = doubleClicked;
    }

    const CodePoint code = opened ? ArrowDownIcon : ArrowRightIcon;
    ly.Unicode(AsStackedId(code), code, getUnicodeParams(OverlayColor_TreeText));

    ly.EndPanel();

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams(OverlayColor_TreeText));
    ly.EndPanel();

    if (toggleOpen)
        toggleWidgetState(id, WidgetStateFlag_Opened);

    if (!opened)
    {
        PopId();
        return false;
    }

    const bool lines = flags & OverlayTreeFlag_DrawLines;
    const bool indent = !(flags & OverlayTreeFlag_NoIndent);

    const FontData &fdata = getFontData();
    const f32 fs = m_Style[OverlayStyle_FontSize];

    const f32 iconWidth = Math::Max(fs * fdata.GetGlyph(ArrowDownIcon)->Advance, m_Style[OverlayStyle_IconWidth]);
    const f32 treeIndent = iconWidth + 2.f * m_Style[OverlayStyle_HeaderPadding];

    ly.BeginPanel(LayoutPanelParameters{
        .Direction = LayoutDirection_LeftToRight, .Alignment = {Alignment_Left, Alignment_Top}, .Sizing = sizing});

    if (indent)
    {
        ly.BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_TopToBottom,
                                            .Alignment = Alignment_Center,
                                            .Sizing = {sabs(treeIndent), flex()}});

        if (lines)
            VerticalLine();

        ly.EndPanel();
    }

    ly.BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_TopToBottom,
                                        .Alignment = {Alignment_Left, Alignment_Top},
                                        .Sizing = sizing,
                                        .ChildGap = m_Style[OverlayStyle_ChildGap]});

    return true;
}

bool Overlay::inputTextBox(char *buf, const u32 size, const TKit::StringView hint, const OverlayInputFlags flags,
                           const InputConvertInfoFlags cflags)
{
    TKIT_ASSERT(size != 0, "[ONYX][OVERLAY] Buffer size for text input cannot be zero");
    Layout &ly = getCurrentLayout();
    const u32 strSize = u32(std::strlen(buf));
    TKIT_ASSERT(strSize < size, "[ONYX][OVERLAY] The input character length ({}) exceeds buffer size ({})", strSize,
                size);

    const LayoutId iboxId = AsStackedId("Input box");
    const LayoutElement *ibox = ly.QueryElement(iboxId);
    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(
        ibox, FocusFlag_ClickedOnMousePress | FocusFlag_KeepActiveOnRelease | FocusFlag_KeepActiveOnPressed |
                  FocusFlag_ActiveAllowsInteraction | FocusFlag_PressedEvenWhenAwayFromHover);

    ly.BeginPanel(iboxId, LayoutPanelParameters{.FillColor = m_Style[OverlayColor_PopupBackground],
                                                .Alignment = {Alignment_Left, Alignment_Center},
                                                .Sizing = {grow(), fit()},
                                                .Padding = m_Style[OverlayStyle_WidgetPadding]});

    const bool mustConvert = cflags & InputConvertFlag_MustConvert;

    // This input text box may be a "converted" slider/drag. this means that the first frame (where layout
    // element querying is not available) must be valid in the sense that we need valid queries. boxPos is very
    // important. without it, we cannot auto highlight. so we try this proxy, by trying to get the position of
    // the parent box if thats the case
    const LayoutElement *box = mustConvert ? ly.QueryElement(AsStackedId("Drag/Slider box")) : ibox;
    const f32 boxSize = ibox ? (ibox->Size[0] - 2.f * m_Style[OverlayStyle_WidgetPadding]) : 0.f;

    const FontData &fdata = getFontData();
    const f32 fs = m_Style[OverlayStyle_FontSize];

    LayoutTextParameters tparams = getTextParams(OverlayColor_InputText);
    const bool pressed = focusFlags & OverlayFocusQueryFlag_Pressed;
    const bool hovered = focusFlags & OverlayFocusQueryFlag_Hovered;
    if (pressed || hovered)
        m_Current->Flags |= WindowInternalFlag_InputHovered;

    bool updated = false;
    if ((focusFlags & OverlayFocusQueryFlag_Active) || mustConvert)
    {
        TKit::String &str = m_InputWidgetBuffer;
        const bool overrideHighlight = cflags & InputConvertFlag_MustOverrideHighlight;
        const bool justActive = (focusFlags & OverlayFocusQueryFlag_JustActive) || overrideHighlight;
        if (justActive)
        {
            str.Clear();
            str.Insert(str.end(), buf, buf + strSize);
        }

        const bool escapeClears = flags & OverlayInputFlag_EscapeClearsAll;

        if (escapeClears && m_EventKeys[Key_Escape])
        {
            str.Clear();
            updated = true;
            m_CursorStart = 0;
            m_CursorEnd = 0;
        }

        TKit::StackArray<f32> advances{};
        advances.Reserve(str.GetSize() + 1);
        advances.Append(0.f);

        TKit::StackArray<f32> midAdvances{};
        midAdvances.Reserve(str.GetSize() + 1);
        midAdvances.Append(0.f);

        f32 textWidth = 0.f;
        fdata.WalkText(str, [&](const u32, const u32, const CodePoint, const f32 w) {
            const f32 width = fs * w;
            // widths.Append(width);

            midAdvances.Append(textWidth + 0.5f * width);
            textWidth += width;
            advances.Append(textWidth);
            return true;
        });

        const f32 boxPos = box ? (box->Position[0] + m_Style[OverlayStyle_WidgetPadding]) : 0.f;
        const bool clicked = focusFlags & OverlayFocusQueryFlag_LeftClicked;

        const bool autoSelectAll = flags & OverlayInputFlag_AutoSelectAll;
        const u32 charCount = str.GetSize();
        const u32 advCount = charCount + 1;

        if (pressed || clicked)
        {
            const f32 onPress = m_MousePosOnPress[0] - boxPos;
            const f32 mpos = m_MousePos[0] - boxPos;

            const bool mustUseCurrent = !clicked && Math::Approximately(mpos, onPress, 1.f);
            const f32 pixelStartPos = mustUseCurrent ? advances[m_CursorStart] : onPress;
            const f32 pixelEndPos = mustUseCurrent ? advances[m_CursorEnd] : mpos;

            m_CursorStart = 0;
            m_CursorEnd = 0;

            bool startReady = false;
            bool endReady = false;
            for (u32 i = 0; i < advCount; ++i)
            {
                const f32 hw = midAdvances[i];
                startReady = hw >= pixelStartPos;
                if (!startReady)
                    m_CursorStart = i;

                endReady = hw >= pixelEndPos;
                if (!endReady)
                    m_CursorEnd = i;
            }
            if (!startReady)
                m_CursorStart = charCount;
            if (!endReady)
                m_CursorEnd = charCount;
        }

        if (m_OverflowClicks == 2 || (justActive && autoSelectAll))
        {
            m_CursorStart = 0;
            m_CursorEnd = charCount;
        }
        else if (m_OverflowClicks == 1)
        {
            if (m_CursorEnd != 0)
            {
                u32 start = m_CursorEnd - 1;
                while (start != 0 && str[start - 1] != ' ')
                    --start;
                m_CursorStart = start;
            }
            if (m_CursorEnd != charCount)
            {
                u32 end = m_CursorEnd - 1;
                while (end != charCount - 1 && str[end + 1] != ' ')
                    ++end;
                m_CursorEnd = end + 1;
            }
        }
        else if (m_EventKeys[Key_Left] || m_EventKeys[Key_Right])
        {
            const bool left = m_EventKeys[Key_Left];

            const u32 limit = left ? 0 : charCount;
            const i32 hlen = i32(m_CursorEnd) - i32(m_CursorStart);
            if (m_CursorEnd != limit)
            {
                const bool lshift = m_Window->IsKeyPressed(Key_LeftShift);
                const i32 diff = left ? -1 : 1;
                if (lshift)
                    m_CursorEnd += diff;
                else if (hlen == 0)
                {
                    m_CursorEnd += diff;
                    if (m_CursorStart != limit)
                        m_CursorStart += diff;
                }
                else if (diff * hlen > 0)
                    m_CursorEnd = m_CursorStart;
                else
                    m_CursorStart = m_CursorEnd;
            }
        }

        const bool negSel = m_CursorStart > m_CursorEnd;
        u32 selStart = negSel ? m_CursorEnd : m_CursorStart;
        u32 selEnd = negSel ? m_CursorStart : m_CursorEnd;

        const f32 selStartAdv = advances[selStart];
        const f32 selEndAdv = advances[selEnd];
        const f32 textEndAdv = advances[m_CursorEnd];

        const f32 hLength = selEndAdv - selStartAdv;
        const bool hasHighlight = hLength != 0.f;

        const bool noHorScroll = flags & OverlayInputFlag_NoHorizontalScroll;
        const f32 textOffset =
            noHorScroll ? 0.f : Math::Min(0.f, boxSize - textWidth - m_Style[OverlayStyle_CursorWidth]);

        tparams.Offset[0] = oabs(textOffset);
        const bool useHint = str.IsEmpty() && !hint.IsEmpty();
        if (useHint)
        {
            tparams.FillColor.rgba[3] = m_Style[OverlayStyle_InputHintAlpha];
            ly.Text(ly.GenerateNextId(), useHint ? hint : str, tparams);
        }
        else
            ly.Text(ly.GenerateNextId(), str, tparams);

        // bc of layout solving, cursor is gonna be offsetted by the text. we have to work out how much to "bring it
        // back", that is, if cursor is in front of the first char (advance == 0), we need to offset it by
        // -textWidth. same goes for highlight

        f32 offset = textEndAdv - textWidth + textOffset;
        if (useHint)
            offset -= fs * fdata.ComputeTextWidth(hint);
        else
            // the 0.1f is because some rounding errors clipping the cursor against the left edge of the input box
            offset += 0.1f;

        const f32 cwidth = m_Style[OverlayStyle_CursorWidth];
        ly.Panel(AsStackedId("Cursor"),
                 LayoutPanelParameters{
                     .FillColor = Color{m_Style[OverlayColor_InputCursor], m_Style[OverlayStyle_InputCursorAlpha]},
                     .Sizing = {sabs(cwidth), grow()},
                     .SelfOffset = oabs({offset, 0.f})});
        if (hasHighlight)
        {
            const f32 hoffset = negSel ? offset : (offset - hLength);
            ly.Panel(AsStackedId("Highlight"),
                     LayoutPanelParameters{.FillColor = Color{m_Style[OverlayColor_InputHighlight], 0.4f},
                                           .Sizing = {sabs(hLength), grow()},
                                           .SelfOffset = oabs({hoffset - cwidth, 0.f})});
        }

        const u32 toRemoveBegin = hasHighlight ? selStart : (selStart - 1);
        const u32 toRemoveEnd = selEnd;

        if (toRemoveBegin < toRemoveEnd && !str.IsEmpty() &&
            (m_EventKeys[Key_Backspace] || (!m_TextInput.IsEmpty() && hasHighlight)))
        {
            updated = true;
            str.RemoveOrdered(str.begin() + toRemoveBegin, str.begin() + toRemoveEnd);

            const u32 removeCount = toRemoveEnd - toRemoveBegin;
            if (!hasHighlight || negSel)
                m_CursorStart -= removeCount;
            if (!hasHighlight || !negSel)
                m_CursorEnd -= removeCount;

            selEnd = toRemoveBegin;
        }

        const u32 insertionsLeft = size - 1 - strSize;
        const u32 insertions = Math::Min(m_TextInput.GetSize(), insertionsLeft);

        updated |= insertions != 0;
        for (u32 i = 0; i < insertions; ++i)
            str.Insert(str.begin() + selEnd, m_TextInput[i]);

        m_CursorStart += insertions;
        m_CursorEnd += insertions;

        const bool enterCommits = flags & OverlayInputFlag_EnterCommitsBuffer;

        if (enterCommits)
            updated = m_EventKeys[Key_Enter];
        if (updated)
        {
            // copy the new string into the buffer and we are done :)
            for (u32 i = 0; i < str.GetSize(); ++i)
                buf[i] = str[i];

            buf[str.GetSize()] = '\0';
        }

        if (!enterCommits)
        {
            const bool enterTrue = flags & OverlayInputFlag_EnterReturnsTrue;
            if (enterTrue)
                updated = m_EventKeys[Key_Enter];
        }
    }
    else
    {
        const bool elide = flags & OverlayInputFlag_ElideLeft;
        const f32 textOffset = elide ? Math::Min(0.f, boxSize - fs * fdata.ComputeTextWidth(buf)) : 0.f;
        const bool useHint = strSize == 0 && !hint.IsEmpty();

        tparams.Offset[0] = oabs(textOffset);
        if (useHint)
        {
            tparams.FillColor.rgba[3] = m_Style[OverlayStyle_InputHintAlpha];
            ly.Text(ly.GenerateNextId(), hint, tparams);
        }
        else
            ly.Text(ly.GenerateNextId(), buf, tparams);
    }

    ly.EndPanel();

    return updated;
}

InputConvertInfoFlags Overlay::mustConvertToInputBox(const InputConvertInfoFlags flags)
{
    InputConvertInfoFlags outFlags = flags;
    const bool allowDoubleClick = flags & InputConvertFlag_AllowDoubleClick;
    const bool hovered = flags & InputConvertFlag_Hovered;

    Layout &ly = getCurrentLayout();

    const usz iboxId = AsStackedId("Input box");
    const LayoutElement *ibox = ly.QueryElement(iboxId);

    const bool ctrl = m_Window->IsKeyPressed(Key_LeftControl);
    const bool dclick = allowDoubleClick && (m_OverflowClicks == 1);

    const bool triggered = hovered && (dclick || (ctrl && checkFlags(EventFlag_LeftMousePressed)));
    const bool persisted =
        ibox && (m_ActiveId == iboxId || (m_ActiveIdLastFrame == iboxId && ibox->IsHovered(m_MousePos)));

    const bool mustConvert = (triggered || persisted) && !m_EventKeys[Key_Enter];
    if (mustConvert)
        outFlags |= InputConvertFlag_MustConvert;
    if (m_ActiveId != iboxId && m_ActiveIdLastFrame != iboxId)
        outFlags |= InputConvertFlag_MustOverrideHighlight;

    if (triggered)
        m_ActiveId = iboxId;
    return outFlags;
}

// TODO(Isma): Too much repetition between this and Button()
bool Overlay::collapseButton(const bool collapsed)
{
    Layout &ly = getCurrentLayout();

    const LayoutId id = AsStackedId("Collapse button");
    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));

    const Color *col = &Color_Transparent;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = &m_Style[OverlayColor_ButtonPressed];

    ly.BeginPanel(id, LayoutPanelParameters{.FillColor = *col,
                                            .Alignment = Alignment_Center,
                                            .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), fit()}});

    const CodePoint code = m_Current->HeaderIcon;
    LayoutUnicodeParameters uparams = getUnicodeParams(OverlayColor_Header);
    if (collapsed)
        uparams.FillColor.rgba[3] = 0.8f;
    ly.Unicode(AsStackedId(code), code, getUnicodeParams(OverlayColor_Header));

    ly.EndPanel();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

OverlayFocusQueryFlags Overlay::queryAndSetFocusStatus(const LayoutElement *elm, const FocusFlags flags)
{
    if (!elm)
        return 0;

    OverlayFocusQueryFlags outFlags = 0;
    TKIT_ASSERT(m_CurrentPopupDepth <= m_PopupStack.GetSize(),
                "[ONYX][OVERLAY] Popup depth ({}) must not be greater than popup stack ({})", m_CurrentPopupDepth,
                m_PopupStack.GetSize());

    const bool evenWhenAway = flags & FocusFlag_PressedEvenWhenAwayFromHover;

    const bool lmpressed = checkFlags(EventFlag_LeftMousePressed);
    const bool lmreleased = checkFlags(EventFlag_LeftMouseReleased);

    const OverlayHoverQueryFlags hflags = queryHoverStatus(elm);
    const bool focusHovered = isElementHovered(hflags);

    const bool setHovered = !(flags & FocusFlag_DoNotSetHoveredId);
    const bool setPressed = !(flags & FocusFlag_DoNotSetPressedId);
    const bool setActive = !(flags & FocusFlag_DoNotSetActiveId);
    const bool protectPopup =
        !(flags & FocusFlag_DoNotProtectPopup) && !(m_EventFlags & EventFlag_PopupProtectionForbidden);

    bool lclicked = focusHovered;
    // NOTE(Isma): Could add a _ClickedOnMousePress for right clicks as well
    if (flags & FocusFlag_ClickedOnMousePress)
        lclicked &= lmpressed;
    else
        lclicked &= lmreleased && m_PressedId == elm->Id;

    // we leniently allow setting hovered id even when blocked by popups, so that windows dont eat into widget hover
    // signals
    // we additionally allow grab blocks here. this is required for window popups: their own grab event would block this
    // hover check, thus disallowing the popup itself to protect itself because of its own interaction
    const OverlayHoveredFlags popHoverFlags =
        OverlayHoveredFlag_AllowBlockedByPopup | OverlayHoveredFlag_AllowBlockedByPopupCollapse |
        OverlayHoveredFlag_AllowBlockedByWindow | OverlayHoverQueryFlag_BlockedByWindowGrab;

    const bool popupHovered = isElementHovered(hflags, popHoverFlags);
    if (popupHovered && protectPopup)
        m_PopupCollapseDepth = Math::Max(m_PopupCollapseDepth, m_CurrentPopupDepth);

    if (m_CurrentPopupDepth != m_PopupStack.GetSize())
    {
        if (m_PopupStack[m_CurrentPopupDepth] == elm->Id)
            outFlags |= OverlayFocusQueryFlag_PopupOpen;

        // hover id is essentially used to stop from windows from moving/resizing when a widget is being hovered. we
        // want to allow immediate dragging when out of the popup, and close everything, except for intermediate popups.
        // those must still prevent windows from moving, because the popup hierarchy is still not completely collapsed.
        // note that we dont allow grab blocks here. we dont want windows to set hovered id
        if (setHovered && popupHovered && m_CurrentPopupDepth != 0)
            m_HoveredId = elm->Id;

        return outFlags;
    }

    const bool rclicked = focusHovered && checkFlags(EventFlag_RightMouseReleased);
    const bool pressed = (focusHovered || (evenWhenAway && m_PressedId == elm->Id)) && m_PressingLeftMouse;

    if (focusHovered && setHovered)
    {
        m_HoveredId = elm->Id;
        outFlags |= OverlayFocusQueryFlag_Hovered;
    }

    if (pressed)
    {
        if (m_ActiveId != elm->Id && m_ActiveIdLastFrame != elm->Id)
            outFlags |= OverlayFocusQueryFlag_JustActive;

        if (setActive && !(flags & FocusFlag_SetActiveOnRelease))
            m_ActiveId = elm->Id;
        if (setPressed && !(flags & FocusFlag_DoNotSetPressedId))
            m_PressedId = elm->Id;
        outFlags |= OverlayFocusQueryFlag_Pressed;
    }
    if (lclicked)
    {
        outFlags |= OverlayFocusQueryFlag_LeftClicked;
        if (m_OverflowClicks == 1)
            outFlags |= OverlayFocusQueryFlag_DoubleClicked;

        if (setActive)
        {
            if (flags & FocusFlag_ToggleActiveOnRelease)
                m_ActiveId = m_ActiveId == elm->Id ? NullLayoutId : elm->Id;
            else
                m_ActiveId = elm->Id;
        }
        if (flags & FocusFlag_LeftClickOpensPopup)
        {
            OpenPopup(elm->Id);
            outFlags |= OverlayFocusQueryFlag_PopupOpen;
        }
    }
    // no rclicked if lclicked, so that popup doesnt increase twice
    else if (rclicked)
    {
        outFlags |= OverlayFocusQueryFlag_RightClicked;
        if (flags & FocusFlag_RightClickOpensPopup)
        {
            OpenPopup(elm->Id);
            outFlags |= OverlayFocusQueryFlag_PopupOpen;
        }
    }

    if (m_PressedId == elm->Id && !lmreleased)
        m_EventFlags |= EventFlag_PressedIdMustPersist;

    if (m_ActiveId == elm->Id)
    {
        const bool unclaimOnPress = lmpressed && (!focusHovered || !(flags & FocusFlag_KeepActiveOnPressed));
        const bool unclaimOnRelease = lmreleased && !(flags & FocusFlag_KeepActiveOnRelease);

        // NOTE(Isma, 25/06/06): I dont remember what i meant by that
        // NOTE(Isma): Should allow flagging as active still?
        if (!unclaimOnPress && !unclaimOnRelease)
        {
            m_EventFlags |= EventFlag_ActiveIdMustPersist;
            outFlags |= OverlayFocusQueryFlag_Active;

            if ((flags & FocusFlag_ActiveAllowsInteraction) && !pressed)
                m_EventFlags |= EventFlag_ActiveAllowsInteraction;
            // NOTE(Isma, 25/06/06): Should consider unsetting this only on setActive bool var
            else
                m_EventFlags &= ~EventFlag_ActiveAllowsInteraction;
        }
    }

    return outFlags;
}

bool Overlay::Button(const TKit::StringView label, const OverlayButtonFlags flags)
{
    Layout &ly = getCurrentLayout();
    const LayoutId id = PushId(label);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));

    const Color *col = &m_Style[OverlayColor_ButtonIdle];
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = &m_Style[OverlayColor_ButtonPressed];
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = &m_Style[OverlayColor_ButtonHovered];

    const bool spanFull = flags & OverlayButtonFlag_SpanFullWidth;

    const vec2<LayoutSizing> sizing = spanFull ? vec2<LayoutSizing>{flex(), fit()} : vec2<LayoutSizing>{fit()};

    const bool small = flags & OverlayButtonFlag_Small;
    const f32 padding = m_Style[small ? OverlayStyle_SmallButtonPadding : OverlayStyle_WidgetPadding];
    m_LastItem = ly.BeginPanel(
        id,
        LayoutPanelParameters{.FillColor = *col, .Alignment = Alignment_Center, .Sizing = sizing, .Padding = padding});

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams(OverlayColor_ButtonText));
    ly.EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

bool Overlay::RadioButton(const TKit::StringView label, const bool active)
{
    Layout &ly = getCurrentLayout();
    const LayoutId id = PushId(label);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));

    const Color *col = &m_Style[OverlayColor_CheckBoxIdle];
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = &m_Style[OverlayColor_CheckBoxPressed];
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = &m_Style[OverlayColor_CheckBoxHovered];

    m_LastItem = ly.BeginPanel(id, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                         .Sizing = fit(),
                                                         .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.BeginPanel(AsStackedId("Outer radio"), LayoutPanelParameters{.FillColor = *col,
                                                                    .Alignment = Alignment_Center,
                                                                    .Sizing = sabs(m_Style[OverlayStyle_WidgetSize]),
                                                                    .Shape = LayoutShape::Circle(),
                                                                    .Padding = 6.f});

    ly.Panel(AsStackedId("Inner radio"),
             LayoutPanelParameters{.FillColor = active ? m_Style[OverlayColor_CheckBoxInner] : Color_Transparent,
                                   .Sizing = grow(),
                                   .Shape = LayoutShape::Circle()});
    ly.EndPanel();

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams(OverlayColor_CheckBoxText));

    ly.EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

// NOTE(Isma): Much repetition with radio button here
bool Overlay::CheckBox(const TKit::StringView label, bool *enable)
{
    Layout &ly = getCurrentLayout();
    const LayoutId id = PushId(label);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));

    const Color *col = &m_Style[OverlayColor_CheckBoxIdle];
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = &m_Style[OverlayColor_CheckBoxPressed];
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = &m_Style[OverlayColor_CheckBoxHovered];

    if (focusFlags & OverlayFocusQueryFlag_LeftClicked)
        *enable = !*enable;

    m_LastItem = ly.BeginPanel(id, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                         .Sizing = fit(),
                                                         .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.BeginPanel(AsStackedId("Outer checkbox"), LayoutPanelParameters{.FillColor = *col,
                                                                       .Alignment = Alignment_Center,
                                                                       .Sizing = sabs(m_Style[OverlayStyle_WidgetSize]),
                                                                       .Padding = 6.f});

    ly.Panel(AsStackedId("Inner checkbox"),
             LayoutPanelParameters{.FillColor = *enable ? m_Style[OverlayColor_CheckBoxInner] : Color_Transparent,
                                   .Sizing = grow()});
    ly.EndPanel();

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams(OverlayColor_CheckBoxText));

    ly.EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

bool Overlay::BeginSelectable(LayoutId id, const bool enabled, const OverlaySelectableFlags flags)
{
    Layout &ly = getCurrentLayout();
    id = PushId(id);
    const LayoutElement *elm = ly.QueryElement(id);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm);

    const bool highlight = flags & OverlaySelectableFlag_Highlight;
    const Color *col = highlight ? &m_Style[OverlayColor_SelectableHovered] : &m_Style[OverlayColor_SelectableIdle];

    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = &m_Style[OverlayColor_SelectablePressed];
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = &m_Style[OverlayColor_SelectableHovered];
    else if (enabled)
        col = &m_Style[OverlayColor_SelectablePressed];

    const bool spanLabel = flags & OverlaySelectableFlag_SpanLabelWidth;
    ly.BeginPanel(id, LayoutPanelParameters{.FillColor = *col,
                                            .Direction = LayoutDirection_TopToBottom,
                                            .Alignment = {Alignment_Left, Alignment_Center},
                                            .Sizing = {spanLabel ? fit() : grow(), fit()},
                                            .Padding = m_Style[OverlayStyle_WidgetPadding]});

    if (!enabled && (flags & OverlaySelectableFlag_SelectOnDoubleClick))
        return focusFlags & OverlayFocusQueryFlag_DoubleClicked;

    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}
bool Overlay::BeginSelectable(const LayoutId id, bool *enabled, const OverlaySelectableFlags flags)
{
    if (BeginSelectable(id, *enabled, flags))
    {
        *enabled = !*enabled;
        return true;
    }
    return false;
}

void Overlay::EndSelectable()
{
    Layout &ly = getCurrentLayout();
    PopId();
    ly.EndPanel();
}

bool Overlay::Selectable(const TKit::StringView label, const bool enabled, const OverlaySelectableFlags flags)
{
    const bool selected = BeginSelectable(label, enabled, flags);
    Layout &ly = getCurrentLayout();
    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams(OverlayColor_DropDownText));
    EndSelectable();

    return selected;
}

bool Overlay::Selectable(const TKit::StringView label, bool *enabled, const OverlaySelectableFlags flags)
{
    if (Selectable(label, *enabled, flags))
    {
        *enabled = !*enabled;
        return true;
    }
    return false;
}

void Overlay::TextRaw(const LayoutTextMode mode, const TKit::StringView text)
{
    LayoutTextParameters params = getTextParams(OverlayColor_Text);
    params.Mode = mode;

    Layout &ly = getCurrentLayout();
    // a very mid solution to unstable ids when text changes every frame (e.g, printing delta times/performance)
    const usz id = m_TextId == NullLayoutId ? ly.GenerateNextId() : m_TextId.Id;
    m_LastItem = ly.Text(id, text, params);
    m_TextId = NullLayoutId;
}
void Overlay::TextIconRaw(const CodePoint icon, const LayoutTextMode mode, const TKit::StringView text)
{
    PushDirection(LayoutDirection_LeftToRight);

    LayoutTextParameters params = getTextParams(OverlayColor_Text);
    params.Mode = mode;

    Layout &ly = getCurrentLayout();
    ly.Unicode(ly.GenerateNextId(), icon, getUnicodeParams(OverlayColor_Text));

    const usz id = m_TextId == NullLayoutId ? ly.GenerateNextId() : m_TextId.Id;
    ly.Text(id, text, params);
    m_TextId = NullLayoutId;
    PopDirection();
}

void Overlay::BeginTooltip()
{
    TKIT_ASSERT(m_Current, "[ONYX][OVERLAY] Cannot begin a tooltip outside of a window");
    TKIT_ASSERT(!m_Tooltip.Active, "[ONYX][OVERLAY] Cannot begin a tooltip inside of a tooltip");
    m_Tooltip.Active = true;
    m_Tooltip.Drawn = true;

    const LayoutId id = "Tooltip";
    const LayoutElement *elm = m_Tooltip.Layout.QueryElement(id);
    const f32v2 size = elm ? elm->Size : f32v2{0.f};

    const f32v2 pos = computeMouseAlignedPosition(size);

    m_Tooltip.Layout.BeginPanel(id, LayoutPanelParameters{.FillColor = m_Style[OverlayColor_WindowBorderIdle],
                                                          .Sizing = fit(),
                                                          .SelfOffset = oabs(pos),
                                                          .Padding = m_Style[OverlayStyle_TooltipPadding]});

    m_Tooltip.Layout.BeginPanel(LayoutPanelParameters{.FillColor = m_Style[OverlayColor_WindowBackgroundExpanded],
                                                      .Direction = LayoutDirection_TopToBottom,
                                                      .Alignment = {Alignment_Left, Alignment_Top},
                                                      .Sizing = fit(),
                                                      .Padding = m_Style[OverlayStyle_ContentAreaPadding],
                                                      .ChildGap = m_Style[OverlayStyle_ChildGap]});
}

bool Overlay::BeginItemTooltip(const OverlayHoveredFlags flags)
{
    if (!IsItemHovered(flags))
        return false;

    BeginTooltip();
    return true;
}

void Overlay::EndTooltip()
{
    TKIT_ASSERT(m_Tooltip.Active, "[ONYX][OVERLAY] Cannot end a tooltip that has not started");
    m_Tooltip.Active = false;
    m_Tooltip.Layout.EndPanel();
    m_Tooltip.Layout.EndPanel();
    m_Tooltip.Layout.Compile();
}

bool Overlay::InputText(TKit::StringView label, char *buf, const u32 size, const TKit::StringView hint,
                        const OverlayInputFlags flags)
{
    beginHorizontalWidget(PushId(label));
    const bool updated = inputTextBox(buf, size, hint, flags);
    endHorizontalWidget(label, OverlayColor_InputText);
    PopId();
    return updated;
}

void Overlay::Draw()
{
    TKIT_ASSERT(
        m_IdStack.IsEmpty(),
        "[ONYX][OVERLAY] Id stack size mismatch (size = {}, should be 0). For every PushId(), there must be a PopId()",
        m_IdStack.GetSize());

    if (m_ActiveWindows.IsEmpty() && !m_Tooltip.Drawn)
        return;

    m_Context->Flush();
    const u32 modalWindow = processWindows();
    if (modalWindow != 0)
        m_View->ClearColor.rgba[3] = 1.f;
    else
        m_View->ClearColor.rgba[3] = 0.f;

    u32 idx = 0;
    for (OverlayWindow *win : m_ActiveWindows)
    {
        if (++idx == modalWindow)
        {
            m_Context->Push();
            m_Context->Scale(2.f * m_View->ScreenToWorld(f32v2{1.f}));
            m_Context->Alpha(0.2f);
            m_Context->Quad();
            m_Context->Pop();
        }
        win->Layout.Compile();
        m_Context->Layout(win->Layout);
    }
    TKIT_ASSERT(!m_Tooltip.Active, "[ONYX][OVERLAY] Found a tooltip that has not been finished");
    if (m_Tooltip.Drawn)
    {
        // tooltip must be compiled everytime it is used
        m_Context->Layout(m_Tooltip.Layout);
        m_Tooltip.Drawn = false;
    }
    m_ActiveWindows.Clear();
}

template <typename F> void Overlay::iterateReverseWindows(const F func)
{
    for (u32 i = m_ActiveWindows.GetSize() - 1; i < m_ActiveWindows.GetSize(); --i)
        if (!func(m_ActiveWindows[i]))
            return;
}

u32 Overlay::processWindows()
{
    u32 modalWindow = 0;
    for (u32 i = 0; i < m_ActiveWindows.GetSize(); ++i)
    {
        m_Current = m_ActiveWindows[i];
        endScroll();
        m_Current->Layout.EndPanel();
        if (m_Current->Flags & OverlayWindowFlag_Modal)
            modalWindow = i + 1;
    }

    const f32v2 mpos = getMousePos();

    m_MouseDelta = mpos - m_MousePos;
    m_MousePos = mpos;

    m_ActiveIdLastFrame = m_ActiveId;
    if (!(m_EventFlags & EventFlag_ActiveIdMustPersist))
    {
        m_ActiveId = NullLayoutId;
        m_EventFlags &= ~EventFlag_ActiveAllowsInteraction;
    }
    if (!(m_EventFlags & EventFlag_PressedIdMustPersist))
        m_PressedId = NullLayoutId;

    TKIT_ASSERT(m_PopupCollapseDepth <= m_PopupStack.GetSize(),
                "[ONYX][OVERLAY] Cannot have a popup depth ({}) greater than the popup stack ({})",
                m_PopupCollapseDepth, m_PopupStack.GetSize());

    if (m_EventFlags & EventFlag_MustCollapsePopups)
    {
        const u32 collapse = Math::Max(m_PopupCollapseDepth, m_ModalCollapseDepth);
        if (m_PressingLeftMouse && collapse < m_PopupStack.GetSize())
            m_EventFlags |= EventFlag_FocusBlockByPopupCollapse;
        m_PopupStack.Resize(collapse);
    }

    m_EventFlags &= EventFlagPersist;

    m_EventKeys.ClearAll();

    const bool widgetBlocked = m_ActiveId != NullLayoutId && !(m_EventFlags & EventFlag_ActiveAllowsInteraction);
    const bool widgetPressed = m_PressedId != NullLayoutId;
    const bool widgetHovered = m_HoveredId != NullLayoutId;
    // if nothing is grabbed, we check mouse cursors here
    if (!m_Grabbed)
    {
        MouseCursor cursor = MouseCursor_Default;
        iterateReverseWindows([&](OverlayWindow *win) {
            // if hovering a widget or window is not hovered (mouse is not on window) remove any hovering and skip
            const bool winHovered = win->CheckFlags(WindowInternalFlag_Hovered);
            const bool popupBlocked = win->PopupDepth != m_PopupStack.GetSize();
            const bool modalBlocked = win->PopupDepth < m_ModalCollapseDepth;
            if (winHovered && win->CheckFlags(WindowInternalFlag_InputHovered))
            {
                win->Resize.Flags = 0;
                cursor = MouseCursor_IBeam;
                return false;
            }
            if (!winHovered || widgetHovered || widgetPressed || widgetBlocked || popupBlocked || modalBlocked)
            {
                win->Resize.Flags = 0;
                return true;
            }
            if (win->CheckFlags(OverlayWindowFlag_NoResize | OverlayWindowFlag_AutoResize))
                return true;
            // else, check if there is resize hover

            const bool hasHoverPadding = win->PopupDepth == 0 || win->PopupDepth == m_ModalCollapseDepth;
            ResizeFlags rflags = 0;
            ResizeInfo &rinfo = win->Resize;
            for (u32 i = 0; i < rinfo.Ids.GetSize(); ++i)
            {
                const usz id = rinfo.Ids[i];
                if (win->Layout.IsHovered(id, m_MousePos, m_Style[OverlayStyle_BorderHoverPadding],
                                          /* so that popups dont "fakingly" announce a resize*/ hasHoverPadding))
                {
                    win->Resize.InteractionColor = &m_Style[OverlayColor_WindowBorderHovered];
                    rflags |= 1U << i;
                }
            }
            rinfo.Flags = rflags;

            const auto cf = [&](const ResizeFlags f) { return f & rflags; };
            if ((cf(ResizeFlag_Left) && cf(ResizeFlag_Bottom)) || (cf(ResizeFlag_Right) && cf(ResizeFlag_Top)))
                cursor = MouseCursor_NESW;

            else if ((cf(ResizeFlag_Right) && cf(ResizeFlag_Bottom)) || (cf(ResizeFlag_Left) && cf(ResizeFlag_Top)))
                cursor = MouseCursor_NWSE;

            else if (cf(ResizeFlag_Left | ResizeFlag_Right))
                cursor = MouseCursor_EW;
            else if (cf(ResizeFlag_Bottom | ResizeFlag_Top))
                cursor = MouseCursor_NS;

            return true;
        });
        m_Window->SetMouseCursor(cursor);
    }

    // remove some state and check whether the window is collapsed
    for (OverlayWindow *win : m_ActiveWindows)
    {
        const bool collapsed =
            !(win->Flags & OverlayWindowFlag_NoCollapse) && Math::Approximately(win->Size[1], win->MinSize[1], 1.f);
        win->HeaderIcon = collapsed ? ArrowRightIcon : ArrowDownIcon;
        win->RemoveFlags(WindowInternalFlag_Hovered | WindowInternalFlag_Focused);
    };

    // check for mouse events
    OverlayWindowFlags wflags = 0;
    f32v2 scroll{0.f};
    m_TextInput.Clear();
    for (const Event &ev : m_Window->GetNewEvents())
    {
        if (ev.Type == Event_MousePressed)
        {
            if (ev.Mouse.Button == Mouse_Button1)
            {
                m_EventFlags |= EventFlag_LeftMousePressed | EventFlag_MustCollapsePopups;
                m_PopupCollapseDepth = 0;
                m_PressingLeftMouse = true;
            }
            if (ev.Mouse.Button == Mouse_Button2)
                m_EventFlags |= EventFlag_RightMousePressed;
        }
        else if (ev.Type == Event_MouseReleased)
        {
            if (ev.Mouse.Button == Mouse_Button1)
            {
                m_EventFlags |= EventFlag_LeftMouseReleased;
                if (m_ClickClock.Restart().AsMilliseconds() <= m_Style[OverlayStyle_ClickMilliseconds])
                    ++m_OverflowClicks;
                m_PressingLeftMouse = false;
                m_EventFlags &= ~EventFlag_FocusBlockByPopupCollapse;
            }
            if (ev.Mouse.Button == Mouse_Button2)
                m_EventFlags |= EventFlag_RightMouseReleased;
        }
        else if (ev.Type == Event_Scrolled)
            scroll = m_Style[OverlayStyle_ScrollSensitivity] * ev.ScrollOffset;
        else if (ev.Type == Event_CharInput)
        {
            char buf[4];
            const u32 count = EncodeUTF8(buf, ev.Character);
            m_TextInput.Insert(m_TextInput.end(), buf, buf + count);
        }
        else if (ev.Type == Event_KeyPressed || ev.Type == Event_KeyRepeat)
            m_EventKeys.Set(ev.Key);
    }

    if (m_ClickClock.GetElapsed().AsMilliseconds() > m_Style[OverlayStyle_ClickMilliseconds])
        m_OverflowClicks = 0;

    bool canAssignHover = true;

    const bool pressed = m_EventFlags & EventFlag_LeftMousePressed;
    iterateReverseWindows([&](OverlayWindow *win) {
        const bool popupBlocked = win->PopupDepth != m_PopupStack.GetSize();
        const bool modalBlocked = win->PopupDepth < m_ModalCollapseDepth;
        const bool hasHoverPadding = win->PopupDepth == 0 || win->PopupDepth == m_ModalCollapseDepth;
        // if we are not popup locked, we jus check if window is hovered, which will allow grab to be set later.
        // if we are popup locked, we can still allow hovering if no depth > 0 widget previously set hovering id. this
        // is because we want to allow dragging immediately when collapsing the whole popup stack, but we dont want
        // dragging when collapsing all. thats why when popups exist, only widgets that belong to the popup tree (any
        // depth except 0) are allowed to set hovered id

        const bool winHovered =
            !modalBlocked && (!popupBlocked || !widgetHovered) && canAssignHover &&
            win->Layout.IsHovered(win->Id, m_MousePos, m_Style[OverlayStyle_BorderHoverPadding], hasHoverPadding);
        const bool inputHovered = win->CheckFlags(WindowInternalFlag_InputHovered);

        win->Flags |= wflags;
        win->RemoveFlags(WindowInternalFlag_InputHovered);

        if (winHovered)
        {
            win->AddFlags(WindowInternalFlag_Hovered);
            canAssignHover = false;
        }

        if (pressed && (win->Resize.Flags != 0 || winHovered))
        {
            if (!widgetHovered && !widgetPressed && !inputHovered)
            {
                win->Resize.Position = win->Position;
                win->Resize.Size = win->Size;
                m_Grabbed = win;
            }
            const bool noFocus = win->Flags & OverlayWindowFlag_NoBringToFocus;
            if (!noFocus)
                win->Layer = toTop();
            // because we just moved the current window to the front (back of the array)
            return false;
        }
        return true;
    });

    m_ActiveWindows.GetBack()->AddFlags(WindowInternalFlag_Focused);

    // now just handle grabbing, which is straightforward
    if (!m_PressingLeftMouse)
    {
        m_Grabbed = nullptr;
        m_MousePosOnPress = m_MousePos;
    }
    else if (m_Grabbed)
    {
        m_Grabbed->Resize.InteractionColor = &m_Style[OverlayColor_WindowBorderPressed];
        ResizeInfo &rinfo = m_Grabbed->Resize;
        f32v2 &p = rinfo.Position;
        f32v2 &s = rinfo.Size;
        const f32v2 &ms = m_Grabbed->MinSize;

        const f32v2 &md = m_MouseDelta;

        const bool noMove = m_Grabbed->Flags & OverlayWindowFlag_NoMove;
        const bool noResize = m_Grabbed->Flags & (OverlayWindowFlag_NoResize | OverlayWindowFlag_AutoResize);

        if (rinfo.Flags == 0 && !noMove)
        {
            p += md;
            m_Grabbed->Position = p;
        }
        else if (!noResize)
        {
            const auto handleResizeAxis = [&](const u32 idx, const ResizeFlags canonical, const ResizeFlags mirrored,
                                              const f32 sign = 1.f) {
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
            handleResizeAxis(0, ResizeFlag_Left, ResizeFlag_Right);
            handleResizeAxis(1, ResizeFlag_Top, ResizeFlag_Bottom, -1.f);
        }
    }

    m_HoveredId = NullLayoutId;
    if (!m_CandidateLayout || !m_CandidateLayout->IsHovered(m_HoveredWidgetCandidate, m_MousePos))
        m_HoveredWidgetCandidate = NullLayoutId;

    if (m_HoveredWidgetCandidate == NullLayoutId)
        m_WidgetHoverClock.Restart();

    m_CandidateLayout = nullptr;

    ScrollBarInfo *vinfo = nullptr;
    ScrollBarInfo *hinfo = nullptr;

    const u32 size = m_ScrollStack.GetSize();
    for (u32 i = size - 1; i < size; --i)
    {
        const usz id = m_ScrollStack[i];
        ScrollInfo &sinfo = m_Scrollables[id];
        if (!vinfo && !(sinfo.Flags & OverlayWindowFlag_NoVerticalScroll))
            vinfo = &sinfo.Vertical;
        if (!hinfo && (sinfo.Flags & OverlayWindowFlag_HorizontalScroll))
            hinfo = &sinfo.Horizontal;
    }

    if (vinfo)
        vinfo->WheelOffset += scroll[1];
    if (hinfo)
        hinfo->WheelOffset += scroll[0];

    m_ScrollStack.Clear();
    TKIT_ASSERT(m_CurrentPopupDepth == 0, "[ONYX][OVERLAY] Pop up stack not properly closed! {} entries remaining",
                m_CurrentPopupDepth);
    TKIT_ASSERT(m_WindowStack.IsEmpty(), "[ONYX][OVERLAY] Window stack not properly closed! {} windows remaining",
                m_WindowStack.GetSize());
    return modalWindow;
}

OverlayWindow *Overlay::getOrCreateOverlayWindow(const LayoutId id)
{
    for (u32 i = 0; i < m_WindowIds.GetSize(); ++i)
        if (id == m_WindowIds[i])
            return &m_OverlayWindows[i];

    m_WindowIds.Append(id);
    OverlayWindow &win = m_OverlayWindows.Append(m_LayoutSpecs);
    win.HeaderIcon = ArrowDownIcon;
    win.Position += m_WindowSpawnOffset;
    win.Layer = toTop();
    m_WindowSpawnOffset += m_Style[OverlayStyle_WindowSpawnDelta];
    return &win;
}

u32 Overlay::getFormatDecimals(const char *format)
{
    for (const char *cc = format; *cc != 0; ++cc)
    {
        const char c0 = cc[0];
        const char c1 = cc[1];
        if (c0 == '.' && c1 >= '0' && c1 <= '9')
            return c1 - '0';
    }
    return 0;
}

const FontData &Overlay::getFontData() const
{
    const Resource font = m_Context->GetState().Font;
    return Resources::GetFontData(font);
}
f32 Overlay::computeWindowMinSize() const
{
    const FontData &fdata = getFontData();
    return m_Style[OverlayStyle_FontSize] * fdata.LineHeight +
           2.f * (m_Style[OverlayStyle_WindowPadding] + m_Style[OverlayStyle_HeaderPadding]);
}

f32v2 Overlay::getMousePos() const
{
    return m_View->ScreenToWorld(m_Window->GetNormalizedMousePosition());
}

f32v2 Overlay::computeMouseAlignedPosition(const f32v2 &size) const
{
    const f32 toffset = m_Style[OverlayStyle_TooltipOffset];
    const f32v2 offset = f32v2{toffset, -2.f * toffset};

    const f32v2 border = m_View->ScreenToWorld(f32v2{1.f});

    const f32 rt = m_MousePos[0] + offset[0] + size[0];
    const f32 rb = m_MousePos[1] + offset[1] - size[1];

    f32v2 pos = m_MousePos + offset;
    if (rt > border[0])
        pos[0] -= rt - border[0];
    if (rb < border[1])
        pos[1] -= rb - border[1];

    return pos;
}

} // namespace Onyx
