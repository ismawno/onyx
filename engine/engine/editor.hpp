#pragma once

#include "engine.hpp"
#include "editor_scene.hpp"

namespace Engine
{
struct EditorData
{
    Onyx::Window *Window = nullptr;
    Onyx::Overlay *Overlay = nullptr;

    TKit::TierString ProjectName;
    fs::path ProjectPath{};
    ProjectSettings Settings{};

    TKit::TierHive<EditorScene> Scenes{};
    Scene ActiveScene = NullScene;

    Scene CreateScene(const EditorScene &scene = {});
    void DestroyScene(Scene sc);

    void DestroyAllScenes()
    {
        const TKit::StackArray<Scene> toRemove = Scenes.GetValidIds();
        for (const Scene sc : toRemove)
            Scene_Destroy(sc);
    }

    EditorScene &GetActiveScene()
    {
        return Scenes[ActiveScene];
    }
    fs::path GetProjectScenesPath() const
    {
        return ProjectPath / "scenes";
    }
    fs::path GetProjectDataPath() const
    {
        return ProjectPath / "data";
    }
    fs::path GetProjectSettingsPath() const
    {
        return GetProjectDataPath() / "settings.yaml";
    }
    fs::path GetOverlayLayoutPath() const
    {
        return GetProjectDataPath() / "ui-layout.yaml";
    }
};
} // namespace Engine
