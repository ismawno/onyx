# Onyx Roadmap

## Phase 1 — Custom Vertices (June)
- [X] `Geometry_Dynamic` with packed u32 RGBA color
- [X] Vertex/index buffer creation and rendering
- [X] Shader for vertex-colored geometry

## Phase 2 — Overlay UI (June–August)

- [X] Button
- [X] RadioButton
- [X] CheckBox / CheckBoxFlags
- [X] HorizontalSlider (single + array)
- [X] HorizontalDrag (single + array)
- [X] InputText (single line)
- [X] InputNumeric
- [X] Text (with fmt formatting)
- [X] Separator / HorizontalSeparator / HorizontalLine / VerticalLine
- [X] Window (resize, collapse, scroll, z-order, spawn offset)
- [X] Window flags (NoResize, NoMove, NoCollapse, NoScrollBar, NoHeaderBar, NoBackground, NoBringToFocus, AlwaysAutoResize)
- [X] Input flags (EnterReturnsTrue, EnterCommitsBuffer, EscapeClearsAll, AutoSelectAll, NoHorizontalScroll, ElideLeft)
- [X] Tooltip (basic, BeginTooltip/EndTooltip, SetTooltip, BeginItemTooltip, SetItemTooltip)
- [X] PushDirection / PushIndent / Pop
- [X] BeginPanel / EndPanel (raw layout access)
- [X] Tooltip delay (hover timer before showing)
- [X] Tooltip repositioning (keep within viewport)
- [X] IsItemClicked()
- [X] IsItemOpened()
- [X] Trees - Framed nodes
- [X] Trees - Opened at start
- [ ] Color picker (HSV square + hue bar + alpha bar, depends on Phase 1)
- [ ] ColorEdit3 / ColorEdit4 (inline color field + popup picker)
- [ ] ColorButton (small color swatch, opens picker on click)
- [X] Combo / Dropdown (BeginCombo / EndCombo)
- [ ] ListBox (scrollable selection list)
- [X] Selectable (clickable row, used in lists/menus)
- [X] TreeNode / TreePush / TreePop (collapsible hierarchy)
- [X] CollapsingHeader (like TreeNode but for sections, no indent)
- [ ] Tabs (BeginTabBar / EndTabBar / BeginTabItem / EndTabItem)
- [ ] ProgressBar
- [ ] TextInput multiline
- [ ] Table (BeginTable / EndTable / TableNextRow / TableNextColumn)
- [X] Popup / Modal (BeginPopup / EndPopup / OpenPopup)
- [X] Context menu (right-click popup, BeginPopupContextItem)
- [X] MenuBar / Menu / MenuItem (BeginMenuBar / BeginMenu / MenuItem)
- [X] Image / ImageButton (display a texture handle inline)
- [ ] BulletText (text with a bullet point prefix)
- [ ] Drag & Drop (BeginDragDropSource / BeginDragDropTarget / AcceptDragDropPayload)
- [ ] Docking (drag windows into splits and tab groups)

### Missing global features
- [ ] BeginDisabled / EndDisabled (grays out + blocks interaction for everything between)
- [X] PushStyleColor / PopStyleColor (temporary color overrides)
- [X] PushStyleVar / PopStyleVar (temporary config overrides: padding, spacing, etc.)
- [ ] BeginGroup / EndGroup (logical grouping for IsItemHovered on a block)
- [ ] Dummy / Spacing (invisible spacer element)
- [ ] SetNextItemWidth (override widget width for the next item)
- [X] BeginChild / EndChild (scrollable sub-region within a window)
- [ ] Clipboard support (Ctrl+C / Ctrl+V in text input)
- [ ] Undo/redo in text input (Ctrl+Z / Ctrl+Y)
- [X] Keyboard cursor movement (arrows, Home, End, Ctrl+arrows)
- [ ] Keyboard navigation / tab focus between widgets
- [X] IsItemHovered / IsItemActive / IsItemClicked (query last widget state)
- [ ] SetKeyboardFocusHere (programmatic focus)
- [ ] PlotLines / PlotHistogram (simple inline sparklines)

### Missing per-widget flags
- [X] Slider: NoLabel, Logarithmic
- [X] Drag: NoLabel
- [X] Combo: NoPreview, HeightSmall/HeightLarge
- [X] TreeNode: DefaultOpen, Leaf, Bullet, OpenOnArrow, OpenOnDoubleClick, SpanFullWidth
- [ ] Tab: Reorderable, NoCloseButton, FittingPolicyScroll/Resize
- [ ] Table: Sortable, Resizable, NoBorders, RowBackground, ScrollX/ScrollY, Hideable columns
- [ ] ColorPicker: NoAlpha, NoLabel, NoInputs, NoSmallPreview, PickerHueBar/PickerHueWheel
- [X] Selectable: SpanAllColumns, AllowDoubleClick, Disabled
- [X] Popup: AnyPopupId, AnyPopupLevel

## Phase 3 — Editor (September–October)
- [ ] ECS (archetypes + parallel scheduling)
- [ ] Scene hierarchy panel
- [ ] Property inspector
- [ ] Editor viewport
- [ ] Offscreen rendering (for thumbnails/previews)

## Phase 4 — Rendering Features (November)
- [ ] Skybox / environment maps
- [ ] Bloom
- [ ] Frustum culling
- [ ] Forward+ rendering

## Phase 5 — Quick Wins (December)
- [ ] Profile macros
- [ ] Screenshots
- [ ] Wireframe rendering
- [ ] Material UV range selection
- [ ] Manual pixel coloring (bit array)

## Phase 6 — Extensibility (January+)
- [ ] Custom shader support (user pipelines, layouts, SPIR-V reflection)
- [ ] Python bindings
- [ ] Sound

## Phase 7 — Polish
- [ ] Redo README
