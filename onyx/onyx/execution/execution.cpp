#include "onyx/core/pch.hpp"
#include "onyx/execution/execution.hpp"

namespace Onyx::Execution
{
static VKit::CommandPool s_Graphics{};
static VKit::CommandPool s_Transfer{};
static TKit::ArenaArray<CommandPool> s_CommandPools{};

ONYX_NO_DISCARD static Result<VKit::CommandPool> createCommandPool(const u32 p_Family)
{
    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto poolres = VKit::CommandPool::Create(device, p_Family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    TKIT_RETURN_ON_ERROR(poolres);
    return poolres.GetValue();
}

ONYX_NO_DISCARD static Result<> createTransientCommandPools()
{
    TKIT_LOG_INFO("[ONYX][EXECUTION] Creating transient command pools");

    const u32 gindex = GetFamilyIndex(VKit::Queue_Graphics);
    const u32 tindex = GetFamilyIndex(VKit::Queue_Transfer);

    const auto gresult = createCommandPool(gindex);
    TKIT_RETURN_ON_ERROR(gresult);

    s_Graphics = gresult.GetValue();
    if (gindex != tindex)
    {
        const auto tresult = createCommandPool(tindex);
        TKIT_RETURN_ON_ERROR(tresult);

        s_Transfer = tresult.GetValue();
    }
    else
        s_Transfer = s_Graphics;
    return Result<>::Ok();
}

Result<CommandPool *> FindSuitableCommandPool(const u32 p_Family)
{
    for (CommandPool &pool : s_CommandPools)
        if (pool.Family == p_Family && !pool.InUse())
        {
            const auto result = pool.Pool.Reset();
            TKIT_RETURN_ON_ERROR(result);
            return &pool;
        }

    CommandPool &pool = s_CommandPools.Append();
    const auto result = createCommandPool(p_Family);
    TKIT_RETURN_ON_ERROR(result);
    pool.Pool = result.GetValue();
    pool.Family = p_Family;
    return &pool;
}
Result<CommandPool *> FindSuitableCommandPool(const VKit::QueueType p_Type)
{
    return FindSuitableCommandPool(GetFamilyIndex(p_Type));
}

Result<> BeginCommandBuffer(const VkCommandBuffer p_CommandBuffer)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    const auto table = Core::GetDeviceTable();
    const VkResult result = table->BeginCommandBuffer(p_CommandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
        return Result<>::Error(result);
    return Result<>::Ok();
}
Result<> EndCommandBuffer(const VkCommandBuffer p_CommandBuffer)
{
    const auto table = Core::GetDeviceTable();
    const VkResult result = table->EndCommandBuffer(p_CommandBuffer);
    if (result != VK_SUCCESS)
        return Result<>::Error(result);
    return Result<>::Ok();
}

Result<> Initialize(const Specs &p_Specs)
{
    s_CommandPools.Reserve(p_Specs.MaxCommandPools);
    const auto result = createTransientCommandPools();
    TKIT_RETURN_ON_ERROR(result);

    const auto &device = Core::GetDevice();
    const auto table = Core::GetDeviceTable();
    const auto &Queues = Core::GetDevice().GetInfo().Queues;
    for (VKit::Queue *q : Queues)
    {
        VkSemaphoreTypeCreateInfoKHR typeInfo{};
        typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue = 0;

        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = &typeInfo;
        VkSemaphore semaphore;

        const VkResult sresult = table->CreateSemaphore(device, &info, nullptr, &semaphore);
        if (sresult != VK_SUCCESS)
            return Result<>::Error(sresult);

        q->TakeTimelineSemaphoreOwnership(semaphore);
        const auto uresult = q->UpdateCompletedTimeline();
        TKIT_RETURN_ON_ERROR(uresult);
    }
    return Result<>::Ok();
}
void Terminate()
{
    s_Graphics.Destroy();
    if (IsSeparateTransferMode())
        s_Transfer.Destroy();
    for (CommandPool &pool : s_CommandPools)
        pool.Pool.Destroy();
}
Result<> UpdateCompletedQueueTimelines()
{
    const auto &Queues = Core::GetDevice().GetInfo().Queues;
    for (VKit::Queue *q : Queues)
    {
        const auto result = q->UpdateCompletedTimeline();
        TKIT_RETURN_ON_ERROR(result);
    }
    return Result<>::Ok();
}
void RevokeUnsubmittedQueueTimelines()
{
    const auto &Queues = Core::GetDevice().GetInfo().Queues;
    for (VKit::Queue *q : Queues)
        q->RevokeUnsubmittedTimelineValues();
}

VKit::Queue *FindSuitableQueue(const VKit::QueueType p_Type)
{
    const auto &Queues = Core::GetDevice().GetInfo().QueuesPerType[p_Type];
    u64 pending = TKIT_U64_MAX;
    VKit::Queue *queue = nullptr;
    for (VKit::Queue *q : Queues)
    {
        const u64 p = q->GetPendingTimeline();
        if (p < pending)
        {
            pending = p;
            queue = q;
        }
    }
    TKIT_ASSERT(queue, "[ONYX] Queue is null. Some pending submissions returned u64 max. This is bad...");
    return queue;
}

bool IsSeparateTransferMode()
{
    return GetFamilyIndex(VKit::Queue_Graphics) != GetFamilyIndex(VKit::Queue_Transfer);
}
u32 GetFamilyIndex(const VKit::QueueType p_Type)
{
    return Core::GetDevice().GetInfo().PhysicalDevice->GetInfo().FamilyIndices[p_Type];
}

VKit::CommandPool &GetTransientGraphicsPool()
{
    return s_Graphics;
}
VKit::CommandPool &GetTransientTransferPool()
{
    return s_Transfer;
}

Result<TKit::TierArray<SyncData>> CreateSyncData(const u32 p_ImageCount)
{
    const auto &device = Core::GetDevice();
    const auto table = device.GetInfo().Table;

    TKit::TierArray<SyncData> syncs{};
    syncs.Resize(p_ImageCount);
    for (u32 i = 0; i < p_ImageCount; ++i)
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkResult result = table->CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].ImageAvailableSemaphore);
        if (result != VK_SUCCESS)
            return Result<>::Error(result);
        result = table->CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].RenderFinishedSemaphore);
        if (result != VK_SUCCESS)
            return Result<>::Error(result);
    }
    return syncs;
}
void DestroySyncData(const TKit::Span<const SyncData> p_Objects)
{
    const auto &device = Core::GetDevice();
    const auto table = device.GetInfo().Table;

    for (const SyncData &data : p_Objects)
    {
        table->DestroySemaphore(device, data.ImageAvailableSemaphore, nullptr);
        table->DestroySemaphore(device, data.RenderFinishedSemaphore, nullptr);
    }
}

} // namespace Onyx::Execution
