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

enum StaticMesh : u8
{
    StaticMesh_Triangle,
    StaticMesh_Quad,
    StaticMesh_Box,
    StaticMesh_Sphere,
    StaticMesh_Cylinder,
    StaticMesh_Custom,
    StaticMesh_None,
};

struct NameComponent
{
    TKIT_YAML_SERIALIZE_DECLARE(NameComponent)
    TKit::TierString Name;
};
template <Dimension D> struct TransformComponent
{
    TKIT_YAML_SERIALIZE_DECLARE(TransformComponent)
    Onyx::Transform<D> Transform;
};
template <Dimension D> struct StaticMeshComponent
{
    TKIT_YAML_SERIALIZE_DECLARE(StaticMeshComponent)
    Onyx::Resource Mesh = Onyx::NullHandle;
    Onyx::Color Color = Onyx::Color_White;
    StaticMesh Type = StaticMesh_None;
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
