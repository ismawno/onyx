#include "engine.hpp"
#include "alias.hpp"
#include "components.hpp"

namespace Engine
{
template <Dimension D> static void scene_Render(TKit::Registry &r)
{
    const auto &query = r.Query<TransformComponent<D>, StaticMeshComponent<D>, RenderContextComponent<D>>();
    query.Each([&](const TransformComponent<D> &transform, const StaticMeshComponent<D> &statMesh,
                   const RenderContextComponent<D> &context) {
        Onyx::RenderContext<D> *ctx = context.Context;
        if (!ctx)
            return;
        const Onyx::Resource mesh = statMesh.Mesh;
        const Onyx::Transform<D> &t = transform.Transform;

        ctx->FillColor(statMesh.Color);
        ctx->StaticMesh(mesh, t.ComputeTransform());
    });
}

void Scene_Render(const Scene sc)
{
    TKit::Registry &r = Scene_GetRegistry(sc);
    scene_Render<D2>(r);
    scene_Render<D3>(r);
}
} // namespace Engine
