#include "onyx/core/pch.hpp"
#include "onyx/data/sync.hpp"

namespace Onyx
{
TKit::Array<SyncData, ONYX_MAX_FRAMES_IN_FLIGHT> CreateSynchronizationObjects() noexcept
{
    const auto &device = Core::GetDevice();
    const auto &table = device.GetTable();

    PerFrameData<SyncData> syncs{};
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        TKIT_ASSERT_RETURNS(table.CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].ImageAvailableSemaphore),
                            VK_SUCCESS, "[ONYX] Failed to create image available semaphore");

        TKIT_ASSERT_RETURNS(table.CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].TransferCopyDoneSemaphore),
                            VK_SUCCESS, "[ONYX] Failed to create transfer copy done semaphore");

        TKIT_ASSERT_RETURNS(table.CreateFence(device, &fenceInfo, nullptr, &syncs[i].InFlightFence), VK_SUCCESS,
                            "[ONYX] Failed to create in flight fence");
    }
    return syncs;
}
void DestroySynchronizationObjects(const TKit::Span<const SyncData> p_Objects) noexcept
{
    const auto &device = Core::GetDevice();
    const auto &table = device.GetTable();
    for (const SyncData &data : p_Objects)
    {
        table.DestroySemaphore(device, data.ImageAvailableSemaphore, nullptr);
        table.DestroySemaphore(device, data.TransferCopyDoneSemaphore, nullptr);
        table.DestroyFence(device, data.InFlightFence, nullptr);
    }
}

} // namespace Onyx
