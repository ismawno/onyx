#include "onyx/app/window.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/asset/assets.hpp"
#include "onyx/rendering/context.hpp"
#include "onyx/core/core.hpp"

using namespace Onyx;

int main()
{
    Core::Terminate();
    ONYX_CHECK_EXPRESSION(Core::Initialize());
    const StatMeshData<D2> data = Assets::CreateSquareMesh<D2>();
    const Mesh mesh = Assets::AddMesh(data);
    ONYX_CHECK_EXPRESSION(Assets::Upload<D2>());

    {
        Window *window = ONYX_CHECK_EXPRESSION(Window::Create());

        RenderContext<D2> *ctx = Renderer::CreateContext<D2>();
        ctx->AddTarget(window);
        window->CreateCamera<D2>();

        while (!window->ShouldClose())
        {
            Input::PollEvents();
            if (ONYX_CHECK_EXPRESSION(window->AcquireNextImage()))
            {
                ctx->Flush();
                ctx->StaticMesh(mesh);

                ONYX_CHECK_EXPRESSION(Execution::UpdateCompletedQueueTimelines());
                Renderer::Coalesce();

                VKit::Queue *tqueue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
                VKit::Queue *gqueue = Execution::FindSuitableQueue(VKit::Queue_Graphics);

                CommandPool *tpool = ONYX_CHECK_EXPRESSION(Execution::FindSuitableCommandPool(VKit::Queue_Transfer));
                CommandPool *gpool = ONYX_CHECK_EXPRESSION(Execution::FindSuitableCommandPool(VKit::Queue_Graphics));

                const VkCommandBuffer tcmd = ONYX_CHECK_EXPRESSION(tpool->Pool.Allocate());
                const VkCommandBuffer gcmd = ONYX_CHECK_EXPRESSION(gpool->Pool.Allocate());

                ONYX_CHECK_EXPRESSION(Execution::BeginCommandBuffer(tcmd));
                const Renderer::TransferSubmitInfo tsinfo = ONYX_CHECK_EXPRESSION(Renderer::Transfer(tqueue, tcmd));
                ONYX_CHECK_EXPRESSION(Execution::EndCommandBuffer(tcmd));

                if (tsinfo)
                    ONYX_CHECK_EXPRESSION(Renderer::SubmitTransfer(tqueue, tpool, tsinfo));

                ONYX_CHECK_EXPRESSION(Execution::BeginCommandBuffer(gcmd));
                Renderer::ApplyAcquireBarriers(gcmd);

                window->BeginRendering(gcmd);
                const Renderer::RenderSubmitInfo rsinfo = ONYX_CHECK_EXPRESSION(Renderer::Render(gqueue, gcmd, window));
                window->EndRendering(gcmd);

                ONYX_CHECK_EXPRESSION(Execution::EndCommandBuffer(gcmd));
                ONYX_CHECK_EXPRESSION(Renderer::SubmitRender(gqueue, gpool, rsinfo));
                ONYX_CHECK_EXPRESSION(window->Present(rsinfo));
            }
            Execution::RevokeUnsubmittedQueueTimelines();
        }
        Renderer::DestroyContext(ctx);
        Window::Destroy(window);
    }
    Core::Terminate();
}
