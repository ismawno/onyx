# Onyx runtime tests

`onyx-tests` checks that Onyx behaves as intended at runtime: windows, views and cameras, render contexts, what ends up
on screen (pixels are read back from render textures), resources and loaders (and the errors they report), input, the
overlay (driven like a user would drive it) and its saved layouts, and audio. Toolkit has tests of its own: nothing here
tests tensors, containers or allocators.

## Building and running

The tests build with the rest of Onyx: the presets turn `ONYX_BUILD_TESTS` on.

```sh
cmake --preset debug && cmake --build --preset debug
./build/debug/tests/onyx-tests                 # everything, one test at a time
./build/debug/tests/onyx-tests --jobs 4        # four at a time
./build/debug/tests/onyx-tests window 'overlay.*' '-*msaa*'   # suites, patterns, exclusions
./build/debug/tests/onyx-tests --list          # what there is
./build/debug/tests/onyx-tests --worker render.quad_covers_its_area   # one test, in this process (for a debugger)
```

The control center runs the same executable from its Tests window, and so does CI (`.github/workflows/ci.yml`).
`ctest` works too: it runs the whole suite as one test.

Every test runs in a process of its own. A test initializes and terminates Onyx, opens windows and may crash: an
assert, a validation error (debug builds panic on them) or a signal only takes its own test down, and is reported as a
crash with the last lines it printed. A watchdog ends tests that hang (60 s by default, `--timeout-scale` stretches it).

Reports: `--junit <file>` writes JUnit XML, `--summary <file>` a Markdown summary (on GitHub Actions it goes to the job
summary on its own, and failures become annotations). `--format events` prints a line protocol for tools: that is what
the control center reads.

### Without a GPU, or without a display

Tests need a Vulkan device and a window system. On a machine without a GPU, Mesa's lavapipe works:

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json xvfb-run -a ./build/debug/tests/onyx-tests --jobs 2
```

`xvfb-run` provides the display on headless Linux. Pixel tests render into render textures, so they need no visible
window. The audio tests skip when there is no output device; on a headless Linux machine a PulseAudio null sink is
enough (`pulseaudio --start && pactl load-module module-null-sink`).

The Vulkan validation layers must be at least as new as the Vulkan headers Onyx builds with (1.4.328 when vulkit fetches
them), or they reject structures they do not know and every test crashes at Initialize. Use the LunarG SDK's layers,
or turn validation off with `VK_LOADER_LAYERS_DISABLE='*validation*'`.

### Tiling window managers (Hyprland, sway)

Onyx uses X11 on Linux by default, so on Hyprland or sway the test windows go through XWayland. Test windows are fixed
size (`TestWindowSpecs::Resizable` is off unless a test needs otherwise; `SetSize` still works), which these window
managers take as a reason to float a window instead of tiling it: tests get the size they ask for. If yours tiles them
anyway, add a float rule for windows whose title starts with `onyx-tests` (on sway: `for_window [title="^onyx-tests"]
floating enable`). `OpenTestWindow` also waits for the window size to settle, as a compositor may still resize a window
right after mapping it. Under a Wayland compositor the compositor places windows, so `window.position_reads_back`
skips.

## Writing tests

A test is a Catch2 test case declared with `ONYX_TEST`, in `suites/<suite>.cpp`. New files there are picked up by
CMake.

```cpp
#include "harness.hpp"
using namespace OnyxTest;

ONYX_TEST(render, quad_covers_its_area, .Description = "A unit quad covers the middle half of a view 2 units tall")
{
    Scene<D2> scene{};                        // a render texture, a view on it and a context targeting the view
    scene.Camera.OrthoParameters.Size = 2.f;
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->Quad();
    const Capture img = Render(scene.Target); // render a couple of frames and read the pixels back
    CHECK_THAT(img.AtNormalized(0.5f, 0.5f), IsColor(ToBytes(Onyx::Color_Red), 3));
}
```

- Catch2's assertions apply. `CHECK` records a failure and goes on, `REQUIRE` ends the test (Onyx and Catch2 build
  without exceptions: the worker process ends, and the runner reports the failure). Expressions are decomposed, so a
  failure shows the values: `0 == 1`. `CHECK_THAT` takes a matcher: `IsNear(expected, epsilon)` for numbers, vectors and
  matrices, `IsColor(rgba, tolerance)` for pixels. `INFO` and `CAPTURE` add context. `SKIP("...")` skips the test (no
  audio device, a platform without the feature), `FAIL("...")` fails it.
- `CHECK_MSG(condition, "format {}", args...)` and `REQUIRE_MSG` say why they failed, with a message for that check only.
  Catch2 does not decompose `&&`, `||` or chained comparisons: wrap those in parentheses.
- Logs use Toolkit logging (`TKIT_LOG_INFO` and the rest). A test's output is shown when it fails, or always with
  `--verbose`.
- Options: `.Description`, `.Init` (`Init_Full`: Onyx and its default resources, the default; `Init_Bare`; `Init_Manual`
  for tests that initialize Onyx themselves) and `.Timeout` in seconds.
- `.KnownIssue = "what is wrong"` is for a bug you decided to leave for later: the test is reported as KNOWN and does not
  fail the run. Once it passes, the runner says so: remove the option then. A bug that is to be fixed keeps failing.
- Helpers (`framework/harness.hpp`): `Frame(n)` runs frames, `WaitFor(condition)` runs frames until something
  asynchronous (a resize, a swapchain recreation) happened, `OpenTestWindow()`, `Scene<D>`, `Capture` (pixel probes and
  counts).
- Files: `TempPath(name)` gives a path in a directory of the test's own (tests run in parallel), which is removed when
  the test ends. `WriteFile` and `MakeDirectory` fill it. Loaders and layouts are tested with files written this way, so
  no assets are needed.

Why Catch2 and a runner of its own: Catch2 runs the test bodies and their assertions, as in Toolkit and vulkit. It runs
a suite in a single process, though, and these tests initialize Onyx, open windows and may crash or hang: the runner
(`framework/supervisor.cpp`) gives each test a process of its own, a watchdog, the reports and the event stream the
control center reads.

### The overlay test engine

The overlay tests are written against an engine in the spirit of Dear ImGui's test engine (`framework/engine.hpp`). A
test gives it a gui and then plays the user, naming items by path:

```cpp
UiEngine ui{};                                 // a window with virtual input and an overlay on it
bool value = false;
ui.Gui = [&](Onyx::Overlay *ov) {              // declared again every frame, as any immediate mode gui
    if (ov->BeginWindow("Settings")) {
        ov->CheckBox("Enabled", &value);
        ov->EndWindow();
    }
};
ui.SetRef("Settings");
ui.ItemClick("Enabled");                       // moves there, presses, releases, with frames in between
CHECK(value);
```

- Paths are the ids the overlay builds: `"Window/Item"`, through `PushId` scopes, tree nodes, child windows and popups
  (`"Test/Popup/Close"`), dropdowns (`"Fruit/Banana"`) and tab bars (`"Tabs/Two"` is a tab's header). `"##MenuBar"`
  stands for a window's menu bar. Labels keep their rules: `"Same##a"`, `"###id"`. A path that starts with `//` ignores
  the reference set with `SetRef`.
- Actions: `ItemClick`, `ItemCtrlClick`, `ItemDoubleClick`, `ItemHover`, `ItemDrag`, `ItemDragAndDrop`, `ItemInput`
  (types into a text input, a numeric one, a slider or a drag), `MenuClick("File/Open")`, `WindowMove`, `WindowResize`,
  `KeyPress`, `KeyChars`, and the raw `MouseMove`, `MouseDown`/`MouseUp`, `MouseWheel`. `GetItem` and `GetWindowInfo`
  say where things are, `ItemExists` whether something is on screen. `Yield(n)` and `YieldUntil(condition)` run frames.
- Items are looked up in what the overlay drew on the last frame, and an action waits for its item to hold still before
  it acts, so a window manager resizing the window right after mapping it does not make a click miss.
- Every action is logged and attached to the next check: a failure lists what the engine did right before it.
- `--speed normal` or `--speed cinematic` (or the control center's speed buttons) moves the mouse at a person's pace,
  paces frames to about 60 a second and draws the cursor, so an overlay test can be watched.

The engine finds windows through `Overlay::findWindow`, reached without changing Onyx (`engine.cpp` says how). A public
`Overlay::FindWindow` would replace that.

## Bugs these tests found

Tests that pin a bug to be fixed say so in a `// Fails until fixed:` comment above them, with what goes wrong and what
fixes it. They fail until then.
