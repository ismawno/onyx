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
enum OverlayEventFlagBit : OverlayEventFlags
{
    OverlayEventFlag_MousePressed = 1U << 0,
    OverlayEventFlag_MouseReleased = 1U << 1,
};
enum OverlayWindowInternalFlagBit : OverlayWindowFlags
{
    OverlayWindowFlag_Collapsed = 1U << 0,
    OverlayWindowFlag_Hovered = 1U << 1,
    OverlayWindowFlag_InputHovered = 1U << 2,
    OverlayWindowFlagClear = (1U << 3) - 1U
};

static Color hex(const TKit::StringView h)
{
    return Color::FromHexadecimal(h);
}

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
    vars[OverlayStyle_HeaderButtonWidth] = 20.f;
    vars[OverlayStyle_BorderHoverPadding] = 8.f;
    vars[OverlayStyle_ContentAreaPadding] = 4.f;
    vars[OverlayStyle_ScrollBarWidth] = 8.f;
    vars[OverlayStyle_ScrollSensitivity] = 16.f;
    vars[OverlayStyle_WidgetSize] = 24.f;
    vars[OverlayStyle_WidgetPadding] = 6.f;
    vars[OverlayStyle_TreeLineWidth] = 4.f;
    vars[OverlayStyle_ClickMilliseconds] = 200.f;
    vars[OverlayStyle_CursorWidth] = 2.f;
    vars[OverlayStyle_HoverDelayShort] = 0.15f;
    vars[OverlayStyle_HoverDelayNormal] = 0.40f;
    vars[OverlayStyle_HoverStationaryThreshold] = 5.f;
    vars[OverlayStyle_BoxInputHintAlpha] = 0.4f;
    vars[OverlayStyle_BoxInputCursorAlpha] = 0.6f;
    return vars;
}

OverlayPalette CreateDefaultOverlayPalette()
{
    OverlayPalette palette;
    palette[OverlayPalette_Idle0] = hex("2D3748");
    palette[OverlayPalette_Idle1] = hex("3A4F6F");

    palette[OverlayPalette_Hovered0] = hex("4A5568");
    palette[OverlayPalette_Hovered1] = hex("5A7A9E");
    palette[OverlayPalette_Hovered2] = hex("3A4A60");

    palette[OverlayPalette_Pressed0] = hex("5A6A7E");
    palette[OverlayPalette_Pressed1] = hex("4A5A72");

    palette[OverlayPalette_Text0] = hex("E2E8F0");

    palette[OverlayPalette_Inner0] = hex("4A8EC2");

    palette[OverlayPalette_Background0] = hex("2A3F5F");
    palette[OverlayPalette_Background1] = hex("344E6E");

    palette[OverlayPalette_Background2] = hex("1E2D45D9");
    palette[OverlayPalette_Background3] = hex("2A3F5FD9");

    return palette;
}

OverlayColors CreateOverlayColorsFromPalette(const OverlayPalette &palette)
{
    OverlayColors colors;

    colors[OverlayColor_Text] = palette[OverlayPalette_Text0];
    colors[OverlayColor_Highlight] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_Line] = palette[OverlayPalette_Background1];

    colors[OverlayColor_WindowBorderIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_WindowBorderHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_WindowBorderPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_WindowHeader] = palette[OverlayPalette_Text0];

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

    colors[OverlayColor_TreeIdle] = palette[OverlayPalette_Background1];
    colors[OverlayColor_TreeHovered] = palette[OverlayPalette_Hovered2];
    colors[OverlayColor_TreePressed] = palette[OverlayPalette_Pressed1];
    colors[OverlayColor_TreeText] = palette[OverlayPalette_Text0];

    colors[OverlayColor_WindowBackgroundExpanded] = palette[OverlayPalette_Background0];
    colors[OverlayColor_WindowBackgroundCollapsed] = palette[OverlayPalette_Background2];
    colors[OverlayColor_WindowHeaderBackgroundExpanded] = palette[OverlayPalette_Background1];
    colors[OverlayColor_WindowHeaderBackgroundCollapsed] = palette[OverlayPalette_Background3];

    return colors;
}

Overlay::Overlay(Window *win, const UserInterfaceSpecs &specs)
    : m_LayoutSpecs(specs.Layout), m_Window(win), m_Style(specs.Style), m_Tooltip(specs.Layout)
{
    TKIT_ASSERT(specs.Layout.RootAlignment[0] == Alignment_Left && specs.Layout.RootAlignment[1] == Alignment_Top,
                "[ONYX][UI] Root alignment for layouts must be Top Left. Other alignments are not supported for root");

    m_Camera.Mode = CameraMode_Viewport;
    m_View = win->CreateRenderView<D2>(&m_Camera, RenderViewFlag_NormalizedCoordinates | RenderViewFlag_Transparency);
    m_View->ClearColor.rgba[3] = 0.f;

    m_Context = CreateRenderContext<D2>();
    m_Context->AddTarget(m_View);
}

void Overlay::drawWindowBorders()
{
    Layout &ly = getCurrentLayout();
    const Color &interaction = *m_Current->Resize.InteractionColor;
    const Color &idle = m_Style[OverlayColor_WindowBorderIdle];

    OverlayResizeInfo &rinfo = m_Current->Resize;

    const bool l = rinfo.Flags & OverlayResizeFlag_Left;
    const bool r = rinfo.Flags & OverlayResizeFlag_Right;
    const bool b = rinfo.Flags & OverlayResizeFlag_Bottom;
    const bool t = rinfo.Flags & OverlayResizeFlag_Top;

    const vec2<LayoutSizing> hsizing = {sabs(m_Style[OverlayStyle_WindowBorderWidth]), grow()};
    const vec2<LayoutSizing> vsizing = {grow(), sabs(m_Style[OverlayStyle_WindowBorderWidth])};

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

void Overlay::drawWindowScrollBar()
{
    Layout &ly = getCurrentLayout();
    const LayoutElement *contentArea = ly.QueryElement("Content area");
    if (!contentArea)
        return;

    const f32 size = contentArea->Size[1];
    const f32 csize = contentArea->ChildrenSize[1] + 2.f * m_Style[OverlayStyle_WindowPadding];

    ScrollBarInfo &sinfo = m_Current->ScrollBar;
    if (csize > size)
    {
        const Color *col = &m_Style[OverlayColor_ScrollBarIdle];

        const LayoutElement *scrollBar = ly.QueryElement("Scroll bar");
        if (scrollBar)
        {
            const DragFocusInfo iinfo = getDragFocusInfo(scrollBar);

            if (iinfo.Pressed)
            {
                col = &m_Style[OverlayColor_ScrollBarPressed];
                sinfo.CursorOffset += m_MouseDelta[1];
            }
            else
                sinfo.CursorOffset = sinfo.BarOffset; // this indirectly saves the WheelOffset state

            if (!iinfo.Pressed && iinfo.Hovered)
                col = &m_Style[OverlayColor_ScrollBarHovered];
        }
        else
            sinfo.CursorOffset = sinfo.BarOffset; // this indirectly saves the WheelOffset state

        const f32 barSize = size * size / csize;
        const f32 maxOffset = size - barSize;

        const f32 elementFactor = csize / size;

        const f32 unbounded = sinfo.CursorOffset + sinfo.WheelOffset / elementFactor;
        sinfo.BarOffset = Math::Clamp(unbounded, -maxOffset, 0.f);

        sinfo.ElementOffset = elementFactor * sinfo.BarOffset;

        const bool noScroll = m_Current->CheckFlags(OverlayWindowFlag_NoScrollBar);
        if (!noScroll)
            ly.Panel("Scroll bar",
                     LayoutPanelParameters{.FillColor = *col,
                                           .Sizing = sabs({m_Style[OverlayStyle_ScrollBarWidth], barSize}),
                                           .SelfOffset = oabs({0.f, sinfo.BarOffset}),
                                           .Shape = LayoutShape::Rectangle(m_Style[OverlayStyle_ScrollBarWidth])});
    }
    else
        sinfo.Reset();

    sinfo.WheelOffset = 0.f;
}

TKit::StringView Overlay::trimLabel(const TKit::StringView label)
{
    const u32 idx = label.FindFirstOf("##");
    if (idx == TKit::StringView::npos)
        return label;
    return label.SubString(idx);
}

bool Overlay::BeginWindow(const TKit::StringView title, const OverlayWindowFlags flags)
{
    TKIT_ASSERT(!m_Current, "[ONYX][Overlay] Cannot begin a new window when another one is being processed");
    const LayoutId id = PushId(title);

    m_Current = getOrCreateOverlayWindow(id);
    m_Current->MinSize = computeWindowMinSize(m_Style[OverlayStyle_WindowPadding], m_Style[OverlayStyle_HeaderPadding],
                                              m_Style[OverlayStyle_FontSize]);
    m_Current->Flags &= OverlayWindowFlagClear;
    m_Current->Flags |= flags;

    Layout &ly = getCurrentLayout();

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
                                                            return m_Style[OverlayColor_WindowBackgroundCollapsed];
                                                        return m_Style[OverlayColor_WindowBackgroundExpanded];
                                                    }(),
                                                .Direction = LayoutDirection_TopToBottom,
                                                .Alignment = topLeft,
                                                .Sizing = sizing,
                                                .SelfOffset = oabs(m_Current->Position),
                                                .Padding = m_Style[OverlayStyle_WindowPadding],
                                                .ChildGap = m_Style[OverlayStyle_ChildGap]});

    m_LastWidget = m_Current->Id;
    drawWindowBorders();
    if (!noHeader)
    {
        ly.BeginPanel(AsStackedId("Header"),
                      LayoutPanelParameters{.FillColor = collapsed
                                                             ? m_Style[OverlayColor_WindowHeaderBackgroundCollapsed]
                                                             : m_Style[OverlayColor_WindowHeaderBackgroundExpanded],
                                            .Alignment = {Alignment_Left, Alignment_Center},
                                            .Sizing = {flex(), fit()},
                                            .Padding = m_Style[OverlayStyle_HeaderPadding],
                                            .ChildGap = m_Style[OverlayStyle_ChildGap]});

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

        ly.Text(AsStackedId(ly.GenerateNextId()), trimLabel(title), getTextParams(OverlayColor_WindowHeader));
        ly.EndPanel();
    }

    ly.BeginPanel(AsStackedId("Scroll area"), LayoutPanelParameters{.Direction = LayoutDirection_RightToLeft,
                                                                    .Alignment = topLeft,
                                                                    .Sizing = autoResize ? fit() : grow(),
                                                                    .ChildGap = 0.5f * m_Style[OverlayStyle_ChildGap]});
    // must pass the id bc at this point, querying plainly with "Scroll area" will mix with the actual "Scroll area"
    // panel, giving a different id
    if (!collapsed && !autoResize)
        drawWindowScrollBar();
    ly.BeginPanel(AsStackedId("Content area"),
                  LayoutPanelParameters{.Direction = LayoutDirection_TopToBottom,
                                        .Alignment = topLeft,
                                        .Sizing = autoResize ? fit() : grow(),
                                        .ChildOffset = oabs({0.f, -m_Current->ScrollBar.ElementOffset}),
                                        .Padding = m_Style[OverlayStyle_ContentAreaPadding],
                                        .ChildGap = m_Style[OverlayStyle_ChildGap]});

    return !collapsed;
}

void Overlay::EndWindow()
{
    TKIT_ASSERT(m_Current, "[ONYX][Overlay] Cannot end a window without having started one");
    m_Current->Layout.EndPanel();
    m_Current->Layout.EndPanel();
    m_Current->Layout.EndPanel();
    m_Current = nullptr;
    PopId();
}

void Overlay::HorizontalSeparator(const TKit::StringView label, const f32 textOffset, const f32 width)
{
    Layout &ly = getCurrentLayout();
    ly.BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_LeftToRight,
                                        .Alignment = {Alignment_Left, Alignment_Center},
                                        .Sizing = {grow(), fit()},
                                        .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.Panel(LayoutPanelParameters{.FillColor = m_Style[OverlayColor_WindowHeaderBackgroundExpanded],
                                   .Sizing = sabs({textOffset, width})});
    ly.Text(trimLabel(label), getTextParams());
    ly.Panel(LayoutPanelParameters{.FillColor = m_Style[OverlayColor_WindowHeaderBackgroundExpanded],
                                   .Sizing = {grow(), sabs(width)}});
    ly.EndPanel();
}

bool Overlay::PushTree(LayoutId id, const TKit::StringView label, const OverlayTreeFlags flags)
{
    id = PushId(id);
    Layout &ly = getCurrentLayout();
    const bool framed = flags & OverlayTreeFlag_Framed;

    const Color *col = framed ? &m_Style[OverlayColor_TreeIdle] : &Color_Transparent;
    ClickFocusInfo info = getClickFocusInfo(ly.QueryElement(id));
    if (info.Pressed)
        col = &m_Style[OverlayColor_TreePressed];
    else if (info.Hovered)
        col = &m_Style[OverlayColor_TreeHovered];

    const bool spanLabel = flags & OverlayTreeFlag_SpanLabelWidth;
    const vec2<LayoutSizing> sizing = {spanLabel ? fit() : grow(), fit()};

    m_LastWidget = ly.BeginPanel(id, LayoutPanelParameters{.FillColor = *col,
                                                           .Alignment = {Alignment_Left, Alignment_Center},
                                                           .Sizing = sizing,
                                                           .Padding = m_Style[OverlayStyle_HeaderPadding],
                                                           .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const bool startOpen = flags & OverlayTreeFlag_StartOpen;
    const bool opened =
        checkWidgetState(id.Id, OverlayWidgetStateFlag_TreeOpened, startOpen ? OverlayWidgetStateFlag_TreeOpened : 0);

    const usz buttonId =
        ly.BeginPanel(AsStackedId("Tree collapse"),
                      LayoutPanelParameters{.Alignment = Alignment_Center,
                                            .Sizing = {sabs(m_Style[OverlayStyle_HeaderButtonWidth]), fit()}});
    if (info.Clicked)
    {
        const bool onArrow = flags & OverlayTreeFlag_OpenOnArrow;
        const bool onDoubleClick = flags & OverlayTreeFlag_OpenOnDoubleClick;
        if (onArrow)
            info.Clicked = ly.IsHovered(buttonId, m_MousePos) || (onDoubleClick && info.DoubleClicked);
        else if (onDoubleClick)
            info.Clicked = info.DoubleClicked;
    }

    const CodePoint code = opened ? m_ExpandedHeaderIcon : m_CollapsedHeaderIcon;
    ly.Unicode(AsStackedId(code), code, getUnicodeParams(OverlayColor_TreeText));

    ly.EndPanel();

    ly.Text(trimLabel(label), getTextParams(OverlayColor_TreeText));
    ly.EndPanel();

    if (info.Clicked)
        toggleWidgetState(id.Id, OverlayWidgetStateFlag_TreeOpened);

    if (!opened)
    {
        PopId();
        return false;
    }

    const bool lines = flags & OverlayTreeFlag_DrawLines;
    const bool indent = !(flags & OverlayTreeFlag_NoIndent);

    const FontData &fdata = getFontData();
    const f32 fs = m_Style[OverlayStyle_FontSize];

    const f32 iconWidth =
        Math::Max(fs * fdata.GetGlyph(m_ExpandedHeaderIcon)->Advance, m_Style[OverlayStyle_HeaderButtonWidth]);
    const f32 treeIndent = iconWidth + 2.f * m_Style[OverlayStyle_HeaderPadding];

    ly.BeginPanel(LayoutPanelParameters{
        .Direction = LayoutDirection_LeftToRight, .Alignment = {Alignment_Left, Alignment_Top}, .Sizing = sizing});

    if (indent)
    {
        ly.BeginPanel(LayoutPanelParameters{.Direction = LayoutDirection_TopToBottom,
                                            .Alignment = Alignment_Center,
                                            .Sizing = {sabs(treeIndent), grow()}});

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
                           const bool overrideEnterFocus)
{
    TKIT_ASSERT(size != 0, "[ONYX][UI] Buffer size for text input cannot be zero");
    Layout &ly = getCurrentLayout();
    const u32 strSize = u32(std::strlen(buf));
    TKIT_ASSERT(strSize < size, "[ONYX][UI] The input character length ({}) exceeds buffer size ({})", strSize, size);

    // lets just first store the buffer in a string for easy insertion

    const LayoutId iboxId = AsStackedId("Input box");
    const LayoutElement *ibox = ly.QueryElement(iboxId);
    const InputFocusInfo info = getInputFocusInfo(ibox);
    // this is the actual input box. color selection is a bit random
    ly.BeginPanel(iboxId, LayoutPanelParameters{.FillColor = m_Style[OverlayColor_WindowBackgroundCollapsed],
                                                .Alignment = {Alignment_Left, Alignment_Center},
                                                .Sizing = {grow(), fit()},
                                                .Padding = m_Style[OverlayStyle_WidgetPadding]});

    bool updated = false;
    // if we are not focused, we pretty much just print the text and thats it (focused just means the cursor is
    // active and awaiting input)
    const f32 boxSize = ibox ? (ibox->Size[0] - 2.f * m_Style[OverlayStyle_WidgetPadding]) : 0.f;

    const FontData &fdata = getFontData();
    const f32 fs = m_Style[OverlayStyle_FontSize];

    LayoutTextParameters tparams = getTextParams();
    if (!info.Focused)
    {
        const bool elide = flags & OverlayInputFlag_ElideLeft;
        const f32 textOffset = elide ? Math::Min(0.f, boxSize - fs * fdata.ComputeTextWidth(buf)) : 0.f;
        const bool useHint = strSize == 0 && !hint.IsEmpty();

        tparams.Offset[0] = oabs(textOffset);
        if (useHint)
        {
            tparams.FillColor.rgba[3] = m_Style[OverlayStyle_BoxInputHintAlpha];
            ly.Text(hint, tparams);
        }
        else
            ly.Text(buf, tparams);
    }
    else
    {
        TKit::String &str = m_InputWidgetBuffer;
        const bool enteredFocus = info.EnteredFocus || overrideEnterFocus;
        if (enteredFocus)
        {
            str.Clear();
            str.Insert(str.end(), buf, buf + strSize);
        }

        const bool escapeClears = flags & OverlayInputFlag_EscapeClearsAll;
        if (escapeClears && m_EventKeys[Key_Escape])
        {
            str.Clear();
            updated = true;
        }
        const f32 boxPos = ibox ? (ibox->Position[0] + m_Style[OverlayStyle_WidgetPadding]) : 0.f;

        f32 relCursorPos = m_Current->TextCursorPos - boxPos;
        // overflow clicks means how many rapid succession clicks have happened without counting the first (aka, ==
        // 1 is a double click)
        //
        // in general, advances are clamped to char borders, but are in pixel units as well
        // pretty straightforward, just grab all the text

        const bool autoSelectAll = flags & OverlayInputFlag_AutoSelectAll;
        if (m_OverflowClicks == 2 || (enteredFocus && autoSelectAll))
        {
            m_Current->TextCursorPos = boxPos;
            m_Current->TextHighlightSize = fs * fdata.ComputeTextWidth(str);
        }
        else if (m_OverflowClicks == 1)
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

            // and then update the global positions/sizes. this is important bc all input is derived from these, so
            // they must remain consisten
            m_Current->TextCursorPos = startAdvance + boxPos;
            m_Current->TextHighlightSize = endAdvance - startAdvance;
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

        // we now get the advances (pixel positions) of cursor, highlight start and highlight end, as well as the
        // index ranges (note that the cursor has a trivial range of 1 element, while highlight may have a large
        // one)

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

        const bool noHorScroll = flags & OverlayInputFlag_NoHorizontalScroll;
        const f32 textOffset =
            noHorScroll ? 0.f : Math::Min(0.f, boxSize - textWidth - m_Style[OverlayStyle_CursorWidth]);

        tparams.Offset[0] = oabs(textOffset);
        const bool useHint = str.IsEmpty() && !hint.IsEmpty();
        if (useHint)
        {
            tparams.FillColor.rgba[3] = m_Style[OverlayStyle_BoxInputHintAlpha];
            ly.Text(useHint ? hint : str, tparams);
        }
        else
            ly.Text(str, tparams);

        const f32 hAdvance = hendAdvance - hstartAdvance;
        // bc of layout solving, cursor is gonna be offsetted by the text. we have to work out how much to "bring it
        // back", that is, if cursor is in front of the first char (advance == 0), we need to offset it by
        // -textWidth. same goes for highlight

        f32 offset = cursorAdvance - textWidth + textOffset;
        if (useHint)
            offset -= fs * fdata.ComputeTextWidth(hint);
        else
            // the 0.1f is because some rounding errors clipping the cursor against the left edge of the input box
            offset += 0.1f;

        // if there is any highlight, cursor is discarded
        const bool hasCursor = hAdvance == 0.f;
        if (hasCursor)
            ly.Panel(AsStackedId("Cursor"),
                     LayoutPanelParameters{
                         .FillColor = Color{m_Style[OverlayColor_Text], m_Style[OverlayStyle_BoxInputCursorAlpha]},
                         .Sizing = {sabs(m_Style[OverlayStyle_CursorWidth]), grow()},
                         .SelfOffset = oabs({offset, 0.f})});
        else
        {
            // highlight may be negative, but we just removed that with hstartPos etc. this means that hAdvance is
            // guaranteed to be positive (which is needed, bc sizes must be). but... a positive size will grow to
            // the right, but if user is dragging to the left, the highlight must grow to the left. we counter that
            // by offsetting the offset (lol) again by the width of the highlight (hAdvance. the difference with the
            // original width is that this one is clamped to character borders)
            const f32 hoffset = negHsize ? (offset - hAdvance) : offset;
            ly.Panel(AsStackedId("Highlight"),
                     LayoutPanelParameters{.FillColor = Color{m_Style[OverlayColor_Highlight], 0.4f},
                                           .Sizing = {sabs(hAdvance), grow()},
                                           .SelfOffset = oabs({hoffset, 0.f})});
        }

        // we just then see how many spots are left and how many spots the user wants to write to. we take the min
        // of them
        const u32 spotsLeft = size - 1 - strSize;
        const u32 spots = Math::Min(m_Current->TextInput.GetSize(), spotsLeft);

        // we take a range to remove characters by cursor or by highlight
        const u32 toRemoveBegin = hasCursor ? cursorStart : hstart;
        const u32 toRemoveEnd = hasCursor ? cursorEnd : hend;

        // and just apply it. for highlight, we want to remove even if user just tried to write
        if (toRemoveBegin != toRemoveEnd && !str.IsEmpty() &&
            (m_EventKeys[Key_Backspace] || (spots != 0 && !hasCursor)))
        {
            updated = true;
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
            // importantly, we have to account for the text we have removed, and subtract it from the cursor pos
            // only if user uses cursor or highlight is negative (bc then the invisible cursor would be at the end
            // of the highlight, and we have to shift it)

            if (hasCursor || negHsize)
                m_Current->TextCursorPos -= fs * toRemoveWidth;

            // we can now reset the highlight
            m_Current->TextHighlightSize = 0.f;

            // this is important bc we are gonna insert now into the cursor position. the deletion might have
            // invalidated the position, so we adjust for that
            if (cursorEnd > toRemoveBegin)
                cursorEnd = toRemoveBegin;
        }

        updated |= spots != 0;
        // and then insert!
        for (u32 i = 0; i < spots; ++i)
            str.Insert(str.begin() + cursorEnd, m_Current->TextInput[i]);

        if (spots != 0)
            m_Current->TextCursorPos += fs * fdata.ComputeTextWidth(m_Current->TextInput);

        const bool enterCommits = flags & OverlayInputFlag_EnterCommitsBuffer;
        if (enterCommits)
            updated = m_Window->IsKeyPressed(Key_Enter);
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
                updated = m_Window->IsKeyPressed(Key_Enter);
        }
    }

    ly.EndPanel();

    return updated;
}

InputConvertInfo Overlay::getInputConvertInfo(const bool hovered, const bool allowDoubleClick)
{
    InputConvertInfo info{};

    Layout &ly = getCurrentLayout();
    const LayoutElement *ibox = ly.QueryElement(AsStackedId("Input box"));

    if (!ibox)
        return info;

    const bool ctrl = m_Window->IsKeyPressed(Key_LeftControl);
    const bool dclick = allowDoubleClick && (m_OverflowClicks == 1);

    const bool triggered = hovered && (dclick || (ctrl && checkFlags(OverlayEventFlag_MousePressed)));
    const bool persisted = ibox && (ibox->IsHovered(m_MousePos) || m_FocusedInputter == ibox->Id);

    info.MustConvert = triggered || persisted;
    info.MustOverrideHighlight = ibox && ibox->IsHovered(m_MousePos) && m_DelayedFocusedInputter != ibox->Id;

    if (info.MustConvert)
    {
        if (ibox)
        {
            m_FocusedInputter = ibox->Id;
            m_DelayedFocusedInputter = ibox->Id; // so that focus highlight all is not triggered
        }
    }
    return info;
}

// TODO(Isma): Too much repetition between this and Button()
bool Overlay::collapseButton()
{
    Layout &ly = getCurrentLayout();

    const LayoutId id = AsStackedId("Collapse button");
    const ClickFocusInfo info = getClickFocusInfo(ly.QueryElement(id));

    const Color *col = &Color_Transparent;
    if (info.Pressed)
        col = &m_Style[OverlayColor_ButtonPressed];

    ly.BeginPanel(id, LayoutPanelParameters{.FillColor = *col,
                                            .Alignment = Alignment_Center,
                                            .Sizing = {sabs(m_Style[OverlayStyle_HeaderButtonWidth]), fit()}});

    const CodePoint code = m_Current->HeaderIcon;
    ly.Unicode(AsStackedId(code), code, getUnicodeParams(OverlayColor_WindowHeader));

    ly.EndPanel();
    return info.Clicked;
}

ClickFocusInfo Overlay::getClickFocusInfo(const LayoutElement *elm)
{
    if (!elm)
        return ClickFocusInfo{};

    const bool canFocus = m_Current->CheckFlags(OverlayWindowFlag_Hovered) && !m_Grabbed;

    const bool canHover = canFocus && m_PressedDragger == NullLayoutId &&
                          (m_PressedClicker == NullLayoutId || m_PressedClicker == elm->Id);
    const bool hovered = canHover && elm->IsHovered(m_MousePos);

    const bool clicked = hovered && m_DelayedPressedClicker == elm->Id && checkFlags(OverlayEventFlag_MouseReleased);
    const bool pressed = hovered && (m_DelayedPressedClicker == NullLayoutId || m_DelayedPressedClicker == elm->Id) &&
                         m_Window->IsMousePressed(Mouse_Button1);

    if (pressed)
    {
        m_PressedClicker = elm->Id;
        m_DelayedPressedClicker = elm->Id;
    }
    if (hovered)
        m_HoveredClicker = elm->Id;

    ClickFocusInfo info;
    info.Clicked = clicked;
    info.DoubleClicked = clicked && m_OverflowClicks == 1;
    info.Pressed = pressed;
    info.Hovered = hovered;
    return info;
}

DragFocusInfo Overlay::getDragFocusInfo(const LayoutElement *elm)
{
    if (!elm)
        return DragFocusInfo{};

    DragFocusInfo info;
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

InputFocusInfo Overlay::getInputFocusInfo(const LayoutElement *elm)
{
    if (!elm)
        return InputFocusInfo{};

    const bool canFocus = m_Current->CheckFlags(OverlayWindowFlag_Hovered) && !m_Grabbed;
    const bool canHover = canFocus && m_PressedDragger == NullLayoutId && m_PressedClicker == NullLayoutId;

    const bool hovered = canHover && elm->IsHovered(m_MousePos);
    const bool pressed = m_Window->IsMousePressed(Mouse_Button1);
    const bool clicked = checkFlags(OverlayEventFlag_MousePressed);

    const bool interacting = hovered && pressed;
    const bool clicking = hovered && clicked;

    if (hovered)
        m_Current->Flags |= OverlayWindowFlag_InputHovered;

    InputFocusInfo info;
    info.EnteredFocus = interacting && m_DelayedFocusedInputter != elm->Id;
    info.Focused = clicking || m_FocusedInputter == elm->Id;

    if (interacting)
    {
        m_Current->TextHighlightSize += m_MouseDelta[0];
        if (clicking)
        {
            m_FocusedInputter = elm->Id;
            m_DelayedFocusedInputter = elm->Id;
            m_Current->TextCursorPos = m_MousePos[0];
        }
    }
    return info;
}

bool Overlay::Button(const TKit::StringView label, const OverlayButtonFlags flags)
{
    Layout &ly = getCurrentLayout();
    const LayoutId id = PushId(label);

    const ClickFocusInfo info = getClickFocusInfo(ly.QueryElement(id));

    const Color *col = &m_Style[OverlayColor_ButtonIdle];
    if (info.Pressed)
        col = &m_Style[OverlayColor_ButtonPressed];
    else if (info.Hovered)
        col = &m_Style[OverlayColor_ButtonHovered];

    const bool spanFull = flags & OverlayButtonFlag_SpanFullWidth;

    const vec2<LayoutSizing> sizing = spanFull ? vec2<LayoutSizing>{grow(), fit()} : vec2<LayoutSizing>{fit()};

    m_LastWidget = ly.BeginPanel(
        id, LayoutPanelParameters{.FillColor = *col, .Alignment = Alignment_Center, .Sizing = sizing, .Padding = 8.f});

    ly.Text(trimLabel(label), getTextParams(OverlayColor_ButtonText));
    ly.EndPanel();
    PopId();
    return info.Clicked;
}

bool Overlay::RadioButton(const TKit::StringView label, const bool active)
{
    Layout &ly = getCurrentLayout();
    const LayoutId id = PushId(label);

    const ClickFocusInfo info = getClickFocusInfo(ly.QueryElement(id));

    const Color *col = &m_Style[OverlayColor_CheckBoxIdle];
    if (info.Pressed)
        col = &m_Style[OverlayColor_CheckBoxPressed];
    else if (info.Hovered)
        col = &m_Style[OverlayColor_CheckBoxHovered];

    m_LastWidget = ly.BeginPanel(id, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
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

    ly.Text(trimLabel(label), getTextParams(OverlayColor_CheckBoxText));

    ly.EndPanel();
    PopId();
    return info.Clicked;
}

// NOTE(Isma): Much repetition with radio button here
bool Overlay::CheckBox(const TKit::StringView label, bool *enable)
{
    Layout &ly = getCurrentLayout();
    const LayoutId id = PushId(label);

    const ClickFocusInfo info = getClickFocusInfo(ly.QueryElement(id));
    const Color *col = &m_Style[OverlayColor_CheckBoxIdle];
    if (info.Pressed)
        col = &m_Style[OverlayColor_CheckBoxPressed];
    else if (info.Hovered)
        col = &m_Style[OverlayColor_CheckBoxHovered];

    if (info.Clicked)
        *enable = !*enable;

    m_LastWidget = ly.BeginPanel(id, LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
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

    ly.Text(trimLabel(label), getTextParams(OverlayColor_CheckBoxText));

    ly.EndPanel();
    PopId();
    return info.Clicked;
}

void Overlay::BeginTooltip()
{
    TKIT_ASSERT(m_Current, "[ONYX][UI] Cannot begin a tooltip outside of a window");
    TKIT_ASSERT(!m_Tooltip.Active, "[ONYX][UI] Cannot begin a tooltip inside of a tooltip");
    m_Tooltip.Active = true;
    m_Tooltip.Drawn = true;

    f32v2 offset = f32v2{m_Style[OverlayStyle_TooltipOffset], -2.f * m_Style[OverlayStyle_TooltipOffset]};
    vec2<Alignment> alg{Alignment_Left, Alignment_Top};

    if (m_MousePos[0] + offset[0] > 0.f)
    {
        alg[0] = Alignment_Right;
        offset[0] = -offset[0];
    }
    if (m_MousePos[1] + offset[1] < 0.f)
    {
        alg[1] = Alignment_Bottom;
        offset[1] = -0.5f * offset[1];
    }
    m_Tooltip.Layout.SetSpecs({.RootAlignment = alg});

    m_Tooltip.Layout.BeginPanel(LayoutPanelParameters{.FillColor = m_Style[OverlayColor_WindowBorderIdle],
                                                      .Sizing = fit(),
                                                      .SelfOffset = oabs(m_MousePos + offset),
                                                      .Padding = m_Style[OverlayStyle_TooltipPadding]});

    m_Tooltip.Layout.BeginPanel(LayoutPanelParameters{.FillColor = m_Style[OverlayColor_WindowBackgroundExpanded],
                                                      .Direction = LayoutDirection_TopToBottom,
                                                      .Alignment = {Alignment_Left, Alignment_Top},
                                                      .Sizing = fit(),
                                                      .Padding = m_Style[OverlayStyle_ContentAreaPadding],
                                                      .ChildGap = m_Style[OverlayStyle_ChildGap]});
}

bool Overlay::BeginItemTooltip()
{
    if (!m_Current->CheckFlags(OverlayWindowFlag_Hovered) || !m_Current->Layout.IsHovered(m_LastWidget, m_MousePos))
        return false;

    BeginTooltip();
    return true;
}

void Overlay::EndTooltip()
{
    TKIT_ASSERT(m_Tooltip.Active, "[ONYX][UI] Cannot end a tooltip that has not started");
    m_Tooltip.Active = false;
    m_Tooltip.Layout.EndPanel();
    m_Tooltip.Layout.EndPanel();
    m_Tooltip.Layout.Compile();
}

bool Overlay::IsItemHovered(const OverlayHoveredFlags flags)
{
    const bool allowBlockWindow = flags & OverlayHoveredFlag_AllowBlockedByWindow;
    const bool shortDelay = flags & OverlayHoveredFlag_ShortDelay;
    const bool normalDelay = flags & OverlayHoveredFlag_NormalDelay;
    const bool stationary = flags & OverlayHoveredFlag_Stationary;
    TKIT_ASSERT(normalDelay + shortDelay != 2,
                "[ONYX][UI] Cannot have short delay and normal delay at the same time in tooltip");

    f32 delay = 0.f;
    if (shortDelay)
        delay = m_Style[OverlayStyle_HoverDelayShort];
    else if (normalDelay)
        delay = m_Style[OverlayStyle_HoverDelayNormal];

    const Layout &ly = getCurrentLayout();

    const bool candidate = (allowBlockWindow || m_Current->CheckFlags(OverlayWindowFlag_Hovered)) &&
                           ly.IsHovered(m_LastWidget, m_MousePos);
    if (candidate)
    {
        const f32 statThres = stationary ? m_Style[OverlayStyle_HoverStationaryThreshold] : TKIT_F32_MAX;
        if (Math::NormSquared(m_MouseDelta) > statThres)
        {
            m_WidgetHoverClock.Restart();
            return false;
        }

        const bool wasCandidate = m_HoveredWidgetCandidate == m_LastWidget;
        const bool noShared = flags & OverlayHoveredFlag_NoSharedDelay;
        if (m_HoveredWidgetCandidate == NullLayoutId || (noShared && !wasCandidate))
            m_WidgetHoverClock.Restart();

        m_HoveredWidgetCandidate = m_LastWidget;
        m_CandidateLayout = &ly;

        const f32 seconds = m_WidgetHoverClock.GetElapsed().AsSeconds();
        return seconds >= delay;
    }
    return false;
}

bool Overlay::InputText(TKit::StringView label, char *buf, const u32 size, const TKit::StringView hint,
                        const OverlayInputFlags flags)
{
    Layout &ly = getCurrentLayout();
    ly.BeginPanel(PushId(label), LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                       .Sizing = {grow(300.f), fit()},
                                                       .ChildGap = m_Style[OverlayStyle_ChildGap]});

    m_LastWidget =
        ly.BeginPanel(AsStackedId("Container"), LayoutPanelParameters{.Alignment = {Alignment_Left, Alignment_Center},
                                                                      .Sizing = {snorm(0.6f), fit()},
                                                                      .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const bool updated = inputTextBox(buf, size, hint, flags);
    ly.EndPanel();
    ly.Text(trimLabel(label), getTextParams());
    ly.EndPanel();
    PopId();
    return updated;
}

void Overlay::Draw()
{
    TKIT_ASSERT(
        m_IdStack.IsEmpty(),
        "[ONYX][UI] Id stack size mismatch (size = {}, should be 0). For every PushId(), there must be a PopId()",
        m_IdStack.GetSize());

    processWindows();
    m_Context->Flush();
    for (OverlayWindow &win : m_OverlayWindows)
    {
        win.Layout.Compile();
        m_Context->Layout(win.Layout);
    }
    TKIT_ASSERT(!m_Tooltip.Active, "[ONYX][UI] Found a tooltip that has not been finished");
    if (m_Tooltip.Drawn)
    {
        // tooltip must be compiled everytime it is used
        m_Context->Layout(m_Tooltip.Layout);
        m_Tooltip.Drawn = false;
    }
}

template <typename F> void Overlay::iterateReverseWindows(const F func)
{
    for (u32 i = m_OverlayWindows.GetSize() - 1; i < m_OverlayWindows.GetSize(); --i)
        if (!func(m_OverlayWindows[i]))
            return;
}

void Overlay::processWindows()
{
    const f32v2 mpos = getMousePos();

    m_MouseDelta = mpos - m_MousePos;
    m_MousePos = mpos;

    m_EventFlags = 0;

    m_EventKeys.ClearAll();

    if (m_FocusedInputter == NullLayoutId)
        m_DelayedFocusedInputter = NullLayoutId;
    if (m_PressedClicker == NullLayoutId)
        m_DelayedPressedClicker = NullLayoutId;

    const bool pressingWidget = m_PressedClicker != NullLayoutId || m_PressedDragger != NullLayoutId;
    // if nothing is grabbed, we check mouse cursors here
    if (!m_Grabbed && !pressingWidget)
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
                if (win.Layout.IsHovered(id, m_MousePos, m_Style[OverlayStyle_BorderHoverPadding]))
                {
                    win.Resize.InteractionColor = &m_Style[OverlayColor_WindowBorderHovered];
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

    // remove some state and check whether the window is collapsed
    for (OverlayWindow &win : m_OverlayWindows)
    {
        const bool collapsed = Math::Approximately(win.Size[1], win.MinSize[1], 1.f);
        win.HeaderIcon = collapsed ? m_CollapsedHeaderIcon : m_ExpandedHeaderIcon;
        win.RemoveFlags(OverlayWindowFlag_Hovered);
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
            m_EventFlags |= OverlayEventFlag_MousePressed;
            m_FocusedInputter = NullLayoutId;
        }
        else if (ev.Type == Event_MouseReleased)
        {
            m_EventFlags |= OverlayEventFlag_MouseReleased;
            if (m_ClickClock.Restart().AsMilliseconds() <= m_Style[OverlayStyle_ClickMilliseconds])
                ++m_OverflowClicks;
            m_PressedClicker = NullLayoutId;
            m_PressedDragger = NullLayoutId;
        }
        else if (ev.Type == Event_Scrolled)
            scroll = m_Style[OverlayStyle_ScrollSensitivity] * ev.ScrollOffset[1];
        else if (ev.Type == Event_CharInput)
        {
            char buf[4];
            const u32 count = EncodeUTF8(buf, ev.Character);
            textInput.Insert(textInput.end(), buf, buf + count);
        }
        else if (ev.Type == Event_KeyPressed || ev.Type == Event_KeyRepeat)
            m_EventKeys.Set(ev.Key);
    }

    if (m_ClickClock.GetElapsed().AsMilliseconds() > m_Style[OverlayStyle_ClickMilliseconds])
        m_OverflowClicks = 0;

    bool canAssignHover = true;

    u32 idx = m_OverlayWindows.GetSize();
    // go through the windows to 1. assign the mouse events to the uppermost hovered window and 2. assign such
    // window as grabbed if user pressed the mouse
    const bool pressed = m_EventFlags & OverlayEventFlag_MousePressed;
    iterateReverseWindows([&](OverlayWindow &win) {
        --idx;
        const bool winHovered =
            canAssignHover && win.Layout.IsHovered(win.Id, m_MousePos, m_Style[OverlayStyle_BorderHoverPadding]);
        const bool inputHovered = win.CheckFlags(OverlayWindowFlag_InputHovered);

        if (pressed)
            win.TextHighlightSize = 0.f;

        win.Flags |= wflags;
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
        m_Grabbed = nullptr;
    else if (m_Grabbed)
    {
        m_Grabbed->Resize.InteractionColor = &m_Style[OverlayColor_WindowBorderPressed];
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
    if (m_CandidateLayout && !m_CandidateLayout->IsHovered(m_HoveredWidgetCandidate, m_MousePos))
        m_HoveredWidgetCandidate = NullLayoutId;
}

void Overlay::bringWindowToTop(const u32 idx)
{
    const OverlayWindow target = std::move(m_OverlayWindows[idx]);
    m_OverlayWindows.RemoveOrdered(m_OverlayWindows.begin() + idx);
    m_OverlayWindows.Append(target);

    const usz id = m_WindowIds[idx];
    m_WindowIds.RemoveOrdered(m_WindowIds.begin() + idx);
    m_WindowIds.Append(id);
}

OverlayWindow *Overlay::getOrCreateOverlayWindow(const usz id)
{
    for (u32 i = 0; i < m_WindowIds.GetSize(); ++i)
        if (id == m_WindowIds[i])
            return &m_OverlayWindows[i];

    m_WindowIds.Append(id);
    OverlayWindow &win = m_OverlayWindows.Append(m_LayoutSpecs);
    win.HeaderIcon = m_ExpandedHeaderIcon;
    win.Position += m_WindowSpawnOffset;
    m_WindowSpawnOffset += m_Style[OverlayStyle_WindowSpawnDelta];
    return &win;
}

const FontData &Overlay::getFontData() const
{
    const Resource font = m_Context->GetState().Font;
    return Resources::GetFontData(font);
}
f32 Overlay::computeWindowMinSize(const f32 winPadding, const f32 headPadding, const f32 fontSize) const
{
    const FontData &fdata = getFontData();
    return fontSize * fdata.LineHeight + 2.f * (winPadding + headPadding);
}

f32v2 Overlay::getMousePos() const
{
    return m_View->ScreenToWorld(m_Window->GetNormalizedMousePosition());
}

} // namespace Onyx
