#pragma once

#include "onyx/state.hpp"
#include "graph/core.hpp"

namespace Onyx
{
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
