#include "onyx/app/window.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/core/core.hpp"

using namespace Onyx;

int main()
{
    Core::Initialize();
    {
        RenderContext<D2> *ctx = Renderer::CreateContext<D2>();
        Window window{};
        ctx->AddTarget(&window);
        window.CreateCamera<D2>();
        const StatMeshData<D2> data = Assets::CreateSquareMesh<D2>();
        const Mesh mesh = Assets::AddMesh(data);
        Assets::Upload<D2>();

        while (!window.ShouldClose())
        {
            Input::PollEvents();
            if (window.AcquireNextImage())
            {
                ctx->Flush();
                ctx->StaticMesh(mesh);

                Execution::UpdateCompletedQueueTimelines();
                Renderer::Coalesce();

                VKit::Queue *tqueue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
                VKit::Queue *gqueue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

                CommandPool &tpool = Execution::FindSuitableCommandPool(VKit::Queue_Transfer);
                CommandPool &gpool = Execution::FindSuitableCommandPool(VKit::Queue_Graphics);

                const VkCommandBuffer tcmd = tpool.Allocate();
                const VkCommandBuffer gcmd = gpool.Allocate();

                Execution::BeginCommandBuffer(tcmd);
                const Renderer::TransferSubmitInfo tsinfo = Renderer::Transfer(tqueue, tcmd);
                Execution::EndCommandBuffer(tcmd);
                if (tsinfo)
                    Renderer::SubmitTransfer(tqueue, tpool, tsinfo);

                Execution::BeginCommandBuffer(gcmd);
                Renderer::ApplyAcquireBarriers(gcmd);

                window.BeginRendering(gcmd);
                const Renderer::RenderSubmitInfo rsinfo = Renderer::Render(gqueue, gcmd, &window);
                window.EndRendering(gcmd);
                Execution::EndCommandBuffer(gcmd);

                Renderer::SubmitRender(gqueue, gpool, rsinfo);
                window.Present(rsinfo);
            }
            Execution::RevokeUnsubmittedQueueTimelines();
        }
        Renderer::DestroyContext(ctx);
    }
    Core::Terminate();
}
