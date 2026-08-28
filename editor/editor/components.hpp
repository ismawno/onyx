#pragma once

#include "alias.hpp"
#include "onyx/handle.hpp"
#include "onyx/transform.hpp"
#include "onyx/context.hpp"
#include "tkit/container/ecs.hpp"

namespace Editor
{
struct NameComponent
{
    TKit::TierString Name;
};
template <Dimension D> struct TransformComponent
{
    Onyx::Transform<D> Transform;
};
template <Dimension D> struct StaticMeshComponent
{
    u32 Index = TKIT_U32_MAX;
    Onyx::Resource Mesh = Onyx::NullHandle;
    Onyx::Color Color = Onyx::Color_White;
};
template <Dimension D> struct RenderContextComponent
{
    Onyx::RenderContext<D> *Context = nullptr;
};

template <typename... Cs> struct ComponentList
{
};

using AllComponents =
    ComponentList<NameComponent, TransformComponent<D2>, StaticMeshComponent<D2>, RenderContextComponent<D2>,
                  TransformComponent<D3>, StaticMeshComponent<D3>, RenderContextComponent<D3>>;

template <typename F, typename... Cs> void ForEachComponentType(const ComponentList<Cs...>, F &&func)
{
    (func.template operator()<Cs>(), ...);
}
inline void RegisterComponents(TKit::Registry &registry)
{
    ForEachComponentType(AllComponents{}, [&]<typename C> { registry.RegisterComponent<C>(); });
}

} // namespace Editor
