#include "onyx/application/application.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/core/core.hpp"
#include "tkit/container/stack_array.hpp"

using namespace Onyx;

void WindowExample(const Mesh mesh, const u32 nwidows = 1)
{
    RenderContext<D2> *ctx = Renderer::CreateContext<D2>();
    TKit::StackArray<Window *> windows{};
    windows.Reserve(nwidows);
    for (u32 i = 0; i < nwidows; ++i)
    {
        Window *win = windows.Append(ONYX_CHECK_EXPRESSION(Platform::CreateWindow()));
        ctx->AddTarget(win);
        win->CreateCamera<D2>()->BackgroundColor = Color{0.1f};
    }

    while (!windows.IsEmpty())
    {
        Input::PollEvents();
        ctx->Flush();
        ctx->Fill(Color{255u, 255u, 0u});
        ctx->StaticMesh(mesh);
        ctx->TranslateX(0.5f);
        ctx->Circle();

        ONYX_CHECK_EXPRESSION(Execution::UpdateCompletedQueueTimelines());
        // Renderer::Coalesce();

        VKit::Queue *tqueue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
        VKit::Queue *gqueue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

        CommandPool *tpool = ONYX_CHECK_EXPRESSION(Execution::FindSuitableCommandPool(VKit::Queue_Transfer));
        CommandPool *gpool = ONYX_CHECK_EXPRESSION(Execution::FindSuitableCommandPool(VKit::Queue_Graphics));

        const VkCommandBuffer tcmd = ONYX_CHECK_EXPRESSION(Execution::Allocate(tpool));

        ONYX_CHECK_EXPRESSION(Execution::BeginCommandBuffer(tcmd));
        const Renderer::TransferSubmitInfo tsinfo = ONYX_CHECK_EXPRESSION(Renderer::Transfer(tqueue, tcmd));
        ONYX_CHECK_EXPRESSION(Execution::EndCommandBuffer(tcmd));

        if (tsinfo)
            ONYX_CHECK_EXPRESSION(Renderer::SubmitTransfer(tqueue, tpool, tsinfo));

        TKit::StackArray<Renderer::RenderSubmitInfo> rinfos{};
        rinfos.Reserve(windows.GetSize());
        u64 acquireMask = 0;
        for (Window *win : windows)
            if (ONYX_CHECK_EXPRESSION(win->AcquireNextImage()))
            {
                acquireMask |= win->GetViewBit();
                const VkCommandBuffer gcmd = ONYX_CHECK_EXPRESSION(Execution::Allocate(gpool));
                ONYX_CHECK_EXPRESSION(Execution::BeginCommandBuffer(gcmd));
                Renderer::ApplyAcquireBarriers(gcmd);

                win->BeginRendering(gcmd);
                rinfos.Append(ONYX_CHECK_EXPRESSION(Renderer::Render(gqueue, gcmd, win)));
                win->EndRendering(gcmd);

                ONYX_CHECK_EXPRESSION(Execution::EndCommandBuffer(gcmd));
            }

        if (!rinfos.IsEmpty())
            ONYX_CHECK_EXPRESSION(Renderer::SubmitRender(gqueue, gpool, rinfos));

        u32 index = 0;
        for (Window *win : windows)
            if (win->GetViewBit() & acquireMask)
                ONYX_CHECK_EXPRESSION(win->Present(rinfos[index++]));

        Execution::RevokeUnsubmittedQueueTimelines();
        for (u32 i = windows.GetSize() - 1; i < windows.GetSize(); --i)
            if (windows[i]->ShouldClose())
            {
                Platform::DestroyWindow(windows[i]);
                windows.RemoveUnordered(windows.begin() + i);
            }
    }
}

void ApplicationExample()
{
    class WinLayer final : public WindowLayer
    {
      public:
        WinLayer(ApplicationLayer *appLayer, Window *window)
            : WindowLayer(appLayer, window, WindowLayerFlag_ImGuiEnabled)
        {
        }

        void OnRender(const DeltaTime &) override
        {
            ApplicationLayer *appLayer = GetApplicationLayer();
            Window *window = GetWindow();
            ImGui::Begin("Hello");
            if (ImGui::Button("Spawn"))
                appLayer->RequestOpenWindow<WinLayer>();
            PresentModeEditor(window);
            DeltaTimeEditor();
            ImGui::End();
        }
    };
    Application app{};
    ONYX_CHECK_EXPRESSION(app.OpenWindow<WinLayer>());
    ONYX_CHECK_EXPRESSION(app.Run());
}

int main()
{
    ONYX_CHECK_EXPRESSION(Core::Initialize());
    const StatMeshData<D2> data = Assets::CreateSquareMesh<D2>();
    const Mesh mesh = Assets::AddMesh(data);
    ONYX_CHECK_EXPRESSION(Assets::Upload<D2>());

    WindowExample(mesh, 10);
    // ApplicationExample();

    Core::Terminate();
}
