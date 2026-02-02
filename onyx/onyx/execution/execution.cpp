#include "onyx/core/pch.hpp"
#include "onyx/execution/execution.hpp"

namespace Onyx
{
struct CommandPool
{
    Execution::Tracker Tracker{};
    u32 Family;
    u32 NextCommand = 0;
    VKit::CommandPool Pool{};
    TKit::TierArray<VkCommandBuffer> CommandBuffers{};
};
} // namespace Onyx

namespace Onyx::Execution
{
static TKit::Storage<VKit::CommandPool> s_Graphics{};
static TKit::Storage<VKit::CommandPool> s_Transfer{};
static TKit::Storage<TKit::ArenaArray<CommandPool>> s_CommandPools{};

ONYX_NO_DISCARD static Result<VKit::CommandPool> createCommandPool(const u32 family)
{
    const VKit::LogicalDevice &device = Core::GetDevice();
    return VKit::CommandPool::Create(device, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
}

ONYX_NO_DISCARD static Result<> createTransientCommandPools()
{
    const u32 gindex = GetFamilyIndex(VKit::Queue_Graphics);
    const u32 tindex = GetFamilyIndex(VKit::Queue_Transfer);

    const auto gresult = createCommandPool(gindex);
    TKIT_RETURN_ON_ERROR(gresult);

    *s_Graphics = gresult.GetValue();
    if (gindex != tindex)
    {
        const auto tresult = createCommandPool(tindex);
        TKIT_RETURN_ON_ERROR(tresult);

        *s_Transfer = tresult.GetValue();
    }
    else
        s_Transfer = s_Graphics;
    return Result<>::Ok();
}

Result<CommandPool *> FindSuitableCommandPool(const u32 family)
{
    for (CommandPool &pool : *s_CommandPools)
        if (pool.Family == family && !pool.Tracker.InUse())
        {
            TKIT_RETURN_IF_FAILED(pool.Pool.Reset());
            pool.NextCommand = 0;
            return &pool;
        }

    CommandPool &pool = s_CommandPools->Append();
    const auto result = createCommandPool(family);
    TKIT_RETURN_ON_ERROR(result);
    pool.Pool = result.GetValue();
    pool.Family = family;
    pool.NextCommand = 0;
    return &pool;
}
Result<CommandPool *> FindSuitableCommandPool(const VKit::QueueType type)
{
    return FindSuitableCommandPool(GetFamilyIndex(type));
}

Result<VkCommandBuffer> Allocate(CommandPool *pool)
{
    if (pool->NextCommand < pool->CommandBuffers.GetSize())
        return pool->CommandBuffers[pool->NextCommand++];

    const auto result = pool->Pool.Allocate();
    TKIT_RETURN_ON_ERROR(result);
    pool->CommandBuffers.Append(result.GetValue());
    ++pool->NextCommand;
    return result;
}
void MarkInUse(CommandPool *pool, const VKit::Queue *queue, const u64 inFlightValue)
{
    pool->Tracker.MarkInUse(queue, inFlightValue);
}

Result<> BeginCommandBuffer(const VkCommandBuffer commandBuffer)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    const auto table = Core::GetDeviceTable();
    const VkResult result = table->BeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
        return Result<>::Error(result);
    return Result<>::Ok();
}
Result<> EndCommandBuffer(const VkCommandBuffer commandBuffer)
{
    const auto table = Core::GetDeviceTable();
    const VkResult result = table->EndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS)
        return Result<>::Error(result);
    return Result<>::Ok();
}

Result<> Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][EXECUTION] Initializing");
    s_Graphics.Construct();
    s_Transfer.Construct();
    s_CommandPools.Construct();
    s_CommandPools->Reserve(specs.MaxCommandPools);
    TKIT_RETURN_IF_FAILED(createTransientCommandPools());

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
        TKIT_RETURN_IF_FAILED(q->UpdateCompletedTimeline());
    }
    return Result<>::Ok();
}
void Terminate()
{
    s_Graphics->Destroy();
    if (IsSeparateTransferMode())
        s_Transfer->Destroy();
    for (CommandPool &pool : *s_CommandPools)
        pool.Pool.Destroy();

    s_Graphics.Destruct();
    s_Transfer.Destruct();
    s_CommandPools.Destruct();
}
Result<> UpdateCompletedQueueTimelines()
{
    const auto &Queues = Core::GetDevice().GetInfo().Queues;
    for (VKit::Queue *q : Queues)
    {
        TKIT_RETURN_IF_FAILED(q->UpdateCompletedTimeline());
    }
    return Result<>::Ok();
}
void RevokeUnsubmittedQueueTimelines()
{
    const auto &Queues = Core::GetDevice().GetInfo().Queues;
    for (VKit::Queue *q : Queues)
        q->RevokeUnsubmittedTimelineValues();
}

VKit::Queue *FindSuitableQueue(const VKit::QueueType type)
{
    const auto &Queues = Core::GetDevice().GetInfo().QueuesPerType[type];
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
u32 GetFamilyIndex(const VKit::QueueType type)
{
    return Core::GetDevice().GetInfo().PhysicalDevice->GetInfo().FamilyIndices[type];
}

VKit::CommandPool &GetTransientGraphicsPool()
{
    return *s_Graphics;
}
VKit::CommandPool &GetTransientTransferPool()
{
    return *s_Transfer;
}

Result<TKit::TierArray<SyncData>> CreateSyncData(const u32 imageCount)
{
    const auto &device = Core::GetDevice();
    const auto table = device.GetInfo().Table;

    TKit::TierArray<SyncData> syncs{};
    syncs.Resize(imageCount);
    for (u32 i = 0; i < imageCount; ++i)
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkResult result = table->CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].ImageAvailableSemaphore);
        if (result != VK_SUCCESS)
        {
            DestroySyncData(syncs);
            return Result<>::Error(result);
        }
        result = table->CreateSemaphore(device, &semaphoreInfo, nullptr, &syncs[i].RenderFinishedSemaphore);
        if (result != VK_SUCCESS)
        {
            DestroySyncData(syncs);
            return Result<>::Error(result);
        }
    }
    return syncs;
}
void DestroySyncData(const TKit::Span<const SyncData> objects)
{
    const auto &device = Core::GetDevice();
    const auto table = device.GetInfo().Table;

    for (const SyncData &data : objects)
    {
        table->DestroySemaphore(device, data.ImageAvailableSemaphore, nullptr);
        table->DestroySemaphore(device, data.RenderFinishedSemaphore, nullptr);
    }
}

} // namespace Onyx::Execution
