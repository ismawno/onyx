# Onyx

Onyx is a C++ graphics engine implemented with Vulkan that I plan to use for any project requiring geometry visualization or a GUI.

I started this project with very little experience with graphics programming. This project represents my first serious "deep dive" into the Vulkan API, which is also the first graphics API I have ever used. I am sure there is a lot of room for improvement but I am happy with how the project has evolved. I originally considered starting with OpenGL, but I eventually decided against it for two main reasons:

1. OpenGL is deprecated in MacOS and I want nice multi-platform support.
2. Vulkan is growing more popular every year due to its versatility and optimization opportunities. Since I recently started graphics programming, I might as well learn a modern API.

## Design

Onyx is designed for users that wish to create simple and fast visualization applications, such as particle simulations, physics engines, geometry visualization etc. It provides a high-level, immediate-mode rendering API while using Vulkan under the hood for performance and scalability. Onyx has been designed with some conveniences: rendering is automatically batched with indirect draw calls, render contexts support dirty tracking to avoid redundant host-device communication, and a considerable part of the rendering API is thread safe and scales well with multiple cores.

Onyx natively supports 2D and 3D rendering with a unified API that lets the user choose the dimensionality of their visualization at compile time. This is advantageous for the 2D case as this allows the library to be more specific. It is also ergonomic, as the vector and matrix types will always match the dimension being used, and the API will not expose functionality that does not make sense for a 2D or 3D application. It of course comes with some caveats, such as a part of the API being templated. Many Onyx objects are templated with the `Dimension` enum, which can take either the `D2` or `D3` values. For simplicity, most examples shown in this README will use the `D2` API unless noted otherwise.

The use of C++ features is somewhat limited. The STL is rarely used, as Onyx uses its own custom containers and allocators provided by the [Toolkit](https://github.com/ismawno/toolkit) library (which I also developed), and features like inheritance are avoided almost completely. Heap allocations are also limited. Custom allocators (arenas, stacks and general-purpose allocators) are heavily used and directly embedded into all custom containers.

### Initialization

Onyx uses a global initialization and termination model. Before using any part of the API, `Onyx::Initialize()` must be called, and `Onyx::Terminate()` must be called before the program exits. Default resources (built-in meshes, samplers, etc.) are created with `Onyx::Resources::CreateDefaultResources()`, which is a convenient but not at all necessary function. Without it, all resources must be manually created and specified when drawn.

```cpp
Onyx::Initialize();
Onyx::Resources::CreateDefaultResources();

// ... your application ...

Onyx::Terminate();
```

### Windows

Windows are created with `Onyx::OpenWindow()` and closed with `Onyx::CloseWindow()`. The global `Onyx::Running()` function returns `false` when all windows have been closed or `Onyx::Quit()` has been called.

```cpp
Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});
```

Onyx supports multiple windows. Each of them is independent to an extent.

A window on its own doesn't render anything - it needs one or more render views to display content.

### Render views

A render view is a viewport into the window with its own camera, scissor region, attachments and feature flags. Views are created from a window and are what render contexts ultimately target.

```cpp
Onyx::Camera<D2> cam{};
Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);
```

Views can be configured with flags to enable features like shadows (`RenderViewFlag_Shadows`) or order-independent transparency (`RenderViewFlag_Transparency`). Each view also has a configurable viewport that controls what portion of the window it occupies and can operate in both normalized or absolute coordinates:

```cpp
view->SetNormalizedViewport({.Position = {0.f, 0.f}, .Extent = {0.5f, 1.f}}); // left half of the window
```

This makes it easy to set up things like split-screen layouts or picture-in-picture views within a single window.

### Render contexts

Render contexts are the main way of communicating with the rendering API. It is the object with which the user will render and manipulate primitives, meshes and lights with an immediate-mode style. Contexts may prepare and expect inputs in 2D or 3D form depending on its dimension template parameter (`D2` or `D3`).

Contexts are independent of windows. They are created globally and then pointed at one or more render views:

```cpp
Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
ctx->AddTarget(view);
```

A single context can target multiple views, and a single view can be targeted by multiple contexts. This means you can, for example, record a static background scene in one context and an animated foreground in another, both rendering into the same view.

Before using a context for the first time in a given frame, the method `Flush()` must be called, which resets its state and render data, allowing the user to draw a new frame. `Flush()` should only be called when explicitly re-using the context to update the scene. If the context represents a static scene updated only in isolated scenarios, it should be left alone - the context will avoid re-creating and sending the data to the device, which can be unnecessary. This makes rendering static scenes potentially more efficient as there is no host-device communication. Because of this, scenes can be divided into different contexts based on update frequency.

An example of its usage may be the following:

```cpp
ctx->Flush();

ctx->Push();
ctx->Scale(2.f);
ctx->Quad();
ctx->Translate(0.f, 10.f);
ctx->Circle();
ctx->Pop();

ctx->RoundedRect();
```

Most of the calls to `RenderContext` modify its internal state or store draw commands. At the end of the frame, the stored render data is sent to the device and rendered. All `RenderContext` instances are independent and can be used in different threads. It is possible to divide a scene between many contexts to allow parallel rendering.

### Cameras

The purpose of a camera is pretty self-explanatory in this context. Cameras are value types that can be created on the stack and passed to render views, although must remain alive until the render view is destroyed. 2D cameras are simple orthographic cameras, while 3D ones allow for other types of perspective projections.

```cpp
Onyx::Camera<D2> cam{};
cam.OrthoParameters.Size = 10.f;
```

Cameras provide convenient methods to modify their view and projection matrices through code or user input. Windows expose a `ControlCamera()` method for built-in basic camera controls:

```cpp
win->ControlCamera(Onyx::GetDeltaTime(win), view->GetCamera());
```

### Frame loop

The frame loop is split into two explicit steps: `Transfer()` and `Render()`. `Transfer()` uploads context data to the GPU (updating meshes, lights, etc.), and `Render()` records and submits the actual draw commands. This separation allows Onyx to skip transfers for contexts that haven't changed and be able to keep rendering them. All examples show these two functions being called with the same frequency, but this is not enforced. `Transfer()` can be called with a lower frequency. Calling it multiple times with no `Render()` in between will only waste resources. Contexts that are meant to be continuously updated should be recorded at the same frequency `Transfer()` is called.

```cpp
while (Onyx::Running())
{
    // ... update contexts ...

    Onyx::Transfer();
    Onyx::Render();
}
```

## Examples

All examples shown here are available under the [demo](https://github.com/ismawno/onyx/tree/main/demo) directory. Be sure to set `ONYX_BUILD_DEMOS` to `ON` in `CMake` to build them.

**Please note that, while all examples should pretty much work as-is, treat them as pseudocode. Onyx is under constant change and maintaining these examples in plain text may introduce errors. To check a working version, take a look at the demo source files directly.**

### Hello World

This is one of the simplest Onyx programs. It creates a window, a camera, a render view, and a render context, and draws a rotating triangle:

```cpp
#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"

using Onyx::D2;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});

    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(view);

    f32 time = 0.f;
    while (Onyx::Running())
    {
        time += Onyx::GetDeltaTime(win).AsSeconds();

        ctx->Flush();
        ctx->Rotate(time);
        ctx->Triangle();

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
```

The setup follows the pattern described above: initialize, create default resources, open a window, create a camera and a view, create a context and point it at the view. Each frame, the context is flushed, a rotation is applied based on elapsed time, and a triangle is drawn. `Transfer()` uploads the data and `Render()` draws it.

### Lights

This example demonstrates lighting and shadows with a split-screen layout: a 2D scene on the left half and a 3D scene on the right, both with shadow-casting directional lights:

```cpp
#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"

using Onyx::D2;
using Onyx::D3;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    const Onyx::Resource lit2 = Onyx::Resources::RegisterMaterial<D2>();
    const Onyx::Resource unlit2 = Onyx::Resources::RegisterMaterial<D2>({.Occluder = true});
    const Onyx::Resource mat3 = Onyx::Resources::RegisterMaterial<D3>();
    Onyx::Resources::Sync(Onyx::SyncFlag_Materials);

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});
    const Onyx::RenderViewFlags vflags =
        Onyx::RenderViewFlag_NormalizedCoordinates | Onyx::RenderViewFlag_Shadows;

    // 2D view on the left half
    Onyx::Camera<D2> cam2{};
    Onyx::RenderView<D2> *view2 = win->CreateRenderView<D2>(&cam2, vflags);
    Onyx::RenderContext<D2> *ctx2 = Onyx::CreateRenderContext<D2>();
    cam2.OrthoParameters.Size = 10.f;
    view2->SetNormalizedViewport({.Position = 0.f, .Extent = {0.5f, 1.f}});
    ctx2->AddTarget(view2);

    // 3D view on the right half
    Onyx::Camera<D3> cam3{};
    Onyx::RenderView<D3> *view3 = win->CreateRenderView<D3>(&cam3, vflags);
    Onyx::RenderContext<D3> *ctx3 = Onyx::CreateRenderContext<D3>();
    cam3.View.Translation[2] = 5.f;
    view3->SetNormalizedViewport({.Position = {0.5f, 0.f}, .Extent = {0.5f, 1.f}});
    ctx3->AddTarget(view3);

    f32 time = 0.f;
    while (Onyx::Running())
    {
        const TKit::Timespan dt = Onyx::GetDeltaTime(win);
        time += dt.AsSeconds();

        // Camera controls: whichever view the mouse is over gets input
        Onyx::RenderView<D2> *v2 = win->GetMouseRenderView<D2>();
        Onyx::RenderView<D3> *v3 = win->GetMouseRenderView<D3>();
        if (v2)
            win->ControlCamera(Onyx::GetDeltaTime(win), v2->GetCamera());
        else if (v3)
            win->ControlCamera(Onyx::GetDeltaTime(win), v3->GetCamera());

        // 2D scene: a lit background quad, a shadow-casting directional light,
        // and a rotating red quad that occludes the light
        {
            ctx2->Flush();
            ctx2->RenderFlags(Onyx::RenderModeFlag_Shaded);
            ctx2->Material(lit2);

            ctx2->Push();
            ctx2->Scale(20.f);
            ctx2->Quad();
            ctx2->Pop();

            ctx2->Material(unlit2);
            ctx2->DirectionalLight(
                {.DepthBias = -0.00001f, .Flags = Onyx::LightFlag_CastShadows});
            ctx2->Rotate(time);
            ctx2->FillColor(Onyx::Color_Red);
            ctx2->Quad();
        }

        // 3D scene: a shadow-casting directional light with fitted cascades,
        // a rotating red box, and a large floor quad
        {
            ctx3->Flush();
            ctx3->Material(mat3);
            ctx3->RenderFlags(Onyx::RenderModeFlag_Shaded);

            ctx3->DirectionalLight(
                {.Cascades = {.View = view3}, .Flags = Onyx::LightFlag_CastShadows});

            ctx3->Push();
            ctx3->RotateZ(time);
            ctx3->FillColor(Onyx::Color_Red);
            ctx3->Box();
            ctx3->Pop();

            ctx3->ScaleX(20.f);
            ctx3->ScaleY(20.f);
            ctx3->RotateX(0.5f * Onyx::Math::Pi());
            ctx3->TranslateY(-2.f);
            ctx3->Quad();
        }

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
```

A few things worth noting: Materials are registered and synced before rendering begins. The 2D scene uses two materials: a standard lit material for the background and an occluder material for the red quad so that it blocks light and casts shadows. The 3D directional light uses fitted cascades by pointing its `Cascades.View` at the active render view, which lets the cascade splits adapt to the camera's frustum. The `GetMouseRenderView()` calls detect which view the mouse is hovering over, so camera controls apply to the correct viewport.

### Transparency

This example demonstrates order-independent transparency (OIT) with another split-screen layout. OIT is enabled per-view with the `RenderViewFlag_Transparency` flag, which means transparent objects are rendered correctly regardless of draw order:

```cpp
#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"

using Onyx::D2;
using Onyx::D3;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});
    const Onyx::RenderViewFlags vflags =
        Onyx::RenderViewFlag_NormalizedCoordinates | Onyx::RenderViewFlag_Transparency;

    // 2D view on the left half
    Onyx::Camera<D2> cam2{};
    Onyx::RenderView<D2> *view2 = win->CreateRenderView<D2>(&cam2, vflags);
    Onyx::RenderContext<D2> *ctx2 = Onyx::CreateRenderContext<D2>();
    cam2.OrthoParameters.Size = 10.f;
    view2->SetNormalizedViewport({.Position = 0.f, .Extent = {0.5f, 1.f}});
    ctx2->AddTarget(view2);

    // 3D view on the right half
    Onyx::Camera<D3> cam3{};
    Onyx::RenderView<D3> *view3 = win->CreateRenderView<D3>(&cam3, vflags);
    Onyx::RenderContext<D3> *ctx3 = Onyx::CreateRenderContext<D3>();
    cam3.View.Translation[2] = 5.f;
    view3->SetNormalizedViewport({.Position = {0.5f, 0.f}, .Extent = {0.5f, 1.f}});
    ctx3->AddTarget(view3);

    f32 time = 0.f;
    while (Onyx::Running())
    {
        const TKit::Timespan dt = Onyx::GetDeltaTime(win);
        time += dt.AsSeconds();

        Onyx::RenderView<D2> *v2 = win->GetMouseRenderView<D2>();
        Onyx::RenderView<D3> *v3 = win->GetMouseRenderView<D3>();
        if (v2)
            win->ControlCamera(Onyx::GetDeltaTime(win), v2->GetCamera());
        else if (v3)
            win->ControlCamera(Onyx::GetDeltaTime(win), v3->GetCamera());

        // 2D scene: overlapping transparent shapes
        {
            ctx2->Flush();

            ctx2->Push();
            ctx2->Scale(20.f);
            ctx2->Quad();
            ctx2->Pop();

            ctx2->Rotate(time);
            ctx2->FillColor(Onyx::Color_Red);
            ctx2->Alpha(0.8f);
            ctx2->Quad();

            ctx2->FillColor(Onyx::Color_Green);
            ctx2->Alpha(0.7f);
            ctx2->Translate(0.2f);
            ctx2->Triangle();

            ctx2->FillColor(Onyx::Color_Blue);
            ctx2->Alpha(0.9f);
            ctx2->Translate(0.2f);
            ctx2->Stadium();
        }

        // 3D scene: overlapping transparent volumes
        {
            ctx3->Flush();

            ctx3->Push();
            ctx3->ScaleX(20.f);
            ctx3->ScaleY(20.f);
            ctx3->RotateX(0.5f * Onyx::Math::Pi());
            ctx3->TranslateY(-2.f);
            ctx3->Quad();
            ctx3->Pop();

            ctx3->RotateZ(time);
            ctx3->FillColor(Onyx::Color_Red);
            ctx3->Alpha(0.8f);
            ctx3->Box();

            ctx3->FillColor(Onyx::Color_Green);
            ctx3->Alpha(0.7f);
            ctx3->TranslateZ(-2.f);
            ctx3->RoundedRect();

            ctx3->FillColor(Onyx::Color_Blue);
            ctx3->Alpha(0.9f);
            ctx3->TranslateZ(-2.f);
            ctx3->Capsule();
        }

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
```

The `Alpha()` call sets the opacity of subsequent draws and automatically marks them for transparent blending. Because OIT is enabled on both views, the overlapping shapes blend correctly without needing to sort by depth. The 2D side stacks a quad, triangle and stadium on top of each other, while the 3D side does the same with a box, rounded rect and capsule. Note that transforms accumulate - each `Translate()` call shifts relative to the current transform state, so the shapes spread apart progressively.

## Dependencies and Third-Party Libraries

Onyx relies on some dependencies such as [Toolkit](https://github.com/ismawno/toolkit), [Vulkit](https://github.com/ismawno/vulkit) and [GLFW](https://github.com/glfw/glfw). Both Toolkit and Vulkit are custom libraries I developed alongside Onyx: Toolkit provides general-purpose utilities (data structures, allocators, profiling, etc.) and Vulkit is an abstraction layer over the Vulkan API that handles device management, resource creation and synchronization. Some dependencies are optional ([ImGui](https://github.com/ocornut/imgui) and [ImPlot](https://github.com/epezent/implot)) and most of them are pulled automatically from `CMake`.

## Versioning

Try to always use a tagged commit when using the library, as I can somewhat guarantee those will build and be stable.

## Building

The building process is straightforward. Using `CMake`:

```sh
cmake --preset release
cmake --build --preset release
```
