#include "onyx/app/app.hpp"
#include "tkit/core/literals.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include <imgui.h>

using Onyx::D2;

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
    RunAppExample1();
    RunAppExample2();
    RunAppExample3();
    Onyx::Core::Terminate();
}