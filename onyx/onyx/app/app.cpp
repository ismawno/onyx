#include "backends/imgui_impl_vulkan.h"
#include "onyx/core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"

#include "tkit/preprocessor/system.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/utils/logging.hpp"
#include "vkit/vulkan/loader.hpp"

namespace Onyx
{
IApplication::~IApplication() noexcept
{
    if (!m_Terminated && m_Started)
        Shutdown();
}

bool IApplication::IsStarted() const noexcept
{
    return m_Started;
}
bool IApplication::IsTerminated() const noexcept
{
    return m_Terminated;
}
bool IApplication::IsRunning() const noexcept
{
    return m_Started && !m_Terminated;
}
TKit::Timespan IApplication::GetDeltaTime() const noexcept
{
    return m_DeltaTime;
}

void IApplication::Startup() noexcept
{
    TKIT_ASSERT(!m_Terminated && !m_Started, "[ONYX] Application cannot be started more than once");
    TKIT_PROFILE_PLOT_CONFIG("Draw calls", TKit::ProfilingPlotFormat::Number, false, true, 0);
    m_Started = true;
    onStart();
}

void IApplication::Shutdown() noexcept
{
    TKIT_ASSERT(!m_Terminated && m_Started, "[ONYX] Application cannot be terminated before it is started");
    onShutdown();
    delete m_UserLayer;
    if (Core::IsDeviceCreated())
        Core::GetDeviceTable().DestroyDescriptorPool(Core::GetDevice(), m_ImGuiPool, nullptr);
    m_Terminated = true;
}

void IApplication::Quit() noexcept
{
    m_QuitFlag = true;
}

void IApplication::ApplyTheme() noexcept
{
    TKIT_ASSERT(m_Theme, "[ONYX] No theme has been set. Set one with SetTheme");
    m_Theme->Apply();
}

void IApplication::Run() noexcept
{
    Startup();
    TKit::Clock clock;
    while (NextFrame(clock))
        ;
    Shutdown();
}

void IApplication::beginRenderImGui() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::IApplication::BeginRenderImGui");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void IApplication::endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::IApplication::EndRenderImGui");
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault(nullptr, nullptr);
}

void IApplication::updateUserLayerPointer() noexcept
{
    if (m_StagedUserLayer)
    {
        delete m_UserLayer;
        m_UserLayer = m_StagedUserLayer;
        m_StagedUserLayer = nullptr;
    }
}

void IApplication::onStart() noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnStart();
}
void IApplication::onShutdown() noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnShutdown();
}
void IApplication::onUpdate() noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnUpdate();
}
void IApplication::onFrameBegin(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnFrameBegin(p_FrameIndex, p_CommandBuffer);
}
void IApplication::onFrameEnd(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnFrameEnd(p_FrameIndex, p_CommandBuffer);
}
void IApplication::onRenderBegin(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRenderBegin(p_FrameIndex, p_CommandBuffer);
}
void IApplication::onRenderEnd(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRenderEnd(p_FrameIndex, p_CommandBuffer);
}
void IApplication::onEvent(const Event &p_Event) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnEvent(p_Event);
}
void IApplication::onUpdate(const u32 p_WindowIndex) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnUpdate(p_WindowIndex);
}
void IApplication::onFrameBegin(const u32 p_WindowIndex, const u32 p_FrameIndex,
                                const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnFrameBegin(p_WindowIndex, p_FrameIndex, p_CommandBuffer);
}
void IApplication::onFrameEnd(const u32 p_WindowIndex, const u32 p_FrameIndex,
                              const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnFrameEnd(p_WindowIndex, p_FrameIndex, p_CommandBuffer);
}
void IApplication::onRenderBegin(const u32 p_WindowIndex, const u32 p_FrameIndex,
                                 const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRenderBegin(p_WindowIndex, p_FrameIndex, p_CommandBuffer);
}
void IApplication::onRenderEnd(const u32 p_WindowIndex, const u32 p_FrameIndex,
                               const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRenderEnd(p_WindowIndex, p_FrameIndex, p_CommandBuffer);
}
void IApplication::onEvent(const u32 p_WindowIndex, const Event &p_Event) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnEvent(p_WindowIndex, p_Event);
}
void IApplication::onImGuiRender() noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnImGuiRender();
}

void IApplication::createImGuiPool() noexcept
{
    constexpr std::uint32_t poolSize = 100;
    VkDescriptorPoolSize poolSizes[11] = {{VK_DESCRIPTOR_TYPE_SAMPLER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, poolSize},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, poolSize},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, poolSize},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, poolSize},
                                          {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, poolSize}};

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = poolSize;
    poolInfo.poolSizeCount = 11;
    poolInfo.pPoolSizes = poolSizes;

    TKIT_ASSERT_RETURNS(
        Core::GetDeviceTable().CreateDescriptorPool(Core::GetDevice(), &poolInfo, nullptr, &m_ImGuiPool), VK_SUCCESS,
        "[ONYX] Failed to create descriptor pool");
}

void IApplication::initializeImGui(Window &p_Window) noexcept
{
    if (!m_ImGuiPool)
        createImGuiPool();
    if (!m_Theme)
        m_Theme = TKit::Scope<BabyTheme>::Create();

    m_ImGuiContext = ImGui::CreateContext();
#ifdef ONYX_ENABLE_IMPLOT
    m_ImPlotContext = ImPlot::CreateContext();
#endif

    IMGUI_CHECKVERSION();
    ImGuiIO &io = ImGui::GetIO();
    ImGuiConfigFlags flags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

#ifndef TKIT_OS_LINUX
    flags |= ImGuiConfigFlags_ViewportsEnable; // linux may use a tiling window manager
#endif

    io.ConfigFlags |= flags;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    m_Theme->Apply();
    TKIT_ASSERT_RETURNS(ImGui_ImplGlfw_InitForVulkan(p_Window.GetWindowHandle(), true), true,
                        "[ONYX] Failed to initialize ImGui GLFW");

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = Core::GetInstance();
    initInfo.PhysicalDevice = Core::GetDevice().GetPhysicalDevice();
    initInfo.Device = Core::GetDevice();
    initInfo.Queue = Core::GetGraphicsQueue();
    initInfo.DescriptorPool = m_ImGuiPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 3;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = p_Window.GetFrameScheduler()->CreateSceneRenderInfo();

    TKIT_ASSERT_RETURNS(ImGui_ImplVulkan_LoadFunctions([](const char *p_Name, void *) -> PFN_vkVoidFunction {
                            return VKit::Vulkan::GetInstanceProcAddr(Core::GetInstance(), p_Name);
                        }),
                        true, "[ONYX] Failed to load ImGui Vulkan functions");
    TKIT_ASSERT_RETURNS(ImGui_ImplVulkan_Init(&initInfo), true, "[ONYX] Failed to initialize ImGui Vulkan");
    TKIT_ASSERT_RETURNS(ImGui_ImplVulkan_CreateFontsTexture(), true,
                        "[ONYX] ImGui failed to create fonts texture for Vulkan");
}

void IApplication::shutdownImGui() noexcept
{
    Core::DeviceWaitIdle();
    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyPlatformWindows();
    ImGui::DestroyContext();
#ifdef ONYX_ENABLE_IMPLOT
    ImPlot::DestroyContext();
#endif
}

void IApplication::setImContexts() noexcept
{
    ImGui::SetCurrentContext(m_ImGuiContext);
#ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(m_ImPlotContext);
#endif
}

Application::Application(const Window::Specs &p_WindowSpecs) noexcept
{
    m_Window.Construct(p_WindowSpecs);
    initializeImGui(*m_Window);
}

void Application::Shutdown() noexcept
{
    if (m_WindowAlive)
    {
        m_Window->GetFrameScheduler()->WaitIdle();
        shutdownImGui();
        m_Window.Destruct();
        m_WindowAlive = false;
    }
    IApplication::Shutdown();
}

static void endFrame() noexcept
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
    const i64 drawCalls = static_cast<i64>(Detail::GetDrawCallCount());
    TKIT_PROFILE_PLOT("Draw calls", static_cast<i64>(drawCalls));
    Detail::ResetDrawCallCount();
#endif
    TKIT_PROFILE_MARK_FRAME();
}

bool Application::NextFrame(TKit::Clock &p_Clock) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::NextFrame");
    setImContexts();
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

    onUpdate();

    RenderCallbacks callbacks{};
    callbacks.OnFrameBegin = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
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
        endRenderImGui(p_CommandBuffer);
    };

    m_Window->Render(callbacks);
    m_DeferFlag = false;
    updateUserLayerPointer();

    if (m_Window->ShouldClose()) [[unlikely]]
    {
        m_Window->GetFrameScheduler()->WaitIdle();
        shutdownImGui();
        m_Window.Destruct();
        m_WindowAlive = false;
        endFrame();
        return false;
    }
    m_DeltaTime = p_Clock.Restart();
    endFrame();
    return true;
}

const Window *Application::GetMainWindow() const noexcept
{
    return m_Window.Get();
}
Window *Application::GetMainWindow() noexcept
{
    return m_Window.Get();
}

void MultiWindowApplication::processFrame(const u32 p_WindowIndex, const RenderCallbacks &p_Callbacks) noexcept
{
    const auto &window = m_Windows[p_WindowIndex];
    for (const Event &event : window->GetNewEvents())
        onEvent(p_WindowIndex, event);

    window->FlushEvents();
    // Should maybe exit if window is closed at this point? (triggered by event)

    onUpdate(p_WindowIndex);
    const u32 size = m_Windows.GetSize();
    const auto &prevWindow = m_Windows[p_WindowIndex == 0 ? (size - 1) : (p_WindowIndex - 1)];

    prevWindow->GetFrameScheduler()->WaitIdle();
    window->Render(p_Callbacks);
}

void MultiWindowApplication::Shutdown() noexcept
{
    CloseAllWindows();
    IApplication::Shutdown();
}

void MultiWindowApplication::CloseAllWindows() noexcept
{
    for (u32 i = m_Windows.GetSize() - 1; i < m_Windows.GetSize(); --i)
        CloseWindow(i);
}

void MultiWindowApplication::CloseWindow(const Window *p_Window) noexcept
{
    for (u32 i = 0; i < m_Windows.GetSize(); ++i)
        if (m_Windows[i].Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    TKIT_ERROR("Window was not found");
}

const Window *MultiWindowApplication::GetWindow(const u32 p_Index) const noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.GetSize(), "[ONYX] Index out of bounds");
    return m_Windows[p_Index].Get();
}
Window *MultiWindowApplication::GetWindow(const u32 p_Index) noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.GetSize(), "[ONYX] Index out of bounds");
    return m_Windows[p_Index].Get();
}

const Window *MultiWindowApplication::GetMainWindow() const noexcept
{
    return GetWindow(0);
}
Window *MultiWindowApplication::GetMainWindow() noexcept
{
    return GetWindow(0);
}

u32 MultiWindowApplication::GetWindowCount() const noexcept
{
    return m_Windows.GetSize();
}

bool MultiWindowApplication::NextFrame(TKit::Clock &p_Clock) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::MultiWindowApplication::NextFrame");
    setImContexts();
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

void MultiWindowApplication::CloseWindow(const u32 p_Index) noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.GetSize(), "[ONYX] Index out of bounds");
    for (const auto &window : m_Windows)
        window->GetFrameScheduler()->WaitIdle();

    if (m_DeferFlag)
    {
        m_Windows[p_Index]->FlagShouldClose();
        return;
    }
    Event event;
    event.Type = Event::WindowClosed;
    event.Window = m_Windows[p_Index].Get();
    onEvent(p_Index, event);

    // Check if the main window got removed. If so, imgui needs to be reinitialized with the new main window
    if (p_Index == 0)
    {
        m_Windows[p_Index]->GetFrameScheduler()->WaitIdle();
        shutdownImGui();
        m_Windows.RemoveOrdered(m_Windows.begin() + p_Index);
        if (!m_Windows.IsEmpty())
            initializeImGui(*m_Windows[0]);
    }
    else
        m_Windows.RemoveOrdered(m_Windows.begin() + p_Index);
}

void MultiWindowApplication::OpenWindow(const Window::Specs &p_Specs) noexcept
{
    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    TKIT_LOG_WARNING_IF(p_Specs.Flags & Window::Flag_ConcurrentQueueSubmission,
                        "[ONYX] Concurrent queue submission works badly in multi window applications. Consider "
                        "disabling it to avoid unexpected crashes.");
    if (m_DeferFlag)
    {
        m_WindowsToAdd.Append(p_Specs);
        return;
    }

    auto window = TKit::Scope<Window>::Create(p_Specs);
    if (m_Windows.IsEmpty())
        initializeImGui(*window);

    m_Windows.Append(std::move(window));
    Event event;
    event.Type = Event::WindowOpened;
    event.Window = m_Windows.GetBack().Get();
    onEvent(m_Windows.GetSize() - 1, event);
}

void MultiWindowApplication::processWindows() noexcept
{
    m_DeferFlag = true;
    RenderCallbacks mainCbs{};
    mainCbs.OnFrameBegin = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
        onFrameBegin(0, p_FrameIndex, p_CommandBuffer);
    };
    mainCbs.OnFrameEnd = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onFrameEnd(0, p_FrameIndex, p_CommandBuffer);
    };
    mainCbs.OnRenderBegin = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onImGuiRender();
        onRenderBegin(0, p_FrameIndex, p_CommandBuffer);
    };
    mainCbs.OnRenderEnd = [this](const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) {
        onRenderEnd(0, p_FrameIndex, p_CommandBuffer);
        endRenderImGui(p_CommandBuffer);
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
