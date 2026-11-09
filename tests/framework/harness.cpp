#include "harness.hpp"
#include "tkit/filesystem/file.hpp"
#include "tkit/filesystem/filesystem.hpp"
#include "tkit/preprocessor/system.hpp"
#include "tkit/profiling/clock.hpp"
#include <cstdlib>
#include <cstring>

namespace OnyxTest
{
// ---------------------------------------------------------------------------------------------------------------------
//  Temporary files
// ---------------------------------------------------------------------------------------------------------------------

static TKit::DynamicString s_TempDirectory{};

static const TKit::DynamicString &tempDirectory()
{
    if (!s_TempDirectory.IsEmpty())
        return s_TempDirectory;

    const auto system = TKit::GetTempDirectoryPath();
    TKit::FileSystemPath dir = system ? *system : TKit::FileSystemPath{"."};
    // tells apart processes started in the same instant only by the clock's resolution: nanoseconds are enough
    dir /= TKit::DynamicString::Format("onyx-tests-{}", TKit::Clock::GetCurrentTime()).CString();
    s_TempDirectory = dir.CString();
    [[maybe_unused]] const auto created = TKit::CreateDirectories(s_TempDirectory);
    return s_TempDirectory;
}

TKit::DynamicString TempPath(const char *name)
{
    TKit::FileSystemPath path{tempDirectory().CString()};
    path /= name;
    return TKit::DynamicString{path.CString()};
}
bool WriteFile(const TKit::DynamicString &path, const void *data, const usz size)
{
    return bool(TKit::WriteFile(path, data, size));
}
bool WriteFile(const TKit::DynamicString &path, const TKit::StringView text)
{
    return bool(TKit::WriteFile(path, text));
}
bool WriteFile(const TKit::DynamicString &path, const char *text)
{
    return WriteFile(path, TKit::StringView{text, usize(std::strlen(text))});
}
bool MakeDirectory(const TKit::DynamicString &path)
{
    return bool(TKit::CreateDirectories(path));
}
void RemoveTempFiles()
{
    // Toolkit's filesystem allocates from the tier allocator Onyx pushes: after Terminate there is none
    if (s_TempDirectory.IsEmpty() || !TKit::GetTier())
        return;
    [[maybe_unused]] const auto removed = TKit::RemoveAll(s_TempDirectory);
    s_TempDirectory.Clear();
}

// ---------------------------------------------------------------------------------------------------------------------
//  Frames and windows
// ---------------------------------------------------------------------------------------------------------------------

void Frame(const u32 count)
{
    for (u32 i = 0; i < count; ++i)
    {
        Onyx::Running();
        Onyx::Transfer();
        Onyx::Render();
    }
}

Onyx::Window *OpenTestWindow(const TestWindowSpecs &specs)
{
    Onyx::OpenWindowSpecs ospecs{};
    ospecs.Window.Title = specs.Title;
    ospecs.Window.ScreenSize = specs.Size;
    ospecs.Window.Flags = specs.Resizable ? specs.Flags : (specs.Flags | Onyx::WindowFlag_NoResize);
    ospecs.Window.PresentMode = specs.PresentMode;
    // frames as fast as possible: tests wait for conditions, not for the monitor
    ospecs.Flags = Onyx::OpenWindowFlag_NoDefaultDeltaTime;
    Onyx::Window *win = Onyx::OpenWindow(ospecs);
    // a freshly mapped window may report a zero framebuffer for a moment, and a window manager may resize it right
    // after mapping it: wait for a size that holds for a little while
    u32v2 last{0};
    TKit::Clock held{};
    WaitFor([&] {
        const u32v2 pixels = Graph::Window_GetPixelSize(win->GetHandle());
        if (pixels != last)
        {
            last = pixels;
            held.Restart();
            return false;
        }
        return pixels[0] != 0 && pixels[1] != 0 && held.GetElapsed().AsMilliseconds<f64>() >= 50.0;
    });
    return win;
}

Graph::Platform GetPlatform()
{
    return Graph::GetPlatformBackendInfo().Platform;
}
bool CompositorPlacesWindows()
{
    if (GetPlatform() == Graph::Platform_Wayland)
        return true;
#ifdef TKIT_OS_LINUX
    const char *display = std::getenv("WAYLAND_DISPLAY");
    return display && *display;
#else
    return false;
#endif
}

// ---------------------------------------------------------------------------------------------------------------------
//  Pixels
// ---------------------------------------------------------------------------------------------------------------------

u8v4 Capture::At(const u32 x, const u32 y) const
{
    TKIT_ASSERT(x < Size[0] && y < Size[1], "[ONYX-TESTS] Pixel ({}, {}) is outside a {}x{} capture", x, y, Size[0],
                Size[1]);
    const u32 i = 4 * (y * Size[0] + x);
    return u8v4{Rgba[i], Rgba[i + 1], Rgba[i + 2], Rgba[i + 3]};
}
u8v4 Capture::AtNormalized(const f32 x, const f32 y) const
{
    const u32 px = u32(TKit::Math::Clamp(x * f32(Size[0]), 0.f, f32(Size[0] - 1)));
    const u32 py = u32(TKit::Math::Clamp(y * f32(Size[1]), 0.f, f32(Size[1] - 1)));
    return At(px, py);
}
u32 Capture::Count(const u8v4 &color, const u32 tolerance) const
{
    return Count(color, u32v2{0}, Size, tolerance);
}
u32 Capture::Count(const u8v4 &color, const u32v2 &min, const u32v2 &max, const u32 tolerance) const
{
    u32 count = 0;
    for (u32 y = min[1]; y < max[1] && y < Size[1]; ++y)
        for (u32 x = min[0]; x < max[0] && x < Size[0]; ++x)
            count += Near(At(x, y), color, tolerance);
    return count;
}

Capture Read(const Onyx::RenderTexture *target)
{
    Capture capture{};
    capture.Size = target->GetPixelSize();
    capture.Rgba.Resize(u32(target->GetBytes()));
    if (!target->ReadPixels(capture.Rgba.GetData()))
        capture.Rgba.Clear();
    return capture;
}
Capture Render(const Onyx::RenderTexture *target, const u32 frames)
{
    Frame(frames);
    return Read(target);
}

u8v4 ToBytes(const Onyx::Color &color)
{
    const f32v4 &c = color.rgba;
    const auto byte = [](const f32 v) { return u8(TKit::Math::Clamp(v, 0.f, 1.f) * 255.f + 0.5f); };
    return u8v4{byte(c[0]), byte(c[1]), byte(c[2]), byte(c[3])};
}
bool Near(const u8v4 &a, const u8v4 &b, const u32 tolerance)
{
    for (u32 i = 0; i < 4; ++i)
    {
        const i32 d = i32(a[i]) - i32(b[i]);
        if (u32(d < 0 ? -d : d) > tolerance)
            return false;
    }
    return true;
}

template <Dimension D> Scene<D>::Scene(const u32v2 &size, const Onyx::RenderViewSpecs &specs)
{
    Target = Onyx::CreateRenderTexture(size);
    View = Target->CreateRenderView<D>(&Camera, specs);
    Context = Onyx::CreateRenderContext<D>();
    // a context only knows the default meshes and font after its first flush
    Context->Flush();
    Context->AddTarget(View);
}
template <Dimension D> Scene<D>::~Scene()
{
    Onyx::DestroyRenderContext(Context);
    Target->DestroyRenderView(View);
    Onyx::DestroyRenderTexture(Target);
}
template struct Scene<D2>;
template struct Scene<D3>;
} // namespace OnyxTest
