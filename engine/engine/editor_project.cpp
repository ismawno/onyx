#include "editor_project.hpp"

namespace Engine
{
Scene EditorProject::CreateScene(const EditorScene &other)
{
    const Scene sc = Scenes.Insert(other);
    EditorScene &scene = Scenes[sc];
    scene.Id = sc;
    scene.Name = other.Name.IsEmpty() ? ONYX_DEFAULT_NAME("Scene", sc) : other.Name;
    scene.Registry.RegisterComponents(AllComponents{});
    return sc;
}

template <Dimension D> Scene EditorProject::CreateDefaultScene()
{
    const Scene sc = CreateScene();
    EditorScene &scene = Scenes[sc];
    const Viewport vp = scene.CreateViewport(ONYX_DEFAULT_RESOLUTION);
    EditorViewport &viewport = scene.Viewports[vp];
    scene.CreateCamera<D>();

    const RenderView rv = scene.CreateRenderView<D>(viewport, 0);
    const RenderContext rc = scene.CreateRenderContext<D>();

    const EditorView<D> &view = viewport.GetViews<D>()[rv];
    view.Handle->ClearColor = Onyx::Color_Sky;
    scene.GetContexts<D>()[rc].Handle->AddTarget(view.Handle);
    return sc;
}

void EditorProject::DestroyScene(const Scene sc)
{
    Scenes[sc].Destroy();
    Scenes.Remove(sc);
}
void EditorProject::Destroy()
{
    const TKit::StackArray<Scene> toRemove = Scenes.GetValidIds();
    for (const Scene sc : toRemove)
        Scene_Destroy(sc);
}

template Scene EditorProject::CreateDefaultScene<D2>();
template Scene EditorProject::CreateDefaultScene<D3>();
} // namespace Engine
