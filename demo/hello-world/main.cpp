#include "onyx/app/app.hpp"
#include "onyx/app/window.hpp"
#include "onyx/imgui/imgui.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"

using Onyx::D2;
using namespace TKit::Alias;

#define ONYX_MAX_WORKERS (ONYX_MAX_THREADS - 1)

static Onyx::Mesh s_Square;

static void RunStandaloneWindow()
{
    Onyx::Window window({.Name = "Standalone Hello, World!", .Dimensions = u32v2{800, 600}});

    const Onyx::StatMeshData<D2> square = Onyx::Assets::CreateSquareMesh<D2>();
    s_Square = Onyx::Assets::AddMesh(square);
    Onyx::Assets::Upload<D2>();

    Onyx::RenderContext<D2> *context = window.CreateRenderContext<D2>();
    window.CreateCamera<D2>();

    while (!window.ShouldClose())
    {
        Onyx::Input::PollEvents();

        // This is simpler but not ideal. The default `Render()` method not always renders (some conditions must be
        // met). In those cases, the context operations go to waste. It is better to manually begin and end the frame to
        // avoid the context operations if the frame cannot start. Still, this is viable and suitable for simple use
        // cases
        context->Flush();

        context->Fill(Onyx::Color::RED);
        context->StaticMesh(s_Square);

        window.Render();
    }
}

static void RunAppExample1()
{
    Onyx::Application app({.Name = "App1 Hello, World!", .Dimensions = u32v2{800, 600}});
    app.Run();
}

static void RunAppExample2()
{
    Onyx::Application app({.Name = "App2 Hello, World!", .Dimensions = u32v2{800, 600}});

    Onyx::RenderContext<D2> *context = app.GetMainWindow()->CreateRenderContext<D2>();
    app.GetMainWindow()->CreateCamera<D2>();

    TKit::Clock clock;
    while (app.NextFrame(clock))
    {
        // This is also not ideal, as the window not always renders in every iteration of the loop, so these operations
        // may go to waste. Still, this is viable and suitable for simple use cases
        context->Flush();

        context->Fill(Onyx::Color::RED);
        context->Circle();
    }
}

static void RunAppExample3()
{
    class MyLayer : public Onyx::UserLayer
    {
      public:
        using Onyx::UserLayer::UserLayer;
        void OnFrameBegin(const Onyx::DeltaTime &, const Onyx::FrameInfo &) override
        {
#ifdef ONYX_ENABLE_IMGUI
            ImGui::Begin("Hello, World!");
            ImGui::Text("Hello, World from ImGui!");
            ImGui::End();
#endif
        }
    };

    Onyx::Application app({.Name = "App3 Hello, World!", .Dimensions = u32v2{800, 600}});
    app.SetUserLayer<MyLayer>();
#if defined(__ONYX_MULTI_WINDOW) && defined(ONYX_ENABLE_IMGUI)
    app.OpenWindow({.Specs = {.Name = "Who's this??", .Dimensions = u32v2{800, 600}}, .EnableImGui = false});
#endif

    app.Run();
}

int main()
{
    TKit::ThreadPool threadPool{ONYX_MAX_WORKERS};

    Onyx::Core::Initialize(Onyx::Specs{.TaskManager = &threadPool});

    RunStandaloneWindow();
    RunAppExample1();
    RunAppExample2();
    RunAppExample3();
    Onyx::Core::Terminate();
}
