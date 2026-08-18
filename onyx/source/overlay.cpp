// NOLINTBEGIN(performance-unnecessary-value-param)

#include "pch.hpp"
#include "onyx/overlay.hpp"
#include "onyx/onyx.hpp"
#include "onyx/renderer.hpp"
#include "onyx/platform.hpp"
#include "tkit/profiling/macros.hpp"
#ifdef TKIT_ENABLE_YAML_SERIALIZATION
#    include "tkit/serialization/yaml/tensor.hpp"
#    include "tkit/serialization/yaml/container.hpp"
#    include <filesystem>
#endif

TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4312)

namespace Onyx
{
/////////////////////////////////////////////
/// GLOBALS
/////////////////////////////////////////////

static constexpr f32 s_CheckboardLight = 0.5f;
static constexpr f32 s_CheckboardDark = 0.3f;
static constexpr usz s_BaseId = 0xA7C3E1D9B4F20856;

static constexpr i32 s_Horizontal = 0;
static constexpr i32 s_Vertical = 1;
static constexpr i32 s_CenterAxis = -1;

static constexpr i32 s_PositiveSide = 1;
static constexpr i32 s_NegativeSide = -1;
static constexpr i32 s_CenterSide = 0;

static constexpr u32 s_Axis = 0;
static constexpr u32 s_Side = 1;

static constexpr i32v2 s_Center = {s_CenterAxis, s_CenterSide};
static constexpr i32v2 s_Left = {s_Horizontal, s_NegativeSide};
static constexpr i32v2 s_Right = {s_Horizontal, s_PositiveSide};
static constexpr i32v2 s_Bottom = {s_Vertical, s_NegativeSide};
static constexpr i32v2 s_Top = {s_Vertical, s_PositiveSide};

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
    StateFlag_HoveredAllowsInteraction = 1U << 3,
    StateFlag_PressedAllowsInteraction = 1U << 4,
    StateFlag_ActiveAllowsInteraction = 1U << 5,

    StateFlag_MustCollapsePopups = 1U << 6,
    StateFlag_FocusBlockByPopupCollapse = 1U << 7,
    StateFlag_PopupProtectionForbidden = 1U << 8,
    StateFlag_MainMenuBarActive = 1U << 9,

    StateFlag_RequestCaptureMouse = 1U << 10,
    StateFlag_RequestCaptureKeyboard = 1U << 11,

    StateFlag_WantCaptureMouse = 1U << 12,
    StateFlag_WantCaptureKeyboard = 1U << 13,

    StateFlag_DragPayloadAccepted = 1U << 14,
    StateFlag_DragPayloadRejected = 1U << 15,
    StateFlag_ActivePromotedFloatElement = 1U << 16,

    // we include all flags except for the active allows interaction. that one is only cleared when active id is cleared
    StateFlagPersist = StateFlag_ActiveAllowsInteraction | StateFlag_PressedAllowsInteraction |
                       StateFlag_FocusBlockByPopupCollapse | StateFlag_WantCaptureMouse | StateFlag_WantCaptureKeyboard,
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
    vars[OverlayStyle_UnicodeSize] = 14.f;
    vars[OverlayStyle_IndentWidth] = 16.f;
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

    colors[OverlayColor_DockPreview] = Color{palette[OverlayPalette_Inner0], 0.6f};
    colors[OverlayColor_DockSpaceBackground] = palette[OverlayPalette_Idle2];

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

    ResizeFlag_DockHorizontal = 1U << 4,
    ResizeFlag_DockVertical = 1U << 5,

    ResizeFlag_WindowBorder = ResizeFlag_Left | ResizeFlag_Right | ResizeFlag_Bottom | ResizeFlag_Top,
    ResizeFlag_DockBorder = ResizeFlag_DockVertical | ResizeFlag_DockHorizontal,
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
    NativeWindowFlag_CursorWasSet = 1U << 10,

    NativeWindowFlagPersist = NativeWindowFlag_PressingLeftMouse | NativeWindowFlag_RepresentsFloatElement |
                              NativeWindowFlag_ActivePromotedFloatElement | NativeWindowFlag_CheckParentForGrab |
                              NativeWindowFlag_CursorWasSet,
};

enum WindowInternalFlagBit : OverlayWindowFlags
{
    WindowInternalFlag_Hovered = 1ULL << 0,
    WindowInternalFlag_Focused = 1ULL << 1,
    WindowInternalFlag_InputHovered = 1ULL << 2,
    WindowInternalFlag_MenuBarOpened = 1ULL << 3,
    WindowInternalFlag_Popup = 1ULL << 4,
    WindowInternalFlag_Active = 1ULL << 5,
    WindowInternalFlag_OwnsNative = 1ULL << 6,
    WindowInternalFlag_HeaderGrabbed = 1ULL << 7,
    WindowInternalFlag_IsDockTarget = 1ULL << 8,
    WindowInternalFlag_MultipleAppends = 1ULL << 9,
    WindowInternalFlag_MustUndock = 1ULL << 10,
    WindowInternalFlag_MustGrabWhenUndocked = 1ULL << 11,
    // needed specially with docking, so that inactive windows have a 0 size. need to query last frame's active because
    // the first thing drawn is always the tree structure
    WindowInternalFlag_ActiveLastFrame = 1ULL << 12,

    // when collapsed, let os window know it must adapt its size
    WindowInternalFlag_WantUpdateSize = 1ULL << 13,

    // this... is a one use flag needed because the layout system queries with one frame of delay. used when auto resize
    // is at play and we need to sync the derived size with the reported window size
    WindowInternalFlag_AllowForLayoutToCatchUp = 1ULL << 14,

    WindowInternalFlag_HasActiveChildren = 1ULL << 15,
    WindowInternalFlag_DockSpace = 1ULL << 16,
    WindowInternalFlag_DockSpaceSubmissionOrderMatters = 1ULL << 17,
    WindowInternalFlag_DockSpaceSubmitted = 1ULL << 18,

    // flags that persist when beginning a new window. essentially all private flags
    WindowInternalFlagPersist = ~(TKit::Limits<OverlayWindowFlags>::Max() << 19),
};

/////////////////////////////////////////////
/// END WINDOW FLAGS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// VALIDATION
/////////////////////////////////////////////

template <typename T, typename F> static bool iterateDockTree(T *parent, F &&func)
{
    constexpr bool hasRet = std::is_same_v<std::invoke_result_t<F, T *>, bool>;
    TKit::StaticArray64<T *> nodes{};

    nodes.Append(parent);
    while (!nodes.IsEmpty())
    {
        T *node = nodes.GetBack();
        nodes.Pop();

        if (!node->IsLeaf())
        {
            T *c0 = node->Children[0];
            T *c1 = node->Children[1];

            nodes.Append(c1);
            nodes.Append(c0);
        }
        if constexpr (hasRet)
        {
            if (!std::forward<F>(func)(node))
                return true;
        }
        else
            std::forward<F>(func)(node);
    }
    return false;
}

#ifdef TKIT_ENABLE_ENSURE
#    define ASSERT_WITH_WINDOW(win, cnd, msg)                                                                          \
        TKIT_ENSURE(cnd, "{} -- Offending window - Title: {} - Id: {}", msg,                                           \
                    (win)->Title.IsEmpty() ? "Unnamed" : (win)->Title, (win)->Id.Id);

// fyi this was generated
static void validateDockTree(const DockNode *root, const TKit::StaticArray64<DockNode *> &dockNodes,
                             const char *context)
{
    if (!root)
        return;

    const auto fptr = [](const auto p) { return TKit::FormatPointer(p); };
    const OverlayWindow *host = root->Host;
    TKIT_ENSURE(host, "[ONYX][OVERLAY][{}] Root node has no Host", context);
    TKIT_ENSURE(root->IsRoot(), "[ONYX][OVERLAY][{}] Root node {} has a parent {}", context, fptr(root),
                fptr(root->Parent));
    TKIT_ENSURE(host->DockRoot == root, "[ONYX][OVERLAY][{}] Host DockRoot {} != root {}", context,
                fptr(host->DockRoot), fptr(root));
    TKIT_ENSURE(!host->DockParent, "[ONYX][OVERLAY][{}] Host DockParent must be null, but is: {}", context,
                fptr(host->DockParent));

    OverlayWindow *nonRoot = nullptr;
    iterateDockTree(root, [&](const DockNode *node) {
        bool found = false;
        for (const auto &n : dockNodes)
            if (n == node)
            {
                found = true;
                break;
            }
        TKIT_ENSURE(found, "[ONYX][OVERLAY][{}] Node {} NOT in m_DockNodes", context, fptr(node));

        TKIT_ENSURE(node->Host == host, "[ONYX][OVERLAY][{}] Node {} has wrong Host (expected {}, got {})", context,
                    fptr(node), fptr(host), fptr(node->Host));

        if (node->IsLeaf())
        {
            for (const OverlayWindow *win : node->Windows)
            {
                TKIT_ENSURE(win->DockParent == node, "[ONYX][OVERLAY][{}] Window {} ({}) in leaf {} has DockParent {}",
                            context, win->Id.Id, win->Title, fptr(node), fptr(win->DockParent));
                TKIT_ENSURE(win->DockRoot == root,
                            "[ONYX][OVERLAY][{}] Window {} ({}) has DockRoot {}, expected root {}", context, win->Id.Id,
                            win->Title, fptr(win->DockRoot), fptr(root));
                TKIT_ENSURE(win->IsRoot() || win->Parent == host->Parent,
                            "[ONYX][OVERLAY][{}] Window {} ({}) has a parent that is different from the host's parent. "
                            "In a dock tree, only one docked window may not be root, and if it is the case, the host "
                            "must not be root as well and have the same parent as that specific window",
                            context, win->Id.Id, win->Title);
                if (!win->IsRoot())
                {
                    TKIT_ENSURE(!nonRoot,
                                "[ONYX][OVERLAY][{}] Both windows {} ({}) and {} ({}) have a parent. In a dock tree, "
                                "only one docked window may not be root, and if it is the case, the host "
                                "must not be root as well and have the same parent as that specific window",
                                context, fptr(nonRoot), nonRoot->Title, win->Id.Id, win->Title);
                }
            }
        }
        else
        {
            TKIT_ENSURE(node->Children[0] && node->Children[1], "[ONYX][OVERLAY][{}] Interior node {} missing children",
                        context, fptr(node));
            TKIT_ENSURE(node->Children[0]->Parent == node,
                        "[ONYX][OVERLAY][{}] Child[0] {} of node {} has wrong parent {}", context,
                        fptr(node->Children[0]), fptr(node), fptr(node->Children[0]->Parent));
            TKIT_ENSURE(node->Children[1]->Parent == node,
                        "[ONYX][OVERLAY][{}] Child[1] {} of node {} has wrong parent {}", context,
                        fptr(node->Children[1]), fptr(node), fptr(node->Children[1]->Parent));
            TKIT_ENSURE(node->Windows.IsEmpty(), "[ONYX][OVERLAY][{}] Interior node {} has {} windows", context,
                        fptr(node), node->Windows.GetSize());
        }
    });
    TKIT_ENSURE(
        !nonRoot || !(host->Flags & WindowInternalFlag_DockSpace),
        "[ONYX][OVERLAY] If a dock host is a manually submitted dockspace, it cannot contain any child windows");
}
static void validateWindowHierarchy(const TKit::StaticArray32<OverlayWindow *> &windows,
                                    const TKit::StaticArray32<Layout *> &layouts,
                                    const TKit::StaticArray64<DockNode *> &dockNodes, const char *context)
{
    const auto fptr = [](const auto p) { return TKit::FormatPointer(p); };

    for (const OverlayWindow *win : windows)
    {
        validateDockTree(win->DockRoot, dockNodes, context);

        if (win->Parent)
        {
            TKIT_ENSURE(!win->Layout, "[ONYX][OVERLAY][{}] Window {} has a Parent {} but also owns a Layout", context,
                        win->Id.Id, fptr(win->Parent));
        }
        else
        {
            TKIT_ENSURE(win->Layout, "[ONYX][OVERLAY][{}] Window {} has no Parent but Layout is null", context,
                        win->Id.Id);
        }

        if (win->Parent && win->DockRoot)
        {
            TKIT_ENSURE(win->Parent != win->DockRoot->Host, "[ONYX][OVERLAY][{}] Window {} has Parent == DockHost ({})",
                        context, win->Id.Id, fptr(win->Parent));
        }

        if (win->Layout)
        {
            bool found = false;
            for (const auto &ly : layouts)
                if (ly == win->Layout)
                {
                    found = true;
                    break;
                }
            TKIT_ENSURE(found, "[ONYX][OVERLAY][{}] Window {} ({}) has Layout {} not in m_Layouts", context, win->Id.Id,
                        win->Title, fptr(win->Layout));
        }
        if (win->IsDocked())
        {
            TKIT_ENSURE(win->DockRoot && win->DockRoot->Host != win,
                        "[ONYX][OVERLAY][{}] Window {} ({}) figures as docked, but is a host", context, win->Id.Id,
                        win->Title);
            bool found = false;
            for (OverlayWindow *child : win->DockParent->Windows)
                if (child == win)
                {
                    found = true;
                    break;
                }
            TKIT_ENSURE(found,
                        "[ONYX][OVERLAY][{}] Window {} ({}) figures as docked, but is not present in the list of "
                        "windows of its dock parent",
                        context, win->Id.Id, win->Title);
        }
    }
}
#    define VALIDATE_DOCK_TREE(root, context) validateDockTree(root, m_DockNodes, context)
#    define VALIDATE_WINDOW_HIERARCHY(context)                                                                         \
        validateWindowHierarchy(m_OverlayWindows, m_Layouts, m_DockNodes, context)
#else
#    define ASSERT_WITH_WINDOW(win, cnd, msg) TKIT_ASSERT(cnd, msg);
#    define VALIDATE_DOCK_TREE(root, context)
#    define VALIDATE_WINDOW_HIERARCHY(context)
#endif

// #define ENABLE_LOG_DOCK_TREE
#ifdef TKIT_ENABLE_DEBUG_LOGS
// fyi this was generated
static void debugDumpDockTree(const TKit::TierArray<DockNode *> &dockNodes, const OverlayWindow *win, const char *label)
{
    TKIT_LOG_INFO("[ONYX][OVERLAY] === Begin {} ===", label);
#    ifdef TKIT_ENABLE_ENSURE
    validateDockTree(win->DockRoot, dockNodes, label);
#    endif

    const auto fptr = [](const auto p) { return TKit::FormatPointer(p); };
    TKIT_LOG_DEBUG("[ONYX][OVERLAY] Window: {} (id: {:#018x})", win->Id.Id, win->Id.Id);
    TKIT_LOG_DEBUG("[ONYX][OVERLAY]   DockRoot: {} | DockParent: {} | Flags: {:#x}", fptr(win->DockRoot),
                   fptr(win->DockParent), win->Flags);
    TKIT_LOG_DEBUG("[ONYX][OVERLAY]   ScreenPos: ({}, {}) | Size: ({}, {})", win->ScreenPos[0], win->ScreenPos[1],
                   win->Size[0], win->Size[1]);
    TKIT_LOG_DEBUG("[ONYX][OVERLAY] Total nodes: {}", dockNodes.GetSize());
    TKIT_LOG_DEBUG_IF(win->IsDockHost(), "[ONYX][OVERLAY] This window figures as the dock host");

    for (u32 i = 0; i < dockNodes.GetSize(); ++i)
    {
        DockNode *n = dockNodes[i];
        const bool leafUnsafe = !n->Children[0] && !n->Children[1];
        TKIT_LOG_DEBUG("[ONYX][OVERLAY]   Node[{}]: {} | Parent: {} | Children: [{}, {}] | Axis: {} | Ratio: {:.3f} | "
                       "Host: {} | Windows: {} | IsLeaf(unsafe): {}",
                       i, fptr(n), fptr(n->Parent), fptr(n->Children[0]), fptr(n->Children[1]), u32(n->Axis), n->Ratio,
                       fptr(n->Host), n->Windows.GetSize(), leafUnsafe);
        for (u32 j = 0; j < n->Windows.GetSize(); ++j)
        {
            OverlayWindow *w = n->Windows[j];
            TKIT_LOG_DEBUG("[ONYX][OVERLAY]     Win[{}]: {} (id: {:#018x}) | DockRoot: {} | DockParent: {}", j, fptr(w),
                           w->Id.Id, fptr(w->DockRoot), fptr(w->DockParent));
        }
    }

    // Check if DockRoot/DockParent are actually in the node list
    if (win->DockRoot)
    {
        bool found = false;
        for (u32 i = 0; i < dockNodes.GetSize(); ++i)
            if (dockNodes[i] == win->DockRoot)
            {
                found = true;
                break;
            }
        TKIT_ENSURE(found, "[ONYX][OVERLAY]   DockRoot {} not in m_DockNodes!", fptr(win->DockRoot));
    }
    if (win->DockParent)
    {
        bool found = false;
        for (u32 i = 0; i < dockNodes.GetSize(); ++i)
            if (dockNodes[i] == win->DockParent)
            {
                found = true;
                break;
            }
        TKIT_ENSURE(found, "[ONYX][OVERLAY]   DockParent {} not in m_DockNodes!", fptr(win->DockParent));
    }

    TKIT_LOG_INFO("[ONYX][OVERLAY] === End {} ===", label);
}
#    define LOG_DOCK_TREE(win, label) debugDumpDockTree(m_DockNodes, win, label)
#else
#    define LOG_DOCK_TREE(win, label)
#endif

/////////////////////////////////////////////
/// END VALIDATION
/////////////////////////////////////////////

/////////////////////////////////////////////
/// INITIALIZATION
/////////////////////////////////////////////

Overlay::Overlay(Window *win, const OverlaySpecs &specs)
    : Flags(specs.Flags), m_LayoutSpecs(specs.Layout), m_Style(specs.Style), m_DefaultStyle(specs.Style)
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
        Flags |= OverlayFlag_WindowPromotions | OverlayFlag_FloatingMode;

    for (u32 i = 0; i < m_DynamicMeshes.GetSize(); ++i)
        m_DynamicMeshes[i] = Resources::RegisterDynamicMesh<D2>();

    Resources::Sync(SyncFlag_DynamicMeshes);
}

Overlay::~Overlay()
{
#ifdef TKIT_ENABLE_YAML_SERIALIZATION
    if (Flags & OverlayFlag_AutoSerialize)
        Serialize();
#endif
    for (OverlayWindow *win : m_OverlayWindows)
        destroyOverlayWindow(win, false);

    for (Layout *ly : m_Layouts)
        destroyLayout(ly);

    NativeWindow *mainNative = getMainNativeWindow();
    for (NativeWindow *nw : m_NativeWindows)
        if (!mainNative || nw != mainNative)
            destroyNativeWindow(nw);

    for (u32 i = 0; i < m_DynamicMeshes.GetSize(); ++i)
        if (Resources::IsResourceValid<D2>(m_DynamicMeshes[i].Handle, Resource_DynamicMesh))
            Resources::DestroyDynamicMesh<D2>(m_DynamicMeshes[i].Handle);
}

/////////////////////////////////////////////
/// END INITIALIZATION
/////////////////////////////////////////////

#ifdef TKIT_ENABLE_YAML_SERIALIZATION
static const OverlayDockNode *createTreeBasedOnSerialized(const TKit::Yaml::Node n)
{
    using Node = TKit::Yaml::Node;
    if (n["Leaf"].as<bool>())
    {
        TKit::StackArray<LayoutId> windows{};
        windows.Reserve(32);
        if (n["Windows"])
            for (const Node id : n["Windows"])
                windows.Append(id.as<usz>());

        return DockTabBar(windows, n["Flags"].as<u32>());
    }

    return DockSplit(LayoutAxis(n["Axis"].as<u32>()), n["Ratio"].as<f32>(),
                     createTreeBasedOnSerialized(n["Children"][0]), createTreeBasedOnSerialized(n["Children"][1]),
                     n["Flags"].as<u32>());
}
void Overlay::Serialize()
{
    if (m_SerializationPath.empty())
        return;

    using Node = TKit::Yaml::Node;

    Node root;
    if (fs::exists(m_SerializationPath))
    {
        root = TKit::Yaml::FromFile(m_SerializationPath);
        root["DockTrees"] = Node{};
    }

    Node windows = root["Windows"];
    Node dockTrees = root["DockTrees"];
    const bool isFloating = Flags & OverlayFlag_FloatingMode;
    for (OverlayWindow *win : m_OverlayWindows)
    {
        Node n = windows[win->Id.Id];
#    ifdef TKIT_ENABLE_ENSURE
        if (!win->Title.IsEmpty())
            n["Title"] = win->Title;
#    endif
        if (!win->IsRoot())
            n["Parent"] = win->Parent->Id.Id;
        else if (win->IsDocked())
            n["Position"] = win->DockParent->ReadOnlyPosition;
        else if (isFloating && win->Native)
            n["Position"] = win->Native->ScreenPos;
        else if (!isFloating && (win->Flags & WindowInternalFlag_OwnsNative))
            n["Position"] = win->Native->ScreenPos - win->Native->Parent->ScreenPos;
        else
            n["Position"] = win->ScreenPos;

        // these flags must persist
        n["Flags"] = win->Flags & (WindowInternalFlag_DockSpace | WindowInternalFlag_DockSpaceSubmissionOrderMatters |
                                   OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted);
        n["Size"] = win->IsDocked() ? win->DockParent->ReadOnlySize : win->Size;
        n["Layer"] = win->Layer;
        n["Docked"] = win->IsDocked();
        n["DockHost"] = win->IsDockHost();

        if (win->IsDockHost())
        {
            TKit::StackArray<Node> nodes{};
            nodes.Reserve(m_DockNodes.GetSize());
            nodes.Append(dockTrees[win->Id.Id]);
            iterateDockTree(win->DockRoot, [&](const DockNode *node) {
                Node child = nodes.GetBack();
                nodes.Pop();

                child["Flags"] = u32(node->Flags);
                const bool leaf = node->IsLeaf();
                child["Leaf"] = leaf;
                if (leaf)
                    for (OverlayWindow *cwin : node->Windows)
                        child["Windows"].push_back(cwin->Id.Id);
                else
                {
                    child["Ratio"] = node->Ratio;
                    child["Axis"] = u32(node->Axis);

                    Node children = child["Children"];
                    children.push_back(Node{});
                    children.push_back(Node{});

                    nodes.Append(children[1]);
                    nodes.Append(children[0]);
                }
            });
        }
    }

    TKit::Yaml::ToFile(m_SerializationPath, root);
}
void Overlay::Deserialize()
{
    if (m_SerializationPath.empty() || !fs::exists(m_SerializationPath))
        return;

    using Node = TKit::Yaml::Node;

    const Node root = TKit::Yaml::FromFile(m_SerializationPath);
    if (root["Windows"])
    {
        const Node windows = root["Windows"];
        u64 maxLayer = 0;
        for (auto it = windows.begin(); it != windows.end(); ++it)
        {
            const usz id = it->first.as<usz>();
            const Node nwin = it->second;
            const bool dockHost = nwin["DockHost"].as<bool>();
            if (dockHost && (!root["DockTrees"] || !root["DockTrees"][id]))
                continue;

            OverlayWindow *parent = nullptr;
            if (nwin["Parent"])
            {
                parent = findWindow(nwin["Parent"].as<usz>());
                TKIT_ASSERT(parent,
                            "[ONYX][OVERLAY] Serialization path claims window has a parent, but the parent has not "
                            "been deserialized");
            }
            OverlayWindow *win = dockHost ? getOrCreateDockHost(id, parent) : getOrCreateOverlayWindow(id, parent);
            if (!parent)
                win->ScreenPos = nwin["Position"].as<f32v2>();
            win->Size = nwin["Size"].as<f32v2>();
            win->Layer = nwin["Layer"].as<u64>();
            win->Flags = nwin["Flags"].as<OverlayWindowFlags>();

            assignNativeWindowSomehow(win, nwin["Docked"].as<bool>());
            maxLayer = Math::Max(maxLayer, win->Layer);
        }
        m_LayerCount = maxLayer + 1;
    }
    if (root["DockTrees"])
    {
        const Node dockTrees = root["DockTrees"];
        for (auto it = dockTrees.begin(); it != dockTrees.end(); ++it)
        {
            const usz hostId = it->first.as<usz>();
            const Node uroot = it->second;

            const OverlayDockNode *root = createTreeBasedOnSerialized(uroot);
            ApplyDockTree(hostId, root);
        }
    }
}
#endif

/////////////////////////////////////////////
/// WINDOWS/MENUS
/////////////////////////////////////////////

NativeWindow *OverlayWindow::GetNative() const
{
    return (Flags & WindowInternalFlag_OwnsNative) ? Native : GetRoot()->Native;
}
NativeWindow *OverlayWindow::GetNativeForGrab() const
{
    NativeWindow *nw = GetNative();
    const bool checkParent = nw->Flags & NativeWindowFlag_CheckParentForGrab;
    return checkParent ? nw->Parent : nw;
}

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
    const NativeWindow *nw = GetNative();
    const f32v2 dims = nw->GetDimensions();

    const f32v2 mnlim = MinSize - Size;
    const f32v2 mxlim = dims - MinSize;

    ScreenPos = Math::Clamp(ScreenPos, mnlim, mxlim);
}
void OverlayWindow::SyncNativeSize()
{
    if (!(Flags & WindowInternalFlag_OwnsNative))
        return;

    if (!Math::Approximately(Size, GetNative()->Size, 1.f))
        Flags |= WindowInternalFlag_WantUpdateSize;
}
bool OverlayWindow::IsFullscreenBlocked() const
{
    return (Flags & WindowInternalFlag_OwnsNative) && GetNative()->Window->IsFullScreen();
}
bool OverlayWindow::CanResize() const
{
    return !(Flags & (OverlayWindowFlag_NoResize | OverlayWindowFlag_AutoResize)) && !IsFullscreenBlocked();
}
// CanMove has additional checks because windows that do not own a layout do not get automatically the _NoMove flag, but
// they do receive the _NoResize one. this is because when undocking a window, we want to be able to drag away windows
// that do not own a layout at the moment ONLY if users did not set the _NoMove flag. so we have a distinction there.
// thats why undockWindow only checks for the flag
bool OverlayWindow::CanMove() const
{
    return !(Flags & OverlayWindowFlag_NoMove) && OwnsActiveLayout() && !IsFullscreenBlocked();
}
bool OverlayWindow::CanCollapse() const
{
    return !DockRoot && !(Flags & (OverlayWindowFlag_NoCollapse | OverlayWindowFlag_NoHeaderBar)) &&
           (IsRoot() || !(Flags & OverlayWindowFlag_ChildGrowHeight)) && !IsFullscreenBlocked();
}

f32v2 OverlayWindow::ToScreen(const f32v2 &world) const
{
    if (Flags & WindowInternalFlag_OwnsNative)
        return GetNative()->ToScreen(world);

    return ToLocalScreen(world);
}
f32v2 OverlayWindow::ToWorld(const f32v2 &screen) const
{
    if (Flags & WindowInternalFlag_OwnsNative)
        return GetNative()->ToWorld(screen);

    return ToLocalWorld(screen);
}

bool Overlay::BeginWindow(const OverlayLabel label, bool *opened, const OverlayWindowFlags flags)
{
    if (opened && !(*opened))
        return false;

    const LayoutId stackedId = PushId(label.Id);
    const bool merge = flags & OverlayWindowFlag_MergeIdWithStack;
    const bool popup = flags & WindowInternalFlag_Popup;
    OverlayWindow *active = getOrCreateOverlayWindow(merge ? stackedId : label.Id, popup ? nullptr : m_Active);

    ASSERT_WITH_WINDOW(
        active, active->IsRoot() || (active->Flags & WindowInternalFlag_Active) || (active->Parent == m_Active),
        "[ONYX][OVERLAY] A child window cannot be called for the first time outside of its parent's Begin/End region");

    m_WindowStack.Append(active);
    m_Active = active;

    if (beginWindow(active, opened, flags, label.Title))
        return true;

    popWindowStack();
    PopId();
    return false;
}

void Overlay::EndWindow()
{
    TKIT_ASSERT(m_Active, "[ONYX][OVERLAY] Cannot end a window without having started one");

    Layout *ly = m_Active->GetActiveLayout();
    if (m_Active->Flags & WindowInternalFlag_MultipleAppends)
    {
        ly->EndPanel();
        popWindowStack();
        PopId();
        return;
    }

    if (m_Active->DockRoot)
    {
        OverlayWindow *host = m_Active->DockRoot->Host;

        endScroll();
        endTab(&m_Active->DockParent->TabData);

        if (!(host->Flags & WindowInternalFlag_MultipleAppends))
            ly->EndPanel();
    }
    else
        endScroll();

    ly->EndPanel();
    popWindowStack();
    PopId();
}

bool Overlay::BeginMenuBar()
{
    if (!(m_Active->Flags & OverlayWindowFlag_MenuBar))
        return false;

    PushId(m_Active->MenuBarId);
    m_Active->GetActiveLayout()->OpenPanel(m_Active->MenuBarId);
    m_Active->Flags |= WindowInternalFlag_MenuBarOpened;
    return true;
}
void Overlay::EndMenuBar()
{
    m_Active->Flags &= ~WindowInternalFlag_MenuBarOpened;
    m_Active->GetActiveLayout()->EndPanel();
    PopId();
}

bool Overlay::BeginMainMenuBar()
{
    if (Flags & OverlayFlag_FloatingMode)
        return false;

    const NativeWindow *nw = GetMainNativeWindow();
    const f32v2 tl = nw->GetWorldTopLeft();
    const f32v2 tr = nw->GetWorldTopRight();
    const f32 xsize = tr[0] - tl[0];

    const LayoutId id = "__onyx_id_Main_menu_bar";
    m_Active = getOrCreateOverlayWindow(id);
    m_Active->Flags |= OverlayWindowFlag_NoPromotion | OverlayWindowFlag_NoDocking;

    m_StateFlags |= StateFlag_MainMenuBarActive;
    m_WindowStack.Append(m_Active);

    Layout *ly = m_Active->GetActiveLayout();
    const LayoutId contentId = PushId("__onyx_id_Main_menu_content_area");
    if (m_Active->Flags & WindowInternalFlag_Active)
    {
        m_Active->Flags |= WindowInternalFlag_MultipleAppends;
        ly->OpenPanel(m_Active->ContentAreaId);
        return true;
    }

    addActiveWindow(m_Active);

    ly->BeginPanel(id, LyPnPar{.FillColor = m_Style[OverlayColor_WindowBackgroundExpanded],
                               .Alignment = TopLeft,
                               .Sizing = {sabs(xsize), fit()},
                               .SelfOffset = oabs(tl),
                               .Padding = m_Style[OverlayStyle_MainMenuBarPadding]});

    const LayoutElementQueryInfo *elm = ly->QueryElement(id);
    m_Active->Size = elm ? elm->Size : f32v2{xsize, getWindowMinSize()};
    if (m_MainDockSpace)
        m_MainDockSpaceOffset = m_Active->Size[1];

    m_Active->ContentAreaId = ly->BeginPanel(contentId, LyPnPar{.FillColor = m_Style[OverlayColor_MenuBarBackground],
                                                                .Direction = LayoutDirection_LeftToRight,
                                                                .Alignment = CenterLeft,
                                                                .Sizing = {grow(), fit()},
                                                                .Shape = rect(m_Style[OverlayStyle_MenuBarRadius])});
    return true;
}

void Overlay::EndMainMenuBar()
{
    TKIT_ASSERT(m_Active->OwnsActiveLayout(), "[ONYX][OVERLAY] The main menu bar window must own the active layout");

    m_StateFlags &= ~StateFlag_MainMenuBarActive;
    m_Active->Layout->EndPanel();
    if (!(m_Active->Flags & WindowInternalFlag_MultipleAppends))
        m_Active->Layout->EndPanel();
    PopId();
    popWindowStack();
}

bool Overlay::BeginMenu(const OverlayLabel label)
{
    Layout *ly = m_Active->GetActiveLayout();
    const LayoutId id = PushId(label.Id);

    const LayoutElementQueryInfo *elm = ly->QueryElement(id);

    const bool mmnActive = m_StateFlags & StateFlag_MainMenuBarActive;
    const bool verticalLayout =
        (mmnActive && m_CurrentPopupDepth != 0) ||
        (!mmnActive && (!(m_Active->Flags & WindowInternalFlag_MenuBarOpened) || m_CurrentPopupDepth != 0));

    OverlayColor col = verticalLayout ? OverlayColor_None : OverlayColor_MenuItemIdle;

    const bool openOnHover = verticalLayout || checkWidgetState(m_Active->MenuBarId, WidgetStateFlag_Opened);

    const FocusFlags fflags = openOnHover ? (FocusFlag_HoverOpensPopup | FocusFlag_HoverRequestsPopupCollapse)
                                          : FocusFlag_LeftClickOpensPopup;

    const f32v2 hoverPad = 12.f;
    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, fflags, verticalLayout ? hoverPad : 0.f);

    const bool popupOpen = focusFlags & OverlayFocusQueryFlag_PopupOpen;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_MenuItemPressed;
    else if (popupOpen || (focusFlags & OverlayFocusQueryFlag_Hovered))
        col = OverlayColor_MenuItemHovered;

    const f32 padding = m_Style[OverlayStyle_MenuPadding];

    const LySz2 sizing = {verticalLayout ? flex() : fit(), verticalLayout ? fit() : flex()};
    ly->BeginPanel(id,
                   LyPnPar{.FillColor = m_Style[col], .Alignment = CenterLeft, .Sizing = sizing, .Padding = padding});

    ly->Text(ly->GenerateNextId(), label.Title, getTextParams());
    if (verticalLayout)
    {
        ly->Panel(LyPnPar{.Sizing = grow()});
        ly->Unicode(NullLayoutId, ArrowRightIcon, getUnicodeParams());
    }

    if (popupOpen)
    {
        if (!verticalLayout)
            m_WidgetStates[m_Active->MenuBarId] = WidgetStateFlag_Opened;

        const LayoutId bid = IdFromStack("__onyx_id_Menu_box");
        const LayoutElementQueryInfo *belm = ly->QueryElement(bid);
        const f32v2 csize = belm ? belm->Size : f32v2{0.f};

        const f32v2 &ppos = elm->Position;
        const f32v2 &psize = elm->Size;

        LyAlg2 att;
        LyAlg2 alg;
        f32v2 offset;

        const f32v4 borders = getWorldEffectiveBorders();
        const f32 lborder = borders[0];
        const f32 rborder = borders[2];
        const f32 bborder = borders[3];

        if (verticalLayout)
        {
            const bool surpasses = (ppos[0] + psize[0] + csize[0]) > rborder;
            att = LyAlg2{surpasses ? Alignment_Left : Alignment_Right, Alignment_Top};
            alg = surpasses ? TopRight : TopLeft;

            const f32 spill = ppos[1] - csize[1] + psize[1];
            const f32 yoffset = spill < bborder ? (bborder - spill) : 0.f;

            offset = {0.f, yoffset};
        }
        else
        {
            const bool surpasses = (ppos[1] - csize[1]) < bborder;
            att = LyAlg2{Alignment_Left, surpasses ? Alignment_Top : Alignment_Bottom};
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

        ly->BeginPanel(bid, LyPnPar{.FillColor = m_Style[OverlayColor_MenuBoxBackground],
                                    .Direction = LayoutDirection_TopToBottom,
                                    .Alignment = TopLeft,
                                    .Sizing = {fit(m_Style[OverlayStyle_MinimumMenuWidth]), fit()},
                                    .SelfOffset = oabs(offset),
                                    .Floating = {.Enable = true, .Attachment = att, .Alignment = alg},
                                    .Padding = padding});

        PushStyleVar(OverlayStyle_WidgetPadding, padding);
        ++m_CurrentPopupDepth;

        // we set a focus status to the background so that it can register inputs and collapse/persist popups
        queryAndSetFocusStatus(ly->QueryElement(bid), FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetActiveId,
                               hoverPad);
        return true;
    }
    ly->EndPanel();
    PopId();
    return false;
}
void Overlay::EndMenu()
{
    PopStyleVar();
    PopId();
    --m_CurrentPopupDepth;
    Layout *ly = m_Active->GetActiveLayout();
    ly->EndPanel();
    ly->EndPanel();
}

bool Overlay::IsCurrentWindowPromoted() const
{
    return m_Active && (m_Active->GetRoot()->Flags & WindowInternalFlag_OwnsNative);
}

bool Overlay::beginWindow(OverlayWindow *active, bool *opened, const OverlayWindowFlags flags,
                          const TKit::StringView title, const bool redirectedByHostedWindow)
{
    VALIDATE_WINDOW_HIERARCHY("BeginWindow()");
#ifdef TKIT_ENABLE_ENSURE
    active->Title = {title.GetData(), title.GetSize()};
#endif

    Layout *ly = active->GetActiveLayout();
    OverlayWindow *parent = active->Parent;

    const bool collapsed = active->IsCollapsed();
    if (active->Flags & WindowInternalFlag_Active)
    {
        active->Flags |= WindowInternalFlag_MultipleAppends;
        if (!collapsed)
        {
            ly->OpenPanel(active->ContentAreaId);
            return true;
        }
        return false;
    }
    const bool isDocked = active->IsDocked();
    OverlayWindow *dockHost = isDocked ? active->DockRoot->Host : nullptr;

    // we need to detect if we need to bail because of our dock host not being active AND being unable to be started by
    // us. how can this happen?
    // - first of course we need to be docked
    // - then, the host must not be a root. if it is a root, its layout is independent and it is perfectly fine for us
    // to submit it. if this is a manual dockspace, it means the user stopped submitting it and this is the first frame
    // it started not being submitted. so for a single frame we will submit it, and then likely be undocked if the
    // OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted is set
    // - if the host is not a root (i.e. a child window embedded in another window), we cannot submit it ourselves _with
    // one exception_. ignoring that exception for now, we have no idea where that child window is wrt its parent layout
    // - the exception is if we are not roots ourselves. because of the invariant: only one child window is allowed per
    // dock tree and if so, the dock host must have that parent exactly, we are definitely able to submit the dockhost!
    if (isDocked && !dockHost->IsRoot() && !(dockHost->Flags & WindowInternalFlag_Active) && active->IsRoot())
        return false;

    active->SubmissionOrder = m_SubmissionOrder++;
    active->PopupDepth = m_CurrentPopupDepth;
    ASSERT_WITH_WINDOW(active, active->IsDockHost() || bool(active->DockRoot) == bool(active->DockParent),
                       "[ONYX][OVERLAY] If a window is not a dock host and has a dock root, it "
                       "must have a dock parent, and vice versa");

    const bool wasAutoresize = active->Flags & OverlayWindowFlag_AutoResize;

    if (redirectedByHostedWindow)
        active->Flags &= ~OverlayWindowFlag_BringToTop;
    else
        active->Flags &= WindowInternalFlagPersist;

    active->Flags |= flags;
    if (active->Flags & OverlayWindowFlag_NoBorders)
        active->Flags |= OverlayWindowFlag_NoResize;

    if (parent)
    {
        parent->Flags |= WindowInternalFlag_HasActiveChildren;
        active->Flags |= OverlayWindowFlag_NoUndocking;
    }

    const bool activeOwnsLayout = active->OwnsActiveLayout();
    if (isDocked)
    {
        // auto resize is not supported with docking or in fullscreen
        active->Flags &= ~(OverlayWindowFlag_AutoResize | OverlayWindowFlag_MoveWithHeader);
        active->Flags |= OverlayWindowFlag_NoBorders | OverlayWindowFlag_NoResize;
    }
    if (!activeOwnsLayout)
    {
        active->Flags |= OverlayWindowFlag_NoPromotion | OverlayWindowFlag_NoBringToFocus;
        active->Flags &= ~OverlayWindowFlag_Modal;
    }
    else
    {
        if (active->IsFullscreenBlocked())
            active->Flags &= ~OverlayWindowFlag_AutoResize;
        if (m_FrameCount != 0 && !(active->Flags & WindowInternalFlag_ActiveLastFrame) &&
            !(active->Flags & OverlayWindowFlag_NoBringToFocus))
            active->Flags |= OverlayWindowFlag_BringToTop;
    }

    const bool ownsNative = active->Flags & WindowInternalFlag_OwnsNative;
    if (activeOwnsLayout && (m_NextWindow.Flags & NextWindowFlag_Position))
    {
        if (ownsNative)
        {
            active->SetActivePosition(m_NextWindow.ScreenPos);
            active->Native->Window->SetPosition(i32v2{m_NextWindow.ScreenPos});
        }
        else
            active->ScreenPos = m_NextWindow.ScreenPos;
    }

    if (m_NextWindow.Flags & NextWindowFlag_Size)
    {
        active->Size = m_NextWindow.Size;
        if (ownsNative)
            active->SyncNativeSize();
    }
    m_NextWindow.Flags = 0;

    assignNativeWindowSomehow(active);

    const auto openMenuBar = [&] {
        const usz menuId = TKit::Hash(active->Id, s_BaseId);
        active->MenuBarId = ly->BeginPanel(menuId, LyPnPar{.FillColor = m_Style[OverlayColor_MenuBarBackground],
                                                           .Direction = LayoutDirection_LeftToRight,
                                                           .Alignment = CenterLeft,
                                                           .Sizing = {grow(), fit()},
                                                           .Shape = rect(m_Style[OverlayStyle_MenuBarRadius])});
        // To be opened by menu bar calls
        ly->EndPanel();
    };

    const f32 cpadding = m_Style[OverlayStyle_ContentAreaPadding];
    const f32 cgap = m_Style[OverlayStyle_ChildGap];

    const auto setupWindowContentArea = [&] {
        if (active->Flags & OverlayWindowFlag_MenuBar)
            openMenuBar();

        // LySz2 scrollSizing;
        // if (forDocking)
        //     scrollSizing = autoResize ? flex() : grow();
        // else
        //     scrollSizing = {autoResize ? flex() : grow(), fit()};

        const bool autoResize = active->Flags & OverlayWindowFlag_AutoResize;
        const LayoutId scrollId = IdFromStack(active->Id);
        const LySz2 scrollSizing = autoResize ? flex() : grow();
        OverlayScrollFlags sflags = OverlayScrollFlag_NoBackground;

        if (active->Flags & OverlayWindowFlag_NoScrollBar)
            sflags |= OverlayScrollFlag_NoScrollBar;
        if (active->Flags & OverlayWindowFlag_NoVerticalScroll)
            sflags |= OverlayScrollFlag_NoVerticalScroll;
        if (active->Flags & OverlayWindowFlag_HorizontalScroll)
            sflags |= OverlayScrollFlag_HorizontalScroll;

        return beginScroll({.Id = scrollId,
                            .OuterSizing = scrollSizing,
                            .ContentSizing = scrollSizing,
                            .ContentPadding = cpadding,
                            .ChildGap = cgap,
                            .Flags = sflags});
    };

    const bool closeButton = !(active->Flags & OverlayWindowFlag_NoCloseButton);
    const auto beginDockedWindow = [&] {
        ly->OpenPanel(active->DockParent->ContentId);
        active->Size = active->DockParent->ReadOnlySize;
        ASSERT_WITH_WINDOW(active, !title.IsEmpty(),
                           "[ONYX][OVERLAY] A title must be provided if the window has a tab");

        if (beginTab(&active->DockParent->TabData, {active->Id, title}, closeButton ? opened : nullptr,
                     OverlayTabFlag_StartOpen | OverlayTabFlag_NoPushId | TabFlag_ForDocking, active))
        {
            addActiveWindow(active);
            active->ContentAreaId = setupWindowContentArea();
            return true;
        }

        ly->EndPanel();
        return false;
    };

    if (isDocked && (dockHost->Flags & WindowInternalFlag_Active))
    {
        if (beginDockedWindow())
        {
            // we use this flag as a signal in EndWindow to successfully account for layout differences
            dockHost->Flags |= WindowInternalFlag_MultipleAppends;
            return true;
        }
        return false;
    }

    if (!isDocked)
    {
        const bool isRoot = active->IsRoot();
        if (active->Flags & OverlayWindowFlag_BringToTop)
            active->SetLayer(toTop());

        if (active->Flags & OverlayWindowFlag_Modal)
            m_ModalCollapseDepth = Math::Max(m_ModalCollapseDepth, m_CurrentPopupDepth);

        addActiveWindow(active);

        const f32 wpadding = m_Style[OverlayStyle_WindowPadding];
        const f32 hpadding = m_Style[OverlayStyle_HeaderPadding];

        const bool autoResize = active->Flags & OverlayWindowFlag_AutoResize;
        if (autoResize && !wasAutoresize)
            active->SizeBeforeAutoResize = active->Size;
        else if (!autoResize && wasAutoresize)
            active->Size = active->SizeBeforeAutoResize;

        const LayoutElementQueryInfo *elm = ly->QueryElement(active->Id);
        if (elm && isRoot)
        {
            if (autoResize)
            {
                if (!(active->Flags & WindowInternalFlag_AllowForLayoutToCatchUp))
                    active->Size = elm->Size;
                else
                    active->Flags &= ~WindowInternalFlag_AllowForLayoutToCatchUp;

                active->SyncNativeSize();
            }
            else if (wasAutoresize)
                active->SyncNativeSize();
        }

        LySz2 sizing;
        if (autoResize)
            sizing = {fit(), collapsed ? sabs(active->Size[1]) : fit()};
        else if (!isRoot && (active->Flags & OverlayWindowFlag_ChildGrow))
        {
            const bool gw = active->Flags & OverlayWindowFlag_ChildGrowWidth;
            const bool gh = active->Flags & OverlayWindowFlag_ChildGrowHeight;
            sizing = {gw ? grow() : sabs(active->Size[0]), gh ? grow(active->MinSize[1]) : sabs(active->Size[1])};
            if (elm && gw)
                active->Size[0] = elm->Size[0];
            if (elm && gh)
                active->Size[1] = elm->Size[1];
        }
        else
            sizing = sabs(active->Size);

        NativeWindow *nw = active->GetNative();
        f32v2 pos;
        if (isRoot)
            pos = ownsNative ? nw->GetWorldTopLeft() : active->ToWorld(active->ScreenPos);
        else
        {
            pos = f32v2{0.f};
            active->ScreenPos = elm ? active->ToLocalScreen(elm->Position + f32v2{0.f, elm->Size[1]}) : f32v2{0.f};
        }
        OverlayColor color = OverlayColor_None;
        if (!(active->Flags & OverlayWindowFlag_NoBackground))
        {
            if (active->Flags & WindowInternalFlag_DockSpace)
            {
                ASSERT_WITH_WINDOW(active, active->IsDockHost(),
                                   "[ONYX][OVERLAY] A window cannot have the dockspace flag without being a dock host");
                color = OverlayColor_DockSpaceBackground;
            }
            else
                color = collapsed ? OverlayColor_WindowBackgroundCollapsed : OverlayColor_WindowBackgroundExpanded;
        }

        ly->BeginPanel(active->Id, LyPnPar{.FillColor = m_Style[color],
                                           .Direction = LayoutDirection_TopToBottom,
                                           .Alignment = TopLeft,
                                           .Sizing = sizing,
                                           .SelfOffset = oabs(pos),
                                           .ChildOverflow = isRoot ? LayoutOverflow_Clip : LayoutOverflow_Spill,
                                           // .SelfOverflow = isRoot ? LayoutOverflow_None : LayoutOverflow_Spill,
                                           .Padding = active->IsDockHost() ? 0.f : wpadding,
                                           .ChildGap = cgap});

        if (isRoot)
            queryAndSetFocusStatus(elm, FocusFlag_DoNotSetHoveredId | FocusFlag_DoNotSetPressedId |
                                            FocusFlag_DoNotSetActiveId | FocusFlag_DoNotSetDraggedId);

        if (!active->IsDockHost() && !(active->Flags & OverlayWindowFlag_NoBorders))
            drawWindowBorders(active);

        if (!(active->Flags & OverlayWindowFlag_NoHeaderBar))
        {
            active->HeaderId =
                ly->BeginPanel(IdFromStack("__onyx_id_Header_bar"),
                               LyPnPar{.FillColor = m_Style[collapsed ? OverlayColor_HeaderBackgroundCollapsed
                                                                      : OverlayColor_HeaderBackgroundExpanded],
                                       .Alignment = CenterLeft,
                                       .Sizing = {flex(), fit()},
                                       .Shape = rect(m_Style[OverlayStyle_HeaderRadius])});

            ly->BeginPanel(LyPnPar{.Alignment = CenterLeft,
                                   .Sizing = {autoResize ? flex() : grow(), fit()},
                                   .Padding = hpadding,
                                   .ChildGap = cgap});

            if (active->CanCollapse() && iconButton(IdFromStack("__onyx_id_Collapse_button"), active->HeaderIcon))
            {
                active->Flags |= WindowInternalFlag_AllowForLayoutToCatchUp;
                if (collapsed)
                    active->Size[1] = active->LastHeight;
                else
                {
                    active->LastHeight = active->Size[1];
                    active->Size[1] = active->MinSize[1];
                }
                if (active->OwnsActiveLayout())
                    active->SyncNativeSize();
            }

            ASSERT_WITH_WINDOW(active, !title.IsEmpty(),
                               "[ONYX][OVERLAY] A title must be provided if the window has a header");
            ly->Text(ly->GenerateNextId(), title, getTextParams());
            ly->EndPanel();

            if (closeButton)
            {
                const LayoutId bid = IdFromStack("__onyx_id_Close_button");
                const bool shouldClose = ownsNative && nw->Window->ShouldClose();
                if (opened && (iconButton(bid, CrossIcon) || shouldClose))
                    *opened = false;
                else if ((active->Flags & WindowInternalFlag_Popup) && (iconButton(bid, CrossIcon) || shouldClose))
                    CloseCurrentPopup();
            }

            ly->EndPanel();
        }

        m_LastItem = active->Id;
        if (collapsed)
        {
            ly->EndPanel();
            return false;
        }

        if (!active->IsDockHost())
            active->ContentAreaId = setupWindowContentArea();
        return true;
    }

    // if we have reached this moment, it means active is definitely docked, dock host is not null, and dock host needs
    // to be opened as well
    ASSERT_WITH_WINDOW(
        active, isDocked,
        "[ONYX][OVERLAY] The only way root window is not active but child is, is by the child window being docked");

    beginDockHost(dockHost, 0, true);
    if (beginDockedWindow())
        return true;

    // this is the missing opened panel from the host's beginWindow. that call will always return true, but we may still
    // return false because the tab is not opened. and when returning false, no EndWindow will be called, so we have to
    // manually close this panel that would otherwise be closed automatically if beginWindow returned false
    ly->EndPanel();
    return false;
}

OverlayWindow *Overlay::findWindow(const LayoutId id)
{
    for (OverlayWindow *win : m_OverlayWindows)
        if (win->Id == id)
            return win;

    return nullptr;
}

OverlayWindow *Overlay::createOverlayWindow(const LayoutId id, OverlayWindow *parent)
{
    OverlayWindow *win = createOverlayWindow();
    win->Id = id;
    win->HeaderIcon = ArrowDownIcon;
    win->Parent = parent;
    if (parent)
    {
        win->Native = parent->GetNative();
        win->Parent = parent;
        win->Flags |= WindowInternalFlag_DockSpaceSubmissionOrderMatters;
    }
    else
    {
        win->Layout = createLayout();
        win->Native = getMainNativeWindow(); // which may be null
    }

    return win;
}

OverlayWindow *Overlay::getOrCreateOverlayWindow(const LayoutId id, OverlayWindow *parent)
{
    OverlayWindow *win = findWindow(id);
    if (win)
        return win;
    return createOverlayWindow(id, parent);
}

void Overlay::assignNativeWindowSomehow(OverlayWindow *win, const bool avoidPromotion)
{
    if (win->Native)
        return;

    win->Native = win->GetRoot()->Native;
    if (win->Native)
        return;

    win->Native = getMainNativeWindow();
    if (win->Native || avoidPromotion)
        return;

    win->Native = promoteWindow(win, win->ScreenPos, win->Size);
}

void Overlay::addActiveWindow(OverlayWindow *win)
{
    for (u32 i = 0; i < m_ActiveWindows.GetSize(); ++i)
        if (win->GetLayer() < m_ActiveWindows[i]->GetLayer())
        {
            m_ActiveWindows.Insert(m_ActiveWindows.begin() + i, win);
            win->Flags |= WindowInternalFlag_Active;
            return;
        }
    m_ActiveWindows.Append(win);
    win->Flags |= WindowInternalFlag_Active;
    return;
}

void Overlay::drawWindowBorders(OverlayWindow *win)
{
    Layout *ly = win->GetActiveLayout();
    const OverlayColor interaction = win->Grab.InteractionColor;
    const OverlayColor idle = OverlayColor_WindowBorderIdle;

    GrabInfo &ginfo = win->Grab;

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

    const LayoutFloatingParameters fparams = {.Enable = true, .DrawOnTop = false, .Clip = true};

    // user data is only used in this case for floating panel promotions not to trigger. we dont want borders promoting
    // to their own windows just because they are floats

    const bool isRoot = win->IsRoot();
    const bool gw = !isRoot && (win->Flags & OverlayWindowFlag_ChildGrowWidth);
    const bool gh = !isRoot && (win->Flags & OverlayWindowFlag_ChildGrowHeight);
    const auto drawLeftBorder = [&] {
        ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight,
                               .Sizing = snorm(1.f),
                               .Floating = fparams,
                               .ChildOverflow = LayoutOverflow_Spill,
                               .SelfOverflow = LayoutOverflow_Spill});

        const LayoutId id = ly->Panel(IdFromStack("__onyx_id_Left"),
                                      LyPnPar{.FillColor = m_Style[l ? interaction : idle], .Sizing = hsizing});
        ginfo.Ids[left] = isRoot ? id : LayoutId{NullLayoutId};
        ly->EndPanel();
    };
    const auto drawRightBorder = [&] {
        ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight,
                               .Sizing = snorm(1.f),
                               .Floating = fparams,
                               .ChildOverflow = LayoutOverflow_Spill,
                               .SelfOverflow = LayoutOverflow_Spill});
        ly->Panel(LyPnPar{.Sizing = grow()});
        const LayoutId id = ly->Panel(IdFromStack("__onyx_id_Right"),
                                      LyPnPar{.FillColor = m_Style[r ? interaction : idle], .Sizing = hsizing});
        ginfo.Ids[right] = gw ? LayoutId{NullLayoutId} : id;
        ly->EndPanel();
    };
    const auto drawBottomBorder = [&] {
        ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_BottomToTop,
                               .Sizing = snorm(1.f),
                               .Floating = fparams,
                               .ChildOverflow = LayoutOverflow_Spill,
                               .SelfOverflow = LayoutOverflow_Spill});

        const LayoutId id = ly->Panel(IdFromStack("__onyx_id_Bottom"),
                                      LyPnPar{.FillColor = m_Style[b ? interaction : idle], .Sizing = vsizing});
        ginfo.Ids[bottom] = gh ? LayoutId{NullLayoutId} : id;
        ly->EndPanel();
    };
    const auto drawTopBorder = [&] {
        ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_BottomToTop,
                               .Sizing = snorm(1.f),
                               .Floating = fparams,
                               .ChildOverflow = LayoutOverflow_Spill,
                               .SelfOverflow = LayoutOverflow_Spill});
        ly->Panel(LyPnPar{.Sizing = grow()});
        const LayoutId id = ly->Panel(IdFromStack("__onyx_id_Top"),
                                      LyPnPar{.FillColor = m_Style[t ? interaction : idle], .Sizing = vsizing});
        ginfo.Ids[top] = isRoot ? id : LayoutId{NullLayoutId};
        ly->EndPanel();
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
// TODO(Isma): BUG: On windows, grabbed doesnt persist when ripping docked
// TODO(Isma): BUG: Border keeps hovered when stepping away from native window
OverlayFocusQueryFlags Overlay::iconButtonFocus(const LayoutId id, const CodePoint code, const LySz ysizing,
                                                const OverlayColor idle, const FocusFlags flags)
{
    Layout *ly = m_Active->GetActiveLayout();
    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly->QueryElement(id), flags);

    OverlayColor col = idle;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_ButtonPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_ButtonHovered;

    m_LastItem = ly->BeginPanel(id, LyPnPar{.FillColor = m_Style[col],
                                            .Alignment = Center,
                                            .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), ysizing}});

    ly->Unicode(NullLayoutId, code, getUnicodeParams());
    ly->EndPanel();
    return focusFlags;
}

void Overlay::popWindowStack()
{
    m_WindowStack.Pop();
    m_Active = m_WindowStack.IsEmpty() ? nullptr : m_WindowStack.GetBack();
}

u32 Overlay::processWindows()
{
    TKIT_PROFILE_NSCOPE("Onyx::Overlay::ProcessWindows");
    TKIT_ASSERT(!m_Active, "[ONYX][OVERLAY] Window stack not properly closed! Active window pointer is not null");
    TKIT_ASSERT(m_CurrentPopupDepth == 0, "[ONYX][OVERLAY] Pop up stack not properly closed! {} entries remaining",
                m_CurrentPopupDepth);
    TKIT_ASSERT(m_WindowStack.IsEmpty(), "[ONYX][OVERLAY] Window stack not properly closed! {} windows remaining",
                m_WindowStack.GetSize());
    TKIT_ASSERT(m_TabBarStack.IsEmpty(), "[ONYX][OVERLAY] A currently opened tab bar has been detected!");

    u32 modalWindow = 0;
    for (u32 i = 0; i < m_ActiveWindows.GetSize(); ++i)
    {
        m_Active = m_ActiveWindows[i];
        if (m_Active->Flags & OverlayWindowFlag_Modal)
            modalWindow = i + 1;

        if (m_Active->IsDockHost())
            iterateDockTree(m_Active->DockRoot, [&](DockNode *node) {
                if (node->IsLeaf() && !node->Windows.IsEmpty())
                    endTabBar(&node->TabData, node);
            });
    }
    m_Active = nullptr;

    m_ActiveIdLastFrame = m_ActiveId;
    if (!(m_StateFlags & StateFlag_ActiveIdMustPersist))
    {
        m_ActiveId = NullLayoutId;
        m_StateFlags &= ~StateFlag_ActiveAllowsInteraction;
    }

    if (!(m_StateFlags & StateFlag_PressedIdMustPersist))
    {
        m_PressedId = NullLayoutId;
        m_StateFlags &= ~StateFlag_PressedAllowsInteraction;
    }

    if (!(m_StateFlags & StateFlag_DraggedIdMustPersist))
    {
        m_DraggedId = NullLayoutId;
        m_DragDropId = NullLayoutId;
    }

    TKIT_ASSERT(m_PopupCollapseDepth <= m_PopupStack.GetSize(),
                "[ONYX][OVERLAY] Cannot have a popup depth ({}) greater than the popup stack ({})",
                m_PopupCollapseDepth, m_PopupStack.GetSize());

    bool pressingMouse = false;

    NativeWindow *nativeHovered = m_Grabbed ? m_Grabbed->GetNative() : nullptr;
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
        if (!m_Grabbed && (!nativeHovered || nativeHovered->Layer < nw->Layer) && nw->Window->IsHovered())
            nativeHovered = nw;
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

    const bool widgetBlocked = m_ActiveId != NullLayoutId && !(m_StateFlags & StateFlag_ActiveAllowsInteraction);
    const bool widgetPressed = m_PressedId != NullLayoutId && !(m_StateFlags & StateFlag_PressedAllowsInteraction);
    const bool widgetHovered = m_HoveredId != NullLayoutId && !(m_StateFlags & StateFlag_HoveredAllowsInteraction);

    m_StateFlags &= StateFlagPersist;
    m_StateFlags |= widgetBlocked * StateFlag_WantCaptureMouse;
    // if nothing is grabbed, we check mouse cursors here
    if (!m_Grabbed && nativeHovered)
    {
        MouseCursor cursor = notAllowed ? MouseCursor_NotAllowed : MouseCursor_Default;
        iterateReverseActiveWindows([&](OverlayWindow *win) {
            NativeWindow *nw = win->GetNative();
            GrabInfo &ginfo = win->Grab;
            ginfo.Flags = 0;
            ginfo.DockNode = nullptr;

            // if hovering a widget or window is not hovered (mouse is not on window) remove any hovering and skip
            const bool winHovered = win->Flags & (WindowInternalFlag_Hovered | WindowInternalFlag_Focused);
            if (winHovered && (win->Flags & WindowInternalFlag_InputHovered))
            {
                cursor = MouseCursor_IBeam;
                ginfo = {};
                return false;
            }

            const bool popupBlocked = win->PopupDepth != m_PopupStack.GetSize();
            const bool modalBlocked = win->PopupDepth < m_ModalCollapseDepth;
            // if (!winHovered || widgetHovered || widgetPressed || widgetBlocked || popupBlocked || modalBlocked)
            if (!winHovered || widgetPressed || widgetBlocked || popupBlocked || modalBlocked || win->IsDocked())
            {
                ginfo = {};
                return true;
            }

            ResizeFlags rflags = 0;

            const bool canResize = win->CanResize();
            const f32 bpadding = m_Style[OverlayStyle_BorderHoverPadding];
            Layout *ly = win->GetActiveLayout();
            if (canResize)
            {
                const bool hasHoverPadding = win->PopupDepth == 0 || win->PopupDepth == m_ModalCollapseDepth;
                for (u32 i = 0; i < ginfo.Ids.GetSize(); ++i)
                {
                    const LayoutId id = ginfo.Ids[i];
                    if (ly->IsHovered(id, nw->WorldMouse, bpadding,
                                      /* so that popups dont "fakingly" announce a resize*/ hasHoverPadding))
                    {
                        ginfo.InteractionColor = OverlayColor_WindowBorderHovered;
                        rflags |= 1U << i;
                    }
                }
            }

            bool mustUseHand = win->GetNative()->Window->IsKeyPressed(Key_LeftControl);
            if (rflags == 0 && win->DockRoot)
                mustUseHand &= iterateDockTree(win->DockRoot, [&](DockNode *node) {
                    const f32v2 &size = node->ReadOnlySize;

                    const LayoutId id = node->BorderId;
                    if ((!(node->Flags & OverlayDockNodeFlag_NoResize) || mustUseHand) &&
                        ly->IsHovered(id, nw->WorldMouse, bpadding))
                    {
                        ginfo.InteractionColor = OverlayColor_WindowBorderHovered;
                        // represents the static dock node size. check GrabInfo definition
                        // for more details
                        ginfo.Size = size;
                        ginfo.DockNode = node;
                        ginfo.StartRatio = node->Ratio;
                        rflags |=
                            node->Axis == LayoutAxis_Horizontal ? ResizeFlag_DockHorizontal : ResizeFlag_DockVertical;
                        return false;
                    }
                    return true;
                });
            else if (!canResize)
                return true;

            mustUseHand &= ginfo.DockNode && ginfo.DockNode->CanUndock();
            ginfo.DockNodePull = mustUseHand;

            ginfo.Flags = rflags;
            const auto cf = [&](const ResizeFlags f) { return f & rflags; };
            if (mustUseHand && cf(ResizeFlag_DockBorder))
                cursor = MouseCursor_Hand;
            else
            {
                if ((cf(ResizeFlag_Left) && cf(ResizeFlag_Bottom)) || (cf(ResizeFlag_Right) && cf(ResizeFlag_Top)))
                    cursor = MouseCursor_NESW;

                else if ((cf(ResizeFlag_Right) && cf(ResizeFlag_Bottom)) || (cf(ResizeFlag_Left) && cf(ResizeFlag_Top)))
                    cursor = MouseCursor_NWSE;

                else if (cf(ResizeFlag_Left | ResizeFlag_Right | ResizeFlag_DockVertical))
                    cursor = MouseCursor_EW;
                else if (cf(ResizeFlag_Bottom | ResizeFlag_Top | ResizeFlag_DockHorizontal))
                    cursor = MouseCursor_NS;
            }
            return true;
        });
        nativeHovered->Window->SetMouseCursor(cursor);
    }

    // check for mouse events
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
            else if (ev.Type == Event_WindowFocused)
            {
                nw->Layer = toTop();
                if (nw->Owner)
                    nw->Owner->SetLayer(toTop());
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

                    // NOTE(Isma, 06/08/26): I dont really like this
                    for (OverlayWindow *win : m_OverlayWindows)
                    {
                        const auto it = m_WidgetStates.Find(win->MenuBarId);
                        if (it != m_WidgetStates.end())
                            it->Value = 0;
                    }
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
        if (!(nw->Flags & NativeWindowFlag_PressingLeftMouse))
            nw->WorldMouseOnPress = nw->WorldMouse;

        if (nw->ClickClock.GetElapsed().AsMilliseconds() > m_Style[OverlayStyle_ClickMilliseconds])
            nw->OverflowClicks = 0;
    }

    // remove some state and check whether the window is collapsed
    const bool mustClearGrabInfo = !m_Grabbed && !nativeHovered;
    for (OverlayWindow *win : m_ActiveWindows)
    {
        const bool locallyCollapsed = win->CanCollapse() && Math::Approximately(win->Size[1], win->MinSize[1], 1.f);
        win->HeaderIcon = locallyCollapsed ? ArrowRightIcon : ArrowDownIcon;

        // we dont clear _Active flag yet as its needed for multi surface later
        win->Flags &= ~(WindowInternalFlag_Hovered | WindowInternalFlag_Focused | WindowInternalFlag_IsDockTarget |
                        WindowInternalFlag_MenuBarOpened | WindowInternalFlag_Popup);
        if (!(Flags & OverlayFlag_WindowPromotions))
            win->ClampToNative();
        if (mustClearGrabInfo)
            win->Grab = {};
    };

    bool canAssignHover = true;

    OverlayWindow *secondHovered = nullptr;
    iterateReverseActiveWindows([&](OverlayWindow *win, const bool overrideHovered = false) {
        const NativeWindow *nw = win->GetNative();
        const bool popupBlocked = win->PopupDepth != m_PopupStack.GetSize();
        const bool modalBlocked = win->PopupDepth < m_ModalCollapseDepth;
        const bool hasHoverPadding = win->PopupDepth == 0 || win->PopupDepth == m_ModalCollapseDepth;
        const bool dragWithHeader = win->Flags & OverlayWindowFlag_MoveWithHeader;

        const bool pressed = nw->Flags & NativeWindowFlag_LeftMousePressed;

        // if we are not popup locked, we jus check if window is hovered, which will allow grab to be set later.
        // if we are popup locked, we can still allow hovering if no depth > 0 widget previously set hovering id.
        // this is because we want to allow dragging immediately when collapsing the whole popup stack, but we dont
        // want dragging when collapsing all. thats why when popups exist, only widgets that belong to the popup
        // tree (any depth except 0) are allowed to set hovered id

        Layout *ly = win->GetActiveLayout();
        const bool bodyHovered =
            !modalBlocked && (!popupBlocked || !widgetHovered) &&
            (overrideHovered ||
             ly->IsHovered(win->Id, nw->WorldMouse, m_Style[OverlayStyle_BorderHoverPadding], hasHoverPadding));

        const bool headerHovered = bodyHovered && ly->IsHovered(win->HeaderId, nw->WorldMouse);
        const bool winHovered = bodyHovered && canAssignHover;
        const bool wantsMove = (dragWithHeader ? (headerHovered && canAssignHover) : winHovered);

        const bool inputHovered = win->Flags & WindowInternalFlag_InputHovered;
        win->Flags &= ~WindowInternalFlag_InputHovered;

        if (winHovered)
        {
            win->Flags |= WindowInternalFlag_Hovered;

            if (!(win->Flags & OverlayWindowFlag_MousePassThrough))
                m_StateFlags |= StateFlag_WantCaptureMouse;
            // we want to keep assigning hover flags until we reach a root. windows are carefully sorted so that, when
            // reverse-iterating, the children of a root window will come first. so we will keep assigning _Hovered
            // until we hit the parent

            canAssignHover = !win->OwnsActiveLayout();
        }
        else if (bodyHovered && !secondHovered && !win->IsDocked())
            secondHovered = win;

        GrabInfo &ginfo = win->Grab;
        const bool wantsResize = ginfo.Flags & ResizeFlag_WindowBorder;
        const bool wantsDockResize = ginfo.Flags & ResizeFlag_DockBorder;
        if (!win->IsDocked() && pressed && (wantsResize || wantsMove))
        {
            const bool allowGrab =
                (wantsMove && win->CanMove()) || (wantsResize && win->CanResize()) ||
                (wantsDockResize && (!(ginfo.DockNode->Flags & OverlayDockNodeFlag_NoResize) || ginfo.DockNodePull));

            if (!(win->Flags & OverlayWindowFlag_NoBringToFocus))
                win->SetLayer(toTop());

            if (!widgetHovered && !widgetPressed && !inputHovered && allowGrab)
            {
                ginfo.ScreenPos = win->GetActivePosition();

                // do not overwrite node size if it is a dock border resize
                if (!wantsDockResize)
                    ginfo.Size = win->Size;

                if (headerHovered && !wantsResize)
                    win->Flags |= WindowInternalFlag_HeaderGrabbed;

                m_Grabbed = win;
                return false;
            }
            return canAssignHover;
        }
        return true;
    });

    OverlayWindow *dockTarget = secondHovered;
    if (dockTarget)
        for (;;)
        {
            if (canDockingHappen(dockTarget))
            {
                dockTarget->Flags |= WindowInternalFlag_IsDockTarget;
                break;
            }
            if (dockTarget->IsRoot())
                break;
            dockTarget = dockTarget->Parent;
        }

    iterateReverseActiveWindows([&](OverlayWindow *win) {
        win->Flags |= WindowInternalFlag_Focused;
        // same logic here: keep going until we hit the parent
        return !win->OwnsActiveLayout();
    });

    const NativeWindow *gnw = m_Grabbed ? m_Grabbed->GetNativeForGrab() : nullptr;
    if (gnw && !(gnw->Flags & NativeWindowFlag_PressingLeftMouse))
    {
        NativeWindow *nw = m_Grabbed->GetNative();
        if (nw->Flags & NativeWindowFlag_CheckParentForGrab)
            nw->Flags |= NativeWindowFlag_LeftMouseReleased;

        nw->Flags &= ~NativeWindowFlag_CheckParentForGrab;
        m_Grabbed->Flags &= ~WindowInternalFlag_HeaderGrabbed;
        const bool move = m_Grabbed->Grab.Flags == 0;
        if (move)
            m_DockSource = m_Grabbed;

        m_Grabbed->Grab.Flags = 0;
        m_Grabbed->Grab.DockNode = nullptr;

        m_Grabbed = nullptr;
    }
    else if (m_Grabbed)
    {
        m_StateFlags |= StateFlag_WantCaptureMouse;
        m_Grabbed->Grab.InteractionColor = OverlayColor_WindowBorderPressed;
        GrabInfo &ginfo = m_Grabbed->Grab;

        NativeWindow *nw = m_Grabbed->GetNative();

        const f32v2 &ms = m_Grabbed->MinSize;
        const f32v2 &md = gnw->ScreenMouseDelta;

        const bool move = ginfo.Flags == 0;                        // && m_Grabbed->CanMove();
        const bool resize = ginfo.Flags & ResizeFlag_WindowBorder; // && m_Grabbed->CanResize();
        const bool dockResize = ginfo.Flags & ResizeFlag_DockBorder;

        TKIT_ASSERT(bool(ginfo.Flags & ResizeFlag_DockBorder) == bool(ginfo.DockNode),
                    "[ONYX][OVERLAY] If a dock border is being modified, the dock node pointer associated cannot be "
                    "null, and viceversa. Dock node not null: {}, border flags: {}",
                    bool(ginfo.DockNode), bool(ginfo.Flags & ResizeFlag_DockBorder));

        f32v2 &gpos = m_Grabbed->GetActivePosition();

        f32v2 &p = ginfo.ScreenPos;
        if (move)
        {
            m_DockSource = m_Grabbed;
            p += md;
            gpos = p;
        }
        else if (resize)
        {
            f32v2 &s = ginfo.Size;
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
        else if (ginfo.DockNode->CanUndock() && nw->Window->IsKeyPressed(Key_LeftControl))
            nw->Window->SetMouseCursor(MouseCursor_Default);
        else if (dockResize)
        {
            DockNode *node = ginfo.DockNode;
            ASSERT_WITH_WINDOW(m_Grabbed, !node->IsLeaf(), "[ONYX][OVERLAY] Node to resize must not be a leaf");

            const u32 iaxis = 1 - node->Axis;
            const f32 sign = iaxis == 0 ? 1.f : -1.f;
            const f32 drag = sign * (nw->WorldMouse[iaxis] - nw->WorldMouseOnPress[iaxis]);
            const f32 size = ginfo.Size[iaxis];

            ginfo.Ratio = ginfo.StartRatio + drag / size;

            const f32 minRatio = Math::Min(0.5f, m_Grabbed->MinSize[iaxis] / size);
            const f32 nratio = Math::Clamp(ginfo.Ratio, minRatio, 1.f - minRatio);

            DockNode *c0 = node->Children[0];
            DockNode *c1 = node->Children[1];

            iterateDockTree(c0, [&](DockNode *child) {
                if (!child->IsLeaf() && child->Axis == node->Axis)
                {
                    const f32 oldSize = node->ReadOnlySize[iaxis] * node->Ratio;
                    const f32 nsize = node->ReadOnlySize[iaxis] * nratio;
                    child->Ratio *= oldSize / nsize;
                    return false;
                }
                return true;
            });
            iterateDockTree(c1, [&](DockNode *child) {
                if (!child->IsLeaf() && child->Axis == node->Axis)
                {
                    const f32 oldSize = node->ReadOnlySize[iaxis] * (1.f - node->Ratio);
                    const f32 nsize = node->ReadOnlySize[iaxis] * (1.f - nratio);
                    child->Ratio = 1.f - (1.f - child->Ratio) * oldSize / nsize;
                    return false;
                }
                return true;
            });

            node->Ratio = nratio;
        }

        if (resize || move)
        {
            if (!(m_Grabbed->Flags & WindowInternalFlag_OwnsNative))
                m_Grabbed->ClampToNative();
            else
            {
                nw->Window->SetPosition(i32v2{gpos});
                nw->Window->SetScreenDimensions(u32v2{m_Grabbed->Size});
                nw->UpdateBorders();
            }
        }
    }
    else
        m_DockSource = nullptr;

    if (!nativeHovered || !m_HoveredLayoutCandidate ||
        !m_HoveredLayoutCandidate->IsHovered(m_HoveredWidgetCandidate, nativeHovered->WorldMouse))
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
        const LayoutId id = m_ScrollStack[i];
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

Layout *Overlay::createLayout()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    return m_Layouts.Append(tier->Create<Layout>(m_LayoutSpecs));
}
void Overlay::destroyLayout(const Layout *ly)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(ly);
}
void Overlay::removeLayout(const Layout *ly)
{
    for (u32 i = 0; i < m_Layouts.GetSize(); ++i)
        if (m_Layouts[i] == ly)
        {
            destroyLayout(ly);
            m_Layouts.RemoveUnordered(m_Layouts.begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][OVERLAY] Layout to remove was not found!");
}

OverlayWindow *Overlay::createOverlayWindow()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    OverlayWindow *win = tier->Create<OverlayWindow>();

    win->MinSize = getWindowMinSize();
    return m_OverlayWindows.Append(win);
}

void Overlay::destroyOverlayWindow(const OverlayWindow *win, const bool scrub)
{
    if (scrub)
    {
        if (win->Flags & WindowInternalFlag_OwnsNative)
            removeNativeWindow(win->Native);
        if (win->Layout)
            removeLayout(win->Layout);
    }
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(win);
}
void Overlay::removeOverlayWindow(const OverlayWindow *win)
{
    if (m_Grabbed == win)
        m_Grabbed = nullptr;
    if (m_DockSource == win)
        m_DockSource = nullptr;
    if (m_Tooltip == win)
        m_Tooltip = nullptr;
    if (m_HoveredLayoutCandidate == win->Layout)
    {
        m_HoveredLayoutCandidate = nullptr;
        m_HoveredWidgetCandidate = NullLayoutId;
    }
    for (u32 i = 0; i < m_ActiveWindows.GetSize(); ++i)
        if (m_ActiveWindows[i] == win)
        {
            m_ActiveWindows.RemoveOrdered(m_ActiveWindows.begin() + i);
            break;
        }
    for (u32 i = 0; i < m_OverlayWindows.GetSize(); ++i)
        if (m_OverlayWindows[i] == win)
        {
            destroyOverlayWindow(win);
            m_OverlayWindows.RemoveUnordered(m_OverlayWindows.begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][OVERLAY] Overlay window to remove was not found!");
}

NativeWindow *Overlay::createNativeWindow(Window *win)
{
    TKit::TierAllocator *tier = TKit::GetTier();
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

void Overlay::cleanupWindowState()
{
    for (OverlayWindow *win : m_OverlayWindows)
    {
        if (win->Flags & WindowInternalFlag_Active)
            win->Flags |= WindowInternalFlag_ActiveLastFrame;
        else
            win->Flags &= ~WindowInternalFlag_ActiveLastFrame;

        win->Flags &=
            ~(WindowInternalFlag_WantUpdateSize | WindowInternalFlag_Active | WindowInternalFlag_MultipleAppends |
              WindowInternalFlag_HasActiveChildren | WindowInternalFlag_DockSpaceSubmitted);
        win->SubmissionOrder = TKIT_U32_MAX;
    }
    m_SubmissionOrder = 0;
}

void Overlay::destroyNativeWindow(const NativeWindow *win)
{
    DestroyRenderContext(win->Context);
    CloseWindow(win->Window);

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(win);
}
void Overlay::removeNativeWindow(const NativeWindow *nw)
{
    for (OverlayWindow *win : m_OverlayWindows)
        if (win->Native == nw)
            win->Native = nw->Parent;

    for (u32 i = 0; i < m_NativeWindows.GetSize(); ++i)
        if (m_NativeWindows[i] == nw)
        {
            TKIT_ASSERT((Flags & OverlayFlag_FloatingMode) || i != 0,
                        "[ONYX][OVERLAY] Main native window can never be removed when not in floating mode!");
            destroyNativeWindow(nw);
            m_NativeWindows.RemoveUnordered(m_NativeWindows.begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][OVERLAY] Native window to remove was not found!");
}

NativeWindow *Overlay::promoteWindow(OverlayWindow *win, const f32v2 &pos, const f32v2 &dims)
{
    ASSERT_WITH_WINDOW(win, !(win->Flags & WindowInternalFlag_OwnsNative),
                       "[ONYX][OVERLAY] Cannot promote a window that owns a native");
    ASSERT_WITH_WINDOW(win, win->OwnsActiveLayout(), "[ONYX][OVERLAY] Only layout owning windows can be promoted");

    NativeWindow *parent = win->Native;
    win->Native = createNativeWindow(pos, dims);
    win->Native->Parent = parent;
    win->Native->Owner = win;
    // win->Layer = toTop();
    win->Native->Layer = win->Layer;
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
    ASSERT_WITH_WINDOW(win, win->Flags & WindowInternalFlag_OwnsNative,
                       "[ONYX][OVERLAY] Cannot demote a window that does not own a native");
    NativeWindow *nw = win->Native;
    NativeWindow *parent = nw->Parent;
    if (parent)
        win->ScreenPos = nw->ScreenPos - parent->ScreenPos;
    else
        win->ScreenPos = nw->ScreenPos;

    removeNativeWindow(nw);
    win->Native = parent;
    win->Flags &= ~WindowInternalFlag_OwnsNative;
}

void Overlay::demoteAllWindows()
{
    for (OverlayWindow *win : m_OverlayWindows)
    {
        NativeWindow *nw = win->Native;
        nw->Flags &= ~NativeWindowFlag_WantUpdateSize;
        // if tooltip native parent is not null, it means the tooltip is owning the native window
        if (win->Flags & WindowInternalFlag_OwnsNative)
            demoteWindow(win);
    }
    removeAllFloatWindows();
}
void Overlay::removeAllFloatWindows()
{
    for (u32 i = m_NativeWindows.GetSize() - 1; i < m_NativeWindows.GetSize(); --i)
        if (m_NativeWindows[i]->Flags & NativeWindowFlag_RepresentsFloatElement)
            removeNativeWindow(m_NativeWindows[i]);
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
    if (!(m_StateFlags & StateFlag_ActivePromotedFloatElement))
        removeAllFloatWindows();
    else
        for (auto it = m_FloatWindows.begin(); it != m_FloatWindows.end();)
        {
            NativeWindow *nw = it->Value;
#ifdef TKIT_ENABLE_ENSURE
            if (nw->Flags & NativeWindowFlag_RepresentsFloatElement)
            {
                TKIT_ENSURE(nw->Parent, "[ONYX][OVERLAY] Float native has null parent");
                bool found = false;
                for (const NativeWindow *n : m_NativeWindows)
                    if (n == nw->Parent)
                    {
                        found = true;
                        break;
                    }
                TKIT_ENSURE(found, "[ONYX][OVERLAY] float native's parent is not in m_NativeWindows");
            }
#endif
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

    for (OverlayWindow *win : m_OverlayWindows)
    {
        const bool active = win->Flags & WindowInternalFlag_Active;
        const bool canPromote = !(win->Flags & OverlayWindowFlag_NoPromotion);

        NativeWindow *nw = win->Native;
        const bool independentlyActive = active && (!win->DockRoot || win->DockRoot->Host == win);
        // instead of hard asserting...
        // TKIT_ASSERT(!independentlyActive || nw,
        //             "[ONYX][OVERLAY] If a window is active, it cannot have a null native window");

        if (!nw)
        {
            // ... we try to fix the issue by trying to assign a native
            if (independentlyActive)
                assignNativeWindowSomehow(win);
            continue;
        }

        const bool ownsNative = win->Flags & WindowInternalFlag_OwnsNative;
        if (ownsNative)
        {
            const bool nativeWants = nw->Flags & NativeWindowFlag_WantUpdateSize;
            const bool childWants = win->Flags & WindowInternalFlag_WantUpdateSize;
            if (nativeWants)
                win->Size = nw->Size;
            else if (childWants)
            {
                nw->Window->SetScreenDimensions(u32v2{win->Size});
                nw->Size = win->Size;
            }
        }
        nw->Flags &= ~NativeWindowFlag_WantUpdateSize;

        // if a window owns a native and that native doesnt have a parent (meaning not even dockspace is parented ->
        // dockspace does not exist -> we are in floating mode, so we can only demote by explicit closure)
        if (ownsNative && !nw->Parent)
        {
            if (!independentlyActive)
                demoteWindow(win);
            continue;
        }
        const f32v2 &wsize = win->Size;
        const f32v2 wpos = ownsNative ? nw->ScreenPos : (nw->ScreenPos + win->ScreenPos);

        const bool outsideNative = isOutsideNative(ownsNative ? nw->Parent : nw, wpos, wsize);
        if (ownsNative && (!canPromote || (!independentlyActive || (m_Grabbed != win && !outsideNative))))
            demoteWindow(win);
        else if (!ownsNative && canPromote && outsideNative && independentlyActive)
            promoteWindow(win, wpos, wsize);
    }
}

template <typename F> void Overlay::iterateReverseWindows(TKit::StaticArray32<OverlayWindow *> &windows, F &&func)
{
    constexpr bool hasRet = std::is_same_v<std::invoke_result_t<F, OverlayWindow *>, bool>;
    for (u32 i = windows.GetSize() - 1; i < windows.GetSize(); --i)
        if constexpr (hasRet)
        {
            if (!std::forward<F>(func)(windows[i]))
                return;
        }
        else
            std::forward<F>(func)(windows[i]);
}

/////////////////////////////////////////////
/// END WINDOWS/MENUS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// DOCKING
/////////////////////////////////////////////

const OverlayDockNode *DockSplit(const LayoutAxis axis, const f32 ratio, const OverlayDockNode *child0,
                                 const OverlayDockNode *child1, OverlayDockNodeFlags flags)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    OverlayDockNode *node = tier->Create<OverlayDockNode>();

    node->Children[0] = child0;
    node->Children[1] = child1;

    node->Axis = axis;
    node->Ratio = ratio;
    node->Flags = flags;

    return node;
}

const OverlayDockNode *DockTabBar(const TKit::Span<const LayoutId> windows, OverlayDockNodeFlags flags)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    OverlayDockNode *node = tier->Create<OverlayDockNode>();

    for (const LayoutId &id : windows)
        node->Windows.Append(id);

    node->Flags = flags;
    return node;
}

void Overlay::UndockWindow(const LayoutId id)
{
    if (!(Flags & OverlayFlag_Docking))
        return;

    OverlayWindow *win = findWindow(id);
    if (win && win->IsDocked())
        win->Flags |= WindowInternalFlag_MustUndock;
}

void Overlay::ApplyDockTree(const LayoutId hostId, const OverlayDockNode *uroot)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    OverlayWindow *host = findWindow(hostId);
    TKIT_ASSERT(!host || !host->IsDocked(), "[ONYX][OVERLAY] Cannot submit a window id as host that is already docked");

    DockNode *root;
    if (!host)
    {
        host = createDockHost(hostId);
        root = host->DockRoot;
    }
    else if (!host->DockRoot)
    {
        root = createDockNode();
        root->Windows.Append(host);

        host = createDockHostFromWindow(host, root);

        host->DockRoot = root;
        host->Id = hostId;
    }
    else
        root = host->DockRoot;

    struct DockInfo
    {
        const OverlayDockNode *UserNode;
        DockNode *Node;
    };

    TKit::StackArray<DockInfo> nodes{};
    nodes.Reserve(m_DockNodes.GetCapacity());

    const auto getDirection = [&](const OverlayDockNode *node) {
        if (node->IsLeaf())
            return s_Center;

        return node->Axis == LayoutAxis_Horizontal ? s_Top : s_Left;
    };

    nodes.Append(uroot, root);
    while (!nodes.IsEmpty())
    {
        const DockInfo info = nodes.GetBack();
        nodes.Pop();

        const OverlayDockNode *unode = info.UserNode;
        DockNode *node = info.Node;
        node->Flags = unode->Flags;

        DockNode *parent = dockInsert(node, getDirection(unode), unode->Ratio);

        if (parent->IsRoot())
            root = parent;

        if (unode->IsLeaf())
            for (const LayoutId &id : unode->Windows)
            {
                OverlayWindow *win = getOrCreateOverlayWindow(id);
                ASSERT_WITH_WINDOW(win, !win->IsDocked() && !win->IsDockHost(),
                                   "[ONYX][OVERLAY] Cannot submit a window to be docked that is already docked "
                                   "elsewhere or is a dock host");

                node->Windows.Append(win);
                win->DockParent = node;
                win->DockRoot = root;
            }
        else
        {
            nodes.Append(unode->Children[1], parent->Children[1]);
            nodes.Append(unode->Children[0], parent->Children[0]);
        }
        tier->Destroy(unode);
    }
}

LayoutId Overlay::DockSpace(const LayoutId id, const OverlayDockNodeFlags flags, const OverlayWindowFlags wflags)
{
    OverlayWindow *host = getOrCreateOverlayWindow(id, m_Active);
    host->Flags &= WindowInternalFlagPersist;

    if (!host->DockRoot)
    {
        DockNode *root = createDockNode();
        root->Host = host;
        host->DockRoot = root;
    }
    host->DockRoot->Flags = flags;

    m_WindowStack.Append(host);
    m_Active = host;

    beginDockHost(host, wflags | WindowInternalFlag_DockSpace | WindowInternalFlag_DockSpaceSubmissionOrderMatters |
                            WindowInternalFlag_DockSpaceSubmitted);
    ASSERT_WITH_WINDOW(host, !(host->Flags & WindowInternalFlag_MultipleAppends),
                       "[ONYX][OVERLAY] Cannot submit the same dock host multiple times");

    host->GetActiveLayout()->EndPanel();

    popWindowStack();
    return id;
}

LayoutId Overlay::FullScreenDockSpace(const OverlayDockNodeFlags flags, const OverlayWindowFlags wflags)
{
    TKIT_ASSERT(!(Flags & OverlayFlag_FloatingMode),
                "[ONYX][OVERLAY] Cannot have a full screen dockspace in floating mode");

    SetNextWindowPosition({0.f, m_MainDockSpaceOffset});
    const LayoutId id = DockSpace("__onyx_id_Main_dockspace", flags,
                                  wflags | OverlayWindowFlag_NoMove | OverlayWindowFlag_NoPromotion |
                                      OverlayWindowFlag_NoBringToFocus | OverlayWindowFlag_NoBorders);

    m_MainDockSpace = findWindow(id);
    m_MainDockSpace->Size = m_MainDockSpace->Native->Size;
    m_MainDockSpace->Size[1] -= m_MainDockSpaceOffset;
    m_MainDockSpaceOffset = 0.f;

    TKIT_ASSERT(m_MainDockSpace);
    ASSERT_WITH_WINDOW(
        m_MainDockSpace, m_MainDockSpace->IsRoot(),
        "[ONYX][OVERLAY] The full screen dockspace can only be called outside a Begin/End window region");

    return id;
}

bool DockNode::IsEmpty() const
{
    if (IsLeaf())
    {
        for (OverlayWindow *win : Windows)
            if (win->Flags & (WindowInternalFlag_Active | WindowInternalFlag_ActiveLastFrame))
                return false;

        // we do this because we dont actually want to collapse the whole node if there are no windows. this is because
        // the only instance where this happens is when using the main fullscreen dockspace. normal dock node leafs will
        // never have an empty window array
        return !Windows.IsEmpty();
    }
    return Children[0]->IsEmpty() && Children[1]->IsEmpty();
}

bool DockNode::CanUndock() const
{
    if (IsRoot() && !(Host->Flags & WindowInternalFlag_DockSpace))
        return false;
    if (IsLeaf())
    {
        for (OverlayWindow *win : Windows)
            if (win->Flags & OverlayWindowFlag_NoUndocking)
                return false;
        return true;
    }
    return Children[0]->CanUndock() && Children[1]->CanUndock();
}

DockNode *Overlay::createDockNode()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    DockNode *node = tier->Create<DockNode>();
    return m_DockNodes.Append(node);
}

void Overlay::destroyDockNode(const DockNode *node)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(node);
}

void Overlay::removeDockNode(const DockNode *node)
{
    for (OverlayWindow *win : m_OverlayWindows)
        if (win->Grab.DockNode == node)
        {
            win->Grab.DockNode = nullptr;
            win->Grab.Flags &= ~ResizeFlag_DockBorder;
        }

    for (u32 i = 0; i < m_DockNodes.GetSize(); ++i)
        if (m_DockNodes[i] == node)
        {
            destroyDockNode(node);
            m_DockNodes.RemoveUnordered(m_DockNodes.begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][OVERLAY] Dock node to remove was not found!");
}

OverlayWindow *Overlay::getOrCreateDockHost(const LayoutId id, OverlayWindow *parent)
{
    OverlayWindow *win = findWindow(id);
    if (win)
        return win;
    return createDockHost(id, parent);
}

OverlayWindow *Overlay::createDockHost(const LayoutId id, OverlayWindow *parent)
{
    OverlayWindow *host = createOverlayWindow(id, parent);
    DockNode *root = createDockNode();
    root->Host = host;
    host->DockRoot = root;
    return host;
}

OverlayWindow *Overlay::createDockHost(const OverlayWindow *win, DockNode *rootNode, const bool fromWindow)
{
    ASSERT_WITH_WINDOW(win, rootNode->IsRoot(),
                       "[ONYX][OVERLAY] When creating a dock host, the passed node must be the root");
    ASSERT_WITH_WINDOW(win, !win->IsDocked(),
                       "[ONYX][OVERLAY] When creating a dock host, the base window must not be docked");

    OverlayWindow *host = createOverlayWindow();
    host->Size = fromWindow ? win->Size : Math::Max(rootNode->ReadOnlySize, win->MinSize);

    if (!fromWindow || win->IsRoot())
    {
        host->Layout = createLayout();
        host->Layer = toTop();
    }
    else
    {
        host->Parent = win->Parent;
        host->Flags |= (win->Flags & OverlayWindowFlag_ChildGrow) | WindowInternalFlag_DockSpaceSubmissionOrderMatters;
    }

    NativeWindow *nw = win->GetNative();
    if (win->GetRoot()->Flags & WindowInternalFlag_OwnsNative)
    {
        NativeWindow *mainNative = getMainNativeWindow();
        const f32v2 screenPos = fromWindow ? nw->ScreenPos : nw->ToScreen(rootNode->ReadOnlyPosition);

        if (!mainNative)
            promoteWindow(host, screenPos, host->Size);
        else
        {
            host->Native = mainNative;
            host->ScreenPos = screenPos - mainNative->ScreenPos;
        }
    }
    else
    {
        host->Native = nw;
        host->ScreenPos = fromWindow ? win->ScreenPos : win->ToLocalScreen(rootNode->ReadOnlyPosition);
    }
    host->DockRoot = rootNode;

    return host;
}

// this is the only function allowed to write ReadOnlyPosition and ReadOnlySize
template <typename F> void Overlay::iterateDockTreeWithLayoutUpdate(const OverlayWindow *win, F &&func)
{
    constexpr bool hasRet = std::is_same_v<std::invoke_result_t<F, DockNode *>, bool>;

    const bool ownsNative = win->Flags & WindowInternalFlag_OwnsNative;
    const f32v2 wpos = ownsNative ? win->Native->GetWorldTopLeft() : win->ToWorld(win->ScreenPos);
    const f32v2 &wsize = win->Size;

    TKit::StackArray<DockNode *> nodes{};
    nodes.Reserve(m_DockNodes.GetSize());

    DockNode *root = win->DockRoot;

    root->ReadOnlyPosition = wpos;
    root->ReadOnlySize = wsize;

    nodes.Append(root);
    ASSERT_WITH_WINDOW(win, root->IsRoot(), "[ONYX][OVERLAY] A dock root must have no parent");
    while (!nodes.IsEmpty())
    {
        DockNode *node = nodes.GetBack();
        nodes.Pop();

        const f32v2 &pos = node->ReadOnlyPosition;
        const f32v2 &size = node->ReadOnlySize;

        if (!node->IsLeaf())
        {
            ASSERT_WITH_WINDOW(win, node->Windows.IsEmpty(), "[ONYX][OVERLAY] A parent node may not have any windows");
            DockNode *c0 = node->Children[0];
            DockNode *c1 = node->Children[1];

            const f32v2 &pos0 = pos;
            f32v2 pos1 = pos;

            f32v2 size0 = size;
            f32v2 size1 = size;

            const u32 iaxis = 1 - node->Axis;
            f32 r = node->Ratio;
            if (c0->IsEmpty())
                r = 0.f;
            else if (c1->IsEmpty())
                r = 1.f;
            node->EffectiveRatio = r;
            size0[iaxis] *= r;
            size1[iaxis] *= 1.f - r;
            pos1[iaxis] += iaxis == 0 ? size0[0] : -size0[1];

            ASSERT_WITH_WINDOW(win, c0->Parent == node && c1->Parent == node,
                               "[ONYX][OVELAY] Parent mismatch! Children nodes must have a correct parent pointer");

            c0->ReadOnlyPosition = pos0;
            c0->ReadOnlySize = size0;

            c1->ReadOnlyPosition = pos1;
            c1->ReadOnlySize = size1;

            nodes.Append(c1);
            nodes.Append(c0);
        }

        if constexpr (hasRet)
        {
            if (!std::forward<F>(func)(node))
                return;
        }
        else
            std::forward<F>(func)(node);
    }
}

bool Overlay::canDockingHappen(const OverlayWindow *target) const
{
    const OverlayWindow *source = m_DockSource;
    const bool dockingEnabled = Flags & OverlayFlag_Docking;
    const bool dockAttempt = dockingEnabled && source && (source->Flags & WindowInternalFlag_HeaderGrabbed);

    const bool dockAllowed = dockAttempt && !((source->Flags | target->Flags) & OverlayWindowFlag_NoDocking);

    const bool submissionOk = dockAllowed && (!(target->Flags & WindowInternalFlag_DockSpaceSubmissionOrderMatters) ||
                                              target->SubmissionOrder < source->SubmissionOrder);
    return submissionOk;
}

void Overlay::beginDockHost(OverlayWindow *host, const OverlayWindowFlags flags, const bool redirectedByHostedWindow)
{
    TKit::TierArray<LayoutId> backup{std::move(m_IdStack)};
    PushId(host->Id);

    TKIT_ENSURE_RETURNS(beginWindow(host, nullptr,
                                    flags | OverlayWindowFlag_NoHeaderBar | OverlayWindowFlag_NoCollapse |
                                        OverlayWindowFlag_NoCloseButton,
                                    {}, redirectedByHostedWindow),
                        true, "[ONYX][OVERLAY] If a window is docked, its parent window must not be collapsed");
    buildDockHostHierarchy(host);

    if (!(host->Flags & (OverlayWindowFlags(WindowInternalFlag_MultipleAppends) | OverlayWindowFlag_NoBorders)))
        drawWindowBorders(host);

    PopId();
    m_IdStack = std::move(backup);
}

void Overlay::buildDockHostHierarchy(OverlayWindow *dockHost)
{
    ASSERT_WITH_WINDOW(dockHost, !dockHost->IsDocked(), "[ONYX][OVERLAY] A dock host cannot be directly docked");
    ASSERT_WITH_WINDOW(dockHost, dockHost->IsDockHost(), "[ONYX][OVERLAY] Passed window must be a dock host");

    Layout *ly = dockHost->GetActiveLayout();
    // invisible header bar
    dockHost->HeaderId = ly->Panel(
        IdFromStack("__onyx_id_Invisible_header_bar"),
        LyPnPar{.Sizing = sabs({dockHost->Size[0], dockHost->MinSize[1]}),
                .Floating = {.Enable = true, .DrawOnTop = false, .Attachment = TopLeft, .Alignment = TopLeft}});

    // this is the first time the dock host (the window that host all docked windows) is getting appended to.
    // before setting up the actual window that is being started by the user, we have to setup the dock tree panel
    // structure of the dockspace that will host all of them
    NativeWindow *nw = dockHost->GetNative();
    const bool hostAllowsBckg =
        dockHost->Flags & (OverlayWindowFlag_NoBackground | OverlayWindowFlags(WindowInternalFlag_DockSpace));

    const f32 cpadding = m_Style[OverlayStyle_ContentAreaPadding];
    const f32 cgap = m_Style[OverlayStyle_ChildGap];

    // this is done because borders must be drawn on top of everything, but cannot be floats drawn on top because they
    // would draw on top of other windows
    struct BorderData
    {
        DockNode *Node;
        LySz2 BorderSize;
        LyOf2 BorderOffset;
        OverlayColor BorderColor;
    };

    TKit::StackArray<BorderData> borderData{};
    borderData.Reserve(m_DockNodes.GetSize());

    iterateDockTreeWithLayoutUpdate(dockHost, [&](DockNode *node) {
        const f32v2 &size = node->ReadOnlySize;

        const LayoutId nodeId = PushId(node);
        DockNode *p = node->Parent;
        if (p)
            ly->OpenPanel(p->ContentId);

        if (node->IsLeaf())
        {
            // if windows are empty, we dont draw any tab bars and have a transparent bckg. this happens when we are
            // the main dockspace, which is the only one allowed to have zero child windows in a node

            const bool empty = node->Windows.IsEmpty();

            TabBarData &tdata = node->TabData;
            bool childHasBckg = hostAllowsBckg && !empty && tdata.OpenId != NullLayoutId;
            if (childHasBckg)
            {
                const u32 idx = tdata.GetTabById(tdata.OpenId);
                childHasBckg = !(tdata.Tabs[idx].Window->Flags & OverlayWindowFlag_NoBackground);
            }

            const Color bckg = m_Style[childHasBckg ? OverlayColor_WindowBackgroundExpanded : OverlayColor_None];
            LySz2 sizing;
            if (!p)
                sizing = srel(1.f);
            else
            {
                const f32 ratio = p->GetChildEffectiveRatio(node);
                sizing = p->Axis == LayoutAxis_Horizontal ? srel({1.f, ratio}) : srel({ratio, 1.f});
            }

            node->ContentId = ly->BeginPanel(nodeId, LyPnPar{.FillColor = bckg,
                                                             .Direction = LayoutDirection_TopToBottom,
                                                             .Alignment = TopLeft,
                                                             .Sizing = sizing,
                                                             .Padding = cpadding,
                                                             .ChildGap = cgap});
            if (!empty)
                beginTabBar(&node->TabData, IdFromStack(nodeId),
                            OverlayTabBarFlag_Reorderable | OverlayTabBarFlag_NoBottomLine | TabBarFlag_ForDocking);
            ly->EndPanel();
        }
        else
        {
            LayoutDirection dir;
            LySz2 borderSize;
            LyOf2 borderOffset;
            const f32 bwidth = m_Style[OverlayStyle_WindowBorderWidth];

            f32 boffset;
            if (p)
                boffset = 1.5f * bwidth;
            else if (dockHost->Flags & OverlayWindowFlag_NoBorders)
                boffset = 0.f;
            else
                boffset = 2.f * bwidth;

            const u32 iaxis = 1 - node->Axis;
            const f32 minRatio = Math::Min(0.5f, dockHost->MinSize[iaxis] / size[iaxis]);
            node->Ratio = Math::Clamp(node->Ratio, minRatio, 1.f - minRatio);

            const f32 ratio = node->EffectiveRatio;
            bool resizing = dockHost->Grab.DockNode == node;
            if (node->Axis == LayoutAxis_Horizontal)
            {
                dir = LayoutDirection_TopToBottom;
                borderSize = sabs({size[0] - boffset, bwidth});
                borderOffset = oabs({p ? -0.25f * bwidth : 0.f, size[1] * (0.5f - ratio)});
                resizing &= bool(dockHost->Grab.Flags & ResizeFlag_DockHorizontal);
            }
            else
            {
                dir = LayoutDirection_LeftToRight;
                borderSize = sabs({bwidth, size[1] - boffset});
                borderOffset = oabs({size[0] * (ratio - 0.5f), p ? 0.25f * bwidth : 0.f});
                resizing &= bool(dockHost->Grab.Flags & ResizeFlag_DockVertical);
            }

            node->ContentId =
                ly->BeginPanel(nodeId, LyPnPar{.Direction = dir, .Alignment = TopLeft, .Sizing = sabs(size)});

            if (ratio != 1.f && ratio != 0.f)
                borderData.Append(node, borderSize, borderOffset,
                                  resizing ? dockHost->Grab.InteractionColor : OverlayColor_WindowBorderIdle);

            // upcoming children will open the panel and draw here

            ly->EndPanel();
        }
        if (p)
            ly->EndPanel();
        PopId();
    });

    for (const BorderData &bdata : borderData)
    {
        DockNode *node = bdata.Node;
        ly->OpenPanel(node->ContentId);

        PushId(node);
        node->BorderId = ly->Panel(IdFromStack("__onyx_id_Dock_axis"), LyPnPar{.FillColor = m_Style[bdata.BorderColor],
                                                                               .Sizing = bdata.BorderSize,
                                                                               .SelfOffset = bdata.BorderOffset,
                                                                               .Floating =
                                                                                   {
                                                                                       .Enable = true,
                                                                                       .DrawOnTop = false,
                                                                                       .Clip = true,
                                                                                       .Attachment = Center,
                                                                                       .Alignment = Center,
                                                                                   },
                                                                               .SelfOverflow = LayoutOverflow_Spill});
        if (nw->Window->IsKeyPressed(Key_LeftControl) && node->CanUndock())
        {
            const LayoutElementQueryInfo *elm = ly->QueryElement(node->BorderId);
            const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(
                elm,
                FocusFlag_ActiveAllowsInteraction | FocusFlag_PressedAllowsInteraction |
                    FocusFlag_HoveredAllowsInteraction,
                m_Style[OverlayStyle_BorderHoverPadding], OverlayHoveredFlag_AllowBlockedByWindowGrab);

            if (focusFlags & OverlayFocusQueryFlag_DragSource)
                node->Flags |= DockNodeFlag_MustUndock | DockNodeFlag_MustGrabWhenUndocked;
        }

        ly->EndPanel();
        PopId();
    }
}

void Overlay::detachNodeFromParent(DockNode *node)
{
    DockNode *parent = node->Parent;
    ASSERT_WITH_WINDOW(parent->Host, parent, "[ONYX][OVERLAY] Cannot detach a root node from its parent");
    OverlayWindow *host = node->Host;

    DockNode *granpa = parent->Parent;
    DockNode *otherChild = parent->OtherChild(node);
    if (granpa)
    {
        otherChild->Parent = granpa;
        granpa->Children[granpa->ChildIndex(parent)] = otherChild;
    }
    else
    {
        // otherChild is the new root
        iterateDockTree(otherChild, [&](const DockNode *n) {
            if (n->IsLeaf())
                for (OverlayWindow *child : n->Windows)
                    child->DockRoot = otherChild;
        });

        otherChild->Parent = nullptr;
        host->DockRoot = otherChild;
    }
    removeDockNode(parent);
    node->Parent = nullptr;
}

void Overlay::undockWindow(OverlayWindow *win)
{
    ASSERT_WITH_WINDOW(win, win->DockRoot && win->DockParent,
                       "[ONYX][OVERLAY] Cannot undock a window that doesnt have a dock root/parent");

    LOG_DOCK_TREE(win, "Before window undock");

    DockNode *root = win->DockRoot;
    DockNode *node = win->DockParent;

    win->DockRoot = nullptr;
    win->DockParent = nullptr;

    for (u32 i = 0; i < node->Windows.GetSize(); ++i)
        if (win == node->Windows[i])
        {
            node->Windows.RemoveUnordered(node->Windows.begin() + i);
            break;
        }
    ASSERT_WITH_WINDOW(win, node->IsLeaf(), "[ONYX][OVERLAY] Window's dock parent must be a leaf node");

    OverlayWindow *winRoot = root->Host->GetRoot();

    ASSERT_WITH_WINDOW(win, !(win->Flags & WindowInternalFlag_OwnsNative),
                       "[ONYX][OVERLAY] A window about to be undocked cannot possibly own any native window");

    const auto adjustWindowPromotion = [&] {
        const bool rootOwns = winRoot->Flags & WindowInternalFlag_OwnsNative;

        win->Size = Math::Max(node->ReadOnlySize, win->MinSize);
        if (!win->Native || (rootOwns && win->Native == winRoot->Native))
        {
            win->Native = getMainNativeWindow();
            promoteWindow(win, win->ScreenPos, win->Size);
        }
        else if (rootOwns)
            win->ScreenPos -= win->Native->ScreenPos;
    };

    // when undocked, child windows need no position modifications at all, and cannot be grabbed/promoted anyways
    if (win->IsRoot())
    {
        if ((win->Flags & WindowInternalFlag_MustGrabWhenUndocked) && !(win->Flags & OverlayWindowFlag_NoMove))
        {
            const f32v2 &pos = winRoot->Native->WorldMouse;

            win->ScreenPos = winRoot->ToScreen(pos + f32v2{-win->MinSize[0], 0.5f * win->MinSize[1]});

            adjustWindowPromotion();

            m_Grabbed = win;
            win->Grab.ScreenPos = win->GetActivePosition();
            win->Grab.Size = win->Size;
            win->Flags |= WindowInternalFlag_HeaderGrabbed;
            win->Layer = toTop();
        }
        else
        {
            // this position is wrt the dockspace, so the transformation must be wrt the dockspace
            const f32v2 &pos = node->ReadOnlyPosition;
            win->ScreenPos = winRoot->ToScreen(pos);
            win->Layer = winRoot->Layer;

            adjustWindowPromotion();
        }
    }

    if (node->Windows.IsEmpty() && !(node->Flags & OverlayDockNodeFlag_CanBeEmpty))
    {
        if (!node->IsRoot())
            detachNodeFromParent(node);
        if (node == root)
            removeOverlayWindow(root->Host);
        removeDockNode(node);
    }

    win->Flags &= ~(WindowInternalFlag_MustUndock | WindowInternalFlag_MustGrabWhenUndocked);
    LOG_DOCK_TREE(win, "After window undock");
}
void Overlay::undockNode(DockNode *node)
{
    ASSERT_WITH_WINDOW(node->Host, !node->IsRoot() || (node->Host->Flags & WindowInternalFlag_DockSpace),
                       "[ONYX][OVERLAY] Cannot undock a root node that is not a dockspace");
    LOG_DOCK_TREE(node->Host, "Before node undock");

    OverlayWindow *oldHost = node->Host;
    const bool grab = node->Flags & DockNodeFlag_MustGrabWhenUndocked;
    node->Flags &= ~(DockNodeFlag_MustUndock | DockNodeFlag_MustGrabWhenUndocked);

    LayoutId id = oldHost->Id;

    if (node->IsRoot())
    {
        // means the node is a dockspace and user is trying to grab all the tabbed windows at root. we need to create a
        // new node and empty out the other
        DockNode *old = node;
        node = createDockNode();
        *node = *old;
        // reset it to factory settings except for host
        *old = DockNode{};
        old->Host = oldHost;

        if (!node->IsLeaf())
        {
            node->Children[0]->Parent = node;
            node->Children[1]->Parent = node;
        }
    }
    else
        detachNodeFromParent(node);

    const auto updateWindows = [&](DockNode *leaf) {
        ASSERT_WITH_WINDOW(node->Host, !leaf->Windows.IsEmpty(), "[ONYX][OVERLAY] Cannot undock an empty node");
        for (OverlayWindow *win : leaf->Windows)
        {
            TKit::HashCombine(id.Id, win->Id.Id);
            win->DockRoot = node;
            win->DockParent = leaf;
        }
    };

    OverlayWindow *host = createDockHostFromNode(oldHost, node);
    node->Host = host;
    // we remove the can be empty flags because undocking a node means it becomes free. dock trees cannot have empty
    // nodes or you end up with holes in the tree
    if (node->IsLeaf())
    {
        node->Flags &= ~OverlayDockNodeFlag_CanBeEmpty;
        updateWindows(node);
    }
    else
        iterateDockTree(node, [&](DockNode *child) {
            child->Flags &= ~OverlayDockNodeFlag_CanBeEmpty;
            child->Host = host;
            if (child->IsLeaf())
                updateWindows(child);
        });

    host->Id = id;
    if (grab)
    {
        const f32v2 &pos = host->GetNative()->WorldMouse;
        host->SetActivePosition(host->ToScreen(pos + 0.5f * f32v2{-host->MinSize[0], host->MinSize[1]}));

        host->Grab.ScreenPos = host->GetActivePosition();
        host->Grab.Size = host->Size;
        host->Flags |= WindowInternalFlag_HeaderGrabbed;
        host->Layer = toTop();
        m_Grabbed = host;
    }

    LOG_DOCK_TREE(node->Host, "After node undock (new tree)");
    LOG_DOCK_TREE(oldHost, "After node undock (old tree)");
}
void Overlay::undockMarked()
{
    const bool dockingEnabled = Flags & OverlayFlag_Docking;
    if (dockingEnabled)
    {
        for (OverlayWindow *win : m_OverlayWindows)
            if ((win->Flags & WindowInternalFlag_DockSpace) && !(win->Flags & WindowInternalFlag_DockSpaceSubmitted) &&
                (win->Flags & OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted))
                iterateDockTree(win->DockRoot, [](DockNode *node) {
                    if (node->IsLeaf())
                        for (OverlayWindow *child : node->Windows)
                            child->Flags |= WindowInternalFlag_MustUndock;
                });
        for (u32 i = m_DockNodes.GetSize() - 1; i < m_DockNodes.GetSize(); --i)
            if (m_DockNodes[i]->Flags & DockNodeFlag_MustUndock)
                undockNode(m_DockNodes[i]);
    }
    // there is a known bug here, and is seemingly mitigated when iterating this way. when trying to remove many docked
    // windows that are deeply nested (multiple nested dockspaces), the final "undocked" positions may get messed up for
    // deeply nested windows. when iterating in reverse window insertion order, this seems to happen less frequently.
    // this is not a very good fix
    const auto isTooEmpty = [](const OverlayWindow *win) {
        return win->IsDocked() && win->DockRoot == win->DockParent &&
               !(win->DockParent->Host->Flags & WindowInternalFlag_DockSpace) &&
               win->DockParent->Windows.GetSize() == 1;
    };
    iterateReverseWindows(m_OverlayWindows, [&](OverlayWindow *win) {
        const bool isDocked = win->IsDocked();

        const bool mustUndock =
            win->Flags & (OverlayWindowFlags(WindowInternalFlag_MustUndock) | OverlayWindowFlag_NoDocking);

        if (isDocked && (!dockingEnabled || mustUndock || isTooEmpty(win)))
            undockWindow(win);
    });
    // a second sweep because last sweep may have left some windows alone that would get undocked next frame with full
    // window size
    if (dockingEnabled)
        iterateReverseWindows(m_OverlayWindows, [&](OverlayWindow *win) {
            if (isTooEmpty(win))
                undockWindow(win);
        });
}

// insert a new node at the target node, involving usually creating a new parent and sibling

// target and source are convenient additions to specify, respectively, the dock host (target) and the new window to be
// docked (source) what does it mean for target to be null? it means target must be derived from the target node's host

// if target is specified, targetNode can be null. this usually means a new dock tree is going to be born from 2
// windows!

// what does it mean for source to be null? it means the node inserted will be empty: no "start" window will be attached
// to it
DockNode *Overlay::dockInsert(DockNode *targetNode, const i32v2 &loc, const f32 ratio, OverlayWindow *source,
                              OverlayWindow *target)
{
    TKIT_ASSERT(targetNode || target,
                "[ONYX][OVERLAY] If target is not specified when inserting, the target node must not be null");

    if (!target)
        target = targetNode->Host;
    ASSERT_WITH_WINDOW(target, !targetNode || target == targetNode->Host,
                       "[ONYX][OVERLAY] If both targetNode and target are specified, the target window must be the "
                       "target node's host");

    LOG_DOCK_TREE(target, "Before insert");
    ASSERT_WITH_WINDOW(target, bool(target->DockRoot) == bool(targetNode),
                       "[ONYX][OVERLAY] If target window has an active dock root, leaf node must not be null. If "
                       "it does not, target node must be null");

    ASSERT_WITH_WINDOW(
        target, !target->DockRoot || target->DockRoot->Host == target,
        "[ONYX][OVERLAY] If docking against a fully formed dock tree, the target window must be the host");

    ASSERT_WITH_WINDOW(
        source, !source || !source->DockRoot || source->IsDockHost(),
        "[ONYX][OVERLAY] If the dock source has a dock root and is about to be docked, it must be a dock host");

    if (!target->DockRoot)
    {
        ASSERT_WITH_WINDOW(target, source, "[ONYX][OVERLAY] If target has no dock root, source must not be null");
        targetNode = createDockNode();
        targetNode->Host = createDockHostFromWindow(target, targetNode);
        targetNode->Host->Id = TKit::Hash(target->Id, source->Id);

        targetNode->Windows.Append(target);
        target->DockRoot = targetNode;
        target->DockParent = targetNode;
    }

    DockNode *parent;
    if (loc[s_Axis] != s_CenterAxis)
    {
        parent = createDockNode();
        OverlayWindow *host = targetNode->Host;
        if (target->DockRoot == targetNode)
        {
            target->DockRoot = parent;
            host->DockRoot = parent;
            iterateDockTree(targetNode, [&](DockNode *child) {
                if (child->IsLeaf())
                    for (OverlayWindow *wchild : child->Windows)
                        wchild->DockRoot = parent;
            });
        }

        parent->Ratio = ratio;
        parent->Axis = LayoutAxis(1 - loc[s_Axis]);
        parent->Host = host;

        DockNode *sibling;
        if (source && source->DockRoot)
        {
            sibling = source->DockRoot;
            removeOverlayWindow(sibling->Host);

            iterateDockTree(sibling, [&](DockNode *node) {
                node->Host = host;
                if (node->IsLeaf())
                    for (OverlayWindow *child : node->Windows)
                        child->DockRoot = target->DockRoot;
            });
        }
        else
        {
            sibling = createDockNode();
            sibling->Host = host;
            if (source)
            {
                sibling->Windows.Append(source);
                source->DockParent = sibling;
                source->DockRoot = target->DockRoot;
            }
        }

        parent->Children[(1 + loc[s_Side]) / 2] = loc[s_Axis] == s_Horizontal ? sibling : targetNode;
        parent->Children[(1 - loc[s_Side]) / 2] = loc[s_Axis] == s_Horizontal ? targetNode : sibling;

        DockNode *granpa = targetNode->Parent;
        if (granpa)
        {
            parent->Parent = granpa;
            granpa->Children[granpa->ChildIndex(targetNode)] = parent;
        }

        targetNode->Parent = parent;
        sibling->Parent = parent;
    }
    else if (source && source->DockRoot)
    {
        DockNode *sourceRoot = source->DockRoot;
        const bool targetIsEmpty = target->DockRoot->IsLeaf() && target->DockRoot->Windows.IsEmpty();
        ASSERT_WITH_WINDOW(source, sourceRoot->IsLeaf() || targetIsEmpty,
                           "[ONYX][OVERLAY] If the dock source has a dock parent and is being docked at the "
                           "center, the dock root must be a leaf node or the target dock tree must be empty");

        if (targetIsEmpty && !sourceRoot->IsLeaf())
        {
            ASSERT_WITH_WINDOW(target, target->IsDockHost(),
                               "[ONYX][OVERLAY] Target must be a dock host if it is empty");
            ASSERT_WITH_WINDOW(target, !target->DockParent,
                               "[ONYX][OVERLAY] Target cannot possibly have a dock parent if it is empty");

            // removed bc its seemingly irrelevant
            // sourceRoot->Flags = target->DockRoot->Flags;
            // we need to assign the _CanBeEmpty flag to a leaf node so that it can propagate upwards when detaching
            // windows. having it in the root or a parent node is irrelevant because parents are destroyed when
            // deletions occur
            if (sourceRoot->Flags & OverlayDockNodeFlag_CanBeEmpty)
            {
                DockNode *c = sourceRoot;
                while (!c->IsLeaf())
                    c = c->Children[0];
                c->Flags |= OverlayDockNodeFlag_CanBeEmpty;
            }
            removeDockNode(target->DockRoot);
            targetNode = sourceRoot;
            target->DockRoot = sourceRoot;
            // no need to set DockParent. this branch cannot happen if target started as a free window
            iterateDockTree(target->DockRoot, [&](DockNode *node) { node->Host = target; });
        }
        else
        {
            for (OverlayWindow *child : sourceRoot->Windows)
            {
                child->DockParent = targetNode;
                child->DockRoot = target->DockRoot;
                targetNode->Windows.Append(child);
            }
            removeDockNode(sourceRoot);
        }
        removeOverlayWindow(source);
        parent = targetNode;
    }
    else
    {
        parent = targetNode;
        if (source)
        {
            targetNode->Windows.Append(source);
            source->DockParent = targetNode;
            source->DockRoot = target->DockRoot;
        }
    }

    LOG_DOCK_TREE(target, "After insert");
    return parent;
}

void Overlay::dockInsertAndDrawPreview(OverlayWindow *target, RenderContext<D2> *ctx)
{
    const bool ownsNative = target->Flags & WindowInternalFlag_OwnsNative;
    NativeWindow *nw = target->GetNative();

    const f32v2 wpos = ownsNative ? nw->GetWorldTopLeft() : target->ToWorld(target->ScreenPos);
    const f32v2 &wsize = target->Size;

    const f32v2 &mpos = nw->WorldMouse;

    const bool mreleased = (nw->Flags | m_DockSource->GetNative()->Flags) & NativeWindowFlag_LeftMouseReleased;
    bool canDrawPreview = true;
    if (!target->IsRoot())
    {
        const OverlayWindow *p = target->Parent;
        const Layout *ly = p->GetActiveLayout();
        const LayoutElementQueryInfo *elm = ly->QueryElement(p->ContentAreaId);

        if (elm)
        {
            const f32v2 &cpos = elm->Position;
            const f32v2 &csize = elm->Size;
            const f32v2 c = cpos + 0.5f * csize;
            ctx->Clip(c, csize);
        }
    }
    const auto drawPreviewIfHoveredAndInsert = [&](DockNode *targetNode, const f32v2 &middle, const f32v2 &pos,
                                                   const f32v2 &hitSize, const f32v2 &regionSize, const i32v2 &loc,
                                                   const f32 sign) {
        if (!canDrawPreview)
            return false;

        const f32v2 relPos = Math::Absolute(mpos - pos);
        bool hovering = relPos[0] <= hitSize[0] && relPos[1] <= hitSize[1];
        if (!hovering && loc[s_Axis] == s_CenterAxis)
        {
            const Layout *ly = target->GetActiveLayout();
            hovering = ly->IsHovered(targetNode ? targetNode->TabData.Id : target->HeaderId, nw->WorldMouse);
        }

        if (hovering)
        {
            canDrawPreview = false;
            f32v2 offset = f32v2{0.f};
            f32v2 dims = 0.5f * regionSize;

            if (loc[s_Axis] != s_Horizontal)
                dims[0] = regionSize[0];
            else
                offset[0] = 0.25f * sign * regionSize[0];

            if (loc[s_Axis] != s_Vertical)
                dims[1] = regionSize[1];
            else
                offset[1] = 0.25f * sign * regionSize[1];

            const f32v2 previewPos = middle + offset;

            ctx->SetTranslation(0.f);
            f32m3 t = f32m3::Identity();
            Transform<D2>::ScaleExtrinsic(t, dims);
            Transform<D2>::TranslateExtrinsic(t, previewPos);
            ctx->Quad(t);

            if (mreleased)
            {
                dockInsert(targetNode, loc, 0.5f, m_DockSource, target);
                return true;
            }
        }
        return false;
    };

    const f32 previewGap = 6.f;
    const auto bottomPreviewInsert = [&](const i32v2 loc, DockNode *leaf, const f32v2 &dockPos, const f32v2 &dockSize) {
        const f32v2 hsize = 0.5f * dockSize;
        const f32v2 middle = dockPos + f32v2{hsize[0], -hsize[1]};

        const f32 previewSize = Math::Min(20.f, 0.1f * Math::Min(dockSize[0], dockSize[1]));
        const f32 previewRadius = 0.4f * previewSize;

        const f32 dpos = previewSize + previewGap + 2.f * previewRadius;
        const f32 hitSize = 0.5f * previewSize + previewRadius;

        const RoundedRectParameters params = {previewSize, previewSize, previewRadius};

        f32v2 offset{0.f};

        const f32 sign = f32(loc[s_Side]);
        if (loc[s_Axis] != s_CenterAxis)
            offset[loc[s_Axis]] = sign * dpos;

        f32v2 pos = middle + offset;

        ctx->SetTranslation(pos);
        ctx->RoundedRect(params);

        return drawPreviewIfHoveredAndInsert(leaf, middle, pos, hitSize, dockSize, loc, sign);
    };

    const f32v2 whsize = 0.5f * wsize;
    const auto topPreviewInsert = [&](const i32v2 loc, DockNode *targetNode) {
        const f32v2 middle = wpos + f32v2{whsize[0], -whsize[1]};

        const f32 mul = (!targetNode || targetNode->IsLeaf()) ? 0.1f : 0.04f;
        const f32 previewSize = Math::Min(20.f, mul * Math::Min(wsize[0], wsize[1]));
        const f32 previewRadius = 0.4f * previewSize;

        const f32 dpos = previewSize + previewGap + 2.f * previewRadius;

        f32v2 offset{0.f};

        const f32 sign = f32(loc[s_Side]);
        offset[loc[s_Axis]] = sign * (whsize[loc[s_Axis]] - 0.5f * dpos);

        f32v2 pos = middle + offset;
        const f32 xmul = loc[s_Axis] == s_Horizontal ? 0.5f : 2.f;
        const f32 ymul = loc[s_Axis] == s_Vertical ? 0.5f : 2.f;

        const f32v2 psize = f32v2{xmul, ymul} * previewSize;
        const RoundedRectParameters params = {psize[0], psize[1], previewRadius};

        ctx->SetTranslation(pos);
        ctx->RoundedRect(params);

        const f32v2 hitSize = 0.5f * psize + previewRadius;
        return drawPreviewIfHoveredAndInsert(targetNode, middle, pos, hitSize, wsize, loc, sign);
    };

    constexpr TKit::FixedArray<i32v2, 4> sides = {s_Left, s_Right, s_Bottom, s_Top};
    ctx->FillColor(m_Style[OverlayColor_DockPreview]);

    const auto insertAllBottoms = [&](DockNode *node, const f32v2 &pos, const f32v2 &size, const bool onlyCenter) {
        if (!onlyCenter)
            for (const i32v2 &s : sides)
                if (bottomPreviewInsert(s, node, pos, size))
                    return true;

        const bool sourceIsALeaf = !m_DockSource->DockRoot || m_DockSource->DockRoot->IsLeaf();
        const bool targetIsEmpty =
            target->DockRoot && target->DockRoot->IsLeaf() && target->DockRoot->Windows.IsEmpty();
        if (sourceIsALeaf || targetIsEmpty)
            return bottomPreviewInsert(s_Center, node, pos, size);
        return false;
    };
    const auto insertAllTops = [&] {
        for (const i32v2 &s : sides)
            if (topPreviewInsert(s, target->DockRoot))
                return true;
        return false;
    };

    if (!target->DockRoot || target->DockRoot->IsLeaf())
    {
        if (!insertAllBottoms(target->DockRoot, wpos, wsize, true))
            insertAllTops();
        return;
    }

    if (insertAllTops())
        return;

    iterateDockTree(target->DockRoot, [&](DockNode *node) {
        const f32v2 &pos = node->ReadOnlyPosition;
        const f32v2 &size = node->ReadOnlySize;
        if (!node->IsLeaf())
            return true;

        const f32v2 relPos = Math::Absolute(mpos - pos);
        if (relPos[0] <= size[0] && relPos[1] <= size[1])
        {
            insertAllBottoms(node, pos, size, false);
            return false;
        }
        return true;
    });
}

void Overlay::applyDockTrees()
{
    for (const DockTreeDescription &tree : m_DockTrees)
        ApplyDockTree(tree.HostId, tree.Root);
    m_DockTrees.Clear();
}

/////////////////////////////////////////////
/// END DOCKING
/////////////////////////////////////////////

/////////////////////////////////////////////
/// WIDGETS
/////////////////////////////////////////////

bool Overlay::Button(const OverlayLabel label, const OverlayButtonFlags flags)
{
    Layout *ly = m_Active->GetActiveLayout();
    const LayoutId id = PushId(label.Id);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly->QueryElement(id));

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

    m_LastItem = ly->BeginPanel(id, LyPnPar{.FillColor = m_Style[col],
                                            .Alignment = Center,
                                            .Sizing = sizing,
                                            .Shape = rect(m_Style[OverlayStyle_ButtonRadius]),
                                            .Padding = padding});

    ly->Text(ly->GenerateNextId(), label.Title, getTextParams());
    ly->EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

bool Overlay::RadioButton(const OverlayLabel label, const bool active)
{
    Layout *ly = m_Active->GetActiveLayout();
    const LayoutId id = PushId(label.Id);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly->QueryElement(id));

    OverlayColor col = OverlayColor_CheckBoxIdle;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_CheckBoxPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_CheckBoxHovered;

    m_LastItem = ly->BeginPanel(
        id, LyPnPar{.Alignment = CenterLeft, .Sizing = fit(), .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly->BeginPanel(LyPnPar{.FillColor = m_Style[col],
                           .Alignment = Center,
                           .Sizing = sabs(m_Style[OverlayStyle_WidgetSize]),
                           .Shape = circle(),
                           .Padding = 6.f});

    ly->Panel(LyPnPar{.FillColor = active ? m_Style[OverlayColor_CheckBoxInner] : Color_Transparent,
                      .Sizing = grow(),
                      .Shape = circle()});
    ly->EndPanel();

    ly->Text(ly->GenerateNextId(), label.Title, getTextParams());

    ly->EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

// NOTE(Isma): Much repetition with radio button here
bool Overlay::CheckBox(const OverlayLabel label, bool *enable)
{
    Layout *ly = m_Active->GetActiveLayout();
    const LayoutId id = PushId(label.Id);

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly->QueryElement(id));

    OverlayColor col = OverlayColor_CheckBoxIdle;
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_CheckBoxPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_CheckBoxHovered;

    if (focusFlags & OverlayFocusQueryFlag_LeftClicked)
        *enable = !*enable;

    m_LastItem = ly->BeginPanel(
        id, LyPnPar{.Alignment = CenterLeft, .Sizing = fit(), .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly->BeginPanel(LyPnPar{.FillColor = m_Style[col],
                           .Alignment = Center,
                           .Sizing = sabs(m_Style[OverlayStyle_WidgetSize]),
                           .Shape = rect(m_Style[OverlayStyle_CheckBoxRadius]),
                           .Padding = 6.f});

    if (*enable)
        ly->Panel(LyPnPar{.FillColor = m_Style[OverlayColor_CheckBoxInner],
                          .Sizing = grow(),
                          .Shape = rect(m_Style[OverlayStyle_CheckBoxRadius])});
    ly->EndPanel();

    ly->Text(ly->GenerateNextId(), label.Title, getTextParams());

    ly->EndPanel();
    PopId();
    return focusFlags & OverlayFocusQueryFlag_LeftClicked;
}

bool Overlay::BeginSelectable(LayoutId id, const bool enabled, const OverlaySelectableFlags flags)
{
    Layout *ly = m_Active->GetActiveLayout();
    id = PushId(id);
    const LayoutElementQueryInfo *elm = ly->QueryElement(id);

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

    m_LastItem = ly->BeginPanel(id, LyPnPar{.FillColor = m_Style[col],
                                            .Direction = LayoutDirection_RightToLeft,
                                            .Alignment = CenterLeft,
                                            .Sizing = sizing,
                                            .Shape = rect(m_Style[OverlayStyle_SelectableRadius])});

    const f32 padding = m_Style[OverlayStyle_WidgetPadding];
    if (cb)
    {
        ly->BeginPanel(LyPnPar{.FillColor = m_Style[col],
                               .Alignment = Center,
                               .Sizing = {sabs(m_Style[OverlayStyle_WidgetSize]), flex()},
                               .Shape = rect(m_Style[OverlayStyle_SelectableCheckBoxRadius]),
                               .Padding = padding});

        if (enabled)
            ly->Panel(LyPnPar{.FillColor = m_Style[OverlayColor_CheckBoxInner],
                              .Sizing = grow(),
                              .Shape = rect(m_Style[OverlayStyle_SelectableCheckBoxRadius])});
        ly->EndPanel();
    }

    const bool ltr = flags & OverlaySelectableFlag_LeftToRight;
    ly->BeginPanel(LyPnPar{.Direction = ltr ? LayoutDirection_LeftToRight : LayoutDirection_TopToBottom,
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
    Layout *ly = m_Active->GetActiveLayout();
    PopId();
    ly->EndPanel();
    ly->EndPanel();
}

bool Overlay::Selectable(const OverlayLabel label, const bool enabled, const OverlaySelectableFlags flags)
{
    const bool selected = BeginSelectable(label.Id, enabled, flags);
    Layout *ly = m_Active->GetActiveLayout();
    ly->Text(ly->GenerateNextId(), label.Title, getTextParams());
    EndSelectable();

    return selected;
}

bool Overlay::Selectable(const OverlayLabel label, bool *enabled, const OverlaySelectableFlags flags)
{
    if (Selectable(label, *enabled, flags))
    {
        *enabled = !*enabled;
        return true;
    }
    return false;
}

void Overlay::ProgressBar(const OverlayLabel label, const TKit::StringView text, const f32 pct)
{
    beginHorizontalWidget(PushId(label.Id));

    Layout *ly = m_Active->GetActiveLayout();

    const f32 padding = m_Style[OverlayStyle_WidgetPadding];
    const f32 lh = getLineHeight() + 2.f * padding;
    ly->BeginPanel(LyPnPar{
        .FillColor = m_Style[OverlayColor_ProgressBarBackground], .Alignment = Center, .Sizing = {flex(), fit(lh)}});

    const bool isIndeterminate = pct < 0.f;
    const f32 idetSize = 0.4f;
    ly->Panel(
        LyPnPar{.FillColor = m_Style[OverlayColor_ProgressBarInner],
                .Sizing = snorm({isIndeterminate ? idetSize : Math::Clamp(pct, 0.f, 1.f), 1.f}),
                .SelfOffset = onorm({isIndeterminate ? (Math::Modulo(-pct, 1.f + idetSize) - idetSize) : 0.f, 0.f}),
                .Floating = {.Enable = true,
                             .DrawOnTop = false,
                             .Clip = true,
                             .Attachment = {Alignment_Left, Alignment_Top},
                             .Alignment = TopLeft}});

    if (!text.IsEmpty())
        ly->Text(ly->GenerateNextId(), text, getTextParams());

    ly->EndPanel();

    endHorizontalWidget(label.Title);
    PopId();
}

void Overlay::BeginTabBar(const LayoutId id, const OverlayTabBarFlags flags)
{
    // actually we can now
    // TKIT_ASSERT(!m_CurrentTabBar, "[ONYX][OVERLAY] A tab bar is already opened. Cannot nest two tab bars in one");
    const LayoutId tid = PushId(id);
    TabBarData *data = &m_TabBarData[tid];
    m_TabBarStack.Append(tid);
    beginTabBar(data, tid, flags);
}

void Overlay::EndTabBar()
{
    ASSERT_WITH_WINDOW(m_Active, !m_TabBarStack.IsEmpty(),
                       "[ONYX][OVERLAY] Cannot end a tab bar without starting one to begin with");
    endTabBar(&m_TabBarData[m_TabBarStack.GetBack()]);
    PopId();
    m_TabBarStack.Pop();
}

bool Overlay::BeginTab(const OverlayLabel label, bool *enabled, const OverlayTabFlags flags)
{
    ASSERT_WITH_WINDOW(m_Active, !m_TabBarStack.IsEmpty(),
                       "[ONYX][OVERLAY] Tabs can only be created inside an active tab bar");
    return beginTab(&m_TabBarData[m_TabBarStack.GetBack()], label, enabled, flags);
}
bool Overlay::InputText(const OverlayLabel label, char *buf, const u32 size, const TKit::StringView hint,
                        const OverlayInputFlags flags)
{
    beginHorizontalWidget(PushId(label.Id));
    const bool updated = inputTextBox(buf, size, hint, flags);
    endHorizontalWidget(label.Title);
    PopId();
    return updated;
}

void Overlay::ColorPreviewTooltip(const TKit::StringView title, const Color &col, const OverlayColorFlags flags)
{
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);
    const bool tlabel = !(flags & OverlayColorFlag_NoTooltipLabel);
    const f32 tooltipSize = m_Style[OverlayStyle_ColorTooltipSize];

    Layout *ly = m_Active->GetActiveLayout();
    const bool info = !(flags & OverlayColorFlag_NoTooltipColorInfo);
    if (info)
    {
        if (tlabel)
        {
            ly->BeginPanel({.Direction = LayoutDirection_TopToBottom,
                            .Alignment = CenterLeft,
                            .Sizing = fit(),
                            .ChildGap = m_Style[OverlayStyle_ChildGap]});

            ly->Text(ly->GenerateNextId(), title, getTextParams());
            HorizontalLine();
        }

        ly->BeginPanel({.Direction = LayoutDirection_LeftToRight,
                        .Alignment = CenterLeft,
                        .Sizing = fit(),
                        .ChildGap = m_Style[OverlayStyle_ChildGap]});

        drawColorPreview(col, tooltipSize, alpha);

        ly->BeginPanel({.Direction = LayoutDirection_TopToBottom,
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
        ly->EndPanel();
        ly->EndPanel();
        if (tlabel)
            ly->EndPanel();
    }
    else
    {
        ly->BeginPanel({.Direction = LayoutDirection_LeftToRight,
                        .Alignment = CenterLeft,
                        .Sizing = fit(),
                        .ChildGap = m_Style[OverlayStyle_ChildGap]});
        drawColorPreview(col, tooltipSize, alpha);
        if (tlabel)
            ly->Text(ly->GenerateNextId(), title, getTextParams());
        ly->EndPanel();
    }
}

void Overlay::ColorPreview(const OverlayLabel label, const Color &col, const OverlayColorFlags flags)
{
    const f32 previewSize = m_Style[OverlayStyle_ColorPreviewSize];

    PushId(label.Id);
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);

    const LayoutId id = drawColorPreview(col, previewSize, alpha);
    m_LastItem = id;

    const bool tooltip = !(flags & OverlayColorFlag_NoTooltip);
    if (tooltip && BeginItemTooltip(OverlayHoveredFlag_ShortDelay))
    {
        ColorPreviewTooltip(label.Title, col, flags);
        EndTooltip();
    }

    m_LastItem = id;
    PopId();
}
bool Overlay::ColorPicker(const OverlayLabel label, const OverlayColorHandle color, const Color *original,
                          const f32 size, const OverlayColorFlags flags)
{
    PushId(label.Id);
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

bool Overlay::ColorButton(const OverlayLabel label, const OverlayColorHandle color, const OverlayColorFlags flags)
{
    f32 *colPtr = color.Data;
    const bool alpha = !(flags & OverlayColorFlag_NoAlpha);
    const Color col =
        alpha ? Color{colPtr[0], colPtr[1], colPtr[2], colPtr[3]} : Color{colPtr[0], colPtr[1], colPtr[2]};

    ColorPreview(label, col, flags);
    bool changed = false;

    const LayoutId id = IdFromStack(label.Id);
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

bool Overlay::ColorEditor(const OverlayLabel label, const OverlayColorHandle color, const OverlayColorFlags flags)
{
    f32 *colPtr = color.Data;
    const bool inputs = !(flags & OverlayColorFlag_NoInput);

    if (inputs)
        beginHorizontalWidget(PushId(label.Id));
    else
        beginHorizontalWidget(PushId(label.Id), fit(), fit());

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
    const LayoutId oldItem = m_LastItem;

    if (!(flags & OverlayColorFlag_NoPreview))
    {
        PushStyleVar(OverlayStyle_ColorPreviewSize, lh);
        PushStyleVar(OverlayStyle_ColorTooltipSize, 2.f * lh);
        ColorButton(label, color, flags);
        PopStyleVar(2);
    }

    m_LastItem = oldItem;

    endHorizontalWidget(label.Title);
    PopId();

    if (!(flags & OverlayColorFlag_NoDragDrop))
    {
        if (BeginDragDropSource())
        {
            SetDragDropPayload("__onyx_id_Color", colPtr, count);
            PushStyleVar(OverlayStyle_ColorTooltipSize, m_Style[OverlayStyle_ColorDragTooltipSize]);
            ColorPreviewTooltip(label.Title, col, OverlayColorFlag_NoTooltipColorInfo);
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

    Layout *ly = m_Active->GetActiveLayout();
    bool changed = false;
    for (u32 i = 0; i < count; ++i)
    {
        PushId(i);

        if (markers)
        {
            ly->BeginPanel(LyPnPar{.Alignment = CenterLeft, .Sizing = {flex(), fit()}});
            ly->Panel(LyPnPar{.FillColor = colors[i % 4], .Sizing = {sabs(2.f), grow()}});
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
            ly->EndPanel();

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
bool Overlay::colorPicker(const OverlayLabel label, f32 *colPtr, const Color &col, const Color *original,
                          const OverlayColorFlags flags, f32 pickerSize)
{
    Layout *ly = m_Active->GetActiveLayout();

    const LayoutId outerId = IdFromStack("__onyx_id_Outer_picker");
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

    const LayoutElementQueryInfo *pickerElm = ly->QueryElement(pickerId);
    const LayoutElementQueryInfo *hueBarElm = ly->QueryElement(hueBarId);
    const LayoutElementQueryInfo *alphaBarElm = ly->QueryElement(alphaBarId);

    const FocusFlags pickerFlags = queryAndSetFocusStatus(pickerElm, FocusFlag_PressedEvenWhenAwayFromHover);
    const FocusFlags hueBarFlags = queryAndSetFocusStatus(hueBarElm, FocusFlag_PressedEvenWhenAwayFromHover);
    const FocusFlags alphaBarFlags = queryAndSetFocusStatus(alphaBarElm, FocusFlag_PressedEvenWhenAwayFromHover);

    constexpr f32 barWidth = 32.f;

    const bool drawPreview = !(flags & OverlayColorFlag_NoPreview);
    const f32 previewSize = m_Style[OverlayStyle_ColorPickerPreviewSize];

    const f32 cgap = m_Style[OverlayStyle_ChildGap];
    const bool inferPickerSize = pickerSize < 0.f;
    if (inferPickerSize)
    {
        const LayoutElementQueryInfo *elm = ly->QueryElement(pickerId);
        pickerSize = elm ? elm->Size[0] : m_Active->Size[0];
    }

    f32 circleSize = 32.f;

    const f32 rodHeight = 4.f;

    const f32 posOffset = 0.5f * pickerSize;

    const NativeWindow *nw = m_Active->GetNative();
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

    ly->BeginPanel(outerId, LyPnPar{.Direction = LayoutDirection_LeftToRight,
                                    .Alignment = TopLeft,
                                    .Sizing = {inferPickerSize ? grow() : fit(), fit()},
                                    .ChildGap = cgap});

    ly->BeginPanel(pickerId, LyPnPar{.FillColor = Color_White,
                                     .Alignment = Center,
                                     .Sizing = {inferPickerSize ? grow() : sabs(pickerSize), sabs(pickerSize)},
                                     .Shape = dynamic(pdata->PickerQuad->Handle)});

    ly->BeginPanel(LyPnPar{.FillColor = Color_White,
                           .Alignment = Center,
                           .Sizing = sabs(circleSize),
                           .SelfOffset = oabs(pdata->CirclePos),
                           .Shape = circle()});

    ly->Panel(LyPnPar{.FillColor = Color{col, 1.f}, .Sizing = sabs(circleSize - 4.f), .Shape = circle()});

    ly->EndPanel();

    ly->EndPanel();

    ly->BeginPanel(hueBarId, LyPnPar{.FillColor = Color_White,
                                     .Alignment = Center,
                                     .Sizing = sabs({barWidth, pickerSize}),
                                     .Shape = dynamic(pdata->HueBar->Handle),
                                     .ChildOverflow = LayoutOverflow_Spill});

    ly->Panel(LyPnPar{.FillColor = Color_White,
                      .Sizing = {srel(1.2f), sabs(rodHeight)},
                      .SelfOffset = oabs({0.f, pdata->HueRodPos})});

    ly->EndPanel();

    if (alpha)
    {
        constexpr u32 tileCount = 16;
        const f32 hsize = 0.5f * barWidth;
        const f32 vsize = pickerSize / f32(tileCount);

        const LySz2 barSizing = sabs({barWidth, pickerSize});
        const LySz2 stripSizing = sabs({hsize, pickerSize});
        const LySz2 tileSizing = sabs({hsize, vsize});

        constexpr TKit::FixedArray<f32, 2> checkboard = {s_CheckboardLight, s_CheckboardDark};

        ly->BeginPanel(LyPnPar{
            .Direction = LayoutDirection_LeftToRight, .Sizing = barSizing, .ChildOverflow = LayoutOverflow_Spill});

        // left horizontal checkboard strip
        ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom, .Sizing = stripSizing});

        for (u32 i = 0; i < tileCount; ++i)
            ly->Panel(LyPnPar{.FillColor = Color{checkboard[i & 1]}, .Sizing = tileSizing});

        ly->EndPanel();

        // right horizontal checkboard strip
        ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom, .Sizing = stripSizing});

        for (u32 i = 0; i < tileCount; ++i)
            ly->Panel(LyPnPar{.FillColor = Color{checkboard[(i + 1) & 1]}, .Sizing = tileSizing});

        ly->EndPanel();

        ly->BeginPanel(alphaBarId, LyPnPar{.FillColor = Color_White,
                                           .Alignment = Center,
                                           .Sizing = barSizing,
                                           .SelfOffset = oabs({-barWidth, 0.f}),
                                           .Shape = dynamic(pdata->AlphaBar->Handle),
                                           .ChildOverflow = LayoutOverflow_Spill,
                                           .ForceBlend = true});

        ly->Panel(LyPnPar{.FillColor = Color_White,
                          .Sizing = {srel(1.2f), sabs(rodHeight)},
                          .SelfOffset = oabs({0.f, pdata->AlphaRodPos})});

        ly->EndPanel();
        ly->EndPanel();
    }

    ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom,
                           .Alignment = TopLeft,
                           .Sizing = fit(),
                           .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly->Text(ly->GenerateNextId(), original ? "Current" : label.Title, getTextParams());
    if (drawPreview)
    {
        PushStyleVar(OverlayStyle_ColorPreviewSize, previewSize);
        PushStyleVar(OverlayStyle_ColorTooltipSize, m_Style[OverlayStyle_ColorPickerTooltipSize]);
        ColorPreview(label, col, flags);
        if (original)
        {
            ly->Text(ly->GenerateNextId(), "Original", getTextParams());
            ColorPreview("Original", *original, flags);
        }
        PopStyleVar(2);
    }
    ly->EndPanel();

    ly->EndPanel();

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

LayoutId Overlay::drawColorPreview(const Color &col, const f32 size, bool alpha)
{
    Layout *ly = m_Active->GetActiveLayout();
    if (alpha)
    {
        const f32 hsize = 0.5f * size;
        ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom, .Alignment = TopLeft, .Sizing = sabs(size)});

        // top horizontal checkboard strip
        ly->BeginPanel(LyPnPar{.Sizing = sabs({size, hsize})});

        ly->Panel(LyPnPar{.FillColor = Color{s_CheckboardLight}, .Sizing = sabs(hsize)});
        ly->Panel(LyPnPar{.FillColor = Color{s_CheckboardDark}, .Sizing = sabs(hsize)});

        ly->EndPanel();

        // bottom horizontal checkboard strip
        ly->BeginPanel(LyPnPar{.Sizing = sabs({size, hsize})});

        ly->Panel(LyPnPar{.FillColor = Color{s_CheckboardDark}, .Sizing = sabs(hsize)});
        ly->Panel(LyPnPar{.FillColor = Color{s_CheckboardLight}, .Sizing = sabs(hsize)});

        ly->EndPanel();

        const LayoutId id = ly->Panel(IdFromStack("__onyx_id_Preview"),
                                      {.FillColor = col, .Sizing = sabs(size), .SelfOffset = oabs({0.f, size})});

        ly->EndPanel();
        return id;
    }
    return ly->Panel(IdFromStack("__onyx_id_Preview"), {.FillColor = Color{col, 1.f}, .Sizing = sabs(size)});
}

void Overlay::beginTabBar(TabBarData *data, const LayoutId id, const OverlayTabBarFlags flags)
{
    const LySz2 scrollSizing = {isAutoResize() ? fit() : grow(), fit()};

    Layout *ly = m_Active->GetActiveLayout();
    ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom,
                           .Alignment = CenterLeft,
                           .Sizing = {grow(), fit()},
                           .Shape = rect(m_Style[OverlayStyle_ScrollAreaBorderRadius])});

    // the id that will need to be opened by tab items to keep appending

    data->Flags = flags;
    data->Id = beginScroll({.Id = id,
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

    ly->EndPanel();
}

void Overlay::endTabBar(TabBarData *data, DockNode *node)
{
    Layout *ly = m_Active->GetActiveLayout();
    ly->OpenPanel(data->Id);

    PushId(data->Id);
    if (data->Flags & TabBarFlag_ForDocking)
    {
        ASSERT_WITH_WINDOW(
            node->Host, node && node->IsLeaf(),
            "[ONYX][OVERLAY] If the _ForDocking tab flag is set, endTabBar() must take a non null dock node leaf");

        const OverlayHoverQueryFlags focusFlags =
            iconButtonFocus(IdFromStack("__onyx_id_Tab_header_button"), ArrowDownIcon, grow());

        if ((focusFlags & OverlayFocusQueryFlag_DragSource) && node->CanUndock())
        {
            if (node->Windows.GetSize() != 1)
                node->Flags |= DockNodeFlag_MustUndock | DockNodeFlag_MustGrabWhenUndocked;
            else if (focusFlags & OverlayFocusQueryFlag_DragSource)
                node->Windows[0]->Flags |= WindowInternalFlag_MustUndock | WindowInternalFlag_MustGrabWhenUndocked;
        }
    }

    PushStyleVar(OverlayStyle_SelectableRadius, m_Style[OverlayStyle_TabRadius]);
    PushStyleVar(OverlayStyle_WidgetPadding, m_Style[OverlayStyle_TabPadding]);

    struct Permutation
    {
        u32 Order1;
        u32 Order2;
        bool Permuted = false;
    };

    Permutation perm{};
    auto &order = data->Order;
    auto &tabs = data->Tabs;

    const bool reorderable = data->Flags & OverlayTabBarFlag_Reorderable;

    const NativeWindow *nw = m_Active->GetNative();
    const f32 mydelta = Math::Absolute(nw->WorldMouse[1] - nw->WorldMouseOnPress[1]);
    const f32 th = m_Style[OverlayStyle_DragThreshold];
    const bool ydragging = mydelta > th;

    for (u32 i = 0; i < order.GetSize(); ++i)
    {
        const u32 idx = reorderable ? order[i] : i;
        Tab &tab = tabs[idx];

        const bool opened = data->OpenId == tab.Id;

        const auto lookForAnotherOpenTab = [&] {
            bool found = false;
            for (u32 i = idx + 1; i < tabs.GetSize(); ++i)
                if (tabs[i].Flags & TabFlag_Enabled)
                {
                    data->OpenId = tabs[i].Id;
                    found = true;
                    break;
                }

            if (!found)
                for (u32 i = 0; i < idx; ++i)
                    if (tabs[i].Flags & TabFlag_Enabled)
                    {
                        data->OpenId = tabs[i].Id;
                        break;
                    }
        };

        if (!(tab.Flags & TabFlag_Enabled))
        {
            if (opened)
                lookForAnotherOpenTab();
            continue;
        }

        const bool button = tab.Flags & TabFlag_DrawCloseButton;

        if (button)
            ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight, .Alignment = CenterLeft, .Sizing = fit()});

        if (BeginSelectable(IdFromStack(tab.Id), opened,
                            OverlaySelectableFlag_SpanLabelWidth | OverlaySelectableFlag_LeftToRight))
        {
            if (tab.Flags & TabFlag_Unselectable)
                tab.Flags &= ~TabFlag_Unselectable;
            else
                data->OpenId = tab.Id;
        }

        ly->Text(ly->GenerateNextId(), tab.Title, getTextParams());
        EndSelectable();

        PushId(tab.Id);
        const LayoutId butId = IdFromStack("__onyx_id_Tab_close");
        PopId();

        const LayoutElementQueryInfo *elm = ly->QueryElement(m_LastItem);
        const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm);

        const bool dragSource = focusFlags & OverlayFocusQueryFlag_DragSource;
        const bool canUndock = tab.Window && !(tab.Window->Flags & OverlayWindowFlag_NoUndocking);

        if (dragSource && reorderable)
            tab.Flags |= TabFlag_Unselectable;

        if (dragSource && canUndock && ydragging)
            tab.Window->Flags |= WindowInternalFlag_MustUndock | WindowInternalFlag_MustGrabWhenUndocked;

        if (reorderable && !perm.Permuted && dragSource)
        {
            const LayoutElementQueryInfo *belm = button ? ly->QueryElement(butId) : nullptr;

            const f32 cgap = 0.5f * m_Style[OverlayStyle_ChildGap];
            const f32 mpos = nw->WorldMouse[0];

            const f32 pos = elm->Position[0] - cgap;
            const f32 size = elm->Size[0] + (belm ? belm->Size[0] : 0.f) + cgap;

            if (!(tab.Flags & TabFlag_JustPermuted))
            {
                const u32 lastIndex = order.GetSize() - 1;
                if (mpos > pos + size)
                {
                    if (i == lastIndex && canUndock)
                        tab.Window->Flags |= WindowInternalFlag_MustUndock | WindowInternalFlag_MustGrabWhenUndocked;
                    else if (i != lastIndex)
                    {
                        tab.Flags |= TabFlag_JustPermuted;
                        perm.Permuted = true;
                        perm.Order1 = i;
                        perm.Order2 = i + 1;
                    }
                }
                else if (mpos < pos)
                {
                    if (i == 0 && canUndock)
                        tab.Window->Flags |= WindowInternalFlag_MustUndock | WindowInternalFlag_MustGrabWhenUndocked;
                    else if (i != 0)
                    {
                        tab.Flags |= TabFlag_JustPermuted;
                        perm.Permuted = true;
                        perm.Order1 = i;
                        perm.Order2 = i - 1;
                    }
                }
            }
            else if (mpos <= pos + size && mpos > pos)
                tab.Flags &= ~TabFlag_JustPermuted;
        }

        if (button && iconButton(butId, CrossIcon, grow(), OverlayColor_SelectableIdle))
        {
            tab.Flags |= TabFlag_RequestClose;
            if (opened)
                lookForAnotherOpenTab();
        }

        if (button)
            ly->EndPanel();
    }
    PopId();

    if (perm.Permuted)
        std::swap(data->Order[perm.Order1], data->Order[perm.Order2]);

    for (Tab &tab : data->Tabs)
        tab.Flags &= ~TabFlag_Enabled;

    PopStyleVar(2);

    ly->EndPanel();

    if (!(data->Flags & OverlayTabBarFlag_NoBottomLine))
        HorizontalLine();
}

bool Overlay::beginTab(TabBarData *data, const OverlayLabel label, bool *enabled, const OverlayTabFlags flags,
                       OverlayWindow *window)
{
    const bool forDocking = flags & TabFlag_ForDocking;
    const LayoutId tabId = forDocking ? label.Id : IdFromStack(label.Id);

    u32 idx = data->GetTabById(tabId);
    m_LastItem = label.Id;

    if (idx == TKIT_U32_MAX)
    {
        idx = data->Tabs.GetSize();
        data->Tabs.Append(tabId, window, TKit::TierString{label.Title.GetData(), label.Title.GetSize()}, flags);
        data->Order.Append(idx);
    }

    Tab &tab = data->Tabs[idx];
    tab.Id = tabId;
    Layout *ly = m_Active->GetActiveLayout();

    const bool mustStartOpen = (flags & OverlayTabFlag_StartOpen) && data->OpenId == NullLayoutId;
    const bool opened = data->OpenId == tabId || mustStartOpen;
    const bool pushId = !(flags & OverlayTabFlag_NoPushId);

    if (mustStartOpen)
        data->OpenId = tabId;

    if (tab.Flags & TabFlag_Enabled)
    {
        if (opened)
        {
            if (pushId)
                PushId(tabId);
            ly->OpenPanel(tabId);
            data->Current = idx;
            return true;
        }
        return false;
    }

    tab.Flags |= flags;
    if (enabled)
    {
        tab.Flags |= TabFlag_DrawCloseButton;
        if (!*enabled)
            return false;

        if (tab.Flags & TabFlag_RequestClose)
        {
            tab.Flags &= ~TabFlag_RequestClose;
            *enabled = false;
        }
    }
    else
        tab.Flags &= ~TabFlag_DrawCloseButton;

    tab.Flags |= TabFlag_Enabled;
    if (data->OpenId != tabId)
        return false;

    if (pushId)
        PushId(tabId);
    TKIT_ASSERT(forDocking == bool(window),
                "[ONYX][OVERLAY] If the _ForDocking tab flag is set, beginTab() must take a non null window");
    ly->BeginPanel(tabId, LyPnPar{.Direction = LayoutDirection_TopToBottom,
                                  .Alignment = TopLeft,
                                  .Sizing = {isAutoResize() ? fit() : grow(), forDocking ? grow() : fit()},
                                  .ChildGap = m_Style[OverlayStyle_ChildGap]});

    data->Current = idx;
    return true;
}

void Overlay::endTab(TabBarData *data)
{
    if (!(data->Tabs[data->Current].Flags & OverlayTabFlag_NoPushId))
        PopId();
    m_Active->GetActiveLayout()->EndPanel();
}

void Overlay::beginHorizontalWidget(const LayoutId id, const LySz2 &outerSizing, const LySz2 &innerSizing)
{
    Layout *ly = m_Active->GetActiveLayout();
    m_LastItem = ly->BeginPanel(
        id, LyPnPar{.Alignment = CenterLeft, .Sizing = outerSizing, .ChildGap = m_Style[OverlayStyle_ChildGap]});

    ly->BeginPanel(LyPnPar{.Alignment = CenterLeft, .Sizing = innerSizing, .ChildGap = m_Style[OverlayStyle_ChildGap]});
}
void Overlay::beginHorizontalWidget(const LayoutId id, const f32 normSize)
{
    const bool autoResize = isAutoResize();
    const bool isFloat = m_CurrentPopupDepth != m_Active->PopupDepth;
    const f32 wsize = m_Active->Size[0];
    const f32 effW = isFloat ? TKIT_F32_MAX : (normSize * wsize);

    const f32 mnw = m_Style[OverlayStyle_WidgetMinimumWidth];

    const LySz2 outerSizing = {autoResize ? fit(mnw) : grow(), fit()};
    const LySz2 innerSizing = {autoResize ? fit(mnw) : flex(0.f, Math::Max(mnw, effW)), fit()};

    return beginHorizontalWidget(id, outerSizing, innerSizing);
}
void Overlay::endHorizontalWidget(const TKit::StringView title)
{
    Layout *ly = m_Active->GetActiveLayout();
    ly->EndPanel();
    if (!title.IsEmpty())
        ly->Text(ly->GenerateNextId(), title, getTextParams());
    ly->EndPanel();
}
bool Overlay::inputTextBox(char *buf, const u32 capacity, const TKit::StringView hint, const OverlayInputFlags flags,
                           const InputConvertInfoFlags cflags)
{
    TKIT_ASSERT(capacity != 0, "[ONYX][OVERLAY] Buffer capacity for text input cannot be zero");
    Layout *ly = m_Active->GetActiveLayout();
    const u32 bufSize = u32(std::strlen(buf));
    TKIT_ASSERT(bufSize < capacity,
                "[ONYX][OVERLAY] The input character length ({}) must be lower than buffer capacity ({}) as the latter "
                "must to account for the null terminator",
                bufSize, capacity);

    const LayoutId iboxId = IdFromStack("__onyx_id_Input_box");
    const LayoutElementQueryInfo *ibox = ly->QueryElement(iboxId);
    const bool mustConvert = cflags & InputConvertFlag_MustConvert;

    FocusFlags fflags = FocusFlag_ClickedOnMousePress | FocusFlag_KeepActiveOnRelease | FocusFlag_KeepActiveOnPressed |
                        FocusFlag_ActiveAllowsInteraction | FocusFlag_PressedEvenWhenAwayFromHover;
    if (mustConvert)
        fflags |= FocusFlag_AllowPressedOnEnter;

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ibox, fflags);

    ly->BeginPanel(iboxId, LyPnPar{.FillColor = m_Style[OverlayColor_InputBackground],
                                   .Alignment = CenterLeft,
                                   .Sizing = {grow(), fit()},
                                   .Shape = rect(m_Style[OverlayStyle_InputBoxRadius]),
                                   .Padding = m_Style[OverlayStyle_WidgetPadding]});

    // This input text box may be a "converted" slider/drag. this means that the first frame (where layout
    // element querying is not available) must be valid in the sense that we need valid queries. boxPos is very
    // important. without it, we cannot auto highlight. so we try this proxy, by trying to get the position of
    // the parent box if thats the case
    const LayoutElementQueryInfo *box =
        mustConvert ? ly->QueryElement(IdFromStack("__onyx_id_Drag/Slider_hbox")) : ibox;
    const f32 boxSize = ibox ? (ibox->Size[0] - 2.f * m_Style[OverlayStyle_WidgetPadding]) : 0.f;

    const FontData &fdata = getFontData();
    const f32 fs = m_Style[OverlayStyle_FontSize];

    LyTxPar tparams = getTextParams();
    const bool pressed = focusFlags & OverlayFocusQueryFlag_Pressed;
    const bool hovered = focusFlags & OverlayFocusQueryFlag_Hovered;
    if (pressed || hovered)
        m_Active->Flags |= WindowInternalFlag_InputHovered;

    bool updated = false;
    if ((focusFlags & OverlayFocusQueryFlag_Active) || mustConvert)
    {
        m_StateFlags |= StateFlag_RequestCaptureKeyboard | StateFlag_WantCaptureKeyboard;

        NativeWindow *nw = m_Active->GetNative();
        const bool ctrl = nw->Window->IsKeyPressed(Key_LeftControl);

        TKit::TierString &str = m_InputWidgetBuffer;
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
            m_RedoStack.Append(m_CursorStart, m_CursorEnd, str);

            m_CursorStart = info.CursorStart;
            m_CursorEnd = info.CursorEnd;
            str = info.Text;

            m_UndoStack.Pop();
            updated = true;
        }
        if (undoRedo && ctrl && !m_RedoStack.IsEmpty() && nw->EventKeys[Key_Y])
        {
            const TextInputStateInfo &info = m_RedoStack.GetBack();
            m_UndoStack.Append(m_CursorStart, m_CursorEnd, str);

            m_CursorStart = info.CursorStart;
            m_CursorEnd = info.CursorEnd;
            str = info.Text;

            m_RedoStack.Pop();
            updated = true;
        }

        const auto pushUndo = [&] {
            if (undoRedo)
            {
                m_UndoStack.Append(m_CursorStart, m_CursorEnd, str);
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
            ly->Text(ly->GenerateNextId(), hint, tparams);
        }
        else
            ly->Text(ly->GenerateNextId(), str, tparams);

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
        ly->Panel(LyPnPar{.FillColor = Color{m_Style[OverlayColor_InputCursor], m_Style[OverlayStyle_CursorOpacity]},
                          .Sizing = {sabs(cwidth), grow()},
                          .SelfOffset = oabs({offset, 0.f})});
        if (hasHighlight)
        {
            const f32 hoffset = negSel ? offset : (offset - hLength);
            ly->Panel(LyPnPar{.FillColor = Color{m_Style[OverlayColor_InputHighlight], 0.4f},
                              .Sizing = {sabs(hLength), grow()},
                              .SelfOffset = oabs({hoffset - cwidth, 0.f})});
        }

        const u32 toRemoveBegin = hasHighlight ? selStart : (selStart - 1);
        const u32 toRemoveEnd = selEnd;

        if (ctrl && nw->EventKeys[Key_V])
        {
            const char *cp = Platform::GetClipboard();

            // could maybe append one by one, as that = operator is constructing a TKit::TierString temp
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
            ly->Text(ly->GenerateNextId(), hint, tparams);
        }
        else
            ly->Text(ly->GenerateNextId(), buf, tparams);
    }

    ly->EndPanel();

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

    Layout *ly = m_Active->GetActiveLayout();
    // a very mid solution to unstable ids when text changes every frame (e.g, printing delta times/performance)
    // UPDATE: text has no id until explicitly set
    ly->Text(m_TextId, text, params);
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

    Layout *ly = m_Active->GetActiveLayout();
    ly->Unicode(NullLayoutId, icon, getUnicodeParams());

    ly->Text(m_TextId, text, params);
    if (m_TextId != NullLayoutId)
    {
        m_LastItem = m_TextId;
        m_TextId = NullLayoutId;
    }
    PopDirection();
}

void Overlay::BeginDisabled(const bool enabled)
{
    m_DisabledStack.Append(m_Style[OverlayStyle_Alpha], u32(enabled));
    if (enabled)
    {
        ++m_DisabledDepth;
        m_Style.Variables[OverlayStyle_Alpha] *= m_Style[OverlayStyle_DisabledAlpha];
    }
}
void Overlay::EndDisabled()
{
    const DisableInfo &dinfo = m_DisabledStack.GetBack();
    m_Style.Variables[OverlayStyle_Alpha] = dinfo.Alpha;
    m_DisabledDepth -= dinfo.Depth;

    m_DisabledStack.Pop();
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

bool Overlay::BeginPopup(const OverlayLabel label, const OverlayWindowFlags flags)
{
    if (m_CurrentPopupDepth == m_PopupStack.GetSize() || m_PopupStack[m_CurrentPopupDepth] != label.Id)
    {
        m_WidgetStates[label.Id] = 0;
        return false;
    }

    ++m_CurrentPopupDepth;
    if (getWidgetState(label.Id) == 0)
    {
        OverlayWindow *win = getOrCreateOverlayWindow(label.Id);
        if (!(win->Flags & WindowInternalFlag_OwnsNative))
            // we dont handle size because BeginWindow does that for us
            win->Native = m_Active->GetNative();

        const f32v2 &size = win->Size;
        win->SetActivePosition(
            win->ToScreen(computeMouseAlignedPosition(win->Native, size, !(flags & OverlayWindowFlag_NoPromotion))));
    }
    m_WidgetStates[label.Id] = WidgetStateFlag_Opened;
    return BeginWindow(label,
                       flags | OverlayWindowFlag_NoCollapse | WindowInternalFlag_Popup | OverlayWindowFlag_NoDocking);
}

void Overlay::EndPopup()
{
    --m_CurrentPopupDepth;
    EndWindow();
}
bool Overlay::BeginDropDown(const OverlayLabel label, const TKit::StringView preview, const OverlayDropDownFlags flags)
{
    Layout *ly = m_Active->GetActiveLayout();
    beginHorizontalWidget(PushId(label.Id), 1.f);

    const LayoutId id = IdFromStack("__onyx_id__Drop_down_box");
    const LayoutElementQueryInfo *elm = ly->QueryElement(id);
    OverlayColor boxCol = OverlayColor_DropDownIdle;

    const OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(elm, FocusFlag_LeftClickOpensPopup);

    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        boxCol = OverlayColor_DropDownPressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        boxCol = OverlayColor_DropDownHovered;

    const bool hasPreview = !(flags & OverlayDropDownFlag_NoPreview);
    const f32 padding = m_Style[OverlayStyle_WidgetPadding];

    const f32 maxWidth = m_CurrentPopupDepth == 0 ? (0.5f * m_Active->Size[0]) : TKIT_F32_MAX;
    ly->BeginPanel(id,
                   LyPnPar{.FillColor = m_Style[boxCol],
                           .Alignment = CenterLeft,
                           .Sizing = {flex(0.f, maxWidth), hasPreview ? fit() : sabs(getLineHeight() + 2.f * padding)},
                           .Shape = rect(m_Style[OverlayStyle_DropDownRadius])});

    if (hasPreview)
    {
        ly->BeginPanel(LyPnPar{
            .Alignment = CenterLeft, .Sizing = {grow(), fit()}, .Padding = m_Style[OverlayStyle_WidgetPadding]});

        ly->Text(ly->GenerateNextId(), preview, getTextParams());
        ly->EndPanel();
    }

    // ly->Panel(IdFromStack("__onyx_id_Push"), LyPnPar{.Sizing = grow()});

    const bool dropDownActive = focusFlags & OverlayFocusQueryFlag_PopupOpen;
    if (!(flags & OverlayDropDownFlag_NoArrowButton))
    {
        OverlayColor buttonCol = dropDownActive ? OverlayColor_DropDownButton : OverlayColor_DropDownHovered;
        ly->BeginPanel(LyPnPar{.FillColor = m_Style[buttonCol],
                               .Alignment = Center,
                               .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), flex()}});
        ly->Unicode(NullLayoutId, ArrowDownIcon, getUnicodeParams());
        ly->EndPanel();
    }

    if (dropDownActive)
    {
        endHorizontalWidget(label.Title);

        const LayoutId did = IdFromStack("__onyx_id_Drop_down");
        const LayoutElementQueryInfo *delm = ly->QueryElement(did);
        const f32 csize = delm ? delm->Size[1] : 0.f;

        const f32 ppos = elm->Position[1];
        const f32 psize = elm->Size[0];

        const bool tight = flags & OverlayDropDownFlag_Tight;

        const f32v4 borders = getWorldEffectiveBorders();
        const f32 bborder = borders[3];

        const bool surpasses = (ppos - csize) < bborder;
        const LyAlg2 att = {Alignment_Left, surpasses ? Alignment_Top : Alignment_Bottom};
        const LyAlg2 alg = surpasses ? BottomLeft : TopLeft;

        ly->BeginPanel(did, LyPnPar{.FillColor = m_Style[OverlayColor_PopupBackground],
                                    .Direction = LayoutDirection_TopToBottom,
                                    .Alignment = TopLeft,
                                    .Sizing = {fit(psize), fit()},
                                    .Shape = rect(m_Style[OverlayStyle_DropDownPopupRadius]),
                                    .Floating = {.Enable = true, .Attachment = att, .Alignment = alg},
                                    .Padding = tight ? 0.f : m_Style[OverlayStyle_WidgetPadding]});

        const f32 height = (flags & OverlayDropDownFlag_HeightSmall) ? m_Style[OverlayStyle_DropDownHeightSmall]
                                                                     : m_Style[OverlayStyle_DropDownHeightRegular];
        const bool largest = flags & OverlayDropDownFlag_HeightLargest;

        const LayoutId sid = IdFromStack("__onyx_id_Scroll");
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

        // we set a focus status to the dropdown so that it can register inputs and collapse/persist popups
        queryAndSetFocusStatus(delm, FocusFlag_DoNotSetPressedId | FocusFlag_DoNotSetActiveId);
        return true;
    }
    endHorizontalWidget(label.Title);
    ly->EndPanel();
    PopId();
    return false;
}

void Overlay::closePopup(const u32 depth)
{
    m_StateFlags |= StateFlag_PopupProtectionForbidden | StateFlag_MustCollapsePopups;
    m_PopupCollapseDepth = depth;
    // this means only the topmost modal can be collapsed
    m_ModalCollapseDepth = Math::Min(m_ModalCollapseDepth, depth);
    m_WidgetStates[m_Active->MenuBarId] = 0;
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
    const bool windowPromotions = allowPromotions && (Flags & OverlayFlag_WindowPromotions);

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
    OverlayWindow *parent = m_Active;

    m_Active = getOrCreateOverlayWindow(id);
    m_Active->Flags |= OverlayWindowFlag_AutoResize;

    PushId(id);

    m_Tooltip = m_Active;
    m_WindowStack.Append(m_Active);

    if (m_Active->Flags & WindowInternalFlag_Active)
        return;

    m_Active->Flags |= WindowInternalFlag_Active;
    m_Active->Flags |= OverlayWindowFlag_NoDocking;

    Layout *ly = m_Active->GetActiveLayout();

    const LayoutElementQueryInfo *elm = ly->QueryElement(id);

    const f32v2 size = elm ? elm->Size : m_Active->Size;

    const bool ownsNative = m_Active->Flags & WindowInternalFlag_OwnsNative;
    NativeWindow *pnw = parent->GetNative();
    if (!ownsNative)
        m_Active->Native = pnw;

    const bool noProm = flags & OverlayTooltipFlag_NoPromotion;
    if (noProm)
        m_Active->Flags |= OverlayWindowFlag_NoPromotion;

    const f32v2 pos = computeMouseAlignedPosition(pnw, size, !noProm);
    if (Flags & OverlayFlag_WindowPromotions)
    {
        // we dont care about the window's actual position (as the tooltip is just visually driven, there is no active
        // interaction for which we would need to store its position) EXCEPT when multi window is involved. thats why we
        // only set the position here

        const f32v2 scpos = parent->ToScreen(pos);
        const bool parentOwns = parent->Flags & WindowInternalFlag_OwnsNative;
        if (ownsNative)
            m_Active->GetNative()->ScreenPos = parentOwns ? scpos : (pnw->ScreenPos + scpos);
        else
            m_Active->ScreenPos = parentOwns ? (scpos - pnw->ScreenPos) : scpos;

        if (!Math::Approximately(m_Active->Size, size, 1.f))
        {
            m_Active->Size = size;
            if (ownsNative)
                m_Active->Flags |= WindowInternalFlag_WantUpdateSize;
        }
    }

    ly->BeginPanel(id, LyPnPar{.FillColor = m_Style[OverlayColor_WindowBorderIdle],
                               .Sizing = fit(),
                               .SelfOffset = oabs(ownsNative ? m_Active->GetNative()->GetWorldTopLeft() : pos),
                               .Shape = rect(m_Style[OverlayStyle_TooltipRadius]),
                               .Padding = m_Style[OverlayStyle_TooltipPadding]});

    ly->BeginPanel(LyPnPar{.FillColor = m_Style[OverlayColor_WindowBackgroundExpanded],
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
    m_Tooltip->Layout->Reset();

    if (m_Tooltip->Flags & WindowInternalFlag_OwnsNative)
        removeNativeWindow(m_Tooltip->Native);

    if (m_Tooltip->Flags & WindowInternalFlag_Active)
        m_Tooltip->Flags |= WindowInternalFlag_ActiveLastFrame;
    else
        m_Tooltip->Flags &= ~WindowInternalFlag_ActiveLastFrame;

    m_Tooltip->Flags &= ~(WindowInternalFlag_Active | WindowInternalFlag_OwnsNative);
    m_Tooltip->Native = getMainNativeWindow();
    m_Tooltip = nullptr;
}
void Overlay::resetTooltip()
{
    if (!m_Tooltip)
        return;
    m_Tooltip->Layout->Reset();
    if (m_Tooltip->Flags & WindowInternalFlag_Active)
        m_Tooltip->Flags |= WindowInternalFlag_ActiveLastFrame;
    else
        m_Tooltip->Flags &= ~WindowInternalFlag_ActiveLastFrame;
    m_Tooltip->Flags &= ~WindowInternalFlag_Active;
    m_Tooltip = nullptr;
}

/////////////////////////////////////////////
/// END TOOLTIPS
/////////////////////////////////////////////

/////////////////////////////////////////////
/// LAYOUT
/////////////////////////////////////////////

void Overlay::BeginScroll(const OverlayLabel label, const f32 maxHeight, const f32 maxWidth,
                          const OverlayScrollFlags flags)
{
    const LayoutId id = PushId(label.Id);
    const bool autoResize = isAutoResize();

    const f32 padding = m_Style[OverlayStyle_ContentAreaPadding];

    const bool borders = flags & OverlayScrollFlag_Borders;
    const bool tight = flags & OverlayScrollFlag_Tight;

    const f32 omw = maxWidth + 2.f * padding;
    const LySz2 outer = {autoResize ? fit() : grow(0.f, omw), fit()};
    const LySz2 content = {autoResize ? fit(0.f, maxWidth) : grow(0.f, maxWidth), fit(0.f, maxHeight)};

    Layout *ly = m_Active->GetActiveLayout();
    ly->BeginPanel(LyPnPar{.FillColor = borders ? m_Style[OverlayColor_ScrollAreaBorders] : Color_Transparent,
                           .Direction = LayoutDirection_TopToBottom,
                           .Alignment = CenterLeft,
                           .Sizing = outer,
                           .Shape = rect(m_Style[OverlayStyle_ScrollAreaBorderRadius]),
                           .Padding = borders ? padding : 0.f});

    if (flags & OverlayScrollFlag_Title)
        ly->Text(ly->GenerateNextId(), label.Title, getTextParams());

    beginScroll({.Id = id,
                 .OuterSizing = outer,
                 .ContentSizing = content,
                 .ContentPadding = tight ? 0.f : padding,
                 .ChildGap = tight ? 0.f : m_Style[OverlayStyle_ChildGap],
                 .Flags = flags});
}
void Overlay::HorizontalSeparator(const OverlayLabel label)
{
    Layout *ly = m_Active->GetActiveLayout();
    ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight,
                           .Alignment = CenterLeft,
                           .Sizing = {flex(), fit()},
                           .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const f32 textOffset = m_Style[OverlayStyle_SeparatorTextOffset];
    const f32 width = m_Style[OverlayStyle_LineWidth];

    ly->Panel(
        LyPnPar{.FillColor = m_Style[OverlayColor_Line], .Sizing = sabs({textOffset, width}), .Shape = rect(width)});

    ly->Text(ly->GenerateNextId(), label.Title, getTextParams());
    ly->Panel(LyPnPar{
        .FillColor = m_Style[OverlayColor_Line], .Sizing = {grow(textOffset), sabs(width)}, .Shape = rect(width)});
    ly->EndPanel();
}

bool Overlay::PushTree(const OverlayLabel label, const OverlayTreeFlags flags)
{
    const LayoutId id = PushId(label.Id);
    Layout *ly = m_Active->GetActiveLayout();
    const bool framed = flags & OverlayTreeFlag_Framed;

    OverlayColor col = framed ? OverlayColor_TreeIdle : OverlayColor_None;

    OverlayFocusQueryFlags focusFlags = queryAndSetFocusStatus(ly->QueryElement(id));
    if (focusFlags & OverlayFocusQueryFlag_Pressed)
        col = OverlayColor_TreePressed;
    else if (focusFlags & OverlayFocusQueryFlag_Hovered)
        col = OverlayColor_TreeHovered;

    const bool horScroll = m_Active->Flags & OverlayWindowFlag_HorizontalScroll;
    const bool autoResize = isAutoResize();
    const LySz growOrFlex = (horScroll || autoResize) ? flex() : grow();

    const bool spanLabel = flags & OverlayTreeFlag_SpanLabelWidth;
    const LySz2 sizing = {spanLabel ? fit() : growOrFlex, fit()};

    m_LastItem = ly->BeginPanel(id, LyPnPar{.FillColor = m_Style[col],
                                            .Alignment = CenterLeft,
                                            .Sizing = sizing,
                                            .Shape = rect(m_Style[OverlayStyle_TreeRadius]),
                                            .Padding = m_Style[OverlayStyle_HeaderPadding],
                                            .ChildGap = m_Style[OverlayStyle_ChildGap]});

    const bool startOpen = flags & OverlayTreeFlag_StartOpen;
    const bool opened = checkWidgetState(id, WidgetStateFlag_Opened, startOpen ? WidgetStateFlag_Opened : 0);

    const LayoutId buttonId =
        ly->BeginPanel(IdFromStack("__onyx_id_Tree_collapse"),
                       LyPnPar{.Alignment = Center, .Sizing = {sabs(m_Style[OverlayStyle_IconWidth]), fit()}});

    bool toggleOpen = focusFlags & OverlayFocusQueryFlag_LeftClicked;
    if (toggleOpen)
    {
        const bool onArrow = flags & OverlayTreeFlag_ToggleOnArrow;
        const bool onDoubleClick = !opened && (flags & OverlayTreeFlag_OpenOnDoubleClick);
        const bool doubleClicked = focusFlags & OverlayFocusQueryFlag_DoubleClicked;
        const NativeWindow *nw = m_Active->GetNative();
        if (onArrow)
            toggleOpen = ly->IsHovered(buttonId, nw->WorldMouse) || (onDoubleClick && doubleClicked);
        else if (onDoubleClick)
            toggleOpen = doubleClicked;
    }

    const CodePoint code = opened ? ArrowDownIcon : ArrowRightIcon;
    ly->Unicode(NullLayoutId, code, getUnicodeParams());

    ly->EndPanel();

    ly->Text(ly->GenerateNextId(), label.Title, getTextParams());
    ly->EndPanel();

    if (toggleOpen)
        toggleWidgetState(id, WidgetStateFlag_Opened);

    if (!opened)
    {
        PopId();
        return false;
    }

    const bool indent = !(flags & OverlayTreeFlag_NoIndent);

    const FontData &fdata = getFontData();
    const f32 fs = m_Style[OverlayStyle_FontSize];

    ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight, .Alignment = TopLeft, .Sizing = sizing});

    if (indent)
    {
        const f32 iconWidth = Math::Max(fs * fdata.GetGlyph(ArrowDownIcon)->Advance, m_Style[OverlayStyle_IconWidth]);
        const f32 treeIndent = iconWidth + 2.f * m_Style[OverlayStyle_HeaderPadding];

        ly->BeginPanel(LyPnPar{
            .Direction = LayoutDirection_TopToBottom, .Alignment = Center, .Sizing = {sabs(treeIndent), flex()}});

        if (flags & OverlayTreeFlag_DrawLines)
            VerticalLine();

        ly->EndPanel();
    }

    ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_TopToBottom,
                           .Alignment = TopLeft,
                           .Sizing = sizing,
                           .ChildGap = m_Style[OverlayStyle_ChildGap]});

    return true;
}
LayoutId Overlay::beginScroll(const ScrollParameterSpecs &specs)
{
    Layout *ly = m_Active->GetActiveLayout();
    ScrollInfo &sinfo = m_Scrollables[specs.Id];
    sinfo.Flags = specs.Flags;

    const bool noHeader = m_Active->Flags & OverlayWindowFlag_NoHeaderBar;
    const bool collapsed = !noHeader && m_Active->HeaderIcon == ArrowRightIcon;
    const bool drawBar = !(specs.Flags & OverlayScrollFlag_NoScrollBar);

    const bool borders = specs.Flags & OverlayScrollFlag_Borders;
    const f32 cpadding = m_Style[OverlayStyle_ContentAreaPadding];
    const bool bckg = !(specs.Flags & OverlayScrollFlag_NoBackground);
    m_LastItem = ly->BeginPanel(
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

    ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_RightToLeft,
                           .Alignment = TopLeft,
                           .Sizing = specs.OuterSizing,
                           .ChildGap = m_Style[OverlayStyle_ScrollBarGap]});

    if (!collapsed && !(specs.Flags & OverlayScrollFlag_NoVerticalScroll))
        appendStack |= performScroll(contentId, sinfo.Vertical, LayoutAxis_Vertical, specs.ContentPadding, drawBar);

    ly->BeginPanel(contentId, LyPnPar{.Direction = specs.Direction,
                                      .Alignment = TopLeft,
                                      .Sizing = specs.ContentSizing,
                                      .ChildOffset = oabs({-sinfo.Horizontal.ElementOffset,
                                                           -sinfo.Vertical.ElementOffset + specs.VerticalOffset}),
                                      .Padding = specs.ContentPadding,
                                      .ChildGap = specs.ChildGap});

    if (appendStack && isElementHovered(ly->QueryElement(specs.Id), OverlayHoveredFlag_AllowBlockedByPressedItem |
                                                                        OverlayHoveredFlag_AllowBlockedByActiveItem |
                                                                        OverlayHoveredFlag_AllowBlockedByDrag |
                                                                        standardHoverAllowance()))
        m_ScrollStack.Append(specs.Id);

    return contentId;
}

void Overlay::endScroll()
{
    Layout *ly = m_Active->GetActiveLayout();
    ly->EndPanel();
    ly->EndPanel();
    ly->EndPanel();
}
bool Overlay::performScroll(const LayoutId contentAreaId, ScrollBarInfo &sinfo, const LayoutAxis axis,
                            const f32 contentPadding, const bool drawBar)
{
    Layout *ly = m_Active->GetActiveLayout();
    const LayoutElementQueryInfo *contentArea = ly->QueryElement(contentAreaId);

    if (contentArea && contentArea->ChildrenSize[axis] > contentArea->Size[axis])
    {
        const f32 size = contentArea->Size[axis];
        const f32 csize = contentArea->ChildrenSize[axis] + 2.f * contentPadding;
        OverlayColor col = OverlayColor_ScrollBarIdle;

        const char *name =
            axis == LayoutAxis_Horizontal ? "__onyx_id_Horizontal_scroll_bar" : "__onyx_id_Vertical_scroll_bar";
        const LayoutId scrollId = IdFromStack(name);

        const LayoutElementQueryInfo *scrollBar = ly->QueryElement(scrollId);
        const f32 sign = axis == LayoutAxis_Horizontal ? -1.f : 1.f;

        const f32 sw = m_Style[OverlayStyle_ScrollBarWidth];
        const f32 mw = 2.f * sw;

        const f32 barSize = Math::Max(mw, size * size / csize);

        const f32 maxBarTravel = size - barSize;
        const f32 maxElementTravel = csize - size;
        const f32 barToElement = maxBarTravel > TKIT_F32_EPSILON ? (maxElementTravel / maxBarTravel) : 0.f;

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
                const NativeWindow *nw = m_Active->GetNative();
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

        if (drawBar)
        {
            LySz2 sizing;
            sizing[axis] = sabs(barSize);
            sizing[1 - axis] = sabs(sw);

            LyOf2 offset;
            offset[axis] = oabs(sinfo.BarOffset);
            offset[1 - axis] = oabs(0.f);

            ly->Panel(scrollId, LyPnPar{.FillColor = m_Style[col],
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
    Layout *ly = m_Active->GetActiveLayout();
    const LayoutElementQueryInfo *elm = ly->QueryElement(m_LastItem);
    const FocusFlags focusFlags = queryAndSetFocusStatus(elm);

    if (focusFlags & OverlayFocusQueryFlag_DragSource)
    {
        BeginTooltip();

        m_DragDropFlags |= flags;
        m_DragDropId = m_DraggedId;
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

    Layout *ly = m_Active->GetActiveLayout();
    const LayoutElementQueryInfo *elm = ly->QueryElement(m_LastItem);
    const FocusFlags focusFlags = queryAndSetFocusStatus(elm);

    m_DragDropFlags = flags;
    const bool target = focusFlags & OverlayFocusQueryFlag_DragTarget;
    if (target)
    {
        if (!(flags & OverlayDragDropFlag_TargetNoOutline))
        {
            LayoutElement *mod = ly->ModifyElement(m_LastItem);
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

OverlayHoverQueryFlags Overlay::queryHoverStatus(const LayoutElementQueryInfo *elm, const f32v2 &padding) const
{
    OverlayHoverQueryFlags flags = 0;
    const LayoutId id = elm ? elm->Id : LayoutId{NullLayoutId};

    const NativeWindow *nw = m_Active->GetNative();
    const bool hovered = elm && elm->IsHovered(nw->WorldMouse, padding);
    const bool windowBlock =
        !(m_Active->Flags & WindowInternalFlag_Focused) && !(m_Active->Flags & WindowInternalFlag_Hovered);
    const bool grabBlock = m_Grabbed;
    const bool pressBlock = m_PressedId != NullLayoutId && m_PressedId != id;
    const bool activeBlocked =
        !(m_StateFlags & StateFlag_ActiveAllowsInteraction) && m_ActiveId != NullLayoutId && m_ActiveId != id;
    const bool popupBlocked = m_CurrentPopupDepth != m_PopupStack.GetSize();
    const bool popupCollapseBlocked = m_StateFlags & StateFlag_FocusBlockByPopupCollapse;
    const bool disabledBlocked = m_DisabledDepth != 0;
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

bool Overlay::isElementHovered(const LayoutElementQueryInfo *elm, const OverlayHoveredFlags flags, const f32v2 &padding)
{
    const OverlayHoverQueryFlags qflags = queryHoverStatus(elm, padding);
    const bool candidate = isElementHovered(qflags, flags);

    const bool shortDelay = flags & OverlayHoveredFlag_ShortDelay;
    const bool normalDelay = flags & OverlayHoveredFlag_NormalDelay;
    const bool stationary = flags & OverlayHoveredFlag_Stationary;
    TKIT_ASSERT(normalDelay + shortDelay != 2,
                "[ONYX][OVERLAY] Cannot have short delay and normal delay at the same time in tooltip");

    const Layout *ly = m_Active->GetActiveLayout();
    if (candidate)
    {
        const f32 statThres = stationary ? m_Style[OverlayStyle_HoverStationaryThreshold] : TKIT_F32_MAX;
        const NativeWindow *nw = m_Active->GetNative();
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
        m_HoveredLayoutCandidate = ly;

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

    Layout *ly = m_Active->GetActiveLayout();

    const LayoutId iboxId = IdFromStack("__onyx_id_Input_box");
    const LayoutElementQueryInfo *ibox = ly->QueryElement(iboxId);

    const NativeWindow *nw = m_Active->GetNative();
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

OverlayFocusQueryFlags Overlay::queryAndSetFocusStatus(const LayoutElementQueryInfo *elm, const FocusFlags flags,
                                                       const f32v2 &padding, const OverlayHoveredFlags hoverFlags)
{
    if (!elm)
        return 0;

    OverlayFocusQueryFlags outFlags = 0;
    TKIT_ASSERT(m_CurrentPopupDepth <= m_PopupStack.GetSize(),
                "[ONYX][OVERLAY] Popup depth ({}) must not be greater than popup stack ({})", m_CurrentPopupDepth,
                m_PopupStack.GetSize());

    const bool evenWhenAway = flags & FocusFlag_PressedEvenWhenAwayFromHover;

    const NativeWindow *nw = m_Active->GetNative();
    const bool lmpressed = nw->Flags & NativeWindowFlag_LeftMousePressed;
    const bool lmreleased = nw->Flags & NativeWindowFlag_LeftMouseReleased;

    const OverlayHoverQueryFlags hflags = queryHoverStatus(elm, padding);

    const bool focusHovered = isElementHovered(hflags, hoverFlags | standardHoverAllowance());

    const bool setHovered = !(flags & FocusFlag_DoNotSetHoveredId);
    const bool setPressed = !(flags & FocusFlag_DoNotSetPressedId);
    const bool setActive = !(flags & FocusFlag_DoNotSetActiveId);
    const bool setDragged = !(flags & FocusFlag_DoNotSetDraggedId);
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
        OverlayHoveredFlag_AllowBlockedByWindow | OverlayHoveredFlag_AllowBlockedByWindowGrab;

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

        // hover id is essentially used to stop windows from moving/resizing when a widget is being hovered. we
        // want to allow immediate dragging when out of the popup, and close everything, except for intermediate
        // popups. those must still prevent windows from moving, because the popup hierarchy is still not completely
        // collapsed. note that we dont allow grab blocks here. we dont want windows to set hovered id
        //
        // this not only stopped working, but caused a bug (back widgets reporting as hovered), and removing this
        // section fixed it. i think this part of the codebase changed enough so that this was no longer necessary if
        // (setHovered && popupHovered && m_CurrentPopupDepth != 0)
        // {
        //     m_HoveredId = elm->Id;
        //     if (flags & FocusFlag_HoveredAllowsInteraction)
        //         m_StateFlags |= StateFlag_HoveredAllowsInteraction;
        //     else
        //         m_StateFlags &= ~StateFlag_HoveredAllowsInteraction;
        // }

        return outFlags;
    }

    const bool allowOnEnter = flags & FocusFlag_AllowPressedOnEnter;
    const bool rclicked = focusHovered && (nw->Flags & NativeWindowFlag_RightMouseReleased);
    const bool pressingMouse = nw->Flags & NativeWindowFlag_PressingLeftMouse;

    const bool pressed = (focusHovered || (evenWhenAway && m_PressedId == elm->Id)) && pressingMouse &&
                         (allowOnEnter || lmpressed || m_PressedId == elm->Id);

    if (focusHovered && setHovered)
    {
        m_HoveredId = elm->Id;
        outFlags |= OverlayFocusQueryFlag_Hovered;
    }

    const bool dragged = (focusHovered && lmpressed) || (m_PressedId == elm->Id && pressingMouse);
    if (dragged)
    {
        const f32 drag = Math::NormSquared(nw->WorldMouse - nw->WorldMouseOnPress);
        const f32 th = m_Style[OverlayStyle_DragThreshold];
        if (setDragged && drag >= th * th)
            m_DraggedId = elm->Id;
    }
    const bool dragHovered = isElementHovered(hflags, OverlayHoveredFlag_AllowBlockedByPressedItem |
                                                          OverlayHoveredFlag_AllowBlockedByActiveItem |
                                                          OverlayHoveredFlag_AllowBlockedByDrag);
    if (dragHovered && m_DraggedId != NullLayoutId && m_DraggedId != elm->Id)
    {
        outFlags |= OverlayFocusQueryFlag_DragTarget;
        if (lmreleased)
            outFlags |= OverlayFocusQueryFlag_DragPayloadDropped;
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
                m_ActiveId = m_ActiveId == elm->Id ? LayoutId{NullLayoutId} : elm->Id;
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
    {
        m_StateFlags |= StateFlag_PressedIdMustPersist;
        if (flags & FocusFlag_PressedAllowsInteraction)
            m_StateFlags |= StateFlag_PressedAllowsInteraction;
        else
            m_StateFlags &= ~StateFlag_PressedAllowsInteraction;
    }
    if (m_HoveredId == elm->Id)
    {
        if (flags & FocusFlag_HoveredAllowsInteraction)
            m_StateFlags |= StateFlag_HoveredAllowsInteraction;
        else
            m_StateFlags &= ~StateFlag_HoveredAllowsInteraction;
    }
    if (m_DraggedId == elm->Id && !lmreleased)
    {
        m_StateFlags |= StateFlag_DraggedIdMustPersist;
        outFlags |= OverlayFocusQueryFlag_DragSource;
    }

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
    ++m_FrameCount;
    TKIT_PROFILE_NSCOPE("Onyx::Overlay::Draw");
    TKIT_ASSERT(m_IdStack.IsEmpty(),
                "[ONYX][OVERLAY] Id stack size mismatch (size = {}, should be 0). For every PushId(), there must "
                "be a PopId()",
                m_IdStack.GetSize());

    const u32 modalWindow = processWindows();
    const bool windowPromotions = Flags & OverlayFlag_WindowPromotions;

    for (const NativeWindow *nw : m_NativeWindows)
    {
        nw->Context->Flush();
        if (modalWindow != 0)
            nw->View->ClearColor.rgba[3] = 1.f;
        else
            nw->View->ClearColor.rgba[3] = 0.f;
    }

    u32 idx = 0;

    OverlayWindow *dockTargetWin = nullptr;
    RenderContext<D2> *dockTargetCtx = nullptr;

    const auto tryAssignDockTarget = [&](OverlayWindow *win, RenderContext<D2> *ctx) {
        if (win->Flags & WindowInternalFlag_IsDockTarget)
        {
            dockTargetWin = win;
            dockTargetCtx = ctx;
        }
    };

    u32 depthCounter = 0;
    u32 floatDepthCounter = 0;
    for (const Layout *ly : m_Layouts)
        floatDepthCounter += ly->GetElements().GetSize();

    for (OverlayWindow *win : m_ActiveWindows)
    {
        NativeWindow *nw = win->GetNative();
        RenderContext<D2> *ctx = nw->Context;
        tryAssignDockTarget(win, ctx);
        if (!win->OwnsActiveLayout())
            continue;

        if (++idx == modalWindow)
        {
            ctx->Push();
            ctx->Scale(nw->GetDimensions());
            ctx->Alpha(0.2f);
            ctx->Quad();
            ctx->Pop();
        }
        win->Layout->Compile(&depthCounter, &floatDepthCounter);

        if (windowPromotions)
        {
            const bool useDepthCounter = win->Layout->HasCustomDepthCounter();
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
            for (const LayoutDrawInfo &info : win->Layout->GetDrawInfo())
            {
                while (info.Depth <= depth && !stack.IsEmpty())
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
                    ctx->LayoutElement(info, nullptr, useDepthCounter);
                else
                {
                    LayoutDrawInfo elm = info;
                    elm.Position += offset;
                    if (elm.ClipMin[0] != TKIT_F32_MAX)
                    {
                        elm.ClipMax += offset;
                        elm.ClipMin += offset;
                    }
                    ctx->LayoutElement(elm, nullptr, useDepthCounter);
                }
            }
            for (const FloatRedirect &rd : stack)
                rd.Context->EndLayoutElements();
            ctx->EndLayoutElements();
        }
        else
            ctx->Layout(*win->Layout);
    }

    if (m_Tooltip)
    {
        m_Tooltip->Layout->EndPanel();
        m_Tooltip->Layout->EndPanel();
        m_Tooltip->Layout->Compile(&depthCounter, &floatDepthCounter);
        m_Tooltip->Native->Context->Layout(*m_Tooltip->Layout);
        if (windowPromotions && (m_Tooltip->Flags & WindowInternalFlag_OwnsNative))
            m_Tooltip->Native->Window->SetPosition(i32v2{m_Tooltip->Native->ScreenPos});
    }

    const auto runDocking = [&] {
        if (dockTargetWin)
            dockInsertAndDrawPreview(dockTargetWin, dockTargetCtx);
    };

    static constexpr u64 framesUntilPromotionsAreAvailable = 60;
    const auto runWindowPromotions = [&] {
        if (!windowPromotions)
            demoteAllWindows();
        else if ((Flags & OverlayFlag_FloatingMode) || !(Flags & OverlayFlag_WindowPromotionInitialFrameCooldown) ||
                 m_FrameCount > framesUntilPromotionsAreAvailable)
            manageWindowPromotions();
    };

    const bool floating = Flags & OverlayFlag_FloatingMode;
    if (floating)
    {
        runWindowPromotions();
        runDocking();
    }
    else
    {
        runDocking();
        runWindowPromotions();
    }

    m_ActiveWindows.Clear();

    undockMarked();
    applyDockTrees();
    cleanupWindowState();
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

// used when having menu bars with windows that are not brought to focus. in those cases popups created by such
// window will be blocked if there is another window existing with a greater layer. so in that case, we allow the
// hover, apart from additional overloads used sometimes
OverlayHoveredFlags Overlay::standardHoverAllowance() const
{
    return (m_Active->PopupDepth != m_CurrentPopupDepth &&
                    (m_Active->Flags &
                     (OverlayWindowFlag_NoBringToFocus | OverlayWindowFlags(WindowInternalFlag_HasActiveChildren)))
                ? OverlayHoveredFlag_AllowBlockedByWindow
                : 0);
}

bool Overlay::isAutoResize() const
{
    return m_CurrentPopupDepth == m_Active->PopupDepth && (m_Active->Flags & OverlayWindowFlag_AutoResize) &&
           !(m_Active->Flags & WindowInternalFlag_MenuBarOpened);
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
    const NativeWindow *nw = m_Active->GetNative();
    const bool windowPromotions = Flags & OverlayFlag_WindowPromotions;
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

#define DEMO_MAX_OPEN_DEPTH 3
// #define DEMO_START_OPEN

static void editDemoWindowFlags(Overlay *ov, OverlayWindowFlags *flags)
{
    ov->CheckBoxFlags("OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted", flags,
                      Onyx::OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted);
    ov->CheckBoxFlags("OverlayWindowFlag_MousePassThrough", flags, Onyx::OverlayWindowFlag_MousePassThrough);
    ov->CheckBoxFlags("OverlayWindowFlag_ChildGrowWidth", flags, Onyx::OverlayWindowFlag_ChildGrowWidth);
    ov->CheckBoxFlags("OverlayWindowFlag_ChildGrowHeight", flags, Onyx::OverlayWindowFlag_ChildGrowHeight);
    ov->CheckBoxFlags("OverlayWindowFlag_NoUndocking", flags, Onyx::OverlayWindowFlag_NoUndocking);
    ov->CheckBoxFlags("OverlayWindowFlag_NoBackground", flags, Onyx::OverlayWindowFlag_NoBackground);
    ov->CheckBoxFlags("OverlayWindowFlag_NoBorders", flags, Onyx::OverlayWindowFlag_NoBorders);
    ov->CheckBoxFlags("OverlayWindowFlag_NoDocking", flags, Onyx::OverlayWindowFlag_NoDocking);
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
    ov->CheckBoxFlags("OverlayWindowFlag_MoveWithHeader", flags, Onyx::OverlayWindowFlag_MoveWithHeader);
}

static void drawDemoMenus(Overlay *ov, bool *enableSettings, bool *enableRenderer, bool *enableStyleEditor,
                          bool *enableMainMenu)
{
    if (ov->BeginMenu("Options"))
    {
        ov->MenuItem("Window settings", enableSettings);
        ov->MenuItem("Renderer stats", enableRenderer);
        ov->MenuItem("Style editor", enableStyleEditor);
        ov->MenuItem("Main menu bar", enableMainMenu);
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
        if (ov->BeginMenu("Recurse"))
        {
            drawDemoMenus(ov, enableSettings, enableRenderer, enableStyleEditor, enableMainMenu);
            ov->EndMenu();
        }
        ov->MenuItem("Select", &selected);
        ov->EndMenu();
    }
}

static void drawDemoContents(Overlay *ov, OverlayFlags &flags, const OverlayWindowFlags wflags,
                             bool *fullScreenDockSpace, bool *enableSettings, bool *enableRenderer,
                             bool *enableStyleEditor, bool *enableMainMenu, const u32 depth)
{
#ifdef DEMO_START_OPEN
    const Onyx::OverlayTreeFlags drawLines = Onyx::OverlayTreeFlag_DrawLines | Onyx::OverlayTreeFlag_StartOpen;
#else
    const Onyx::OverlayTreeFlags drawLines = Onyx::OverlayTreeFlag_DrawLines;
#endif
    static bool disableGlobal = false;
    if (ov->PushTree("Configuration"))
    {
        ov->BeginDisabled(flags & Onyx::OverlayFlag_FloatingMode);
        ov->CheckBoxFlags("OverlayFlag_WindowPromotions", &flags, Onyx::OverlayFlag_WindowPromotions);
        ov->EndDisabled();
        if (flags & Onyx::OverlayFlag_FloatingMode)
            ov->SetItemTooltip("When in floating mode, window promotions must be on or terrible things will happen");

        ov->CheckBoxFlags("OverlayFlag_Docking", &flags, Onyx::OverlayFlag_Docking);
        ov->PopTree();
    }

    const Onyx::OverlayTreeFlags tcflags =
        depth > DEMO_MAX_OPEN_DEPTH ? (drawLines & ~Onyx::OverlayTreeFlag_StartOpen) : drawLines;
    if (ov->PushTree("Child windows", tcflags))
    {
        static bool enableChild = true;
        ov->CheckBox("Enable child", &enableChild);
        if (ov->BeginWindow("Overlay demo child", &enableChild,
                            wflags | Onyx::OverlayWindowFlag_MenuBar | Onyx::OverlayWindowFlag_MergeIdWithStack))
        {
            drawDemoContents(ov, flags, wflags, fullScreenDockSpace, enableSettings, enableRenderer, enableStyleEditor,
                             enableMainMenu, depth + 1);
            ov->EndWindow();
        }

        ov->PopTree();
    }

    const NativeWindow *nw = ov->GetMainNativeWindow();
    const f32 ftime = nw ? Onyx::GetDeltaTime(nw->Window).AsMilliseconds() : Onyx::GetDeltaTime().AsMilliseconds();
    if (ov->PushTree("General", drawLines))
    {
        ov->SetNextTextId(ov->IdFromStack("Delta time"));
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
        drawDemoMenus(ov, enableSettings, enableRenderer, enableStyleEditor, enableMainMenu);
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

    if (ov->PushTree("Docking", drawLines))
    {
        static Onyx::OverlayDockNodeFlags dflags = Onyx::OverlayDockNodeFlag_CanBeEmpty;
        static Onyx::OverlayWindowFlags windowFlags = 0;

        ov->BeginDisabled(flags & Onyx::OverlayFlag_FloatingMode);
        ov->CheckBox("Enable full screen dock space", fullScreenDockSpace);
        ov->EndDisabled();
        if (flags & Onyx::OverlayFlag_FloatingMode)
            ov->SetItemTooltip("When in floating mode, full screen dockspaces cannot exist");

        ov->HorizontalSeparator("Dock node flags");

        ov->CheckBoxFlags("OverlayDockNodeFlag_CanBeEmpty", &dflags, Onyx::OverlayDockNodeFlag_CanBeEmpty);
        ov->SetItemTooltip("It is strongly recommended you leave this on");
        ov->CheckBoxFlags("OverlayDockNodeFlag_NoResize", &dflags, Onyx::OverlayDockNodeFlag_NoResize);

        if (ov->PushTree("Window flags", drawLines))
        {
            editDemoWindowFlags(ov, &windowFlags);
            ov->PopTree();
        }

        ov->TextRaw("This is an example dock space. You may dock windows here");
        ov->DockSpace(ov->IdFromStack("My dock host"), dflags, windowFlags);

        ov->PopTree();
    }

    if (ov->PushTree("Drag & Drop", drawLines))
    {
        ov->TextRaw(
            TextMode_Wrapped,
            "Generally drag & drop is enabled by default in color editors. Here it is "
            "disabled explicitly by using OverlayColorFlag_NoDragDrop for demonstration "
            "purposes through the user-facing API. Color editors are the only widgets (for "
            "now) that provide drag & drop capabilities out of the box, but they can be set up for any UI element");

        static Onyx::OverlayDragDropFlags dflags = 0;
        ov->CheckBoxFlags("OverlayDragDropFlag_SourceNoTooltip", &dflags, Onyx::OverlayDragDropFlag_SourceNoTooltip);

        ov->CheckBoxFlags("OverlayDragDropFlag_TargetNoTooltip", &dflags, Onyx::OverlayDragDropFlag_TargetNoTooltip);
        ov->CheckBoxFlags("OverlayDragDropFlag_TargetNoOutline", &dflags, Onyx::OverlayDragDropFlag_TargetNoOutline);
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

        ov->HorizontalDrag("You can do both here!##Drag", &val3, 0.1f, 0.f, 10.f);
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
        ov->CheckBoxFlags("OverlayInputFlag_EnterCommitsBuffer", &iflags, Onyx::OverlayInputFlag_EnterCommitsBuffer);
        ov->CheckBoxFlags("OverlayInputFlag_EscapeClearsAll", &iflags, Onyx::OverlayInputFlag_EscapeClearsAll);
        ov->CheckBoxFlags("OverlayInputFlag_AutoSelectAll", &iflags, Onyx::OverlayInputFlag_AutoSelectAll);
        ov->CheckBoxFlags("OverlayInputFlag_NoHorizontalScroll", &iflags, Onyx::OverlayInputFlag_NoHorizontalScroll);
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
        editDemoWindowFlags(ov, &pflags);

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
                ov->SetNextTextId(ov->IdFromStack("Text id"));
                ov->Text("Right click me and change the value!: {}", value);
                if (ov->BeginPopupContextItem("Value edit",
                                              Onyx::OverlayWindowFlag_AutoResize | Onyx::OverlayWindowFlag_BringToTop))
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

        const bool hovered = ov->IsItemHovered(hflags);
        const bool opened = ov->IsItemOpened();
        const Onyx::OverlayHoverQueryFlags hqflags = ov->QueryItemHoverStatus();
        const Onyx::OverlayFocusQueryFlags fqflags = ov->QueryItemFocusStatus(fflags);

        ov->HorizontalSeparator("Hovering info");
        ov->Text("Hovered: {}", hovered);
        ov->Text("Blocked by window: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByWindow));
        ov->Text("Blocked by window grab: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByWindowGrab));
        ov->Text("Blocked by pressed item: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPressedItem));
        ov->Text("Blocked by active item: {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByActiveItem));
        ov->Text("Blocked by popup : {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPopup));
        ov->Text("Blocked by popup collapse : {}", bool(hqflags & Onyx::OverlayHoverQueryFlag_BlockedByPopupCollapse));
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
        ov->Text("Dragged: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_DragSource));
        ov->Text("Drag hovered: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_DragTarget));
        ov->Text("Popup open: {}", bool(fqflags & Onyx::OverlayFocusQueryFlag_PopupOpen));

        ov->HorizontalSeparator("State info");
        ov->Text("Opened: {}", opened);

        ov->HorizontalSeparator("Focus info");
        ov->Text("Want capture mouse: {}", ov->WantCaptureMouse());
        ov->Text("Want capture keyboard: {}", ov->WantCaptureKeyboard());

        ov->PopTree();
    }

    if (ov->PushTree("Selectables", drawLines))
    {
        static Onyx::OverlaySelectableFlags sflags = 0;

        ov->CheckBoxFlags("OverlaySelectableFlag_SpanLabelWidth", &sflags, Onyx::OverlaySelectableFlag_SpanLabelWidth);
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

        ov->BeginScroll("Title", dimensions[1], xunlim ? TKIT_F32_MAX : dimensions[0], sflags);

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
        ov->CheckBoxFlags("OverlaySliderFlag_NoValueLabelClamp", &sflags, Onyx::OverlaySliderFlag_NoValueLabelClamp);

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
        static Onyx::OverlayTabBarFlags tflags = 0;
        static bool tab1 = true;
        static bool tab3 = true;
        ov->CheckBoxFlags("OverlayTabBarFlag_Reorderable", &tflags, Onyx::OverlayTabBarFlag_Reorderable);
        ov->CheckBoxFlags("OverlayTabBarFlag_NoBottomLine", &tflags, Onyx::OverlayTabBarFlag_NoBottomLine);
        ov->CheckBox("Enable tab 1", &tab1);
        ov->CheckBox("Enable tab 3", &tab3);

        ov->BeginTabBar("Example", tflags);
        if (ov->BeginTab("Tab 1", &tab1, Onyx::OverlayTabFlag_StartOpen))
        {
            ov->TextRaw("I am tab 1");
            ov->EndTab();
        }
        if (ov->BeginTab("Tab 2"))
        {
            ov->TextRaw("I am tab 2");
            ov->BeginTabBar("Nested");

            if (ov->BeginTab("Hello", Onyx::OverlayTabFlag_StartOpen))
            {
                ov->TextRaw("I am text inside a nested tab");
                ov->EndTab();
            }
            if (ov->BeginTab("Hi!"))
            {
                ov->TextRaw("I am also text inside a nested tab");
                ov->EndTab();
            }
            ov->EndTabBar();

            ov->EndTab();
        }
        if (ov->BeginTab("Tab 3", &tab3))
        {
            ov->TextRaw("I am tab 3");
            ov->EndTab();
        }
        ov->EndTabBar();
        ov->PopTree();
    }

    if (ov->PushTree("Text", drawLines))
    {
        ov->TextRaw("This is some raw text");
        ov->TextRaw(Onyx::TextMode_Wrapped,
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
}

void Overlay::ShowDemo(bool *enabled)
{
    TKIT_PROFILE_NSCOPE("Onyx::Overlay::Demo");
    static Onyx::OverlayWindowFlags wflags = 0;
#ifdef DEMO_START_OPEN
    static bool enableSettings = true;
    static bool enableRenderer = true;
    static bool enableStyleEditor = true;
#else
    static bool enableSettings = false;
    static bool enableRenderer = false;
    static bool enableStyleEditor = false;
#endif
    static bool enableMainMenu = false;

    Overlay *ov = this;

    static bool fullScreenDockSpace = false;

    if (ov->Flags & OverlayFlag_FloatingMode)
        fullScreenDockSpace = false;
    if (fullScreenDockSpace)
        ov->FullScreenDockSpace(Onyx::OverlayDockNodeFlag_CanBeEmpty,
                                Onyx::OverlayWindowFlag_NoBackground | Onyx::OverlayWindowFlag_MousePassThrough |
                                    Onyx::OverlayWindowFlag_DockSpaceUndockWhenNotSubmitted);

    if (enableMainMenu && ov->BeginMainMenuBar())
    {
        drawDemoMenus(ov, &enableSettings, &enableRenderer, &enableStyleEditor, &enableMainMenu);
        ov->EndMainMenuBar();
    }

    if (ov->BeginWindow("Overlay demo", enabled, wflags | Onyx::OverlayWindowFlag_MenuBar))
    {
        drawDemoContents(ov, ov->Flags, wflags, &fullScreenDockSpace, &enableSettings, &enableRenderer,
                         &enableStyleEditor, &enableMainMenu, 0);
        ov->EndWindow();
    }

    if (ov->BeginWindow("Window settings", &enableSettings, wflags))
    {
        if (ov->BeginMenuBar())
        {
            drawDemoMenus(ov, &enableSettings, &enableRenderer, &enableStyleEditor, &enableMainMenu);
            ov->EndMenuBar();
        }
        editDemoWindowFlags(ov, &wflags);
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
        if (ov->PushTree("Miscellaneous", Onyx::OverlayTreeFlag_DrawLines))
        {
            ov->TextRaw(Onyx::TextMode_Wrapped, "Nothing to see here! This is extra content to demonstrate "
                                                "appending multiple times to a window after it has "
                                                "been closed");
            if (ov->BeginMenu("A rogue menu item"))
            {
                drawDemoMenus(ov, &enableSettings, &enableRenderer, &enableStyleEditor, &enableMainMenu);
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
    BeginTabBar("Dimension");
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
    Layout *ly = m_Active->GetActiveLayout();

    constexpr u32 bufSize = 128;
    static char colorFilter[bufSize] = {0};

    TextRaw("Welcome to the (pretty minimal) style editor! Right click any item to reset it to default");

    BeginTabBar("Style");

    if (BeginTab("Colors", OverlayTabFlag_StartOpen))
    {
        const auto colorEditor = [&](const TKit::StringView str, const u32 col) {
            if (colorFilter[0] != 0 && !str.Contains(colorFilter))
                return;

            static TKit::Clock flashClock{};
            static OverlayColors backup;
            static u32 flashing = TKIT_U32_MAX;

            ly->BeginPanel(LyPnPar{.Direction = LayoutDirection_LeftToRight,
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
            if (BeginPopupContextItem({IdFromStack(col), "Default"}, OverlayWindowFlag_AutoResize |
                                                                         OverlayWindowFlag_BringToTop |
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
            ly->EndPanel();
        };

        InputText("Filter##Colors", colorFilter, bufSize);

        colorEditor("None", OverlayColor_None);
        colorEditor("Text", OverlayColor_Text);
        colorEditor("Line", OverlayColor_Line);
        colorEditor("DockPreview", OverlayColor_DockPreview);
        colorEditor("DockSpaceBackground", OverlayColor_DockSpaceBackground);
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
            if (BeginPopupContextItem({IdFromStack(var), "Default"}, OverlayWindowFlag_AutoResize |
                                                                         OverlayWindowFlag_BringToTop |
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
        varSlider("UnicodeSize", OverlayStyle_UnicodeSize, 1.f, 100.f);
        varSlider("IndentWidth", OverlayStyle_IndentWidth, 1.f, 100.f);
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
// NOLINTEND(performance-unnecessary-value-param)
TKIT_COMPILER_WARNING_IGNORE_POP()
