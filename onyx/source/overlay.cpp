#include "pch.hpp"
#include "onyx/overlay.hpp"
#include "onyx/onyx.hpp"
#include "onyx/platform.hpp"
#include "onyx/renderer.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx
{
/////////////////////////////////////////////
/// GLOBALS
/////////////////////////////////////////////

static constexpr f32 s_CheckboardLight = 0.5f;
static constexpr f32 s_CheckboardDark = 0.3f;
static const LayoutId s_MenuBarId = LayoutId{"__onyx_id_Menu_bar"};

/////////////////////////////////////////////
/// END GLOBALS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// GENERAL
/////////////////////////////////////////////

enum StateFlagBit : StateFlags
{
    StateFlag_ActiveIdMustPersist = 1U << 0,
    StateFlag_PressedIdMustPersist = 1U << 1,
    StateFlag_DraggedIdMustPersist = 1U << 2,
    StateFlag_ActiveAllowsInteraction = 1U << 3,

    StateFlag_MustCollapsePopups = 1U << 4,
    StateFlag_FocusBlockByPopupCollapse = 1U << 5,
    StateFlag_PopupProtectionForbidden = 1U << 6,
    StateFlag_MainMenuBarActive = 1U << 7,
    StateFlag_MenuBarActive = 1U << 8,
    StateFlag_Disabled = 1U << 9,

    StateFlag_RequestCaptureMouse = 1U << 10,
    StateFlag_RequestCaptureKeyboard = 1U << 11,

    StateFlag_WantCaptureMouse = 1U << 12,
    StateFlag_WantCaptureKeyboard = 1U << 13,

    StateFlag_DragPayloadAccepted = 1U << 14,
    StateFlag_DragPayloadRejected = 1U << 15,
    StateFlag_ActivePromotedFloatElement = 1U << 16,
    // we include all flags except for the active allows interaction. that one is only cleared when active id is cleared
    StateFlagPersist = StateFlag_ActiveAllowsInteraction | StateFlag_FocusBlockByPopupCollapse | StateFlag_Disabled |
                       StateFlag_WantCaptureMouse | StateFlag_WantCaptureKeyboard
};

/////////////////////////////////////////////
/// END GENERAL
/////////////////////////////////////////////

/////////////////////////////////////////////
/// STYLING
/////////////////////////////////////////////

OverlayStyleVariables CreateDefaultOverlayVariables()
{
    OverlayStyleVariables vars;
    vars[OverlayStyle_FontSize] = 14.f;
    vars[OverlayStyle_ChildGap] = 8.f;

    vars[OverlayStyle_HeaderRadius] = 0.f;
    vars[OverlayStyle_MenuBarRadius] = 0.f;

    vars[OverlayStyle_DropDownRadius] = 0.f;
    vars[OverlayStyle_DropDownPopupRadius] = 0.f;

    vars[OverlayStyle_DragThreshold] = 16.f;
    vars[OverlayStyle_DragOutlineWidth] = 0.3f;

    vars[OverlayStyle_ScrollAreaBorderRadius] = 0.f;
    vars[OverlayStyle_TreeRadius] = 0.f;
    vars[OverlayStyle_InputBoxRadius] = 0.f;
    vars[OverlayStyle_ButtonRadius] = 0.f;
    vars[OverlayStyle_CheckBoxRadius] = 0.f;
    vars[OverlayStyle_SelectableRadius] = 0.f;
    vars[OverlayStyle_SelectableCheckBoxRadius] = 0.f;
    vars[OverlayStyle_TooltipRadius] = 0.f;
    vars[OverlayStyle_ImageRadius] = 0.f;
    vars[OverlayStyle_TabRadius] = 0.f;

    vars[OverlayStyle_TabPadding] = 3.f;
    vars[OverlayStyle_TabGap] = 4.f;

    vars[OverlayStyle_LineRadius] = 0.f;
    vars[OverlayStyle_LineWidth] = 4.f;
    vars[OverlayStyle_SeparatorTextOffset] = 20.f;

    vars[OverlayStyle_SliderRadius] = 0.f;
    vars[OverlayStyle_SliderInnerRadius] = 0.f;

    vars[OverlayStyle_VerticalSliderWidth] = 24.f;
    vars[OverlayStyle_VerticalSliderHeight] = 160.f;

    vars[OverlayStyle_Alpha] = 1.f;
    vars[OverlayStyle_DisabledAlpha] = 0.8f;

    vars[OverlayStyle_ListBoxMaxHeight] = 140.f;

    vars[OverlayStyle_TooltipOffset] = 12.f;
    vars[OverlayStyle_TooltipPadding] = 2.f;

    vars[OverlayStyle_MainMenuBarPadding] = 4.f;
    vars[OverlayStyle_MinimumMenuWidth] = 150.f;

    vars[OverlayStyle_WindowPadding] = 8.f;
    vars[OverlayStyle_WindowBorderWidth] = 3.f;
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

    vars[OverlayStyle_MenuPadding] = 4.f;
    vars[OverlayStyle_TreeLineWidth] = 4.f;

    vars[OverlayStyle_ClickMilliseconds] = 200.f;
    vars[OverlayStyle_CursorWidth] = 2.f;

    vars[OverlayStyle_DropDownHeightSmall] = 80.f;
    vars[OverlayStyle_DropDownHeightRegular] = 200.f;

    vars[OverlayStyle_HoverDelayShort] = 0.15f;
    vars[OverlayStyle_HoverDelayNormal] = 0.40f;
    vars[OverlayStyle_HoverStationaryThreshold] = 5.f;

    vars[OverlayStyle_HintOpacity] = 0.6f;
    vars[OverlayStyle_CursorOpacity] = 0.6f;

    vars[OverlayStyle_ColorPreviewSize] = 64.f;
    vars[OverlayStyle_ColorTooltipSize] = 96.f;
    vars[OverlayStyle_ColorDragTooltipSize] = 32.f;

    vars[OverlayStyle_ColorPickerSize] = 196.f;
    vars[OverlayStyle_ColorPickerPreviewSize] = 64.f;
    vars[OverlayStyle_ColorPickerTooltipSize] = 96.f;

    return vars;
}

static Color hex(const TKit::StringView h)
{
    return Color::FromHexadecimal(h);
}
static Color rgba(const f32 r, const f32 g, const f32 b, const f32 a = 1.f)
{
    return Color{r, g, b, a};
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

    palette[OverlayPalette_Inner0] = hex("D4885B");
    palette[OverlayPalette_Inner1] = hex("E09A6C");

    palette[OverlayPalette_Background0] = hex("3A2828");
    palette[OverlayPalette_Background1] = hex("4A3535");
    palette[OverlayPalette_Background2] = hex("362525");

    return palette;
}

OverlayPalette CreateBabyBlueOverlayPalette()
{
    OverlayPalette palette;
    palette[OverlayPalette_Idle0] = rgba(0.20f, 0.22f, 0.27f);
    palette[OverlayPalette_Idle1] = rgba(0.25f, 0.28f, 0.35f);
    palette[OverlayPalette_Idle2] = rgba(0.22f, 0.24f, 0.30f);

    palette[OverlayPalette_Hovered0] = rgba(0.30f, 0.33f, 0.40f);
    palette[OverlayPalette_Hovered1] = rgba(0.36f, 0.40f, 0.48f);
    palette[OverlayPalette_Hovered2] = rgba(0.26f, 0.29f, 0.36f);
    palette[OverlayPalette_Hovered3] = rgba(0.33f, 0.37f, 0.45f);

    palette[OverlayPalette_Pressed0] = rgba(0.20f, 0.38f, 0.58f);
    palette[OverlayPalette_Pressed1] = rgba(0.18f, 0.34f, 0.52f);
    palette[OverlayPalette_Pressed2] = rgba(0.23f, 0.42f, 0.62f);

    palette[OverlayPalette_Text0] = rgba(0.86f, 0.93f, 0.89f);

    palette[OverlayPalette_Inner0] = rgba(0.26f, 0.54f, 0.85f);
    palette[OverlayPalette_Inner1] = rgba(0.32f, 0.60f, 0.90f);

    palette[OverlayPalette_Background0] = rgba(0.13f, 0.14f, 0.17f);
    palette[OverlayPalette_Background1] = rgba(0.20f, 0.22f, 0.27f);
    palette[OverlayPalette_Background2] = rgba(0.10f, 0.11f, 0.14f);

    return palette;
}

OverlayColors CreateOverlayColorsFromPalette(const OverlayPalette &palette)
{
    OverlayColors colors;

    colors[OverlayColor_None] = Color_Transparent;
    colors[OverlayColor_Text] = palette[OverlayPalette_Text0];
    colors[OverlayColor_Line] = palette[OverlayPalette_Background1];

    colors[OverlayColor_DragOutline] = palette[OverlayPalette_Background1];

    colors[OverlayColor_InputCursor] = palette[OverlayPalette_Text0];
    colors[OverlayColor_InputHighlight] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_InputBackground] = palette[OverlayPalette_Background2];

    colors[OverlayColor_WindowBorderIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_WindowBorderHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_WindowBorderPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_Header] = palette[OverlayPalette_Text0];

    colors[OverlayColor_ButtonIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_ButtonHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_ButtonPressed] = palette[OverlayPalette_Pressed0];

    colors[OverlayColor_CheckBoxIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_CheckBoxHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_CheckBoxPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_CheckBoxInner] = palette[OverlayPalette_Inner0];

    colors[OverlayColor_SliderIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_SliderHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_SliderPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_SliderInner] = palette[OverlayPalette_Inner0];

    colors[OverlayColor_DragIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_DragHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_DragPressed] = palette[OverlayPalette_Pressed0];

    colors[OverlayColor_ScrollBarIdle] = palette[OverlayPalette_Idle1];
    colors[OverlayColor_ScrollBarHovered] = palette[OverlayPalette_Hovered1];
    colors[OverlayColor_ScrollBarPressed] = palette[OverlayPalette_Inner0];
    colors[OverlayColor_ScrollAreaBorders] = palette[OverlayPalette_Background1];

    colors[OverlayColor_ProgressBarBackground] = palette[OverlayPalette_Background2];
    colors[OverlayColor_ProgressBarInner] = palette[OverlayPalette_Pressed0];

    colors[OverlayColor_TreeIdle] = palette[OverlayPalette_Background1];
    colors[OverlayColor_TreeHovered] = palette[OverlayPalette_Hovered2];
    colors[OverlayColor_TreePressed] = palette[OverlayPalette_Pressed1];

    colors[OverlayColor_DropDownIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_DropDownHovered] = palette[OverlayPalette_Hovered0];
    colors[OverlayColor_DropDownPressed] = palette[OverlayPalette_Pressed0];
    colors[OverlayColor_DropDownButton] = palette[OverlayPalette_Inner0];

    colors[OverlayColor_SelectableIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_SelectableHovered] = palette[OverlayPalette_Hovered3];
    colors[OverlayColor_SelectablePressed] = palette[OverlayPalette_Pressed2];

    colors[OverlayColor_MenuItemIdle] = palette[OverlayPalette_Idle0];
    colors[OverlayColor_MenuItemHovered] = palette[OverlayPalette_Hovered3];
    colors[OverlayColor_MenuItemPressed] = palette[OverlayPalette_Pressed2];
    colors[OverlayColor_MenuBoxBackground] = palette[OverlayPalette_Background2];

    colors[OverlayColor_PopupBackground] = palette[OverlayPalette_Background2];

    colors[OverlayColor_WindowBackgroundExpanded] = palette[OverlayPalette_Background0];
    colors[OverlayColor_WindowBackgroundCollapsed] = palette[OverlayPalette_Background0];

    colors[OverlayColor_HeaderBackgroundExpanded] = palette[OverlayPalette_Background1];
    colors[OverlayColor_HeaderBackgroundCollapsed] = palette[OverlayPalette_Background1];

    colors[OverlayColor_MenuBarBackground] = palette[OverlayPalette_Background1];

    return colors;
}

/////////////////////////////////////////////
/// END STYLING
/////////////////////////////////////////////

/////////////////////////////////////////////
/// WINDOW FLAGS
/////////////////////////////////////////////

enum ResizeFlagBit : ResizeFlags
{
    ResizeFlag_Left = 1U << 0,
    ResizeFlag_Right = 1U << 1,
    ResizeFlag_Bottom = 1U << 2,
    ResizeFlag_Top = 1U << 3,
};
enum NativeWindowFlagBit : NativeWindowFlags
{
    NativeWindowFlag_LeftMousePressed = 1U << 0,
    NativeWindowFlag_LeftMouseReleased = 1U << 1,

    NativeWindowFlag_RightMousePressed = 1U << 2,
    NativeWindowFlag_RightMouseReleased = 1U << 3,

    // required bc immediate queries to the window cause widgets to see the mouse pressed before the actual mouse
    // pressed event. this is important for elements that if they are active think they are currently pressed,
    // causing the firs mouse click outside their bounding box to still qualify as pressed
    NativeWindowFlag_PressingLeftMouse = 1U << 4,
    //

    // used when os window updates size so that child overlay window can follow along
    NativeWindowFlag_WantUpdateSize = 1U << 6,

    NativeWindowFlag_RepresentsFloatElement = 1U << 7,
    NativeWindowFlag_ActivePromotedFloatElement = 1U << 8,

    // this flag allows for persisting the m_Grabbed field when promoting windows. this currently doesnt work in linux
    // as glfw fires a mouse release event on window creation
    NativeWindowFlag_CheckParentForGrab = 1U << 9,

    NativeWindowFlagPersist = NativeWindowFlag_PressingLeftMouse | NativeWindowFlag_RepresentsFloatElement |
                              NativeWindowFlag_ActivePromotedFloatElement | NativeWindowFlag_CheckParentForGrab,
};

enum WindowInternalFlagBit : OverlayWindowFlags
{
    WindowInternalFlag_Hovered = 1U << 0,
    WindowInternalFlag_Focused = 1U << 1,
    WindowInternalFlag_InputHovered = 1U << 2,
    WindowInternalFlag_MenuBarOpened = 1U << 3,
    WindowInternalFlag_ClosePopupButton = 1U << 4,
    WindowInternalFlag_Active = 1U << 5,
    WindowInternalFlag_OwnsNative = 1U << 6,
    WindowInternalFlag_MainMenuBar = 1U << 7,

    // when collapsed, let os window know it must adapt its size
    WindowInternalFlag_WantUpdateSize = 1U << 8,

    // this... is a one use flag needed because the layout system queries with one frame of delay. used when auto resize
    // is at play and we need to sync the derived size with the reported window size
    WindowInternalFlag_AllowForLayoutToCatchUp = 1U << 9,
    WindowInternalFlagPersist = WindowInternalFlag_Hovered | WindowInternalFlag_Focused |
                                WindowInternalFlag_InputHovered | WindowInternalFlag_Active |
                                WindowInternalFlag_MainMenuBar | WindowInternalFlag_OwnsNative |
                                WindowInternalFlag_AllowForLayoutToCatchUp,
};

/////////////////////////////////////////////
/// END WINDOW FLAGS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// INITIALIZATION
/////////////////////////////////////////////

Overlay::Overlay(Window *win, const OverlaySpecs &specs)
    : m_Flags(specs.Flags), m_LayoutSpecs(specs.Layout), m_Style(specs.Style), m_DefaultStyle(specs.Style)
{
    TKIT_ASSERT(
        specs.Layout.RootAlignment[0] == Alignment_Left && specs.Layout.RootAlignment[1] == Alignment_Top,
        "[ONYX][OVERLAY] Root alignment for layouts must be Top Left. Other alignments are not supported for root");

    m_Camera.Mode = CameraMode_Viewport;
    if (win)
    {
        NativeWindow *nw = createNativeWindow(win);
        nw->ScreenPos = f32v2{nw->Window->GetPosition()};
    }
    else
        m_Flags |= OverlayFlag_WindowPromotions | OverlayFlag_FloatingMode;

    for (u32 i = 0; i < m_DynamicMeshes.GetSize(); ++i)
        m_DynamicMeshes[i] = Resources::RegisterDynamicMesh<D2>();

    Resources::Sync(SyncFlag_DynamicMeshes);
}

Overlay::~Overlay()
{
    for (const OverlayWindow &win : m_OverlayWindows)
        if (win.Flags & WindowInternalFlag_OwnsNative)
            destroyNativeWindow(win.Native);

    for (NativeWindow *nw : m_NativeWindows)
        if (nw->Flags & NativeWindowFlag_RepresentsFloatElement)
            destroyNativeWindow(nw);

    for (u32 i = 0; i < m_DynamicMeshes.GetSize(); ++i)
        if (Resources::IsResourceValid<D2>(m_DynamicMeshes[i].Handle, Resource_DynamicMesh))
            Resources::DestroyDynamicMesh<D2>(m_DynamicMeshes[i].Handle);
}

/////////////////////////////////////////////
/// END INITIALIZATION
/////////////////////////////////////////////

/////////////////////////////////////////////
/// WINDOWS/MENUS
/////////////////////////////////////////////

const f32v2 &OverlayWindow::GetActivePosition() const
{
    return (Flags & WindowInternalFlag_OwnsNative) ? Native->ScreenPos : ScreenPos;
}
f32v2 &OverlayWindow::GetActivePosition()
{
    return (Flags & WindowInternalFlag_OwnsNative) ? Native->ScreenPos : ScreenPos;
}
void OverlayWindow::SetActivePosition(const f32v2 &pos)
{
    // lol
    ((Flags & WindowInternalFlag_OwnsNative) ? Native->ScreenPos : ScreenPos) = pos;
}
void OverlayWindow::ClampToNative()
{
    // only works if window does not own native. we may adapt this if we want to clamp per monitor
    const NativeWindow *nw = Native;
    const f32v2 dims = nw->GetDimensions();

    const f32v2 mnlim = MinSize - Size;
    const f32v2 mxlim = dims - MinSize;

    ScreenPos = Math::Clamp(ScreenPos, mnlim, mxlim);
}
void OverlayWindow::SyncNativeSize()
{
    if (!(Flags & WindowInternalFlag_OwnsNative))
        return;

    if (!Math::Approximately(Size, Native->Size, 1.f))
        Flags |= WindowInternalFlag_WantUpdateSize;
}
bool OverlayWindow::IsFullscreenBlocked() const
{
    return (Flags & WindowInternalFlag_OwnsNative) && Native->Window->IsFullScreen();
}
bool OverlayWindow::CanResize() const
{
    return !(Flags & (OverlayWindowFlag_NoResize | OverlayWindowFlag_AutoResize)) && !IsFullscreenBlocked();
}
bool OverlayWindow::CanMove() const
{
    return !(Flags & OverlayWindowFlag_NoMove) && !IsFullscreenBlocked();
}
bool OverlayWindow::CanCollapse() const
{
    return !(Flags & (OverlayWindowFlag_NoCollapse | OverlayWindowFlag_NoHeaderBar)) && !IsFullscreenBlocked();
}

f32v2 OverlayWindow::ToScreen(const f32v2 &world) const
{
    if (Flags & WindowInternalFlag_OwnsNative)
        return Native->ToScreen(world);

    return Native->View->ToAbsolute(Native->View->WorldToScreen(world));
}
f32v2 OverlayWindow::ToWorld(const f32v2 &screen) const
{
    if (Flags & WindowInternalFlag_OwnsNative)
        return Native->ToWorld(screen);

    return Native->View->ScreenToWorld(Native->View->ToNormalized(screen));
}

bool Overlay::BeginWindow(const TKit::StringView title, bool *opened, OverlayWindowFlags flags)
{
    if (opened && !(*opened))
        return false;

    const LayoutId id = title; /* forcing titles to be unique for now */ // PushId(title);
    const LayoutId nid = PushId(id);

    OverlayWindow *win = getOrCreateOverlayWindow(id);

    const bool ownsNative = win->Flags & WindowInternalFlag_OwnsNative;
    // if ownsNative is true, ->Native cannot be null
    if (m_NextWindow.Flags & NextWindowFlag_Position)
    {
        if (ownsNative)
        {
            win->SetActivePosition(m_NextWindow.ScreenPos);
            win->Native->Window->SetPosition(i32v2{m_NextWindow.ScreenPos});
        }
        else
            win->ScreenPos = m_NextWindow.ScreenPos;
    }
    if (m_NextWindow.Flags & NextWindowFlag_Size)
    {
        win->Size = m_NextWindow.Size;
        if (ownsNative)
            win->SyncNativeSize();
    }
    m_NextWindow.Flags = 0;

    assignNativeWindowSomehow(win);

    m_Current = win;
    m_Current->PopupDepth = m_CurrentPopupDepth;

    // we just dont support auto resize if we are fullscreen
    if (m_Current->IsFullscreenBlocked())
        flags &= ~OverlayWindowFlag_AutoResize;

    if (flags & OverlayWindowFlag_BringToTop)
        m_Current->Layer = toTop();
    if (flags & OverlayWindowFlag_Modal)
        m_ModalCollapseDepth = Math::Max(m_ModalCollapseDepth, m_CurrentPopupDepth);

    const bool noCollapse = !m_Current->CanCollapse();
    const bool noHeader = flags & OverlayWindowFlag_NoHeaderBar;
    const bool collapsed = !noCollapse && m_Current->HeaderIcon == ArrowRightIcon;

    m_WindowStack.Append(m_Current);
    if (m_Current->Flags & WindowInternalFlag_Active)
    {
        // window is still active. user can still append to it
        if (collapsed)
            EndWindow();
        return !collapsed;
    }
    addActiveWindow(m_Current);

    const bool wasAutoresize = m_Current->Flags & OverlayWindowFlag_AutoResize;

    m_Current->Flags &= WindowInternalFlagPersist;
    m_Current->MinSize =
        getLineHeight() + 2.f * (m_Style[OverlayStyle_WindowPadding] + m_Style[OverlayStyle_HeaderPadding]);
    m_Current->Flags |= flags;

    Layout &ly = GetCurrentLayout();

    const bool autoResize = flags & OverlayWindowFlag_AutoResize;

    const usz scrollId = IdFromStack(nid);
    ScrollInfo &sinfo = m_Scrollables[scrollId];

    NativeWindow *nw = m_Current->Native;
    if (autoResize && !wasAutoresize)
        m_Current->SizeBeforeAutoResize = m_Current->Size;
    else if (!autoResize && wasAutoresize)
        m_Current->Size = m_Current->SizeBeforeAutoResize;

    if (autoResize)
    {
        sinfo = ScrollInfo{};

        if (!(m_Current->Flags & WindowInternalFlag_AllowForLayoutToCatchUp))
        {
            const LayoutElement *elm = ly.QueryElement(m_Current->Id);
            if (elm)
                m_Current->Size = elm->Size;
        }
        else
            m_Current->Flags &= ~WindowInternalFlag_AllowForLayoutToCatchUp;

        m_Current->SyncNativeSize();
    }
    else if (wasAutoresize)
        m_Current->SyncNativeSize();

    // ugly
    const LySz2 sizing = [&]() -> LySz2 {
        if (autoResize)
            return collapsed ? LySz2{fit(), sabs(m_Current->Size[1])} : LySz2{fit()};

        return sabs(m_Current->Size);
    }();

    const f32v2 pos = ownsNative ? nw->GetWorldTopLeft() : m_Current->ToWorld(m_Current->ScreenPos);

    m_Current->Id = ly.BeginPanel(
        id,
        LyPnPar{.FillColor =
                    m_Style[collapsed ? OverlayColor_WindowBackgroundCollapsed : OverlayColor_WindowBackgroundExpanded],
                .Direction = LayoutDirection_TopToBottom,
                .Alignment = TopLeft,
                .Sizing = sizing,
                .SelfOffset = oabs(pos),
                .Padding = m_Style[OverlayStyle_WindowPadding],
                .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const LayoutElement *elm = ly.QueryElement(id);
    queryAndSetFocusStatus(elm, FocusFlag_DoNotSetHoveredId | FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetActiveId);
    m_LastItem = id;

    drawWindowBorders();
    if (!noHeader)
    {
        ly.BeginPanel(LyPnPar{
            .FillColor =
                m_Style[collapsed ? OverlayColor_HeaderBackgroundCollapsed : OverlayColor_HeaderBackgroundExpanded],
            .Alignment = CenterLeft,
            .Sizing = {flex(), fit()},
            .Shape = rect(m_Style[OverlayStyle_HeaderRadius])});

        ly.BeginPanel(LyPnPar{.Alignment = CenterLeft,
                              .Sizing = {autoResize ? flex() : grow(), fit()},
                              .Padding = m_Style[OverlayStyle_HeaderPadding],
                              .ChildGap = m_Style[OverlayStyle_ChildGap]});

        if (!noCollapse && iconButton(IdFromStack("__onyx_id_Collapse_button"), m_Current->HeaderIcon))
        {
            m_Current->Flags |= WindowInternalFlag_AllowForLayoutToCatchUp;
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
            m_Current->SyncNativeSize();
        }

        ly.Text(ly.GenerateNextId(), trimLabel(title), getTextParams());
        ly.EndPanel();

        if (!(flags & OverlayWindowFlag_NoCloseButton))
        {
            const LayoutId bid = IdFromStack("__onyx_id_Close_button");
            if (opened && (iconButton(bid, CrossIcon) || (ownsNative && nw->Window->ShouldClose())))
                *opened = false;
            else if ((flags & WindowInternalFlag_ClosePopupButton) &&
                     (iconButton(bid, CrossIcon) || (ownsNative && nw->Window->ShouldClose())))
                CloseCurrentPopup();
        }

        ly.EndPanel();
    }

    if (flags & OverlayWindowFlag_MenuBar)
    {
        ly.BeginPanel(s_MenuBarId, LyPnPar{.FillColor = m_Style[OverlayColor_MenuBarBackground],
                                           .Direction = LayoutDirection_LeftToRight,
                                           .Alignment = CenterLeft,
                                           .Sizing = {flex(), fit()},
                                           .Shape = rect(m_Style[OverlayStyle_MenuBarRadius])});
        // To be opened by menu bar calls
        ly.EndPanel();
    }

    const LySz2 scrollSizing = autoResize ? flex() : grow();
    const f32 cpadding = m_Style[OverlayStyle_ContentAreaPadding];
    beginScroll({.Id = scrollId,
                 .OuterSizing = scrollSizing,
                 .ContentSizing = scrollSizing,
                 .ContentPadding = cpadding,
                 .ChildGap = m_Style[OverlayStyle_ChildGap],
                 .Flags = flags | OverlayScrollFlag_NoBackground});

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
    popWindowStack();
    PopId();
}

bool Overlay::BeginMenuBar()
{
    if (!(m_Current->Flags & OverlayWindowFlag_MenuBar))
        return false;

    PushId(s_MenuBarId);
    m_Current->Layout.OpenPanel(s_MenuBarId);
    m_Current->Flags |= WindowInternalFlag_MenuBarOpened;
    m_StateFlags |= StateFlag_MenuBarActive;
    return true;
}
void Overlay::EndMenuBar()
{
    m_Current->Flags &= ~WindowInternalFlag_MenuBarOpened;
    m_Current->Layout.EndPanel();
    m_StateFlags &= ~StateFlag_MenuBarActive;
    PopId();
}

bool Overlay::BeginMainMenuBar()
{
    if (m_Flags & OverlayFlag_FloatingMode)
        return false;

    const NativeWindow *nw = GetMainNativeWindow();
    const f32v2 tl = nw->GetWorldTopLeft();
    const f32v2 tr = nw->GetWorldTopRight();
    const f32 xsize = tr[0] - tl[0];

    m_Current = getOrCreateOverlayWindow("__onyx_id_Main_menu_bar");
    m_Current->Flags |= WindowInternalFlag_MainMenuBar;
    m_Current->Flags |= OverlayWindowFlag_NoPromotion;

    m_StateFlags |= StateFlag_MainMenuBarActive;
    m_WindowStack.Append(m_Current);

    if (m_Current->Flags & WindowInternalFlag_Active)
        return true;

    addActiveWindow(m_Current);
    Layout &ly = GetCurrentLayout();

    m_Current->Id =
        ly.BeginPanel("__onyx_id_Background", LyPnPar{.FillColor = m_Style[OverlayColor_WindowBackgroundExpanded],
                                                      .Alignment = TopLeft,
                                                      .Sizing = {sabs(xsize), fit()},
                                                      .SelfOffset = oabs(tl),
                                                      .Padding = m_Style[OverlayStyle_MainMenuBarPadding]});

    ly.BeginPanel(PushId("__onyx_id_Main_menu_bar"), LyPnPar{.FillColor = m_Style[OverlayColor_MenuBarBackground],
                                                             .Direction = LayoutDirection_LeftToRight,
                                                             .Alignment = CenterLeft,
                                                             .Sizing = {grow(), fit()},
                                                             .Shape = rect(m_Style[OverlayStyle_MenuBarRadius])});
    return true;
}

void Overlay::EndMainMenuBar()
{
    m_StateFlags &= ~StateFlag_MainMenuBarActive;
    PopId();
    popWindowStack();
}

bool Overlay::BeginMenu(const TKit::StringView label)
{
    Layout &ly = GetCurrentLayout();
    const LayoutId id = PushId(label);
    const LayoutElement *elm = ly.QueryElement(id);

    const bool mmnActive = m_StateFlags & StateFlag_MainMenuBarActive;
    const bool verticalLayout =
        (mmnActive && m_CurrentPopupDepth != 0) ||
        (!mmnActive && (!(m_Current->Flags & WindowInternalFlag_MenuBarOpened) || m_CurrentPopupDepth != 0));

    OverlayColor col = verticalLayout ? OverlayColor_None : OverlayColor_MenuItemIdle;

    const bool openOnHover = verticalLayout || checkWidgetState(s_MenuBarId, WidgetStateFlag_Opened);

    const FocusFlags fflags = openOnHover ? (FocusFlag_HoverOpensPopup | FocusFlag_HoverRequestsPopupCollapse)
                                          : FocusFlag_LeftClickOpensPopup;

    const f32v2 hoverPad = 8.f;
    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, fflags, verticalLayout ? hoverPad : 0.f);

    const bool popupOpen = focusFlags & OverlayFocusQueryFlag_PopupOpen;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_MenuItemPressed;
    else if (popupOpen || (focusFlags & OverlayFocusQueryFlag_Hovered))
        col = OverlayColor_MenuItemHovered;

    const f32 padding = m_Style[OverlayStyle_MenuPadding];

    const LySz2 sizing = {verticalLayout ? flex() : fit(), verticalLayout ? fit() : flex()};
    ly.BeginPanel(id,
                  LyPnPar{.FillColor = m_Style[col], .Alignment = CenterLeft, .Sizing = sizing, .Padding = padding});

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());
    if (verticalLayout)
    {
        ly.Panel(LyPnPar{.Sizing = grow()});
        ly.Unicode(NullLayoutId, ArrowRightIcon, getUnicodeParams());
    }

    if (popupOpen)
    {
        if (!verticalLayout)
            m_WidgetStates[s_MenuBarId] = WidgetStateFlag_Opened;

        const usz bid = IdFromStack("__onyx_id_Menu_box");
        const LayoutElement *belm = ly.QueryElement(bid);
        const f32v2 csize = belm ? belm->Size : f32v2{0.f};

        const f32v2 &ppos = elm->Position;
        const f32v2 &psize = elm->Size;

        LyAtt2 att;
        LyAlg2 alg;
        f32v2 offset;

        const f32v4 borders = getWorldEffectiveBorders();
        const f32 lborder = borders[0];
        const f32 rborder = borders[2];
        const f32 bborder = borders[3];

        if (verticalLayout)
        {
            const bool surpasses = (ppos[0] + psize[0] + csize[0]) > rborder;
            att = LyAtt2{surpasses ? LayoutAttachment_Left : LayoutAttachment_Right, LayoutAttachment_Top};
            alg = surpasses ? TopRight : TopLeft;

            const f32 spill = ppos[1] - csize[1] + psize[1];
            const f32 yoffset = spill < bborder ? (bborder - spill) : 0.f;

            offset = {0.f, yoffset};
        }
        else
        {
            const bool surpasses = (ppos[1] - csize[1]) < bborder;
            att = LyAtt2{LayoutAttachment_Left, surpasses ? LayoutAttachment_Top : LayoutAttachment_Bottom};
            alg = surpasses ? BottomLeft : TopLeft;

            const f32 rspill = ppos[0] + csize[0];
            const f32 lspill = ppos[0];

            f32 xoffset = 0.f;
            if (rspill > rborder)
                xoffset = rborder - rspill;
            else if (lspill < lborder)
                xoffset = lborder - lspill;

            offset = {xoffset, 0.f};
        }

        ly.BeginPanel(bid, LyPnPar{.FillColor = m_Style[OverlayColor_MenuBoxBackground],
                                   .Direction = LayoutDirection_TopToBottom,
                                   .Alignment = TopLeft,
                                   .Sizing = {fit(m_Style[OverlayStyle_MinimumMenuWidth]), fit()},
                                   .SelfOffset = oabs(offset),
                                   .Floating = {.Enable = true, .Attachment = att, .Alignment = alg},
                                   .Padding = padding});

        PushStyleVar(OverlayStyle_WidgetPadding, padding);
        ++m_CurrentPopupDepth;
        queryAndSetFocusStatus(ly.QueryElement(bid), FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetActiveId,
                               hoverPad);
        return true;
    }
    ly.EndPanel();
    PopId();
    return false;
}
void Overlay::EndMenu()
{
    PopStyleVar();
    PopId();
    --m_CurrentPopupDepth;
    Layout &ly = GetCurrentLayout();
    ly.EndPanel();
    ly.EndPanel();
}

bool Overlay::IsCurrentWindowPromoted() const
{
    return m_Current && (m_Current->Flags & WindowInternalFlag_OwnsNative);
}

OverlayWindow *Overlay::getOrCreateOverlayWindow(const LayoutId id)
{
    for (u32 i = 0; i < m_WindowIds.GetSize(); ++i)
        if (id == m_WindowIds[i])
            return &m_OverlayWindows[i];

    m_WindowIds.Append(id);
    OverlayWindow &win = m_OverlayWindows.Append(m_LayoutSpecs);
    win.HeaderIcon = ArrowDownIcon;
    win.Layer = toTop();
    win.Native = getMainNativeWindow(); // which may be null
    m_WindowSpawnOffset += m_Style[OverlayStyle_WindowSpawnDelta];
    return &win;
}

void Overlay::assignNativeWindowSomehow(OverlayWindow *win)
{
    if (win->Native)
        return;

    NativeWindow *nw = getMainNativeWindow();
    if (!nw)
        nw = m_Current ? m_Current->Native : promoteWindow(win, win->ScreenPos, win->Size);
    win->Native = nw;
}

void Overlay::addActiveWindow(OverlayWindow *win)
{
    for (u32 i = 0; i < m_ActiveWindows.GetSize(); ++i)
        if (win->Layer <= m_ActiveWindows[i]->Layer)
        {
            m_ActiveWindows.Insert(m_ActiveWindows.begin() + i, win);
            win->Flags |= WindowInternalFlag_Active;
            return;
        }
    m_ActiveWindows.Append(win);
    win->Flags |= WindowInternalFlag_Active;
    return;
}

void Overlay::drawWindowBorders()
{
    Layout &ly = GetCurrentLayout();
    const OverlayColor interaction = m_Current->Grab.InteractionColor;
    const OverlayColor idle = OverlayColor_WindowBorderIdle;

    GrabInfo &ginfo = m_Current->Grab;

    const bool l = ginfo.Flags & ResizeFlag_Left;
    const bool r = ginfo.Flags & ResizeFlag_Right;
    const bool b = ginfo.Flags & ResizeFlag_Bottom;
    const bool t = ginfo.Flags & ResizeFlag_Top;

    const LySz2 hsizing = {sabs(m_Style[OverlayStyle_WindowBorderWidth]), grow()};
    const LySz2 vsizing = {grow(), sabs(m_Style[OverlayStyle_WindowBorderWidth])};

    const ResizeEdge left = ResizeEdge_Left;
    const ResizeEdge right = ResizeEdge_Right;
    const ResizeEdge bottom = ResizeEdge_Bottom;
    const ResizeEdge top = ResizeEdge_Top;

    const LayoutFloatingParameters fparams = {.Enable = true, .DrawOnTop = false};

    const auto drawLeftBorder = [&] {
        ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight, .Sizing = snorm(1.f), .Floating = fparams});
        ginfo.Ids[left] =
            ly.Panel("__onyx_id_Left", LyPnPar{.FillColor = m_Style[l ? interaction : idle], .Sizing = hsizing});
        ly.EndPanel();
    };
    const auto drawRightBorder = [&] {
        ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight, .Sizing = snorm(1.f), .Floating = fparams});
        ly.Panel(LyPnPar{.Sizing = grow()});
        ginfo.Ids[right] =
            ly.Panel("__onyx_id_Right", LyPnPar{.FillColor = m_Style[r ? interaction : idle], .Sizing = hsizing});
        ly.EndPanel();
    };
    const auto drawBottomBorder = [&] {
        ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_BottomToTop, .Sizing = snorm(1.f), .Floating = fparams});
        ginfo.Ids[bottom] =
            ly.Panel("__onyx_id_Bottom", LyPnPar{.FillColor = m_Style[b ? interaction : idle], .Sizing = vsizing});
        ly.EndPanel();
    };
    const auto drawTopBorder = [&] {
        ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_BottomToTop, .Sizing = snorm(1.f), .Floating = fparams});
        ly.Panel(LyPnPar{.Sizing = grow()});
        ginfo.Ids[top] =
            ly.Panel("__onyx_id_Top", LyPnPar{.FillColor = m_Style[t ? interaction : idle], .Sizing = vsizing});
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

// TODO(Isma): Too much repetition between this and Button()
bool Overlay::iconButton(const LayoutId id, const CodePoint code, const LySz ysizing, const OverlayColor idle)
{
    Layout &ly = GetCurrentLayout();
    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));

    OverlayColor col = idle;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_ButtonPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_ButtonHovered;

    ly.BeginPanel(id, LyPnPar{.FillColor = m_Style[col],
                              .Alignment = Center,
                              .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), ysizing}});

    ly.Unicode(NullLayoutId, code, getUnicodeParams());
    ly.EndPanel();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

void Overlay::popWindowStack()
{
    m_WindowStack.Pop();
    m_Current = m_WindowStack.IsEmpty() ? nullptr : m_WindowStack.GetBack();
}

u32 Overlay::processWindows()
{
    TKIT_PROFILE_NSCOPE("Onyx::Overlay::ProcessWindows");
    TKIT_ASSERT(!m_Current, "[ONYX][OVERLAY] Window stack not properly closed! Current window pointer is not null");
    TKIT_ASSERT(m_CurrentPopupDepth == 0, "[ONYX][OVERLAY] Pop up stack not properly closed! {} entries remaining",
                m_CurrentPopupDepth);
    TKIT_ASSERT(m_WindowStack.IsEmpty(), "[ONYX][OVERLAY] Window stack not properly closed! {} windows remaining",
                m_WindowStack.GetSize());
    TKIT_ASSERT(!m_CurrentTabBar, "[ONYX][OVERLAY] A currently opened tab bar has been detected!");

    u32 modalWindow = 0;
    for (u32 i = 0; i < m_ActiveWindows.GetSize(); ++i)
    {
        m_Current = m_ActiveWindows[i];
        if (m_Current->Flags & WindowInternalFlag_MainMenuBar)
        {
            m_Current->Layout.EndPanel();
            m_Current->Layout.EndPanel();
        }
        else
        {
            endScroll();
            m_Current->Layout.EndPanel();
            if (m_Current->Flags & OverlayWindowFlag_Modal)
                modalWindow = i + 1;
        }
    }
    m_Current = nullptr;

    m_ActiveIdLastFrame = m_ActiveId;
    if (!(m_StateFlags & StateFlag_ActiveIdMustPersist))
    {
        m_ActiveId = NullLayoutId;
        m_StateFlags &= ~StateFlag_ActiveAllowsInteraction;
    }

    if (!(m_StateFlags & StateFlag_PressedIdMustPersist))
        m_PressedId = NullLayoutId;

    if (!(m_StateFlags & StateFlag_DraggedIdMustPersist))
    {
        m_DraggedId = NullLayoutId;
        m_DragDropId = NullLayoutId;
    }

    TKIT_ASSERT(m_PopupCollapseDepth <= m_PopupStack.GetSize(),
                "[ONYX][OVERLAY] Cannot have a popup depth ({}) greater than the popup stack ({})",
                m_PopupCollapseDepth, m_PopupStack.GetSize());

    bool pressingMouse = false;

    NativeWindow *hovered = m_Grabbed ? m_Grabbed->Native : nullptr;
    for (NativeWindow *nw : m_NativeWindows)
    {
        // nw->ScreenPos = f32v2{nw->Window->GetPosition()};
        nw->UpdateBorders();
        if (nw->Flags & NativeWindowFlag_RepresentsFloatElement)
        {
            // forward the events to the parent
            for (const Event &ev : nw->Window->GetNewEvents())
                if (ev.Type == Event_MousePressed || ev.Type == Event_MouseReleased || ev.Type == Event_CharInput ||
                    ev.Type == Event_Scrolled)
                    nw->Parent->Window->PushEvent(ev);

            nw->Window->FlushEvents();
            continue;
        }

        const f32v2 smpos = nw->ScreenPos + nw->Window->GetAbsoluteMousePosition();
        nw->ScreenMouseDelta = smpos - nw->ScreenMousePos;
        nw->ScreenMousePos = smpos;

        const f32v2 mpos = nw->ToWorld(smpos);
        nw->WorldMouseDelta = mpos - nw->WorldMouse;
        nw->WorldMouse = mpos;

        nw->EventKeys.ClearAll();
        pressingMouse |= bool(nw->Flags & NativeWindowFlag_PressingLeftMouse);
        if (!m_Grabbed && (!hovered || hovered->Layer < nw->Layer) && nw->Window->IsHovered())
            hovered = nw;
    }

    if (m_StateFlags & StateFlag_MustCollapsePopups)
    {
        const u32 collapse = Math::Max(m_PopupCollapseDepth, m_ModalCollapseDepth);
        if (pressingMouse && collapse < m_PopupStack.GetSize())
            m_StateFlags |= StateFlag_FocusBlockByPopupCollapse;
        m_PopupStack.Resize(collapse);
    }
    if (!(m_StateFlags & StateFlag_RequestCaptureMouse))
        m_StateFlags &= ~StateFlag_WantCaptureMouse;

    if (!(m_StateFlags & StateFlag_RequestCaptureKeyboard))
        m_StateFlags &= ~StateFlag_WantCaptureKeyboard;

    const bool notAllowed =
        (m_StateFlags & StateFlag_DragPayloadRejected) && !(m_StateFlags & StateFlag_DragPayloadAccepted);

    m_StateFlags &= StateFlagPersist;

    const bool widgetBlocked = m_ActiveId != NullLayoutId && !(m_StateFlags & StateFlag_ActiveAllowsInteraction);
    const bool widgetPressed = m_PressedId != NullLayoutId;
    const bool widgetHovered = m_HoveredId != NullLayoutId;

    m_StateFlags |= widgetBlocked * StateFlag_WantCaptureMouse;
    // if nothing is grabbed, we check mouse cursors here
    if (!m_Grabbed && hovered)
    {
        MouseCursor cursor = notAllowed ? MouseCursor_NotAllowed : MouseCursor_Default;
        iterateReverseWindows([&](OverlayWindow *win) {
            // if hovering a widget or window is not hovered (mouse is not on window) remove any hovering and skip
            const bool winHovered = win->Flags & (WindowInternalFlag_Hovered | WindowInternalFlag_Focused);
            const bool popupBlocked = win->PopupDepth != m_PopupStack.GetSize();
            const bool modalBlocked = win->PopupDepth < m_ModalCollapseDepth;
            if (winHovered && (win->Flags & WindowInternalFlag_InputHovered))
            {
                win->Grab.Flags = 0;
                cursor = MouseCursor_IBeam;
                return false;
            }
            if (!winHovered || widgetHovered || widgetPressed || widgetBlocked || popupBlocked || modalBlocked)
            {
                win->Grab.Flags = 0;
                return true;
            }
            if (!win->CanResize())
                return true;
            // else, check if there is resize hover

            const bool hasHoverPadding = win->PopupDepth == 0 || win->PopupDepth == m_ModalCollapseDepth;
            ResizeFlags rflags = 0;
            GrabInfo &ginfo = win->Grab;
            for (u32 i = 0; i < ginfo.Ids.GetSize(); ++i)
            {
                const usz id = ginfo.Ids[i];
                if (win->Layout.IsHovered(id, hovered->WorldMouse, m_Style[OverlayStyle_BorderHoverPadding],
                                          /* so that popups dont "fakingly" announce a resize*/ hasHoverPadding))
                {
                    win->Grab.InteractionColor = OverlayColor_WindowBorderHovered;
                    rflags |= 1U << i;
                }
            }
            ginfo.Flags = rflags;

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
        hovered->Window->SetMouseCursor(cursor);
    }

    // check for mouse events
    OverlayWindowFlags wflags = 0;
    f32v2 scroll{0.f};

    for (NativeWindow *nw : m_NativeWindows)
    {
        nw->TextInput.Clear();
        nw->Flags &= NativeWindowFlagPersist;
        for (const Event &ev : nw->Window->GetNewEvents())
        {
            if (ev.Type == Event_WindowResized)
            {
                nw->Size = ev.WindowSize;
                nw->Flags |= NativeWindowFlag_WantUpdateSize;
            }
            else if (ev.Type == Event_WindowMoved)
                nw->ScreenPos = ev.WindowPos;
            else if (ev.Type == Event_MouseEntered)
            {
                nw->Layer = toTop();
                if (nw->Owner)
                    nw->Owner->Layer = toTop();
            }
            else if (ev.Type == Event_MousePressed)
            {
                if (ev.Mouse.Button == Mouse_Button1)
                    nw->Flags |= NativeWindowFlag_LeftMousePressed | NativeWindowFlag_PressingLeftMouse;
                if (ev.Mouse.Button == Mouse_Button2)
                    nw->Flags |= NativeWindowFlag_RightMousePressed;
                requestCollapsePopups();
            }
            else if (ev.Type == Event_MouseReleased)
            {
                if (ev.Mouse.Button == Mouse_Button1)
                {
                    nw->Flags |= NativeWindowFlag_LeftMouseReleased;
                    nw->Flags &= ~NativeWindowFlag_PressingLeftMouse;

                    if (nw->ClickClock.Restart().AsMilliseconds() <= m_Style[OverlayStyle_ClickMilliseconds])
                        ++nw->OverflowClicks;
                    m_StateFlags &= ~StateFlag_FocusBlockByPopupCollapse;
                    m_WidgetStates[s_MenuBarId] = 0;
                }
                if (ev.Mouse.Button == Mouse_Button2)
                    nw->Flags |= NativeWindowFlag_RightMouseReleased;
            }
            else if (ev.Type == Event_Scrolled)
                scroll += m_Style[OverlayStyle_ScrollSensitivity] * ev.ScrollOffset;
            else if (ev.Type == Event_CharInput)
            {
                char buf[4];
                const u32 count = EncodeUTF8(buf, ev.Character);
                nw->TextInput.Insert(nw->TextInput.end(), buf, buf + count);
            }
            else if (ev.Type == Event_KeyPressed || ev.Type == Event_KeyRepeat)
                nw->EventKeys.Set(ev.Key);
        }

        if (nw->ClickClock.GetElapsed().AsMilliseconds() > m_Style[OverlayStyle_ClickMilliseconds])
            nw->OverflowClicks = 0;
    }

    // remove some state and check whether the window is collapsed
    for (OverlayWindow *win : m_ActiveWindows)
    {
        const bool collapsed =
            !(win->Flags & OverlayWindowFlag_NoCollapse) && Math::Approximately(win->Size[1], win->MinSize[1], 1.f);
        win->HeaderIcon = collapsed ? ArrowRightIcon : ArrowDownIcon;

        // we dont clear _Active flag yet as its needed for multi surface later
        win->Flags &= ~(WindowInternalFlag_Hovered | WindowInternalFlag_Focused);
        if (!(m_Flags & OverlayFlag_WindowPromotions))
            win->ClampToNative();
    };

    bool canAssignHover = true;
    iterateReverseWindows([&](OverlayWindow *win) {
        const NativeWindow *nw = win->Native;
        const bool popupBlocked = win->PopupDepth != m_PopupStack.GetSize();
        const bool modalBlocked = win->PopupDepth < m_ModalCollapseDepth;
        const bool hasHoverPadding = win->PopupDepth == 0 || win->PopupDepth == m_ModalCollapseDepth;
        const bool pressed = nw->Flags & NativeWindowFlag_LeftMousePressed;

        // if we are not popup locked, we jus check if window is hovered, which will allow grab to be set later.
        // if we are popup locked, we can still allow hovering if no depth > 0 widget previously set hovering id.
        // this is because we want to allow dragging immediately when collapsing the whole popup stack, but we dont
        // want dragging when collapsing all. thats why when popups exist, only widgets that belong to the popup
        // tree (any depth except 0) are allowed to set hovered id

        const bool winHovered =
            !modalBlocked && (!popupBlocked || !widgetHovered) && canAssignHover &&
            win->Layout.IsHovered(win->Id, nw->WorldMouse, m_Style[OverlayStyle_BorderHoverPadding], hasHoverPadding);

        const bool inputHovered = win->Flags & WindowInternalFlag_InputHovered;

        win->Flags |= wflags;
        win->Flags &= ~WindowInternalFlag_InputHovered;

        if (winHovered)
        {
            win->Flags |= WindowInternalFlag_Hovered;
            m_StateFlags |= StateFlag_WantCaptureMouse;
            canAssignHover = false;
        }

        if (pressed && (win->Grab.Flags != 0 || winHovered))
        {
            if (!widgetHovered && !widgetPressed && !inputHovered)
            {
                win->Grab.ScreenPos = win->GetActivePosition();
                // we dont use effective here bc if we are resizing, it means we are using the manual size
                win->Grab.Size = win->Size;
                m_Grabbed = win;
            }
            const bool noFocus = win->Flags & OverlayWindowFlag_NoBringToFocus;
            if (!noFocus)
                win->Layer = toTop();
            return false;
        }
        return true;
    });

    if (!m_ActiveWindows.IsEmpty())
        m_ActiveWindows.GetBack()->Flags |= WindowInternalFlag_Focused;

    NativeWindow *gchild = m_Grabbed ? m_Grabbed->Native : nullptr;
    NativeWindow *gnw;
    const bool checkParent = gchild && gchild->Flags & NativeWindowFlag_CheckParentForGrab;
    if (checkParent)
        gnw = gchild->Parent;
    else
        gnw = gchild;

    if (hovered && !(hovered->Flags & NativeWindowFlag_PressingLeftMouse))
        hovered->WorldMouseOnPress = hovered->WorldMouse;

    // now just handle grabbing, which is straightforward
    if (gnw && !(gnw->Flags & NativeWindowFlag_PressingLeftMouse))
    {
        gchild->Flags &= ~NativeWindowFlag_CheckParentForGrab;
        m_Grabbed = nullptr;
    }
    else if (m_Grabbed)
    {
        m_Grabbed->Grab.InteractionColor = OverlayColor_WindowBorderPressed;
        GrabInfo &ginfo = m_Grabbed->Grab;

        NativeWindow *nw = m_Grabbed->Native;

        f32v2 &p = ginfo.ScreenPos;
        f32v2 &s = ginfo.Size;

        const f32v2 &ms = m_Grabbed->MinSize;
        const f32v2 &md = checkParent ? gnw->ScreenMouseDelta : nw->ScreenMouseDelta;

        const bool move = ginfo.Flags == 0 && m_Grabbed->CanMove();
        const bool resize = m_Grabbed->CanResize();
        f32v2 &gpos = m_Grabbed->GetActivePosition();

        if (move)
        {
            p += md;
            gpos = p;
        }
        else if (resize)
        {
            const auto handleResizeAxis = [&](const u32 idx, const ResizeFlags canonical, const ResizeFlags mirrored) {
                const f32 step = md[idx];
                if (ginfo.Flags & canonical)
                {
                    s[idx] -= step;
                    p[idx] += step;
                }
                else if (ginfo.Flags & mirrored)
                    s[idx] += step;

                if (s[idx] >= ms[idx])
                {
                    gpos[idx] = p[idx];
                    m_Grabbed->Size[idx] = s[idx];
                }
            };
            handleResizeAxis(0, ResizeFlag_Left, ResizeFlag_Right);
            handleResizeAxis(1, ResizeFlag_Top, ResizeFlag_Bottom);
        }

        if (!(m_Grabbed->Flags & WindowInternalFlag_OwnsNative))
            m_Grabbed->ClampToNative();
        else if (resize || move)
        {
            nw->Window->SetPosition(i32v2{gpos});
            nw->Window->SetScreenDimensions(u32v2{m_Grabbed->Size});
            nw->UpdateBorders();
        }
    }

    if (!hovered || !m_HoveredLayoutCandidate ||
        !m_HoveredLayoutCandidate->IsHovered(m_HoveredWidgetCandidate, hovered->WorldMouse))
        m_HoveredWidgetCandidate = NullLayoutId;

    if (m_HoveredWidgetCandidate == NullLayoutId)
        m_WidgetHoverClock.Restart();

    m_HoveredId = NullLayoutId;
    m_HoveredLayoutCandidate = nullptr;

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
    m_DragDropFlags = 0;

    return modalWindow;
}

NativeWindow *Overlay::createNativeWindow(Window *win)
{
    TKit::TierAllocator *tier = GetTier();
    NativeWindow *nw = tier->Create<NativeWindow>();

    m_NativeWindows.Append(nw);
    nw->Window = win;
    nw->View = win->CreateRenderView<D2>(&m_Camera, GetRenderViewFlags());

    nw->Context = CreateRenderContext<D2>();
    nw->Context->AddTarget(nw->View);
    nw->UpdateBorders();

    return nw;
}
NativeWindow *Overlay::createNativeWindow(const f32v2 &pos, const f32v2 &dims, const WindowFlags flags)
{
    WindowSpecs specs{};
    specs.Position = i32v2{pos};
    specs.Dimensions = u32v2{dims};

    const NativeWindow *mainNative = GetMainNativeWindow();
    specs.PresentMode = mainNative ? mainNative->Window->GetPresentMode() : PresentMode_Immediate;
    specs.Flags = flags | WindowFlag_InstallCallbacks | WindowFlag_Visible | WindowFlag_FocusOnShow;

    Window *win = OpenWindow(
        {.Window = specs, .Flags = OpenWindowFlag_DoNotDestroyOnQuit | OpenWindowFlag_DoNotDestroyOnShouldClose});
    NativeWindow *nw = createNativeWindow(win);
    nw->ScreenPos = pos;
    return nw;
}

void Overlay::destroyNativeWindow(NativeWindow *win)
{
    DestroyRenderContext(win->Context);
    CloseWindow(win->Window);

    TKit::TierAllocator *tier = GetTier();
    tier->Destroy(win);
}
void Overlay::removeNativeWindow(NativeWindow *win)
{
    for (u32 i = 0; i < m_NativeWindows.GetSize(); ++i)
        if (m_NativeWindows[i] == win)
        {
            TKIT_ASSERT((m_Flags & OverlayFlag_FloatingMode) || i != 0,
                        "[ONYX][OVERLAY] Main native window can never be removed when not in floating mode!");
            destroyNativeWindow(win);
            m_NativeWindows.RemoveUnordered(m_NativeWindows.begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][OVERLAY] Native window to remove was not found!");
}

NativeWindow *Overlay::promoteWindow(OverlayWindow *win, const f32v2 &pos, const f32v2 &dims)
{
    NativeWindow *parent = win->Native;
    win->Native = createNativeWindow(pos, dims);
    win->Native->Parent = parent;
    win->Native->Owner = win;
    win->Layer = toTop();
    win->Flags |= WindowInternalFlag_OwnsNative;
    win->ScreenPos = f32v2{0.f};

    if (parent)
        parent->ScreenPos = f32v2{parent->Window->GetPosition()};
    if (win == m_Grabbed)
    {
        win->Native->Flags |= NativeWindowFlag_CheckParentForGrab;
        win->Grab.ScreenPos = win->GetActivePosition();
    }
    return win->Native;
}

void Overlay::demoteWindow(OverlayWindow *win)
{
    NativeWindow *parent = win->Native->Parent;
    if (parent)
        win->ScreenPos = win->Native->ScreenPos - parent->ScreenPos;
    else
        win->ScreenPos = win->Native->ScreenPos;

    removeNativeWindow(win->Native);
    win->Native = parent;
    win->Flags &= ~WindowInternalFlag_OwnsNative;
}

void Overlay::demoteAllWindows()
{
    for (OverlayWindow &win : m_OverlayWindows)
    {
        NativeWindow *nw = win.Native;
        nw->Flags &= ~NativeWindowFlag_WantUpdateSize;
        win.Flags &= ~WindowInternalFlag_WantUpdateSize;

        // if tooltip native parent is not null, it means the tooltip is owning the native window
        if (win.Flags & WindowInternalFlag_OwnsNative)
            demoteWindow(&win);
    }
    removeAllFloatWindows();
}
void Overlay::removeAllFloatWindows()
{
    for (NativeWindow *nw : m_NativeWindows)
        if (nw->Flags & NativeWindowFlag_RepresentsFloatElement)
            removeNativeWindow(nw);
    m_FloatWindows.Clear();
}

static bool isOutsideNative(const NativeWindow *nw, const f32v2 &pos, const f32v2 &size)
{
    const f32v2 dims = nw->GetDimensions();
    const f32 l = nw->ScreenPos[0];
    const f32 t = nw->ScreenPos[1];

    const f32 r = l + dims[0];
    const f32 b = t + dims[1];

    const f32 wl = pos[0];
    const f32 wr = pos[0] + size[0];
    const f32 wt = pos[1];
    const f32 wb = pos[1] + size[1];
    return wl < l || wb > b || wr > r || wt < t;
}

void Overlay::manageWindowPromotions()
{
    for (OverlayWindow &win : m_OverlayWindows)
    {
        const bool active = win.Flags & WindowInternalFlag_Active;
        const bool canPromote = !(win.Flags & OverlayWindowFlag_NoPromotion);

        NativeWindow *nw = win.Native;
        TKIT_ASSERT(!active || nw, "[ONYX][OVERLAY] If a window is active, it cannot have a null native window");

        if (!nw)
            continue;

        const bool ownsNative = win.Flags & WindowInternalFlag_OwnsNative;
        if (ownsNative)
        {
            const bool nativeWants = nw->Flags & NativeWindowFlag_WantUpdateSize;
            const bool childWants = win.Flags & WindowInternalFlag_WantUpdateSize;
            if (nativeWants)
                win.Size = nw->Size;
            else if (childWants)
            {
                nw->Window->SetScreenDimensions(u32v2{win.Size});
                nw->Size = win.Size;
            }
        }
        nw->Flags &= ~NativeWindowFlag_WantUpdateSize;
        win.Flags &= ~(WindowInternalFlag_WantUpdateSize | WindowInternalFlag_Active);

        // if a window owns a native and that native doesnt have a parent (meaning not even main window is parented ->
        // main window does not exist -> we are in floating mode, so we cannot demote)
        if (ownsNative && !nw->Parent)
        {
            if (!active)
                demoteWindow(&win);
            continue;
        }
        const f32v2 &wsize = win.Size;
        const f32v2 wpos = ownsNative ? nw->ScreenPos : (nw->ScreenPos + win.ScreenPos);

        const bool outsideNative = isOutsideNative(ownsNative ? nw->Parent : nw, wpos, wsize);
        if (ownsNative && (!canPromote || (!active || (m_Grabbed != &win && !outsideNative))))
            demoteWindow(&win);
        else if (!ownsNative && canPromote && outsideNative && active)
            promoteWindow(&win, wpos, wsize);
    }
    if (!(m_StateFlags & StateFlag_ActivePromotedFloatElement))
        removeAllFloatWindows();
    else
        for (auto it = m_FloatWindows.begin(); it != m_FloatWindows.end();)
        {
            NativeWindow *nw = it->Value;
            if (!(nw->Flags & NativeWindowFlag_ActivePromotedFloatElement))
            {
                removeNativeWindow(nw);
                it = m_FloatWindows.Remove(it);
            }
            else
            {
                nw->Flags &= ~NativeWindowFlag_ActivePromotedFloatElement;
                ++it;
            }
        }
    m_StateFlags &= ~StateFlag_ActivePromotedFloatElement;
}

template <typename F> void Overlay::iterateReverseWindows(const F func)
{
    for (u32 i = m_ActiveWindows.GetSize() - 1; i < m_ActiveWindows.GetSize(); --i)
        if (!func(m_ActiveWindows[i]))
            return;
}

/////////////////////////////////////////////
/// END WINDOWS/MENUS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// WIDGETS
/////////////////////////////////////////////

bool Overlay::Button(const TKit::StringView label, const OverlayButtonFlags flags)
{
    Layout &ly = GetCurrentLayout();
    const LayoutId id = PushId(label);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));

    OverlayColor col = OverlayColor_ButtonIdle;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_ButtonPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_ButtonHovered;

    const bool spanFull = flags & OverlayButtonFlag_SpanFullWidth;

    const bool small = flags & OverlayButtonFlag_Small;
    const f32 padding = m_Style[small ? OverlayStyle_SmallButtonPadding : OverlayStyle_WidgetPadding];

    f32 mnSize = 0.f;
    if (flags & OverlayButtonFlag_TryKeepSquare)
        mnSize = getLineHeight() + 2.f * padding;

    const LySz2 sizing = spanFull ? LySz2{flex(mnSize), fit()} : LySz2{fit(mnSize), fit()};

    m_LastItem = ly.BeginPanel(id, LyPnPar{.FillColor = m_Style[col],
                                           .Alignment = Center,
                                           .Sizing = sizing,
                                           .Shape = rect(m_Style[OverlayStyle_ButtonRadius]),
                                           .Padding = padding});

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());
    ly.EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

bool Overlay::RadioButton(const TKit::StringView label, const bool active)
{
    Layout &ly = GetCurrentLayout();
    const LayoutId id = PushId(label);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));

    OverlayColor col = OverlayColor_CheckBoxIdle;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_CheckBoxPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_CheckBoxHovered;

    m_LastItem = ly.BeginPanel(
        id, LyPnPar{.Alignment = CenterLeft, .Sizing = fit(), .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.BeginPanel(LyPnPar{.FillColor = m_Style[col],
                          .Alignment = Center,
                          .Sizing = sabs(m_Style[OverlayStyle_WidgetSize]),
                          .Shape = circle(),
                          .Padding = 6.f});

    ly.Panel(LyPnPar{.FillColor = active ? m_Style[OverlayColor_CheckBoxInner] : Color_Transparent,
                     .Sizing = grow(),
                     .Shape = circle()});
    ly.EndPanel();

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());

    ly.EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

// NOTE(Isma): Much repetition with radio button here
bool Overlay::CheckBox(const TKit::StringView label, bool *enable)
{
    Layout &ly = GetCurrentLayout();
    const LayoutId id = PushId(label);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));

    OverlayColor col = OverlayColor_CheckBoxIdle;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_CheckBoxPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_CheckBoxHovered;

    if (focusFlags & OverlayFocusQueryFlag_LeftClicked)
        *enable = !*enable;

    m_LastItem = ly.BeginPanel(
        id, LyPnPar{.Alignment = CenterLeft, .Sizing = fit(), .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.BeginPanel(LyPnPar{.FillColor = m_Style[col],
                          .Alignment = Center,
                          .Sizing = sabs(m_Style[OverlayStyle_WidgetSize]),
                          .Shape = rect(m_Style[OverlayStyle_CheckBoxRadius]),
                          .Padding = 6.f});

    if (*enable)
        ly.Panel(LyPnPar{.FillColor = m_Style[OverlayColor_CheckBoxInner],
                         .Sizing = grow(),
                         .Shape = rect(m_Style[OverlayStyle_CheckBoxRadius])});
    ly.EndPanel();

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());

    ly.EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

bool Overlay::BeginSelectable(LayoutId id, const bool enabled, const OverlaySelectableFlags flags)
{
    Layout &ly = GetCurrentLayout();
    id = PushId(id);
    const LayoutElement *elm = ly.QueryElement(id);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm);

    const bool highlight = flags & OverlaySelectableFlag_Highlight;
    const bool cb = flags & OverlaySelectableFlag_CheckBox;

    OverlayColor col = highlight ? OverlayColor_SelectableHovered : OverlayColor_SelectableIdle;

    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_SelectablePressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_SelectableHovered;
    else if (enabled && !cb)
        col = OverlayColor_SelectablePressed;

    const bool spanLabel = flags & OverlaySelectableFlag_SpanLabelWidth;
    const bool fwidth = flags & OverlaySelectableFlag_FlexWidth;

    const LySz xsizing = fwidth ? flex() : grow();
    const LySz2 sizing = {spanLabel ? fit() : xsizing, fit()};
    ly.BeginPanel(id, LyPnPar{.FillColor = m_Style[col],
                              .Direction = LayoutDirection_RightToLeft,
                              .Alignment = CenterLeft,
                              .Sizing = sizing,
                              .Shape = rect(m_Style[OverlayStyle_SelectableRadius])});

    const f32 padding = m_Style[OverlayStyle_WidgetPadding];
    if (cb)
    {
        ly.BeginPanel(LyPnPar{.FillColor = m_Style[col],
                              .Alignment = Center,
                              .Sizing = {sabs(m_Style[OverlayStyle_WidgetSize]), flex()},
                              .Shape = rect(m_Style[OverlayStyle_SelectableCheckBoxRadius]),
                              .Padding = padding});

        if (enabled)
            ly.Panel(LyPnPar{.FillColor = m_Style[OverlayColor_CheckBoxInner],
                             .Sizing = grow(),
                             .Shape = rect(m_Style[OverlayStyle_SelectableCheckBoxRadius])});
        ly.EndPanel();
    }

    const bool ltr = flags & OverlaySelectableFlag_LeftToRight;
    ly.BeginPanel(LyPnPar{.Direction = ltr ? LayoutDirection_LeftToRight : LayoutDirection_TopToBottom,
                          .Alignment = CenterLeft,
                          .Sizing = sizing,
                          .Padding = padding});

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
    Layout &ly = GetCurrentLayout();
    PopId();
    ly.EndPanel();
    ly.EndPanel();
}

bool Overlay::Selectable(const TKit::StringView label, const bool enabled, const OverlaySelectableFlags flags)
{
    const bool selected = BeginSelectable(label, enabled, flags);
    Layout &ly = GetCurrentLayout();
    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());
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

void Overlay::ProgressBar(const TKit::StringView label, const TKit::StringView text, const f32 pct)
{
    beginHorizontalWidget(PushId(label));
    Layout &ly = GetCurrentLayout();

    const f32 padding = m_Style[OverlayStyle_WidgetPadding];
    const f32 lh = getLineHeight() + 2.f * padding;
    ly.BeginPanel(LyPnPar{
        .FillColor = m_Style[OverlayColor_ProgressBarBackground], .Alignment = Center, .Sizing = {flex(), fit(lh)}});

    const bool isIndeterminate = pct < 0.f;
    const f32 idetSize = 0.4f;
    ly.Panel(
        LyPnPar{.FillColor = m_Style[OverlayColor_ProgressBarInner],
                .Sizing = snorm({isIndeterminate ? idetSize : Math::Clamp(pct, 0.f, 1.f), 1.f}),
                .SelfOffset = onorm({isIndeterminate ? (Math::Modulo(-pct, 1.f + idetSize) - idetSize) : 0.f, 0.f}),
                .Floating = {.Enable = true,
                             .DrawOnTop = false,
                             .Clip = true,
                             .Attachment = {LayoutAttachment_Left, LayoutAttachment_Top},
                             .Alignment = TopLeft}});

    if (!text.IsEmpty())
        ly.Text(ly.GenerateNextId(), text, getTextParams());

    ly.EndPanel();

    endHorizontalWidget(label);
    PopId();
}

void Overlay::BeginTabBar(const LayoutId id)
{
    TKIT_ASSERT(!m_CurrentTabBar, "[ONYX][OVERLAY] A tab bar is already opened. Cannot nest two tab bars in one");

    const LySz2 scrollSizing = {isAutoResize() ? fit() : grow(), fit()};
    const LayoutId tid = PushId(id);

    Layout &ly = GetCurrentLayout();
    ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom,
                          .Alignment = CenterLeft,
                          .Sizing = {grow(), fit()},
                          .Shape = rect(m_Style[OverlayStyle_ScrollAreaBorderRadius])});
    beginScroll({.Id = tid,
                 .Direction = LayoutDirection_LeftToRight,
                 .OuterSizing = scrollSizing,
                 .ContentSizing = scrollSizing,
                 .ContentPadding = 0.f,
                 .ChildGap = m_Style[OverlayStyle_TabGap],
                 .Flags = OverlayScrollFlag_NoVerticalScroll | OverlayScrollFlag_HorizontalScroll});

    endScroll();

    PushStyleColor(OverlayColor_Line, m_Style[OverlayColor_SelectablePressed]);
    PushStyleVar(OverlayStyle_LineRadius, 0.f);
    HorizontalLine();
    PopStyleColor();
    PopStyleVar();

    ly.EndPanel();

    m_CurrentTabBar = &m_TabBarData[tid];
    // the id that will need to be opened by tab items to keep appending
    m_CurrentTabBar->Id = IdFromStack("__onyx_id_Content_area");
}

void Overlay::EndTabBar()
{
    TKIT_ASSERT(m_CurrentTabBar, "[ONYX][OVERLAY] Cannot end a tab bar without starting one to begin with");
    m_CurrentTabBar = nullptr;
    m_TabIndex = 0;
    HorizontalLine();
    PopId();
}

bool Overlay::BeginTab(const TKit::StringView label, bool *enabled, const OverlayTabFlags flags)
{
    TKIT_ASSERT(m_CurrentTabBar, "[ONYX][OVERLAY] Tabs can only be created inside an active tab bar");
    const u32 idx = m_TabIndex++;
    if (enabled && !*enabled)
        return false;

    Layout &ly = GetCurrentLayout();
    ly.OpenPanel(m_CurrentTabBar->Id);

    const LayoutId id = PushId(label);
    bool isOpen = m_CurrentTabBar->OpenId == id;
    const bool startOpen = flags & OverlayTabFlag_StartOpen;

    PushStyleVar(OverlayStyle_SelectableRadius, m_Style[OverlayStyle_TabRadius]);
    PushStyleVar(OverlayStyle_WidgetPadding, m_Style[OverlayStyle_TabPadding]);

    if (enabled)
        ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight, .Alignment = CenterLeft, .Sizing = fit()});

    if (Selectable(label, isOpen, OverlaySelectableFlag_SpanLabelWidth | OverlaySelectableFlag_LeftToRight) ||
        (startOpen && m_CurrentTabBar->OpenId == NullLayoutId))
    {
        if (idx < m_CurrentTabBar->OpenIndex)
            isOpen = true;
        m_CurrentTabBar->OpenId = id;
        m_CurrentTabBar->OpenIndex = idx;
    }
    if (enabled && iconButton(IdFromStack("__onyx_id_Tab_close"), CrossIcon, grow(), OverlayColor_SelectableIdle))
        *enabled = false;

    if (enabled)
        ly.EndPanel();

    PopStyleVar(2);

    ly.EndPanel();

    if (!isOpen)
    {
        PopId();
        return false;
    }

    ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom,
                          .Alignment = TopLeft,
                          .Sizing = {isAutoResize() ? fit() : flex(), fit()},
                          .ChildGap = m_Style[OverlayStyle_ChildGap]});

    return true;
}
bool Overlay::InputText(TKit::StringView label, char *buf, const u32 size, const TKit::StringView hint,
                        const OverlayInputFlags flags)
{
    beginHorizontalWidget(PushId(label));
    const bool updated = inputTextBox(buf, size, hint, flags);
    endHorizontalWidget(label);
    PopId();
    return updated;
}

void Overlay::ColorPreviewTooltip(const TKit::StringView label, const Color &col, const OverlayColorFlags flags)
{
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);
    const bool tlabel = !(flags & OverlayColorFlag_NoTooltipLabel);
    const f32 tooltipSize = m_Style[OverlayStyle_ColorTooltipSize];

    Layout &ly = GetCurrentLayout();
    const bool info = !(flags & OverlayColorFlag_NoTooltipColorInfo);
    if (info)
    {
        if (tlabel)
        {
            ly.BeginPanel({.Direction = LayoutDirection_TopToBottom,
                           .Alignment = CenterLeft,
                           .Sizing = fit(),
                           .ChildGap = m_Style[OverlayStyle_ChildGap]});

            ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());
            HorizontalLine();
        }

        ly.BeginPanel({.Direction = LayoutDirection_LeftToRight,
                       .Alignment = CenterLeft,
                       .Sizing = fit(),
                       .ChildGap = m_Style[OverlayStyle_ChildGap]});

        drawColorPreview(col, tooltipSize, alpha);

        ly.BeginPanel({.Direction = LayoutDirection_TopToBottom,
                       .Alignment = CenterLeft,
                       .Sizing = fit(),
                       .ChildGap = m_Style[OverlayStyle_ChildGap]});

        const f32v4 hsv = col.ToHSV();
        if (flags & OverlayColorFlag_Float)
        {
            if (alpha)
            {
                Text("RGB: {:.2f}, {:.2f}, {:.2f}, {:.2f}", col.rgba[0], col.rgba[1], col.rgba[2], col.rgba[3]);
                Text("HSV: {:.2f}, {:.2f}, {:.2f}, {:.2f}", hsv[0], hsv[1], hsv[2], hsv[3]);
            }
            else
            {
                Text("RGB: {:.2f}, {:.2f}, {:.2f}", col.rgb[0], col.rgb[1], col.rgb[2]);
                Text("HSV: {:.2f}, {:.2f}, {:.2f}", hsv[0], hsv[1], hsv[2]);
            }
        }
        else
        {
            if (alpha)
            {
                Text("RGB: {}, {}, {}, {}", col.r<u32>(), col.g<u32>(), col.b<u32>(), col.a<u32>());
                Text("HSV: {}, {}, {}, {}", u32(360.f * hsv[0]), u32(100.f * hsv[1]), u32(100.f * hsv[2]),
                     u32(255.f * hsv[3]));
            }
            else
            {
                Text("RGB: {}, {}, {}", col.r<u32>(), col.g<u32>(), col.b<u32>());
                Text("HSV: {}, {}, {}", u32(360.f * hsv[0]), u32(100.f * hsv[1]), u32(100.f * hsv[2]));
            }
        }
        if (alpha)
            Text("Hex: #{:08X}", col.ToHexadecimal<u32>(true));
        else
            Text("Hex: #{:06X}", col.ToHexadecimal<u32>(false));
        ly.EndPanel();
        ly.EndPanel();
        if (tlabel)
            ly.EndPanel();
    }
    else
    {
        ly.BeginPanel({.Direction = LayoutDirection_LeftToRight,
                       .Alignment = CenterLeft,
                       .Sizing = fit(),
                       .ChildGap = m_Style[OverlayStyle_ChildGap]});
        drawColorPreview(col, tooltipSize, alpha);
        if (tlabel)
            ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());
        ly.EndPanel();
    }
}

void Overlay::ColorPreview(const TKit::StringView label, const Color &col, const OverlayColorFlags flags)
{
    const f32 previewSize = m_Style[OverlayStyle_ColorPreviewSize];

    PushId(label);
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);

    const usz id = drawColorPreview(col, previewSize, alpha);
    m_LastItem = id;

    const bool tooltip = !(flags & OverlayColorFlag_NoTooltip);
    if (tooltip && BeginItemTooltip(OverlayHoveredFlag_ShortDelay))
    {
        ColorPreviewTooltip(label, col, flags);
        EndTooltip();
    }

    m_LastItem = id;
    PopId();
}
bool Overlay::ColorPicker(const TKit::StringView label, const OverlayColorHandle color, const Color *original,
                          const f32 size, const OverlayColorFlags flags)
{
    PushId(label);
    f32 *colPtr = color.Data;

    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);
    const Color col =
        alpha ? Color{colPtr[0], colPtr[1], colPtr[2], colPtr[3]} : Color{colPtr[0], colPtr[1], colPtr[2]};
    bool changed = colorPicker(label, colPtr, col, original, flags, size);

    const bool inputs = !(flags & OverlayColorFlag_NoInput);
    if (inputs)
    {
        beginHorizontalWidget(PushId("__onyx_id_RGB"), 1.f);
        changed |= colorDrag(colPtr, col, flags);
        endHorizontalWidget();
        PopId();

        beginHorizontalWidget(PushId("__onyx_id_HSV"), 1.f);
        changed |= colorDrag(colPtr, col, flags | OverlayColorFlag_HSV);
        endHorizontalWidget();
        PopId();

        changed |= colorHexInput(colPtr, col, flags);
    }
    PopId();
    return changed;
}

bool Overlay::ColorButton(const TKit::StringView label, const OverlayColorHandle color, const OverlayColorFlags flags)
{
    f32 *colPtr = color.Data;
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);
    const Color col =
        alpha ? Color{colPtr[0], colPtr[1], colPtr[2], colPtr[3]} : Color{colPtr[0], colPtr[1], colPtr[2]};

    ColorPreview(label, col, flags);
    bool changed = false;

    const LayoutId id = IdFromStack(label);
    if (!(flags & OverlayColorFlag_NoPicker) &&
        BeginPopupContextItem(
            label, OverlayWindowFlag_AutoResize | OverlayWindowFlag_NoHeaderBar | OverlayWindowFlag_BringToTop,
            OverlayPopupFlag_LeftClick))
    {
        if (!checkWidgetState(id, WidgetStateFlag_Opened))
            m_PickerOriginal = col;

        changed = ColorPicker(label, color, &m_PickerOriginal, m_Style[OverlayStyle_ColorPickerSize], flags);
        EndPopup();
        m_WidgetStates[id] = WidgetStateFlag_Opened;
    }
    else
        m_WidgetStates[id] = 0;

    return changed;
}

bool Overlay::ColorEditor(const TKit::StringView label, const OverlayColorHandle color, const OverlayColorFlags flags)
{
    f32 *colPtr = color.Data;
    const bool inputs = !(flags & OverlayColorFlag_NoInput);

    if (inputs)
        beginHorizontalWidget(PushId(label));
    else
        beginHorizontalWidget(PushId(label), fit(), fit());

    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);
    const u32 count = 3 + alpha;
    TKIT_ENSURE(count <= color.Size, "[ONYX][OVERLAY] Specified color has no alpha! Must pass "
                                     "OverlayColorFlag_NoAlpha flag to avoid memory corruption");

    bool changed = false;

    const Color col =
        alpha ? Color{colPtr[0], colPtr[1], colPtr[2], colPtr[3]} : Color{colPtr[0], colPtr[1], colPtr[2]};

    if (inputs)
    {
        if (flags & OverlayColorFlag_Hex)
            colorHexInput(colPtr, col, flags);
        else
            colorDrag(colPtr, col, flags);
    }

    const f32 lh = getLineHeight() + 2.f * m_Style[OverlayStyle_WidgetPadding];
    const usz oldItem = m_LastItem;

    if (!(flags & OverlayColorFlag_NoPreview))
    {
        PushStyleVar(OverlayStyle_ColorPreviewSize, lh);
        PushStyleVar(OverlayStyle_ColorTooltipSize, 2.f * lh);
        ColorButton(label, color, flags);
        PopStyleVar(2);
    }

    m_LastItem = oldItem;

    endHorizontalWidget(label);
    PopId();

    if (!(flags & OverlayColorFlag_NoDragDrop))
    {
        if (BeginDragDropSource())
        {
            SetDragDropPayload("__onyx_id_Color", colPtr, count);
            PushStyleVar(OverlayStyle_ColorTooltipSize, m_Style[OverlayStyle_ColorDragTooltipSize]);
            ColorPreviewTooltip(label, col, OverlayColorFlag_NoTooltipColorInfo);
            PopStyleVar();
            EndDragDropSource();
        }
        if (BeginDragDropTarget())
        {
            if (const auto pl = AcceptDragDropPayload("__onyx_id_Color"))
            {
                const u32 size = Math::Min(count, pl.Size);
                const f32 *payload = rcast<f32 *>(pl.Data);
                for (u32 i = 0; i < size; ++i)
                    colPtr[i] = payload[i];
            }
            EndDragDropTarget();
        }
    }
    return changed;
}

bool Overlay::colorHexInput(f32 *colPtr, const Color &col, const OverlayColorFlags flags)
{
    constexpr u32 bsize = 10;
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);

    TKit::StaticString<bsize> strBuf = col.ToHexadecimal<TKit::StaticString<bsize>>(alpha);
    char *buf = strBuf.CString();

    const u32 count = 3 + alpha;

    if (inputTextBox(buf, bsize, {}, OverlayInputFlag_AutoSelectAll))
    {
        TKit::StaticString<bsize> hex = buf;
        if (!hex.IsEmpty() && hex[0] == '#')
            hex = hex.SubString(1);

        const u32 hexSize = 6 + 2 * alpha;
        while (hex.GetSize() < hexSize)
            hex.Append('0');

        hex.Resize(hexSize);

        const Color fhex = Color::FromHexadecimal(hex);
        for (u32 i = 0; i < count; ++i)
            colPtr[i] = fhex.rgba[i];

        return true;
    }
    return false;
}
bool Overlay::colorDrag(f32 *colPtr, const Color &col, const OverlayColorFlags flags)
{
    const TKit::FixedArray4<Color> colors{Color_Red, Color_Green, Color_Blue, Color_White * 0.4f};

    const bool markers = !(flags & OverlayColorFlag_NoColorMarkers) && !(flags & OverlayColorFlag_HSV);
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);
    const u32 count = 3 + alpha;

    constexpr TKit::FixedArray8<const char *> rgbFormats = {"R: {:.2f}", "G: {:.2f}", "B: {:.2f}", "A: {:.2f}",
                                                            "R: {}",     "G: {}",     "B: {}",     "A: {}"};

    constexpr TKit::FixedArray8<const char *> hsvFormats = {"H: {:.2f}", "S: {:.2f}", "V: {:.2f}", "A: {:.2f}",
                                                            "H: {}",     "S: {}",     "V: {}",     "A: {}"};

    constexpr TKit::FixedArray4<f32> rgbNorm = {255.f, 255.f, 255.f, 255.f};
    constexpr TKit::FixedArray4<f32> hsvNorm = {360.f, 100.f, 100.f, 255.f};

    const bool hsv = flags & OverlayColorFlag_HSV;

    f32v4 colVals = hsv ? col.ToHSV() : col.rgba;

    const TKit::FixedArray8<const char *> *formats = hsv ? &hsvFormats : &rgbFormats;
    const TKit::FixedArray4<f32> *norms = hsv ? &hsvNorm : &rgbNorm;

    Layout &ly = GetCurrentLayout();
    bool changed = false;
    for (u32 i = 0; i < count; ++i)
    {
        PushId(i);

        if (markers)
        {
            ly.BeginPanel(LyPnPar{.Alignment = CenterLeft, .Sizing = {flex(), fit()}});
            ly.Panel(LyPnPar{.FillColor = colors[i % 4], .Sizing = {sabs(2.f), grow()}});
        }
        if (flags & OverlayColorFlag_Float)
            changed |= horizontalDragBox(&colVals[i], 0.01f, 0.f, 1.f, formats->At(i), OverlaySliderFlag_ClampOnInput);
        else
        {
            u32 uval = u32(colVals[i] * norms->At(i));
            if (horizontalDragBox(&uval, 0.5f, 0u, u32(norms->At(i)), formats->At(i + 4),
                                  OverlaySliderFlag_ClampOnInput))
            {
                changed = true;
                colVals[i] = f32(uval) / norms->At(i);
            }
        }

        if (markers)
            ly.EndPanel();

        PopId();
    }
    if (changed)
    {
        const f32v4 rgba = hsv ? Color::FromHSV(colVals).rgba : colVals;
        for (u32 i = 0; i < count; ++i)
            colPtr[i] = rgba[i];
    }
    return changed;
}
bool Overlay::colorPicker(const TKit::StringView label, f32 *colPtr, const Color &col, const Color *original,
                          const OverlayColorFlags flags, const f32 pickerSize)
{
    Layout &ly = GetCurrentLayout();

    const usz outerId = IdFromStack("__onyx_id_Outer_picker");
    const auto it = m_PickerMeshes.Find(outerId);
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);

    PickerData *pdata;

    constexpr u32 pickerDivs = 10;
    constexpr u32 hueDivs = 32;

    const auto updatePickerGradient = [&](DynamicMeshData<D2> *data, const f32 hue) {
        data->Vertices.Clear();

        for (u32 y = 0; y <= pickerDivs; ++y)
            for (u32 x = 0; x <= pickerDivs; ++x)
            {
                const f32 s = 1.f - f32(x) / f32(pickerDivs);
                const f32 v = f32(y) / f32(pickerDivs);
                const f32v4 hsva{hue, s, v, 1.f};
                const u32 color = Color::FromHSV(hsva).ToLinear().Pack();
                data->Vertices.Append(f32v2{s, f32(y) / f32(pickerDivs)}, 0.f, color);
            }
    };

    const auto updateAlphaBarGradient = [&](DynamicMeshData<D2> *data) {
        data->Vertices.Clear();

        const u32 opaque = Color{col, 1.f}.ToLinear().Pack();
        const u32 transparent = Color{col, 0.f}.ToLinear().Pack();

        data->Vertices.Append(f32v2{0.f, 0.f}, 0.f, transparent);
        data->Vertices.Append(f32v2{1.f, 0.f}, 0.f, transparent);
        data->Vertices.Append(f32v2{0.f, 1.f}, 0.f, opaque);
        data->Vertices.Append(f32v2{1.f, 1.f}, 0.f, opaque);
    };

    bool changed = false;
    f32v4 hsv;
    if (it == m_PickerMeshes.end())
    {
        hsv = col.ToHSV();
        const auto updateHueBarGradient = [&](DynamicMeshData<D2> *data) {
            data->Vertices.Clear();

            for (u32 y = 0; y <= hueDivs; ++y)
            {
                const f32 h = f32(y) / f32(hueDivs);
                const u32 color = Color::FromHSV({1.f - h, 1.f, 1.f, 1.f}).ToLinear().Pack();
                data->Vertices.Append(f32v2{0.f, h}, 0.f, color);
                data->Vertices.Append(f32v2{1.f, h}, 0.f, color);
            }
        };

        PickerData ndata;
        ndata.PickerQuad = &m_DynamicMeshes[m_DynamicMeshIndex++];
        ndata.HueBar = &m_DynamicMeshes[m_DynamicMeshIndex++];
        ndata.AlphaBar = &m_DynamicMeshes[m_DynamicMeshIndex++];

        updatePickerGradient(ndata.PickerQuad->Data, hsv[0]);
        updateHueBarGradient(ndata.HueBar->Data);
        updateAlphaBarGradient(ndata.AlphaBar->Data);

        for (u32 y = 0; y < pickerDivs; ++y)
            for (u32 x = 0; x < pickerDivs; ++x)
            {
                const u32 tl = y * (pickerDivs + 1) + x;
                const u32 tr = tl + 1;
                const u32 bl = tl + (pickerDivs + 1);
                const u32 br = bl + 1;

                ndata.PickerQuad->Data->Indices.Append(tl);
                ndata.PickerQuad->Data->Indices.Append(tr);
                ndata.PickerQuad->Data->Indices.Append(bl);
                ndata.PickerQuad->Data->Indices.Append(tr);
                ndata.PickerQuad->Data->Indices.Append(br);
                ndata.PickerQuad->Data->Indices.Append(bl);
            }

        for (u32 y = 0; y < hueDivs; ++y)
        {
            const u32 tl = y * 2;
            const u32 tr = tl + 1;
            const u32 bl = tl + 2;
            const u32 br = tl + 3;
            ndata.HueBar->Data->Indices.Append(tl);
            ndata.HueBar->Data->Indices.Append(tr);
            ndata.HueBar->Data->Indices.Append(bl);
            ndata.HueBar->Data->Indices.Append(tr);
            ndata.HueBar->Data->Indices.Append(br);
            ndata.HueBar->Data->Indices.Append(bl);
        }

        ndata.AlphaBar->Data->Indices.Append(0);
        ndata.AlphaBar->Data->Indices.Append(1);
        ndata.AlphaBar->Data->Indices.Append(2);
        ndata.AlphaBar->Data->Indices.Append(1);
        ndata.AlphaBar->Data->Indices.Append(3);
        ndata.AlphaBar->Data->Indices.Append(2);

        pdata = &m_PickerMeshes.Insert(outerId, ndata);
        pdata->Hsv = hsv;
        pdata->Rgb = col.rgb;
    }
    else
    {
        pdata = &it->Value;
        if (col.rgb != pdata->Rgb)
        {
            hsv = col.ToHSV();
            pdata->Rgb = col.rgb;
            pdata->Hsv = f32v3{hsv};
            updatePickerGradient(pdata->PickerQuad->Data, hsv[0]);
            updateAlphaBarGradient(pdata->AlphaBar->Data);
        }
        else
            hsv = {pdata->Hsv, col.rgba[3]};
    }

    const LayoutId pickerId = IdFromStack("__onyx_id_Picker");
    const LayoutId hueBarId = IdFromStack("__onyx_id_Hue_bar");
    const LayoutId alphaBarId = IdFromStack("__onyx_id_Alpha_bar");

    const LayoutElement *pickerElm = ly.QueryElement(pickerId);
    const LayoutElement *hueBarElm = ly.QueryElement(hueBarId);
    const LayoutElement *alphaBarElm = ly.QueryElement(alphaBarId);

    const FocusFlags pickerFlags = queryAndSetFocusStatus(pickerElm, FocusFlag_PressedEvenWhenAwayFromHover);
    const FocusFlags hueBarFlags = queryAndSetFocusStatus(hueBarElm, FocusFlag_PressedEvenWhenAwayFromHover);
    const FocusFlags alphaBarFlags = queryAndSetFocusStatus(alphaBarElm, FocusFlag_PressedEvenWhenAwayFromHover);

    f32 circleSize = 32.f;

    constexpr f32 barWidth = 32.f;
    const f32 rodHeight = 4.f;

    const f32 posOffset = 0.5f * pickerSize;

    const NativeWindow *nw = m_Current->Native;
    if (pickerFlags & OverlayFocusQueryFlag_Pressed)
    {
        circleSize *= 1.2f;
        pdata->CirclePos = nw->WorldMouse - pickerElm->Position - posOffset;
        pdata->CirclePos = Math::Clamp(pdata->CirclePos, -posOffset, posOffset);

        hsv[1] = 0.5f + pdata->CirclePos[0] / pickerSize;
        hsv[2] = 0.5f + pdata->CirclePos[1] / pickerSize;
        updateAlphaBarGradient(pdata->AlphaBar->Data);

        changed = true;
    }
    else
    {
        pdata->CirclePos[0] = pickerSize * (hsv[1] - 0.5f);
        pdata->CirclePos[1] = pickerSize * (hsv[2] - 0.5f);
    }

    if (hueBarFlags & OverlayFocusQueryFlag_Pressed)
    {
        pdata->HueRodPos = nw->WorldMouse[1] - hueBarElm->Position[1] - posOffset;
        pdata->HueRodPos = Math::Clamp(pdata->HueRodPos, -posOffset, posOffset);

        hsv[0] = 0.5f - pdata->HueRodPos / pickerSize;
        updatePickerGradient(pdata->PickerQuad->Data, hsv[0]);
        updateAlphaBarGradient(pdata->AlphaBar->Data);

        changed = true;
    }
    else
        pdata->HueRodPos = pickerSize * (0.5f - hsv[0]);

    if (alphaBarFlags & OverlayFocusQueryFlag_Pressed)
    {
        pdata->AlphaRodPos = nw->WorldMouse[1] - alphaBarElm->Position[1] - posOffset;
        pdata->AlphaRodPos = Math::Clamp(pdata->AlphaRodPos, -posOffset, posOffset);

        hsv[3] = 0.5f + pdata->AlphaRodPos / pickerSize;
        changed = true;
    }
    else
        pdata->AlphaRodPos = pickerSize * (hsv[3] - 0.5f);

    ly.BeginPanel(outerId, LyPnPar{.Direction = LayoutDirection_LeftToRight,
                                   .Alignment = TopLeft,
                                   .Sizing = fit(),
                                   .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.BeginPanel(pickerId, LyPnPar{.FillColor = Color_White,
                                    .Alignment = Center,
                                    .Sizing = sabs(pickerSize),
                                    .Shape = dynamic(pdata->PickerQuad->Handle)});

    ly.BeginPanel(LyPnPar{.FillColor = Color_White,
                          .Alignment = Center,
                          .Sizing = sabs(circleSize),
                          .SelfOffset = oabs(pdata->CirclePos),
                          .Shape = circle()});

    ly.Panel(LyPnPar{.FillColor = Color{col, 1.f}, .Sizing = sabs(circleSize - 4.f), .Shape = circle()});

    ly.EndPanel();

    ly.EndPanel();

    ly.BeginPanel(hueBarId, LyPnPar{.FillColor = Color_White,
                                    .Alignment = Center,
                                    .Sizing = sabs({barWidth, pickerSize}),
                                    .Shape = dynamic(pdata->HueBar->Handle),
                                    .Overflow = LayoutOverflow_Spill});

    ly.Panel(LyPnPar{.FillColor = Color_White,
                     .Sizing = {srel(1.2f), sabs(rodHeight)},
                     .SelfOffset = oabs({0.f, pdata->HueRodPos})});

    ly.EndPanel();

    if (alpha)
    {
        constexpr u32 tileCount = 16;
        const f32 hsize = 0.5f * barWidth;
        const f32 vsize = pickerSize / f32(tileCount);

        const LySz2 barSizing = sabs({barWidth, pickerSize});
        const LySz2 stripSizing = sabs({hsize, pickerSize});
        const LySz2 tileSizing = sabs({hsize, vsize});

        constexpr TKit::FixedArray<f32, 2> checkboard = {s_CheckboardLight, s_CheckboardDark};

        ly.BeginPanel(
            LyPnPar{.Direction = LayoutDirection_LeftToRight, .Sizing = barSizing, .Overflow = LayoutOverflow_Spill});

        // left horizontal checkboard strip
        ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom, .Sizing = stripSizing});

        for (u32 i = 0; i < tileCount; ++i)
            ly.Panel(LyPnPar{.FillColor = Color{checkboard[i & 1]}, .Sizing = tileSizing});

        ly.EndPanel();

        // right horizontal checkboard strip
        ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom, .Sizing = stripSizing});

        for (u32 i = 0; i < tileCount; ++i)
            ly.Panel(LyPnPar{.FillColor = Color{checkboard[(i + 1) & 1]}, .Sizing = tileSizing});

        ly.EndPanel();

        ly.BeginPanel(alphaBarId, LyPnPar{.FillColor = Color_White,
                                          .Alignment = Center,
                                          .Sizing = barSizing,
                                          .SelfOffset = oabs({-barWidth, 0.f}),
                                          .Shape = dynamic(pdata->AlphaBar->Handle),
                                          .Overflow = LayoutOverflow_Spill,
                                          .ForceBlend = true});

        ly.Panel(LyPnPar{.FillColor = Color_White,
                         .Sizing = {srel(1.2f), sabs(rodHeight)},
                         .SelfOffset = oabs({0.f, pdata->AlphaRodPos})});

        ly.EndPanel();
        ly.EndPanel();
    }

    ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom,
                          .Alignment = TopLeft,
                          .Sizing = fit(),
                          .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.Text(ly.GenerateNextId(), original ? "Current" : trimLabel(label), getTextParams());
    if (!(flags & OverlayColorFlag_NoPreview))
    {
        PushStyleVar(OverlayStyle_ColorPreviewSize, m_Style[OverlayStyle_ColorPickerPreviewSize]);
        PushStyleVar(OverlayStyle_ColorTooltipSize, m_Style[OverlayStyle_ColorPickerTooltipSize]);
        ColorPreview(label, col, flags);
        if (original)
        {
            ly.Text(ly.GenerateNextId(), "Original", getTextParams());
            ColorPreview("Original", *original, flags);
        }
        PopStyleVar(2);
    }
    ly.EndPanel();

    ly.EndPanel();

    if (changed)
    {
        const u32 count = 3 + alpha;
        const f32v4 rgba = Color::FromHSV(hsv).rgba;
        for (u32 i = 0; i < count; ++i)
            colPtr[i] = rgba[i];
        pdata->Hsv = hsv;
        pdata->Rgb = f32v3{rgba};
    }

    return changed;
}

usz Overlay::drawColorPreview(const Color &col, const f32 size, bool alpha)
{
    Layout &ly = GetCurrentLayout();
    if (alpha)
    {
        const f32 hsize = 0.5f * size;
        ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom, .Alignment = TopLeft, .Sizing = sabs(size)});

        // top horizontal checkboard strip
        ly.BeginPanel(LyPnPar{.Sizing = sabs({size, hsize})});

        ly.Panel(LyPnPar{.FillColor = Color{s_CheckboardLight}, .Sizing = sabs(hsize)});
        ly.Panel(LyPnPar{.FillColor = Color{s_CheckboardDark}, .Sizing = sabs(hsize)});

        ly.EndPanel();

        // bottom horizontal checkboard strip
        ly.BeginPanel(LyPnPar{.Sizing = sabs({size, hsize})});

        ly.Panel(LyPnPar{.FillColor = Color{s_CheckboardDark}, .Sizing = sabs(hsize)});
        ly.Panel(LyPnPar{.FillColor = Color{s_CheckboardLight}, .Sizing = sabs(hsize)});

        ly.EndPanel();

        const usz id = ly.Panel(IdFromStack("__onyx_id_Preview"),
                                {.FillColor = col, .Sizing = sabs(size), .SelfOffset = oabs({0.f, size})});

        ly.EndPanel();
        return id;
    }
    return ly.Panel(IdFromStack("__onyx_id_Preview"), {.FillColor = Color{col, 1.f}, .Sizing = sabs(size)});
}

TKit::StringView Overlay::trimLabel(const TKit::StringView label)
{
    const u32 idx = label.FindFirstOf("##");
    return label.SubString(0, idx);
}
void Overlay::beginHorizontalWidget(const usz id, const LySz2 &outerSizing, const LySz2 &innerSizing)
{
    Layout &ly = GetCurrentLayout();
    m_LastItem = ly.BeginPanel(
        id, LyPnPar{.Alignment = CenterLeft, .Sizing = outerSizing, .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly.BeginPanel(LyPnPar{.Alignment = CenterLeft, .Sizing = innerSizing, .ChildGap = m_Style[OverlayStyle_ChildGap]});
}
void Overlay::beginHorizontalWidget(const usz id, const f32 normSize)
{
    const bool autoResize = isAutoResize();
    const bool isFloat = m_CurrentPopupDepth != m_Current->PopupDepth;
    const f32 effW = isFloat ? TKIT_F32_MAX : (normSize * m_Current->Size[0]);

    const f32 mnw = m_Style[OverlayStyle_WidgetMinimumWidth];

    const LySz2 outerSizing = {autoResize ? fit(mnw) : flex(), fit()};
    const LySz2 innerSizing = {autoResize ? fit(mnw) : flex(0.f, effW), fit()};

    return beginHorizontalWidget(id, outerSizing, innerSizing);
}
void Overlay::endHorizontalWidget(TKit::StringView label)
{
    Layout &ly = GetCurrentLayout();
    ly.EndPanel();
    if (!label.IsEmpty())
        ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());
    ly.EndPanel();
}
bool Overlay::inputTextBox(char *buf, const u32 capacity, const TKit::StringView hint, const OverlayInputFlags flags,
                           const InputConvertInfoFlags cflags)
{
    TKIT_ASSERT(capacity != 0, "[ONYX][OVERLAY] Buffer capacity for text input cannot be zero");
    Layout &ly = GetCurrentLayout();
    const u32 bufSize = u32(std::strlen(buf));
    TKIT_ASSERT(bufSize < capacity,
                "[ONYX][OVERLAY] The input character length ({}) must be lower than buffer capacity ({}) as the latter "
                "must to account for the null terminator",
                bufSize, capacity);

    const LayoutId iboxId = IdFromStack("__onyx_id_Input_box");
    const LayoutElement *ibox = ly.QueryElement(iboxId);
    const bool mustConvert = cflags & InputConvertFlag_MustConvert;

    FocusFlags fflags = FocusFlag_ClickedOnMousePress | FocusFlag_KeepActiveOnRelease | FocusFlag_KeepActiveOnPressed |
                        FocusFlag_ActiveAllowsInteraction | FocusFlag_PressedEvenWhenAwayFromHover;
    if (mustConvert)
        fflags |= FocusFlag_AllowPressedPickUp;

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ibox, fflags);

    ly.BeginPanel(iboxId, LyPnPar{.FillColor = m_Style[OverlayColor_InputBackground],
                                  .Alignment = CenterLeft,
                                  .Sizing = {grow(), fit()},
                                  .Shape = rect(m_Style[OverlayStyle_InputBoxRadius]),
                                  .Padding = m_Style[OverlayStyle_WidgetPadding]});

    // This input text box may be a "converted" slider/drag. this means that the first frame (where layout
    // element querying is not available) must be valid in the sense that we need valid queries. boxPos is very
    // important. without it, we cannot auto highlight. so we try this proxy, by trying to get the position of
    // the parent box if thats the case
    const LayoutElement *box = mustConvert ? ly.QueryElement(IdFromStack("__onyx_id_Drag/Slider_hbox")) : ibox;
    const f32 boxSize = ibox ? (ibox->Size[0] - 2.f * m_Style[OverlayStyle_WidgetPadding]) : 0.f;

    const FontData &fdata = getFontData();
    const f32 fs = m_Style[OverlayStyle_FontSize];

    LyTxPar tparams = getTextParams();
    const bool pressed = focusFlags & OverlayFocusQueryFlag_Pressed;
    const bool hovered = focusFlags & OverlayFocusQueryFlag_Hovered;
    if (pressed || hovered)
        m_Current->Flags |= WindowInternalFlag_InputHovered;

    bool updated = false;
    if ((focusFlags & OverlayFocusQueryFlag_Active) || mustConvert)
    {
        m_StateFlags |= StateFlag_RequestCaptureKeyboard | StateFlag_WantCaptureKeyboard;

        NativeWindow *nw = m_Current->Native;
        const bool ctrl = nw->Window->IsKeyPressed(Key_LeftControl);

        TKit::String &str = m_InputWidgetBuffer;
        const bool overrideHighlight = cflags & InputConvertFlag_MustOverrideHighlight;
        const bool justActive = (focusFlags & OverlayFocusQueryFlag_JustActive) || overrideHighlight;
        const bool undoRedo = !(flags & OverlayInputFlag_NoUndoRedo);

        if (justActive)
        {
            str.Clear();
            str.Insert(str.end(), buf, buf + bufSize);

            m_UndoStack.Clear();
            m_RedoStack.Clear();
        }

        if (undoRedo && ctrl && !m_UndoStack.IsEmpty() && nw->EventKeys[Key_Z])
        {
            const TextInputStateInfo &info = m_UndoStack.GetBack();
            m_RedoStack.Append(Math::Max(m_CursorStart, m_CursorEnd), str);

            m_CursorStart = info.Cursor;
            m_CursorEnd = info.Cursor;
            str = info.Text;

            m_UndoStack.Pop();
            updated = true;
        }
        if (undoRedo && ctrl && !m_RedoStack.IsEmpty() && nw->EventKeys[Key_Y])
        {
            const TextInputStateInfo &info = m_RedoStack.GetBack();
            m_UndoStack.Append(Math::Max(m_CursorStart, m_CursorEnd), str);

            m_CursorStart = info.Cursor;
            m_CursorEnd = info.Cursor;
            str = info.Text;

            m_RedoStack.Pop();
            updated = true;
            updated = true;
        }

        const auto pushUndo = [&] {
            if (undoRedo)
            {
                m_UndoStack.Append(Math::Max(m_CursorStart, m_CursorEnd), str);
                m_RedoStack.Clear();
            }
        };

        const bool escapeClears = flags & OverlayInputFlag_EscapeClearsAll;

        if (escapeClears && nw->EventKeys[Key_Escape])
        {
            pushUndo();
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
            const f32 onPress = nw->WorldMouseOnPress[0] - boxPos;
            const f32 mpos = nw->WorldMouse[0] - boxPos;

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

        if (nw->OverflowClicks == 2 || (justActive && autoSelectAll))
        {
            m_CursorStart = 0;
            m_CursorEnd = charCount;
        }
        else if (nw->OverflowClicks == 1)
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
        else if (nw->EventKeys[Key_Left] || nw->EventKeys[Key_Right])
        {
            const bool left = nw->EventKeys[Key_Left];

            const u32 limit = left ? 0 : charCount;
            const i32 hlen = i32(m_CursorEnd) - i32(m_CursorStart);
            if (m_CursorEnd != limit)
            {
                const bool lshift = nw->Window->IsKeyPressed(Key_LeftShift);
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
            tparams.FillColor.rgba[3] = m_Style[OverlayStyle_HintOpacity];
            // tparams.FillColor *= m_Style[OverlayStyle_HintOpacity];
            ly.Text(ly.GenerateNextId(), hint, tparams);
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
        ly.Panel(LyPnPar{.FillColor = Color{m_Style[OverlayColor_InputCursor], m_Style[OverlayStyle_CursorOpacity]},
                         .Sizing = {sabs(cwidth), grow()},
                         .SelfOffset = oabs({offset, 0.f})});
        if (hasHighlight)
        {
            const f32 hoffset = negSel ? offset : (offset - hLength);
            ly.Panel(LyPnPar{.FillColor = Color{m_Style[OverlayColor_InputHighlight], 0.4f},
                             .Sizing = {sabs(hLength), grow()},
                             .SelfOffset = oabs({hoffset - cwidth, 0.f})});
        }

        const u32 toRemoveBegin = hasHighlight ? selStart : (selStart - 1);
        const u32 toRemoveEnd = selEnd;

        if (ctrl && nw->EventKeys[Key_V])
        {
            const char *cp = Platform::GetClipboard();

            // could maybe append one by one, as that = operator is constructing a TKit::String temp
            if (cp && cp[0])
                nw->TextInput = cp;
        }

        if (toRemoveBegin < toRemoveEnd && !str.IsEmpty())
        {
            if (ctrl && nw->EventKeys[Key_C])
            {
                const TKit::StackString clipboard{str.begin() + toRemoveBegin, str.begin() + toRemoveEnd};
                Platform::SetClipboard(clipboard.CString());
            }

            if (nw->EventKeys[Key_Backspace] || (!nw->TextInput.IsEmpty() && hasHighlight))
            {
                pushUndo();

                updated = true;
                str.RemoveOrdered(str.begin() + toRemoveBegin, str.begin() + toRemoveEnd);

                const u32 removeCount = toRemoveEnd - toRemoveBegin;
                if (!hasHighlight || negSel)
                    m_CursorStart -= removeCount;
                if (!hasHighlight || !negSel)
                    m_CursorEnd -= removeCount;

                selEnd = toRemoveBegin;
            }
        }

        const u32 insertionsLeft = capacity - 1 - str.GetSize();
        const u32 insertions = Math::Min(nw->TextInput.GetSize(), insertionsLeft);

        updated |= insertions != 0;
        if (insertions != 0)
        {
            pushUndo();
            for (u32 i = 0; i < insertions; ++i)
                str.Insert(str.begin() + selEnd + i, nw->TextInput[i]);
        }

        m_CursorStart += insertions;
        m_CursorEnd += insertions;

        const bool enterCommits = flags & OverlayInputFlag_EnterCommitsBuffer;

        if (enterCommits)
            updated = nw->EventKeys[Key_Enter];
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
                updated = nw->EventKeys[Key_Enter];
        }
    }
    else
    {
        const bool elide = flags & OverlayInputFlag_ElideLeft;
        const f32 textOffset = elide ? Math::Min(0.f, boxSize - fs * fdata.ComputeTextWidth(buf)) : 0.f;
        const bool useHint = bufSize == 0 && !hint.IsEmpty();

        tparams.Offset[0] = oabs(textOffset);
        if (useHint)
        {
            tparams.FillColor.rgba[3] = m_Style[OverlayStyle_HintOpacity];
            // tparams.FillColor *= m_Style[OverlayStyle_HintOpacity];
            ly.Text(ly.GenerateNextId(), hint, tparams);
        }
        else
            ly.Text(ly.GenerateNextId(), buf, tparams);
    }

    ly.EndPanel();

    return updated;
}

/////////////////////////////////////////////
/// END WIDGETS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// DISPLAY
/////////////////////////////////////////////

void Overlay::TextRaw(const LayoutTextMode mode, const TKit::StringView text)
{
    LyTxPar params = getTextParams();
    params.Mode = mode;

    Layout &ly = GetCurrentLayout();
    // a very mid solution to unstable ids when text changes every frame (e.g, printing delta times/performance)
    // UPDATE: text has no id until explicitly set
    ly.Text(m_TextId, text, params);
    if (m_TextId != NullLayoutId)
    {
        m_LastItem = m_TextId;
        m_TextId = NullLayoutId;
    }
}
void Overlay::TextIconRaw(const CodePoint icon, const LayoutTextMode mode, const TKit::StringView text)
{
    PushDirection(LayoutDirection_LeftToRight);

    LyTxPar params = getTextParams();
    params.Mode = mode;

    Layout &ly = GetCurrentLayout();
    ly.Unicode(NullLayoutId, icon, getUnicodeParams());

    ly.Text(m_TextId, text, params);
    if (m_TextId != NullLayoutId)
    {
        m_LastItem = m_TextId;
        m_TextId = NullLayoutId;
    }
    PopDirection();
}

void Overlay::BeginDisabled(const bool enabled)
{
    m_DisabledStack.Append(m_Style[OverlayStyle_Alpha]);
    if (enabled)
    {
        m_StateFlags |= StateFlag_Disabled;
        m_Style.Variables[OverlayStyle_Alpha] *= m_Style[OverlayStyle_DisabledAlpha];
    }
}
void Overlay::EndDisabled()
{
    m_Style.Variables[OverlayStyle_Alpha] = m_DisabledStack.GetBack();
    m_DisabledStack.Pop();
    if (m_DisabledStack.IsEmpty())
        m_StateFlags &= ~StateFlag_Disabled;
}

/////////////////////////////////////////////
/// END DISPLAY
/////////////////////////////////////////////

/////////////////////////////////////////////
/// POPUPS
/////////////////////////////////////////////

void Overlay::CloseCurrentPopup()
{
    TKIT_ASSERT(m_CurrentPopupDepth != 0,
                "[ONYX][OVERLAY] CloseCurrentPopup() can only be called inside an active popup");
    closePopup(m_CurrentPopupDepth - 1);
}
void Overlay::CloseChildPopup()
{
    closePopup(m_CurrentPopupDepth);
}
void Overlay::CollapsePopups()
{
    closePopup(0);
}

bool Overlay::BeginPopup(const LayoutId id, const TKit::StringView title, const OverlayWindowFlags flags)
{
    // we cannot apply the id stack here because user must unequivocally reference this from anywhere
    if (m_CurrentPopupDepth == m_PopupStack.GetSize() || m_PopupStack[m_CurrentPopupDepth] != id)
    {
        m_WidgetStates[id] = 0;
        return false;
    }

    ++m_CurrentPopupDepth;
    if (getWidgetState(id) == 0)
    {
        OverlayWindow *win = getOrCreateOverlayWindow(title);
        if (!(win->Flags & WindowInternalFlag_OwnsNative))
            // we dont handle size because BeginWindow does that for us
            win->Native = m_Current->Native;

        const f32v2 &size = win->Size;
        win->SetActivePosition(
            win->ToScreen(computeMouseAlignedPosition(win->Native, size, !(flags & OverlayWindowFlag_NoPromotion))));
    }
    m_WidgetStates[id] = WidgetStateFlag_Opened;

    return BeginWindow(title, flags | OverlayWindowFlag_NoCollapse | WindowInternalFlag_ClosePopupButton);
}

void Overlay::EndPopup()
{
    --m_CurrentPopupDepth;
    EndWindow();
}
bool Overlay::BeginDropDown(const TKit::StringView label, const TKit::StringView preview,
                            const OverlayDropDownFlags flags)
{
    Layout &ly = GetCurrentLayout();
    beginHorizontalWidget(PushId(label), 1.f);

    const LayoutId id = IdFromStack("__onyx_id__Drop_down_box");
    const LayoutElement *elm = ly.QueryElement(id);
    OverlayColor boxCol = OverlayColor_DropDownIdle;

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_LeftClickOpensPopup);

    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        boxCol = OverlayColor_DropDownPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        boxCol = OverlayColor_DropDownHovered;

    const bool hasPreview = !(flags & OverlayDropDownFlag_NoPreview);
    const f32 padding = m_Style[OverlayStyle_WidgetPadding];

    const f32 maxWidth = m_CurrentPopupDepth == 0 ? (0.5f * m_Current->Size[0]) : TKIT_F32_MAX;
    ly.BeginPanel(id,
                  LyPnPar{.FillColor = m_Style[boxCol],
                          .Alignment = CenterLeft,
                          .Sizing = {flex(0.f, maxWidth), hasPreview ? fit() : sabs(getLineHeight() + 2.f * padding)},
                          .Shape = rect(m_Style[OverlayStyle_DropDownRadius])});

    if (hasPreview)
    {
        ly.BeginPanel(LyPnPar{
            .Alignment = CenterLeft, .Sizing = {grow(), fit()}, .Padding = m_Style[OverlayStyle_WidgetPadding]});

        ly.Text(ly.GenerateNextId(), preview, getTextParams());
        ly.EndPanel();
    }

    // ly.Panel(IdFromStack("__onyx_id_Push"), LyPnPar{.Sizing = grow()});

    const bool dropDownActive = focusFlags & OverlayFocusQueryFlag_PopupOpen;
    if (!(flags & OverlayDropDownFlag_NoArrowButton))
    {
        OverlayColor buttonCol = dropDownActive ? OverlayColor_DropDownButton : OverlayColor_DropDownHovered;
        ly.BeginPanel(LyPnPar{.FillColor = m_Style[buttonCol],
                              .Alignment = Center,
                              .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), flex()}});
        ly.Unicode(NullLayoutId, ArrowDownIcon, getUnicodeParams());
        ly.EndPanel();
    }

    if (dropDownActive)
    {
        endHorizontalWidget(label);

        const usz did = IdFromStack("__onyx_id_Drop_down");
        const LayoutElement *delm = ly.QueryElement(did);
        const f32 csize = delm ? delm->Size[1] : 0.f;

        const f32 ppos = elm->Position[1];
        const f32 psize = elm->Size[0];

        const bool tight = flags & OverlayDropDownFlag_Tight;

        const f32v4 borders = getWorldEffectiveBorders();
        const f32 bborder = borders[3];

        const bool surpasses = (ppos - csize) < bborder;
        const LyAtt2 att = {LayoutAttachment_Left, surpasses ? LayoutAttachment_Top : LayoutAttachment_Bottom};
        const LyAlg2 alg = surpasses ? BottomLeft : TopLeft;

        ly.BeginPanel(did, LyPnPar{.FillColor = m_Style[OverlayColor_PopupBackground],
                                   .Direction = LayoutDirection_TopToBottom,
                                   .Alignment = TopLeft,
                                   .Sizing = {fit(psize), fit()},
                                   .Shape = rect(m_Style[OverlayStyle_DropDownPopupRadius]),
                                   .Floating = {.Enable = true, .Attachment = att, .Alignment = alg},
                                   .Padding = tight ? 0.f : m_Style[OverlayStyle_WidgetPadding]});

        const f32 height = (flags & OverlayDropDownFlag_HeightSmall) ? m_Style[OverlayStyle_DropDownHeightSmall]
                                                                     : m_Style[OverlayStyle_DropDownHeightRegular];
        const bool largest = flags & OverlayDropDownFlag_HeightLargest;

        const usz sid = IdFromStack("__onyx_id_Scroll");
        const LySz2 osizing = flex();
        const LySz2 csizing = {flex(), largest ? fit() : fit(0.f, height)};
        const f32 cgap = tight ? 0.f : m_Style[OverlayStyle_ChildGap];

        ++m_CurrentPopupDepth;
        PushStyleVar(OverlayStyle_ScrollBarGap, 0.f);
        beginScroll({.Id = sid,
                     .OuterSizing = osizing,
                     .ContentSizing = csizing,
                     .ContentPadding = 0.f,
                     .ChildGap = cgap,
                     .Flags = OverlayScrollFlag_NoBackground});

        queryAndSetFocusStatus(delm, FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetActiveId);
        return true;
    }
    endHorizontalWidget(label);
    ly.EndPanel();
    PopId();
    return false;
}

void Overlay::closePopup(const u32 depth)
{
    m_StateFlags |= StateFlag_PopupProtectionForbidden | StateFlag_MustCollapsePopups;
    m_PopupCollapseDepth = depth;
    // this means only the topmost modal can be collapsed
    m_ModalCollapseDepth = Math::Min(m_ModalCollapseDepth, depth);
    m_WidgetStates[s_MenuBarId] = 0;
}
void Overlay::requestCollapsePopups()
{
    m_StateFlags |= StateFlag_MustCollapsePopups;
    m_PopupCollapseDepth = 0;
}
static f32v2 getMonitorDimensions()
{
    return f32v2{Platform::GetVideoMode(Platform::GetMainMonitor()).Dimensions};
}
f32v2 Overlay::computeMouseAlignedPosition(const NativeWindow *win, const f32v2 &size, const bool allowPromotions) const
{
    const f32 toffset = m_Style[OverlayStyle_TooltipOffset];
    const f32v2 offset = f32v2{toffset, -2.f * toffset};

    f32v2 pos = win->WorldMouse + offset;
    const bool windowPromotions = allowPromotions && (m_Flags & OverlayFlag_WindowPromotions);

    const f32v2 br = windowPromotions ? win->ToWorld(getMonitorDimensions()) : win->WorldBottomRightBorder;
    const f32 rt = win->WorldMouse[0] + offset[0] + size[0];
    const f32 rb = win->WorldMouse[1] + offset[1] - size[1];
    if (rt > br[0])
        pos[0] -= rt - br[0];
    if (rb < br[1])
        pos[1] -= rb - br[1];

    return pos;
}

/////////////////////////////////////////////
/// END POPUPS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// TOOLTIPS
/////////////////////////////////////////////
void Overlay::BeginTooltip(const OverlayTooltipFlags flags)
{
    m_LastItemTooltipBackup = m_LastItem;
    if (flags & OverlayTooltipFlag_Reset)
        trashTooltip();

    const LayoutId id = "__onyx_id_Tooltip";
    OverlayWindow *parent = m_Current;

    m_Current = getOrCreateOverlayWindow(id);
    m_Current->Flags |= OverlayWindowFlag_AutoResize;

    PushId(id);

    m_Tooltip = m_Current;
    m_WindowStack.Append(m_Current);

    if (m_Current->Flags & WindowInternalFlag_Active)
        return;

    m_Current->Flags |= WindowInternalFlag_Active;

    Layout &ly = GetCurrentLayout();

    const LayoutElement *elm = ly.QueryElement(id);

    const f32v2 size = elm ? elm->Size : m_Current->Size;

    const bool ownsNative = m_Current->Flags & WindowInternalFlag_OwnsNative;
    if (!ownsNative)
        m_Current->Native = parent->Native;

    const bool noProm = flags & OverlayTooltipFlag_NoPromotion;
    if (noProm)
        m_Current->Flags |= OverlayWindowFlag_NoPromotion;

    const f32v2 pos = computeMouseAlignedPosition(parent->Native, size, !noProm);
    if (m_Flags & OverlayFlag_WindowPromotions)
    {
        // we dont care about the window's actual position (as the tooltip is just visually driven, there is no active
        // interaction for which we would need to store its position) EXCEPT when multi window is involved. thats why we
        // only set the position here

        const f32v2 scpos = parent->ToScreen(pos);
        const bool parentOwns = parent->Flags & WindowInternalFlag_OwnsNative;
        if (ownsNative)
            m_Current->Native->ScreenPos = parentOwns ? scpos : (parent->Native->ScreenPos + scpos);
        else
            m_Current->ScreenPos = parentOwns ? (scpos - parent->Native->ScreenPos) : scpos;

        if (!Math::Approximately(m_Current->Size, size, 1.f))
        {
            m_Current->Size = size;
            if (ownsNative)
                m_Current->Flags |= WindowInternalFlag_WantUpdateSize;
        }
    }

    ly.BeginPanel(id, LyPnPar{.FillColor = m_Style[OverlayColor_WindowBorderIdle],
                              .Sizing = fit(),
                              .SelfOffset = oabs(ownsNative ? m_Current->Native->GetWorldTopLeft() : pos),
                              .Shape = rect(m_Style[OverlayStyle_TooltipRadius]),
                              .Padding = m_Style[OverlayStyle_TooltipPadding]});

    ly.BeginPanel(LyPnPar{.FillColor = m_Style[OverlayColor_WindowBackgroundExpanded],
                          .Direction = LayoutDirection_TopToBottom,
                          .Alignment = TopLeft,
                          .Sizing = fit(),
                          .Shape = rect(m_Style[OverlayStyle_TooltipRadius]),
                          .Padding = m_Style[OverlayStyle_ContentAreaPadding],
                          .ChildGap = m_Style[OverlayStyle_ChildGap]});
}

bool Overlay::BeginItemTooltip(const OverlayHoveredFlags flags)
{
    if (!IsItemHovered(flags))
        return false;

    BeginTooltip(OverlayTooltipFlag_Reset);
    return true;
}

void Overlay::EndTooltip()
{
    PopId();
    popWindowStack();
    m_LastItem = m_LastItemTooltipBackup;
}

void Overlay::trashTooltip()
{
    if (!m_Tooltip)
        return;
    m_Tooltip->Layout.Reset();

    if (m_Tooltip->Flags & WindowInternalFlag_OwnsNative)
        removeNativeWindow(m_Tooltip->Native);

    m_Tooltip->Flags &= ~(WindowInternalFlag_Active | WindowInternalFlag_OwnsNative);
    m_Tooltip->Native = getMainNativeWindow();
    m_Tooltip = nullptr;
}
void Overlay::resetTooltip()
{
    if (!m_Tooltip)
        return;
    m_Tooltip->Layout.Reset();
    m_Tooltip->Flags &= ~WindowInternalFlag_Active;
    m_Tooltip = nullptr;
}

/////////////////////////////////////////////
/// END TOOLTIPS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// LAYOUT
/////////////////////////////////////////////

bool Overlay::BeginScroll(const TKit::StringView label, const f32 maxHeight, const f32 maxWidth,
                          const OverlayScrollFlags flags)
{
    const usz id = PushId(label);
    const bool autoResize = isAutoResize();

    const f32 padding = m_Style[OverlayStyle_ContentAreaPadding];

    const bool borders = flags & OverlayScrollFlag_Borders;
    const bool tight = flags & OverlayScrollFlag_Tight;

    const f32 omw = maxWidth + 2.f * padding;
    const LySz2 outer = {autoResize ? fit() : grow(0.f, omw), fit()};
    const LySz2 content = {autoResize ? fit(0.f, maxWidth) : grow(0.f, maxWidth), fit(0.f, maxHeight)};

    Layout &ly = GetCurrentLayout();
    ly.BeginPanel(LyPnPar{.FillColor = borders ? m_Style[OverlayColor_ScrollAreaBorders] : Color_Transparent,
                          .Direction = LayoutDirection_TopToBottom,
                          .Alignment = CenterLeft,
                          .Sizing = outer,
                          .Shape = rect(m_Style[OverlayStyle_ScrollAreaBorderRadius]),
                          .Padding = borders ? padding : 0.f});

    if (flags & OverlayScrollFlag_Title)
        ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());

    return beginScroll({.Id = id,
                        .OuterSizing = outer,
                        .ContentSizing = content,
                        .ContentPadding = tight ? 0.f : padding,
                        .ChildGap = tight ? 0.f : m_Style[OverlayStyle_ChildGap],
                        .Flags = flags});
}
void Overlay::HorizontalSeparator(const TKit::StringView label)
{
    Layout &ly = GetCurrentLayout();
    ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight,
                          .Alignment = CenterLeft,
                          .Sizing = {flex(), fit()},
                          .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const f32 textOffset = m_Style[OverlayStyle_SeparatorTextOffset];
    const f32 width = m_Style[OverlayStyle_LineWidth];

    ly.Panel(
        LyPnPar{.FillColor = m_Style[OverlayColor_Line], .Sizing = sabs({textOffset, width}), .Shape = rect(width)});

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());
    ly.Panel(LyPnPar{
        .FillColor = m_Style[OverlayColor_Line], .Sizing = {grow(textOffset), sabs(width)}, .Shape = rect(width)});
    ly.EndPanel();
}

bool Overlay::PushTreeRaw(LayoutId id, const TKit::StringView label, const OverlayTreeFlags flags)
{
    id = PushId(id);
    Layout &ly = GetCurrentLayout();
    const bool framed = flags & OverlayTreeFlag_Framed;

    OverlayColor col = framed ? OverlayColor_TreeIdle : OverlayColor_None;

    OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly.QueryElement(id));
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_TreePressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_TreeHovered;

    const bool horScroll = m_Current->Flags & OverlayWindowFlag_HorizontalScroll;
    const bool autoResize = isAutoResize();
    const LySz growOrFlex = (horScroll || autoResize) ? flex() : grow();

    const bool spanLabel = flags & OverlayTreeFlag_SpanLabelWidth;
    const LySz2 sizing = {spanLabel ? fit() : growOrFlex, fit()};

    m_LastItem = ly.BeginPanel(id, LyPnPar{.FillColor = m_Style[col],
                                           .Alignment = CenterLeft,
                                           .Sizing = sizing,
                                           .Shape = rect(m_Style[OverlayStyle_TreeRadius]),
                                           .Padding = m_Style[OverlayStyle_HeaderPadding],
                                           .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const bool startOpen = flags & OverlayTreeFlag_StartOpen;
    const bool opened = checkWidgetState(id, WidgetStateFlag_Opened, startOpen ? WidgetStateFlag_Opened : 0);

    const usz buttonId =
        ly.BeginPanel(IdFromStack("__onyx_id_Tree_collapse"),
                      LyPnPar{.Alignment = Center, .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), fit()}});

    bool toggleOpen = focusFlags & OverlayFocusQueryFlag_LeftClicked;
    if (toggleOpen)
    {
        const bool onArrow = flags & OverlayTreeFlag_ToggleOnArrow;
        const bool onDoubleClick = !opened && (flags & OverlayTreeFlag_OpenOnDoubleClick);
        const bool doubleClicked = focusFlags & OverlayFocusQueryFlag_DoubleClicked;
        const NativeWindow *nw = m_Current->Native;
        if (onArrow)
            toggleOpen = ly.IsHovered(buttonId, nw->WorldMouse) || (onDoubleClick && doubleClicked);
        else if (onDoubleClick)
            toggleOpen = doubleClicked;
    }

    const CodePoint code = opened ? ArrowDownIcon : ArrowRightIcon;
    ly.Unicode(NullLayoutId, code, getUnicodeParams());

    ly.EndPanel();

    ly.Text(ly.GenerateNextId(), trimLabel(label), getTextParams());
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

    ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight, .Alignment = TopLeft, .Sizing = sizing});

    if (indent)
    {
        ly.BeginPanel(LyPnPar{
            .Direction = LayoutDirection_TopToBottom, .Alignment = Center, .Sizing = {sabs(treeIndent), flex()}});

        if (lines)
            VerticalLine();

        ly.EndPanel();
    }

    ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom,
                          .Alignment = TopLeft,
                          .Sizing = sizing,
                          .ChildGap = m_Style[OverlayStyle_ChildGap]});

    return true;
}
bool Overlay::beginScroll(const ScrollParameterSpecs &specs)
{
    Layout &ly = GetCurrentLayout();
    ScrollInfo &sinfo = m_Scrollables[specs.Id];
    sinfo.Flags = specs.Flags;

    const bool noHeader = m_Current->Flags & OverlayWindowFlag_NoHeaderBar;
    const bool collapsed = !noHeader && m_Current->HeaderIcon == ArrowRightIcon;
    const bool drawBar = !(specs.Flags & OverlayScrollFlag_NoScrollBar);

    const bool borders = specs.Flags & OverlayScrollFlag_Borders;
    const f32 cpadding = m_Style[OverlayStyle_ContentAreaPadding];
    const bool bckg = !(specs.Flags & OverlayScrollFlag_NoBackground);
    m_LastItem = ly.BeginPanel(
        specs.Id, LyPnPar{.FillColor = bckg ? m_Style[OverlayColor_WindowBackgroundExpanded] : Color_Transparent,
                          .Direction = LayoutDirection_BottomToTop,
                          .Alignment = TopLeft,
                          .Sizing = specs.OuterSizing,
                          .Shape = rect(m_Style[OverlayStyle_ScrollAreaBorderRadius]),
                          .Padding = borders ? cpadding : 0.f,
                          .ChildGap = m_Style[OverlayStyle_ScrollBarGap]});

    const LayoutId contentId = IdFromStack("__onyx_id_Content_area");
    bool appendStack = false;
    if (!collapsed && (specs.Flags & OverlayScrollFlag_HorizontalScroll))
        appendStack |= performScroll(contentId, sinfo.Horizontal, LayoutAxis_Horizontal, specs.ContentPadding, drawBar);

    ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_RightToLeft,
                          .Alignment = TopLeft,
                          .Sizing = specs.OuterSizing,
                          .ChildGap = m_Style[OverlayStyle_ScrollBarGap]});

    if (!collapsed && !(specs.Flags & OverlayScrollFlag_NoVerticalScroll))
        appendStack |= performScroll(contentId, sinfo.Vertical, LayoutAxis_Vertical, specs.ContentPadding, drawBar);

    ly.BeginPanel(contentId, LyPnPar{.Direction = specs.Direction,
                                     .Alignment = TopLeft,
                                     .Sizing = specs.ContentSizing,
                                     .ChildOffset = oabs({-sinfo.Horizontal.ElementOffset,
                                                          -sinfo.Vertical.ElementOffset + specs.VerticalOffset}),
                                     .Padding = specs.ContentPadding,
                                     .ChildGap = specs.ChildGap});

    if (isElementHovered(ly.QueryElement(specs.Id), OverlayHoveredFlag_AllowBlockedByPressedItem |
                                                        OverlayHoveredFlag_AllowBlockedByActiveItem |
                                                        OverlayHoveredFlag_AllowBlockedByDrag))
    {
        if (appendStack)
            m_ScrollStack.Append(specs.Id);
        return true;
    }
    return false;
}

void Overlay::endScroll()
{
    Layout &ly = GetCurrentLayout();
    ly.EndPanel();
    ly.EndPanel();
    ly.EndPanel();
}
bool Overlay::performScroll(const LayoutId contentAreaId, ScrollBarInfo &sinfo, const LayoutAxis axis,
                            const f32 contentPadding, const bool drawBar)
{
    Layout &ly = GetCurrentLayout();
    const LayoutElement *contentArea = ly.QueryElement(contentAreaId);

    if (contentArea && contentArea->ChildrenSize[axis] > contentArea->Size[axis])
    {
        const f32 size = contentArea->Size[axis];
        const f32 csize = contentArea->ChildrenSize[axis] + 2.f * contentPadding;
        OverlayColor col = OverlayColor_ScrollBarIdle;

        const char *name =
            axis == LayoutAxis_Horizontal ? "__onyx_id_Horizontal_scroll_bar" : "__onyx_id_Vertical_scroll_bar";
        const LayoutId scrollId = IdFromStack(name);

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
            if (pressed || !Math::ApproachesZero(sinfo.WheelOffset))
            {
                const f32 wheel = barToElement > TKIT_F32_EPSILON ? (sinfo.WheelOffset / barToElement) : 0.f;
                const f32 unbounded = sinfo.CursorOffset + wheel;
                const f32 bounded = Math::Clamp(unbounded, -maxBarTravel, 0.f);

                sinfo.BarOffset = sign * bounded;
                sinfo.ElementOffset = barToElement * sinfo.BarOffset;

                if (pressed)
                {
                    col = OverlayColor_ScrollBarPressed;
                    const NativeWindow *nw = m_Current->Native;
                    sinfo.CursorOffset += sign * nw->WorldMouseDelta[axis];
                }
                else
                {
                    sinfo.CursorOffset = sign * sinfo.BarOffset; // this indirectly saves the WheelOffset state
                    sinfo.WheelOffset = 0.f;
                }
            }
            else
            {
                sinfo.ElementOffset = sign * Math::Clamp(sign * sinfo.ElementOffset, -maxElementTravel, 0.f);
                sinfo.BarOffset = barToElement > TKIT_F32_EPSILON ? sinfo.ElementOffset / barToElement : 0.f;
                sinfo.CursorOffset = sign * sinfo.BarOffset; // this indirectly saves the WheelOffset state
            }
            if (!pressed && hovered)
                col = OverlayColor_ScrollBarHovered;
        }
        else
            sinfo.CursorOffset = sign * sinfo.BarOffset; // this indirectly saves the WheelOffset state

        if (drawBar)
        {
            LySz2 sizing;
            sizing[axis] = sabs(barSize);
            sizing[1 - axis] = sabs(sw);

            LyOf2 offset;
            offset[axis] = oabs(sinfo.BarOffset);
            offset[1 - axis] = oabs(0.f);

            ly.Panel(scrollId, LyPnPar{.FillColor = m_Style[col],
                                       .Sizing = sizing,
                                       .SelfOffset = offset,
                                       .Shape = rect(m_Style[OverlayStyle_ScrollBarWidth])});
        }
        return true;
    }

    sinfo = ScrollBarInfo{};
    return false;
}

/////////////////////////////////////////////
/// END LAYOUT
/////////////////////////////////////////////

/////////////////////////////////////////////
/// INTERACTION/INPUT
/////////////////////////////////////////////

bool Overlay::BeginDragDropSource(const OverlayDragDropFlags flags)
{
    Layout &ly = GetCurrentLayout();
    const LayoutElement *elm = ly.QueryElement(m_LastItem);
    const FocusFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_EnableDragging);

    const f32 th = m_Style[OverlayStyle_DragThreshold];

    const NativeWindow *nw = m_Current->Native;
    const f32 drag = Math::NormSquared(nw->WorldMouse - nw->WorldMouseOnPress);
    if ((m_DragDropId == m_LastItem || drag > th * th) && focusFlags & OverlayFocusQueryFlag_DragSource)
    {
        m_DragDropId = m_LastItem;
        BeginTooltip();

        m_DragDropFlags |= flags;
        return true;
    }
    return false;
}
void Overlay::EndDragDropSource()
{
    EndTooltip();
    if (m_DragDropFlags & (OverlayDragDropFlag_SourceNoTooltip | DragDropFlag_MustClearTooltip))
        trashTooltip();

    m_DragDropFlags = 0;
}
bool Overlay::BeginDragDropTarget(const OverlayDragDropFlags flags)
{
    if (m_DragDropId == NullLayoutId)
        return false;

    Layout &ly = GetCurrentLayout();
    const LayoutElement *elm = ly.QueryElement(m_LastItem);
    const FocusFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_EnableDragging);

    m_DragDropFlags = flags;
    const bool target = focusFlags & OverlayFocusQueryFlag_DragTarget;
    if (target)
    {
        if (!(flags & OverlayDragDropFlag_TargetNoOutline))
        {
            LayoutElement *mod = ly.ModifyElement(m_LastItem);
            mod->OutlineColor = m_Style[OverlayColor_DragOutline];
            mod->OutlineWidth = m_Style[OverlayStyle_DragOutlineWidth];
        }
        if (flags & OverlayDragDropFlag_TargetNoTooltip)
        {
            trashTooltip();
            m_DragDropFlags |= DragDropFlag_MustClearTooltip;
        }
    }

    if (focusFlags & OverlayFocusQueryFlag_DragPayloadDropped)
    {
        m_DragDropFlags |= DragDropFlag_PayloadDropped;
        return true;
    }
    return target;
}

OverlayDragDropPayload Overlay::AcceptDragDropPayload(const TKit::StringView identifier)
{
    if (!m_DragDropPayload)
        return {};

    const bool canAccept = m_DragDropFlags & (OverlayDragDropFlag_TargetAcceptOnHover | DragDropFlag_PayloadDropped);
    const bool isValid = m_DragDropPayload.Identifier == identifier;
    if (isValid)
    {
        m_StateFlags |= StateFlag_DragPayloadAccepted;
        if (canAccept)
            return m_DragDropPayload;
    }
    else if (!(m_DragDropFlags & OverlayDragDropFlag_TargetNoNotAllowedCursor))
        m_StateFlags |= StateFlag_DragPayloadRejected;

    return {};
}

bool Overlay::WantCaptureMouse() const
{
    return m_StateFlags & StateFlag_WantCaptureMouse;
}
bool Overlay::WantCaptureKeyboard() const
{
    return m_StateFlags & StateFlag_WantCaptureKeyboard;
}

OverlayHoverQueryFlags Overlay::queryHoverStatus(const LayoutElement *elm, const f32v2 &padding) const
{
    OverlayHoverQueryFlags flags = 0;
    const usz id = elm ? elm->Id : NullLayoutId;

    const NativeWindow *nw = m_Current->Native;
    const bool hovered = elm && elm->IsHovered(nw->WorldMouse, padding);
    const bool windowBlock =
        !(m_Current->Flags & WindowInternalFlag_Focused) && !(m_Current->Flags & WindowInternalFlag_Hovered);
    const bool grabBlock = m_Grabbed;
    const bool pressBlock = m_PressedId != NullLayoutId && m_PressedId != id;
    const bool activeBlocked =
        !(m_StateFlags & StateFlag_ActiveAllowsInteraction) && m_ActiveId != NullLayoutId && m_ActiveId != id;
    const bool popupBlocked = m_CurrentPopupDepth != m_PopupStack.GetSize();
    const bool popupCollapseBlocked = m_StateFlags & StateFlag_FocusBlockByPopupCollapse;
    const bool disabledBlocked = m_StateFlags & StateFlag_Disabled;
    const bool dragBlocked = m_DraggedId != NullLayoutId && m_DraggedId != id;

    flags |= OverlayHoverQueryFlag_Hovered * hovered;
    flags |= OverlayHoverQueryFlag_BlockedByWindow * windowBlock;
    flags |= OverlayHoverQueryFlag_BlockedByWindowGrab * grabBlock;
    flags |= OverlayHoverQueryFlag_BlockedByPressedItem * pressBlock;
    flags |= OverlayHoverQueryFlag_BlockedByActiveItem * activeBlocked;
    flags |= OverlayHoverQueryFlag_BlockedByPopup * popupBlocked;
    flags |= OverlayHoverQueryFlag_BlockedByPopupCollapse * popupCollapseBlocked;
    flags |= OverlayHoverQueryFlag_BlockedByDisabled * disabledBlocked;
    flags |= OverlayHoverQueryFlag_BlockedByDrag * dragBlocked;

    return flags;
}

bool Overlay::isElementHovered(const LayoutElement *elm, const OverlayHoveredFlags flags, const f32v2 &padding)
{
    const OverlayHoverQueryFlags qflags = queryHoverStatus(elm, padding);
    const bool candidate = isElementHovered(qflags, flags);

    const bool shortDelay = flags & OverlayHoveredFlag_ShortDelay;
    const bool normalDelay = flags & OverlayHoveredFlag_NormalDelay;
    const bool stationary = flags & OverlayHoveredFlag_Stationary;
    TKIT_ASSERT(normalDelay + shortDelay != 2,
                "[ONYX][OVERLAY] Cannot have short delay and normal delay at the same time in tooltip");

    const Layout &ly = GetCurrentLayout();
    if (candidate)
    {
        const f32 statThres = stationary ? m_Style[OverlayStyle_HoverStationaryThreshold] : TKIT_F32_MAX;
        const NativeWindow *nw = m_Current->Native;
        if (Math::NormSquared(nw->WorldMouseDelta) > statThres)
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
        m_HoveredLayoutCandidate = &ly;

        if (noShared && !wasCandidate)
            m_WidgetHoverClock.Restart();

        const f32 seconds = m_WidgetHoverClock.GetElapsed().AsSeconds();
        return seconds >= delay;
    }
    return false;
}

InputConvertInfoFlags Overlay::mustConvertToInputBox(const InputConvertInfoFlags flags)
{
    InputConvertInfoFlags outFlags = flags;
    const bool allowDoubleClick = flags & InputConvertFlag_AllowDoubleClick;
    const bool hovered = flags & InputConvertFlag_Hovered;

    Layout &ly = GetCurrentLayout();

    const usz iboxId = IdFromStack("__onyx_id_Input_box");
    const LayoutElement *ibox = ly.QueryElement(iboxId);

    const NativeWindow *nw = m_Current->Native;
    const bool ctrl = nw->Window->IsKeyPressed(Key_LeftControl);
    const bool dclick = allowDoubleClick && (nw->OverflowClicks == 1);

    const bool triggered = hovered && (dclick || (ctrl && (nw->Flags & NativeWindowFlag_LeftMousePressed)));
    const bool persisted =
        ibox && (m_ActiveId == iboxId || (m_ActiveIdLastFrame == iboxId && ibox->IsHovered(nw->WorldMouse)));

    const bool mustConvert = (triggered || persisted) && !nw->EventKeys[Key_Enter];
    if (mustConvert)
        outFlags |= InputConvertFlag_MustConvert;
    if (m_ActiveId != iboxId && m_ActiveIdLastFrame != iboxId)
        outFlags |= InputConvertFlag_MustOverrideHighlight;

    if (triggered)
        m_ActiveId = iboxId;
    return outFlags;
}

OverlayFocusQueryFlags Overlay::queryAndSetFocusStatus(const LayoutElement *elm, const FocusFlags flags,
                                                       const f32v2 &padding)
{
    if (!elm)
        return 0;

    OverlayFocusQueryFlags outFlags = 0;
    TKIT_ASSERT(m_CurrentPopupDepth <= m_PopupStack.GetSize(),
                "[ONYX][OVERLAY] Popup depth ({}) must not be greater than popup stack ({})", m_CurrentPopupDepth,
                m_PopupStack.GetSize());

    const bool evenWhenAway = flags & FocusFlag_PressedEvenWhenAwayFromHover;

    const NativeWindow *nw = m_Current->Native;
    const bool lmpressed = nw->Flags & NativeWindowFlag_LeftMousePressed;
    const bool lmreleased = nw->Flags & NativeWindowFlag_LeftMouseReleased;

    const OverlayHoverQueryFlags hflags = queryHoverStatus(elm, padding);
    const bool focusHovered = isElementHovered(hflags);

    const bool setHovered = !(flags & FocusFlag_DoNotSetHoveredId);
    const bool setPressed = !(flags & FocusFlag_DoNotSetPressedId);
    const bool setActive = !(flags & FocusFlag_DoNotSetActiveId);
    const bool protectPopup =
        !(flags & FocusFlag_DoNotProtectPopup) && !(m_StateFlags & StateFlag_PopupProtectionForbidden);

    bool lclicked = focusHovered;
    // NOTE(Isma): Could add a _ClickedOnMousePress for right clicks as well
    if (flags & FocusFlag_ClickedOnMousePress)
        lclicked &= lmpressed;
    else
        // NOTE(Isma, 03/07/25): If we are not allowed to set pressed id, there is no way we can report left clicks on
        // release. thats why we allow setting a click event even if pressed id is not set. should not break any widget
        // focus related deal. this is mostly done for user facing read only queries
        lclicked &= lmreleased && (!setPressed || m_PressedId == elm->Id);

    // we leniently allow setting hovered id even when blocked by popups, so that windows dont eat into widget hover
    // signals
    // we additionally allow grab blocks here. this is required for window popups: their own grab event would block
    // this hover check, thus disallowing the popup itself to protect itself because of its own interaction
    const OverlayHoveredFlags popHoverFlags =
        OverlayHoveredFlag_AllowBlockedByPopup | OverlayHoveredFlag_AllowBlockedByPopupCollapse |
        OverlayHoveredFlag_AllowBlockedByWindow | OverlayHoverQueryFlag_BlockedByWindowGrab;

    const bool popupHovered = isElementHovered(hflags, popHoverFlags);
    if (popupHovered && protectPopup)
    {
        if (flags & FocusFlag_HoverOpensPopup)
        {
            TKIT_ASSERT(flags & FocusFlag_HoverRequestsPopupCollapse,
                        "[ONYX][OVERLAY] Causing hover to open a popup without forcing a collapse on this level "
                        "will cause the popup stack to grow indefinitely");
            OpenPopup(elm->Id);
            outFlags |= OverlayFocusQueryFlag_PopupOpen;
        }
        if (flags & FocusFlag_HoverRequestsPopupCollapse)
            requestCollapsePopups();
        m_PopupCollapseDepth = Math::Max(m_PopupCollapseDepth, m_CurrentPopupDepth);
    }

    if (m_CurrentPopupDepth != m_PopupStack.GetSize())
    {
        if (m_PopupStack[m_CurrentPopupDepth] == elm->Id)
            outFlags |= OverlayFocusQueryFlag_PopupOpen;

        // hover id is essentially used to stop from windows from moving/resizing when a widget is being hovered. we
        // want to allow immediate dragging when out of the popup, and close everything, except for intermediate
        // popups. those must still prevent windows from moving, because the popup hierarchy is still not completely
        // collapsed. note that we dont allow grab blocks here. we dont want windows to set hovered id
        if (setHovered && popupHovered && m_CurrentPopupDepth != 0)
            m_HoveredId = elm->Id;

        return outFlags;
    }

    const bool allowPickup = flags & FocusFlag_AllowPressedPickUp;
    const bool rclicked = focusHovered && (nw->Flags & NativeWindowFlag_RightMouseReleased);
    const bool pressingMouse = nw->Flags & NativeWindowFlag_PressingLeftMouse;

    const bool pressed = (focusHovered || (evenWhenAway && m_PressedId == elm->Id)) && pressingMouse &&
                         (allowPickup || lmpressed || m_PressedId == elm->Id);

    if (focusHovered && setHovered)
    {
        m_HoveredId = elm->Id;
        outFlags |= OverlayFocusQueryFlag_Hovered;
    }

    if (flags & FocusFlag_EnableDragging)
    {
        const bool dragged = (focusHovered && lmpressed) || (m_DraggedId == elm->Id && pressingMouse);
        if (dragged)
        {
            m_DraggedId = elm->Id;
            m_StateFlags |= StateFlag_DraggedIdMustPersist;
            outFlags |= OverlayFocusQueryFlag_DragSource;
        }

        const bool dragHovered = isElementHovered(hflags, OverlayHoveredFlag_AllowBlockedByPressedItem |
                                                              OverlayHoveredFlag_AllowBlockedByActiveItem |
                                                              OverlayHoveredFlag_AllowBlockedByDrag);
        if (dragHovered && m_DraggedId != NullLayoutId && m_DraggedId != elm->Id)
        {
            if (m_DragDropId != NullLayoutId)
                outFlags |= OverlayFocusQueryFlag_DragTarget;
            if (lmreleased)
                outFlags |= OverlayFocusQueryFlag_DragPayloadDropped;
        }
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
        if (nw->OverflowClicks == 1)
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

    // *i think* lmreleased check could be omitted bc pressed id wont be set if mouse is not being pressed rn
    if (m_PressedId == elm->Id && !lmreleased)
        m_StateFlags |= StateFlag_PressedIdMustPersist;

    if (m_ActiveId == elm->Id)
    {
        const bool unclaimOnPress = lmpressed && (!focusHovered || !(flags & FocusFlag_KeepActiveOnPressed));
        const bool unclaimOnRelease = lmreleased && !(flags & FocusFlag_KeepActiveOnRelease);

        // NOTE(Isma, 25/06/06): I dont remember what i meant by that
        // NOTE(Isma): Should allow flagging as active still?
        if (!unclaimOnPress && !unclaimOnRelease)
        {
            m_StateFlags |= StateFlag_ActiveIdMustPersist;
            outFlags |= OverlayFocusQueryFlag_Active;

            if ((flags & FocusFlag_ActiveAllowsInteraction) && !pressed)
                m_StateFlags |= StateFlag_ActiveAllowsInteraction;
            // NOTE(Isma, 25/06/06): Should consider unsetting this only on setActive bool var
            else
                m_StateFlags &= ~StateFlag_ActiveAllowsInteraction;
        }
    }

    return outFlags;
}

/////////////////////////////////////////////
/// END INTERACTION/INPUT
/////////////////////////////////////////////

/////////////////////////////////////////////
/// RENDERING
/////////////////////////////////////////////

void Overlay::Draw()
{
    TKIT_PROFILE_NSCOPE("Onyx::Overlay::Draw");
    TKIT_ASSERT(m_IdStack.IsEmpty(),
                "[ONYX][OVERLAY] Id stack size mismatch (size = {}, should be 0). For every PushId(), there must "
                "be a PopId()",
                m_IdStack.GetSize());

    const u32 modalWindow = processWindows();
    const bool windowPromotions = m_Flags & OverlayFlag_WindowPromotions;

    for (const NativeWindow *nw : m_NativeWindows)
    {
        nw->Context->Flush();
        if (modalWindow != 0)
            nw->View->ClearColor.rgba[3] = 1.f;
        else
            nw->View->ClearColor.rgba[3] = 0.f;
    }

    u32 idx = 0;
    for (OverlayWindow *win : m_ActiveWindows)
    {
        NativeWindow *nw = win->Native;
        RenderContext<D2> *ctx = nw->Context;
        if (++idx == modalWindow)
        {
            ctx->Push();
            ctx->Scale(nw->GetDimensions());
            ctx->Alpha(0.2f);
            ctx->Quad();
            ctx->Pop();
        }
        win->Layout.Compile();

        if (windowPromotions)
        {
            ctx->BeginLayoutElements();

            struct FloatRedirect
            {
                RenderContext<D2> *Context;
                f32v2 Offset;
                u32 Depth;
            };

            TKit::StaticArray<FloatRedirect, ONYX_MAX_VIEWS> stack{};

            u32 depth = 0;
            f32v2 offset{0.f};
            for (const LayoutDrawInfo &info : win->Layout.GetDrawInfo())
            {
                if (info.Depth <= depth && !stack.IsEmpty())
                {
                    ctx->EndLayoutElements();

                    const FloatRedirect &rd = stack.GetBack();
                    ctx = rd.Context;
                    offset = rd.Offset;
                    depth = rd.Depth;
                    stack.Pop();
                }
                const LayoutElementFlags relFlags =
                    LayoutElementFlag_FloatEnable | LayoutElementFlag_FloatDrawOnTop | LayoutElementFlag_FloatClip;
                const LayoutElementFlags reqFlags = LayoutElementFlag_FloatEnable | LayoutElementFlag_FloatDrawOnTop;

                if ((info.Flags & relFlags) == reqFlags)
                {
                    const f32v2 &size = info.Size;
                    // subtract size bc real positions are at bottom left
                    const f32v2 scpos = nw->ToScreen(f32v2{info.Position[0], info.Position[1] + size[1]});

                    if (!Math::ApproachesZero(size[0], 1.f) && !Math::ApproachesZero(size[1], 1.f) &&
                        (!stack.IsEmpty() || isOutsideNative(nw, scpos, size)))
                    {
                        m_StateFlags |= StateFlag_ActivePromotedFloatElement;
                        const auto it = m_FloatWindows.Find(info.Id);
                        NativeWindow *floatNative;
                        if (it == m_FloatWindows.end())
                        {
                            floatNative = createNativeWindow(scpos, size, WindowFlag_Floating);
                            floatNative->Parent = nw;
                            floatNative->Flags |=
                                NativeWindowFlag_RepresentsFloatElement | NativeWindowFlag_ActivePromotedFloatElement;
                            m_FloatWindows[info.Id] = floatNative;
                        }
                        else
                        {
                            floatNative = it->Value;
                            floatNative->Flags |= NativeWindowFlag_ActivePromotedFloatElement;
                            const i32v2 ipos = i32v2{scpos};
                            const i32v2 wpos = floatNative->Window->GetPosition();
                            if (ipos[0] != wpos[0] || ipos[1] != wpos[1])
                                floatNative->Window->SetPosition(ipos);
                        }

                        stack.Append(ctx, offset, depth);
                        depth = info.Depth;

                        ctx = floatNative->Context;
                        offset = floatNative->GetWorldBottomLeft() - info.Position;

                        ctx->Flush();
                        ctx->BeginLayoutElements();
                    }
                }
                if (!(info.Flags & LayoutElementFlag_Drawable))
                    continue;

                if (stack.IsEmpty())
                    ctx->LayoutElement(info);
                else
                {
                    LayoutDrawInfo elm = info;
                    elm.Position += offset;
                    if (elm.ClipMin[0] != TKIT_F32_MAX)
                    {
                        elm.ClipMax += offset;
                        elm.ClipMin += offset;
                    }
                    ctx->LayoutElement(elm);
                }
            }
            for (const FloatRedirect &rd : stack)
                rd.Context->EndLayoutElements();
            ctx->EndLayoutElements();
        }
        else
        {
            win->Flags &= ~WindowInternalFlag_Active;
            ctx->Layout(win->Layout);
        }
    }

    if (m_Tooltip)
    {
        m_Tooltip->Layout.EndPanel();
        m_Tooltip->Layout.EndPanel();
        m_Tooltip->Layout.Compile();
        m_Tooltip->Native->Context->Layout(m_Tooltip->Layout);
        if (windowPromotions && (m_Tooltip->Flags & WindowInternalFlag_OwnsNative))
            m_Tooltip->Native->Window->SetPosition(i32v2{m_Tooltip->Native->ScreenPos});
    }

    m_ActiveWindows.Clear();
    if (!windowPromotions)
        demoteAllWindows();
    else
        manageWindowPromotions();
    resetTooltip();
}

/////////////////////////////////////////////
/// END RENDERING
/////////////////////////////////////////////

/////////////////////////////////////////////
/// HELPERS
/////////////////////////////////////////////

const FontData &Overlay::getFontData() const
{
    if (!m_NativeWindows.IsEmpty())
    {
        const NativeWindow *nw = m_NativeWindows[0];
        const Resource font = nw->Context->GetState().Font;
        return Resources::GetFontData(font);
    }
    return Resources::GetFontData(Resources::GetDefaultResources().Font);
}
f32 Overlay::getLineHeight() const
{
    return m_Style[OverlayStyle_FontSize] * getFontData().LineHeight;
}
bool Overlay::isAutoResize() const
{
    return m_CurrentPopupDepth == m_Current->PopupDepth && (m_Current->Flags & OverlayWindowFlag_AutoResize) &&
           !(m_StateFlags & StateFlag_MenuBarActive);
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

f32v4 Overlay::getWorldEffectiveBorders() const
{
    const NativeWindow *nw = m_Current->Native;
    const bool windowPromotions = m_Flags & OverlayFlag_WindowPromotions;
    f32v2 topLeft;
    f32v2 bottomRight;
    if (windowPromotions)
    {
        const f32v2 mdims = getMonitorDimensions();
        topLeft = nw->ToWorld(f32v2{0.f});
        bottomRight = nw->ToWorld(mdims);
    }
    else
    {
        topLeft = nw->GetWorldTopLeft();
        bottomRight = nw->GetWorldBottomRight();
    }
    return f32v4{topLeft[0], topLeft[1], bottomRight[0], bottomRight[1]};
}

/////////////////////////////////////////////
/// END HELPERS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// DEMO
/////////////////////////////////////////////

void Overlay::ShowDemo()
{
    TKIT_PROFILE_NSCOPE("Onyx::Overlay::Demo");
    static Onyx::OverlayWindowFlags wflags = 0;
    static bool enableSettings = false;
    static bool enableRenderer = false;
    static bool enableStyleEditor = false;
    static bool enableMainMenu = false;

    const Onyx::OverlayTreeFlags drawLines = Onyx::OverlayTreeFlag_DrawLines;
    Overlay *ov = this;

    const auto editWindowFlags = [&](Onyx::OverlayWindowFlags *flags) {
        ov->CheckBoxFlags("OverlayWindowFlag_NoResize", flags, Onyx::OverlayWindowFlag_NoResize);
        ov->CheckBoxFlags("OverlayWindowFlag_NoMove", flags, Onyx::OverlayWindowFlag_NoMove);
        ov->CheckBoxFlags("OverlayWindowFlag_NoCollapse", flags, Onyx::OverlayWindowFlag_NoCollapse);
        ov->CheckBoxFlags("OverlayWindowFlag_NoScrollBar", flags, Onyx::OverlayWindowFlag_NoScrollBar);
        ov->CheckBoxFlags("OverlayWindowFlag_NoHeaderBar", flags, Onyx::OverlayWindowFlag_NoHeaderBar);
        ov->CheckBoxFlags("OverlayWindowFlag_NoBringToFocus", flags, Onyx::OverlayWindowFlag_NoBringToFocus);
        ov->CheckBoxFlags("OverlayWindowFlag_NoPromotion", flags, Onyx::OverlayWindowFlag_NoPromotion);
        ov->CheckBoxFlags("OverlayWindowFlag_NoVerticalScroll", flags, Onyx::OverlayWindowFlag_NoVerticalScroll);
        ov->CheckBoxFlags("OverlayWindowFlag_HorizontalScroll", flags, Onyx::OverlayWindowFlag_HorizontalScroll);
        ov->CheckBoxFlags("OverlayWindowFlag_AutoResize", flags, Onyx::OverlayWindowFlag_AutoResize);
        ov->CheckBoxFlags("OverlayWindowFlag_BringToTop", flags, Onyx::OverlayWindowFlag_BringToTop);
        ov->CheckBoxFlags("OverlayWindowFlag_Modal", flags, Onyx::OverlayWindowFlag_Modal);
        ov->CheckBoxFlags("OverlayWindowFlag_NoCloseButton", flags, Onyx::OverlayWindowFlag_NoCloseButton);
        ov->CheckBoxFlags("OverlayWindowFlag_MenuBar", flags, Onyx::OverlayWindowFlag_MenuBar);
    };
    const auto drawMenus = [&] {
        if (ov->BeginMenu("Options"))
        {
            ov->MenuItem("Window settings", &enableSettings);
            ov->MenuItem("Renderer stats", &enableRenderer);
            ov->MenuItem("Style editor", &enableStyleEditor);
            ov->MenuItem("Main menu bar", &enableMainMenu);
            ov->EndMenu();
        }
        if (ov->BeginMenu("Menu"))
        {
            static bool selected = false;
            ov->HorizontalSeparator("This is a demo menu");
            ov->MenuItem("New");
            ov->MenuItem("Open");
            if (ov->BeginMenu("Open as..."))
            {
                ov->MenuItem("File 1");
                ov->MenuItem("File 2");
                ov->MenuItem("File 3");
                ov->MenuItem("File 4");
                if (ov->BeginMenu("More..."))
                {
                    ov->MenuItem("Nothing to see here");
                    ov->EndMenu();
                }
                ov->EndMenu();
            }
            if (ov->BeginMenu("Options"))
            {
                static f32 val = 3.f;
                ov->HorizontalSlider("Slider", &val, -10.f, 10.f);
                ov->Button("Press me");

                ov->BeginScroll("Scroll", 100.f, Onyx::OverlayScrollFlag_Borders);
                for (u32 i = 0; i < 10; ++i)
                    ov->Text("Bla bla");
                ov->EndScroll();

                static u32 element = 0;
                ov->DropDown("Drop down", &element, "Hello 1#Hello 2#Hello 3");
                ov->EndMenu();
            }
            ov->MenuItem("Select", &selected);
            ov->EndMenu();
        }
    };

    if (enableMainMenu && ov->BeginMainMenuBar())
    {
        drawMenus();
        ov->EndMainMenuBar();
    }

    static bool disableGlobal = false;
    if (ov->BeginWindow("Overlay demo", wflags | Onyx::OverlayWindowFlag_MenuBar))
    {
        if (ov->PushTree("Configuration"))
        {
            ov->BeginDisabled(m_Flags & OverlayFlag_FloatingMode);
            ov->CheckBoxFlags("OverlayFlag_WindowPromotions", &m_Flags, Onyx::OverlayFlag_WindowPromotions);
            ov->EndDisabled();
            if (m_Flags & OverlayFlag_FloatingMode)
                ov->SetItemTooltip(
                    "When in floating mode, window promotions must be on or terrible things will happen");
            ov->PopTree();
        }
        const NativeWindow *nw = GetMainNativeWindow();
        const f32 ftime = nw ? Onyx::GetDeltaTime(nw->Window).AsMilliseconds() : Onyx::GetDeltaTime().AsMilliseconds();
        if (ov->PushTree("General", drawLines))
        {
            ov->SetNextTextId("Delta time");
            ov->Text("Delta time: {:.2f} ms", ftime);
            if (ov->BeginItemTooltip())
            {
                ov->TextRaw("I am a tooltip!");
                ov->TextRaw("And this is the time that passes between frames");
                ov->EndTooltip();
            }
            static bool disableLocal = false;
            static bool dummy = false;

            ov->CheckBox("Disable other sections", &disableGlobal);
            ov->CheckBox("Disable items below", &disableLocal);

            ov->BeginDisabled(disableLocal);
            ov->TextRaw("I can be disabled");
            ov->CheckBox("I can be disabled##CB", &dummy);
            ov->Button("I can be disabled##ov->Button");
            ov->EndDisabled();

            ov->PopTree();
        }

        if (ov->BeginMenuBar())
        {
            drawMenus();
            ov->EndMenuBar();
        }

        ov->BeginDisabled(disableGlobal);
        if (ov->PushTree("Buttons", drawLines))
        {
            static bool helloText = false;
            if (ov->Button("This is a button"))
                helloText = !helloText;

            if (helloText)
                ov->Text("Hi!");

            ov->Button("I have a twin##Cant see me");
            ov->Button("I have a twin##Cant see me eiter");
            ov->Button("I am a long button", Onyx::OverlayButtonFlag_SpanFullWidth);

            ov->PushDirection(Onyx::LayoutDirection_LeftToRight, 0.f);
            ov->TextRaw("A small button can be easily ");
            ov->Button("embedded", Onyx::OverlayButtonFlag_Small);
            ov->TextRaw(" in text");
            ov->PopDirection();

            static u32 radio = 0;
            ov->PushDirection(Onyx::LayoutDirection_LeftToRight);
            ov->RadioButton("I am enabled!", &radio, 0);
            ov->RadioButton("I am not :(", &radio, 1);

            ov->PopDirection();
            ov->PopTree();
        }

        if (ov->PushTree("Color editor", drawLines))
        {
            static Onyx::OverlayColorFlags cflags = 0;
            ov->CheckBoxFlags("OverlayColorFlag_NoAlpha", &cflags, Onyx::OverlayColorFlag_NoAlpha);
            ov->CheckBoxFlags("OverlayColorFlag_NoInput", &cflags, Onyx::OverlayColorFlag_NoInput);
            ov->CheckBoxFlags("OverlayColorFlag_NoColorMarkers", &cflags, Onyx::OverlayColorFlag_NoColorMarkers);
            ov->CheckBoxFlags("OverlayColorFlag_NoPicker", &cflags, Onyx::OverlayColorFlag_NoPicker);
            ov->CheckBoxFlags("OverlayColorFlag_NoTooltip", &cflags, Onyx::OverlayColorFlag_NoTooltip);
            ov->CheckBoxFlags("OverlayColorFlag_NoPreview", &cflags, Onyx::OverlayColorFlag_NoPreview);
            ov->CheckBoxFlags("OverlayColorFlag_NoTooltipLabel", &cflags, Onyx::OverlayColorFlag_NoTooltipLabel);
            ov->CheckBoxFlags("OverlayColorFlag_HSV", &cflags, Onyx::OverlayColorFlag_HSV);
            ov->CheckBoxFlags("OverlayColorFlag_Hex", &cflags, Onyx::OverlayColorFlag_Hex);
            ov->CheckBoxFlags("OverlayColorFlag_Float", &cflags, Onyx::OverlayColorFlag_Float);

            static Onyx::Color col = Onyx::Color_Red;
            ov->ColorEditor("Color", &col, cflags);
            ov->ColorPreview("Preview", col, cflags);
            ov->ColorButton("ov->Button", &col, cflags);
            ov->ColorPicker("Picker", &col, cflags);
            ov->PopTree();
        }

        if (ov->PushTree("Drag & Drop"))
        {
            ov->TextRaw(
                TextMode_Wrapped,
                "Generally drag & drop is enabled by default in color editors. Here it is "
                "disabled explicitly by using OverlayColorFlag_NoDragDrop for demonstration "
                "purposes through the user-facing API. Color editors are the only widgets (for "
                "now) that provide drag & drop capabilities out of the box, but they can be set up for any UI element");

            static Onyx::OverlayDragDropFlags dflags = 0;
            ov->CheckBoxFlags("OverlayDragDropFlag_SourceNoTooltip", &dflags,
                              Onyx::OverlayDragDropFlag_SourceNoTooltip);

            ov->CheckBoxFlags("OverlayDragDropFlag_TargetNoTooltip", &dflags,
                              Onyx::OverlayDragDropFlag_TargetNoTooltip);
            ov->CheckBoxFlags("OverlayDragDropFlag_TargetNoOutline", &dflags,
                              Onyx::OverlayDragDropFlag_TargetNoOutline);
            ov->CheckBoxFlags("OverlayDragDropFlag_TargetNoNotAllowedCursor", &dflags,
                              Onyx::OverlayDragDropFlag_TargetNoNotAllowedCursor);
            ov->CheckBoxFlags("OverlayDragDropFlag_TargetAcceptOnHover", &dflags,
                              Onyx::OverlayDragDropFlag_TargetAcceptOnHover);

            static Onyx::Color col1 = Color_Orange;
            static Onyx::Color col2 = Color_Cyan;

            ov->ColorEditor("Pick me up!", &col1, OverlayColorFlag_NoDragDrop);
            if (ov->BeginDragDropSource(dflags))
            {
                ov->SetDragDropPayload("COLOR", &col1);
                ov->PushStyleVar(OverlayStyle_ColorTooltipSize, 32.f);
                ov->ColorPreviewTooltip("Color", col1, OverlayColorFlag_NoTooltipColorInfo);
                ov->PopStyleVar();
                ov->EndDragDropSource();
            }

            ov->ColorEditor("Drop something on me!", &col2, OverlayColorFlag_NoDragDrop);
            if (ov->BeginDragDropTarget(dflags))
            {
                if (const auto pl = ov->AcceptDragDropPayload("COLOR"))
                    col2 = *rcast<Color *>(pl.Data);
                ov->EndDragDropTarget();
            }

            static f32 val1 = 1.f;
            static f32 val2 = 2.f;
            static f32 val3 = 3.f;

            ov->HorizontalSlider("Pick me up!##Slider", &val1, 0.f, 10.f);
            if (ov->BeginDragDropSource(dflags))
            {
                ov->SetDragDropPayload("VALUE", &val1);
                ov->Text("Value: {}", val1);
                ov->EndDragDropSource();
            }

            ov->HorizontalSlider("Drop something on me!##Slider", &val2, 0.f, 10.f);
            if (ov->BeginDragDropTarget(dflags))
            {
                if (const auto pl = ov->AcceptDragDropPayload("VALUE"))
                    val2 = *rcast<f32 *>(pl.Data);
                ov->EndDragDropTarget();
            }

            HorizontalDrag("You can do both here!##Drag", &val3, 0.1f, 0.f, 10.f);
            if (ov->BeginDragDropSource(dflags))
            {
                ov->SetDragDropPayload("VALUE", &val3);
                ov->Text("Value: {}", val3);
                ov->EndDragDropSource();
            }
            if (ov->BeginDragDropTarget(dflags))
            {
                if (const auto pl = ov->AcceptDragDropPayload("VALUE"))
                    val3 = *rcast<f32 *>(pl.Data);
                ov->EndDragDropTarget();
            }

            ov->PopTree();
        }

        if (ov->PushTree("Dropdowns", drawLines))
        {
            static Onyx::OverlayDropDownFlags dflags = 0;
            ov->CheckBoxFlags("OverlayDropDownFlag_NoArrowButton", &dflags, Onyx::OverlayDropDownFlag_NoArrowButton);
            ov->CheckBoxFlags("OverlayDropDownFlag_NoPreview", &dflags, Onyx::OverlayDropDownFlag_NoPreview);
            ov->CheckBoxFlags("OverlayDropDownFlag_HeightSmall", &dflags, Onyx::OverlayDropDownFlag_HeightSmall);
            ov->CheckBoxFlags("OverlayDropDownFlag_HeightRegular", &dflags, Onyx::OverlayDropDownFlag_HeightRegular);
            ov->CheckBoxFlags("OverlayDropDownFlag_HeightLargest", &dflags, Onyx::OverlayDropDownFlag_HeightLargest);
            ov->CheckBoxFlags("OverlayDropDownFlag_Tight", &dflags, Onyx::OverlayDropDownFlag_Tight);
            if (ov->BeginDropDown("Hello", "Some preview", dflags))
            {
                static bool dummy = false;
                ov->TextRaw("Some text");
                ov->Button("I am a button in a drop down!");
                ov->CheckBox("You can pretty much put whatever you want in here...", &dummy);

                if (ov->BeginDropDown("Even another dropdown!", "I am another preview", dflags))
                {
                    for (u32 i = 0; i < 10; ++i)
                        ov->TextRaw("Bla bla");
                    ov->EndDropDown();
                }

                ov->EndDropDown();
            }
            const TKit::FixedArray<TKit::StringView, 8> elements{"Element 1", "Element 2", "Element 3", "Element 4",
                                                                 "Element 5", "Element 6", "Element 7", "Element 8"};
            static u32 idx = 0;
            ov->DropDown("One-liner 1", &idx, elements, dflags);
            ov->DropDown("One-liner 2##You should not see this", &idx, "I am#part of#the same#string", dflags);
            ov->PopTree();
        }

        if (ov->PushTree("Inputs", drawLines))
        {
            static Onyx::OverlayInputFlags iflags = 0;
            ov->CheckBoxFlags("OverlayInputFlag_EnterReturnsTrue", &iflags, Onyx::OverlayInputFlag_EnterReturnsTrue);
            ov->CheckBoxFlags("OverlayInputFlag_EnterCommitsBuffer", &iflags,
                              Onyx::OverlayInputFlag_EnterCommitsBuffer);
            ov->CheckBoxFlags("OverlayInputFlag_EscapeClearsAll", &iflags, Onyx::OverlayInputFlag_EscapeClearsAll);
            ov->CheckBoxFlags("OverlayInputFlag_AutoSelectAll", &iflags, Onyx::OverlayInputFlag_AutoSelectAll);
            ov->CheckBoxFlags("OverlayInputFlag_NoHorizontalScroll", &iflags,
                              Onyx::OverlayInputFlag_NoHorizontalScroll);
            ov->CheckBoxFlags("OverlayInputFlag_ElideLeft", &iflags, Onyx::OverlayInputFlag_ElideLeft);
            ov->CheckBoxFlags("OverlayInputFlag_StepButtons", &iflags, Onyx::OverlayInputFlag_StepButtons);
            ov->CheckBoxFlags("OverlayInputFlag_NoUndoRedo", &iflags, Onyx::OverlayInputFlag_NoUndoRedo);

            static char buf1[32] = "This is some nice text";
            ov->InputText("Text 1", buf1, 32, "I am a little hint", iflags);

            static i32 iival = 4;
            ov->InputNumeric("Some integer", &iival, "{}", "Add a number!", iflags);

            static f32 ifval = 8.f;
            ov->InputNumeric("Some float", &ifval, "{:.3f}", nullptr, iflags);
            ov->PopTree();
        }

        if (ov->PushTree("Progress bars", drawLines))
        {
            constexpr u32 top = 1200;
            static u32 current = 0;
            static f32 time = 0.f;
            static TKit::Clock clock{};
            static bool manual = false;

            ov->CheckBox("Manual", &manual);
            if (manual)
                ov->HorizontalSlider("AAA", &current, 0u, top);
            else
            {
                time += clock.Restart().AsSeconds();
                current = u32(0.5f * (1.f - Math::Sine(time)) * top);
            }
            const f32 pct = f32(current) / f32(top);

            ov->ProgressBar("PB 1", pct, "{:.1f}%", 100.f * pct);
            ov->ProgressBar("PB 2", pct, "{}/{}", current, top);
            ov->ProgressBar("Indeterminate", "Waiting...", -time);

            ov->PopTree();
        }

        if (ov->PushTree("Popups", drawLines))
        {
            static Onyx::OverlayWindowFlags pflags =
                Onyx::OverlayWindowFlag_BringToTop | Onyx::OverlayWindowFlag_AutoResize;
            editWindowFlags(&pflags);

            if (ov->Button("Open popup"))
                ov->OpenPopup("Popup example");

            if (ov->BeginPopup("Popup example", pflags))
            {
                ov->TextRaw("I am a popup");
                if (ov->Button("Another one?"))
                    ov->OpenPopup("Yes, another one");

                ov->SetItemTooltip("This will open another popup!");

                if (ov->BeginPopup("Yes, another one", pflags))
                {
                    ov->TextRaw("Hi!");

                    static u32 value = 3;
                    ov->SetNextTextId("Text id");
                    ov->Text("Right click me and change the value!: {}", value);
                    if (ov->BeginPopupContextItem("Value edit", Onyx::OverlayWindowFlag_AutoResize |
                                                                    Onyx::OverlayWindowFlag_BringToTop))
                    {
                        ov->InputNumeric("Value", &value);
                        ov->EndPopup();
                    }

                    if (ov->Button("Close##Two"))
                        ov->CloseCurrentPopup();
                    ov->EndPopup();
                }

                if (ov->Button("Close##One"))
                    ov->CloseCurrentPopup();
                ov->EndPopup();
            }
            ov->PopTree();
        }

        if (ov->PushTree("Queries", drawLines))
        {
            ov->HorizontalSeparator("Hover flags");
            static Onyx::OverlayHoveredFlags hflags = 0;

            ov->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByWindow", &hflags,
                              Onyx::OverlayHoveredFlag_AllowBlockedByWindow);
            ov->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByWindowGrab", &hflags,
                              Onyx::OverlayHoveredFlag_AllowBlockedByWindowGrab);
            ov->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByPressedItem", &hflags,
                              Onyx::OverlayHoveredFlag_AllowBlockedByPressedItem);
            ov->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByActiveItem", &hflags,
                              Onyx::OverlayHoveredFlag_AllowBlockedByActiveItem);
            ov->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByPopup", &hflags,
                              Onyx::OverlayHoveredFlag_AllowBlockedByPopup);
            ov->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByPopupCollapse", &hflags,
                              Onyx::OverlayHoveredFlag_AllowBlockedByPopupCollapse);
            ov->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByDisabled", &hflags,
                              Onyx::OverlayHoveredFlag_AllowBlockedByDisabled);
            ov->CheckBoxFlags("OverlayHoveredFlag_AllowBlockedByDrag", &hflags,
                              Onyx::OverlayHoveredFlag_AllowBlockedByDrag);
            ov->CheckBoxFlags("OverlayHoveredFlag_NoSharedDelay", &hflags, Onyx::OverlayHoveredFlag_NoSharedDelay);

            ov->BeginDisabled(hflags & Onyx::OverlayHoveredFlag_NormalDelay);
            ov->CheckBoxFlags("OverlayHoveredFlag_ShortDelay", &hflags, Onyx::OverlayHoveredFlag_ShortDelay);
            ov->EndDisabled();

            ov->BeginDisabled(hflags & Onyx::OverlayHoveredFlag_ShortDelay);
            ov->CheckBoxFlags("OverlayHoveredFlag_NormalDelay", &hflags, Onyx::OverlayHoveredFlag_NormalDelay);
            ov->EndDisabled();

            ov->CheckBoxFlags("OverlayHoveredFlag_Stationary", &hflags, Onyx::OverlayHoveredFlag_Stationary);

            ov->HorizontalSeparator("Focus flags");
            static Onyx::OverlayFocusFlags fflags = 0;
            ov->CheckBoxFlags("OverlayFocusFlag_PressedEvenWhenAwayFromHover", &fflags,
                              Onyx::OverlayFocusFlag_PressedEvenWhenAwayFromHover);

            ov->HorizontalSeparator("The experiment");
            if (ov->PushTree("I am to be queried"))
                ov->PopTree();

            const bool hovered = IsItemHovered(hflags);
            const bool opened = IsItemOpened();
            const Onyx::OverlayHoverQueryFlags hqflags = QueryItemHoverStatus();
            const Onyx::OverlayFocusQueryFlags fqflags = QueryItemFocusStatus(fflags);

            ov->HorizontalSeparator("Hovering info");
            ov->Text("Hovered: {}", hovered);
            ov->Text("Blocked by window: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByWindow));
            ov->Text("Blocked by window grab: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByWindowGrab));
            ov->Text("Blocked by pressed item: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPressedItem));
            ov->Text("Blocked by active item: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByActiveItem));
            ov->Text("Blocked by popup : {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPopup));
            ov->Text("Blocked by popup collapse : {}",
                     bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPopupCollapse));
            ov->Text("Blocked by disabled : {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByDisabled));
            ov->Text("Natively hovered: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_Hovered));

            ov->HorizontalSeparator("Focus info");

            static TKit::Clock lclickClock{};
            static TKit::Clock rclickClock{};
            static TKit::Clock dclickClock{};

            if (fqflags & Onyx::OverlayFocusQueryFlag_LeftClicked)
                lclickClock.Restart();
            if (fqflags & Onyx::OverlayFocusQueryFlag_RightClicked)
                rclickClock.Restart();
            if (fqflags & Onyx::OverlayFocusQueryFlag_DoubleClicked)
                dclickClock.Restart();

            ov->Text("Hovered: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_Hovered));
            ov->Text("Pressed: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_Pressed));
            ov->Text("Left clicked: {:.1f} seconds ago", lclickClock.GetElapsed().AsSeconds());
            ov->Text("Right clicked: {:.1f} seconds ago", rclickClock.GetElapsed().AsSeconds());
            ov->Text("Double clicked: {:.1f} seconds ago", dclickClock.GetElapsed().AsSeconds());
            ov->Text("Active: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_Active));
            ov->Text("Just active: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_JustActive));
            ov->Text("Popup open: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_PopupOpen));

            ov->HorizontalSeparator("State info");
            ov->Text("Opened: {}", opened);

            ov->HorizontalSeparator("Focus info");
            ov->Text("Want capture mouse: {}", WantCaptureMouse());
            ov->Text("Want capture keyboard: {}", WantCaptureKeyboard());

            ov->PopTree();
        }

        if (ov->PushTree("Selectables", drawLines))
        {
            static Onyx::OverlaySelectableFlags sflags = 0;

            ov->CheckBoxFlags("OverlaySelectableFlag_SpanLabelWidth", &sflags,
                              Onyx::OverlaySelectableFlag_SpanLabelWidth);
            ov->CheckBoxFlags("OverlaySelectableFlag_SelectOnDoubleClick", &sflags,
                              Onyx::OverlaySelectableFlag_SelectOnDoubleClick);
            ov->CheckBoxFlags("OverlaySelectableFlag_Highlight", &sflags, Onyx::OverlaySelectableFlag_Highlight);
            ov->CheckBoxFlags("OverlaySelectableFlag_CheckBox", &sflags, Onyx::OverlaySelectableFlag_CheckBox);

            ov->Selectable("I am not selected at all##One", false, sflags);
            ov->Selectable("I am permanently selected", true, sflags);
            ov->Selectable("I am not selected at all##Two", false, sflags);

            static bool enabled[3] = {false, false, false};
            ov->Selectable("I can be toggled on and off##One", &enabled[0], sflags);
            ov->Selectable("I can be toggled on and off##Two", &enabled[1], sflags);

            ov->BeginSelectable(&enabled[2], sflags);
            ov->TextRaw("I am a fancy selectable");
            ov->TextRaw("I even have multiple lines");
            ov->EndSelectable();

            const TKit::FixedArray<TKit::StringView, 8> elements{"Element 1", "Element 2", "Element 3", "Element 4",
                                                                 "Element 5", "Element 6", "Element 7", "Element 8"};

            static u32 idx = 0;
            ov->ListBox("List box 1", &idx, elements, sflags);
            ov->ListBox("List box 2##You should not see this", &idx, "I am#part of#the same#string", sflags);

            ov->PopTree();
        }

        if (ov->PushTree("Scroll area", drawLines))
        {
            static f32v2 dimensions = {400.f, 200.f};
            static bool xunlim = true;
            static Onyx::OverlayScrollFlags sflags = 0;
            ov->CheckBoxFlags("OverlayScrollFlag_Borders", &sflags, Onyx::OverlayScrollFlag_Borders);
            ov->CheckBoxFlags("OverlayScrollFlag_Title", &sflags, Onyx::OverlayScrollFlag_Title);
            ov->CheckBoxFlags("OverlayScrollFlag_NoBackground", &sflags, Onyx::OverlayScrollFlag_NoBackground);
            ov->CheckBoxFlags("OverlayScrollFlag_NoScrollBar", &sflags, Onyx::OverlayScrollFlag_NoScrollBar);
            ov->CheckBoxFlags("OverlayScrollFlag_NoVerticalScroll", &sflags, Onyx::OverlayScrollFlag_NoVerticalScroll);
            ov->CheckBoxFlags("OverlayScrollFlag_HorizontalScroll", &sflags, Onyx::OverlayScrollFlag_HorizontalScroll);

            ov->CheckBox("Unlimited width", &xunlim);
            if (xunlim)
                ov->HorizontalSlider("Maximum height", &dimensions[1], 50.f, 800.f, "{:.0f}");
            else
                ov->HorizontalSlider("Maximum dimensions", &dimensions, 50.f, 800.f, "{:.0f}");

            const bool focused = ov->BeginScroll("Title", dimensions[1], xunlim ? TKIT_F32_MAX : dimensions[0], sflags);

            if (focused)
                ov->TextRaw("I am focused!");
            else
                ov->TextRaw("I am not focused");

            ov->TextRaw("I am a long text that will require you to scroll horizontally to read fully, allowing me to "
                        "showcase the feature");
            ov->Button("I am a useless button");
            if (ov->PushTree("Some content", Onyx::OverlayTreeFlag_StartOpen))
            {
                for (u32 i = 0; i < 10; ++i)
                    ov->Text("Bla bla");
                ov->PopTree();
            }

            ov->BeginScroll("I am yet another scroll", 200.f, 200.f, sflags);
            if (ov->PushTree("Some more content"))
            {
                for (u32 i = 0; i < 10; ++i)
                    ov->Text("Bla bla");
                ov->PopTree();
            }
            ov->EndScroll();

            ov->EndScroll();
            ov->PopTree();
        }

        if (ov->PushTree("Sliders/Drags", drawLines))
        {
            static Onyx::OverlaySliderFlags sflags = 0;
            ov->CheckBoxFlags("OverlaySliderFlag_ClampOnInput", &sflags, Onyx::OverlaySliderFlag_ClampOnInput);
            ov->CheckBoxFlags("OverlaySliderFlag_Logarithmic", &sflags, Onyx::OverlaySliderFlag_Logarithmic);
            ov->CheckBoxFlags("OverlaySliderFlag_NoRoundToFormat", &sflags, Onyx::OverlaySliderFlag_NoRoundToFormat);
            ov->CheckBoxFlags("OverlaySliderFlag_NoInput", &sflags, Onyx::OverlaySliderFlag_NoInput);

            ov->HorizontalSeparator("Horizontal sliders");
            static f32 fval[2] = {4, 7};
            ov->Text("Underlying values: {:.2f}, {:.2f}", fval[0], fval[1]);

            ov->HorizontalSlider("My slider float", fval, 0.f, 10.f, "Value: {:.1f}", 2, sflags);
            ov->HorizontalSlider("My other slider float", fval, -10.f, 20.f, "{:.2f}", 1, sflags);

            static i32 ival = 7;
            ov->Text("Underlying value: {}", ival);
            ov->HorizontalSlider("My slider int", &ival, -3, 28, nullptr, 1, sflags);
            ov->HorizontalSlider("My small slider int", &ival, 0, 2, nullptr, 1, sflags);

            ov->HorizontalSeparator("Horizontal drags");
            static f32 speed = 0.1f;
            ov->HorizontalSlider("Drag speed", &speed, 1e-2f, 10.f, "{:.2f}", 1, Onyx::OverlaySliderFlag_Logarithmic);

            ov->HorizontalDrag("My drag float", fval, speed, 0.f, 10.f, "Value: {:.1f}", 2, sflags);
            ov->HorizontalDrag("My other drag float", fval, speed, -10.f, 20.f, "{:.2f}", 1, sflags);

            ov->HorizontalDrag("My drag int", &ival, speed, -3, 28, nullptr, 1, sflags);
            ov->HorizontalDrag("My small drag int", &ival, speed, 0, 2, nullptr, 1, sflags);

            static u32 uval2[3] = {7, 2, 5};
            ov->HorizontalDrag("My drag uint", uval2, speed, 1, 87, nullptr, 3, sflags);

            ov->PushId("Vertical");

            ov->HorizontalSeparator("Vertical sliders");

            ov->PushDirection(LayoutDirection_LeftToRight);

            ov->VerticalSlider("My slider float", fval, 0.f, 10.f, "Value: {:.1f}", 2, sflags);
            ov->VerticalSlider("My other slider float", fval, -10.f, 20.f, "{:.2f}", 1, sflags);

            ov->VerticalSlider("My slider int", &ival, -3, 28, nullptr, 1, sflags);
            ov->VerticalSlider("My small slider int", &ival, 0, 2, nullptr, 1, sflags);

            ov->PopDirection();

            ov->HorizontalSeparator("Vertical drags");

            ov->PushDirection(LayoutDirection_LeftToRight);

            ov->VerticalDrag("My drag float", fval, speed, 0.f, 10.f, "Value: {:.1f}", 2, sflags);
            ov->VerticalDrag("My other drag float", fval, speed, -10.f, 20.f, "{:.2f}", 1, sflags);

            ov->VerticalDrag("My drag int", &ival, speed, -3, 28, nullptr, 1, sflags);
            ov->VerticalDrag("My small drag int", &ival, speed, 0, 2, nullptr, 1, sflags);

            ov->VerticalDrag("My drag uint", uval2, speed, 1, 87, nullptr, 3, sflags);

            ov->PopDirection();
            ov->PopId();

            ov->PopTree();
        }

        if (ov->PushTree("Tabs", drawLines))
        {
            ov->TextRaw(Onyx::TextMode_Wrapped,
                        "Tab features are currently pretty minimal at the moment and the styling is not great either");
            static bool tab1 = true;
            ov->CheckBox("Enable tab 1", &tab1);

            ov->BeginTabBar();
            if (ov->BeginTab("Tab 1", &tab1, Onyx::OverlayTabFlag_StartOpen))
            {
                ov->TextRaw("I am tab 1");
                ov->EndTab();
            }
            if (ov->BeginTab("Tab 2"))
            {
                ov->TextRaw("I am tab 2");
                ov->EndTab();
            }
            ov->EndTabBar();
            ov->PopTree();
        }

        if (ov->PushTree("Text", drawLines))
        {
            ov->TextRaw("This is some raw text");
            ov->TextRaw(
                Onyx::TextMode_Wrapped,
                "This is some text that should wrap because it is too long to fit into the width of the window");
            ov->TextIconRaw(Onyx::BulletIcon, "A bullet!");
            ov->TextIcon(Onyx::ArrowRightIcon, "Here is the delta time again: {:.2f} ms", ftime);
            ov->PopTree();
        }

        if (ov->PushTree("Tooltips", drawLines))
        {
            static Onyx::OverlayTooltipFlags tflags = 0;
            ov->CheckBoxFlags("OverlayTooltipFlag_NoPromotion", &tflags, Onyx::OverlayTooltipFlag_NoPromotion);

            ov->Button("I am an instant tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
            if (ov->IsItemHovered())
                ov->SetTooltip(tflags, "I am instant!");

            ov->Button("I am a short-delayed tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
            if (ov->IsItemHovered(Onyx::OverlayHoveredFlag_ShortDelay))
                ov->SetTooltip(tflags, "I am a bit delayed!");

            ov->Button("I am a normal-delayed tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
            if (ov->IsItemHovered(Onyx::OverlayHoveredFlag_NormalDelay))
                ov->SetTooltip(tflags, "I am delayed!");

            ov->Button("I am a stationary tooltip", Onyx::OverlayButtonFlag_SpanFullWidth);
            if (ov->IsItemHovered(Onyx::OverlayHoveredFlag_Stationary | Onyx::OverlayHoveredFlag_NormalDelay))
                ov->SetTooltip(tflags, "I am stationary!");
            ov->PopTree();
        }

        if (ov->PushTree("Trees", drawLines))
        {
            static Onyx::OverlayTreeFlags tflags = 0;
            ov->CheckBoxFlags("OverlayTreeFlag_DrawLines", &tflags, drawLines);
            ov->CheckBoxFlags("OverlayTreeFlag_ToggleOnArrow", &tflags, Onyx::OverlayTreeFlag_ToggleOnArrow);
            ov->CheckBoxFlags("OverlayTreeFlag_OpenOnDoubleClick", &tflags, Onyx::OverlayTreeFlag_OpenOnDoubleClick);
            ov->CheckBoxFlags("OverlayTreeFlag_SpanLabelWidth", &tflags, Onyx::OverlayTreeFlag_SpanLabelWidth);
            ov->CheckBoxFlags("OverlayTreeFlag_Framed", &tflags, Onyx::OverlayTreeFlag_Framed);
            ov->CheckBoxFlags("OverlayTreeFlag_NoIndent", &tflags, Onyx::OverlayTreeFlag_NoIndent);

            if (ov->PushTree("Click me", tflags))
            {
                if (ov->PushTree("Simple example", tflags))
                {
                    ov->Button("Hello");
                    ov->PopTree();
                }
                if (ov->PushTree("I am open", tflags | Onyx::OverlayTreeFlag_StartOpen))
                {
                    ov->Text("You can see me");
                    ov->PopTree();
                }
                ov->PopTree();
            }
            ov->PopTree();
        }
        ov->EndDisabled();
        ov->EndWindow();
    }

    if (ov->BeginWindow("Window settings", &enableSettings, wflags))
    {
        editWindowFlags(&wflags);
        ov->EndWindow();
    }
    if (ov->BeginWindow("Renderer statistics", &enableRenderer, wflags))
    {
        ov->ShowRendererStatistics();
        ov->EndWindow();
    }
    if (ov->BeginWindow("Style editor", &enableStyleEditor, wflags))
    {
        ov->ShowStyleEditor();
        ov->EndWindow();
    }

    if (ov->BeginWindow("Overlay demo"))
    {
        if (ov->PushTree("Miscellaneous", drawLines))
        {
            ov->TextRaw(Onyx::TextMode_Wrapped, "Nothing to see here! This is extra content to demonstrate "
                                                "appending multiple times to a window after it has "
                                                "been closed");
            if (ov->BeginMenu("A rogue menu item"))
            {
                drawMenus();
                ov->EndMenu();
            }
            ov->PopTree();
        }
        ov->EndWindow();
    }
}

template <typename... Args> static TKit::StackString fmt(const fmt::format_string<Args...> str, Args &&...args)
{
    return TKit::StackString::Format(str, std::forward<Args>(args)...);
}

void Overlay::ShowRendererStatistics()
{
    TKIT_PROFILE_NSCOPE("Onyx::Overlay::RendererStatistics");
    BeginTabBar();
    if (BeginTab("2D", OverlayTabFlag_StartOpen))
    {
        Renderer::DisplayMemoryLayout<D2>(this);
        EndTab();
    }
    if (BeginTab("3D"))
    {
        Renderer::DisplayMemoryLayout<D3>(this);
        EndTab();
    }

    EndTabBar();
}

void Overlay::ShowStyleEditor()
{
    TKIT_PROFILE_NSCOPE("Onyx::Overlay::StyleEditor");
    Layout &ly = GetCurrentLayout();

    constexpr u32 bufSize = 128;
    static char colorFilter[bufSize] = {0};

    TextRaw("Welcome to the (pretty minimal) style editor! Right click any item to reset it to default");

    BeginTabBar();

    if (BeginTab("Colors", OverlayTabFlag_StartOpen))
    {
        const auto colorEditor = [&](const TKit::StringView str, const u32 col) {
            if (colorFilter[0] != 0 && !str.Contains(colorFilter))
                return;

            static TKit::Clock flashClock{};
            static OverlayColors backup;
            static u32 flashing = TKIT_U32_MAX;

            ly.BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight,
                                  .Alignment = TopLeft,
                                  .Sizing = {grow(), fit()},
                                  .ChildGap = m_Style[OverlayStyle_ChildGap]});
            PushId(str);
            if (Button("?"))
            {
                flashing = col;
                backup[col] = m_Style.Colors[col];
                flashClock.Restart();
            }
            if (BeginItemTooltip(OverlayHoveredFlag_ShortDelay))
            {
                TextRaw("Press this button to flash this color and easily identify where it is used");
                EndTooltip();
            }

            if (flashing == col)
            {
                const f32 elapsed = flashClock.GetElapsed().AsSeconds();
                if (elapsed < 1.f)
                {
                    const f32 t = 0.5f * (1.f - Math::Sine(10.f * elapsed));
                    m_Style.Colors[col] = Color{t, 4.f * t * (1.f - t), 1.f - t};
                }
                else if (flashing == col)
                {
                    m_Style.Colors[col] = backup[col];
                    flashing = TKIT_U32_MAX;
                }
            }

            ColorEditor(fmt("[{}] {}", col, str), &m_Style.Colors[col]);
            if (BeginPopupContextItem(IdFromStack(col), "Default",
                                      OverlayWindowFlag_AutoResize | OverlayWindowFlag_BringToTop |
                                          OverlayWindowFlag_NoHeaderBar))
            {
                if (Button("Reset to default"))
                {
                    m_Style.Colors[col] = m_DefaultStyle.Colors[col];
                    CloseCurrentPopup();
                }

                EndPopup();
            }
            PopId();
            ly.EndPanel();
        };

        InputText("Filter##Colors", colorFilter, bufSize);

        colorEditor("None", OverlayColor_None);
        colorEditor("Text", OverlayColor_Text);
        colorEditor("Line", OverlayColor_Line);
        colorEditor("DragOutline", OverlayColor_DragOutline);
        colorEditor("InputCursor", OverlayColor_InputCursor);
        colorEditor("InputHighlight", OverlayColor_InputHighlight);
        colorEditor("InputBackground", OverlayColor_InputBackground);
        colorEditor("WindowBorderIdle", OverlayColor_WindowBorderIdle);
        colorEditor("WindowBorderHovered", OverlayColor_WindowBorderHovered);
        colorEditor("WindowBorderPressed", OverlayColor_WindowBorderPressed);
        colorEditor("Header", OverlayColor_Header);
        colorEditor("ButtonIdle", OverlayColor_ButtonIdle);
        colorEditor("ButtonHovered", OverlayColor_ButtonHovered);
        colorEditor("ButtonPressed", OverlayColor_ButtonPressed);
        colorEditor("CheckBoxIdle", OverlayColor_CheckBoxIdle);
        colorEditor("CheckBoxHovered", OverlayColor_CheckBoxHovered);
        colorEditor("CheckBoxPressed", OverlayColor_CheckBoxPressed);
        colorEditor("CheckBoxInner", OverlayColor_CheckBoxInner);
        colorEditor("SliderIdle", OverlayColor_SliderIdle);
        colorEditor("SliderHovered", OverlayColor_SliderHovered);
        colorEditor("SliderPressed", OverlayColor_SliderPressed);
        colorEditor("SliderInner", OverlayColor_SliderInner);
        colorEditor("DragIdle", OverlayColor_DragIdle);
        colorEditor("DragHovered", OverlayColor_DragHovered);
        colorEditor("DragPressed", OverlayColor_DragPressed);
        colorEditor("TreeIdle", OverlayColor_TreeIdle);
        colorEditor("TreeHovered", OverlayColor_TreeHovered);
        colorEditor("TreePressed", OverlayColor_TreePressed);
        colorEditor("DropDownIdle", OverlayColor_DropDownIdle);
        colorEditor("DropDownHovered", OverlayColor_DropDownHovered);
        colorEditor("DropDownPressed", OverlayColor_DropDownPressed);
        colorEditor("DropDownButton", OverlayColor_DropDownButton);
        colorEditor("SelectableIdle", OverlayColor_SelectableIdle);
        colorEditor("SelectableHovered", OverlayColor_SelectableHovered);
        colorEditor("SelectablePressed", OverlayColor_SelectablePressed);
        colorEditor("MenuItemIdle", OverlayColor_MenuItemIdle);
        colorEditor("MenuItemHovered", OverlayColor_MenuItemHovered);
        colorEditor("MenuItemPressed", OverlayColor_MenuItemPressed);
        colorEditor("MenuBoxBackground", OverlayColor_MenuBoxBackground);
        colorEditor("ScrollBarIdle", OverlayColor_ScrollBarIdle);
        colorEditor("ScrollBarHovered", OverlayColor_ScrollBarHovered);
        colorEditor("ScrollBarPressed", OverlayColor_ScrollBarPressed);
        colorEditor("ScrollAreaBorders", OverlayColor_ScrollAreaBorders);
        colorEditor("ProgressBarBackground", OverlayColor_ProgressBarBackground);
        colorEditor("ProgressBarInner", OverlayColor_ProgressBarInner);
        colorEditor("PopupBackground", OverlayColor_PopupBackground);
        colorEditor("WindowBackgroundExpanded", OverlayColor_WindowBackgroundExpanded);
        colorEditor("WindowBackgroundCollapsed", OverlayColor_WindowBackgroundCollapsed);
        colorEditor("HeaderBackgroundExpanded", OverlayColor_HeaderBackgroundExpanded);
        colorEditor("HeaderBackgroundCollapsed", OverlayColor_HeaderBackgroundCollapsed);
        colorEditor("MenuBarBackground", OverlayColor_MenuBarBackground);
        EndTab();
    }

    if (BeginTab("Variables"))
    {
        static char varFilter[bufSize] = {0};
        const auto varSlider = [&](const TKit::StringView str, const u32 var, const f32 mn, const f32 mx) {
            if (varFilter[0] != 0 && !str.Contains(varFilter))
                return;

            PushId(str);
            HorizontalSlider(fmt("[{}] {}", var, str), &m_Style.Variables[var], mn, mx);
            if (BeginPopupContextItem(IdFromStack(var), "Default",
                                      OverlayWindowFlag_AutoResize | OverlayWindowFlag_BringToTop |
                                          OverlayWindowFlag_NoHeaderBar))
            {
                if (Button("Reset to default"))
                {
                    m_Style.Variables[var] = m_DefaultStyle.Variables[var];
                    CloseCurrentPopup();
                }

                EndPopup();
            }
            PopId();
        };

        InputText("Filter##Variables", varFilter, bufSize);

        varSlider("FontSize", OverlayStyle_FontSize, 1.f, 100.f);
        varSlider("ChildGap", OverlayStyle_ChildGap, 0.f, 50.f);
        varSlider("HeaderRadius", OverlayStyle_HeaderRadius, 0.f, 50.f);
        varSlider("MenuBarRadius", OverlayStyle_MenuBarRadius, 0.f, 50.f);
        varSlider("DropDownRadius", OverlayStyle_DropDownRadius, 0.f, 50.f);
        varSlider("DropDownPopupRadius", OverlayStyle_DropDownPopupRadius, 0.f, 50.f);
        varSlider("DragThreshold", OverlayStyle_DragThreshold, 0.f, 50.f);
        varSlider("DragOutlineWidth", OverlayStyle_DragOutlineWidth, 0.f, 1.f);
        varSlider("ScrollAreaBorderRadius", OverlayStyle_ScrollAreaBorderRadius, 0.f, 50.f);
        varSlider("TreeRadius", OverlayStyle_TreeRadius, 0.f, 50.f);
        varSlider("InputBoxRadius", OverlayStyle_InputBoxRadius, 0.f, 50.f);
        varSlider("ButtonRadius", OverlayStyle_ButtonRadius, 0.f, 50.f);
        varSlider("CheckBoxRadius", OverlayStyle_CheckBoxRadius, 0.f, 50.f);
        varSlider("SelectableRadius", OverlayStyle_SelectableRadius, 0.f, 50.f);
        varSlider("SelectableCheckBoxRadius", OverlayStyle_SelectableCheckBoxRadius, 0.f, 50.f);
        varSlider("TooltipRadius", OverlayStyle_TooltipRadius, 0.f, 50.f);
        varSlider("ImageRadius", OverlayStyle_ImageRadius, 0.f, 50.f);
        varSlider("TabRadius", OverlayStyle_TabRadius, 0.f, 50.f);
        varSlider("TabPadding", OverlayStyle_TabPadding, 0.f, 50.f);
        varSlider("TabGap", OverlayStyle_TabGap, 0.f, 50.f);
        varSlider("LineRadius", OverlayStyle_LineRadius, 0.f, 50.f);
        varSlider("LineWidth", OverlayStyle_LineWidth, 0.f, 50.f);
        varSlider("SeparatorTextOffset", OverlayStyle_SeparatorTextOffset, 0.f, 50.f);
        varSlider("SliderRadius", OverlayStyle_SliderRadius, 0.f, 50.f);
        varSlider("SliderInnerRadius", OverlayStyle_SliderInnerRadius, 0.f, 50.f);
        varSlider("VerticalSliderWidth", OverlayStyle_VerticalSliderWidth, 0.f, 50.f);
        varSlider("VerticalSliderHeight", OverlayStyle_VerticalSliderHeight, 0.f, 300.f);
        varSlider("Alpha", OverlayStyle_Alpha, 0.f, 1.f);
        varSlider("DisabledAlpha", OverlayStyle_DisabledAlpha, 0.f, 1.f);
        varSlider("ListBoxMaxHeight", OverlayStyle_ListBoxMaxHeight, 20.f, 500.f);
        varSlider("TooltipOffset", OverlayStyle_TooltipOffset, 0.f, 100.f);
        varSlider("TooltipPadding", OverlayStyle_TooltipPadding, 0.f, 50.f);
        varSlider("MainMenuBarPadding", OverlayStyle_MainMenuBarPadding, 0.f, 50.f);
        varSlider("MinimumMenuWidth", OverlayStyle_MinimumMenuWidth, 50.f, 500.f);
        varSlider("WindowPadding", OverlayStyle_WindowPadding, 0.f, 50.f);
        varSlider("WindowBorderWidth", OverlayStyle_WindowBorderWidth, 0.f, 20.f);
        varSlider("WindowSpawnDelta", OverlayStyle_WindowSpawnDelta, 0.f, 200.f);
        varSlider("HeaderPadding", OverlayStyle_HeaderPadding, 0.f, 50.f);
        varSlider("IconWidth", OverlayStyle_IconWidth, 0.f, 100.f);
        varSlider("BorderHoverPadding", OverlayStyle_BorderHoverPadding, 0.f, 50.f);
        varSlider("ContentAreaPadding", OverlayStyle_ContentAreaPadding, 0.f, 50.f);
        varSlider("ScrollBarWidth", OverlayStyle_ScrollBarWidth, 1.f, 50.f);
        varSlider("ScrollBarGap", OverlayStyle_ScrollBarGap, 0.f, 50.f);
        varSlider("ScrollSensitivity", OverlayStyle_ScrollSensitivity, 1.f, 200.f);
        varSlider("WidgetSize", OverlayStyle_WidgetSize, 1.f, 100.f);
        varSlider("WidgetPadding", OverlayStyle_WidgetPadding, 0.f, 50.f);
        varSlider("WidgetMinimumWidth", OverlayStyle_WidgetMinimumWidth, 50.f, 1000.f);
        varSlider("SmallButtonPadding", OverlayStyle_SmallButtonPadding, 0.f, 50.f);
        varSlider("MenuPadding", OverlayStyle_MenuPadding, 0.f, 50.f);
        varSlider("TreeLineWidth", OverlayStyle_TreeLineWidth, 0.f, 20.f);
        varSlider("ClickMilliseconds", OverlayStyle_ClickMilliseconds, 50.f, 1000.f);
        varSlider("CursorWidth", OverlayStyle_CursorWidth, 0.5f, 10.f);
        varSlider("HoverDelayShort", OverlayStyle_HoverDelayShort, 0.f, 2.f);
        varSlider("HoverDelayNormal", OverlayStyle_HoverDelayNormal, 0.f, 2.f);
        varSlider("HoverStationaryThreshold", OverlayStyle_HoverStationaryThreshold, 0.f, 50.f);
        varSlider("DropDownHeightSmall", OverlayStyle_DropDownHeightSmall, 20.f, 500.f);
        varSlider("DropDownHeightRegular", OverlayStyle_DropDownHeightRegular, 50.f, 1000.f);
        varSlider("HintOpacity", OverlayStyle_HintOpacity, 0.f, 1.f);
        varSlider("CursorOpacity", OverlayStyle_CursorOpacity, 0.f, 1.f);
        varSlider("ColorPreviewSize", OverlayStyle_ColorPreviewSize, 10.f, 300.f);
        varSlider("ColorTooltipSize", OverlayStyle_ColorTooltipSize, 10.f, 300.f);
        varSlider("ColorDragTooltipSize", OverlayStyle_ColorDragTooltipSize, 10.f, 300.f);
        varSlider("ColorPickerSize", OverlayStyle_ColorPickerSize, 50.f, 500.f);
        EndTab();
    }

    EndTabBar();
}
/////////////////////////////////////////////
/// END DEMO
/////////////////////////////////////////////
} // namespace Onyx
