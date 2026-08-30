#pragma once

#include "alias.hpp"
#include "tkit/container/ecs.hpp"

namespace Engine
{
void Initialize();
void Terminate();
void Run();

using Scene = u32;
using Viewport = u32;
using RenderView = u32;
using RenderContext = u32;

constexpr Scene NullScene = TKit::Limits<Scene>::Max();
constexpr Viewport NullViewport = TKit::Limits<Viewport>::Max();
constexpr RenderView NullRenderView = TKit::Limits<RenderView>::Max();
constexpr RenderContext NullRenderContext = TKit::Limits<RenderContext>::Max();

Scene Scene_Create();
void Scene_Destroy(Scene sc);

Viewport Scene_CreateViewport(Scene sc, const u32v2 &size);
void Scene_DestroyViewport(Scene sc, Viewport vp);

template <Dimension D> RenderContext Scene_CreateRenderContext(Scene sc);
template <Dimension D> void Scene_DestroyRenderContext(Scene sc, RenderContext rc);

void Scene_FlushContexts(Scene sc);
void Scene_Render(Scene sc);

TKit::Registry &Scene_GetRegistry(Scene sc);

// on hold right now: we need to pass the camera component this view will be attached to when the scene plays
// in fact, everytime you ask for an entity camera, a view should be created for you
// IN FACT THESE SHOULD NOT BE EXPOSED!! in entity/runtime/play land, you dont create views automatically. you create
// entity cameras!

// template <Dimension D> RenderView Viewport_CreateRenderView(Viewport vp);
// template <Dimension D> void Viewport_DestroyRenderView(Viewport vp, RenderView rv);

} // namespace Engine
