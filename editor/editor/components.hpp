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

void RegisterComponents(TKit::Registry &registry);

} // namespace Editor
