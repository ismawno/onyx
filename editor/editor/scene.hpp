#pragma once

#include "alias.hpp"
#include "tkit/container/ecs.hpp"
#include "onyx/context.hpp"

namespace Editor
{
using TKit::ComponentId;
using TKit::Entity;
using TKit::NullEntity;

class Scene
{
  public:
    Scene();

    Entity CreateEntity(TKit::StringView name);
    Entity CreateEntity()
    {
        return Registry.CreateEntity();
    }

    void Draw();

    template <Dimension D> void AddTarget(const Onyx::RenderView<D> *view, const u32 contextIdx = 0) const
    {
        GetRenderContext<D>(contextIdx)->AddTarget(view);
    }

    template <Dimension D> const TKit::TierArray<Onyx::RenderContext<D> *> &GetRenderContexts() const
    {
        if constexpr (D == D2)
            return m_Contexts2;
        else
            return m_Contexts3;
    }
    template <Dimension D> Onyx::RenderContext<D> *GetRenderContext(const u32 idx) const
    {
        return GetRenderContexts<D>()[idx];
    }
    template <Dimension D> Onyx::RenderContext<D> *GetMainRenderContext() const
    {
        return GetRenderContext<D>(0);
    }

    TKit::Registry Registry{};

  private:
    template <Dimension D> void draw();

    TKit::TierArray<Onyx::RenderContext<D2> *> m_Contexts2{};
    TKit::TierArray<Onyx::RenderContext<D3> *> m_Contexts3{};
};
} // namespace Editor
