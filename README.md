# onyx

**Onyx** is a C++ application framework implemented with Vulkan and ImGui that I plan to use anytime I am working on a project that requires some kind of geometry visualization/GUI. It is meant for personal use only, although I have tried my best to make it as user-friendly as possible.

I have very little experience with graphics programming, this project being the first serious "deep" dive into the Vulkan API, which is also the first graphics API I have ever used, so I am sure there is a lot of room for improvement.

Given all of this and that the scope of this project seems to be reasonably small, one could argue OpenGL may have been a better option, but I decided against it for two main reasons:

1. OpenGL is deprecated in MacOS, and I use that OS as my main coding environment.
2. Vulkan is growing more popular every year because of its versatility and optimization opportunities. If I am going to start graphics programming now, I might as well do it in a modern API. I can't help but feel I am wasting my time learning an API I dont think is one of "the best" (which is arguably a very bad argument lol) but thats how it is.

## Features

The 3 main features I consider the most valuable are the application/window API (with its corresponding layer system), the render API (used to draw simple geometry to a window) and the ImGui and ImPlot support.

### Window API

This is the most simple use case of the **onyx** framework, which allows you to have a window up and running very quickly. It is also the most customizable, as it doesnt even use the application interface to work. The downside is that some higher level features, such as ImGui, will not be there right off the bat (you will have to manually setup the ImGui backend, which I highly discourage. If you want to use ImGui, go to the [Application API](#application-api) section).

Creating and using an **onyx** window looks like this:

```cpp
ONYX::Window::Specs specs;
specs.Name = "Standalone Hello, World!";
specs.Width = 800;
specs.Height = 600;

ONYX::Window window(specs);

while (!window.ShouldClose())
{
    ONYX::Input::PollEvents();

    ONYX::RenderContext<D2> *context = window.GetRenderContext<D2>();
    context->Flush(ONYX::Color::BLACK);

    context->KeepWindowAspect();

    context->Fill(ONYX::Color::RED);
    context->Square();

    window.Render();
}
```

The full code with examples can be found at [hello-world](https://github.com/ismawno/onyx/onyx/onyx/hello-world/main.cpp).

### Application API

This feature allows the user to control the flow of their application. One of the simplest use cases would look like this:

```cpp
ONYX::Window::Specs specs;
specs.Name = "App1 Hello, World!";
specs.Width = 800;
specs.Height = 600;

ONYX::Application app(specs);

app.Run();
```

Note that this setup wont do anything. You will just endup with a pitch black window, which I assume may not be very useful. You can breakdown the `Application::Run()` method so that you can insert some of your own logic into the frame loop:

```cpp
ONYX::Window::Specs specs;
specs.Name = "App2 Hello, World!";
specs.Width = 800;
specs.Height = 600;

ONYX::Application app(specs);

KIT::Clock clock;
app.Startup();
while (app.NextFrame(clock))
{
    ONYX::RenderContext<D2> *context = app.GetMainWindow()->GetRenderContext<D2>();
    context->Flush(ONYX::Color::BLACK);

    context->KeepWindowAspect();

    context->Fill(ONYX::Color::RED);
    context->Square();
}
app.Shutdown();
```

This setup is more flexible than the previous, but otherwise very similar to the one shown in the [Window API](#window-api) section, and it still wont allow the user to use ImGui. When using the Applicatio API, you most likely will want to use the layer system to take advantage of the full capabilities of the inerface like so:

```cpp
ONYX::Window::Specs specs;
specs.Name = "App3 Hello, World!";
specs.Width = 800;
specs.Height = 600;

class MyLayer : public ONYX::Layer
{
    public:
    MyLayer() noexcept : Layer("MyLayer")
    {
    }

    void OnRender(const VkCommandBuffer) noexcept override
    {
        ImGui::Begin("Hello, World!");
        ImGui::Text("Hello, World from ImGui!");
        ImGui::End();
    }
};

ONYX::Application app(specs);
app.Layers.Push<MyLayer>();

app.Run();
```

There is more to this system, as you can use other layer callbacks such as `OnEvent()`, `OnUpdate()` etcetera. All of the user-relevant parts of this library are documented in the source code. The documentation can be built with doxygen as well. As mentioned earlier, the whole working example can be found at [hello-world](https://github.com/ismawno/onyx/onyx/onyx/hello-world/main.cpp).

#### Multi-Window Application

**Onyx** supports a multi window application interface as well, where it is possible to have many windows per application. Within this interface, there are two possible implementations of a multi-window application: A serial and a concurrent one. The former runs all windows in the main thread, while the latter hosts a dedicated thread for each window.

The details of how this feature works will not be discussed in depth in this README (for that, go to the documentation an the [onyx/app/mwapp.hpp](https://github.com/ismawno/onyx/onyx/onyx/app/mwapp.hpp) and [onyx/app/layer.hpp](https://github.com/ismawno/onyx/onyx/onyx/app/layer.hpp) files, where this interface is explained thoroughly, as well as how it interacts with the layer system. This is important, specially in the concurrent case, where user-end synchronization issues can occur).

Please note that the serial implementation of this interface is much more forgiving and easier to use than the concurrent one. The latter requires careful consideration at the user-end implementation, as I have tried to minimize locking (otherwise, the point of parallelism is kind of defeated).

I must also mention that I decided to implement this interface for fun. I have NOT measured its performance, and I cannot say with certainty it will have performance benefits over the serial implementation. Use it at your own risk.

### Render API

The **onyx** framework counts on two very basic renderers to draw simple geometries to the screen. The renderers are almost identical. One is specialized for 2D rendering, and the other for 3D rendering, the latter containing noticeably more features. The API is immediate mode, similar to ImGui, and examples of its use are shown (indirectly) in the previous section through the `RenderContext<D>` class, where `D` is either `D2`or `D3` (the dimensionality).

I got inspired from the [Processing](https://processing.org) API, so you will find some of its features replicated here (such as the `Push()`/`Pop()` methods).

More details of this feature can be found in the source code documentation at [onyx/onyx/rendering/render_context.hpp](https://github.com/ismawno/onyx/onyx/onyx/rendering/render_context.hpp).

**Onyx** also supports model rendering through the [Model](https://github.com/ismawno/onyx/onyx/onyx/draw/model.hpp) class. Each render implements instance rendering when possible. All primitives or models of the same type get rendered under a single draw call.

### ImGui usage

ImGui support comes right out of the box in **onyx** when using the application interface, as shown in the last example in the [Application API](#application-api) section. You can start using the ImGui API right away from the `OnRender()` callback, in the case of a single-window application, and from the `OnImGuiRender()` callback from a multi-window application.

## Dependencies and Third-Party Libraries

**Onyx** uses a bunch of dependencies, mainly to cover the platform-independent window API and graphics API aspects, as well as the ImGui support. The list of dependencies is the following:

- [glfw](https://github.com/glfw/glfw) The platform-agnostic window API I decided to go with.

- [glm](https://github.com/g-truc/glm) The maths library I use to handle transformations and algebra.

- [imgui](https://github.com/ocornut/imgui) The ImGui library

- [implot](https://github.com/epezent/implot) The ImPlot library (optional, can be enabled through CMake options)

- [tobjl](https://github.com/tinyobjloader/tinyobjloader) A small library to quickly load .obj models.

- [toolkit](https://github.com/ismawno/toolkit) A small utilities library of mine.

- [vma](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) The Vulkan memory allocator, used when managing buffer memory.

- [vulkan](https://github.com/KhronosGroup/Vulkan-Loader) The Vulkan API.

## Building

The building process is mostly straightforward. Create a `build` folder, `cd` into it, and run `cmake ..`. All available **onyx** options will be printed out.

Then compile the project with your editor/IDE of choice, and run the [hello-world](https://github.com/ismawno/onyx/onyx/onyx/hello-world/main.cpp) example along with both demos ([single window](https://github.com/ismawno/onyx/onyx/single-window-demo) and [multi window](https://github.com/ismawno/onyx/onyx/multi-window-demo))

