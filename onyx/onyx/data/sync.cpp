#include "onyx/core/pch.hpp"
#include "onyx/data/sync.hpp"

namespace Onyx::Detail
{
PerFrameData<SyncFrameData> CreatePerFrameSyncData()
{
    const auto &device = Core::GetDevice();
    const auto &table = device.GetInfo().Table;

    PerFrameData<SyncFrameData> syncs{};
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VKIT_ASSERT_EXPRESSION(
            table.CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].ImageAvailableSemaphore));
        VKIT_ASSERT_EXPRESSION(
            table.CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].TransferCopyDoneSemaphore));
        VKIT_ASSERT_EXPRESSION(table.CreateFence(device, &fenceInfo, nullptr, &syncs[i].InFlightFence));
    }
    return syncs;
}
PerImageData<SyncImageData> CreatePerImageSyncData(const u32 p_ImageCount)
{
    const auto &device = Core::GetDevice();
    const auto &table = device.GetInfo().Table;

    PerImageData<SyncImageData> syncs{};
    syncs.Resize(p_ImageCount);
    for (u32 i = 0; i < p_ImageCount; ++i)
    {
        syncs[i].InFlightImage = VK_NULL_HANDLE;
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VKIT_ASSERT_EXPRESSION(
            table.CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].RenderFinishedSemaphore));
    }
    return syncs;
}
void DestroyPerFrameSyncData(const TKit::Span<const SyncFrameData> p_Objects)
{
    const auto &device = Core::GetDevice();
    const auto &table = device.GetInfo().Table;
    for (const SyncFrameData &data : p_Objects)
    {
        table.DestroySemaphore(device, data.ImageAvailableSemaphore, nullptr);
        table.DestroySemaphore(device, data.TransferCopyDoneSemaphore, nullptr);
        table.DestroyFence(device, data.InFlightFence, nullptr);
    }
}
void DestroyPerImageSyncData(const TKit::Span<const SyncImageData> p_Objects)
{
    const auto &device = Core::GetDevice();
    const auto &table = device.GetInfo().Table;

    for (const SyncImageData &data : p_Objects)
        table.DestroySemaphore(device, data.RenderFinishedSemaphore, nullptr);
}

} // namespace Onyx::Detail
