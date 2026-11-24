#pragma once

#ifndef ONYX_ENABLE_HOOKS
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_HOOKS"
#endif

#include "onyx/core.hpp"
#include "onyx/handle.hpp"

namespace Onyx
{
enum HookStage : u8
{
    HookStage_Vertex,
    HookStage_Fragment,
    HookStage_Count,
};

enum HookInterp : u8
{
    HookInterp_Smooth,
    HookInterp_Flat,
    HookInterp_NoPerspective,
};

enum HookType : u8
{
    HookType_F32,
    HookType_F32V1 = HookType_F32,
    HookType_F32V2,
    HookType_F32V3,
    HookType_F32V4,

    HookType_I32,
    HookType_I32V1 = HookType_I32,
    HookType_I32V2,
    HookType_I32V3,
    HookType_I32V4,

    HookType_U32,
    HookType_U32V1 = HookType_U32,
    HookType_U32V2,
    HookType_U32V3,
    HookType_U32V4,
};

struct HookVarying
{
    TKit::TierString Name;
    HookType Type;
    HookInterp Interpolation;
};

struct HookSpecs
{
    TKit::Span<const HookVarying> Varyings{};
    TKit::StringView SearchPath{};
};

using ShaderCombination = ten<Graph::Shader, RenderPass_Count, Geometry_Count>;

template <Dimension D> Resource Hook_FromSource(HookStage stage, TKit::StringView source, const HookSpecs &specs = {});
template <Dimension D>
ONYX_NO_DISCARD Result<Resource> Hook_FromFile(HookStage stage, const TKit::TierPath &filepath,
                                               const HookSpecs &specs = {});

template <Dimension D> void Hook_UpdateFromSource(Resource hook, TKit::StringView source, const HookSpecs &specs = {});
template <Dimension D>
ONYX_NO_DISCARD Result<> Hook_UpdateFromFile(Resource hook, const TKit::TierPath &filepath,
                                             const HookSpecs &specs = {});

struct HookPrecompileOptions
{
    TKit::FixedArray<bool, RenderPass_Count> RenderPasses{};
    TKit::FixedArray<bool, Geometry_Count> Geometries{};

    template <u32 Count> static constexpr TKit::FixedArray<bool, Count> &Full(TKit::FixedArray<bool, Count> &arr)
    {
        for (bool &b : arr)
            b = true;
        return arr;
    }

    static constexpr HookPrecompileOptions Full()
    {
        HookPrecompileOptions opts;
        Full(opts.RenderPasses);
        Full(opts.Geometries);
        return opts;
    }
};

template <Dimension D>
ONYX_NO_DISCARD Result<> Hook_Precompile(Resource hook,
                                         const HookPrecompileOptions &opts = HookPrecompileOptions::Full());

const char *ToString(HookStage stage);
} // namespace Onyx
