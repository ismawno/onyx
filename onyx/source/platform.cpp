#include "pch.hpp"
#include "platform.hpp"
#include "tkit/container/static_array.hpp"

namespace Onyx::Platform
{
static TKit::Storage<TKit::StaticArray<Window *, ONYX_MAX_VIEWS>> s_Windows{};

void Initialize()
{
    TKIT_LOG_INFO("[ONYX][PLATFORM] Initializing");
    s_Windows.Construct();
}

void Terminate()
{
    DestroyWindows();
    s_Windows.Destruct();
}

Window *CreateWindow(const WindowSpecs &specs)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Window *window = tier->Create<Window>(specs);
    s_Windows->Append(window);
    return window;
}

static void removeWindow(const Window *window)
{
    for (u32 i = 0; i < s_Windows->GetSize(); ++i)
        if (s_Windows->At(i) == window)
        {
            s_Windows->RemoveUnordered(s_Windows->begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][PLATFORM] Window '{}' not found", window->GetTitle());
}

void DestroyWindow(const Window *window)
{
    removeWindow(window);
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(window);
}

void DestroyWindows()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (const Window *window : *s_Windows)
        tier->Destroy(window);
    s_Windows->Clear();
}

} // namespace Onyx::Platform
