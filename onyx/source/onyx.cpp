#include "pch.hpp"
#include "onyx/onyx.hpp"
#include "platform.hpp"
#include "renderer.hpp"
#include "resources.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "imgui_backend.hpp"
#    include <imgui.h>
#    ifdef ONYX_ENABLE_IMPLOT
#        include <implot.h>
#    endif
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
    TKit::Timespan DeltaError{};
#ifdef ONYX_ENABLE_IMGUI
    ImGuiContext *ImContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlotContext *ImpContext = nullptr;
#    endif
    bool ImGuiRendered = false;
#endif
    OpenWindowFlags Flags = 0;

    bool IsDue() const
    {
        return Clock.GetElapsed() >= DeltaTarget - DeltaError;
    }
    void MarkTick()
    {
        DeltaTime = Clock.Restart();
        const f32 smoothness = 0.5f;
        const TKit::Timespan error = DeltaTime - DeltaTarget;
        DeltaError = smoothness * DeltaError + (1.f - smoothness) * error;
    }
};

struct ApiData
{
    TKit::StaticArray<WindowData, ONYX_MAX_VIEWS> Windows{};
    TKit::StaticArray<RenderTexture *, ONYX_MAX_VIEWS> RenderTextures{};

    TKit::TierArray<RenderContext<D2> *> Contexts2{};
    TKit::TierArray<RenderContext<D3> *> Contexts3{};

    TKit::Clock FrameClock{};
    TKit::Clock TimeClock{};
    TKit::Timespan DeltaTime{};
    QuitFlags Quit = 0;
};

static TKit::Storage<ApiData> s_Data{};

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

TKit::Timespan GetTime()
{
    return s_Data->TimeClock.GetElapsed();
}
TKit::Timespan GetDeltaTime()
{
    return s_Data->DeltaTime;
}
TKit::Timespan GetDeltaTime(const Window *win)
{
    return s_Data->Windows[getWindowIndex(win)].DeltaTime;
}
TKit::Timespan GetTargetDeltaTime(const Window *win)
{
    return s_Data->Windows[getWindowIndex(win)].DeltaTarget;
}
void SetTargetDeltaTime(Window *win, const TKit::Timespan target)
{
    s_Data->Windows[getWindowIndex(win)].DeltaTarget = target;
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

    wdata.Flags = specs.Flags;
    if (specs.Window.PresentMode == PresentMode_Immediate && !(specs.Flags & OpenWindowFlag_NoDefaultDeltaTime))
        wdata.DeltaTarget = wdata.Window->GetMonitorDeltaTime();

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

template <Dimension D> RenderContext<D> *CreateRenderContext(const u32 immediateDynamicMeshCapacity)
{
    return Renderer::CreateContext<D>(immediateDynamicMeshCapacity);
}
template <Dimension D> void DestroyRenderContext(RenderContext<D> *ctx)
{
    Renderer::DestroyContext<D>(ctx);
}

RenderTexture *CreateRenderTexture(const u32v2 &dimensions)
{
    TKit::TierAllocator *tier = GetTier();
    RenderTexture *rtex = tier->Create<RenderTexture>(dimensions);
    return s_Data->RenderTextures.Append(rtex);
}

void DestroyRenderTexture(RenderTexture *rtex)
{
    for (u32 i = 0; i < s_Data->RenderTextures.GetSize(); ++i)
    {
        RenderTexture *r = s_Data->RenderTextures[i];
        if (r == rtex)
        {
            TKit::TierAllocator *tier = GetTier();
            tier->Destroy(r);
            s_Data->RenderTextures.RemoveUnordered(s_Data->RenderTextures.begin() + i);
            return;
        }
    }
    TKIT_FATAL("[ONYX] Failed to find render texture to destroy. Ensure the render texture was created with the "
               "CreateRenderTexture() function");
}

bool Running()
{
    // a bit weird to have it here, but it works
    Platform::PollEvents();
    //
    if (s_Data->Quit & QuitFlag_Quit)
    {
        if (s_Data->Quit & QuitFlag_DestroyWindows)
            for (u32 i = s_Data->Windows.GetSize() - 1; i < s_Data->Windows.GetSize(); --i)
                if (!(s_Data->Windows[i].Flags & OpenWindowFlag_DoNotDestroyOnQuit))
                    closeWindow(i);

        s_Data->Quit = 0;
        return false;
    }
    return !s_Data->Windows.IsEmpty();
}
void Quit(const QuitFlags flags)
{
    s_Data->Quit = flags | QuitFlag_Quit;
}

void Transfer(const TransferInfo &info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Transfer");
    Execution::UpdateCompletedQueueTimelines();

    static u64 transferCount = 0;
    if (info.CoalescePeriod != 0 && (transferCount++ % info.CoalescePeriod) == 0)
        Renderer::Coalesce();

    VKit::Queue *tqueue = Execution::GetQueue(VKit::Queue_Transfer);
    CommandPool *tpool = Execution::FindAvailableCommandPool(VKit::Queue_Transfer);

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
            if (win->GetPresentMode() == PresentMode_VSync &&
                (event.Type == Event_SwapchainRecreated || event.Type == Event_WindowMoved))
                wdata.DeltaTarget = win->GetMonitorDeltaTime();

        win->FlushEvents();
        if (wdata.IsDue() && win->AcquireNextImage(info.AcquireImageTimeout))
        {
            wdata.MarkTick();
            AcquiredWindow &acwin = acqWindows.Append();
            acwin.Window = &wdata;
        }
    }

    // cant do it in the same loop because the texture handle offset buffer must be fully updated

    if (!acqWindows.IsEmpty() || !s_Data->RenderTextures.IsEmpty())
    {
        for (RenderTexture *rtex : s_Data->RenderTextures)
            rtex->FindAvailableImages();

        VKit::Queue *gqueue = Execution::GetQueue(VKit::Queue_Graphics);
        CommandPool *gpool = Execution::FindAvailableCommandPool(VKit::Queue_Graphics);

        TKit::StaticArray<RenderSubmitInfo, ONYX_MAX_VIEWS> rinfos{};
        Renderer::PrepareRender();

        bool once = true;
        for (RenderTexture *rtex : s_Data->RenderTextures)
        {
            const VkCommandBuffer cmd = Execution::Allocate(gpool);

            Execution::BeginCommandBuffer(cmd);
            if (once)
            {
                Resources::UpdateTextureIdOffsetBuffer(cmd);
                Renderer::ApplyAcquireBarriers(cmd);
                once = false;
            }
            const RenderSubmitInfo rinfo = Renderer::Render(gqueue, cmd, rtex);
            Execution::EndCommandBuffer(cmd);
            rinfos.Append(rinfo);
        }

#ifdef ONYX_ENABLE_IMGUI
        TKit::StaticArray<u32, ONYX_MAX_VIEWS> platformWindowIndices{};
        u32 platformWindowStart = 0;
        const auto multiViewports = [] { return ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable; };
#endif

        u32 maxFlight = 0;
        for (AcquiredWindow &acwin : acqWindows)
        {
            WindowData *wdata = acwin.Window;
            const VkCommandBuffer cmd = Execution::Allocate(gpool);

            Execution::BeginCommandBuffer(cmd);
            if (once)
            {
                Resources::UpdateTextureIdOffsetBuffer(cmd);
                Renderer::ApplyAcquireBarriers(cmd);
                once = false;
            }

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
            maxFlight = rinfo.InFlightValue;
            Execution::EndCommandBuffer(cmd);
            rinfos.Append(rinfo);

#ifdef ONYX_ENABLE_IMGUI
            if (wdata->ImContext && multiViewports())
            {
                acwin.PlatformWindowStart = platformWindowStart;
                for (u32 i = 0; i < ImGuiBackend::GetPlatformWindowCount(); ++i)
                    if (ImGuiBackend::AcquirePlatformWindowImage(i, info.AcquireImageTimeout))
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
        if (maxFlight != 0)
        {
            for (RenderTexture *rtex : s_Data->RenderTextures)
                rtex->MarkReadImageInUse({.Queue = gqueue, .InFlightValue = maxFlight});
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
    {
        const WindowData &wdata = s_Data->Windows[i];
        if (!(wdata.Flags & OpenWindowFlag_DoNotDestroyOnShouldClose) && wdata.Window->ShouldClose())
            closeWindow(i);
    }

    if (!s_Data->Windows.IsEmpty())
    {
        const auto computeSleep = [](WindowData &wdata) {
            return wdata.DeltaTarget - wdata.Clock.GetElapsed() - wdata.DeltaError;
        };
        TKit::Timespan sleep = computeSleep(s_Data->Windows[0]);
        for (u32 i = 1; i < s_Data->Windows.GetSize(); ++i)
        {
            const TKit::Timespan s = computeSleep(s_Data->Windows[i]);
            if (s < sleep)
                sleep = s;
        }
        if (sleep > TKit::Timespan{})
        {
            TKIT_PROFILE_NSCOPE("Onyx::Application::Sleep");
            TKit::Timespan::Sleep(sleep);
        }
    }
    s_Data->DeltaTime = s_Data->FrameClock.Restart();
    TKIT_PROFILE_MARK_FRAME();
}

void InitializeApi()
{
    s_Data.Construct();
}
void TerminateApi()
{
    TKit::TierAllocator *tier = GetTier();
    for (WindowData &wdata : s_Data->Windows)
        cleanupWindowData(wdata);

    for (RenderTexture *rtex : s_Data->RenderTextures)
        tier->Destroy(rtex);
    s_Data.Destruct();
}

template RenderContext<D2> *CreateRenderContext(u32 immediateDynamicMeshCapacity);
template RenderContext<D3> *CreateRenderContext(u32 immediateDynamicMeshCapacity);

template void DestroyRenderContext(RenderContext<D2> *ctx);
template void DestroyRenderContext(RenderContext<D3> *ctx);

} // namespace Onyx
