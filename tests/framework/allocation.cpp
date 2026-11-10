#include "alias.hpp"
#include "tkit/memory/tier_allocator.hpp"
#include "tkit/preprocessor/system.hpp"

namespace OnyxTest
{
void PushThreadAllocator(const usz maxAllocation)
{
    static thread_local bool pushed = false;
    if (pushed)
        return;
    pushed = true;

    // one description for every thread: building one checks the whole layout in debug builds, which takes a moment for
    // large allocations. a function local static is initialized once, whichever thread gets here first
    static const TKit::TierDescriptions description{{.MaxAllocation = maxAllocation, .TierSlotDecay = 0.8f}};
    // never destroyed, as Onyx does with its own: a container may outlive the thread that filled it, and workers end
    // with _Exit anyway. aligned for anything Onyx aligns to a cache line
    TKit::PushTier(new TKit::TierAllocator(description, TKIT_CACHE_LINE_SIZE));
}
} // namespace OnyxTest
