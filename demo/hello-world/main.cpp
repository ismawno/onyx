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
    RenderView<D2> *view = win->CreateRenderView<D2>(&cam);

    ctx->AddTarget(view);

    while (Running())
    {
        ctx->Flush();
        ctx->FillColor(Color{255u, 255u, 0u});
        ctx->StaticMesh(mesh);
        ctx->TranslateX(0.5f);
        ctx->Circle();

#ifdef ONYX_ENABLE_IMGUI
        if (CanRenderImGui(win))
        {
            ImGui::Begin("This is a test");
            ImGui::Text("This is some text");
            ImGui::End();
            ImGui::ShowDemoWindow();
        }
#endif

        Transfer();
        Render();
    }

    Terminate();
}
