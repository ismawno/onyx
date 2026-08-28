#include "scene.hpp"
#include "components.hpp"
#include "onyx/onyx.hpp"

namespace Editor
{
Scene::Scene()
{
    RegisterComponents(Registry);
    m_Contexts2.Append(Onyx::CreateRenderContext<D2>());
    m_Contexts3.Append(Onyx::CreateRenderContext<D3>());
}
Entity Scene::CreateEntity(const TKit::StringView name)
{
    const Entity e = Registry.CreateEntity();
    Registry.AddComponent<NameComponent>(e, name);
    return e;
}

void Scene::Draw()
{
    draw<D2>();
    draw<D3>();
}

template <Dimension D> void Scene::draw()
{
    for (Onyx::RenderContext<D> *ctx : GetRenderContexts<D>())
        ctx->Flush();

    const auto &query = Registry.Query<TransformComponent<D>, StaticMeshComponent<D>, RenderContextComponent<D>>();
    query.Each([&](const TransformComponent<D> &transform, const StaticMeshComponent<D> &statMesh,
                   const RenderContextComponent<D> &context) {
        Onyx::RenderContext<D> *ctx = context.Context;
        const Onyx::Resource mesh = statMesh.Mesh;
        const Onyx::Transform<D> &t = transform.Transform;

        ctx->FillColor(statMesh.Color);
        ctx->StaticMesh(mesh, t.ComputeTransform());
    });
}

} // namespace Editor
