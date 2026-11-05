#pragma once

#include "alias.hpp"
#include "components.hpp"
#include "onyx/render_texture.hpp"
#include "onyx/overlay.hpp"
#include "tkit/container/ecs.hpp"

#define ONYX_DIMENSION_GETTER(varname)                                                                                 \
    if constexpr (D == D2)                                                                                             \
        return varname##2;                                                                                             \
    else                                                                                                               \
        return varname##3

#define ONYX_DEFAULT_NAME(prefix, idx) TKit::TierString::Format(prefix " {}", idx)

namespace Engine
{
template <Dimension D> struct EditorCamera
{
    TKIT_YAML_SERIALIZE_DECLARE(EditorCamera)
    Scene ParentScene;
    TKit::TierString Name{};
    Onyx::Camera<D> *Handle;
    // lol
    u32 RefCount = 0;
};

template <Dimension D> struct EditorView
{
    RenderView Id;
    Viewport ParentViewport;

    TKit::TierString Name{};
    Onyx::RenderView<D> *Handle;
    // Note(Isma): Need to store here as well what entity's camera will take over the view when play is hit
    u32 EditorCameraIndex = TKIT_U32_MAX;
};

template <Dimension D> struct EditorContext
{
    RenderContext Id;
    Scene ParentScene;
    TKit::TierString Name{};
    Onyx::RenderContext<D> *Handle;
    bool DrawAxes = false;
};

struct EditorViewport
{
    Viewport Id;
    Scene ParentScene;
    TKit::TierString Name{};
    const Onyx::OverlayWindow *Window = nullptr;
    Onyx::RenderTexture *Target = nullptr;

    TKit::TierHive<EditorView<D2>> Views2{};
    TKit::TierHive<EditorView<D3>> Views3{};

    f32v2 Position{0.f};
    f32v2 Size{0.f};
    u32v2 Resolution{0};
    f32 Aspect = 0.f;
    bool Visible = true;

    template <Dimension D> const TKit::TierHive<EditorView<D>> &GetViews() const
    {
        ONYX_DIMENSION_GETTER(Views);
    }
    template <Dimension D> TKit::TierHive<EditorView<D>> &GetViews()
    {
        ONYX_DIMENSION_GETTER(Views);
    }
};

struct EditorScene
{
    Scene Id;
    TKit::TierString Name{};
    TKit::Registry Registry{};

    Entity SelectedEntity = TKit::NullEntity;

    // TODO(Isma): If viewports are created/destroyed on play, we need to be able to restore that!
    TKit::TierHive<EditorViewport> Viewports{};

    TKit::TierArray<EditorCamera<D2>> Cameras2{};
    TKit::TierArray<EditorCamera<D3>> Cameras3{};

    TKit::TierHive<EditorContext<D2>> Contexts2{};
    TKit::TierHive<EditorContext<D3>> Contexts3{};

    u32 Dim = D2;

    Viewport CreateViewport(const u32v2 &resolution);
    void DestroyViewport(Viewport vp);

    template <Dimension D> RenderView CreateRenderView(EditorViewport &viewport, u32 camIdx);
    template <Dimension D> void DestroyRenderView(RenderView rv, EditorViewport &viewport);

    template <Dimension D> EditorCamera<D> &CreateCamera();
    template <Dimension D> void DestroyCamera(const u32 camIdx);

    template <Dimension D> RenderContext CreateRenderContext();
    template <Dimension D> void DestroyRenderContext(RenderContext ctx);

    void Destroy();

    void Serialize(const fs::path &path) const;
    static Result<EditorScene> Deserialize(const fs::path &path);

    template <Dimension D> void FlushContexts()
    {
        for (const EditorContext<D> &ctx : GetContexts<D>())
            ctx.Handle->Flush();
    }
    void FlushContexts()
    {
        FlushContexts<D2>();
        FlushContexts<D3>();
    }

    template <Dimension D> u32 FindContextInsertionIndex(const Onyx::RenderContext<D> *ctx) const
    {
        const TKit::TierHive<EditorContext<D>> &ctxs = GetContexts<D>();
        u32 idx = 0;
        const bool found = ctxs.IterateByInsertionOrder([&](const EditorContext<D> &rc) {
            if (rc.Handle == ctx)
                return false;
            ++idx;
            return true;
        });
        return found ? idx : TKIT_U32_MAX;
    }

    template <Dimension D> bool HasDimension() const
    {
        return Dim == 0 || Dim == D;
    }
    template <Dimension D> bool IsDimension() const
    {
        return Dim == D;
    }
    bool HasBothDimensions() const
    {
        return Dim == 0;
    }
    template <Dimension D> const TKit::TierArray<EditorCamera<D>> &GetCameras() const
    {
        ONYX_DIMENSION_GETTER(Cameras);
    }
    template <Dimension D> TKit::TierArray<EditorCamera<D>> &GetCameras()
    {
        ONYX_DIMENSION_GETTER(Cameras);
    }
    template <Dimension D> const TKit::TierHive<EditorContext<D>> &GetContexts() const
    {
        ONYX_DIMENSION_GETTER(Contexts);
    }
    template <Dimension D> TKit::TierHive<EditorContext<D>> &GetContexts()
    {
        ONYX_DIMENSION_GETTER(Contexts);
    }
};
} // namespace Engine
