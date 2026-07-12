#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"

using Onyx::D2;
using Onyx::D3;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    const Onyx::Resource lit2 = Onyx::Resources::RegisterMaterial<D2>();
    const Onyx::Resource unlit2 = Onyx::Resources::RegisterMaterial<D2>({.Occluder = true});

    const Onyx::Resource mat3 = Onyx::Resources::RegisterMaterial<D3>();

    Onyx::Resources::Sync(Onyx::SyncFlag_Materials);

    Onyx::Window *win = Onyx::OpenWindow();

    const Onyx::RenderViewFlags vflags = Onyx::RenderViewFlag_NormalizedCoordinates | Onyx::RenderViewFlag_Shadows;

    Onyx::Camera<D2> cam2{};
    Onyx::RenderView<D2> *view2 = win->CreateRenderView<D2>(&cam2, vflags);
    Onyx::RenderContext<D2> *ctx2 = Onyx::CreateRenderContext<D2>();

    cam2.OrthoParameters.Size = 10.f;
    view2->SetNormalizedViewport({.Position = 0.f, .Extent = {0.5f, 1.f}});
    ctx2->AddTarget(view2);

    Onyx::Camera<D3> cam3{};
    Onyx::RenderView<D3> *view3 = win->CreateRenderView<D3>(&cam3, vflags);
    Onyx::RenderContext<D3> *ctx3 = Onyx::CreateRenderContext<D3>();

    cam3.View.Translation[2] = 5.f;
    view3->SetNormalizedViewport({.Position = {0.5f, 0.f}, .Extent = {0.5f, 1.f}});
    ctx3->AddTarget(view3);

    f32 time = 0.f;
    while (Onyx::Running())
    {
        const TKit::Timespan dt = Onyx::GetDeltaTime(win);
        time += dt.AsSeconds();

        Onyx::RenderView<D2> *v2 = win->GetMouseRenderView<D2>();
        Onyx::RenderView<D3> *v3 = win->GetMouseRenderView<D3>();
        if (v2)
            win->ControlCamera(Onyx::GetDeltaTime(win), v2->GetCamera());
        else if (v3)
            win->ControlCamera(Onyx::GetDeltaTime(win), v3->GetCamera());

        {
            ctx2->Flush();
            ctx2->RenderFlags(Onyx::RenderModeFlag_Shaded);
            ctx2->Material(lit2);

            ctx2->Push();
            ctx2->Scale(20.f);
            ctx2->Quad();
            ctx2->Pop();

            ctx2->Material(unlit2);
            ctx2->DirectionalLight({.DepthBias = -0.00001f, .Flags = Onyx::LightFlag_CastShadows});
            ctx2->Rotate(time);
            ctx2->FillColor(Onyx::Color_Red);
            ctx2->Quad();
        }

        {
            ctx3->Flush();

            ctx3->Material(mat3);
            ctx3->RenderFlags(Onyx::RenderModeFlag_Shaded);
            ctx3->DirectionalLight({.Cascades = {.View = view3}, .Flags = Onyx::LightFlag_CastShadows});

            ctx3->Push();
            ctx3->RotateZ(time);
            ctx3->FillColor(Onyx::Color_Red);
            ctx3->Box();
            ctx3->Pop();

            ctx3->ScaleX(20.f);
            ctx3->ScaleY(20.f);
            ctx3->RotateX(0.5f * Onyx::Math::Pi());
            ctx3->TranslateY(-2.f);
            ctx3->Quad();
        }

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
