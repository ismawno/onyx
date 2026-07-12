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

    Onyx::Window *win = Onyx::OpenWindow();

    const Onyx::RenderViewFlags vflags = Onyx::RenderViewFlag_NormalizedCoordinates | Onyx::RenderViewFlag_Transparency;

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

            ctx2->Push();
            ctx2->Scale(20.f);
            ctx2->Quad();
            ctx2->Pop();

            ctx2->Rotate(time);

            ctx2->FillColor(Onyx::Color_Red);
            ctx2->Alpha(0.8f);
            ctx2->Quad();

            ctx2->FillColor(Onyx::Color_Green);
            ctx2->Alpha(0.7f);
            ctx2->Translate(0.2f);
            ctx2->Triangle();

            ctx2->FillColor(Onyx::Color_Blue);
            ctx2->Alpha(0.9f);
            ctx2->Translate(0.2f);
            ctx2->Stadium();
        }

        {
            ctx3->Flush();

            ctx3->Push();
            ctx3->ScaleX(20.f);
            ctx3->ScaleY(20.f);
            ctx3->RotateX(0.5f * Onyx::Math::Pi());
            ctx3->TranslateY(-2.f);
            ctx3->Quad();
            ctx3->Pop();

            ctx3->RotateZ(time);

            ctx3->FillColor(Onyx::Color_Red);
            ctx3->Alpha(0.8f);
            ctx3->Box();

            ctx3->FillColor(Onyx::Color_Green);
            ctx3->Alpha(0.7f);
            ctx3->TranslateZ(-2.f);
            ctx3->RoundedRect();

            ctx3->FillColor(Onyx::Color_Blue);
            ctx3->Alpha(0.9f);
            ctx3->TranslateZ(-2.f);
            ctx3->Capsule();
        }

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
