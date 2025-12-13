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

    if (p_Data.Layer)
        for (const Event &event : p_Data.Window->GetNewEvents())
            p_Data.Layer->OnEvent(event);

    p_Data.Window->FlushEvents();
    RenderCallbacks callbacks;
    if (p_Data.Layer)
    {
        p_Data.Layer->OnUpdate();
        callbacks.OnFrameBegin = [&p_Data](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
            p_Data.Layer->OnFrameBegin(p_FrameIndex, p_CommandBuffer);
        };
        callbacks.OnFrameEnd = [&p_Data](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
            p_Data.Layer->OnFrameEnd(p_FrameIndex, p_CommandBuffer);
        };
        callbacks.OnRenderBegin = [&p_Data](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
            p_Data.Layer->OnRenderBegin(p_FrameIndex, p_CommandBuffer);
        };
#ifdef ONYX_ENABLE_IMGUI
        if (p_Data.CheckFlags(Flag_ImGuiEnabled))
            callbacks.OnRenderEnd = [&p_Data](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
                p_Data.Layer->OnRenderEnd(p_FrameIndex, p_CommandBuffer);
                endRenderImGui(p_CommandBuffer);
            };
        else
            callbacks.OnRenderEnd = [&p_Data](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
                p_Data.Layer->OnRenderEnd(p_FrameIndex, p_CommandBuffer);
            };
#else
        callbacks.OnRenderEnd = [&p_Data](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
            p_Data.Layer->OnRenderEnd(p_FrameIndex, p_CommandBuffer);
        };
#endif
    }
#ifdef ONYX_ENABLE_IMGUI
    else if (p_Data.CheckFlags(Flag_ImGuiEnabled))
        callbacks.OnRenderEnd = [](const u32, const VkCommandBuffer p_CommandBuffer) {
            endRenderImGui(p_CommandBuffer);
        };

    if (p_Data.CheckFlags(Flag_ImGuiEnabled))
        callbacks.OnBadFrame = [](const u32) { ImGui::Render(); };
#endif
    p_Data.Window->Render(callbacks);
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
Window *Application::OpenWindow(const WindowSpecs &p_Specs)
{
    if (checkFlags(Flag_Defer))
    {
        m_WindowsToAdd.Append(p_Specs);
        return nullptr;
    }

    return openWindow(p_Specs);
}
Window *Application::OpenWindow(const Window::Specs &p_Specs)
{
    return OpenWindow(WindowSpecs{.Specs = p_Specs});
}

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
Window *Application::openWindow(const WindowSpecs &p_Baby)
{
    WindowData data;
    data.Window = m_WindowAllocator.Create<Window>(p_Baby.Specs);

#    ifdef ONYX_ENABLE_IMGUI
    if (p_Baby.EnableImGui)
    {
        initializeImGui(data);
        data.SetFlags(Flag_ImGuiEnabled);
    }
#    endif

    m_Windows.Append(data);

    if (p_Baby.CreationCallback)
        p_Baby.CreationCallback(data.Window);
    return data.Window;
}
#endif

} // namespace Onyx
