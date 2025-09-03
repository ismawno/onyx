#pragma once

#include "onyx/core/core.hpp"

namespace Onyx
{
struct SyncData
{
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore RenderFinishedSemaphore;
    VkSemaphore TransferCopyDoneSemaphore;
    VkFence InFlightFence;
};

/**
 * @brief Creates synchronization objects for submission and swap chain image synchronization.
 *
 * Initializes semaphores and fences required for synchronization during rendering.
 *
 * @return The newly created objects.
 */
PerFrameData<SyncData> CreateSynchronizationObjects();

/**
 * @brief Destroys synchronization objects.
 *
 * Releases Vulkan resources associated with the semaphores and fences in the given span of `SyncData` structures.
 *
 * @param p_Objects A span of `SyncData` structures whose resources will be destroyed.
 */
void DestroySynchronizationObjects(TKit::Span<const SyncData> p_Objects);
} // namespace Onyx
