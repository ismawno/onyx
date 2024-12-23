#include "onyx/app/app.hpp"
#include "onyx/core/shaders.hpp"
#include "tkit/core/literals.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include <imgui.h>

using Onyx::D2;
using namespace TKit::Alias;

static void RunStandaloneWindow() noexcept
{
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
}

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

static void RunStandaloneWindowPreProcessing() noexcept
{
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
}

static void RunAppExample1() noexcept
{
    Onyx::Window::Specs specs;
    specs.Name = "App1 Hello, World!";
    specs.Width = 800;
    specs.Height = 600;

    Onyx::Application app(specs);

    app.Run();
}

static void RunAppExample2() noexcept
{
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
}

static void RunAppExample3() noexcept
{
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
}

int main()
{
    TKit::ThreadPool<std::mutex> threadPool(7);

    Onyx::Core::Initialize(&threadPool);
    RunStandaloneWindow();
    RunStandaloneWindowPreProcessing();
    RunAppExample1();
    RunAppExample2();
    RunAppExample3();
    Onyx::Core::Terminate();
}