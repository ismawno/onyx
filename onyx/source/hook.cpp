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
struct Hook
{
    Hook(const TKit::StringView source, const HookSpecs &specs)
        : Source(source), SearchPath(specs.SearchPath), Varyings(specs.Varyings), Stage(specs.Stage)
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

    void Reset(const TKit::StringView source)
    {
        Source = source;
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

template <Dimension D> Resource Hook_FromSource(const TKit::StringView source, const HookSpecs &specs)
{
    HookData &data = s_Data->At(D);
    // in debug youll see counts be max + 1 if this trips, its fine
    TKIT_ENSURE(
        data.Counts[specs.Stage]++ < s_MaxHooks[specs.Stage],
        "[ONYX][SHADER-HOOK] Cannot create {}D shader hook of specs.Stage {} as there are already the maximum amount "
        "for this specs.Stage ({})",
        u8(D), ToString(specs.Stage), s_MaxHooks[specs.Stage]);

    const ResourceType rtype = specs.Stage == HookStage_Vertex ? Resource_VertexHook : Resource_FragmentHook;
    return Handle_CreateForResource(rtype, data.Hooks[specs.Stage].Insert(source, specs));
}

template <Dimension D> Result<Resource> Hook_FromFile(const TKit::TierPath &filepath, const HookSpecs &specs)
{
    ONYX_GENERATE_ERROR_FILE_NOT_FOUND(filepath, "[ONYX][SHADER-HOOK] Audio file '{}' does not exist",
                                       "[ONYX][SHADER-HOOK] Expected an audio file at '{}', but found a directory");

    const auto res = TKit::ReadFile<TKit::StackString>(filepath);
    ONYX_RETURN_ERROR_IF(!res, Error_LoadFailed, "[ONYX][SHADER-HOOK] Failed to load shader at '{}' with error: {}",
                         filepath, TKit::ToString(res.GetError()));

    return Hook_FromSource<D>(*res, specs);
}

template <Dimension D> static Hook &getHook(const Resource handle)
{
    const ResourceType rtype = Handle_GetResourceType(handle);
    if (rtype == Resource_VertexHook)
    {
        ONYX_CHECK_RESOURCE_HANDLE(handle, Resource_VertexHook);
        return s_Data->At(D).Hooks[HookStage_Vertex][Handle_GetResourceId(handle)];
    }
    ONYX_CHECK_RESOURCE_HANDLE(handle, Resource_FragmentHook);
    return s_Data->At(D).Hooks[HookStage_Fragment][Handle_GetResourceId(handle)];
}

template <Dimension D> static Result<> Hook_Precompile(Hook &hook, const u32 field, const HookPrecompileOptions &opts)
{
    ShaderCompileOptions copts{};
    copts.Dimensions[D - 2] = true;
    copts.Geometries = opts.Geometries;
    copts.RenderPasses = opts.RenderPasses;

    HookCompileOptions hopts{};
    const RenderState mask = hook.Stage == HookStage_Vertex ? VertexHook_Mask : FragmentHook_Mask;
    hopts.HookBitField = field << mask;
    hopts.Stage = hook.Stage;
    hopts.Varyings = hook.Varyings;
    hopts.SearchPath = hook.SearchPath;
    hopts.Source = hook.Source;
    hopts.VertexOrOpaque = &hook.VertexOrOpaque;

    if (hook.Stage == HookStage_Fragment)
        hopts.Transparent = &hook.Transparent;

    copts.HookOptions = &hopts;
    return Renderer_CompileShaders(copts);
}

template <Dimension D> Result<> Hook_Precompile(const Resource handle, const HookPrecompileOptions &opts)
{
    Hook &hook = getHook<D>(handle);
    Hook_Precompile<D>(hook, Handle_GetResourceId(handle) + 1, opts);
}

template <Dimension D>
HookShaders Hook_GetFromRenderState(const RenderPass rpass, const Geometry geo, const RenderState state)
{
    HookShaders shaders{};
    const u32 vfield = (state & VertexHook_Mask) >> VertexHook_Offset;
    const u32 ffield = (state & FragmentHook_Mask) >> FragmentHook_Offset;

    const auto precompileIfNeeded = [&](const HookStage stage, const u32 field) -> const Hook & {
        const u32 id = field - 1;
        const Hook &hook = s_Data->At(D).Hooks[stage][id];
        if (!hook.HasShader(rpass, geo))
        {
            HookPrecompileOptions opts{};
            opts.Geometries[geo] = true;
            opts.RenderPasses[rpass] = true;
            Hook_Precompile<D>(hook, field, opts);
        }
        return hook;
    };

    if (vfield != 0)
    {
        const Hook &hook = precompileIfNeeded(HookStage_Vertex, vfield);
        shaders.Vertex = hook.VertexOrOpaque[rpass][geo];
    }
    if (ffield != 0)
    {
        const Hook &hook = precompileIfNeeded(HookStage_Fragment, ffield);
        shaders.Opaque = hook.VertexOrOpaque[rpass][geo];
        shaders.Transparent = hook.Transparent[rpass][geo];
    }
    return shaders;
}

template <Dimension D> bool vertexHook_IsResourceValid(const u32 id)
{
    return s_Data->At(D).Hooks[HookStage_Vertex].Contains(id);
}
template <Dimension D> bool fragmentHook_IsResourceValid(const u32 id)
{
    return s_Data->At(D).Hooks[HookStage_Fragment].Contains(id);
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
} // namespace Onyx
