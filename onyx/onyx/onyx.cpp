#include "onyx/core/pch.hpp"
#include "onyx/onyx.hpp"
#include "onyx/platform/platform.hpp"
#include "onyx/rendering/renderer.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/imgui.hpp"
#    include "onyx/imgui/backend.hpp"
#    include "onyx/imgui/theme.hpp"
#endif
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
struct WindowData
{
    Window *Window = nullptr;
    TKit::Clock Clock{};
    TKit::Timespan DeltaTarget{};
    TKit::Timespan DeltaTime{};
#ifdef ONYX_ENABLE_IMGUI
    ImGuiContext *ImContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlotContext *ImpContext = nullptr;
#    endif
    bool ImGuiRendered = false;
#endif

    bool IsDue() const
    {
        return Clock.GetElapsed() >= DeltaTarget;
    }
    void MarkTick()
    {
        DeltaTime = Clock.Restart();
    }
};

struct ApiData
{
    TKit::StaticArray<WindowData, ONYX_MAX_VIEWS> Windows{};

    TKit::TierArray<RenderContext<D2> *> Contexts2{};
    TKit::TierArray<RenderContext<D3> *> Contexts3{};

    TKit::Clock Clock{};
    TKit::Timespan DeltaTime{};

    template <Dimension D> auto &GetContexts()
    {
        if constexpr (D == D2)
            return Contexts2;
        else
            return Contexts3;
    }
};

static TKit::Storage<ApiData> s_Data{};

void InitializeApi()
{
    s_Data.Construct();
}
void TerminateApi()
{
    s_Data.Destruct();
}

#ifdef ONYX_ENABLE_IMGUI
static void initializeImGui(WindowData &wdata)
{
    TKIT_ASSERT(!wdata.ImContext,
                "[ONYX][APPLICATION] Trying to initialize ImGui for window '{}' when it is already enabled. If you "
                "meant to reload ImGui, use ReloadImGui()",
                wdata.Window->GetTitle());

    IMGUI_CHECKVERSION();

    if (wdata.ImContext)
        ImGui::DestroyContext(wdata.ImContext);

#    ifdef ONYX_ENABLE_IMPLOT
    if (wdata.ImpContext)
        ImPlot::DestroyContext(wdata.ImpContext);
#    endif

    wdata.ImContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(wdata.ImContext);
#    ifdef ONYX_ENABLE_IMPLOT
    wdata.ImpContext = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(wdata.ImpContext);
#    endif

    ImGuiBackend::Create(wdata.Window);
    ImGuiIO &io = ImGui::GetIO();
    ImFont *font = io.Fonts->AddFontFromFileTTF(ONYX_ROOT_PATH "/onyx/fonts/OpenSans-Regular.ttf", 16.f);
    io.FontDefault = font;
    ApplyImGuiTheme(ImGuiTheme_Baby);
}
static void shutdownImGui(WindowData &wdata)
{
    TKIT_ASSERT(
        wdata.ImContext,
        "[ONYX][APPLICATION] Trying to shut down ImGui for window '{}' when it is not initialized to begin with",
        wdata.Window->GetTitle());

    ImGui::SetCurrentContext(wdata.ImContext);
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(wdata.ImpContext);
#    endif

    ImGuiBackend::Destroy();

    ImGui::DestroyContext(wdata.ImContext);
    wdata.ImContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::DestroyContext(wdata.ImpContext);
    wdata.ImpContext = nullptr;
#    endif
}
#endif

static u32 getWindowIndex(const Window *window)
{
    for (u32 i = 0; i < s_Data->Windows.GetSize(); ++i)
        if (s_Data->Windows[i].Window == window)
            return i;

    TKIT_FATAL("[ONYX] Failed to find window '{}'. Ensure any window you attempt to close was opened with the "
               "OpenWindow() function, not the Platform::CreateWindow() one",
               window->GetTitle());
    return TKIT_U32_MAX;
}

static void cleanupWindowData(WindowData &wdata)
{
#ifdef ONYX_ENABLE_IMGUI
    if (wdata.ImContext)
        shutdownImGui(wdata);
#endif
    Platform::DestroyWindow(wdata.Window);
}

static void closeWindow(const u32 idx)
{
    cleanupWindowData(s_Data->Windows[idx]);
    s_Data->Windows.RemoveOrdered(s_Data->Windows.begin() + idx);
}

Window *OpenWindow(const OpenWindowSpecs &specs)
{
    WindowData &wdata = s_Data->Windows.Append();
    wdata.Window = Platform::CreateWindow(specs.Window);
#ifdef ONYX_ENABLE_IMGUI
    if (specs.Flags & OpenWindowFlag_EnableImGui)
        initializeImGui(wdata);
#endif
    return wdata.Window;
}
void CloseWindow(Window *window)
{
    const u32 idx = getWindowIndex(window);
    closeWindow(idx);
}

#ifdef ONYX_ENABLE_IMGUI
bool CanRenderImGui(Window *window)
{
    WindowData &wdata = s_Data->Windows[getWindowIndex(window)];
    TKIT_ASSERT(
        wdata.ImContext,
        "[ONYX] ImGui is not enabled for window '{}'. Make sure to set the appropiate flags when opening a window",
        wdata.Window->GetTitle());

    if (wdata.ImGuiRendered)
        return false;

    ImGui::SetCurrentContext(wdata.ImContext);
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(wdata.ImpContext);
#    endif

    ImGuiBackend::NewFrame();
    wdata.ImGuiRendered = true;
    return true;
}
#endif

template <Dimension D> RenderContext<D> *CreateRenderContext()
{
    return Renderer::CreateContext<D>();
}
template <Dimension D> void DestroyRenderContex(RenderContext<D> *ctx)
{
    Renderer::DestroyContext<D>(ctx);
}

bool Running()
{
    return !s_Data->Windows.IsEmpty();
}
void Quit()
{
    for (WindowData &wdata : s_Data->Windows)
        cleanupWindowData(wdata);
    s_Data->Windows.Clear();
}

void Transfer(const TransferInfo &info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Transfer");
    Execution::UpdateCompletedQueueTimelines();

    static u64 transferCount = 0;
    if (info.CoalescePeriod != 0 && transferCount++ % info.CoalescePeriod)
        Renderer::Coalesce();

    VKit::Queue *tqueue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
    CommandPool *tpool = Execution::FindSuitableCommandPool(VKit::Queue_Transfer);

    const VkCommandBuffer cmd = Execution::Allocate(tpool);

    Execution::BeginCommandBuffer(cmd);
    const TransferSubmitInfo tinfo = Renderer::Transfer(tqueue, cmd);
    Execution::EndCommandBuffer(cmd);
    if (tinfo)
        Renderer::SubmitTransfer(tqueue, tpool, tinfo);
}
void Render(const RenderInfo &info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Render");
    // this will bee needed when render flags has more than the imgui flag
    // #ifdef ONYX_ENABLE_IMGUI
    //     TKIT_ASSERT(!(info.Flags & RenderFlag_ImGui),
    //                 "[ONYX] Cannot specify imgui in render flags. That specific flag will be handled internally");
    // #endif

    Platform::PollEvents();
    Execution::UpdateCompletedQueueTimelines();

    struct AcquiredWindow
    {
        WindowData *Window;
#ifdef ONYX_ENABLE_IMGUI
        u32 PlatformWindowStart = 0;
        u32 PlatformWindowEnd = 0;
#endif
    };
    const u32 winCount = s_Data->Windows.GetSize();
    TKit::StackArray<AcquiredWindow> acqWindows{};
    acqWindows.Reserve(winCount);

    for (WindowData &wdata : s_Data->Windows)
    {
        Window *win = wdata.Window;
        for (const Event &event : win->GetNewEvents())
            if (win->IsVSync() && (event.Type == Event_SwapChainRecreated || event.Type == Event_WindowMoved))
                wdata.DeltaTarget = win->GetMonitorDeltaTime();

        win->FlushEvents();
        if (wdata.IsDue() && win->AcquireNextImage(info.AcquireImageTimeout))
        {
            wdata.MarkTick();
            AcquiredWindow &acwin = acqWindows.Append();
            acwin.Window = &wdata;
        }
    }

    if (!acqWindows.IsEmpty())
    {
        VKit::Queue *gqueue = Execution::FindSuitableQueue(VKit::Queue_Graphics);
        CommandPool *gpool = Execution::FindSuitableCommandPool(VKit::Queue_Graphics);

#ifdef ONYX_ENABLE_IMGUI
        TKit::StaticArray<u32, ONYX_MAX_VIEWS> platformWindowIndices{};
        u32 platformWindowStart = 0;
        const auto multiViewports = [] { return ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable; };
#endif

        TKit::StaticArray<RenderSubmitInfo, ONYX_MAX_VIEWS> rinfos{};
        Renderer::PrepareRender();
        for (AcquiredWindow &acwin : acqWindows)
        {
            WindowData *wdata = acwin.Window;
            const VkCommandBuffer cmd = Execution::Allocate(gpool);

            Execution::BeginCommandBuffer(cmd);
            Renderer::ApplyAcquireBarriers(cmd);

            RenderFlags flags = 0;
#ifdef ONYX_ENABLE_IMGUI
            if (wdata->ImContext)
            {
                ImGui::SetCurrentContext(wdata->ImContext);
#    ifdef ONYX_ENABLE_IMPLOT
                ImPlot::SetCurrentContext(wdata->ImpContext);
#    endif
                if (wdata->ImGuiRendered)
                {
                    flags |= RenderFlag_ImGui;
                    wdata->ImGuiRendered = false;
                }
            }
#endif
            const RenderSubmitInfo rinfo = Renderer::Render(gqueue, cmd, wdata->Window, flags);
            Execution::EndCommandBuffer(cmd);
            rinfos.Append(rinfo);

#ifdef ONYX_ENABLE_IMGUI
            if (wdata->ImContext && multiViewports())
            {
                acwin.PlatformWindowStart = platformWindowStart;
                for (u32 i = 0; i < ImGuiBackend::GetPlatformWindowCount(); ++i)
                    if (ImGuiBackend::AcquirePlatformWindowImage(i, Poll))
                    {
                        platformWindowIndices.Append(i);
                        ++platformWindowStart;
                    }

                acwin.PlatformWindowEnd = platformWindowStart;
                for (u32 i = acwin.PlatformWindowStart; i < acwin.PlatformWindowEnd; ++i)
                {
                    const u32 windowIndex = platformWindowIndices[i];
                    const VkCommandBuffer plcmd = Execution::Allocate(gpool);

                    Execution::BeginCommandBuffer(plcmd);
                    rinfos.Append(ImGuiBackend::RenderPlatformWindow(windowIndex, gqueue, plcmd));
                    Execution::EndCommandBuffer(plcmd);
                }
            }
#endif
        }

        Renderer::SubmitRender(gqueue, gpool, rinfos);

        for (const AcquiredWindow &acwin : acqWindows)
        {
            WindowData *wdata = acwin.Window;
            wdata->Window->Present();
#ifdef ONYX_ENABLE_IMGUI
            if (wdata->ImContext)
            {
                ImGui::SetCurrentContext(wdata->ImContext);
#    ifdef ONYX_ENABLE_IMPLOT
                ImPlot::SetCurrentContext(wdata->ImpContext);
#    endif
                if (multiViewports())
                    for (u32 i = acwin.PlatformWindowStart; i < acwin.PlatformWindowEnd; ++i)
                    {
                        const u32 windowIndex = platformWindowIndices[i];
                        ImGuiBackend::PresentPlatformWindow(windowIndex);
                    }
            }
#endif
        }
    }
    for (u32 i = 0; i < s_Data->Windows.GetSize(); ++i)
        if (s_Data->Windows[i].Window->ShouldClose())
            closeWindow(i);

    if (!s_Data->Windows.IsEmpty())
    {
        const auto computeSleep = [](WindowData &wdata) { return wdata.DeltaTarget - wdata.Clock.GetElapsed(); };
        TKit::Timespan sleep = computeSleep(s_Data->Windows[0]);
        for (u32 i = 1; i < s_Data->Windows.GetSize(); ++i)
        {
            const TKit::Timespan s = computeSleep(s_Data->Windows[i]);
            if (s < sleep)
                sleep = s;
        }
        {
            TKIT_PROFILE_NSCOPE("Onyx::Application::Sleep");
            TKit::Timespan::Sleep(sleep);
        }
    }
    s_Data->DeltaTime = s_Data->Clock.Restart();
}

template RenderContext<D2> *CreateRenderContext();
template RenderContext<D3> *CreateRenderContext();

} // namespace Onyx
