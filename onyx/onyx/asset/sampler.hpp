#pragma once

#include "onyx/asset/handle.hpp"

namespace Onyx
{
using Sampler = Asset;
constexpr Sampler NullSampler = NullAsset;

enum SamplerMode : u8
{
    SamplerMode_Linear,
    SamplerMode_Nearest,
};

enum SamplerFilter : u8
{
    SamplerFilter_Linear,
    SamplerFilter_Nearest,
    SamplerFilter_Cubic,
};

enum SamplerWrap : u8
{
    SamplerWrap_Repeat,
    SamplerWrap_ClampToEdge,
    SamplerWrap_MirroredRepeat,
};

struct SamplerData
{
    SamplerMode Mode = SamplerMode_Linear;

    SamplerFilter MinFilter = SamplerFilter_Linear;
    SamplerFilter MagFilter = SamplerFilter_Linear;

    SamplerWrap WrapU = SamplerWrap_Repeat;
    SamplerWrap WrapV = SamplerWrap_Repeat;
    SamplerWrap WrapW = SamplerWrap_Repeat;
};

} // namespace Onyx
