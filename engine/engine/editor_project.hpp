#pragma once

#include "engine.hpp"
#include "editor_scene.hpp"

#define EDITOR_DEFAULT_RESOLUTION u32v2{1920, 1080}

namespace Engine
{
struct EditorProject
{
    TKit::TierString Name;
    TKit::TierPath Path{};
    ProjectSettings Settings{};

    TKit::TierHive<EditorScene> Scenes{};
    Scene ActiveScene = NullScene;

    Scene CreateScene(const EditorScene &scene = {});
    template <Dimension D> Scene CreateDefaultScene();

    void DestroyScene(Scene sc);
    void Destroy();

    ONYX_NO_DISCARD Result<> Serialize(const TKit::TierPath &path);
    ONYX_NO_DISCARD Result<> Serialize()
    {
        return Serialize(Path);
    }
    ONYX_NO_DISCARD static Result<EditorProject> Deserialize(const TKit::TierPath &path);

    EditorScene &GetActiveScene()
    {
        return Scenes[ActiveScene];
    }
    TKit::TierPath GetScenesPath() const
    {
        return Path / "scenes";
    }
    TKit::TierPath GetDataPath() const
    {
        return Path / "data";
    }
    TKit::TierPath GetSettingsPath() const
    {
        return GetDataPath() / "settings.yaml";
    }
    TKit::TierPath GetOverlayLayoutPath() const
    {
        return GetDataPath() / "ui-layout.yaml";
    }
};
} // namespace Engine
