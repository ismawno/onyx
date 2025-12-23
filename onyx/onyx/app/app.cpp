#define ONYX_IMGUI_INCLUDE_BACKEND
#include "onyx/core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/glfw.hpp"
#include "onyx/core/core.hpp"
#include "onyx/imgui/imgui.hpp"
#include "onyx/imgui/implot.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/utils/debug.hpp"
#include "vkit/vulkan/loader.hpp"

namespace Onyx
{
using WindowData = Application::WindowData;
void Application::Quit()
{
    setFlags(Flag_Quit);
}

void Application::Run()
{
    TKit::Clock clock;
    while (NextFrame(clock))
        ;
}

#ifdef ONYX_ENABLE_IMGUI
void Application::applyTheme(const WindowData &p_Data)
{
    TKIT_ASSERT(p_Data.Theme, "[ONYX] No theme has been set for the given windos. Set one with SetTheme()");
    ImGuiContext *pcontext = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(p_Data.ImGuiContext);
    p_Data.Theme->Apply();
    ImGui::SetCurrentContext(pcontext);
}

void Application::ApplyTheme(const Window *p_Window)
{
    const WindowData *data = getWindowData(p_Window);
    applyTheme(*data);
}

static void beginRenderImGui()
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::BeginRenderImGui");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

static void endRenderImGui(const VkCommandBuffer p_CommandBuffer)
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::EndRenderImGui");
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    const ImGuiIO &io = ImGui::GetIO();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void Application::setUpdateDeltaTime(const TKit::Timespan p_Target, WindowData &p_Data)
{
    p_Data.UpdateClock.Delta.Time.Target = p_Target;
}
void Application::setRenderDeltaTime(const TKit::Timespan p_Target, WindowData &p_Data)
{
    TKIT_LOG_WARNING_IF(p_Data.Window->IsVSync(),
                        "[ONYX] When the present mode of the window is FIFO (V-Sync), setting the target delta "
                        "time for said window is useless");
    if (!p_Data.Window->IsVSync())
        p_Data.RenderClock.Delta.Time.Target = p_Target;
}

Application::WindowData *Application::getWindowData(const Window *p_Window)
{
    if (m_MainWindow.Window == p_Window)
        return &m_MainWindow;
#    ifdef __ONYX_MULTI_WINDOW
    for (WindowData &data : m_Windows)
        if (data.Window == p_Window)
            return &data;
#    endif
    TKIT_FATAL("[ONYX] No window data found for '{}' window", p_Window->GetName());
    return nullptr;
}
const Application::WindowData *Application::getWindowData(const Window *p_Window) const
{
    if (m_MainWindow.Window == p_Window)
        return &m_MainWindow;
#    ifdef __ONYX_MULTI_WINDOW
    for (const WindowData &data : m_Windows)
        if (data.Window == p_Window)
            return &data;
#    endif
    TKIT_FATAL("[ONYX] No window data found for '{}' window", p_Window->GetName());
    return nullptr;
}

static void initializeImGui(WindowData &p_Data)
{
    Window *window = p_Data.Window;
    TKIT_ASSERT(!p_Data.CheckFlags(Application::Flag_ImGuiRunning),
                "[ONYX] Trying to initialize ImGui for window '{}' when it is already running. If you "
                "meant to reload ImGui, use ReloadImGui()",
                window->GetName());

    IMGUI_CHECKVERSION();
    if (!p_Data.Theme)
        p_Data.Theme = new BabyTheme;

    p_Data.ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(p_Data.ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    p_Data.ImPlotContext = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(p_Data.ImPlotContext);
#    endif

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags = p_Data.ImGuiConfigFlags;

    TKIT_ASSERT_RETURNS(ImGui_ImplGlfw_InitForVulkan(window->GetWindowHandle(), true), true,
                        "[ONYX] Failed to initialize ImGui GLFW for window '{}'", window->GetName());

    const VKit::Instance &instance = Core::GetInstance();
    const VKit::LogicalDevice &device = Core::GetDevice();

    TKIT_LOG_WARNING_IF(
        (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) &&
            instance.GetInfo().Flags & VKit::Instance::Flag_HasValidationLayers,
        "[ONYX] Vulkan validation layers have become stricter regarding semaphore and fence usage when submitting to "
        "queues. ImGui may not have caught up to this and may trigger validation errors when the "
        "ImGuiConfigFlags_ViewportsEnable flag is set. If this is the case, either disable the flag or the vulkan "
        "validation layers. If the application runs well, you may safely ignore this warning");

    ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
    pipelineInfo.PipelineRenderingCreateInfo = window->GetFrameScheduler()->CreateSceneRenderInfo();
    pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    const VkSurfaceCapabilitiesKHR &sc =
        window->GetFrameScheduler()->GetSwapChain().GetInfo().SupportDetails.Capabilities;

    const u32 imageCount = sc.minImageCount != sc.maxImageCount ? sc.minImageCount + 1 : sc.minImageCount;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = instance.GetInfo().ApiVersion;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = device.GetInfo().PhysicalDevice;
    initInfo.Device = device;
    initInfo.Queue = window->GetFrameScheduler()->GetQueueData().Graphics->Queue;
    initInfo.QueueFamily = Core::GetFamilyIndex(VKit::Queue_Graphics);
    initInfo.DescriptorPoolSize = 100;
    initInfo.MinImageCount = sc.minImageCount;
    initInfo.ImageCount = imageCount;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain = pipelineInfo;

    TKIT_ASSERT_RETURNS(ImGui_ImplVulkan_LoadFunctions(instance.GetInfo().ApiVersion,
                                                       [](const char *p_Name, void *) -> PFN_vkVoidFunction {
                                                           return VKit::Vulkan::GetInstanceProcAddr(Core::GetInstance(),
                                                                                                    p_Name);
                                                       }),
                        true, "[ONYX] Failed to load ImGui Vulkan functions");

    TKIT_ASSERT_RETURNS(ImGui_ImplVulkan_Init(&initInfo), true,
                        "[ONYX] Failed to initialize ImGui Vulkan for window '{}'", window->GetName());

    ImFont *font = io.Fonts->AddFontFromFileTTF(ONYX_ROOT_PATH "/onyx/fonts/OpenSans-Regular.ttf", 16.f);
    io.FontDefault = font;
    p_Data.Theme->Apply();

    p_Data.SetFlags(Application::Flag_ImGuiRunning);
}

static void shutdownImGui(WindowData &p_Data)
{
    TKIT_ASSERT(p_Data.CheckFlags(Application::Flag_ImGuiRunning),
                "[ONYX] Trying to shut down ImGui when it is not initialized to begin with");

    p_Data.ClearFlags(Application::Flag_ImGuiRunning);

    ImGui::SetCurrentContext(p_Data.ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(p_Data.ImPlotContext);
#    endif

    Core::DeviceWaitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyPlatformWindows();
    ImGui::DestroyContext(p_Data.ImGuiContext);
    p_Data.ImGuiContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::DestroyContext(p_Data.ImPlotContext);
    p_Data.ImPlotContext = nullptr;
#    endif
}

static bool displayDeltaTime(Window *p_Window, DeltaInfo &p_Delta, const UserLayer::Flags p_Flags,
                             const bool p_CheckVSync)
{
    DeltaTime &time = p_Delta.Time;

    if (time.Measured > p_Delta.Max)
        p_Delta.Max = time.Measured;
    p_Delta.Smoothed = p_Delta.Smoothness * p_Delta.Smoothed + (1.f - p_Delta.Smoothness) * time.Measured;

    ImGui::SliderFloat("Smoothing factor", &p_Delta.Smoothness, 0.f, 0.999f);
    if (p_Flags & UserLayer::Flag_DisplayHelp)
        UserLayer::HelpMarkerSameLine(
            "Because frames get dispatched so quickly, the frame time can vary a lot, be inconsistent, and hard to "
            "see. This slider allows you to smooth out the frame time across frames, making it easier to see the "
            "trend.");

    ImGui::Combo("Unit", &p_Delta.Unit, "s\0ms\0us\0ns\0");
    const u32 mfreq = ToFrequency(p_Delta.Smoothed);
    u32 tfreq = ToFrequency(p_Delta.Time.Target);

    bool changed = false;
    if (p_CheckVSync && p_Window->IsVSync())
        ImGui::Text("Target hertz: %u", tfreq);
    else
    {
        changed = ImGui::Checkbox("Limit hertz", &p_Delta.Limit);
        if (changed)
            p_Delta.Time.Target = p_Delta.Limit ? p_Window->GetMonitorDeltaTime() : TKit::Timespan{};

        if (p_Delta.Limit)
        {
            const u32 mn = 30;
            const u32 mx = 240;
            if (ImGui::SliderScalarN("Target hertz", ImGuiDataType_U32, &tfreq, 1, &mn, &mx))
            {
                p_Delta.Time.Target = ToDeltaTime(tfreq);
                changed = true;
            }
        }
    }
    ImGui::Text("Measured hertz: %u", mfreq);

    if (p_Delta.Unit == 0)
        ImGui::Text("Measured delta time: %.4f s (max: %.4f s)", p_Delta.Smoothed.AsSeconds(), p_Delta.Max.AsSeconds());
    else if (p_Delta.Unit == 1)
        ImGui::Text("Measured delta time: %.2f ms (max: %.2f ms)", p_Delta.Smoothed.AsMilliseconds(),
                    p_Delta.Max.AsMilliseconds());
    else if (p_Delta.Unit == 2)
        ImGui::Text("Measured delta time: %u us (max: %u us)", static_cast<u32>(p_Delta.Smoothed.AsMicroseconds()),
                    static_cast<u32>(p_Delta.Max.AsMicroseconds()));
    else
#    ifndef TKIT_OS_LINUX
        ImGui::Text("Measured delta time: %llu ns (max: %llu ns)", p_Delta.Smoothed.AsNanoseconds(),
                    p_Delta.Max.AsNanoseconds());
#    else
        ImGui::Text("Measured delta time: %lu ns (max: %lu ns)", p_Delta.Smoothed.AsNanoseconds(),
                    p_Delta.Max.AsNanoseconds());
#    endif

    if (p_Flags & UserLayer::Flag_DisplayHelp)
        UserLayer::HelpMarkerSameLine(
            "The delta time is a measure of the time it takes to complete a frame loop around a particular callback "
            "(which can be an update or render callback), and it is one of the main indicators of an application "
            "smoothness. It is also used to calculate the frames per second (FPS) of the application. A good frame "
            "time is usually no larger than 16.67 ms (that is, 60 fps). It is also bound to the present mode of the "
            "window.");

    if (ImGui::Button("Reset maximum"))
        p_Delta.Max = TKit::Timespan{};
    return changed;
}

static bool displayDeltaTime(WindowData &p_Data, const UserLayer::Flags p_Flags)
{
    const auto sync = [](DeltaInfo &p_Dst, const DeltaInfo &p_Src) {
        p_Dst.Time.Target = p_Src.Time.Target;
        p_Dst.Smoothness = p_Src.Smoothness;
        p_Dst.Unit = p_Src.Unit;
        p_Dst.Limit = p_Src.Limit;
    };
    ImGui::Text("Delta time");
    bool changed = ImGui::Button("Sync");
    if (changed)
        sync(p_Data.UpdateClock.Delta, p_Data.RenderClock.Delta);

    if (!p_Data.Window->IsVSync())
    {
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Mirror", &p_Data.MirrorDeltas);
    }
    const bool mirror = !p_Data.Window->IsVSync() && p_Data.MirrorDeltas;
    if (ImGui::TreeNode("Render"))
    {
        if (displayDeltaTime(p_Data.Window, p_Data.RenderClock.Delta, p_Flags, true))
        {
            if (mirror)
                sync(p_Data.UpdateClock.Delta, p_Data.RenderClock.Delta);
            changed = true;
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Update"))
    {
        if (displayDeltaTime(p_Data.Window, p_Data.UpdateClock.Delta, p_Flags, false))
        {
            if (mirror)
                sync(p_Data.RenderClock.Delta, p_Data.UpdateClock.Delta);
            changed = true;
        }
        ImGui::TreePop();
    }
    return changed;
}

bool Application::DisplayDeltaTime(const UserLayer::Flags p_Flags)
{
    return displayDeltaTime(m_MainWindow, p_Flags);
}
bool Application::DisplayDeltaTime(const Window *p_Window, const UserLayer::Flags p_Flags)
{
    WindowData *data = getWindowData(p_Window);
    return displayDeltaTime(*data, p_Flags);
}

bool Application::enableImGui(WindowData &p_Data, const i32 p_Flags)
{
    p_Data.ImGuiConfigFlags = p_Flags;
    if (checkFlags(Flag_Defer))
    {
        p_Data.SetFlags(Flag_MustDisableImGui);
        return false;
    }
    shutdownImGui(p_Data);
    p_Data.ClearFlags(Flag_MustDisableImGui | Flag_ImGuiEnabled);
    return true;
}

bool Application::EnableImGui(const Window *p_Window, const i32 p_Flags)
{
    WindowData *data = getWindowData(p_Window);
    return enableImGui(*data, p_Flags);
}

bool Application::EnableImGui(const i32 p_Flags)
{
    return enableImGui(m_MainWindow, p_Flags);
}

bool Application::DisableImGui(const Window *p_Window)
{
    WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
    if (checkFlags(Flag_Defer))
    {
        data->SetFlags(Flag_MustDisableImGui);
        return false;
    }
    shutdownImGui(*data);
    data->ClearFlags(Flag_MustDisableImGui | Flag_ImGuiEnabled);
    return true;
}

bool Application::reloadImGui(WindowData &p_Data, const i32 p_Flags)
{
    p_Data.ImGuiConfigFlags = p_Flags;
    if (checkFlags(Flag_Defer))
    {
        p_Data.SetFlags(Flag_MustDisableImGui | Flag_MustEnableImGui);
        return false;
    }
    shutdownImGui(p_Data);
    initializeImGui(p_Data);
    return true;
}

bool Application::ReloadImGui(const i32 p_Flags)
{
    return reloadImGui(m_MainWindow, p_Flags);
}
bool Application::ReloadImGui(const Window *p_Window, const i32 p_Flags)
{
    WindowData *data = p_Window ? getWindowData(p_Window) : &m_MainWindow;
    return reloadImGui(*data, p_Flags);
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

void Application::processWindow(WindowData &p_Data)
{
    if (p_Data.UpdateClock.IsDue())
    {
        p_Data.UpdateClock.Update();
        if (p_Data.Layer)
            p_Data.Layer->OnUpdate(p_Data.UpdateClock.Delta.Time);
    }

    if (!p_Data.RenderClock.IsDue())
        return;
    p_Data.RenderClock.Update();
    const FrameInfo info = p_Data.Window->BeginFrame(WaitMode::Poll);
    if (!info)
        return;

#ifdef ONYX_ENABLE_IMGUI
    TKIT_ASSERT(!p_Data.CheckFlags(Flag_ImGuiEnabled) || p_Data.CheckFlags(Flag_ImGuiRunning),
                "[ONYX] ImGui is enabled for window '{}' but no instance of "
                "ImGui is running. This should not be possible",
                p_Data.Window->GetName());

    TKIT_ASSERT(p_Data.CheckFlags(Flag_ImGuiEnabled) || !p_Data.CheckFlags(Flag_ImGuiRunning),
                "[ONYX] ImGui is disabled for window '{}' but an instance of "
                "ImGui is running. This should not be possible",
                p_Data.Window->GetName());

    ImGui::SetCurrentContext(p_Data.ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(p_Data.ImPlotContext);
#    endif
    if (p_Data.CheckFlags(Flag_ImGuiEnabled))
        beginRenderImGui();
#endif

    for (const Event &event : p_Data.Window->GetNewEvents())
    {
        if (p_Data.Window->IsVSync() && (event.Type == Event::SwapChainRecreated || event.Type == Event::WindowMoved))
        {
            p_Data.RenderClock.Delta.Time.Target = p_Data.Window->GetMonitorDeltaTime();
            p_Data.RenderClock.Delta.Limit = true;
        }

        if (p_Data.Layer)
            p_Data.Layer->OnEvent(event);
    }

    p_Data.Window->FlushEvents();
    if (p_Data.Layer)
        p_Data.Layer->OnFrameBegin(p_Data.RenderClock.Delta.Time, info);

    const VkPipelineStageFlags transferFlags = p_Data.Window->SubmitContextData(info);

    p_Data.Window->BeginRendering();
    if (p_Data.Layer)
        p_Data.Layer->OnRenderBegin(p_Data.RenderClock.Delta.Time, info);

    p_Data.Window->Render(info);
#ifdef ONYX_ENABLE_IMGUI
    if (p_Data.CheckFlags(Flag_ImGuiEnabled))
        endRenderImGui(info.GraphicsCommand);
#endif

    if (p_Data.Layer)
        p_Data.Layer->OnRenderEnd(p_Data.RenderClock.Delta.Time, info);
    p_Data.Window->EndRendering();

    if (p_Data.Layer)
        p_Data.Layer->OnFrameEnd(p_Data.RenderClock.Delta.Time, info);
    p_Data.Window->EndFrame(transferFlags);
}

static void syncDeferredOperations(WindowData &p_Data)
{
    if (p_Data.CheckFlags(Application::Flag_MustDestroyLayer))
    {
        delete p_Data.Layer;
        p_Data.Layer = nullptr;
        p_Data.ClearFlags(Application::Flag_MustDestroyLayer);
    }
    if (p_Data.CheckFlags(Application::Flag_MustReplaceLayer))
    {
        p_Data.Layer = p_Data.StagedLayer;
        p_Data.StagedLayer = nullptr;
        p_Data.ClearFlags(Application::Flag_MustReplaceLayer);
    }

#ifdef ONYX_ENABLE_IMGUI

    if (p_Data.CheckFlags(Application::Flag_MustDisableImGui))
    {
        shutdownImGui(p_Data);
        p_Data.ClearFlags(Application::Flag_MustDisableImGui | Application::Flag_ImGuiEnabled);
    }

    if (p_Data.CheckFlags(Application::Flag_MustEnableImGui))
    {
        initializeImGui(p_Data);
        p_Data.ClearFlags(Application::Flag_MustEnableImGui);
        p_Data.SetFlags(Application::Flag_ImGuiEnabled);
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

    if (checkFlags(Flag_Quit)) [[unlikely]]
    {
        closeAllWindows();
        clearFlags(Flag_Quit);
    }
}

bool Application::NextFrame(TKit::Clock &p_Clock)
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::NextFrame");

    setFlags(Flag_Defer);
    Input::PollEvents();

    processWindow(m_MainWindow);
#ifdef __ONYX_MULTI_WINDOW
    for (WindowData &data : m_Windows)
        processWindow(data);
#endif
    clearFlags(Flag_Defer);
    syncDeferredOperations();
    endFrame();

    const auto computeSleep = [](const WindowData &p_Data) {
        return std::min(p_Data.RenderClock.Delta.Time.Target - p_Data.RenderClock.Clock.GetElapsed(),
                        p_Data.UpdateClock.Delta.Time.Target - p_Data.UpdateClock.Clock.GetElapsed());
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
    TKit::Timespan::Sleep(sleep);
    m_DeltaTime = p_Clock.Restart();
    return m_MainWindow.Window;
}

Application::WindowData Application::createWindow(const WindowSpecs &p_Specs)
{
    WindowData data;
    data.Window = m_WindowAllocator.Create<Window>(p_Specs.Specs);
    if (data.Window->IsVSync())
    {
        data.RenderClock.Delta.Time.Target = data.Window->GetMonitorDeltaTime();
        data.RenderClock.Delta.Limit = true;
        data.UpdateClock = data.RenderClock;
    }

#ifdef ONYX_ENABLE_IMGUI
    if (p_Specs.EnableImGui)
    {
        initializeImGui(data);
        data.SetFlags(Flag_ImGuiEnabled);
    }
#endif

    return data;
}

void Application::destroyWindow(WindowData &p_Data)
{
    delete p_Data.StagedLayer;
    delete p_Data.Layer;

    p_Data.StagedLayer = nullptr;
    p_Data.Layer = nullptr;
#ifdef ONYX_ENABLE_IMGUI
    if (p_Data.CheckFlags(Flag_ImGuiEnabled))
        shutdownImGui(p_Data);
#endif
    m_WindowAllocator.Destroy(p_Data.Window);
    p_Data.Window = nullptr;
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
bool Application::CloseWindow(Window *p_Window)
{
    if (checkFlags(Flag_Defer))
    {
        p_Window->FlagShouldClose();
        return false;
    }
    if (m_MainWindow.Window == p_Window)
    {
        destroyWindow(m_MainWindow);
        return true;
    }
    for (u32 i = 0; i < m_Windows.GetSize(); ++i)
        if (m_Windows[i].Window == p_Window)
        {
            destroyWindow(m_Windows[i]);
            m_Windows.RemoveUnordered(m_Windows.begin() + i);
            return true;
        }
    TKIT_FATAL("[ONYX] Failed to close window: Window '{}' not found", p_Window->GetName());
    return false;
}
Window *Application::openWindow(const WindowSpecs &p_Specs)
{
    const WindowData &data = m_Windows.Append(createWindow(p_Specs));
    if (p_Specs.CreationCallback)
        p_Specs.CreationCallback(data.Window);
    return data.Window;
}
#endif

} // namespace Onyx
