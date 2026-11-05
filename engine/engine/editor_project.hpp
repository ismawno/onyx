#pragma once

#include "engine.hpp"
#include "editor_scene.hpp"

#define ONYX_DEFAULT_RESOLUTION u32v2{1920, 1080}

namespace Engine
{
struct EditorProject
{
    TKit::TierString Name;
    fs::path Path{};
    ProjectSettings Settings{};

    TKit::TierHive<EditorScene> Scenes{};
    Scene ActiveScene = NullScene;

    Scene CreateScene(const EditorScene &scene = {});
    template <Dimension D> Scene CreateDefaultScene();

    void DestroyScene(Scene sc);
    void Destroy();

    void Serialize(const fs::path &path);
    void Serialize()
    {
        Serialize(Path);
    }
    static Result<EditorProject> Deserialize(const fs::path &path);

    EditorScene &GetActiveScene()
    {
        return Scenes[ActiveScene];
    }
    fs::path GetScenesPath() const
    {
        return Path / "scenes";
    }
    fs::path GetDataPath() const
    {
        return Path / "data";
    }
    fs::path GetSettingsPath() const
    {
        return GetDataPath() / "settings.yaml";
    }
    fs::path GetOverlayLayoutPath() const
    {
        return GetDataPath() / "ui-layout.yaml";
    }
};
} // namespace Engine
