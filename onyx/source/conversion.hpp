#pragma once

#include "onyx/instance.hpp"
#include "onyx/image.hpp"
#include "pass.hpp"
#include "graph/core.hpp"
#include "tkit/utils/debug.hpp"

namespace Onyx
{
const char *ToString(Geometry geo);
const char *ToString(LightType light);
const char *ToString(RenderPass pass);
const char *ToString(PipelinePass pass);
const char *ToString(BlendPass bpass);

Graph::Format GetFormat(const u32 components, ImageComponentType type, bool rgb);

constexpr RenderPass GetRenderPass(const PipelinePass mode)
{
    switch (mode)
    {
    case PipelinePass_Shaded:
        return RenderPass_Shaded;
    case PipelinePass_Flat:
    case PipelinePass_Outlined:
        return RenderPass_Flat;
    default:
        TKIT_FATAL("[ONYX][PASS] Unrecognized pipeline mode: {}", ToString(mode));
        return RenderPass_Count;
    }
}
} // namespace Onyx
