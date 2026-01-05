#include "onyx/core/pch.hpp"
#include "onyx/execution/queues.hpp"
#include "onyx/core/core.hpp"

namespace Onyx::Queues
{
struct CommandPools
{
    VKit::CommandPool Graphics{};
    VKit::CommandPool Transfer{};
};

static CommandPools s_Pools{};

static void createCommandPools()
{
    TKIT_LOG_INFO("[ONYX] Creating global command pools");
    const VKit::LogicalDevice &device = Core::GetDevice();

    const u32 gindex = GetFamilyIndex(VKit::Queue_Graphics);
    const u32 tindex = GetFamilyIndex(VKit::Queue_Transfer);

    const auto create = [=, &device] {
        CommandPools pools;
        auto poolres = VKit::CommandPool::Create(device, gindex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

        VKIT_CHECK_RESULT(poolres);
        pools.Graphics = poolres.GetValue();
        if (gindex != tindex)
        {
            poolres = VKit::CommandPool::Create(device, tindex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
            VKIT_CHECK_RESULT(poolres);
            pools.Transfer = poolres.GetValue();
        }
        else
            pools.Transfer = pools.Graphics;
        return pools;
    };

    s_Pools = create();
}

void Initialize()
{
    createCommandPools();
}
void Terminate()
{
    const auto destroy = [](CommandPools &p_Pools) {
        p_Pools.Graphics.Destroy();
        if (IsSeparateTransferMode())
            p_Pools.Transfer.Destroy();
    };
    destroy(s_Pools);
}
bool IsSeparateTransferMode()
{
    return GetFamilyIndex(VKit::Queue_Graphics) != GetFamilyIndex(VKit::Queue_Transfer);
}
u32 GetFamilyIndex(const VKit::QueueType p_Type)
{
    return Core::GetDevice().GetInfo().PhysicalDevice.GetInfo().FamilyIndices[p_Type];
}

VKit::Queue *GetQueue(const VKit::QueueType p_Type)
{
    const auto &queues = Core::GetDevice().GetInfo().Queues[p_Type];
    u64 pending = TKIT_U64_MAX;
    VKit::Queue *queue = nullptr;
    for (VKit::Queue *q : queues)
    {
        const auto result = q->GetPendingSubmissionCount();
        VKIT_CHECK_RESULT(result);
        const u64 p = result.GetValue();
        if (p < pending)
        {
            pending = p;
            queue = q;
        }
    }
    TKIT_ASSERT(queue, "[ONYX] Queue is null. Some pending submissions returned U64 max. This is bad...");
    return queue;
}

VKit::CommandPool &GetGraphicsPool()
{
    return s_Pools.Graphics;
}
VKit::CommandPool &GetTransferPool()
{
    return s_Pools.Transfer;
}

} // namespace Onyx::Queues
