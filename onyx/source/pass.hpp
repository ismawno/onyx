#pragma once

#include "onyx/pass.hpp"

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

enum StandalonePass : u8
{
    StandalonePass_RayMarch,
    StandalonePass_Blend,
    StandalonePass_Outline,
    StandalonePass_FXAA,
    StandalonePass_Compositor,
    StandalonePass_Count,
};

enum AttachmentKind : u8
{
    AttachmentKind_SrgbColor,
    AttachmentKind_LinearColor,
    AttachmentKind_DepthStencil,
    AttachmentKind_Accumulation,
    AttachmentKind_Revealage,
};

constexpr Graph::Format GetAttachmentFormat(const AttachmentKind kind)
{
    switch (kind)
    {
    case AttachmentKind_SrgbColor:
        return Graph::Format_B8G8R8A8_SRGB;
    case AttachmentKind_LinearColor:
        return Graph::Format_B8G8R8A8_UNORM;
    case AttachmentKind_DepthStencil:
        return Graph::Format_D32_SFLOAT_S8_UINT;
    case AttachmentKind_Accumulation:
        return Graph::Format_R16G16B16A16_SFLOAT;
    case AttachmentKind_Revealage:
        return Graph::Format_R8_UNORM;
    default:
        return Graph::Format_Undefined;
    }
}

} // namespace Onyx
