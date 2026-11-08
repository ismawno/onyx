#pragma once

#include "alias.hpp"
#include "onyx/overlay.hpp"
#include "tkit/container/ecs.hpp"

namespace Engine
{
bool Initialize();
void Terminate();
void Run();

constexpr Scene NullScene = TKit::Limits<Scene>::Max();
constexpr Viewport NullViewport = TKit::Limits<Viewport>::Max();
constexpr RenderView NullRenderView = TKit::Limits<RenderView>::Max();
constexpr RenderContext NullRenderContext = TKit::Limits<RenderContext>::Max();

Scene Scene_Create();
void Scene_Destroy(Scene sc);

Viewport Scene_CreateViewport(Scene sc, const u32v2 &size);
void Scene_DestroyViewport(Scene sc, Viewport vp);

template <Dimension D> RenderContext Scene_CreateRenderContext(Scene sc);
template <Dimension D> void Scene_DestroyRenderContext(Scene sc, RenderContext ctx);

void Scene_FlushContexts(Scene sc);
void Scene_Render(Scene sc);

ONYX_NO_DISCARD Result<> Scene_Serialize(Scene sc, const TKit::TierPath &path);
ONYX_NO_DISCARD Result<> Scene_Deserialize(Scene sc, const TKit::TierPath &path);
ONYX_NO_DISCARD inline Result<Scene> Scene_Deserialize(const TKit::TierPath &path)
{
    const Scene sc = Scene_Create();
    TKIT_RETURN_IF_FAILED(Scene_Deserialize(sc, path));
    return sc;
}

TKit::Registry &Scene_GetRegistry(Scene sc);

// on hold right now: we need to pass the camera component this view will be attached to when the scene plays
// in fact, everytime you ask for an entity camera, a view should be created for you
// IN FACT THESE SHOULD NOT BE EXPOSED!! in entity/runtime/play land, you dont create views automatically. you create
// entity cameras!

// template <Dimension D> RenderView Viewport_CreateRenderView(Viewport vp);
// template <Dimension D> void Viewport_DestroyRenderView(Viewport vp, RenderView rv);

struct ProjectSettings
{
    TKIT_YAML_SERIALIZE_DECLARE(ProjectSettings)
    Onyx::OverlayFlags OverlayFlags = Onyx::OverlayFlag_Docking;
    Onyx::PresentMode PresentMode = Onyx::PresentMode_Immediate;
    f32 DeltaTargetMillis = 0.f;
    TKit::Timespan GetDeltaTarget() const
    {
        return TKit::Timespan::FromMilliseconds(DeltaTargetMillis);
    }
};
} // namespace Engine
