#include "pch.hpp"
#include "onyx/specs.hpp"
#include "execution.hpp"
#include "graph/execution.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx::Execution
{
struct CommandPool
{
    Graph::CommandPool Pool;
    Graph::Tracker Tracker{};
};
static Graph::CommandPool s_Graphics = NullHandle;
static Graph::CommandPool s_Transfer = NullHandle;
static TKit::Storage<TKit::ArenaArray<CommandPool>> s_CommandPools{};

static void createTransientCommandPools()
{
    s_Graphics = Graph::CommandPool_Create(Graph::Queue_Graphics, Graph::CommandPoolFlag_CreateTransient);
    s_Transfer = Graph::CommandPool_Create(Graph::Queue_Transfer, Graph::CommandPoolFlag_CreateTransient);

    if (Graph::IsValidationEnabled())
    {
        Graph::CommandPool_SetName(s_Graphics, "onyx-transient-graphics-command-pool");
        Graph::CommandPool_SetName(s_Transfer, "onyx-transient-transfer-command-pool");
    }
}

Graph::CommandPool FindAvailableCommandPool(const Graph::QueueType queue)
{
    for (CommandPool &pool : *s_CommandPools)
        if (pool.Tracker.Queue == queue && !pool.Tracker.InUse())
        {
            Graph::CommandPool_Reset(pool.Pool);
            return pool.Pool;
        }

    CommandPool &pool = s_CommandPools->Append();
    pool.Pool = Graph::CommandPool_Create(queue, Graph::CommandPoolFlag_CreateTransient);
    pool.Tracker.Queue = queue;
    if (Graph::IsValidationEnabled())
    {
        const TKit::StackString name = TKit::StackString::Format("onyx-ring-command-pool-index-{}-type-{}",
                                                                 s_CommandPools->GetSize() - 1, u32(queue));
        Graph::CommandPool_SetName(pool.Pool, name.CString());
    }
    return pool.Pool;
}

void MarkInUse(const Graph::CommandPool pool, const u64 inFlightValue)
{
    for (CommandPool &p : *s_CommandPools)
        if (p.Pool == pool)
        {
            p.Tracker.InFlightValue = inFlightValue;
            return;
        }
    TKIT_FATAL("[ONYX][EXECUTION] Failed to find command pool with handle {:010x}", pool);
}

void Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][EXECUTION] Initializing");
    s_CommandPools.Construct();
    s_CommandPools->Reserve(specs.MaxCommandPools);
    createTransientCommandPools();
}
void Terminate()
{
    Graph::CommandPool_Destroy(s_Graphics);
    Graph::CommandPool_Destroy(s_Transfer);
    s_Graphics = Graph::NullHandle;
    s_Transfer = Graph::NullHandle;
    for (const CommandPool &pool : *s_CommandPools)
        Graph::CommandPool_Destroy(pool.Pool);

    s_CommandPools.Destruct();
}

Graph::CommandPool GetTransientGraphicsPool()
{
    return s_Graphics;
}
Graph::CommandPool GetTransientTransferPool()
{
    return s_Transfer;
}

} // namespace Onyx::Execution
