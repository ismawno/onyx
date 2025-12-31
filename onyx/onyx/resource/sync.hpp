#pragma once

#include "onyx/core/core.hpp"

namespace Onyx::Detail
{
struct SyncFrameData
{
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore TransferCopyDoneSemaphore;
    VkFence InFlightFence;
};

struct SyncImageData
{
    VkSemaphore RenderFinishedSemaphore;
    VkFence InFlightImage;
};

PerFrameData<SyncFrameData> CreatePerFrameSyncData();
PerImageData<SyncImageData> CreatePerImageSyncData(u32 p_ImageCount);

void DestroyPerFrameSyncData(TKit::Span<const SyncFrameData> p_Objects);
void DestroyPerImageSyncData(TKit::Span<const SyncImageData> p_Objects);
} // namespace Onyx::Detail
