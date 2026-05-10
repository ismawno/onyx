#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"

using Onyx::D2;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();

    const Onyx::StatMeshData<D2> tdata = Onyx::CreateTriangleMeshData<D2>();
    const Onyx::ResourcePool mpool = Onyx::Resources::CreateResourcePool<D2>(Onyx::Resource_StaticMesh);
    const Onyx::Resource triangle = Onyx::Resources::RegisterMesh(mpool, tdata);
    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();

    Onyx::Window *win = Onyx::OpenWindow();

    Onyx::Camera<D2> cam{};

    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    ctx->AddTarget(view);

    Onyx::SetTargetDeltaTime(win, TKit::Timespan::FromSeconds(1.f / 60.f));
    f32 time = 0.f;
    while (Onyx::Running())
    {
        time += Onyx::GetDeltaTime(win).AsSeconds();
        ctx->Flush();
        ctx->Rotate(time);
        ctx->StaticMesh(triangle);

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
