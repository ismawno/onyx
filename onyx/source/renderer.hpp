#pragma once

#include "onyx/core.hpp"
#include "onyx/window.hpp"
#include "onyx/render_texture.hpp"
#ifdef ONYX_ENABLE_HOOKS
#    include "onyx/hook.hpp"
#endif
#include "onyx/onyx.hpp"
#include "graph/descriptor.hpp"

namespace Onyx
{
template <Dimension D> class RenderContext;
struct RenderContextSpecs;

struct RenderSubmitInfo
{
    Graph::CommandBuffer Command = Graph::NullHandle;
    Graph::Swapchain Swapchain = Graph::NullHandle;
    u64 InFlightValue = 0;
};

using RenderFlags = u8;
enum RenderFlagBit : RenderFlags
{
#ifdef ONYX_ENABLE_IMGUI
    RenderFlag_ImGui = 1U << 0,
#endif
};

void Renderer_Initialize();
void Renderer_Terminate();

template <Dimension D> RenderContext<D> *Renderer_CreateContext(const RenderContextSpecs &specs);
template <Dimension D> void Renderer_DestroyContext(const RenderContext<D> *context);
void Renderer_FlushAllContexts();

// TODO(Isma): Remove this. will not be necessary, onyx.hpp handles it
void Renderer_AddTarget(const ViewMask vmask);
void Renderer_RemoveTarget(const ViewMask vmask);

// TODO(Isma): Add a bit more clearance on what these do. They essentially write into the renderer's descriptor sets
template <Dimension D>
void Renderer_WriteBufferDescriptor(u32 binding, const Graph::DescriptorBufferInfo &info, RenderPass pass,
                                    u32 dstElement = 0);
template <Dimension D>
void Renderer_WriteImageDescriptor(u32 binding, const Graph::DescriptorImageInfo &info, RenderPass pass,
                                   u32 dstElement = 0);
template <Dimension D>
void Renderer_WriteSamplerDescriptor(u32 binding, Graph::Sampler sampler, RenderPass pass, u32 dstElement = 0);

Graph::Sampler Renderer_GetNearSampler();
Graph::Sampler Renderer_GetLinearSampler();
Graph::Sampler Renderer_GetEdgeSampler();

#ifdef GRAPH_HAS_SHADER_COMPILATION_BACKEND
#    ifdef ONYX_ENABLE_HOOKS
struct HookGlobalData
{
    const void *Data;
    u32 Size;
};
struct HookCompileOptions
{
    TKit::StringView Source{};
    // must be null terminated!!
    TKit::StringView SearchPath{};
    //
    TKit::Span<const HookVarying> Varyings{};

    ShaderCombination *VertexOrOpaque = nullptr;
    ShaderCombination *Transparent = nullptr;
    HookStage Stage = HookStage_Count;

    RenderState HookBitField = 0;
};

void Renderer_RemoveStaleHookPipelines(const ShaderCompileOptions &opts, RenderState hookBitField, HookStage stage);

#    endif
struct RendererShaderCompileOptions
{
    // Standalone is ignored if HookOptions is not null
    ShaderCompileOptions Options{};

#    ifdef ONYX_ENABLE_HOOKS
    // if hook options are null, compilation will be done for the default shaders with no hooks, replacing old default
    // shaders and invalidating all pipelines using the specified shader combination. if hook options are present,
    // compilation will be done against current default shaders including the hook, and only pipelines using such hooks
    // will be replaced
    const HookCompileOptions *HookOptions = nullptr;
#    endif
    bool RemoveStalePipelines = true;
};
ONYX_NO_DISCARD Result<> Renderer_CompileShaders(const RendererShaderCompileOptions &opts);
#endif

// consider having arrays of semaphores to allow for some flexibility

// NOTE(Isma): Same family index multi queue support was dropped. passing queues around is in theory no longer needed
// anymore
struct TransferInfo
{
#ifdef ONYX_ENABLE_HOOKS
    HookGlobalData HookGlobalData2{};
    HookGlobalData HookGlobalData3{};

    template <Dimension D> const HookGlobalData &GetHookGlobalData() const
    {
        if constexpr (D == D2)
            return HookGlobalData2;
        else
            return HookGlobalData3;
    }
#endif
};
void Renderer_Transfer(const TransferInfo &info);

// must be immediately called before rendering all windows (not for all windows, but before rendering any window)
RenderSubmitInfo Renderer_Render(Graph::CommandBuffer command, Window *window, RenderFlags flags = 0);
RenderSubmitInfo Renderer_Render(Graph::CommandBuffer command, RenderTexture *rtex);

void Renderer_SubmitRender(Graph::CommandPool pool, TKit::Span<const RenderSubmitInfo> info);

} // namespace Onyx
