#include "onyx/core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/glfw.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/imgui/implot.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/backend.hpp"
#endif
#include "tkit/profiling/macros.hpp"
#include "tkit/utils/debug.hpp"

namespace Onyx
{
using WindowData = Application::WindowData;
void Application::Quit()
{
    setFlags(ApplicationFlag_Quit);
}

void Application::Run()
{
    TKit::Clock clock;
    while (NextFrame(clock))
        ;
}

#ifdef ONYX_ENABLE_IMGUI
void Application::applyTheme(const WindowData &data)
{
    TKIT_ASSERT(data.Theme, "[ONYX][APPLICATION] No theme has been set for the given windos. Set one with SetTheme()");
    ImGuiContext *pcontext = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(data.ImGuiContext);
    data.Theme->Apply();
    ImGui::SetCurrentContext(pcontext);
}

void Application::ApplyTheme(const Window *window)
{
    const WindowData *data = getWindowData(window);
    applyTheme(*data);
}

static void beginRenderImGui()
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::BeginRenderImGui");
    NewImGuiFrame();
}

static void endRenderImGui(const VkCommandBuffer commandBuffer)
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::EndRenderImGui");
    ImGui::Render();
    RenderImGuiData(ImGui::GetDrawData(), commandBuffer);
    RenderImGuiWindows();
}

void Application::setUpdateDeltaTime(const TKit::Timespan target, WindowData &data)
{
    data.UpdateClock.Delta.Time.Target = target;
}
void Application::setRenderDeltaTime(const TKit::Timespan target, WindowData &data)
{
    TKIT_LOG_WARNING_IF(
        data.Window->IsVSync(),
        "[ONYX][APPLICATION] When the present mode of the window is FIFO (V-Sync), setting the target delta "
        "time for said window is useless");
    if (!data.Window->IsVSync())
        data.RenderClock.Delta.Time.Target = target;
}

Application::WindowData *Application::getWindowData(const Window *window)
{
    if (m_MainWindow.Window == window)
        return &m_MainWindow;
#    ifdef __ONYX_MULTI_WINDOW
    for (WindowData &data : m_Windows)
        if (data.Window == window)
            return &data;
#    endif
    TKIT_FATAL("[ONYX][APPLICATION] No window data found for '{}' window", window->GetName());
    return nullptr;
}
const Application::WindowData *Application::getWindowData(const Window *window) const
{
    if (m_MainWindow.Window == window)
        return &m_MainWindow;
#    ifdef __ONYX_MULTI_WINDOW
    for (const WindowData &data : m_Windows)
        if (data.Window == window)
            return &data;
#    endif
    TKIT_FATAL("[ONYX][APPLICATION] No window data found for '{}' window", window->GetName());
    return nullptr;
}

static void initializeImGui(WindowData &data)
{
    Window *window = data.Window;
    TKIT_ASSERT(!data.CheckFlags(ApplicationFlag_ImGuiRunning),
                "[ONYX][APPLICATION] Trying to initialize ImGui for window '{}' when it is already running. If you "
                "meant to reload ImGui, use ReloadImGui()",
                window->GetName());

    IMGUI_CHECKVERSION();
    if (!data.Theme)
        data.Theme = new BabyTheme;

    data.ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(data.ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    data.ImPlotContext = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(data.ImPlotContext);
#    endif

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags = data.ImGuiConfigFlags;
    InitializeImGui(window);
    ImFont *font = io.Fonts->AddFontFromFileTTF(ONYX_ROOT_PATH "/onyx/fonts/OpenSans-Regular.ttf", 16.f);
    io.FontDefault = font;
    data.Theme->Apply();

    data.SetFlags(ApplicationFlag_ImGuiRunning);
}

static void shutdownImGui(WindowData &data)
{
    TKIT_ASSERT(data.CheckFlags(ApplicationFlag_ImGuiRunning),
                "[ONYX][APPLICATION] Trying to shut down ImGui when it is not initialized to begin with");

    data.ClearFlags(ApplicationFlag_ImGuiRunning);

    ImGui::SetCurrentContext(data.ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(data.ImPlotContext);
#    endif

    ShutdownImGui();

    ImGui::DestroyContext(data.ImGuiContext);
    data.ImGuiContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::DestroyContext(data.ImPlotContext);
    data.ImPlotContext = nullptr;
#    endif
}

static bool displayDeltaTime(Window *window, DeltaInfo &delta, const UserLayerFlags flags, const bool checkVSync)
{
    DeltaTime &time = delta.Time;

    if (time.Measured > delta.Max)
        delta.Max = time.Measured;
    delta.Smoothed = delta.Smoothness * delta.Smoothed + (1.f - delta.Smoothness) * time.Measured;

    ImGui::SliderFloat("Smoothing factor", &delta.Smoothness, 0.f, 0.999f);
    if (flags & UserLayerFlag_DisplayHelp)
        UserLayer::HelpMarkerSameLine(
            "Because frames get dispatched so quickly, the frame time can vary a lot, be inconsistent, and hard to "
            "see. This slider allows you to smooth out the frame time across frames, making it easier to see the "
            "trend.");

    ImGui::Combo("Unit", &delta.Unit, "s\0ms\0us\0ns\0");
    const u32 mfreq = ToFrequency(delta.Smoothed);
    u32 tfreq = ToFrequency(delta.Time.Target);

    bool changed = false;
    if (checkVSync && window->IsVSync())
        ImGui::Text("Target hertz: %u", tfreq);
    else
    {
        changed = ImGui::Checkbox("Limit hertz", &delta.Limit);
        if (changed)
            delta.Time.Target = delta.Limit ? window->GetMonitorDeltaTime() : TKit::Timespan{};

        if (delta.Limit)
        {
            const u32 mn = 30;
            const u32 mx = 240;
            if (ImGui::SliderScalarN("Target hertz", ImGuiDataType_U32, &tfreq, 1, &mn, &mx))
            {
                delta.Time.Target = ToDeltaTime(tfreq);
                changed = true;
            }
        }
    }
    ImGui::Text("Measured hertz: %u", mfreq);

    if (delta.Unit == 0)
        ImGui::Text("Measured delta time: %.4f s (max: %.4f s)", delta.Smoothed.AsSeconds(), delta.Max.AsSeconds());
    else if (delta.Unit == 1)
        ImGui::Text("Measured delta time: %.2f ms (max: %.2f ms)", delta.Smoothed.AsMilliseconds(),
                    delta.Max.AsMilliseconds());
    else if (delta.Unit == 2)
        ImGui::Text("Measured delta time: %u us (max: %u us)", static_cast<u32>(delta.Smoothed.AsMicroseconds()),
                    static_cast<u32>(delta.Max.AsMicroseconds()));
    else
#    ifndef TKIT_OS_LINUX
        ImGui::Text("Measured delta time: %llu ns (max: %llu ns)", delta.Smoothed.AsNanoseconds(),
                    delta.Max.AsNanoseconds());
#    else
        ImGui::Text("Measured delta time: %lu ns (max: %lu ns)", delta.Smoothed.AsNanoseconds(),
                    delta.Max.AsNanoseconds());
#    endif

    if (flags & UserLayerFlag_DisplayHelp)
        UserLayer::HelpMarkerSameLine(
            "The delta time is a measure of the time it takes to complete a frame loop around a particular callback "
            "(which can be an update or render callback), and it is one of the main indicators of an application "
            "smoothness. It is also used to calculate the frames per second (FPS) of the application. A good frame "
            "time is usually no larger than 16.67 ms (that is, 60 fps). It is also bound to the present mode of the "
            "window.");

    if (ImGui::Button("Reset maximum"))
        delta.Max = TKit::Timespan{};
    return changed;
}

static bool displayDeltaTime(WindowData &data, const UserLayerFlags flags)
{
    const auto sync = [](DeltaInfo &dst, const DeltaInfo &src) {
        dst.Time.Target = src.Time.Target;
        dst.Smoothness = src.Smoothness;
        dst.Unit = src.Unit;
        dst.Limit = src.Limit;
    };
    ImGui::Text("Delta time");
    bool changed = ImGui::Button("Sync");
    if (changed)
        sync(data.UpdateClock.Delta, data.RenderClock.Delta);

    if (!data.Window->IsVSync())
    {
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Mirror", &data.MirrorDeltas);
    }
    const bool mirror = !data.Window->IsVSync() && data.MirrorDeltas;
    if (ImGui::TreeNode("Render"))
    {
        if (displayDeltaTime(data.Window, data.RenderClock.Delta, flags, true))
        {
            if (mirror)
                sync(data.UpdateClock.Delta, data.RenderClock.Delta);
            changed = true;
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Update"))
    {
        if (displayDeltaTime(data.Window, data.UpdateClock.Delta, flags, false))
        {
            if (mirror)
                sync(data.RenderClock.Delta, data.UpdateClock.Delta);
            changed = true;
        }
        ImGui::TreePop();
    }
    return changed;
}

bool Application::DisplayDeltaTime(const UserLayerFlags flags)
{
    return displayDeltaTime(m_MainWindow, flags);
}
bool Application::DisplayDeltaTime(const Window *window, const UserLayerFlags flags)
{
    WindowData *data = getWindowData(window);
    return displayDeltaTime(*data, flags);
}

bool Application::enableImGui(WindowData &data, const i32 flags)
{
    data.ImGuiConfigFlags = flags;
    if (checkFlags(ApplicationFlag_Defer))
    {
        data.SetFlags(ApplicationFlag_MustDisableImGui);
        return false;
    }
    shutdownImGui(data);
    data.ClearFlags(ApplicationFlag_MustDisableImGui | ApplicationFlag_ImGuiEnabled);
    return true;
}

bool Application::EnableImGui(const Window *window, const i32 flags)
{
    WindowData *data = getWindowData(window);
    return enableImGui(*data, flags);
}

bool Application::EnableImGui(const i32 flags)
{
    return enableImGui(m_MainWindow, flags);
}

bool Application::DisableImGui(const Window *window)
{
    WindowData *data = window ? getWindowData(window) : &m_MainWindow;
    if (checkFlags(ApplicationFlag_Defer))
    {
        data->SetFlags(ApplicationFlag_MustDisableImGui);
        return false;
    }
    shutdownImGui(*data);
    data->ClearFlags(ApplicationFlag_MustDisableImGui | ApplicationFlag_ImGuiEnabled);
    return true;
}

bool Application::reloadImGui(WindowData &data, const i32 flags)
{
    data.ImGuiConfigFlags = flags;
    if (checkFlags(ApplicationFlag_Defer))
    {
        data.SetFlags(ApplicationFlag_MustDisableImGui | ApplicationFlag_MustEnableImGui);
        return false;
    }
    shutdownImGui(data);
    initializeImGui(data);
    return true;
}

bool Application::ReloadImGui(const i32 flags)
{
    return reloadImGui(m_MainWindow, flags);
}
bool Application::ReloadImGui(const Window *window, const i32 flags)
{
    WindowData *data = window ? getWindowData(window) : &m_MainWindow;
    return reloadImGui(*data, flags);
}

#endif

static void endFrame()
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
    const i64 drawCalls = static_cast<i64>(Detail::GetDrawCallCount());
    TKIT_PROFILE_PLOT("Draw calls", static_cast<i64>(drawCalls));
    Detail::ResetDrawCallCount();
#endif
    TKIT_PROFILE_MARK_FRAME();
}

void Application::processWindow(WindowData &data)
{
    if (data.UpdateClock.IsDue())
    {
        data.UpdateClock.Update();
        if (data.Layer)
            data.Layer->OnUpdate(data.UpdateClock.Delta.Time);
    }

    if (!data.RenderClock.IsDue())
        return;
    const FrameInfo info = data.Window->BeginFrame(WaitMode::Poll);
    if (!info)
        return;

    data.RenderClock.Update();
#ifdef ONYX_ENABLE_IMGUI
    TKIT_ASSERT(!data.CheckFlags(ApplicationFlag_ImGuiEnabled) || data.CheckFlags(ApplicationFlag_ImGuiRunning),
                "[ONYX][APPLICATION] ImGui is enabled for window '{}' but no instance of "
                "ImGui is running. This should not be possible",
                data.Window->GetName());

    TKIT_ASSERT(data.CheckFlags(ApplicationFlag_ImGuiEnabled) || !data.CheckFlags(ApplicationFlag_ImGuiRunning),
                "[ONYX][APPLICATION] ImGui is disabled for window '{}' but an instance of "
                "ImGui is running. This should not be possible",
                data.Window->GetName());

    ImGui::SetCurrentContext(data.ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(data.ImPlotContext);
#    endif
    if (data.CheckFlags(ApplicationFlag_ImGuiEnabled))
        beginRenderImGui();
#endif

    for (const Event &event : data.Window->GetNewEvents())
    {
        if (data.Window->IsVSync() && (event.Type == Event_SwapChainRecreated || event.Type == Event_WindowMoved))
        {
            data.RenderClock.Delta.Time.Target = data.Window->GetMonitorDeltaTime();
            data.RenderClock.Delta.Limit = true;
        }

        if (data.Layer)
            data.Layer->OnEvent(event);
    }

    data.Window->FlushEvents();
    if (data.Layer)
        data.Layer->OnFrameBegin(data.RenderClock.Delta.Time, info);

    const VkPipelineStageFlags transferFlags = data.Window->SubmitContextData(info);

    data.Window->BeginRendering();
    if (data.Layer)
        data.Layer->OnRenderBegin(data.RenderClock.Delta.Time, info);

    data.Window->Render(info);
#ifdef ONYX_ENABLE_IMGUI
    if (data.CheckFlags(ApplicationFlag_ImGuiEnabled))
        endRenderImGui(info.GraphicsCommand);
#endif

    if (data.Layer)
        data.Layer->OnRenderEnd(data.RenderClock.Delta.Time, info);
    data.Window->EndRendering();

    if (data.Layer)
        data.Layer->OnFrameEnd(data.RenderClock.Delta.Time, info);
    data.Window->EndFrame(transferFlags);
}

static void syncDeferredOperations(WindowData &data)
{
    if (data.CheckFlags(ApplicationFlag_MustDestroyLayer))
    {
        delete data.Layer;
        data.Layer = nullptr;
        data.ClearFlags(ApplicationFlag_MustDestroyLayer);
    }
    if (data.CheckFlags(ApplicationFlag_MustReplaceLayer))
    {
        data.Layer = data.StagedLayer;
        data.StagedLayer = nullptr;
        data.ClearFlags(ApplicationFlag_MustReplaceLayer);
    }

#ifdef ONYX_ENABLE_IMGUI

    if (data.CheckFlags(ApplicationFlag_MustDisableImGui))
    {
        shutdownImGui(data);
        data.ClearFlags(ApplicationFlag_MustDisableImGui | ApplicationFlag_ImGuiEnabled);
    }

    if (data.CheckFlags(ApplicationFlag_MustEnableImGui))
    {
        initializeImGui(data);
        data.ClearFlags(ApplicationFlag_MustEnableImGui);
        data.SetFlags(ApplicationFlag_ImGuiEnabled);
    }
#endif
}

void Application::syncDeferredOperations()
{
    ::Onyx::syncDeferredOperations(m_MainWindow);
#ifdef __ONYX_MULTI_WINDOW
    for (WindowData &data : m_Windows)
        ::Onyx::syncDeferredOperations(data);

    for (const WindowSpecs &specs : m_WindowsToAdd)
        openWindow(specs);
    m_WindowsToAdd.Clear();

    for (u32 i = m_Windows.GetSize() - 1; i < m_Windows.GetSize(); --i)
        if (m_Windows[i].Window->ShouldClose())
        {
            destroyWindow(m_Windows[i]);
            m_Windows.RemoveUnordered(m_Windows.begin() + i);
        }
#endif
    if (m_MainWindow.Window->ShouldClose())
    {
        destroyWindow(m_MainWindow);
#ifdef __ONYX_MULTI_WINDOW
        if (!m_Windows.IsEmpty())
        {
            m_MainWindow = m_Windows[0];
            m_Windows.RemoveUnordered(m_Windows.begin());
        }
#endif
    }

    if (checkFlags(ApplicationFlag_Quit)) [[unlikely]]
    {
        closeAllWindows();
        clearFlags(ApplicationFlag_Quit);
    }
}

bool Application::NextFrame(TKit::Clock &clock)
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::NextFrame");

    setFlags(ApplicationFlag_Defer);
    Input::PollEvents();

    processWindow(m_MainWindow);
#ifdef __ONYX_MULTI_WINDOW
    for (WindowData &data : m_Windows)
        processWindow(data);
#endif
    clearFlags(ApplicationFlag_Defer);
    syncDeferredOperations();
    endFrame();

    const auto computeSleep = [](const WindowData &data) {
        return Math::Min(data.RenderClock.Delta.Time.Target - data.RenderClock.Clock.GetElapsed(),
                         data.UpdateClock.Delta.Time.Target - data.UpdateClock.Clock.GetElapsed());
    };

    TKit::Timespan sleep = computeSleep(m_MainWindow);
#ifdef __ONYX_MULTI_WINDOW
    for (WindowData &data : m_Windows)
    {
        const TKit::Timespan s = computeSleep(data);
        if (s < sleep)
            sleep = s;
    }
#endif
    {
        TKIT_PROFILE_NSCOPE("Onyx::Application::Sleep");
        TKit::Timespan::Sleep(sleep);
    }
    m_DeltaTime = clock.Restart();
    return m_MainWindow.Window;
}

Application::WindowData Application::createWindow(const WindowSpecs &specs)
{
    WindowData data;
    data.Window = m_WindowAllocator.Create<Window>(specs.Specs);
    data.RenderClock.Delta.Time.Target = data.Window->GetMonitorDeltaTime();
    data.RenderClock.Delta.Limit = true;
    data.UpdateClock = data.RenderClock;

#ifdef ONYX_ENABLE_IMGUI
    if (specs.EnableImGui)
    {
        initializeImGui(data);
        data.SetFlags(ApplicationFlag_ImGuiEnabled);
    }
#endif

    return data;
}

void Application::destroyWindow(WindowData &data)
{
    delete data.StagedLayer;
    delete data.Layer;

    data.StagedLayer = nullptr;
    data.Layer = nullptr;
#ifdef ONYX_ENABLE_IMGUI
    if (data.CheckFlags(ApplicationFlag_ImGuiEnabled))
        shutdownImGui(data);
#endif
    m_WindowAllocator.Destroy(data.Window);
    data.Window = nullptr;
}

void Application::closeAllWindows()
{
    if (m_MainWindow.Window)
        destroyWindow(m_MainWindow);
#ifdef __ONYX_MULTI_WINDOW
    for (WindowData &data : m_Windows)
        destroyWindow(data);
    m_Windows.Clear();
#endif
}

#ifdef __ONYX_MULTI_WINDOW
bool Application::CloseWindow(Window *window)
{
    if (checkFlags(ApplicationFlag_Defer))
    {
        window->FlagShouldClose();
        return false;
    }
    if (m_MainWindow.Window == window)
    {
        destroyWindow(m_MainWindow);
        return true;
    }
    for (u32 i = 0; i < m_Windows.GetSize(); ++i)
        if (m_Windows[i].Window == window)
        {
            destroyWindow(m_Windows[i]);
            m_Windows.RemoveUnordered(m_Windows.begin() + i);
            return true;
        }
    TKIT_FATAL("[ONYX][APPLICATION] Failed to close window: Window '{}' not found", window->GetName());
    return false;
}
Window *Application::openWindow(const WindowSpecs &specs)
{
    const WindowData &data = m_Windows.Append(createWindow(specs));
    if (specs.CreationCallback)
        specs.CreationCallback(data.Window);
    return data.Window;
}
#endif

} // namespace Onyx
