#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"

using Onyx::D2;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    const Onyx::Resource lit = Onyx::Resources::RegisterMaterial<D2>();
    const Onyx::Resource unlit = Onyx::Resources::RegisterMaterial<D2>({.Occluder = true});
    Onyx::Resources::Sync(Onyx::SyncFlag_Materials);

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});

    Onyx::Camera<D2> cam{};
    cam.OrthoParameters.Size = 10.f;
    Onyx::RenderView<D2> *view =
        win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates | Onyx::RenderViewFlag_Shadows);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(view);

    f32 time = 0.f;
    while (Onyx::Running())
    {
        win->ControlCamera(Onyx::GetDeltaTime(win), &cam);
        time += Onyx::GetDeltaTime(win).AsSeconds();

        ctx->Flush();
        ctx->RenderFlags(Onyx::RenderModeFlag_Shaded);
        ctx->Material(lit);

        ctx->Push();
        ctx->Scale(20.f);
        ctx->Quad();
        ctx->Pop();

        ctx->Material(unlit);
        ctx->DirectionalLight({.DepthBias = -0.00001f, .Flags = Onyx::LightFlag_CastShadows});
        ctx->Rotate(time);
        ctx->FillColor(Onyx::Color_Red);
        ctx->Quad();

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
