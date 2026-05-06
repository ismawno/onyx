#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include <imgui.h>
#endif

using namespace Onyx;

int main()
{
    Initialize();
    const StatMeshData<D2> data = CreateQuadMeshData<D2>();
    const ResourcePool pool = Resources::CreateResourcePool<D2>(Resource_StaticMesh);

    const Resource mesh = Resources::RegisterMesh(pool, data);
    Resources::Sync(SyncFlag_All);

    RenderContext<D2> *ctx = CreateRenderContext<D2>();

#ifdef ONYX_ENABLE_IMGUI
    Window *win = OpenWindow({.Flags = OpenWindowFlag_EnableImGui});
#else
    Window *win = OpenWindow();
#endif

    Camera<D2> cam{};
    cam.OrthoParameters.Size = 5.f;
    RenderView<D2> *view = win->CreateRenderView<D2>(
        &cam, RenderViewFlag_NormalizedViewportCoordinates | RenderViewFlag_NormalizedScissorCoordinates |
                  RenderViewFlag_DynamicViewport | RenderViewFlag_Outlines | RenderViewFlag_PostProcess);

    ctx->AddTarget(view);

    while (Running())
    {
        ctx->Flush();
        ctx->AddRenderFlags(RenderModeFlag_Outlined);
        ctx->OutlineColor(Color_Orange);
        ctx->OutlineWidth(1.f);
        ctx->FillColor(Color_Blue);
        ctx->StaticMesh(mesh);
        ctx->TranslateX(0.5f);
        ctx->Circle();

        win->ControlCamera(GetDeltaTime(win), &cam);

#ifdef ONYX_ENABLE_IMGUI
        if (CanRenderImGui(win))
        {
            const f32v2 npos = win->GetNormalizedMousePosition();
            const f32v2 apos = win->GetAbsoluteMousePosition();
            const f32v2 wpos = view->ScreenToWorld(npos);

            ImGui::Begin("This is a test");
            ImGui::Text("Normalized mouse: (%.2f, %.2f)", npos[0], npos[1]);
            ImGui::Text("Absolute mouse: (%.0f, %.0f)", apos[0], apos[1]);
            ImGui::Text("World mouse: (%.2f, %.2f)", wpos[0], wpos[1]);
            ImGui::End();

            // Viewport vp{};
            // vp.Position = npos;
            // vp.Extent = 1.f - 2.f * npos;
            // view->SetNormalizedViewport(vp);
        }
#endif

        Transfer();
        Render();
    }

    Terminate();
}
