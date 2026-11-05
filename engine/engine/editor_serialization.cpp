#include "editor_scene.hpp"
#include "editor.hpp"
#include "onyx/onyx.hpp"
#include "tkit/preprocessor/utils.hpp"
#include "tkit/serialization/yaml/codec.hpp"
#include "tkit/serialization/yaml/container.hpp"
#include "tkit/serialization/yaml/tensor.hpp"
#include "tkit/serialization/yaml/quaternion.hpp"
#include "tkit/serialization/yaml/include/onyx/camera.hpp"
#include "tkit/serialization/yaml/include/onyx/transform.hpp"
#include "tkit/serialization/yaml/include/onyx/camera.hpp"
#include "tkit/serialization/yaml/include/onyx/view.hpp"
#include "tkit/serialization/yaml/engine/components.hpp"
#include "tkit/serialization/yaml/engine/engine.hpp"

#define ONYX_CHECK_HAS_CHILD(node, child, errtype)                                                                     \
    if (!node.HasChild(child))                                                                                         \
        return Result<errtype>::Error(Error_DeserializationFailed,                                                     \
                                      TKit::TierString::Format("[ONYX][EDITOR] The node '{}' was not "                 \
                                                               "found",                                                \
                                                               child));

// #define ONYX_TRY_READ(varname, node, child, errtype) node[child].Read(varname)
#define ONYX_TRY_READ(varname, node, child, errtype)                                                                   \
    ONYX_CHECK_HAS_CHILD(node, child, errtype)                                                                         \
    const auto TKIT_CONCAT(res, __LINE__) = node[child].TryRead(varname);                                              \
    if (!TKIT_CONCAT(res, __LINE__))                                                                                   \
        return Result<errtype>::Error(Error_DeserializationFailed, TKIT_CONCAT(res, __LINE__).GetError().Message);

#define ONYX_TRY_READ_DECLARE(type, varname, node, child, errtype)                                                     \
    type varname;                                                                                                      \
    ONYX_TRY_READ(varname, node, child, errtype)

namespace Engine
{
template <Dimension D> static void serializeCameras(const EditorScene &scene, const char *name, YamlNode root)
{
    YamlNode cameras = root[name];
    for (const EditorCamera<D> &cam : scene.GetCameras<D>())
    {
        YamlNode node = cameras.Append();
        node["Name"] << cam.Name;
        node["RefCount"] << cam.RefCount;
        node["Camera"] << *cam.Handle;
        node["Scene"] << cam.ParentScene;
    }
}
template <Dimension D> static void serializeContexts(const EditorScene &scene, const char *name, YamlNode root)
{
    YamlNode contexts = root[name];
    const TKit::TierHive<EditorContext<D>> &rcs = scene.GetContexts<D>();
    contexts["Indices"] << rcs.GetIndices();
    contexts["Ids"] << rcs.GetIds();
    for (const EditorContext<D> &rc : scene.GetContexts<D>())
    {
        YamlNode node = contexts["Contexts"].Append();
        node["Name"] << rc.Name;
        node["Draw axes"] << rc.DrawAxes;
        node["Scene"] << rc.ParentScene;

        for (const Viewport vp : scene.Viewports.GetValidIds())
            for (const RenderView rv : scene.Viewports[vp].GetViews<D>().GetValidIds())
                if (rc.Handle->HasTarget(scene.Viewports[vp].GetViews<D>()[rv].Handle))
                {
                    YamlNode target = node["Targets"].Append();
                    target["Viewport"] << vp;
                    target["View"] << rv;
                }
    }
}

template <Dimension D> static void serializeViews(const EditorViewport &viewport, const char *name, YamlNode root)
{
    YamlNode views = root[name];
    const TKit::TierHive<EditorView<D>> &rviews = viewport.GetViews<D>();
    views["Indices"] << rviews.GetIndices();
    views["Ids"] << rviews.GetIds();
    for (const EditorView<D> &rv : rviews)
    {
        YamlNode node = views["Views"].Append();
        node["Name"] << rv.Name;
        node["Viewport"] << rv.ParentViewport;
        node["Viewport data"] << rv.Handle->GetViewport();
        node["Scissor data"] << rv.Handle->GetScissor();
        node["Editor camera"] << rv.EditorCameraIndex;
        node["Clear color"] << rv.Handle->ClearColor;
        node["Flags"] << rv.Handle->GetFlags();
    }
}
template <typename C>
static void serializeComponent(const TKit::Registry &r, const Entity e, const char *name, YamlNode root)
{
    const C *comp = r.GetComponent<C>(e);
    if (comp)
        root[name] << *comp;
}
template <Dimension D>
static void serializeRenderContextComponent(const EditorScene &scene, const TKit::Registry &r, const Entity e,
                                            const char *name, YamlNode root)
{
    const RenderContextComponent<D> *comp = r.GetComponent<RenderContextComponent<D>>(e);
    if (comp)
        root[name] << scene.FindContextInsertionIndex(comp->Context);
}

void EditorScene::Serialize(const fs::path &path) const
{
    const TKit::Registry &r = Registry;

    TKit::YamlTree tree{};
    YamlNode root = tree.GetRoot();
    root["Name"] << Name;

    serializeCameras<D2>(*this, "2D Cameras", root);
    serializeCameras<D3>(*this, "3D Cameras", root);

    serializeContexts<D2>(*this, "2D Contexts", root);
    serializeContexts<D3>(*this, "3D Contexts", root);

    YamlNode viewports = root["Viewports"];
    viewports["Indices"] << Viewports.GetIndices();
    viewports["Ids"] << Viewports.GetIds();
    for (const EditorViewport &vp : Viewports)
    {
        YamlNode node = viewports["Viewports"].Append();
        node["Name"] << vp.Name;
        node["Resolution"] << vp.Resolution;
        node["Aspect"] << vp.Aspect;
        node["Visible"] << vp.Visible;

        serializeViews<D2>(vp, "2D Views", node);
        serializeViews<D3>(vp, "3D Views", node);
    }

    YamlNode entities = root["Entities"];
    entities["Indices"] << r.GetEntityIndices();
    entities["Ids"] << r.GetEntityIds();

    for (const Entity e : r.GetEntities())
    {
        YamlNode node = entities["Entities"].Append();
        serializeComponent<NameComponent>(r, e, "NameComponent", node);

        serializeComponent<TransformComponent<D2>>(r, e, "TransformComponent2D", node);
        serializeComponent<TransformComponent<D3>>(r, e, "TransformComponent3D", node);

        serializeComponent<StaticMeshComponent<D2>>(r, e, "StaticMeshComponent2D", node);
        serializeComponent<StaticMeshComponent<D3>>(r, e, "StaticMeshComponent3D", node);

        serializeRenderContextComponent<D2>(*this, r, e, "RenderContextComponent2D", node);
        serializeRenderContextComponent<D3>(*this, r, e, "RenderContextComponent2D", node);
    }

    tree.ToFile(path);
}

template <Dimension D>
static Result<> deserializeCameras(EditorScene &scene, const char *name, const ConstYamlNode root)
{
    if (!root.HasChild(name))
        return Result<>::Ok();

    const ConstYamlNode cameras = root[name];
    TKit::TierArray<EditorCamera<D>> &cams = scene.GetCameras<D>();
    TKIT_ASSERT(cams.IsEmpty(), "[ONYX][EDITOR] Cameras should be empty");

    TKit::StackArray<Onyx::Camera<D>> data{};
    data.Reserve(cameras.GetChildCount());
    for (const ConstYamlNode node : cameras)
    {
        EditorCamera<D> &cam = cams.Append();
        ONYX_TRY_READ(cam.Name, node, "Name", );
        ONYX_TRY_READ(cam.ParentScene, node, "Scene", );
        ONYX_TRY_READ(cam.RefCount, node, "RefCount", );
        ONYX_TRY_READ(data.Append(), node, "Camera", );
    }

    TKit::TierAllocator *tier = TKit::GetTier();
    for (u32 i = 0; i < cams.GetSize(); ++i)
        cams[i].Handle = tier->Create<Onyx::Camera<D>>(data[i]);

    return Result<>::Ok();
}

template <Dimension D> Result<> deserializeContexts(EditorScene &scene, const char *name, const ConstYamlNode root)
{
    if (!root.HasChild(name) || !root[name].HasChild("Indices"))
        return Result<>::Ok();

    const ConstYamlNode contexts = root[name];
    ONYX_TRY_READ_DECLARE(TKit::StackArray<u32>, indices, contexts, "Indices", EditorData);
    ONYX_TRY_READ_DECLARE(TKit::StackArray<RenderContext>, ids, contexts, "Ids", EditorData);

    TKit::TierHive<EditorContext<D>> ctxs{indices, ids};
    if (contexts.HasChild("Contexts"))
    {
        struct TargetInfo
        {
            RenderContext Context;
            Onyx::ViewMask Mask;
        };
        TKit::StackArray<TargetInfo> tinfos{};
        tinfos.Reserve(contexts["Contexts"].GetChildCount());
        for (const ConstYamlNode node : contexts["Contexts"])
        {
            const RenderContext rc = ctxs.Insert();
            EditorContext<D> &ctx = ctxs[rc];
            ONYX_TRY_READ(ctx.Name, node, "Name", );
            ONYX_TRY_READ(ctx.ParentScene, node, "Scene", );
            ONYX_TRY_READ(ctx.DrawAxes, node, "Draw axes", );

            if (node.HasChild("Targets"))
            {
                TargetInfo &tinfo = tinfos.Append(rc, 0);
                for (const ConstYamlNode target : node["Targets"])
                {
                    ONYX_TRY_READ_DECLARE(Viewport, vp, target, "Viewport", );
                    ONYX_TRY_READ_DECLARE(RenderView, rv, target, "View", );
                    tinfo.Mask |= scene.Viewports[vp].GetViews<D>()[rv].Handle->GetViewBit();
                }
            }
        }
        for (EditorContext<D> &ctx : ctxs)
            ctx.Handle = Onyx::CreateRenderContext<D>();

        for (const TargetInfo &tinfo : tinfos)
            ctxs[tinfo.Context].Handle->AddTarget(tinfo.Mask);
    }
    scene.GetContexts<D>() = ctxs;
    return Result<>::Ok();
}

template <Dimension D>
static Result<> deserializeViews(const EditorScene &scene, EditorViewport &viewport, const char *name,
                                 const ConstYamlNode root)
{
    if (!root.HasChild(name) || !root[name].HasChild("Indices"))
        return Result<>::Ok();

    const ConstYamlNode views = root[name];
    ONYX_TRY_READ_DECLARE(TKit::StackArray<u32>, indices, views, "Indices", EditorData);
    ONYX_TRY_READ_DECLARE(TKit::StackArray<RenderContext>, ids, views, "Ids", EditorData);

    TKit::TierHive<EditorView<D>> rviews{indices, ids};
    if (views.HasChild("Views"))
    {
        struct ViewInfo
        {
            RenderView View;
            Onyx::Viewport Viewport;
            Onyx::Scissor Scissor;
            Onyx::Color Color;
            Onyx::RenderViewFlags Flags;
        };

        TKit::StackArray<ViewInfo> vinfos{};
        vinfos.Reserve(views["Views"].GetChildCount());
        for (const ConstYamlNode node : views["Views"])
        {
            const RenderView rv = rviews.Insert();
            EditorView<D> &rview = rviews[rv];
            ViewInfo &vinfo = vinfos.Append();

            ONYX_TRY_READ(rview.Name, node, "Name", );
            ONYX_TRY_READ(rview.ParentViewport, node, "Viewport", );
            ONYX_TRY_READ(rview.EditorCameraIndex, node, "Editor camera", );
            ONYX_TRY_READ(vinfo.Viewport, node, "Viewport data", );
            ONYX_TRY_READ(vinfo.Scissor, node, "Scissor data", );
            ONYX_TRY_READ(vinfo.Color, node, "Clear color", );
            ONYX_TRY_READ(vinfo.Flags, node, "Flags", );
            vinfo.View = rv;
        }

        for (const ViewInfo &vinfo : vinfos)
        {
            EditorView<D> &view = rviews[vinfo.View];
            view.Handle = viewport.Target->CreateRenderView<D>(scene.GetCameras<D>()[view.EditorCameraIndex].Handle,
                                                               {.Flags = vinfo.Flags});
            view.Handle->SetFixedViewport(vinfo.Viewport);
            view.Handle->SetFixedScissor(vinfo.Scissor);
            view.Handle->ClearColor = vinfo.Color;
        }
    }

    viewport.GetViews<D>() = rviews;
    return Result<>::Ok();
}

template <typename C>
static Result<> deserializeComponent(const char *name, const ConstYamlNode node, const Entity e, TKit::Registry &r)
{
    if (node.HasChild(name))
    {
        ONYX_TRY_READ_DECLARE(C, comp, node, name, );
        r.AddComponent<C>(e, comp);
    }
    return Result<>::Ok();
}
template <Dimension D>
static Result<> deserializeRenderContextComponent(const char *name, const ConstYamlNode node, const EditorScene &scene,
                                                  const Entity e, TKit::Registry &r)
{
    if (node.HasChild(name))
    {
        ONYX_TRY_READ_DECLARE(u32, idx, node, name, );
        r.AddComponent<RenderContextComponent<D>>(e, idx != TKIT_U32_MAX ? scene.GetContexts<D>().AtByIndex(idx).Handle
                                                                         : nullptr);
    }
    return Result<>::Ok();
}

Result<EditorScene> EditorScene::Deserialize(const fs::path &path)
{
    ONYX_GENERATE_ERROR_FILE_NOT_FOUND(path, EditorScene, "[ONYX][EDITOR] File {} not found");
    const auto res = YamlTree::FromFile(path);
    if (!res)
        return Result<>::Error(Error_DeserializationFailed, res.GetError());

    const YamlTree &tree = *res;
    const ConstYamlNode root = tree.GetRoot();

    EditorScene scene{};
    TKit::Registry &r = scene.Registry;

    TKIT_RETURN_IF_FAILED(deserializeCameras<D2>(scene, "2D Cameras", root));
    TKIT_RETURN_IF_FAILED(deserializeCameras<D3>(scene, "3D Cameras", root));

    const ConstYamlNode viewports = root["Viewports"];

    const TKit::StackArray<u32> indices = viewports["Indices"].Read<TKit::StackArray<u32>>();
    const TKit::StackArray<Viewport> ids = viewports["Ids"].Read<TKit::StackArray<Viewport>>();

    TKit::TierHive<EditorViewport> vps{indices, ids};
    if (viewports.HasChild("Viewports"))
        for (const ConstYamlNode node : viewports["Viewports"])
        {
            EditorViewport &vp = vps[vps.Insert()];
            vp.Name = node["Name"].Read<TKit::TierString>();
            vp.Resolution = node["Resolution"].Read<u32v2>();
            vp.Aspect = node["Aspect"].Read<f32>();
            vp.Visible = node["Visible"].Read<bool>();
            vp.Target = Onyx::CreateRenderTexture(vp.Resolution);

            deserializeViews<D2>(scene, vp, "2D Views", node);
            deserializeViews<D3>(scene, vp, "3D Views", node);
        }

    scene.Viewports = vps;

    TKIT_RETURN_IF_FAILED(deserializeContexts<D2>(scene, "2D Contexts", root));
    TKIT_RETURN_IF_FAILED(deserializeContexts<D3>(scene, "3D Contexts", root));

    if (root.HasChild("Entities") && root["Entities"].HasChild("Indices"))
    {
        const ConstYamlNode entities = root["Entities"];
        const TKit::StackArray<u32> entIdxs = entities["Indices"].Read<TKit::StackArray<u32>>();
        const TKit::StackArray<RenderContext> entIds = entities["Ids"].Read<TKit::StackArray<RenderContext>>();
        r.EstablishEntityIndicesAndIds(entIdxs, entIds);
        for (const ConstYamlNode entity : entities["Entities"])
        {
            const Entity e = r.CreateEntity();
            TKIT_RETURN_IF_FAILED(deserializeComponent<NameComponent>("NameComponent", entity, e, r));

            TKIT_RETURN_IF_FAILED(deserializeComponent<TransformComponent<D2>>("TransformComponent2D", entity, e, r));
            TKIT_RETURN_IF_FAILED(deserializeComponent<TransformComponent<D3>>("TransformComponent3D", entity, e, r));

            TKIT_RETURN_IF_FAILED(deserializeComponent<StaticMeshComponent<D2>>("StaticMeshComponent2D", entity, e, r));
            TKIT_RETURN_IF_FAILED(deserializeComponent<StaticMeshComponent<D3>>("StaticMeshComponent3D", entity, e, r));

            TKIT_RETURN_IF_FAILED(
                deserializeRenderContextComponent<D2>("RenderContextComponent2D", entity, scene, e, r));
            TKIT_RETURN_IF_FAILED(
                deserializeRenderContextComponent<D3>("RenderContextComponent3D", entity, scene, e, r));
        }
    }

    scene.Viewports = vps;
    scene.Name = root["Name"].Read<TKit::TierString>();
    return scene;
}

Result<> serializeEditor(const EditorData &editor, const fs::path &ppath)
{
    TKIT_LOG_INFO("[ONYX][EDITOR] Serializing project at '{}'", ppath.string());

    const fs::path scenePath = editor.GetProjectScenesPath();

    fs::create_directories(ppath);
    fs::create_directories(scenePath);
    fs::create_directories(editor.GetProjectDataPath());

    for (const Scene sc : editor.Scenes.GetValidIds())
        Scene_Serialize(sc, scenePath / TKit::StackString::Format("{}.yaml", editor.Scenes[sc].Name).CString());

    if (editor.Overlay)
        editor.Overlay->Serialize(editor.GetOverlayLayoutPath());

    YamlTree tree{};
    YamlNode root = tree.GetRoot();

    YamlNode scenes = root["Scenes"];
    scenes["Indices"] << editor.Scenes.GetIndices();
    scenes["Ids"] << editor.Scenes.GetIds();
    scenes["Active"] << editor.ActiveScene;

    root["Settings"] << editor.Settings;

    tree.ToFile(editor.GetProjectSettingsPath());
    return Result<>::Ok();
}

Result<EditorData> deserializeEditor(const fs::path &ppath)
{
    ONYX_GENERATE_ERROR_DIRECTORY_NOT_FOUND(ppath, , "[ONYX][EDITOR] The directory {} was not found");
    EditorData editor{};
    editor.ProjectName = ppath.filename().string();
    editor.ProjectPath = ppath;

    TKIT_LOG_INFO("[ONYX][EDITOR] Deserializing project at {}", ppath.string());

    const fs::path scenePath = editor.GetProjectScenesPath();

    const auto res = YamlTree::FromFile(editor.GetProjectSettingsPath());
    if (!res)
        return Result<>::Error(Error_DeserializationFailed, res.GetError());

    const YamlTree &tree = *res;
    const ConstYamlNode root = tree.GetRoot();

    ONYX_CHECK_HAS_CHILD(root, "Scenes", EditorData);

    const ConstYamlNode scenes = root["Scenes"];

    ONYX_TRY_READ_DECLARE(TKit::StackArray<u32>, indices, scenes, "Indices", EditorData);
    ONYX_TRY_READ_DECLARE(TKit::StackArray<RenderContext>, ids, scenes, "Ids", EditorData);

    editor.Scenes = TKit::TierHive<EditorScene>{indices, ids};

    ONYX_TRY_READ(editor.ActiveScene, scenes, "Active", EditorData);
    ONYX_TRY_READ(editor.Settings, root, "Settings", EditorData);

    for (const fs::directory_entry &dir : fs::directory_iterator(scenePath))
        if (!dir.is_directory())
        {
            const auto res = EditorScene::Deserialize(dir.path());
            TKIT_RETURN_ON_ERROR(res);
            editor.CreateScene(*res);
        }

    return editor;
}

} // namespace Engine
