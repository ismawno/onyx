#include "onyx/application/application.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/core/core.hpp"
#include "tkit/container/stack_array.hpp"

using namespace Onyx;

void WindowExample(const Asset mesh, const u32 nwidows = 1)
{
    struct WindowView
    {
        Window *Win;
        RenderView<D2> *View;
    };
    RenderContext<D2> *ctx = Renderer::CreateContext<D2>();
    TKit::StackArray<WindowView> windows{};
    windows.Reserve(nwidows);

    Camera<D2> cam{};
    cam.OrthoParameters.Size = 5.f;
    for (u32 i = 0; i < nwidows; ++i)
    {
        WindowView &wv = windows.Append();
        wv.Win = Platform::CreateWindow();
        wv.View = wv.Win->CreateRenderView(&cam);
        ctx->AddTarget(wv.View);
    }

    while (!windows.IsEmpty())
    {
        Platform::PollEvents();
        ctx->Flush();
        ctx->FillColor(Color{255u, 255u, 0u});
        ctx->StaticMesh(mesh);
        ctx->TranslateX(0.5f);
        ctx->Circle();

        Execution::UpdateCompletedQueueTimelines();
        // Renderer::Coalesce();

        VKit::Queue *tqueue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
        VKit::Queue *gqueue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

        CommandPool *tpool = Execution::FindSuitableCommandPool(VKit::Queue_Transfer);
        CommandPool *gpool = Execution::FindSuitableCommandPool(VKit::Queue_Graphics);

        const VkCommandBuffer tcmd = Execution::Allocate(tpool);

        Execution::BeginCommandBuffer(tcmd);
        const TransferSubmitInfo tsinfo = Renderer::Transfer(tqueue, tcmd);
        Execution::EndCommandBuffer(tcmd);

        if (tsinfo)
            Renderer::SubmitTransfer(tqueue, tpool, tsinfo);

        TKit::StackArray<RenderSubmitInfo> rinfos{};
        rinfos.Reserve(windows.GetSize());
        u64 acquireMask = 0;
        Renderer::PrepareRender();
        for (const WindowView &wv : windows)
            if (wv.Win->AcquireNextImage(Block))
            {
                acquireMask |= wv.View->GetViewBit();
                const VkCommandBuffer gcmd = Execution::Allocate(gpool);
                Execution::BeginCommandBuffer(gcmd);
                Renderer::ApplyAcquireBarriers(gcmd);

                const RenderSubmitInfo rinfo = Renderer::Render(gqueue, gcmd, wv.Win);
                rinfos.Append(rinfo);

                Execution::EndCommandBuffer(gcmd);
            }

        if (!rinfos.IsEmpty())
            Renderer::SubmitRender(gqueue, gpool, rinfos);

        for (const WindowView &wv : windows)
            if (wv.View->GetViewBit() & acquireMask)
                wv.Win->Present();

        for (u32 i = windows.GetSize() - 1; i < windows.GetSize(); --i)
            if (windows[i].Win->ShouldClose())
            {
                Platform::DestroyWindow(windows[i].Win);
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
            : WindowLayer(appLayer, window, {.Flags = WindowLayerFlag_ImGuiEnabled})
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
    app.OpenWindow<WinLayer>();
    app.Run();
}

int main()
{
    Initialize();
    const StatMeshData<D2> data = CreateQuadMeshData<D2>();

    const AssetPool pool = Assets::CreateAssetPool<D2>(Asset_StaticMesh);

    const Asset mesh = Assets::CreateMesh(pool, data);
    Assets::Upload();

    WindowExample(mesh, 1);
    // ApplicationExample();

    Terminate();
}
