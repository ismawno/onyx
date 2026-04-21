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
    const VKit::LogicalDevice &device = GetDevice();
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
        *s_Transfer = *s_Graphics;
    if (IsDebugUtilsEnabled())
    {
        TKIT_RETURN_IF_FAILED(s_Graphics->SetName("onyx-transient-graphics-command-pool"));
        if (gindex != tindex)
            return s_Transfer->SetName("onyx-transient-transfer-command-pool");
    }
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
    if (IsDebugUtilsEnabled())
    {
        const std::string name =
            TKit::Format("onyx-ring-command-pool-index-{}-family-{}", s_CommandPools->GetSize() - 1, family);
        TKIT_RETURN_IF_FAILED(pool.Pool.SetName(name.c_str()));
    }
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

    const auto table = GetDeviceTable();
    VKIT_RETURN_IF_FAILED(table->BeginCommandBuffer(commandBuffer, &beginInfo), Result<>);
    return Result<>::Ok();
}
Result<> EndCommandBuffer(const VkCommandBuffer commandBuffer)
{
    const auto table = GetDeviceTable();
    VKIT_RETURN_IF_FAILED(table->EndCommandBuffer(commandBuffer), Result<>);
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

    const auto &device = GetDevice();
    const auto table = GetDeviceTable();
    const auto &queues = GetDevice().GetInfo().Queues;
    for (u32 i = 0; i < queues.GetSize(); ++i)
    {
        VKit::Queue *q = queues[i];
        VkSemaphoreTypeCreateInfoKHR typeInfo{};
        typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue = 0;

        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = &typeInfo;
        VkSemaphore semaphore;

        VKIT_RETURN_IF_FAILED(table->CreateSemaphore(device, &info, nullptr, &semaphore), Result<>);
        q->TakeTimelineSemaphoreOwnership(semaphore);
        TKIT_RETURN_IF_FAILED(q->UpdateCompletedTimeline());
        if (IsDebugUtilsEnabled())
        {
            const std::string name =
                TKit::Format("onyx-timeline-semaphore-queue-index-{}-family-{}", i, q->GetFamily());
            TKIT_RETURN_IF_FAILED(device.SetObjectName(semaphore, VK_OBJECT_TYPE_SEMAPHORE, name.c_str()));
        }
    }
#ifdef TKIT_ENABLE_INFO_LOGS
    const auto &qptype = GetDevice().GetInfo().QueuesPerType;
    for (u32 i = 0; i < qptype.GetSize(); ++i)
    {
        const auto &qs = qptype[i];
        TKIT_LOG_INFO("[ONYX][EXECUTION] {} '{}' queue(s) have been retrieved from the device", qs.GetSize(),
                      VKit::ToString(VKit::QueueType(i)));
    }
#endif
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
    const auto &queues = GetDevice().GetInfo().Queues;
    for (VKit::Queue *q : queues)
    {
        TKIT_RETURN_IF_FAILED(q->UpdateCompletedTimeline());
    }
    return Result<>::Ok();
}

VKit::Queue *FindSuitableQueue(const VKit::QueueType type)
{
    const auto &queues = GetDevice().GetInfo().QueuesPerType[type];
    u64 pending = TKIT_U64_MAX;
    VKit::Queue *queue = nullptr;
    for (VKit::Queue *q : queues)
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
    return GetDevice().GetInfo().PhysicalDevice->GetInfo().FamilyIndices[type];
}

VKit::CommandPool &GetTransientGraphicsPool()
{
    return *s_Graphics;
}
VKit::CommandPool &GetTransientTransferPool()
{
    return *s_Transfer;
}

} // namespace Onyx::Execution
