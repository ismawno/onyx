#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"

using Onyx::D3;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    const Onyx::Resource mat = Onyx::Resources::RegisterMaterial<D3>();
    Onyx::Resources::Sync(Onyx::SyncFlag_Materials);

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});

    Onyx::Camera<D3> cam{};
    Onyx::RenderView<D3> *view =
        win->CreateRenderView<D3>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates | Onyx::RenderViewFlag_Shadows);

    Onyx::RenderContext<D3> *ctx = Onyx::CreateRenderContext<D3>();
    ctx->AddTarget(view);

    f32 time = 0.f;

    cam.View.Translation[2] = 5.f;
    while (Onyx::Running())
    {
        win->ControlCamera(Onyx::GetDeltaTime(win), &cam);
        time += Onyx::GetDeltaTime(win).AsSeconds();

        ctx->Flush();

        ctx->Material(mat);
        ctx->RenderFlags(Onyx::RenderModeFlag_Shaded);
        ctx->DirectionalLight({.Cascades = {.View = view}, .Flags = Onyx::LightFlag_CastShadows});

        ctx->Push();
        ctx->RotateZ(time);
        ctx->FillColor(Onyx::Color_Red);
        ctx->Box();
        ctx->Pop();

        ctx->ScaleX(20.f);
        ctx->ScaleY(20.f);
        ctx->RotateX(0.5f * Onyx::Math::Pi());
        ctx->TranslateY(-2.f);
        ctx->Quad();

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
