#pragma once

#include "onyx/rendering/pass.hpp"

namespace Onyx
{
enum PipelinePass : u8
{
    PipelinePass_Flat,
    PipelinePass_Shaded,
    PipelinePass_Outlined,
    PipelinePass_Count,
};

enum RenderPass : u8
{
    RenderPass_Flat,
    RenderPass_Shaded,
    RenderPass_Shadow,
    RenderPass_Count
};

} // namespace Onyx
