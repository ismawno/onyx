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
    const RenderViewFlags f =
        RenderViewFlag_Outlines | RenderViewFlag_PostProcess | RenderViewFlag_NormalizedCoordinates;
    const bool norm = f & RenderViewFlag_NormalizedCoordinates;
    RenderView<D2> *view = win->CreateRenderView<D2>(&cam, f);

    ctx->AddTarget(view);

    SetTargetDeltaTime(win, TKit::Timespan::FromSeconds(1.f / 240.f));
    while (Running())
    {
        const f32v2 mpos = view->ScreenToWorld(win->GetNormalizedMousePosition());
        ctx->Flush();
        ctx->FillColor(Color_Green);
        ctx->StaticMesh(mesh);
        ctx->FillColor(Color_Blue);
        const bool lshift = win->IsKeyPressed(Key_LeftShift);
        ctx->Rotate(0.3f);
        if (lshift)
            ctx->BeginClip(ctx->WorldToLocal(mpos), f32v2{1.f});
        ctx->Scale(1.5f);
        ctx->StaticMesh(mesh);
        ctx->TranslateX(0.5f);
        ctx->Circle();
        if (lshift)
            ctx->EndClip();

        win->ControlCamera(GetDeltaTime(win), &cam);

#ifdef ONYX_ENABLE_IMGUI
        if (CanRenderImGui(win))
        {
            const f32v2 npos = win->GetNormalizedMousePosition();
            const f32v2 apos = win->GetAbsoluteMousePosition();
            const f32v2 wpos = view->ScreenToWorld(norm ? npos : apos);
            const f32v2 vpos = view->ScreenToViewport(norm ? npos : apos);
            const f32 dt = GetDeltaTime(win).AsMilliseconds();

            ImGui::Begin("This is a test");
            ImGui::Text("Delta time: %.2f", dt);
            ImGui::Text("Normalized mouse: (%.2f, %.2f)", npos[0], npos[1]);
            ImGui::Text("Absolute mouse: (%.0f, %.0f)", apos[0], apos[1]);

            ImGui::Text("World mouse: (%.2f, %.2f)", wpos[0], wpos[1]);
            ImGui::Text("Viewport mouse: (%.2f, %.2f)", vpos[0], vpos[1]);
            ImGui::End();

            if (win->IsMousePressed(Mouse_Button2))
            {
                Scissor sc{};
                sc.Position = vpos;
                if (norm)
                {
                    sc.Extent = 1.f - vpos;
                    view->SetNormalizedScissor(sc);
                }
                else
                {
                    sc.Extent = f32v2{view->GetExtent()} - vpos;
                    view->SetAbsoluteScissor(sc);
                }
            }
            else if (win->IsMousePressed(Mouse_Button1))
            {
                Viewport vp{};
                vp.Position = apos;
                vp.Extent = f32v2{view->GetParentExtent()} - apos;
                view->SetAbsoluteViewport(vp);
            }
        }
#endif

        Transfer();
        Render();
    }

    Terminate();
}
