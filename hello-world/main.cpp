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

static VKit::GraphicsJob SetupCustomPipeline(Onyx::Window &p_Window) noexcept
{
    const VKit::Shader fragment = Onyx::CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/rainbow.frag");

    auto lresult = VKit::PipelineLayout::Builder(Onyx::Core::GetDevice()).Build();
    VKIT_ASSERT_RESULT(lresult);
    VKit::PipelineLayout &layout = lresult.GetValue();

    const auto presult = VKit::GraphicsPipeline::Builder(Onyx::Core::GetDevice(), layout, p_Window.GetRenderPass())
                             .SetViewportCount(1)
                             .AddShaderStage(Onyx::GetFullPassVertexShader(), VK_SHADER_STAGE_VERTEX_BIT)
                             .AddShaderStage(fragment, VK_SHADER_STAGE_FRAGMENT_BIT)
                             .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                             .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                             .AddDefaultColorAttachment()
                             .Build();

    const VKit::GraphicsPipeline &pipeline = presult.GetValue();
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(fragment);
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(pipeline);

    VKIT_ASSERT_RESULT(presult);
    return VKit::GraphicsJob(pipeline, layout);
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
    const VKit::Shader shader = Onyx::CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/blur.frag");

    VKit::PipelineLayout::Builder builder = p_Window.GetPostProcessing()->CreatePipelineLayoutBuilder();

    const auto result = builder.AddPushConstantRange<BlurData>(VK_SHADER_STAGE_FRAGMENT_BIT).Build();
    VKIT_ASSERT_RESULT(result);
    const VKit::PipelineLayout &layout = result.GetValue();

    p_Window.SetupPostProcessing(layout, shader);
    static BlurData blurData{};

    p_Window.GetPostProcessing()->UpdatePushConstantRange(0, &blurData);

    Onyx::Core::GetDeletionQueue().SubmitForDeletion(shader);
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
}

static void RunStandaloneWindowCustomPipeline() noexcept
{
    Onyx::Window::Specs specs;
    specs.Name = "Standalone Hello, World! With a custom rainbow background and a post-processing effect!";
    specs.Width = 800;
    specs.Height = 600;

    Onyx::Window window(specs);

    const VKit::GraphicsJob job = SetupCustomPipeline(window);
    SetupPostProcessing(window);

    while (!window.ShouldClose())
    {
        Onyx::Input::PollEvents();

        Onyx::RenderContext<D2> *context = window.GetRenderContext<D2>();
        context->Flush(Onyx::Color::BLACK);

        context->Fill(Onyx::Color::RED);
        context->Square();

        window.RenderSubmitFirst([&job](const VkCommandBuffer p_CommandBuffer) {
            job.Bind(p_CommandBuffer);
            job.Draw(p_CommandBuffer, 3);
        });
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

    Onyx::Application app(specs);
    app.SetUserLayer<MyLayer>();

    app.Run();
}

int main()
{
    TKit::ThreadPool<std::mutex> threadPool(7);

    Onyx::Core::Initialize(&threadPool);
    RunStandaloneWindow();
    RunStandaloneWindowCustomPipeline();
    RunAppExample1();
    RunAppExample2();
    RunAppExample3();
    Onyx::Core::Terminate();
}