#include "components.hpp"

namespace Editor
{
void RegisterComponents(TKit::Registry &registry)
{
    registry
        .RegisterComponents<NameComponent, TransformComponent<D2>, StaticMeshComponent<D2>, RenderContextComponent<D2>,
                            TransformComponent<D3>, StaticMeshComponent<D3>, RenderContextComponent<D3>>();
}
} // namespace Editor
