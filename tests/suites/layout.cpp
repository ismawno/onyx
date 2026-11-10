// Overlay layouts: Serialize writes where windows are and how they are docked, and Deserialize brings it back into
// another overlay. A layout file is read and checked whole before anything is applied, so a broken one is reported and
// changes nothing.

#include "engine.hpp"
#include <cstring>
#include <string>

using namespace OnyxTest;

#ifdef TKIT_ENABLE_YAML_SERIALIZATION

// two floating windows: A placed by position, B by size, both on birth only. a loaded layout must win over both
static void floatingGui(UiEngine &ui)
{
    ui.Gui = [](Onyx::Overlay *ov) {
        ov->SetNextWindowPosition({40.f, 50.f}, Onyx::OverlayCondition_OnBirth);
        if (ov->BeginWindow("A"))
        {
            ov->Text("First window");
            ov->EndWindow();
        }
        ov->SetNextWindowSize({260.f, 180.f}, Onyx::OverlayCondition_OnBirth);
        if (ov->BeginWindow("B"))
        {
            ov->Text("Second window");
            ov->EndWindow();
        }
    };
}

// a full screen dockspace split between two windows. the app submits its tree only when it has no layout to load, as
// the editor does. docked windows are laid out by their dock node and do not update their own position: they are
// measured through a widget that spans their width
struct DockedGui
{
    bool Submit = true;
    f32 Ratio = 0.5f;
    bool Submitted = false;
    bool LeftDocked = false;
    bool RightDocked = false;
    Onyx::LayoutId Host{};
};
static void dockedGui(UiEngine &ui, DockedGui &state)
{
    ui.Gui = [&state](Onyx::Overlay *ov) {
        state.Host = ov->FullScreenDockSpace();
        if (state.Submit && !state.Submitted)
        {
            // a vertical divider: Left and Right side by side
            ov->SubmitDockTree(state.Host, Onyx::DockSplit(Onyx::LayoutAxis_Vertical, state.Ratio,
                                                           Onyx::DockTabBar("Left"), Onyx::DockTabBar("Right")));
            state.Submitted = true;
        }
        state.LeftDocked = false;
        state.RightDocked = false;
        if (ov->BeginWindow("Left"))
        {
            ov->Button("Left fill", Onyx::OverlayButtonFlag_SpanFullWidth);
            state.LeftDocked = ov->IsCurrentWindowDocked();
            ov->EndWindow();
        }
        if (ov->BeginWindow("Right"))
        {
            ov->Button("Right fill", Onyx::OverlayButtonFlag_SpanFullWidth);
            state.RightDocked = ov->IsCurrentWindowDocked();
            ov->EndWindow();
        }
    };
}

ONYX_TEST(layout, windows_come_back_where_they_were_left,
          .Description = "A saved layout puts floating windows back at the position and size they were left at, over "
                         "what the gui asks for when they are born")
{
    const Path path = TempPath("floating.yaml");
    f32v2 aMin{}, bMin{}, bSize{};
    {
        UiEngine ui{};
        floatingGui(ui);
        ui.Yield(3);
        REQUIRE((ui.GetWindowInfo("A").Found && ui.GetWindowInfo("B").Found));
        // leave both where the gui does not put them: A moved by its header, B grown by its corner
        ui.WindowMove("A", ui.GetWindowInfo("A").Min + f32v2{150.f, 120.f});
        ui.WindowResize("B", ui.GetWindowInfo("B").Size() + f32v2{50.f, 30.f});
        ui.Yield(2);
        aMin = ui.GetWindowInfo("A").Min;
        bMin = ui.GetWindowInfo("B").Min;
        bSize = ui.GetWindowInfo("B").Size();
        REQUIRE_MSG(TKit::Math::Norm(aMin - f32v2{40.f, 50.f}) > 50.f, "A did not move: {}", aMin);

        const auto saved = ui.GetOverlay()->Serialize(path);
        REQUIRE_MSG(saved, "Serialize: {}", Describe(saved));
    }

    UiEngine ui{};
    const auto loaded = ui.GetOverlay()->Deserialize(path);
    REQUIRE_MSG(loaded, "Deserialize: {}", Describe(loaded));
    floatingGui(ui);
    ui.Yield(3);
    REQUIRE((ui.GetWindowInfo("A").Found && ui.GetWindowInfo("B").Found));
    CHECK_THAT(ui.GetWindowInfo("A").Min, IsNear(aMin, 1.f));
    CHECK_THAT(ui.GetWindowInfo("B").Min, IsNear(bMin, 1.f));
    CHECK_THAT(ui.GetWindowInfo("B").Size(), IsNear(bSize, 1.f));
}

ONYX_TEST(layout, dock_trees_come_back_docked,
          .Description = "A saved layout docks windows back into their dockspace, split where they were split")
{
    const Path path = TempPath("docked.yaml");
    {
        UiEngine ui{{640, 480}, Onyx::OverlayFlag_Docking};
        DockedGui state{.Ratio = 0.3f};
        dockedGui(ui, state);
        ui.Yield(6);
        REQUIRE((state.LeftDocked && state.RightDocked));
        const auto saved = ui.GetOverlay()->Serialize(path);
        REQUIRE_MSG(saved, "Serialize: {}", Describe(saved));
    }

    UiEngine ui{{640, 480}, Onyx::OverlayFlag_Docking};
    const auto loaded = ui.GetOverlay()->Deserialize(path);
    REQUIRE_MSG(loaded, "Deserialize: {}", Describe(loaded));
    DockedGui state{.Submit = false};
    dockedGui(ui, state);
    ui.Yield(6);
    const ItemInfo left = ui.GetItem("Left/Left fill");
    const ItemInfo right = ui.GetItem("Right/Right fill");
    REQUIRE((left.Found && right.Found));
    CHECK(state.LeftDocked);
    CHECK(state.RightDocked);
    const f32v2 lmin = left.Min, lmax = left.Max;
    const f32v2 rmin = right.Min, rmax = right.Max;
    CHECK_MSG(lmax[0] <= rmin[0] + 2.f, "Left {}..{} is not left of right {}..{}", lmin, lmax, rmin, rmax);
    // the split was saved at 30% of 640 pixels: the default tree would split in half
    const f32 lw = lmax[0] - lmin[0];
    const f32 rw = rmax[0] - rmin[0];
    CHECK_MSG(lw < rw * 0.6f && lw > 120.f, "The split did not come back at 30%: widths {} and {}", lw, rw);
}

ONYX_TEST(layout, saving_needs_a_path_that_can_be_written,
          .Description = "Serialize reports an error, instead of failing silently, when no path was set or the file "
                         "cannot be written")
{
    UiEngine ui{};
    floatingGui(ui);
    ui.Yield(2);
    Onyx::Overlay *ov = ui.GetOverlay();

    const auto none = ov->Serialize();
    REQUIRE_MSG(!none, "Saved without a path");
    CHECK(none.GetError().GetCode() == Onyx::Error_SerializationFailed);

    const Path nowhere = TempPath("no-such-directory/layout.yaml");
    const auto failed = ov->Serialize(nowhere);
    REQUIRE_MSG(!failed, "Saved into a directory that does not exist");
    CHECK(failed.GetError().GetCode() == Onyx::Error_SerializationFailed);
    CHECK_MSG(failed.GetError().GetMessage().Contains(nowhere.CString()), "The message does not name the path: {}",
              Describe(failed));
}

ONYX_TEST(layout, missing_layout_files_are_reported,
          .Description = "Deserialize tells a missing layout file from a directory where the file should be, and a "
                         "directory path stands for the default file name inside it")
{
    UiEngine ui{};
    Onyx::Overlay *ov = ui.GetOverlay();

    const auto missing = ov->Deserialize(TempPath("none.yaml"));
    REQUIRE(!missing);
    CHECK(missing.GetError().GetCode() == Onyx::Error_FileNotFound);

    const Path dir = TempPath("layouts");
    REQUIRE(MakeDirectory(dir));
    ov->SetSerializationPath(dir);
    const Path resolved = ov->GetSerializationPath();
    REQUIRE_MSG(resolved == dir / "onyx-overlay.yaml", "A directory resolved to {}", resolved);

    // a directory where the layout file should be
    REQUIRE(MakeDirectory(resolved));
    const auto notFile = ov->Deserialize();
    REQUIRE(!notFile);
    CHECK(notFile.GetError().GetCode() == Onyx::Error_IsNotFile);
}

ONYX_TEST(layout, broken_layouts_are_rejected_and_change_nothing,
          .Description = "A layout file with a missing key, a value of the wrong kind, a bad dock split or an unknown "
                         "parent is reported, and nothing in it is applied, not even what comes before the fault")
{
    UiEngine ui{};
    floatingGui(ui);
    ui.Yield(3);
    REQUIRE(ui.GetWindowInfo("A").Found);
    const f32v2 before = ui.GetWindowInfo("A").Min;

    // every file moves A first, then breaks: a layout that is applied as it is read would leave A moved
    const u64 a = Onyx::LayoutId{"A"}.Id;
    const String moveA = String::Format(
        "Windows:\n  {}: {{Position: [300, 300], Flags: 0, Size: [200, 150], Layer: 0, Docked: 0, DockHost: 0}}\n", a);
    const String host = "  99: {Position: [0, 0], Flags: 0, Size: [400, 300], Layer: 0, Docked: 0, DockHost: 1}\n";
    const String emptyLeaf = "{Flags: 0, Leaf: 1}";
    const auto split = [&](const char *axis, const char *ratio, const bool twoChildren) {
        return moveA + host + "DockTrees:\n  99:\n    Flags: 0\n    Leaf: 0\n    Ratio: " + ratio +
               "\n    Axis: " + axis + "\n    Children:\n      - " + emptyLeaf + "\n" +
               (twoChildren ? "      - " + emptyLeaf + "\n" : "");
    };
    struct Case
    {
        const char *Name;
        String Text;
    };
    // a file that is not YAML at all is covered by layout.files_that_are_not_yaml_are_rejected
    const Case cases[] = {
        {"a window without a size",
         String::Format("Windows:\n  {}: {{Position: [300, 300], Flags: 0, Layer: 0, Docked: "
                        "0, DockHost: 0}}\n",
                        a)},
        {"a size that is not a vector", String::Format("Windows:\n  {}: {{Position: [300, 300], Flags: 0, Size: big, "
                                                       "Layer: 0, Docked: 0, DockHost: 0}}\n",
                                                       a)},
        {"a window id that is not a number", moveA + "  not-an-id: {Position: [0, 0], Flags: 0, Size: [10, 10], "
                                                     "Layer: 0, Docked: 0, DockHost: 0}\n"},
        {"a parent that does not exist", moveA + "  77: {Parent: 12345, Flags: 0, Size: [10, 10], Layer: 0, Docked: "
                                                 "0, DockHost: 0}\n"},
        {"a split ratio past 1", split("1", "1.5", true)},
        {"an axis that does not exist", split("7", "0.5", true)},
        {"a split with one child", split("1", "0.5", false)},
    };

    u32 index = 0;
    for (const Case &c : cases)
    {
        const Path path = TempPath(String::Format("broken-{}.yaml", index++).CString());
        REQUIRE(WriteFile(path, c.Text));
        const auto result = ui.GetOverlay()->Deserialize(path);
        CHECK_MSG(!result, "{}: the layout was accepted", c.Name);
        if (!result)
            CHECK_MSG(result.GetError().GetCode() == Onyx::Error_DeserializationFailed, "{}: {}", c.Name,
                      Describe(result));
        ui.Yield(2);
        CHECK_MSG(TKit::Math::Norm(ui.GetWindowInfo("A").Min - before) < 1.f, "{}: A moved from {} to {}", c.Name,
                  before, ui.GetWindowInfo("A").Min);
    }
}

// Fails until fixed: Toolkit's YAML parser does not return syntax errors: in debug builds its ryml parse callback is
// TKIT_PANIC, and release builds keep ryml's default handler, which aborts. So YamlTree::FromFile never fails on a
// malformed file, and Deserialize on one ends the process before its own checks run. The editor reads its layout at
// startup. The fix is in Toolkit: a parse callback that hands control back to FromString (ryml's callback must not
// return), so FromFile can report the error
ONYX_TEST(layout, files_that_are_not_yaml_are_rejected,
          .Description =
              "A layout file that is not YAML at all (truncated, overwritten, the wrong file) is reported as "
              "broken and changes nothing")
{
    UiEngine ui{};
    floatingGui(ui);
    ui.Yield(3);
    REQUIRE(ui.GetWindowInfo("A").Found);
    const f32v2 before = ui.GetWindowInfo("A").Min;

    const Path path = TempPath("not-yaml.yaml");
    REQUIRE(WriteFile(path, "Windows: [this is: not, {closed\n"));
    const auto result = ui.GetOverlay()->Deserialize(path);
    CHECK_MSG(!result, "A file that is not YAML was accepted");
    if (!result)
        CHECK(result.GetError().GetCode() == Onyx::Error_DeserializationFailed);
    ui.Yield(2);
    CHECK_THAT(ui.GetWindowInfo("A").Min, IsNear(before, 1.f));
}

// Fails until fixed: Deserialize checks the shape of each dock tree but not its windows: a window listed in two leaves
// passes, and while applying it ApplyDockTree's 'already docked elsewhere' assert ends the process (release builds skip
// the assert and dock the window twice). The editor loads its layout this way at startup, so a damaged layout file
// crashes it instead of falling back to the default layout. Checking that every window id appears in one leaf at most,
// across all trees, fixes it
ONYX_TEST(layout, a_window_in_two_dock_leaves_is_rejected,
          .Description = "A layout that docks the same window in two places is reported as broken, like any other "
                         "layout that cannot be applied")
{
    UiEngine ui{{640, 480}, Onyx::OverlayFlag_Docking};
    DockedGui state{.Submit = false};
    dockedGui(ui, state);
    ui.Yield(2);

    const u64 host = state.Host.Id;
    const u64 left = Onyx::LayoutId{"Left"}.Id;
    const String text = String::Format(
        "Windows:\n"
        "  {0}: {{Position: [0, 0], Flags: 196608, Size: [640, 480], Layer: 0, Docked: 0, DockHost: 1}}\n"
        "  {1}: {{Position: [0, 0], Flags: 0, Size: [320, 480], Layer: 0, Docked: 1, DockHost: 0}}\n"
        "DockTrees:\n"
        "  {0}:\n"
        "    Flags: 0\n    Leaf: 0\n    Ratio: 0.5\n    Axis: 1\n    Children:\n"
        "      - {{Flags: 0, Leaf: 1, Windows: [{1}]}}\n"
        "      - {{Flags: 0, Leaf: 1, Windows: [{1}]}}\n",
        host, left);
    const Path path = TempPath("twice-docked.yaml");
    REQUIRE(WriteFile(path, text));
    const auto result = ui.GetOverlay()->Deserialize(path);
    CHECK_MSG(!result, "A window docked in two leaves was accepted");
    ui.Yield(2);
}

// Fails until fixed: Deserialize applies each saved tree with ApplyDockTree on top of the dockspace's current tree. The
// windows it lists are still docked there, so in debug builds the 'already docked elsewhere' assert ends the process:
// loading a layout at runtime, or the same layout twice, crashes. Release builds skip the assert and pass this test,
// but ApplyDockTree gives the root new children without freeing the old ones, which stay orphaned in m_DockNodes: every
// load leaks a tree. Undocking a host's windows and freeing its tree before applying the saved one fixes both
ONYX_TEST(layout, a_layout_loads_over_a_docked_one,
          .Description = "Loading a layout into an overlay whose dockspace is already docked (the app's default tree, "
                         "or the same layout loaded before) replaces what is there",
          .KnownIssue = "In general, user should try to undock a window that wants to be re-docked. Leaving this here "
                        "in case we want to automate this process when applying a dock tree automatically")
{
    UiEngine ui{{640, 480}, Onyx::OverlayFlag_Docking};
    DockedGui state{};
    dockedGui(ui, state);
    ui.Yield(6);
    REQUIRE((state.LeftDocked && state.RightDocked));

    const Path path = TempPath("current.yaml");
    const auto saved = ui.GetOverlay()->Serialize(path);
    REQUIRE_MSG(saved, "Serialize: {}", Describe(saved));
    const auto loaded = ui.GetOverlay()->Deserialize(path);
    REQUIRE_MSG(loaded, "Deserialize: {}", Describe(loaded));
    ui.Yield(4);
    CHECK(state.LeftDocked);
    CHECK(state.RightDocked);
}

#else
ONYX_TEST(layout, serialization_is_compiled_in, .Description = "Layouts need TOOLKIT_ENABLE_YAML_SERIALIZATION")
{
    SKIP("TOOLKIT_ENABLE_YAML_SERIALIZATION is off");
}
#endif
