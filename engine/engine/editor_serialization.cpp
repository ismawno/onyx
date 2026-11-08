#include "editor_scene.hpp"
#include "editor_project.hpp"
#include "utils.hpp"
#include "onyx/onyx.hpp"
#include "tkit/preprocessor/utils.hpp"
#include "tkit/serialization/yaml/codec.hpp"
#include "tkit/serialization/yaml/container.hpp"
#include "tkit/serialization/yaml/tensor.hpp"
#include "tkit/serialization/yaml/quaternion.hpp"
#include "tkit/serialization/yaml/include/onyx/transform.hpp"
#include "tkit/serialization/yaml/include/onyx/camera.hpp"
#include "tkit/serialization/yaml/include/onyx/view.hpp"
#include "tkit/serialization/yaml/engine/components.hpp"
#include "tkit/serialization/yaml/engine/engine.hpp"

#define ONYX_CHECK_HAS_CHILD(node, child)                                                                              \
    ONYX_EMIT_ERROR_IF(!node.HasChild(child), Onyx::Error_DeserializationFailed,                                       \
                       "[ONYX][EDITOR] Failed to load scene: '{}' is missing required key '{}'",                       \
                       TKit::YamlGeneratePath(node), child)

// #define ONYX_TRY_READ(varname, node, child, errtype) node[child].Read(varname)
#define ONYX_TRY_READ(varname, node, child) TKIT_RETURN_IF_FAILED(readYamlChild(node, child, varname, #varname))

#define ONYX_TRY_READ_DECLARE(type, varname, node, child)                                                              \
    type varname;                                                                                                      \
    ONYX_TRY_READ(varname, node, child)

#define ONYX_TRY_READ_INDICES_IDS(indName, idName, node)                                                               \
    ONYX_TRY_READ_DECLARE(TKit::StackArray<u32>, indName, node, "Indices");                                            \
    ONYX_TRY_READ_DECLARE(TKit::StackArray<u32>, idName, node, "Ids");                                                 \
    TKIT_RETURN_IF_FAILED(checkHiveLayout(indName, idName, node));

namespace Engine
{
template <typename T>
ONYX_NO_DISCARD static Result<> readYamlChild(const TKit::ConstYamlNode node, const char *key, T &out, const char *what)
{
    ONYX_CHECK_HAS_CHILD(node, key);
    const TKit::ConstYamlNode child = node[key];
    const auto res = child.TryRead(out);
    ONYX_EMIT_ERROR_IF(!res, Error_DeserializationFailed,
                       "[ONYX][EDITOR] Failed to load scene: Could not read '{}' (into {}): got {} - {}",
                       TKit::YamlGeneratePath(child), what, TKit::YamlDescribeNode(child), res.GetError().Message);
    return Result<>::Ok();
}
template <typename Id>
ONYX_NO_DISCARD static Result<> checkHiveLayout(const TKit::StackArray<u32> &indices, const TKit::StackArray<Id> &ids,
                                                const ConstYamlNode node)
{
    if (indices.GetSize() != ids.GetSize())
        return Result<>::Error(Error_DeserializationFailed,
                               TKit::TierString::Format("'{}': 'Indices' has {} entries but 'Ids' has {}",
                                                        TKit::YamlGeneratePath(node), indices.GetSize(),
                                                        ids.GetSize()));
    for (u32 id = 0; id < indices.GetSize(); ++id)
        if (indices[id] >= ids.GetSize() || u32(ids[indices[id]]) != id)
            return Result<>::Error(
                Error_DeserializationFailed,
                TKit::TierString::Format("'{}': 'Indices' and 'Ids' are inconsistent at id {} (file corrupted or "
                                         "hand-edited?)",
                                         TKit::YamlGeneratePath(node), id));
    return Result<>::Ok();
}
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

Result<> EditorScene::Serialize(const TKit::TierPath &path) const
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
        serializeRenderContextComponent<D3>(*this, r, e, "RenderContextComponent3D", node);
    }

    ONYX_CHECK_FILESYSTEM_ERROR(
        tree.ToFile(path),
        "[ONYX][EDITOR] Failed to save scene: Could not export the serialized scene to '{}' with error: {}", path,
        TKit::ToString(res.GetError()));
    return Result<>::Ok();
}

template <Dimension D>
ONYX_NO_DISCARD static Result<> deserializeCameras(EditorScene &scene, const char *name, const ConstYamlNode root)
{
    if (!root.HasChild(name))
        return Result<>::Ok();

    const ConstYamlNode cameras = root[name];
    TKit::TierArray<EditorCamera<D>> &cams = scene.GetCameras<D>();
    TKIT_ASSERT(cams.IsEmpty(), "[ONYX][EDITOR] Cameras should be empty");

    TKit::StackArray<Onyx::Camera<D>> data{};
    data.Reserve(cameras.GetChildCount());

    TKit::TierAllocator *tier = TKit::GetTier();
    for (const ConstYamlNode node : cameras)
    {
        EditorCamera<D> &cam = cams.Append();
        cam.Handle = tier->Create<Onyx::Camera<D>>();
        ONYX_TRY_READ(cam.Name, node, "Name");
        ONYX_TRY_READ(cam.ParentScene, node, "Scene");
        ONYX_TRY_READ(cam.RefCount, node, "RefCount");
        ONYX_TRY_READ(*cam.Handle, node, "Camera");
    }

    return Result<>::Ok();
}

template <Dimension D> Result<> deserializeContexts(EditorScene &scene, const char *name, const ConstYamlNode root)
{
    if (!root.HasChild(name) || !root[name].HasChild("Indices"))
        return Result<>::Ok();

    const ConstYamlNode contexts = root[name];
    ONYX_TRY_READ_INDICES_IDS(indices, ids, contexts);

    TKit::TierHive<EditorContext<D>> ctxs{indices, ids};
    if (contexts.HasChild("Contexts"))
        for (const ConstYamlNode node : contexts["Contexts"])
        {
            EditorContext<D> &ctx = ctxs[ctxs.Insert()];
            ctx.Handle = Onyx::CreateRenderContext<D>();
            ONYX_TRY_READ(ctx.Name, node, "Name");
            ONYX_TRY_READ(ctx.ParentScene, node, "Scene");
            ONYX_TRY_READ(ctx.DrawAxes, node, "Draw axes");

            if (node.HasChild("Targets"))
                for (const ConstYamlNode target : node["Targets"])
                {
                    ONYX_TRY_READ_DECLARE(Viewport, vp, target, "Viewport");
                    ONYX_TRY_READ_DECLARE(RenderView, rv, target, "View");
                    const EditorViewport *viewport = scene.Viewports.Get(vp);
                    if (!viewport)
                        return Result<>::Error(
                            Error_DeserializationFailed,
                            TKit::TierString::Format("[ONYX][EDITOR] Failed to load scene: '{}' targets viewport id "
                                                     "{}, which does not exist in this scene",
                                                     TKit::YamlGeneratePath(target), vp));

                    const EditorView<D> *view = viewport->GetViews<D>().Get(rv);
                    if (!view)
                        return Result<>::Error(
                            Error_DeserializationFailed,
                            TKit::TierString::Format("[ONYX][EDITOR] Failed to load scene: '{}' targets {}D view id {} "
                                                     "in viewport '{}', but that viewport has no such view",
                                                     TKit::YamlGeneratePath(target), u32(D), rv, viewport->Name));
                    ctx.Handle->AddTarget(view->Handle);
                }
        }

    scene.GetContexts<D>() = ctxs;
    return Result<>::Ok();
}

template <Dimension D>
ONYX_NO_DISCARD static Result<> deserializeViews(const EditorScene &scene, EditorViewport &viewport, const char *name,
                                                 const ConstYamlNode root)
{
    if (!root.HasChild(name) || !root[name].HasChild("Indices"))
        return Result<>::Ok();

    const ConstYamlNode views = root[name];
    ONYX_TRY_READ_INDICES_IDS(indices, ids, views);

    TKit::TierHive<EditorView<D>> rviews{indices, ids};
    if (views.HasChild("Views"))
        for (const ConstYamlNode node : views["Views"])
        {
            ONYX_TRY_READ_DECLARE(u32, camIdx, node, "Editor camera");
            ONYX_TRY_READ_DECLARE(Onyx::RenderViewFlags, flags, node, "Flags");
            ONYX_TRY_READ_DECLARE(Onyx::Viewport, vp, node, "Viewport data");
            ONYX_TRY_READ_DECLARE(Onyx::Scissor, sc, node, "Scissor data");
            ONYX_TRY_READ_DECLARE(TKit::TierString, name, node, "Name");
            if (camIdx >= scene.GetCameras<D>().GetSize())
                return Result<>::Error(
                    Error_DeserializationFailed,
                    TKit::TierString::Format("[ONYX][EDITOR] Failed to load scene: View '{}' at '{}' uses {}D camera "
                                             "index {}, but the scene only has {} {}D cameras",
                                             name, TKit::YamlGeneratePath(node), u32(D), camIdx,
                                             scene.GetCameras<D>().GetSize(), u32(D)));

            const RenderView rv = rviews.Insert();
            EditorView<D> &rview = rviews[rv];

            rview.Handle = viewport.Target->CreateRenderView<D>(scene.GetCameras<D>()[camIdx].Handle, {.Flags = flags});
            rview.Handle->SetFixedViewport(vp);
            rview.Handle->SetFixedScissor(sc);
            rview.Name = name;
            rview.EditorCameraIndex = camIdx;

            ONYX_TRY_READ(rview.Handle->ClearColor, node, "Clear color");
            ONYX_TRY_READ(rview.ParentViewport, node, "Viewport");
        }

    viewport.GetViews<D>() = rviews;
    return Result<>::Ok();
}

template <typename C>
ONYX_NO_DISCARD static Result<> deserializeComponent(const char *name, const ConstYamlNode node, const Entity e,
                                                     TKit::Registry &r)
{
    if (node.HasChild(name))
    {
        ONYX_TRY_READ_DECLARE(C, comp, node, name);
        r.AddComponent<C>(e, comp);
    }
    return Result<>::Ok();
}
template <Dimension D>
ONYX_NO_DISCARD static Result<> deserializeRenderContextComponent(const char *name, const ConstYamlNode node,
                                                                  const EditorScene &scene, const Entity e,
                                                                  TKit::Registry &r)
{
    if (node.HasChild(name))
    {
        ONYX_TRY_READ_DECLARE(u32, idx, node, name);
        r.AddComponent<RenderContextComponent<D>>(e, idx != TKIT_U32_MAX ? scene.GetContexts<D>().AtByIndex(idx).Handle
                                                                         : nullptr);
    }
    return Result<>::Ok();
}

Result<EditorScene> EditorScene::Deserialize(const TKit::TierPath &path)
{
    ONYX_GENERATE_ERROR_FILE_NOT_FOUND(path, "[ONYX][EDITOR] Scene file '{}' does not exist",
                                       "[ONYX][EDITOR] Expected a scene file at '{}', but found a directory");

    const auto res = YamlTree::FromFile(path);
    ONYX_EMIT_ERROR_IF(!res, Error_DeserializationFailed,
                       "[ONYX][EDITOR] Failed to load scene: Could not deserialize scene at '{}' with error: {}", path,
                       TKit::ToString(res.GetError()));

    const YamlTree &tree = *res;
    const ConstYamlNode root = tree.GetRoot();

    EditorScene scene{};
    TKit::Registry &r = scene.Registry;

    TKIT_RETURN_IF_FAILED(deserializeCameras<D2>(scene, "2D Cameras", root));
    TKIT_RETURN_IF_FAILED(deserializeCameras<D3>(scene, "3D Cameras", root));

    ONYX_CHECK_HAS_CHILD(root, "Viewports");
    const ConstYamlNode viewports = root["Viewports"];

    ONYX_TRY_READ_INDICES_IDS(indices, ids, viewports);

    TKit::TierHive<EditorViewport> vps{indices, ids};
    if (viewports.HasChild("Viewports"))
        for (const ConstYamlNode node : viewports["Viewports"])
        {
            EditorViewport &vp = vps[vps.Insert()];
            ONYX_TRY_READ(vp.Name, node, "Name");
            ONYX_TRY_READ(vp.Resolution, node, "Resolution");
            ONYX_TRY_READ(vp.Aspect, node, "Aspect");
            ONYX_TRY_READ(vp.Visible, node, "Visible");
            vp.Target = Onyx::CreateRenderTexture(vp.Resolution);

            TKIT_RETURN_IF_FAILED(deserializeViews<D2>(scene, vp, "2D Views", node));
            TKIT_RETURN_IF_FAILED(deserializeViews<D3>(scene, vp, "3D Views", node));
        }

    scene.Viewports = vps;

    TKIT_RETURN_IF_FAILED(deserializeContexts<D2>(scene, "2D Contexts", root));
    TKIT_RETURN_IF_FAILED(deserializeContexts<D3>(scene, "3D Contexts", root));

    if (root.HasChild("Entities") && root["Entities"].HasChild("Indices"))
    {
        ONYX_CHECK_HAS_CHILD(root, "Entities");
        const ConstYamlNode entities = root["Entities"];

        ONYX_TRY_READ_INDICES_IDS(entIdxs, entIds, entities);

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

Result<> EditorProject::Serialize(const TKit::TierPath &ppath)
{
    TKIT_LOG_INFO("[ONYX][EDITOR] Serializing project at '{}'", ppath);

    const TKit::TierPath scenePath = GetScenesPath();
    const TKit::TierPath dataPath = GetDataPath();
    const TKit::TierPath settingsPath = GetSettingsPath();

    ONYX_CHECK_FILESYSTEM_ERROR(
        TKit::CreateDirectories(scenePath),
        "[ONYX][EDITOR] Failed to save scene: Could not to create directories at '{}' with error: {}", scenePath,
        TKit::ToString(res.GetError()));
    ONYX_CHECK_FILESYSTEM_ERROR(
        TKit::CreateDirectories(dataPath),
        "[ONYX][EDITOR] Failed to save scene: Could not to create directories at '{}' with error: {}", dataPath,
        TKit::ToString(res.GetError()));

    for (const EditorScene &scene : Scenes)
    {
        TKIT_RETURN_IF_FAILED(scene.Serialize(scenePath / TKit::StackString::Format("{}.yaml", scene.Name)));
    }

    YamlTree tree{};
    YamlNode root = tree.GetRoot();

    YamlNode scenes = root["Scenes"];
    scenes["Indices"] << Scenes.GetIndices();
    scenes["Ids"] << Scenes.GetIds();
    scenes["Active"] << ActiveScene;

    root["Settings"] << Settings;

    ONYX_CHECK_FILESYSTEM_ERROR(
        tree.ToFile(settingsPath),
        "[ONYX][EDITOR] Failed to save scene: Could not export the serialized settings to '{}' with error: {}",
        settingsPath, TKit::ToString(res.GetError()));

    return Result<>::Ok();
}

Result<EditorProject> EditorProject::Deserialize(const TKit::TierPath &ppath)
{
    ONYX_GENERATE_ERROR_DIRECTORY_NOT_FOUND(
        ppath, "[ONYX][EDITOR] Failed to load project: The project directory '{}' does not exist",
        "[ONYX][EDITOR] Failed to load project: Expected a project directory at '{}', but found a file");

    EditorProject project{};
    project.Name = ppath.GetFilename().GetString();
    project.Path = ppath;

    const TKit::TierPath scenePath = project.GetScenesPath();
    const TKit::TierPath settingsPath = project.GetSettingsPath();

    ONYX_EMIT_ERROR_IF(
        !TKit::PathExists(scenePath) || !TKit::IsDirectory(scenePath), Error_DeserializationFailed,
        "[ONYX][EDITOR] Failed to load project: '{}' does not look like an Onyx project. Missing directory at '{}{}'",
        ppath, ppath, scenePath);

    ONYX_EMIT_ERROR_IF(
        !TKit::PathExists(settingsPath) || TKit::IsDirectory(settingsPath), Error_DeserializationFailed,
        "[ONYX][EDITOR] Failed to load project: '{}' does not look like an Onyx project. Missing file at '{}{}'", ppath,
        ppath, settingsPath);

    struct MopMop
    {
        MopMop(EditorProject &p) : Project(p)
        {
        }
        ~MopMop()
        {
            if (DoMop)
                Project.Destroy();
        }

        EditorProject &Project;
        bool DoMop = true;
    };
    MopMop mopmop{project};

    TKIT_LOG_INFO("[ONYX][EDITOR] Deserializing project at {}", ppath);

    const auto res = YamlTree::FromFile(settingsPath);
    ONYX_EMIT_ERROR_IF(!res, Error_DeserializationFailed,
                       "[ONYX][EDITOR] Failed to load project: Could not deserialize settings at '{}' with error: {}",
                       settingsPath, TKit::ToString(res.GetError()));

    const YamlTree &tree = *res;
    const ConstYamlNode root = tree.GetRoot();

    ONYX_CHECK_HAS_CHILD(root, "Scenes");

    const ConstYamlNode scenes = root["Scenes"];

    ONYX_TRY_READ_INDICES_IDS(indices, ids, scenes);

    project.Scenes = TKit::TierHive<EditorScene>{indices, ids};

    ONYX_TRY_READ(project.ActiveScene, scenes, "Active");
    ONYX_TRY_READ(project.Settings, root, "Settings");

    auto itRes = TKit::IterateDirectory(scenePath);
    ONYX_EMIT_ERROR_IF(!itRes, Error_DeserializationFailed,
                       "[ONYX][EDITOR] Failed to load project: Could not iterate scene folder at '{}' with error: {}",
                       scenePath, TKit::ToString(itRes.GetError()));

    for (const auto &scene : *itRes)
        if (!scene.IsDirectory())
        {
            const auto res = EditorScene::Deserialize(scene);
            TKIT_RETURN_ON_ERROR(res);
            project.CreateScene(*res);
        }

    mopmop.DoMop = false;
    return project;
}

} // namespace Engine
