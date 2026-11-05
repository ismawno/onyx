#include "editor_scene.hpp"
#include "onyx/onyx.hpp"

namespace Engine
{
Viewport EditorScene::CreateViewport(const u32v2 &resolution)
{
    const Viewport vp = Viewports.Insert();
    EditorViewport &viewport = Viewports[vp];

    viewport.Id = vp;
    viewport.ParentScene = Id;
    viewport.Target = Onyx::CreateRenderTexture(resolution);
    viewport.Name = ONYX_DEFAULT_NAME("Viewport", vp);
    viewport.Position = 0.f;
    viewport.Size = 1.f;
    viewport.Resolution = resolution;
    viewport.Aspect = f32(resolution[0]) / f32(resolution[1]);

    return vp;
}

void EditorScene::DestroyViewport(const Viewport vp)
{
    EditorViewport &viewport = Viewports[vp];

    // we do it "manually" cause we want to remove the refcounts of the cameras
    const TKit::StackArray<RenderView> rvs2 = viewport.Views2.GetValidIds();
    for (const RenderView rv : rvs2)
        DestroyRenderView<D2>(rv, viewport);

    const TKit::StackArray<RenderView> rvs3 = viewport.Views3.GetValidIds();
    for (const RenderView rv : rvs3)
        DestroyRenderView<D3>(rv, viewport);

    Onyx::DestroyRenderTexture(viewport.Target);
    Viewports.Remove(vp);
}

template <Dimension D> RenderView EditorScene::CreateRenderView(EditorViewport &viewport, const u32 camIdx)
{
    EditorCamera<D> &cam = GetCameras<D>()[camIdx];
    ++cam.RefCount;

    TKit::TierHive<EditorView<D>> &rvs = viewport.GetViews<D>();
    const RenderView rv = rvs.Insert();

    EditorView<D> &rview = rvs[rv];
    rview.Id = rv;
    rview.ParentViewport = viewport.Id;
    rview.Name = ONYX_DEFAULT_NAME("View", rv);
    rview.Handle = viewport.Target->CreateRenderView<D>(cam.Handle);
    rview.EditorCameraIndex = camIdx;

    return rv;
}
template <Dimension D> void EditorScene::DestroyRenderView(const RenderView rv, EditorViewport &viewport)
{
    TKit::TierHive<EditorView<D>> &rvs = viewport.GetViews<D>();
    EditorView<D> &rview = rvs[rv];
    EditorCamera<D> &cam = GetCameras<D>()[rview.EditorCameraIndex];

    TKIT_ASSERT(cam.Handle == rview.Handle->GetCamera(),
                "[ONYX][EDITOR] When destroying a render view, the passed camera must be attached to the view");
    --cam.RefCount;
    viewport.Target->DestroyRenderView(rview.Handle);
    rvs.Remove(rv);
}

template <Dimension D> EditorCamera<D> &EditorScene::CreateCamera()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    TKit::TierArray<EditorCamera<D>> &cams = GetCameras<D>();
    return cams.Append(Id, ONYX_DEFAULT_NAME("Camera", cams.GetSize()), tier->Create<Onyx::Camera<D>>(), 0);
}
template <Dimension D> void EditorScene::DestroyCamera(const u32 camIdx)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    TKit::TierArray<EditorCamera<D>> &cams = GetCameras<D>();
    tier->Destroy(cams[camIdx].Handle);
    cams.RemoveOrdered(cams.begin() + camIdx);
}

template <Dimension D> RenderContext EditorScene::CreateRenderContext()
{
    TKit::TierHive<EditorContext<D>> &ctxs = GetContexts<D>();

    const RenderContext ctx = ctxs.Insert();
    EditorContext<D> &ectx = ctxs[ctx];
    ectx.Id = ctx;
    ectx.ParentScene = Id;
    ectx.Name = ONYX_DEFAULT_NAME("Context", ctx);
    ectx.Handle = Onyx::CreateRenderContext<D>();

    return ctx;
}

template <Dimension D> void EditorScene::DestroyRenderContext(const RenderContext ctx)
{
    TKit::TierHive<EditorContext<D>> &ctxs = GetContexts<D>();

    EditorContext<D> &ectx = ctxs[ctx];
    Registry.Query<RenderContextComponent<D>>().Each([&](RenderContextComponent<D> &comp) {
        if (comp.Context == ectx.Handle)
            comp.Context = nullptr;
    });

    Onyx::DestroyRenderContext(ectx.Handle);
    ctxs.Remove(ctx);
}

void EditorScene::Destroy()
{
    const TKit::StackArray<Viewport> vps = Viewports.GetValidIds();

    for (const Viewport vp : vps)
        DestroyViewport(vp);

    Viewports.Clear();

    TKit::TierAllocator *tier = TKit::GetTier();
    for (const EditorCamera<D2> &cam : Cameras2)
        tier->Destroy(cam.Handle);
    for (const EditorCamera<D3> &cam : Cameras3)
        tier->Destroy(cam.Handle);

    Cameras2.Clear();
    Cameras3.Clear();

    for (const EditorContext<D2> &ctx : Contexts2)
        Onyx::DestroyRenderContext(ctx.Handle);
    for (const EditorContext<D3> &ctx : Contexts3)
        Onyx::DestroyRenderContext(ctx.Handle);

    Contexts2.Clear();
    Contexts3.Clear();
    Registry.Clear();
}

template RenderView EditorScene::CreateRenderView<D2>(EditorViewport &viewport, u32 camIdx);
template RenderView EditorScene::CreateRenderView<D3>(EditorViewport &viewport, u32 camIdx);
template void EditorScene::DestroyRenderView<D2>(RenderView rv, EditorViewport &viewport);
template void EditorScene::DestroyRenderView<D3>(RenderView rv, EditorViewport &viewport);

template EditorCamera<D2> &EditorScene::CreateCamera<D2>();
template EditorCamera<D3> &EditorScene::CreateCamera<D3>();
template void EditorScene::DestroyCamera<D2>(u32 camIdx);
template void EditorScene::DestroyCamera<D3>(u32 camIdx);

template RenderContext EditorScene::CreateRenderContext<D2>();
template RenderContext EditorScene::CreateRenderContext<D3>();
template void EditorScene::DestroyRenderContext<D2>(RenderContext ctx);
template void EditorScene::DestroyRenderContext<D3>(RenderContext ctx);
} // namespace Engine
