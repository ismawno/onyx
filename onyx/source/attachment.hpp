#pragma once

#include "onyx/alias.hpp"

namespace Onyx
{
enum AttachmentType : u8
{
    Attachment_Transparent,
    Attachment_Revealage,
    Attachment_Intermediate,
    Attachment_Outline,
    Attachment_DepthStencil,
    Attachment_Final,
    Attachment_Count,
};

constexpr VkFormat GetAttachmentFormat(const AttachmentType atype)
{
    switch (atype)
    {
    case Attachment_Intermediate:
    case Attachment_Outline:
    case Attachment_Final:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case Attachment_Transparent:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Attachment_Revealage:
        return VK_FORMAT_R16_SFLOAT;
    case Attachment_DepthStencil:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

} // namespace Onyx
