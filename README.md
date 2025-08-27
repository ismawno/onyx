# Onyx

Onyx is a C++ application framework implemented with Vulkan and ImGui that I plan to use for any project requiring geometry visualization or a GUI. It is meant for personal use only, although I have tried my best to make it as user-friendly as possible.

I have very little experience with graphics programming. This project represents my first serious "deep dive" into the Vulkan API, which is also the first graphics API I have ever used. I am sure there is a lot of room for improvement.

Given this and the reasonably small scope of this project, one could argue that OpenGL might have been a better option. However, I decided against it for two main reasons:

1. OpenGL is deprecated in macOS, and I use macOS as my main coding environment. EDIT - I know use linux, but I want good macOS support as well.
2. Vulkan is growing more popular every year due to its versatility and optimization opportunities. Since I am starting graphics programming now, I might as well learn a modern API. I can’t help but feel that learning an API I don't think is "the best" would be a waste of time (arguably, a subjective argument, but that’s how I feel).

## Features

The three main features I consider the most valuable are:
- The application/window API (with its corresponding layer system),
- The render API (used to draw simple geometry to a window),
- Built-in support for ImGui and ImPlot.

### Window API

**Note:** All examples will use the 2D API. 3D is almost identical and can be accessed by changing the template argument from `D2` to `D3`.

This is the simplest use case of the Onyx framework, allowing you to get a window up and running very quickly. It is also highly customizable, as it doesn’t require the application interface to work. However, some higher-level features, such as ImGui, are not included by default (you will have to manually set up the ImGui backend, which I highly discourage. If you want to use ImGui, refer to the [Application API](#application-api) section).

Creating and using an Onyx window looks like this:

```cpp
Onyx::Window window({.Name = "Standalone Hello, World!", .Width = 800, .Height = 600});
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

It is also very easy to include your own shaders into the Onyx's rendering setup. It is possible to do so with a custom pipeline binding through the `VKit::GraphicsJob` abstraction or with post-processing effects. The latter includes a pre-bound sampled texture that represents the frame that is about to be rendered to the screen, to be modified freely. To add such features, the following two functions may be defined:

```cpp
static VKit::GraphicsJob SetupCustomPipeline(Onyx::Window &p_Window) noexcept
{
    VKit::Shader fragment = Onyx::CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/rainbow.frag");

    auto lresult = VKit::PipelineLayout::Builder(Onyx::Core::GetDevice()).Build();
    VKIT_ASSERT_RESULT(lresult);
    VKit::PipelineLayout &layout = lresult.GetValue();

    const auto presult = VKit::GraphicsPipeline::Builder(Onyx::Core::GetDevice(), layout,
                                                         p_Window.GetFrameScheduler()->CreateSceneRenderInfo())
                             .SetViewportCount(1)
                             .AddShaderStage(Onyx::GetFullPassVertexShader(), VK_SHADER_STAGE_VERTEX_BIT)
                             .AddShaderStage(fragment, VK_SHADER_STAGE_FRAGMENT_BIT)
                             .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                             .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                             .AddDefaultColorAttachment()
                             .Build();

    const VKit::GraphicsPipeline &pipeline = presult.GetValue();
    fragment.Destroy();
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(pipeline);

    VKIT_ASSERT_RESULT(presult);

    const auto jresult = VKit::GraphicsJob::Create(pipeline, layout);
    VKIT_ASSERT_RESULT(jresult);

    return jresult.GetValue();
}

static void SetPostProcessing(Onyx::Window &p_Window) noexcept
{
    struct BlurData
    {
        u32 KernelSize = 8;

        // Window dimensions
        f32 Width = 800.f;
        f32 Height = 600.f;
    };
    VKit::Shader shader = Onyx::CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/blur.frag");

    Onyx::FrameScheduler *fs = p_Window.GetFrameScheduler();
    VKit::PipelineLayout::Builder builder = fs->GetPostProcessing()->CreatePipelineLayoutBuilder();

    const auto result = builder.AddPushConstantRange<BlurData>(VK_SHADER_STAGE_FRAGMENT_BIT).Build();
    VKIT_ASSERT_RESULT(result);
    const VKit::PipelineLayout &layout = result.GetValue();

    fs->SetPostProcessing(layout, shader);
    static BlurData blurData{};

    fs->GetPostProcessing()->UpdatePushConstantRange(0, &blurData);

    shader.Destroy();
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
}
```

Then, by modifying the window setup:

```cpp
Onyx::Window window(
        {.Name = "Standalone Hello, World! With a custom rainbow background and a post-processing effect!",
         .Width = 800,
         .Height = 600});

const VKit::GraphicsJob job = SetupCustomPipeline(window);
SetPostProcessing(window);

Onyx::RenderContext<D2> *context = window.CreateRenderContext<D2>();
window.CreateCamera<D2>()->Transparent = true;
while (!window.ShouldClose())
{
    Onyx::Input::PollEvents();

    context->Flush();

    context->Fill(Onyx::Color::RED);
    context->Square();

    window.RenderSubmitFirst([&job](const VkCommandBuffer p_CommandBuffer) {
        job.Bind(p_CommandBuffer);
        job.Draw(p_CommandBuffer, 3);
    });
}
```

Note that, to ensure the custom pipeline that draws a rainbow does so in the background, `RenderSubmitFirst()` is called so that the lambda passed is executed before the main scene rendering. To submit effects that would override previous draws, use `RenderSubmitLast()`.

These steps are very similar to perform with the Application API. Instead of using lambdas, the layer callbacks should be used instead, which already provide a Vulkan command buffer. To handle resource cleanup, a global Onyx deletion queue is being used.

The full code with examples can be found at [hello-world](https://github.com/ismawno/onyx/blob/main/hello-world/main.cpp).

### Application API

This feature allows the user to control the flow of their application. One of the simplest use cases looks like this:


```cpp
Onyx::Application app({.Name = "App1 Hello, World!", .Width = 800, .Height = 600});
app.Run();
```

Note that this setup won’t do anything beyond opening a pitch-black window, which may not be very useful. You can break down the `Application::Run()` method to insert your own logic into the frame loop:

```cpp
Onyx::Application app({.Name = "App2 Hello, World!", .Width = 800, .Height = 600});

TKit::Clock clock;
app.Startup();
Onyx::RenderContext<D2> *context = app.GetMainWindow()->CreateRenderContext<D2>();
app.GetMainWindow()->CreateCamera<D2>();
while (app.NextFrame(clock))
{
    context->Flush();

    context->Fill(Onyx::Color::RED);
    context->Square();
}
app.Shutdown();
```

This setup is more flexible than the previous one but still similar to the example shown in the [Window API](#window-api) section. However, it still won’t allow the user to use ImGui. To take full advantage of the application interface, including ImGui, you will likely want to use the user layer to leverage its full capabilities:


```cpp
class MyLayer : public Onyx::UserLayer
{
    public:
    void OnRender(const VkCommandBuffer) noexcept override
    {
        ImGui::Begin("Hello, World!");
        ImGui::Text("Hello, World from ImGui!");
        ImGui::End();
    }
};

Onyx::Application app({.Name = "App3 Hello, World!", .Width = 800, .Height = 600});
app.SetUserLayer<MyLayer>();

app.Run();
```

There is more to this system, such as additional layer callbacks like `OnEvent()` and `OnUpdate()`. All user-relevant parts of this library are documented in the source code. The documentation can also be built with Doxygen. As mentioned earlier, the full working example can be found at [hello-world](https://github.com/ismawno/onyx/blob/main/hello-world/main.cpp).

#### Multi-Window Application

Onyx also supports a multi-window application interface, allowing many windows per application. The main difference with the standard application is that windows must be opened manually, including the main window. For more details, refer to the documentation and the [onyx/app/mwapp.hpp](https://github.com/ismawno/onyx/blob/main/onyx/onyx/app/mwapp.hpp) and [onyx/app/user_layer.hpp](https://github.com/ismawno/onyx/blob/main/onyx/onyx/app/user_layer.hpp) files.

### Render API

The Onyx framework includes two basic renderers for drawing simple geometry:
- **2D Renderer**: For drawing 2D shapes.
- **3D Renderer**: For rendering 3D objects, with additional features.

The API uses an immediate mode approach, inspired by the [Processing](https://processing.org) API. Features like `Push()`/`Pop()` are implemented for state management. Examples can be found in the previous sections, where the `RenderContext<D>` class is used (`D` being either `D2` or `D3`).

Onyx also supports meshemeshes rendering through the [Mesh](https://github.com/ismawno/onyx/blob/main/onyx/onyx/object/mesh.hpp) class. When possible, it uses instance rendering to minimize draw calls.

### ImGui Usage

ImGui support is built-in when using the application interface, as shown in the last example in the [Application API](#application-api) section. You can use the ImGui API directly in the `OnRender()` callback for single-window applications or the `OnImGuiRender()` callback for multi-window applications.

## Dependencies and Third-Party Libraries

Onyx relies on several dependencies for platform-independent windowing, graphics APIs, and ImGui support:

- [toolkit](https://github.com/ismawno/toolkit): A utility library I have developed.

- [vulkit](https://github.com/ismawno/vulkit): A Vulkan library I have developed.

- [glfw](https://github.com/glfw/glfw): Platform-agnostic window API.

- [glm](https://github.com/g-truc/glm): Math library for transformations and algebra.

- [imgui](https://github.com/ocornut/imgui): ImGui library.

- [implot](https://github.com/epezent/implot): ImPlot library (optional, can be enabled via `CMake`).

- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader): Library for loading `.obj` meshes.

**Note:** `CMake` is required to be manually installed in your system.

## Versioning

As some Onyx dependencies are being developed by me and are under constant change, I can only guarantee this project will build from a tagged commit. This is because such dependencies are fetched with `CMake`'s `FetchContent` module with the `GIT_TAG` parameter set to `main` for all commits except for tagged ones. This makes my life easier when it comes to updating the dependencies according to my needs.

## Known bugs -- SOLVED --

Sometimes, in very rare occasions, the framework (and thus the whole program) may experience a crash (segfault) when shutting down the application. I have seen this only happen in release/distribution builds, where optimizations are on and debug symbols are rarely enabled. I did manage to catch the crash with the debugger once. I even thought I knew what was happening and added a fix, but it didn't work... The crash happens at the last call to `vkDeviceWaitIdle()`, which is called by the `Onyx::Core::Shutdown()` method. As it only happens on program shutdown, it should not affect the user experience in any way. It does annoy me that the application may not exit gracefully sometimes, but I have decided to leave it as is for now.

## Building

The building process is (fortunately) very straightforward. Because of how much I hate how the `CMake` cache works, I have left some python building scripts in the [setup](https://github.com/ismawno/onyx/tree/main/setup) folder.

The reason behind this is that `CMake` sometimes stores some variables in cache that you may not want to persist. This results in some default values for variables being only relevant if the variable itself is not already stored in cache. The problem with this is that I feel it is very easy to lose track of what configuration is being built unless I type in all my `CMake` flags explicitly every time I build the project, and that is just unbearable. Hence, these python scripts provide flags with reliable defaults stored in a `build.ini` file that are always applied unless explicitly changed with a command line argument.

Specifically, the [build.py](https://github.com/ismawno/onyx/blob/main/setup/build.py) file, when executed from the root folder, will handle the entire `CMake` execution process for you. You can enter `python setup/build.py -h` to see the available options.

If you prefer using `CMake` directly, that's perfectly fine as well. Create a `build` folder, `cd` into it, and run `cmake ..`. All available Onyx options will be displayed.

Then compile the project with your editor/IDE of choice, and test the [hello-world](https://github.com/ismawno/onyx/blob/main/hello-world/main.cpp) example along with the demos ([single-window](https://github.com/ismawno/onyx/blob/main/single-window-demo) and [multi-window](https://github.com/ismawno/onyx/blob/main/multi-window-demo)).
