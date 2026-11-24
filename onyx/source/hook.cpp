#include "pch.hpp"
#include "onyx/hook.hpp"
#include "resources.hpp"
#include "renderer.hpp"
#include "graph/shader.hpp"
#include "tkit/container/hive.hpp"
#include "tkit/filesystem/file.hpp"

#define ONYX_MAX_HOOKS (ONYX_MAX_VERTEX_HOOKS + ONYX_MAX_FRAGMENT_HOOKS)

namespace Onyx
{
static void resetComb(ShaderCombination &comb, const bool forInit = false)
{
    for (Graph::Shader &sh : comb)
        if (forInit || sh != Graph::NullHandle)
        {
            if (!forInit)
                Graph::Shader_Destroy(sh);
            sh = Graph::NullHandle;
        }
}
template <Dimension D> static void resetComb(ShaderCombination &comb, const ShaderCompileOptions &opts)
{
    for (u32 rpass = 0; rpass < RenderPass_Count; ++rpass)
        for (u32 geo = 0; geo < Geometry_Count; ++geo)
            if (opts.Dimensions[D - 2] && opts.RenderPasses[rpass] && opts.Geometries[geo] &&
                comb[rpass][geo] != Graph::NullHandle)
            {
                Graph::Shader_Destroy(comb[rpass][geo]);
                comb[rpass][geo] = Graph::NullHandle;
            }
}
struct Hook
{
    Hook(const HookStage stage, const TKit::StringView source, const HookSpecs &specs)
        : Source(source), SearchPath(specs.SearchPath), Varyings(specs.Varyings), Stage(stage)
    {
        resetComb(VertexOrOpaque, true);
        resetComb(Transparent, true);
    }

    TKit::TierString Source;
    TKit::TierString SearchPath;
    TKit::TierArray<HookVarying> Varyings;
    HookStage Stage;

    ShaderCombination VertexOrOpaque{};
    ShaderCombination Transparent{};

    template <Dimension D> void Reset(const u32 id, const ShaderCompileOptions &opts)
    {
        resetComb<D>(VertexOrOpaque, opts);
        resetComb<D>(Transparent, opts);
        const RenderState offset = Stage == HookStage_Vertex ? VertexHook_Offset : FragmentHook_Offset;
        Renderer_RemoveStaleHookPipelines((id + 1) << offset, Stage);
    }

    void Reset(const TKit::StringView source, const HookSpecs &specs)
    {
        Source = source;
        SearchPath = specs.SearchPath;
        Varyings = specs.Varyings;
        resetComb(VertexOrOpaque);
        resetComb(Transparent);
    }

    bool HasShader(const RenderPass rpass, const Geometry geo) const
    {
        if (Stage == HookStage_Vertex)
            return VertexOrOpaque[rpass][geo] != Graph::NullHandle;

        return VertexOrOpaque[rpass][geo] != Graph::NullHandle && Transparent[rpass][geo] != Graph::NullHandle;
    }
};

struct HookData
{
    TKit::FixedArray<TKit::StaticHive<Hook, ONYX_MAX_HOOKS>, HookStage_Count> Hooks{};
#ifdef TKIT_ENABLE_ENSURE
    TKit::FixedArray<u32, HookStage_Count> Counts{};
#endif
};

#ifdef TKIT_ENABLE_ENSURE
static constexpr TKit::FixedArray<u32, HookStage_Count> s_MaxHooks{ONYX_MAX_VERTEX_HOOKS, ONYX_MAX_FRAGMENT_HOOKS};
#endif

static TKit::Storage<TKit::FixedArray<HookData, D_Count>> s_Data{};

void Hook_Initialize()
{
    s_Data.Construct();
}
void Hook_Terminate()
{
    s_Data.Destruct();
}

template <Dimension D>
Resource Hook_FromSource(const HookStage stage, const TKit::StringView source, const HookSpecs &specs)
{
    HookData &data = s_Data->At(D - 2);
    // in debug youll see counts be max + 1 if this trips, its fine
    TKIT_ENSURE(data.Counts[stage]++ < s_MaxHooks[stage],
                "[ONYX][SHADER-HOOK] Cannot create {}D shader hook of stage {} as there are already the maximum amount "
                "for this stage ({})",
                u8(D), ToString(stage), s_MaxHooks[stage]);

    const ResourceType rtype = stage == HookStage_Vertex ? Resource_VertexHook : Resource_FragmentHook;
    return Handle_CreateForResource(rtype, data.Hooks[stage].Insert(stage, source, specs));
}

template <Dimension D>
Result<Resource> Hook_FromFile(const HookStage stage, const TKit::TierPath &filepath, const HookSpecs &specs)
{
    ONYX_GENERATE_ERROR_FILE_NOT_FOUND(filepath, "[ONYX][SHADER-HOOK] Shader file '{}' does not exist",
                                       "[ONYX][SHADER-HOOK] Expected a shader file at '{}', but found a directory");

    const auto res = TKit::ReadFile<TKit::StackString>(filepath);
    ONYX_RETURN_ERROR_IF(!res, Error_LoadFailed, "[ONYX][SHADER-HOOK] Failed to load shader at '{}' with error: {}",
                         filepath, TKit::ToString(res.GetError()));

    return Hook_FromSource<D>(stage, *res, specs);
}

template <Dimension D> static Hook &getHook(const Resource handle)
{
    const ResourceType rtype = Handle_GetResourceType(handle);
    if (rtype == Resource_VertexHook)
    {
        ONYX_CHECK_RESOURCE_HANDLE(handle, Resource_VertexHook);
        return s_Data->At(D - 2).Hooks[HookStage_Vertex][Handle_GetResourceId(handle)];
    }
    ONYX_CHECK_RESOURCE_HANDLE(handle, Resource_FragmentHook);
    return s_Data->At(D - 2).Hooks[HookStage_Fragment][Handle_GetResourceId(handle)];
}

template <Dimension D>
void Hook_UpdateFromSource(const Resource handle, const TKit::StringView source, const HookSpecs &specs)
{
    Hook &hook = getHook<D>(handle);
    hook.Reset(source, specs);
}
template <Dimension D>
ONYX_NO_DISCARD Result<> Hook_UpdateFromFile(const Resource handle, const TKit::TierPath &filepath,
                                             const HookSpecs &specs)
{
    ONYX_GENERATE_ERROR_FILE_NOT_FOUND(filepath, "[ONYX][SHADER-HOOK] Shader file '{}' does not exist",
                                       "[ONYX][SHADER-HOOK] Expected an audio file at '{}', but found a directory");

    const auto res = TKit::ReadFile<TKit::StackString>(filepath);
    ONYX_RETURN_ERROR_IF(!res, Error_LoadFailed, "[ONYX][SHADER-HOOK] Failed to load shader at '{}' with error: {}",
                         filepath, TKit::ToString(res.GetError()));

    Hook_UpdateFromSource<D>(handle, *res, specs);
    return Result<>::Ok();
}

template <Dimension D> static Result<> hook_Precompile(Hook &hook, const u32 field, const HookPrecompileOptions &opts)
{
    RendererShaderCompileOptions ropts{};
    ShaderCompileOptions &copts = ropts.Options;
    copts.Dimensions[D - 2] = true;
    copts.Geometries = opts.Geometries;
    copts.RenderPasses = opts.RenderPasses;
    if (hook.Stage == HookStage_Fragment)
        copts.RenderPasses[RenderPass_Shadow] = false;

    HookCompileOptions hopts{};
    const RenderState offset = hook.Stage == HookStage_Vertex ? VertexHook_Offset : FragmentHook_Offset;
    hopts.HookBitField = field << offset;
    hopts.Stage = hook.Stage;
    hopts.Varyings = hook.Varyings;
    hopts.SearchPath = hook.SearchPath;
    hopts.Source = hook.Source;
    hopts.VertexOrOpaque = &hook.VertexOrOpaque;

    if (hook.Stage == HookStage_Fragment)
        hopts.Transparent = &hook.Transparent;

    ropts.HookOptions = &hopts;
    return Renderer_CompileShaders(ropts);
}

template <Dimension D> Result<> Hook_Precompile(const Resource handle, const HookPrecompileOptions &opts)
{
    Hook &hook = getHook<D>(handle);
    return hook_Precompile<D>(hook, Handle_GetResourceId(handle) + 1, opts);
}

template <Dimension D>
HookShaders Hook_GetFromRenderState(const RenderPass rpass, const Geometry geo, const RenderState state)
{
    HookShaders shaders{};
    const u32 vfield = (state & VertexHook_Mask) >> VertexHook_Offset;
    const u32 ffield = (state & FragmentHook_Mask) >> FragmentHook_Offset;

    const auto precompileIfNeeded = [&](const HookStage stage, const u32 field) -> const Hook * {
        const u32 id = field - 1;
        Hook &hook = s_Data->At(D - 2).Hooks[stage][id];
        if (!hook.HasShader(rpass, geo))
        {
            HookPrecompileOptions opts{};
            opts.Geometries[geo] = true;
            opts.RenderPasses[rpass] = true;
            const auto res = hook_Precompile<D>(hook, field, opts);
            ONYX_LOG_RESULT_ERROR(res);
            if (!res)
                return nullptr;
        }
        return &hook;
    };

    if (vfield != 0)
    {
        const Hook *hook = precompileIfNeeded(HookStage_Vertex, vfield);
        if (hook)
            shaders.Vertex = hook->VertexOrOpaque[rpass][geo];
    }
    if (ffield != 0)
    {
        const Hook *hook = precompileIfNeeded(HookStage_Fragment, ffield);
        if (hook)
        {
            shaders.Opaque = hook->VertexOrOpaque[rpass][geo];
            shaders.Transparent = hook->Transparent[rpass][geo];
        }
    }
    return shaders;
}

template <Dimension D> void hook_ResetAll(const ShaderCompileOptions &opts)
{
    for (auto &hooks : s_Data->At(D - 2).Hooks)
        for (const u32 id : hooks.GetValidIds())
            hooks[id].Reset<D>(id, opts);
}

void Hook_ResetAll(const ShaderCompileOptions &opts)
{
    hook_ResetAll<D2>(opts);
    hook_ResetAll<D3>(opts);
}

template <Dimension D> bool vertexHook_IsResourceValid(const u32 id)
{
    return s_Data->At(D - 2).Hooks[HookStage_Vertex].Contains(id);
}
template <Dimension D> bool fragmentHook_IsResourceValid(const u32 id)
{
    return s_Data->At(D - 2).Hooks[HookStage_Fragment].Contains(id);
}

const char *ToString(const HookStage stage)
{
    switch (stage)
    {
    case HookStage_Vertex:
        return "HookStage_Vertex";
    case HookStage_Fragment:
        return "HookStage_Fragment";
    default:
        TKIT_FATAL("[ONYX][SHADER-HOOK] Unrecognized shader hook stage {}", u8(stage));
        return "Unknown";
    }
}
template Resource Hook_FromSource<D2>(HookStage stage, TKit::StringView source, const HookSpecs &specs);
template Resource Hook_FromSource<D3>(HookStage stage, TKit::StringView source, const HookSpecs &specs);

template Result<Resource> Hook_FromFile<D2>(HookStage stage, const TKit::TierPath &filepath, const HookSpecs &specs);
template Result<Resource> Hook_FromFile<D3>(HookStage stage, const TKit::TierPath &filepath, const HookSpecs &specs);

template void Hook_UpdateFromSource<D2>(Resource hook, TKit::StringView source, const HookSpecs &specs);
template void Hook_UpdateFromSource<D3>(Resource hook, TKit::StringView source, const HookSpecs &specs);

template Result<> Hook_UpdateFromFile<D2>(Resource hook, const TKit::TierPath &filepath, const HookSpecs &specs);
template Result<> Hook_UpdateFromFile<D3>(Resource hook, const TKit::TierPath &filepath, const HookSpecs &specs);

template Result<> Hook_Precompile<D2>(Resource handle, const HookPrecompileOptions &opts);
template Result<> Hook_Precompile<D3>(Resource handle, const HookPrecompileOptions &opts);

template bool vertexHook_IsResourceValid<D2>(u32 id);
template bool vertexHook_IsResourceValid<D3>(u32 id);

template bool fragmentHook_IsResourceValid<D2>(u32 id);
template bool fragmentHook_IsResourceValid<D3>(u32 id);

template HookShaders Hook_GetFromRenderState<D2>(RenderPass rpass, Geometry geo, RenderState state);
template HookShaders Hook_GetFromRenderState<D3>(RenderPass rpass, Geometry geo, RenderState state);
} // namespace Onyx
