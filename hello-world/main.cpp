#include "onyx/app/app.hpp"
#include "kit/core/literals.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"
#include <imgui.h>

using ONYX::D2;
using namespace KIT::Literals;

static void RunStandaloneWindow() noexcept
{
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
}

static void RunAppExample1() noexcept
{
    ONYX::Window::Specs specs;
    specs.Name = "App1 Hello, World!";
    specs.Width = 800;
    specs.Height = 600;

    ONYX::Application app(specs);

    app.Run();
}

static void RunAppExample2() noexcept
{
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
}

static void RunAppExample3() noexcept
{
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
}

int main()
{
    KIT::StackAllocator allocator(10_kb);
    KIT::ThreadPool<std::mutex> threadPool(7);

    ONYX::Core::Initialize(&allocator, &threadPool);
    RunStandaloneWindow();
    RunAppExample1();
    RunAppExample2();
    RunAppExample3();
    ONYX::Core::Terminate();
}