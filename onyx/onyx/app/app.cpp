#define ONYX_IMGUI_INCLUDE_BACKEND
#include "onyx/core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/glfw.hpp"
#include "onyx/core/core.hpp"
#include "onyx/core/imgui.hpp"
#include "onyx/core/implot.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/utils/debug.hpp"
#include "vkit/vulkan/loader.hpp"

namespace Onyx
{
Application::Application(const WindowSpecs &p_MainSpecs)
{
    m_MainWindow.Window = m_WindowAllocator.Create<Window>(p_MainSpecs.Specs);

#ifdef ONYX_ENABLE_IMGUI
    m_ImGuiBackendFlags = ImGuiBackendFlags_RendererHasTextures;
    if (p_MainSpecs.EnableImGui)
    {
        initializeImGui(m_MainWindow);
        m_MainWindow.SetFlags(Flag_ImGuiEnabled);
    }
#endif

    // a bit useless in this case tbh
    if (p_MainSpecs.CreationCallback)
        p_MainSpecs.CreationCallback(m_MainWindow.Window);
}
Application::Application(const Window::Specs &p_MainSpecs) : Application(WindowSpecs{.Specs = p_MainSpecs})
{
}

Application::~Application()
{
    closeAllWindows();
}

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
void Application::ApplyTheme()
{
    TKIT_ASSERT(m_Theme, "[ONYX] No theme has been set. Set one with SetTheme");
    m_Theme->Apply();
}

static void beginRenderImGui()
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::BeginRenderImGui");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

static void endRenderImGui(VkCommandBuffer p_CommandBuffer)
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

static i32 createVkSurface(ImGuiViewport *, ImU64 p_Instance, const void *p_Callbacks, ImU64 *p_Surface)
{
    return glfwCreateWindowSurface(reinterpret_cast<VkInstance>(p_Instance), nullptr,
                                   static_cast<const VkAllocationCallbacks *>(p_Callbacks),
                                   reinterpret_cast<VkSurfaceKHR *>(&p_Surface));
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

void Application::initializeImGui(WindowData &p_Data)
{
    Window *window = p_Data.Window;
    TKIT_ASSERT(!p_Data.CheckFlags(Flag_ImGuiRunning),
                "[ONYX] Trying to initialize ImGui for window '{}' when it is already running. If you "
                "meant to reload ImGui, use ReloadImGui()",
                window->GetName());

    IMGUI_CHECKVERSION();
    if (!m_Theme)
        m_Theme = TKit::Scope<BabyTheme>::Create();

    p_Data.ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(p_Data.ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    p_Data.ImPlotContext = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(p_Data.ImPlotContext);
#    endif

    ImGuiIO &io = ImGui::GetIO();
    TKIT_LOG_WARNING_IF(!(m_ImGuiBackendFlags & ImGuiBackendFlags_RendererHasTextures),
                        "[ONYX] ImGui may fail to initialize if ImGuiBackendFlags_RendererHasTextures is not set. If "
                        "you experience issues, try setting it with SetImGuiBackendFlags()");

    io.ConfigFlags = m_ImGuiConfigFlags;
    io.BackendFlags = m_ImGuiBackendFlags;

    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    if (!(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
        pio.Platform_CreateVkSurface = createVkSurface;

    m_Theme->Apply();

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

    p_Data.SetFlags(Flag_ImGuiRunning);
}

void Application::shutdownImGui(WindowData &p_Data)
{
    TKIT_ASSERT(p_Data.CheckFlags(Flag_ImGuiRunning),
                "[ONYX] Trying to shut down ImGui when it is not initialized to begin with");

    p_Data.ClearFlags(Flag_ImGuiRunning);

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
    DeltaTime &delta = p_Delta.Time;

    if (delta.Measured > p_Delta.Max)
        p_Delta.Max = delta.Measured;
    p_Delta.Smoothed = p_Delta.Smoothness * p_Delta.Smoothed + (1.f - p_Delta.Smoothness) * delta.Measured;

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
        const u32 mn = 30;
        const u32 mx = 240;
        changed = ImGui::SliderScalarN("Target hertz", ImGuiDataType_U32, &tfreq, 1, &mn, &mx);
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

bool Application::DisplayUpdateDeltaTime(const UserLayer::Flags p_Flags)
{
    if (displayDeltaTime(m_MainWindow.Window, m_MainWindow.UpdateClock.Delta, p_Flags, false))
    {
        SetUpdateDeltaTime(m_MainWindow.UpdateClock.Delta.Time.Target);
        return true;
    }
    return false;
}
bool Application::DisplayRenderDeltaTime(const UserLayer::Flags p_Flags)
{
    if (displayDeltaTime(m_MainWindow.Window, m_MainWindow.RenderClock.Delta, p_Flags, true))
    {
        SetUpdateDeltaTime(m_MainWindow.RenderClock.Delta.Time.Target);
        return true;
    }
    return false;
}

bool Application::DisplayUpdateDeltaTime(Window *p_Window, const UserLayer::Flags p_Flags)
{
    WindowData *data = getWindowData(p_Window);
    if (displayDeltaTime(data->Window, data->UpdateClock.Delta, p_Flags, false))
    {
        SetUpdateDeltaTime(data->UpdateClock.Delta.Time.Target);
        return true;
    }
    return false;
}
bool Application::DisplayRenderDeltaTime(Window *p_Window, const UserLayer::Flags p_Flags)
{
    WindowData *data = getWindowData(p_Window);
    if (displayDeltaTime(data->Window, data->RenderClock.Delta, p_Flags, true))
    {
        SetUpdateDeltaTime(data->RenderClock.Delta.Time.Target);
        return true;
    }
    return false;
}

bool Application::EnableImGui(const i32 p_Flags, Window *p_Window)
{
    m_ImGuiConfigFlags = p_Flags;
    return EnableImGui(p_Window);
}
bool Application::EnableImGui(Window *p_Window)
{
    WindowData *data = getWindowData(p_Window);
    if (checkFlags(Flag_Defer))
    {
        data->SetFlags(Flag_MustEnableImGui);
        return false;
    }
    initializeImGui(*data);
    data->ClearFlags(Flag_MustEnableImGui);
    data->SetFlags(Flag_ImGuiEnabled);
    return true;
}

bool Application::DisableImGui(Window *p_Window)
{
    WindowData *data = getWindowData(p_Window);
    if (checkFlags(Flag_Defer))
    {
        data->SetFlags(Flag_MustDisableImGui);
        return false;
    }
    shutdownImGui(*data);
    data->ClearFlags(Flag_MustDisableImGui | Flag_ImGuiEnabled);
    return true;
}

bool Application::ReloadImGui(const i32 p_Flags, Window *p_Window)
{
    m_ImGuiConfigFlags = p_Flags;
    return ReloadImGui(p_Window);
}
bool Application::ReloadImGui(Window *p_Window)
{
    WindowData *data = getWindowData(p_Window);
    if (checkFlags(Flag_Defer))
    {
        data->SetFlags(Flag_MustDisableImGui | Flag_MustEnableImGui);
        return false;
    }
    shutdownImGui(*data);
    initializeImGui(*data);
    return true;
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
    if (p_Data.Layer && p_Data.UpdateClock.IsDue())
    {
        p_Data.UpdateClock.Update();
        p_Data.Layer->OnUpdate(p_Data.UpdateClock.Delta.Time);
    }

    if (!p_Data.RenderClock.IsDue())
        return;
    p_Data.RenderClock.Update();
    const FrameInfo info = p_Data.Window->BeginFrame();
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
        if (event.Type == Event::SwapChainRecreated)
            updateMinimumTargetDelta();

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

void Application::syncDeferredOperations()
{
    syncDeferredOperations(m_MainWindow);
#ifdef __ONYX_MULTI_WINDOW
    for (WindowData &data : m_Windows)
        syncDeferredOperations(data);

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

void Application::syncDeferredOperations(WindowData &p_Data)
{
    if (p_Data.CheckFlags(Flag_MustDestroyLayer))
    {
        delete p_Data.Layer;
        p_Data.Layer = nullptr;
        p_Data.ClearFlags(Flag_MustDestroyLayer);
    }
    if (p_Data.CheckFlags(Flag_MustReplaceLayer))
    {
        p_Data.Layer = p_Data.StagedLayer;
        p_Data.StagedLayer = nullptr;
        p_Data.ClearFlags(Flag_MustReplaceLayer);
    }

#ifdef ONYX_ENABLE_IMGUI

    if (p_Data.CheckFlags(Flag_MustDisableImGui))
    {
        shutdownImGui(p_Data);
        p_Data.ClearFlags(Flag_MustDisableImGui | Flag_ImGuiEnabled);
    }

    if (p_Data.CheckFlags(Flag_MustEnableImGui))
    {
        initializeImGui(p_Data);
        p_Data.ClearFlags(Flag_MustEnableImGui);
        p_Data.SetFlags(Flag_ImGuiEnabled);
    }
#endif
}

void Application::updateMinimumTargetDelta()
{
    m_MinimumTargetDelta =
        std::min(m_MainWindow.UpdateClock.Delta.Time.Target, m_MainWindow.RenderClock.Delta.Time.Target);
#ifdef __ONYX_MULTI_WINDOW
    for (const WindowData &data : m_Windows)
    {
        const TKit::Timespan mn = std::min(data.UpdateClock.Delta.Time.Target, data.RenderClock.Delta.Time.Target);
        if (mn < m_MinimumTargetDelta)
            m_MinimumTargetDelta = mn;
    }
#endif
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

    const TKit::Timespan elapsed = p_Clock.GetElapsed();
    if (elapsed < m_MinimumTargetDelta)
        TKit::Timespan::Sleep(m_MinimumTargetDelta - elapsed);

    m_DeltaTime = p_Clock.Restart();
    return m_MainWindow.Window;
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
    WindowData data;
    data.Window = m_WindowAllocator.Create<Window>(p_Specs.Specs);
    if (data.Window->IsVSync())
        data.RenderClock.Delta.Time.Target = data.Window->GetMonitorDeltaTime();

#    ifdef ONYX_ENABLE_IMGUI
    if (p_Specs.EnableImGui)
    {
        initializeImGui(data);
        data.SetFlags(Flag_ImGuiEnabled);
    }
#    endif

    m_Windows.Append(data);

    if (p_Specs.CreationCallback)
        p_Specs.CreationCallback(data.Window);

    updateMinimumTargetDelta();
    return data.Window;
}
#endif

} // namespace Onyx
