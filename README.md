# Onyx

Onyx is a C++ application framework implemented with Vulkan and ImGui that I plan to use for any project requiring geometry visualization or a GUI. It is meant for personal use only, although I have tried my best to make it as user-friendly as possible.

I have very little experience with graphics programming. This project represents my first serious "deep dive" into the Vulkan API, which is also the first graphics API I have ever used. I am sure there is a lot of room for improvement.

Given this and the reasonably small scope of this project, one could argue that OpenGL might have been a better option. However, I decided against it for two main reasons:

1. OpenGL is deprecated in macOS, and I use macOS as my main coding environment.
2. Vulkan is growing more popular every year due to its versatility and optimization opportunities. Since I am starting graphics programming now, I might as well learn a modern API. I can’t help but feel that learning an API I don't think is "the best" would be a waste of time (arguably, a subjective argument, but that’s how I feel).

## Features

The three main features I consider the most valuable are:
- The application/window API (with its corresponding layer system),
- The render API (used to draw simple geometry to a window),
- Built-in support for ImGui and ImPlot.

### Window API

This is the simplest use case of the Onyx framework, allowing you to get a window up and running very quickly. It is also highly customizable, as it doesn’t require the application interface to work. However, some higher-level features, such as ImGui, are not included by default (you will have to manually set up the ImGui backend, which I highly discourage. If you want to use ImGui, refer to the [Application API](#application-api) section).

Creating and using an Onyx window looks like this:

```cpp
Onyx::Window::Specs specs;
specs.Name = "Standalone Hello, World!";
specs.Width = 800;
specs.Height = 600;

Onyx::Window window(specs);

while (!window.ShouldClose())
{
    Onyx::Input::PollEvents();

    Onyx::RenderContext<D2> *context = window.GetRenderContext<D2>();
    context->Flush(Onyx::Color::BLACK);

    context->Fill(Onyx::Color::RED);
    context->Square();

    window.Render();
}
```

It is also possible to add pre- and post-processing effects that act on the raw color attachments of the framebuffers witha custom fragment shader. To add such effects to the previous example, the following two functions may be defined:

```cpp
static void SetupPreProcessing(Onyx::Window &p_Window) noexcept
{
    VKit::Shader shader = Onyx::CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/rainbow.frag");

    auto result = VKit::PipelineLayout::Builder(Onyx::Core::GetDevice()).Build();
    VKIT_ASSERT_RESULT(result);
    VKit::PipelineLayout &layout = result.GetValue();

    p_Window.SetupPreProcessing(layout, shader);

    Onyx::Core::GetDeletionQueue().SubmitForDeletion(shader);
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
}

static void SetupPostProcessing(Onyx::Window &p_Window) noexcept
{
    struct BlurData
    {
        u32 KernelSize = 8;

        // Window dimensions
        f32 Width = 800.f;
        f32 Height = 600.f;
    };
    VKit::Shader shader = Onyx::CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/blur.frag");

    VKit::PipelineLayout::Builder builder = p_Window.GetPostProcessing()->CreatePipelineLayoutBuilder();
    auto result = builder.AddPushConstantRange<BlurData>(VK_SHADER_STAGE_FRAGMENT_BIT).Build();
    VKIT_ASSERT_RESULT(result);
    VKit::PipelineLayout &layout = result.GetValue();

    p_Window.SetupPostProcessing(layout, shader);
    static BlurData blurData{};

    p_Window.GetPostProcessing()->UpdatePushConstantRange(&blurData);

    Onyx::Core::GetDeletionQueue().SubmitForDeletion(shader);
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
}
```

Then, by modifying the window setup:

```cpp
Onyx::Window::Specs specs;
specs.Name = "Standalone Hello, World! With a pre- and post-processing effect!";
specs.Width = 800;
specs.Height = 600;

Onyx::Window window(specs);

SetupPreProcessing(window);
SetupPostProcessing(window);

while (!window.ShouldClose())
{
    Onyx::Input::PollEvents();

    Onyx::RenderContext<D2> *context = window.GetRenderContext<D2>();
    context->Flush(Onyx::Color::BLACK);

    context->Fill(Onyx::Color::RED);
    context->Square();

    window.Render();
}
```

These steps are very similar to perform with the Application API. Note that a global Onyx deletion queue is being used to handle resource cleanup at the end of the program.

The full code with examples can be found at [hello-world](https://github.com/ismawno/onyx/blob/main/hello-world/main.cpp).

### Application API

This feature allows the user to control the flow of their application. One of the simplest use cases looks like this:


```cpp
Onyx::Window::Specs specs;
specs.Name = "App1 Hello, World!";
specs.Width = 800;
specs.Height = 600;

Onyx::Application app(specs);

app.Run();
```

Note that this setup won’t do anything beyond opening a pitch-black window, which may not be very useful. You can break down the `Application::Run()` method to insert your own logic into the frame loop:

```cpp
Onyx::Window::Specs specs;
specs.Name = "App2 Hello, World!";
specs.Width = 800;
specs.Height = 600;

Onyx::Application app(specs);

TKit::Clock clock;
app.Startup();
while (app.NextFrame(clock))
{
    Onyx::RenderContext<D2> *context = app.GetMainWindow()->GetRenderContext<D2>();
    context->Flush(Onyx::Color::BLACK);

    context->Fill(Onyx::Color::RED);
    context->Square();
}
app.Shutdown();
```

This setup is more flexible than the previous one but still similar to the example shown in the [Window API](#window-api) section. However, it still won’t allow the user to use ImGui. To take full advantage of the application interface, including ImGui, you will likely want to use the layer system to leverage its full capabilities:


```cpp
Onyx::Window::Specs specs;
specs.Name = "App3 Hello, World!";
specs.Width = 800;
specs.Height = 600;

class MyLayer : public Onyx::Layer
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

Onyx::Application app(specs);
app.Layers.Push<MyLayer>();

app.Run();
```

There is more to this system, such as additional layer callbacks like `OnEvent()` and `OnUpdate()`. All user-relevant parts of this library are documented in the source code. The documentation can also be built with Doxygen. As mentioned earlier, the full working example can be found at [hello-world](https://github.com/ismawno/onyx/blob/main/hello-world/main.cpp).

#### Multi-Window Application

Onyx also supports a multi-window application interface, allowing many windows per application. This interface offers two implementations:
1. **Serial**: All windows run on the main thread.
2. **Concurrent**: Each window runs on its own dedicated thread.

For more details, refer to the documentation and the [onyx/app/mwapp.hpp](https://github.com/ismawno/onyx/blob/main/onyx/onyx/app/mwapp.hpp) and [onyx/app/layer.hpp](https://github.com/ismawno/onyx/blob/main/onyx/onyx/app/layer.hpp) files.

The serial implementation is more forgiving and easier to use than the concurrent one. The latter requires careful synchronization on the user’s end to avoid issues. I have minimized locking to preserve parallelism (to my humble extent), but I have not measured its performance, so use it at your own risk.

### Render API

The Onyx framework includes two basic renderers for drawing simple geometry:
- **2D Renderer**: For drawing 2D shapes.
- **3D Renderer**: For rendering 3D objects, with additional features.

The API uses an immediate mode approach, inspired by the [Processing](https://processing.org) API. Features like `Push()`/`Pop()` are implemented for state management. Examples can be found in the previous sections, where the `RenderContext<D>` class is used (`D` being either `D2` or `D3`).

Onyx also supports model rendering through the [Model](https://github.com/ismawno/onyx/blob/main/onyx/onyx/draw/model.hpp) class. When possible, it uses instance rendering to minimize draw calls.

### ImGui Usage

ImGui support is built-in when using the application interface, as shown in the last example in the [Application API](#application-api) section. You can use the ImGui API directly in the `OnRender()` callback for single-window applications or the `OnImGuiRender()` callback for multi-window applications.

## Dependencies and Third-Party Libraries

Onyx relies on several dependencies for platform-independent windowing, graphics APIs, and ImGui support:

- [glfw](https://github.com/glfw/glfw): Platform-agnostic window API.
- [glm](https://github.com/g-truc/glm): Math library for transformations and algebra.
- [imgui](https://github.com/ocornut/imgui): ImGui library.
- [implot](https://github.com/epezent/implot): ImPlot library (optional, can be enabled via CMake).
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader): Library for loading `.obj` models.
- [vulkit](https://github.com/ismawno/vulkit): A Vulkan library I have developed.

## Building

The building process is straightforward. Create a `build` folder, `cd` into it, and run `cmake ..`. All available Onyx options will be displayed.

Then compile the project with your editor/IDE of choice, and test the [hello-world](https://github.com/ismawno/onyx/blob/main/hello-world/main.cpp) example along with the demos ([single-window](https://github.com/ismawno/onyx/blob/main/single-window-demo) and [multi-window](https://github.com/ismawno/onyx/blob/main/multi-window-demo)).