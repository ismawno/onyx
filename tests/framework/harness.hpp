#pragma once

// Helpers the suites share: running frames, opening test windows, reading pixels back and driving an overlay with
// injected input, the way a person would.

#include "test.hpp"
#include "onyx/onyx.hpp"
#include "onyx/overlay.hpp"
#include "onyx/resources.hpp"
#include <catch2/matchers/catch_matchers.hpp>

namespace OnyxTest
{
using Onyx::D2;
using Onyx::D3;
using Onyx::Dimension;

// ---------------------------------------------------------------------------------------------------------------------
//  Temporary files
// ---------------------------------------------------------------------------------------------------------------------

// a path for a file the test writes. every test process gets a directory of its own under the system's temporary
// directory (tests run in parallel), and the runner removes it when the test ends. nothing is created at that path.
// it is the path type Onyx takes: loaders, layouts and audio get it as it is
Path TempPath(const char *name);
// false if the file could not be written
bool WriteFile(const Path &path, const void *data, usz size);
bool WriteFile(const Path &path, StringView text);
bool WriteFile(const Path &path, const char *text);
// creates the directory and its parents. false if it could not
bool MakeDirectory(const Path &path);

// a c string as a Toolkit string, so checks can compare it and Catch2 prints it: CHECK(Text(buffer) == "hello")
inline String Text(const char *text)
{
    return String{text};
}
// what an Onyx result has to say, for the message of a check: REQUIRE_MSG(result, "{}", Describe(result))
template <typename T> String Describe(const Onyx::Result<T> &result)
{
    if (result)
        return String{"ok"};
    // Onyx's own string, whatever String is
    const auto message = result.GetError().ToString();
    return String{message};
}

// ---------------------------------------------------------------------------------------------------------------------
//  Frames and windows
// ---------------------------------------------------------------------------------------------------------------------

// one turn of the frame loop: poll the platform, upload what is pending and render every window and render texture
void Frame(u32 count = 1);

// windows resize, move and recreate swapchains asynchronously. tests wait for the platform instead of assuming it is
// instantaneous: frames run until the condition holds, at most maxFrames of them. the condition is checked right after
// polling, so it sees this frame's events (the render that follows flushes them)
template <typename F> bool WaitFor(F condition, const u32 maxFrames = 240)
{
    for (u32 i = 0; i < maxFrames; ++i)
    {
        Onyx::Running();
        if (condition())
            return true;
        Onyx::Transfer();
        Onyx::Render();
    }
    return condition();
}

// test windows are small, unpaced (frames run as fast as they can) and use immediate presentation. they are fixed size
// too (SetSize still works): tiling window managers such as Hyprland and sway float fixed size windows instead of
// tiling them, so a test gets the size it asks for instead of a tile that changes right after the window maps
struct TestWindowSpecs
{
    u32v2 Size{320, 240};
    Onyx::WindowFlags Flags = 0;
    const char *Title = "onyx-tests";
    Onyx::PresentMode PresentMode = Onyx::PresentMode_Immediate;
    bool Resizable = false;
};
// opens the window and waits until its size has settled: the window manager may still resize it after mapping it
Onyx::Window *OpenTestWindow(const TestWindowSpecs &specs = {});

// the platform the window system runs on. a few behaviours differ (wayland cannot place windows, for one)
Graph::Platform GetPlatform();
// true when a Wayland compositor decides where windows go: on Wayland, and on X11 through XWayland (Onyx uses X11 on
// Linux by default, so this is the case on Hyprland, sway or a GNOME/KDE Wayland session)
bool CompositorPlacesWindows();

// ---------------------------------------------------------------------------------------------------------------------
//  What a test leaves behind. Tests that pass share Onyx with the tests after them (worker.cpp), so a test gives it
//  back as it found it. Windows left open are closed for it. A render context or a render texture made with these
//  lives until the test ends and is destroyed then, the latest first: use them for what the test does not destroy
//  itself. Scene and UiEngine clean up after themselves
// ---------------------------------------------------------------------------------------------------------------------

template <Dimension D> Onyx::RenderContext<D> *TestContext(const Onyx::RenderContextSpecs &specs = {});
Onyx::RenderTexture *TestRenderTexture(const u32v2 &size, const Onyx::RenderTextureSpecs &specs = {});

// ---------------------------------------------------------------------------------------------------------------------
//  Pixels
// ---------------------------------------------------------------------------------------------------------------------

// pixels read back from a render texture: 8 bit RGBA, sRGB encoded, rows top to bottom
struct Capture
{
    u32v2 Size{0};
    Array<u8> Rgba{};

    bool IsEmpty() const
    {
        return Rgba.IsEmpty();
    }
    u8v4 At(u32 x, u32 y) const;
    // normalized coordinates: (0, 0) is the top left corner, (1, 1) the bottom right one
    u8v4 AtNormalized(f32 x, f32 y) const;
    // how many pixels are within tolerance of a color, in the given region (pixels, inclusive min, exclusive max)
    u32 Count(const u8v4 &color, u32 tolerance = 3) const;
    u32 Count(const u8v4 &color, const u32v2 &min, const u32v2 &max, u32 tolerance = 3) const;
};

// renders the frames a render texture needs to be up to date (contexts upload on Transfer, views composite on
// Render) and reads its last image back
Capture Render(const Onyx::RenderTexture *target, u32 frames = 2);
Capture Read(const Onyx::RenderTexture *target);

// the bytes a color produces on an sRGB target: what the readback of an opaque fill of that color must be
u8v4 ToBytes(const Onyx::Color &color);
bool Near(const u8v4 &a, const u8v4 &b, u32 tolerance = 3);

// CHECK_THAT(capture.AtNormalized(0.5f, 0.5f), IsColor(red, 3)): every channel within the tolerance. a failure prints
// the pixel and the color expected
class ColorMatcher final : public Catch::Matchers::MatcherBase<u8v4>
{
  public:
    ColorMatcher(const u8v4 &expected, const u32 tolerance) : m_Expected(expected), m_Tolerance(tolerance)
    {
    }
    bool match(const u8v4 &actual) const override
    {
        return Near(actual, m_Expected, m_Tolerance);
    }
    std::string describe() const override
    {
        return fmt::format("is within {} of {}", m_Tolerance, m_Expected);
    }

  private:
    u8v4 m_Expected;
    u32 m_Tolerance;
};
inline ColorMatcher IsColor(const u8v4 &expected, const u32 tolerance = 3)
{
    return ColorMatcher{expected, tolerance};
}

// a render texture with one 2D or 3D view and a context targeting it: the smallest scene a pixel test needs
template <Dimension D> struct Scene
{
    Onyx::RenderTexture *Target = nullptr;
    Onyx::Camera<D> Camera{};
    Onyx::RenderView<D> *View = nullptr;
    Onyx::RenderContext<D> *Context = nullptr;

    explicit Scene(const u32v2 &size = {64, 64}, const Onyx::RenderViewSpecs &specs = {});
    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    ~Scene();
};
} // namespace OnyxTest
