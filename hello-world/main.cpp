#include "onyx/app/app.hpp"
#include "onyx/app/window.hpp"
#include "onyx/core/shaders.hpp"
#include "onyx/rendering/camera.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "vkit/pipeline/pipeline_job.hpp"
#include "vkit/vulkan/vulkan.hpp"
#include <imgui.h>

using Onyx::D2;
using namespace TKit::Alias;

static void RunStandaloneWindow() noexcept
{
    Onyx::Window window({.Name = "Standalone Hello, World!", .Width = 800, .Height = 600});
    Onyx::RenderContext<D2> *context = window.CreateRenderContext<D2>();
    context->CreateCamera();

    while (!window.ShouldClose())
    {
        Onyx::Input::PollEvents();

        context->Flush(Onyx::Color::BLACK);

        context->Fill(Onyx::Color::RED);
        context->Square();

        window.Render();
    }
}

static VKit::GraphicsJob SetupCustomPipeline(Onyx::Window &p_Window) noexcept
{
    VKit::Shader fragment = Onyx::CreateShader(ONYX_ROOT_PATH "/demo-utils/shaders/rainbow.frag");

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

    VKIT_ASSERT_RESULT(presult);
    const VKit::GraphicsPipeline &pipeline = presult.GetValue();

    fragment.Destroy();
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(pipeline);

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

    VKit::PipelineLayout::Builder builder = p_Window.GetPostProcessing()->CreatePipelineLayoutBuilder();

    const auto result = builder.AddPushConstantRange<BlurData>(VK_SHADER_STAGE_FRAGMENT_BIT).Build();
    VKIT_ASSERT_RESULT(result);
    const VKit::PipelineLayout &layout = result.GetValue();

    p_Window.SetPostProcessing(layout, shader);
    static BlurData blurData{};

    p_Window.GetPostProcessing()->UpdatePushConstantRange(0, &blurData);

    shader.Destroy();
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
}

static void RunStandaloneWindowCustomPipeline() noexcept
{
    Onyx::Window window(
        {.Name = "Standalone Hello, World! With a custom rainbow background and a post-processing effect!",
         .Width = 800,
         .Height = 600});

    const VKit::GraphicsJob job = SetupCustomPipeline(window);
    SetPostProcessing(window);
    Onyx::RenderContext<D2> *context = window.CreateRenderContext<D2>();
    context->CreateCamera();

    while (!window.ShouldClose())
    {
        Onyx::Input::PollEvents();

        context->Flush(Onyx::Color::BLACK);

        context->Fill(Onyx::Color::RED);
        context->Square();

        window.RenderSubmitFirst([&job](const u32, const VkCommandBuffer p_CommandBuffer) {
            job.Bind(p_CommandBuffer);
            job.Draw(p_CommandBuffer, 3);
        });
    }
}

static void RunAppExample1() noexcept
{
    Onyx::Application app({.Name = "App1 Hello, World!", .Width = 800, .Height = 600});
    app.Run();
}

static void RunAppExample2() noexcept
{
    Onyx::Application app({.Name = "App2 Hello, World!", .Width = 800, .Height = 600});

    const auto result = Onyx::Mesh<D2>::Load(ONYX_ROOT_PATH "/onyx/meshes/square.obj");
    VKIT_ASSERT_RESULT(result);
    Onyx::Mesh<D2> square = result.GetValue();
    Onyx::RenderContext<D2> *context = app.GetMainWindow()->CreateRenderContext<D2>();
    context->CreateCamera();

    TKit::Clock clock;
    app.Startup();
    while (app.NextFrame(clock))
    {
        context->Flush(Onyx::Color::BLACK);

        context->Fill(Onyx::Color::RED);
        context->Mesh(square);
    }
    square.Destroy();
    app.Shutdown();
}

static void RunAppExample3() noexcept
{
    class MyLayer : public Onyx::UserLayer
    {
      public:
        void OnRender(const u32, const VkCommandBuffer) noexcept override
        {
            ImGui::Begin("Hello, World!");
            ImGui::Text("Hello, World from ImGui!");
            ImGui::End();
        }
    };

    Onyx::Application app({.Name = "App3 Hello, World!", .Width = 800, .Height = 600});
    app.SetUserLayer<MyLayer>();

    app.Run();
}

int main()
{
    TKit::ThreadPool threadPool(7);

    Onyx::Core::Initialize(&threadPool);
    RunStandaloneWindow();
    RunStandaloneWindowCustomPipeline();
    RunAppExample1();
    RunAppExample2();
    RunAppExample3();
    Onyx::Core::Terminate();
}
