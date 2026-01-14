# Onyx

Onyx is a core-friendly C++ application framework implemented with Vulkan that I plan to use for any project requiring geometry visualization or a GUI.

I started this project with very little experience with graphics programming. This project represents my first serious "deep dive" into the Vulkan API, which is also the first graphics API I have ever used. I am sure there is a lot of room for improvement but I am happy with how the project has evolved. I originally considered starting with OpenGL, but I eventually decided against it for two main reasons:

1. OpenGL is deprecated in MacOS and I want good multi-platform support.
2. Vulkan is growing more popular every year due to its versatility and optimization opportunities. Since I recently started graphics programming, I might as well learn a modern API.


## Features

Onyx is designed for users that wish to create simple and fast visualization applications, such as particle simulations, physics engines, geometry visualization etc. It does not yet support graphics that attempt to simulate real-life environments, although it is a planned feature. Onyx has been designed for performance so most of the rendering API is thread safe and scales well with multiple cores.

Onyx seamlessly supports 2D and 3D rendering with a unified API that let's the user choose the dimensionality of your visualization at compile time. This is specially advantageous for the 2D case as this allows the library to be much more memory efficient, unlocking many optimizations. It is also ergonomic, as the vector and matrix types will always match the dimension being used, and the API will not expose functionality that does not make sense for a 2D or 3D application. To specify this, many Onyx objects are templated with the `Dimension` enum, which can take either the `D2` or `D3` values. For simplicity, all examples shown in this README will use the `D2` api. `D3` is almost identical with small variations that will not matter for the examples.

Although it will not be explicitly needed in most use cases, Onyx exposes the Vulkan API so that you can hook custom behaviour to the frame loop by recording any command directly into the main command buffer.

A full sandbox of Onyx is available as a [demo](https://github.com/ismawno/onyx/tree/main/demo/sandbox). Be sure to set `ONYX_BUILD_DEMOS` to `ON` in `CMake` to be able to run it.

### Windows

The most straightforward way of using Onyx is through a standalone window. It is also the most versatile and customizable, as it gives you explicit control of the frame loop, but lacks out of the box features such as ImGui or ImPlot, which must be setup manually. Window management can be deferred to application objects (see [Applications](#applications)).

Onyx supports the usage of multiple windows. Each of them is independent to an extent, and can be run on different threads as long as they use different queues. This however is slightly discouraged as the performance gains may be minimal and per-window queue assignment is automatic. The `Application` class may handle multiple windows for you. Control the maximum amount of windows with `ONYX_APP_MAX_WINDOWS`. If set to 1, the API to open/close additional windows will be disabled.

Creating an Onyx window looks like this:

**Please note that, while all examples should pretty much work as-is, treat them as pseudocode. Onyx is under constant change and maintaining these examples in plain text may introduce errors. To check a working version of pretty much all of the examples in this README, take a look at [this file](https://github.com/ismawno/onyx/blob/main/demo/hello-world/main.cpp)**

```cpp
Onyx::Window window({.Name = "Standalone Hello, World!", .Dimensions = u32v2{800, 600}});
```

But before moving forward and start rendering to the window, two more objects must be introduced.

#### Render contexts

Render contexts are the main way of communicating with the rendering API. It is the object with which the user will render and manipulate primitives, meshes and lights with an immediate mode style. Contexts may prepare and expect inputs in 2D or 3D form depending on its dimension template parameter (`D2` or `D3`). An example of its usage may be the following:

```cpp
Onyx::RenderContext<D2> *context = window.CreateRenderContext<D2>();
context->Flush();

context->Push();
context->Scale(2.f);
context->Square();
context->Translate(0.f, 10.f);
context->Circle();
context->Pop();

context->RoundedSquare();
```

Most of the calls to `RenderContext` modify its internal state or store draw commands. At the end of the frame, the stored render data is sent to the device and rendered. Almost all `RenderContext` methods are thread safe and scale well when executed in parallel. Methods that are not thread safe will be explicitly marked as so in its documentation.

`RenderContext`s cannot be created out of thin air. They must be bound to a window. Each window may have multiple contexts of multiple dimensions. Before using a context for the first time in a given frame, the method `Flush()` must be called, which resets its state and render data, allowing the user to draw a new frame. `Flush()` should only be called when explicitly re-using the context to update the scene. If the context represents a static scene updated only in isolated scenarios, it should be left alone as the context will avoid re-creating and sending the data to the device, which can be a real bottleneck. This makes rendering static scenes much more efficient as there is no host-device communication. Because of this, the scene should be divided into different contexts based on update frequency.

`RenderContext`s are documented in more detail in the [source code](https://github.com/ismawno/onyx/blob/main/onyx/onyx/rendering/context.hpp).

#### Cameras

The purpose of a camera is pretty self-explanatory in the context of graphics programming. Cameras are owned by windows and are created very similarly to render contexts. 2D cameras are simple, orthographic cameras, while 3D ones allow for other types of perspective projections.
```cpp
Onyx::Camera<D2> *camera = window.CreateCamera<D2>();
```

Cameras don't usually require much interaction. They provide convenient methods to modify its view and projection matrices through code or even user input (see `ControlMovementWithUserInput()` and `CameraControls<D>`). More documentation is available in the [source code](https://github.com/ismawno/onyx/blob/main/onyx/onyx/property/camera.hpp).

With these three ingredients, a very minimal working setup can be used to draw a simple primitive to the screen:

```cpp
Onyx::Window window({.Name = "Standalone Hello, World!", .Dimensions = u32v2{800, 600}});
Onyx::RenderContext<D2> *context = window.CreateRenderContext<D2>();
window.CreateCamera<D2>();

while (!window.ShouldClose())
{
    Onyx::Input::PollEvents();

    context->Flush();

    context->Fill(Onyx::Color::RED);
    context->Square();

    window.Render();
}
```
### Applications

This feature grants the user high-level control of the flow of their application. One of the simplest use cases looks like this:

```cpp
Onyx::Application app({.Name = "App1 Hello, World!", .Dimensions = u32v2{800, 600}});
app.Run();
```

Note that this setup won’t do anything beyond opening a pitch-black window, which may not be very useful. You can break down the `Application::Run()` method to insert your own logic into the frame loop:

```cpp
Onyx::Application app({.Name = "App2 Hello, World!", .Dimensions = u32v2{800, 600}});

TKit::Clock clock;

Onyx::Window *window = app.GetMainWindow();
Onyx::RenderContext<D2> *context = window->CreateRenderContext<D2>();
window->CreateCamera<D2>();
while (app.NextFrame(clock))
{
    context->Flush();

    context->Fill(Onyx::Color::RED);
    context->Square();
}

```

This setup is more flexible than the previous one but still similar to the example shown in the [Windows](#window-api) section and not how an application is intended to be used. To take full advantage of the application interface, you will likely want to use the user layer to leverage its full capabilities:

**Note that ImGui must be enabled to replicate this example.**

```cpp
class MyLayer : public Onyx::UserLayer
{
    public:
    void OnUpdate() override
    {
        ImGui::Begin("Hello, World!");
        ImGui::Text("Hello, World from ImGui!");
        ImGui::End();
    }
};

Onyx::Application app({.Name = "App3 Hello, World!", .Dimensions = u32v2{800, 600}});
app.SetUserLayer<MyLayer>();

app.Run();
```

Every window gets a user layer (must be explicitly added), ImGui and ImPlot contexts (if enabled).

There is more to this system, such as additional layer callbacks like `OnEvent()` and `OnUpdate()`. All user-relevant parts of this library are documented in the source code. The documentation can also be built with Doxygen. As mentioned earlier, the full working example can be found at [hello-world](https://github.com/ismawno/onyx/blob/main/demo/hello-world/main.cpp).

Onyx also supports a multi-window application interface, allowing many windows per application by using the `OpenWindow()` and `CloseWindow()` methods and setting `ONYX_APP_MAX_WINDOWS` to a value greater than 1. For more details, refer to the documentation [here](https://github.com/ismawno/onyx/blob/main/onyx/onyx/app/app.hpp) and [here](https://github.com/ismawno/onyx/blob/main/onyx/onyx/app/user_layer.hpp).

### ImGui/ImPlot Usage

If enabled through `CMake`, ImGui and ImPlot support is built-in when using the application interface, as shown in the last example in the [Applications](#applications) section. You can use the ImGui API directly in the `OnUpdate()` or `OnRenderBegin()` callbacks.

## Dependencies and Third-Party Libraries

Onyx relies on some dependencies such as [toolkit](https://github.com/ismawno/toolkit), [vulkit](https://github.com/ismawno/vulkit) and [glfw](https://github.com/glfw/glfw). Some of them are optional ([imgui](https://github.com/ocornut/imgui) and [implot](https://github.com/epezent/implot)) and most of them are pulled automatically from `CMake`.

## Versioning

Try to always use a tagged commit when using the library, as I can guarantee those will build and be stable.

## Building

The building process is straightforward. Using `CMake`:

```sh
cmake --preset release
cmake --build --preset release
```
