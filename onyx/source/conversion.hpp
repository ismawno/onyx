#pragma once

#include "onyx/instance.hpp"
#include "onyx/view.hpp"
#include "onyx/image.hpp"
#include "pass.hpp"
#include "tkit/utils/debug.hpp"

namespace Onyx
{
const char *ToString(Geometry geo);
const char *ToString(LightType light);
const char *ToString(RenderPass pass);
const char *ToString(PipelinePass pass);

Format GetFormat(const u32 components, ImageComponentType type, bool rgb);
VkFormat AsVulkanFormat(const Format format);

constexpr VkExtent2D AsVulkanExtent(const u32v2 &extent)
{
    return {extent[0], extent[1]};
}

constexpr VkViewport AsVulkanViewport(const Viewport &vp)
{
    return {vp.Position[0], vp.Position[1], vp.Dimensions[0], vp.Dimensions[1], vp.Depth[0], vp.Depth[1]};
}

constexpr VkRect2D AsVulkanScissor(const Scissor &sc)
{
    return {{sc.Offset[0], sc.Offset[1]}, {sc.Extent[0], sc.Extent[1]}};
}
constexpr RenderPass GetRenderPass(const PipelinePass mode)
{
    switch (mode)
    {
    case PipelinePass_Shaded:
        return RenderPass_Shaded;
    case PipelinePass_Flat:
        return RenderPass_Flat;
    case PipelinePass_Outlined:
        return RenderPass_Flat;
    default:
        TKIT_FATAL("[ONYX][PASS] Unrecognized pipeline mode: {}", ToString(mode));
        return RenderPass_Count;
    }
}
} // namespace Onyx
