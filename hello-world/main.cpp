#include "onyx/app/app.hpp"
#include "onyx/app/window.hpp"
#include "onyx/core/shaders.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "vkit/pipeline/pipeline_job.hpp"
#include "vkit/vulkan/vulkan.hpp"
#include "onyx/core/imgui.hpp"

using Onyx::D2;
using namespace TKit::Alias;

#define ONYX_MAX_WORKERS (ONYX_MAX_THREADS - 1)

static void RunStandaloneWindow()
{
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
}

static VKit::GraphicsJob SetupCustomPipeline(Onyx::Window &p_Window)
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

    VKIT_ASSERT_RESULT(presult);
    const VKit::GraphicsPipeline &pipeline = presult.GetValue();

    fragment.Destroy();
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(layout);
    Onyx::Core::GetDeletionQueue().SubmitForDeletion(pipeline);

    const auto jresult = VKit::GraphicsJob::Create(pipeline, layout);
    VKIT_ASSERT_RESULT(jresult);
    return jresult.GetValue();
}

static void SetPostProcessing(Onyx::Window &p_Window)
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

static void RunStandaloneWindowCustomPipeline()
{
    Onyx::Window window(
        {.Name = "Standalone Hello, World! With a custom rainbow background and a post-processing effect!",
         .Width = 800,
         .Height = 600});

    const VKit::GraphicsJob job = SetupCustomPipeline(window);
    SetPostProcessing(window);
    Onyx::RenderContext<D2> *context = window.CreateRenderContext<D2>();
    window.CreateCamera<D2>()->Transparent = true;

    Onyx::RenderCallbacks cbs{};
    cbs.OnRenderBegin = [&job](const u32, const VkCommandBuffer p_CommandBuffer) {
        job.Bind(p_CommandBuffer);
        job.Draw(p_CommandBuffer, 3);
    };

    while (!window.ShouldClose())
    {
        Onyx::Input::PollEvents();

        context->Flush();

        context->Fill(Onyx::Color::RED);
        context->Square();

        window.Render(cbs);
    }
}

static void RunAppExample1()
{
    Onyx::Application app({.Name = "App1 Hello, World!", .Width = 800, .Height = 600});
    app.Run();
}

static void RunAppExample2()
{
    Onyx::Application app({.Name = "App2 Hello, World!", .Width = 800, .Height = 600});

    const auto result = Onyx::Mesh<D2>::Load(ONYX_ROOT_PATH "/onyx/meshes/square.obj");
    VKIT_ASSERT_RESULT(result);
    Onyx::Mesh<D2> square = result.GetValue();
    Onyx::RenderContext<D2> *context = app.GetMainWindow()->CreateRenderContext<D2>();
    app.GetMainWindow()->CreateCamera<D2>();

    TKit::Clock clock;
    app.Startup();
    while (app.NextFrame(clock))
    {
        context->Flush();

        context->Fill(Onyx::Color::RED);
        context->Mesh(square);
    }
    square.Destroy();
    app.Shutdown();
}

static void RunAppExample3()
{
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

    Onyx::Application app({.Name = "App3 Hello, World!", .Width = 800, .Height = 600});
    app.SetUserLayer<MyLayer>();

    app.Run();
}

int main()
{
    TKit::ThreadPool threadPool{ONYX_MAX_WORKERS};

    Onyx::Core::Initialize(Onyx::Specs{.TaskManager = &threadPool});
    RunStandaloneWindow();
    RunStandaloneWindowCustomPipeline();
    RunAppExample1();
    RunAppExample2();
    RunAppExample3();
    Onyx::Core::Terminate();
}
