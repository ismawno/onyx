#pragma once

#include "alias.hpp"
#include "onyx/handle.hpp"
#include "onyx/transform.hpp"
#include "onyx/context.hpp"
#include "tkit/container/ecs.hpp"

namespace Engine
{
using TKit::ComponentId;
using TKit::Entity;
using TKit::NullEntity;

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

template <Dimension D>
using ComponentsWithDimension =
    TKit::ComponentSet<TransformComponent<D>, StaticMeshComponent<D>, RenderContextComponent<D>>;
using ComponentsWithoutDimension = TKit::ComponentSet<NameComponent>;

template <Dimension D>
using AllComponentsWithDimension =
    TKit::ComponentSet<NameComponent, TransformComponent<D>, StaticMeshComponent<D>, RenderContextComponent<D>>;

using AllComponents =
    TKit::MergeComponentSets<ComponentsWithoutDimension, ComponentsWithDimension<D2>, ComponentsWithDimension<D3>>;

} // namespace Engine
