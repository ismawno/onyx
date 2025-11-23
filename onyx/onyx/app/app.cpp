#include "onyx/core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/glfw.hpp"
#include "onyx/core/core.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/utils/debug.hpp"
#include "vkit/vulkan/loader.hpp"

namespace Onyx
{
#ifdef ONYX_ENABLE_IMGUI
IApplication::IApplication(const i32 p_ImGuiConfigFlags) : m_ImGuiConfigFlags(p_ImGuiConfigFlags)
{
}
#endif
IApplication::~IApplication()
{
    if (!m_Terminated && m_Started)
        Shutdown();
}

void IApplication::Startup()
{
    TKIT_ASSERT(!m_Terminated && !m_Started, "[ONYX] Application cannot be started more than once");
    TKIT_PROFILE_PLOT_CONFIG("Draw calls", TKit::ProfilingPlotFormat::Number, false, true, 0);
    m_Started = true;
    onStart();
}

void IApplication::Shutdown()
{
    TKIT_ASSERT(!m_Terminated && m_Started, "[ONYX] Application cannot be terminated before it is started");
    onShutdown();
    delete m_UserLayer;
    m_Terminated = true;
}

void IApplication::Quit()
{
    m_QuitFlag = true;
}

void IApplication::ApplyTheme()
{
    TKIT_ASSERT(m_Theme, "[ONYX] No theme has been set. Set one with SetTheme");
    m_Theme->Apply();
}

void IApplication::Run()
{
    Startup();
    TKit::Clock clock;
    while (NextFrame(clock))
        ;
    Shutdown();
}

#ifdef ONYX_ENABLE_IMGUI
void IApplication::beginRenderImGui()
{
    TKIT_PROFILE_NSCOPE("Onyx::IApplication::BeginRenderImGui");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void IApplication::endRenderImGui(VkCommandBuffer p_CommandBuffer)
{
    TKIT_PROFILE_NSCOPE("Onyx::IApplication::EndRenderImGui");
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    const ImGuiIO &io = ImGui::GetIO();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
#endif

void IApplication::updateUserLayerPointer()
{
    if (m_StagedUserLayer)
    {
        delete m_UserLayer;
        m_UserLayer = m_StagedUserLayer;
        m_StagedUserLayer = nullptr;
    }
}

void IApplication::onStart()
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnStart();
}
void IApplication::onShutdown()
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnShutdown();
}
void IApplication::onUpdate()
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnUpdate();
}
void IApplication::onFrameBegin(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnFrameBegin(p_FrameIndex, p_CommandBuffer);
}
void IApplication::onFrameEnd(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnFrameEnd(p_FrameIndex, p_CommandBuffer);
}
void IApplication::onRenderBegin(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRenderBegin(p_FrameIndex, p_CommandBuffer);
}
void IApplication::onRenderEnd(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRenderEnd(p_FrameIndex, p_CommandBuffer);
}
void IApplication::onEvent(const Event &p_Event)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnEvent(p_Event);
}
void IApplication::onUpdate(const u32 p_WindowIndex)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnUpdate(p_WindowIndex);
}
void IApplication::onFrameBegin(const u32 p_WindowIndex, const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnFrameBegin(p_WindowIndex, p_FrameIndex, p_CommandBuffer);
}
void IApplication::onFrameEnd(const u32 p_WindowIndex, const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnFrameEnd(p_WindowIndex, p_FrameIndex, p_CommandBuffer);
}
void IApplication::onRenderBegin(const u32 p_WindowIndex, const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRenderBegin(p_WindowIndex, p_FrameIndex, p_CommandBuffer);
}
void IApplication::onRenderEnd(const u32 p_WindowIndex, const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRenderEnd(p_WindowIndex, p_FrameIndex, p_CommandBuffer);
}
void IApplication::onEvent(const u32 p_WindowIndex, const Event &p_Event)
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnEvent(p_WindowIndex, p_Event);
}
#ifdef ONYX_ENABLE_IMGUI
void IApplication::onImGuiRender()
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnImGuiRender();
}

static i32 createVkSurface(ImGuiViewport *, ImU64 p_Instance, const void *p_Callbacks, ImU64 *p_Surface)
{
    return glfwCreateWindowSurface(reinterpret_cast<VkInstance>(p_Instance), nullptr,
                                   static_cast<const VkAllocationCallbacks *>(p_Callbacks),
                                   reinterpret_cast<VkSurfaceKHR *>(&p_Surface));
}

void IApplication::initializeImGui(Window &p_Window)
{
    if (!m_Theme)
        m_Theme = TKit::Scope<BabyTheme>::Create();

    ImGui::CreateContext();
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::CreateContext();
#    endif

    IMGUI_CHECKVERSION();
    ImGuiIO &io = ImGui::GetIO();

    io.ConfigFlags = m_ImGuiConfigFlags;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;

    ImGuiPlatformIO &pio = ImGui::GetPlatformIO();
    if (!(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
        pio.Platform_CreateVkSurface = createVkSurface;

    m_Theme->Apply();
    TKIT_ASSERT_RETURNS(ImGui_ImplGlfw_InitForVulkan(p_Window.GetWindowHandle(), true), true,
                        "[ONYX] Failed to initialize ImGui GLFW");

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
    pipelineInfo.PipelineRenderingCreateInfo = p_Window.GetFrameScheduler()->CreateSceneRenderInfo();
    pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    const VkSurfaceCapabilitiesKHR &sc =
        p_Window.GetFrameScheduler()->GetSwapChain().GetInfo().SupportDetails.Capabilities;

    const u32 imageCount = sc.minImageCount != sc.maxImageCount ? sc.minImageCount + 1 : sc.minImageCount;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = instance.GetInfo().ApiVersion;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = device.GetPhysicalDevice();
    initInfo.Device = device;
    initInfo.Queue = Core::GetGraphicsQueue();
    initInfo.QueueFamily = Core::GetGraphicsIndex();
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
    TKIT_ASSERT_RETURNS(ImGui_ImplVulkan_Init(&initInfo), true, "[ONYX] Failed to initialize ImGui Vulkan");
}

void IApplication::shutdownImGui()
{
    Core::DeviceWaitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyPlatformWindows();
    ImGui::DestroyContext();
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::DestroyContext();
#    endif
}
#endif

#ifdef ONYX_ENABLE_IMGUI
Application::Application(const Window::Specs &p_WindowSpecs, const i32 p_ImGuiConfigFlags)
    : IApplication(p_ImGuiConfigFlags)
{
    m_Window.Construct(p_WindowSpecs);
    initializeImGui(*m_Window);
}
Application::Application(const i32 p_ImGuiConfigFlags) : IApplication(p_ImGuiConfigFlags)
{
    m_Window.Construct();
    initializeImGui(*m_Window);
}
#else
Application::Application(const Window::Specs &p_WindowSpecs)
{
    m_Window.Construct(p_WindowSpecs);
}
#endif
void Application::Shutdown()
{
    if (m_WindowAlive)
    {
#ifdef ONYX_ENABLE_IMGUI
        shutdownImGui();
#endif
        m_Window.Destruct();
        m_WindowAlive = false;
    }
    IApplication::Shutdown();
}

static void endFrame()
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
    const i64 drawCalls = static_cast<i64>(Detail::GetDrawCallCount());
    TKIT_PROFILE_PLOT("Draw calls", static_cast<i64>(drawCalls));
    Detail::ResetDrawCallCount();
#endif
    TKIT_PROFILE_MARK_FRAME();
}

bool Application::NextFrame(TKit::Clock &p_Clock)
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::NextFrame");
    if (m_QuitFlag) [[unlikely]]
    {
        m_QuitFlag = false;
        endFrame();
        return false;
    }

    m_DeferFlag = true;
    Input::PollEvents();
    for (const Event &event : m_Window->GetNewEvents())
        onEvent(event);

    m_Window->FlushEvents();
    // Should maybe exit if window is closed at this point (triggered by event)

#ifdef ONYX_ENABLE_IMGUI
    beginRenderImGui();
#endif
    onUpdate();

    RenderCallbacks callbacks{};
    callbacks.OnFrameBegin = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onFrameBegin(p_FrameIndex, p_CommandBuffer);
    };
    callbacks.OnFrameEnd = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onFrameEnd(p_FrameIndex, p_CommandBuffer);
    };
    callbacks.OnRenderBegin = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onRenderBegin(p_FrameIndex, p_CommandBuffer);
    };
    callbacks.OnRenderEnd = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onRenderEnd(p_FrameIndex, p_CommandBuffer);
#ifdef ONYX_ENABLE_IMGUI
        endRenderImGui(p_CommandBuffer);
#endif
    };
#ifdef ONYX_ENABLE_IMGUI
    callbacks.OnBadFrame = [](const u32) { ImGui::Render(); };
#endif

    m_Window->Render(callbacks);
    m_DeferFlag = false;
    updateUserLayerPointer();

    if (m_Window->ShouldClose()) [[unlikely]]
    {
#ifdef ONYX_ENABLE_IMGUI
        shutdownImGui();
#endif
        m_Window.Destruct();
        m_WindowAlive = false;
        endFrame();
        return false;
    }
    m_DeltaTime = p_Clock.Restart();
    endFrame();
    return true;
}

#ifdef ONYX_ENABLE_IMGUI
MultiWindowApplication::MultiWindowApplication(const i32 p_ImGuiConfigFlags) : IApplication(p_ImGuiConfigFlags)
{
}
#endif
void MultiWindowApplication::processFrame(const u32 p_WindowIndex, const RenderCallbacks &p_Callbacks)
{
    Window *window = m_Windows[p_WindowIndex];
    for (const Event &event : window->GetNewEvents())
        onEvent(p_WindowIndex, event);

    window->FlushEvents();
    // Should maybe exit if window is closed at this point? (triggered by event)

#ifdef ONYX_ENABLE_IMGUI
    if (p_WindowIndex == 0)
        beginRenderImGui();
#endif

    onUpdate(p_WindowIndex);
    window->Render(p_Callbacks);
}

void MultiWindowApplication::Shutdown()
{
    CloseAllWindows();
    IApplication::Shutdown();
}

void MultiWindowApplication::CloseAllWindows()
{
    for (u32 i = m_Windows.GetSize() - 1; i < m_Windows.GetSize(); --i)
        CloseWindow(i);
}

void MultiWindowApplication::CloseWindow(const Window *p_Window)
{
    for (u32 i = 0; i < m_Windows.GetSize(); ++i)
        if (m_Windows[i] == p_Window)
        {
            CloseWindow(i);
            return;
        }
    TKIT_FATAL("Window was not found");
}

bool MultiWindowApplication::NextFrame(TKit::Clock &p_Clock)
{
    TKIT_PROFILE_NSCOPE("Onyx::MultiWindowApplication::NextFrame");
    if (m_Windows.IsEmpty() || m_QuitFlag) [[unlikely]]
    {
        m_QuitFlag = false;
        endFrame();
        return false;
    }

    Input::PollEvents();
    processWindows();

    m_DeltaTime = p_Clock.Restart();
    endFrame();
    return !m_Windows.IsEmpty();
}

void MultiWindowApplication::CloseWindow(const u32 p_Index)
{
    TKIT_ASSERT(p_Index < m_Windows.GetSize(), "[ONYX] Index out of bounds");

    Window *window = m_Windows[p_Index];
    if (m_DeferFlag)
    {
        window->FlagShouldClose();
        return;
    }
    Event event;
    event.Type = Event::WindowClosed;
    event.Window = window;
    onEvent(p_Index, event);

    // Check if the main window got removed. If so, imgui needs to be reinitialized with the new main window
    if (p_Index == 0)
    {
#ifdef ONYX_ENABLE_IMGUI
        shutdownImGui();
#endif
        m_WindowAllocator.Destroy(window);
        m_Windows.RemoveOrdered(m_Windows.begin() + p_Index);
#ifdef ONYX_ENABLE_IMGUI
        if (!m_Windows.IsEmpty())
            initializeImGui(*m_Windows[0]);
#endif
    }
    else
    {
        m_WindowAllocator.Destroy(window);
        m_Windows.RemoveOrdered(m_Windows.begin() + p_Index);
    }
}

void MultiWindowApplication::OpenWindow(const Window::Specs &p_Specs)
{
    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    if (m_DeferFlag)
    {
        m_WindowsToAdd.Append(p_Specs);
        return;
    }

    Window *window = m_WindowAllocator.Create<Window>(p_Specs);
#ifdef ONYX_ENABLE_IMGUI
    if (m_Windows.IsEmpty())
        initializeImGui(*window);
#endif

    m_Windows.Append(window);
    Event event;
    event.Type = Event::WindowOpened;
    event.Window = window;
    onEvent(m_Windows.GetSize() - 1, event);
}

void MultiWindowApplication::processWindows()
{
    m_DeferFlag = true;
    RenderCallbacks mainCbs{};
    mainCbs.OnFrameBegin = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onFrameBegin(0, p_FrameIndex, p_CommandBuffer);
    };
    mainCbs.OnFrameEnd = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onFrameEnd(0, p_FrameIndex, p_CommandBuffer);
    };
    mainCbs.OnRenderBegin = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
#ifdef ONYX_ENABLE_IMGUI
        onImGuiRender();
#endif
        onRenderBegin(0, p_FrameIndex, p_CommandBuffer);
    };
    mainCbs.OnRenderEnd = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onRenderEnd(0, p_FrameIndex, p_CommandBuffer);
#ifdef ONYX_ENABLE_IMGUI
        endRenderImGui(p_CommandBuffer);
#endif
    };

    processFrame(0, mainCbs);
    for (u32 i = 1; i < m_Windows.GetSize(); ++i)
    {
        RenderCallbacks secCbs{};
        secCbs.OnFrameBegin = [this, i](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
            onFrameBegin(i, p_FrameIndex, p_CommandBuffer);
        };
        secCbs.OnFrameEnd = [this, i](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
            onFrameEnd(i, p_FrameIndex, p_CommandBuffer);
        };
        secCbs.OnRenderBegin = [this, i](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
            onRenderBegin(i, p_FrameIndex, p_CommandBuffer);
        };
        secCbs.OnRenderEnd = [this, i](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
            onRenderEnd(i, p_FrameIndex, p_CommandBuffer);
        };
        processFrame(i, secCbs);
    }

    m_DeferFlag = false;
    updateUserLayerPointer();

    for (u32 i = m_Windows.GetSize() - 1; i < m_Windows.GetSize(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
    for (const auto &specs : m_WindowsToAdd)
        OpenWindow(specs);
    m_WindowsToAdd.Clear();
}

} // namespace Onyx
