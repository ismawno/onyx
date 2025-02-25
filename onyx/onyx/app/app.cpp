#include "onyx/core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"

#include "tkit/profiling/macros.hpp"
#include "tkit/profiling/vulkan.hpp"

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
    m_Started = true;
    onStart();
}

void IApplication::Shutdown() noexcept
{
    TKIT_ASSERT(!m_Terminated && m_Started, "[ONYX] Application cannot be terminated before it is started");
    onShutdown();
    delete m_UserLayer;
    if (Core::IsDeviceCreated())
        vkDestroyDescriptorPool(Core::GetDevice(), m_ImGuiPool, nullptr);
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
void IApplication::onRender(const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRender(p_CommandBuffer);
}
void IApplication::onLateRender(const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnLateRender(p_CommandBuffer);
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
void IApplication::onRender(const u32 p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnRender(p_WindowIndex, p_CommandBuffer);
}
void IApplication::onLateRender(const u32 p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer) [[likely]]
        m_UserLayer->OnLateRender(p_WindowIndex, p_CommandBuffer);
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

    TKIT_ASSERT_RETURNS(vkCreateDescriptorPool(Core::GetDevice(), &poolInfo, nullptr, &m_ImGuiPool), VK_SUCCESS,
                        "[ONYX] Failed to create descriptor pool");
}

void IApplication::initializeImGui(Window &p_Window) noexcept
{
    if (!m_ImGuiPool)
        createImGuiPool();
    if (!m_Theme)
        m_Theme = TKit::Scope<DefaultTheme>::Create();

    ImGui::CreateContext();
#ifdef ONYX_ENABLE_IMPLOT
    ImPlot::CreateContext();
#endif

    IMGUI_CHECKVERSION();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    m_Theme->Apply();
    ImGui_ImplGlfw_InitForVulkan(p_Window.GetWindowHandle(), true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = Core::GetInstance();
    initInfo.PhysicalDevice = Core::GetDevice().GetPhysicalDevice();
    initInfo.Device = Core::GetDevice();
    initInfo.Queue = Core::GetGraphicsQueue();
    initInfo.DescriptorPool = m_ImGuiPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = p_Window.GetRenderPass();
    initInfo.Subpass = 0;

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
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

Application::Application(const Window::Specs &p_WindowSpecs) noexcept
{
    m_Window.Construct(p_WindowSpecs);
    initializeImGui(*m_Window);
}

void Application::Shutdown() noexcept
{
    if (m_WindowAlive)
    {
        shutdownImGui();
        m_Window.Destruct();
        m_WindowAlive = false;
    }
    IApplication::Shutdown();
}

bool Application::NextFrame(TKit::Clock &p_Clock) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::NextFrame");
    if (m_QuitFlag) [[unlikely]]
    {
        m_QuitFlag = false;
        TKIT_PROFILE_MARK_FRAME();
        return false;
    }

    m_DeferFlag = true;
    Input::PollEvents();
    for (const Event &event : m_Window->GetNewEvents())
        onEvent(event);

    m_Window->FlushEvents();
    // Should maybe exit if window is closed at this point (triggered by event)

    onUpdate();

    const auto drawCalls = [this](const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
        onRender(p_CommandBuffer);
    };
    const auto uiSubmission = [this](const VkCommandBuffer p_CommandBuffer) {
        onLateRender(p_CommandBuffer);
        endRenderImGui(p_CommandBuffer);
    };

    m_Window->Render(drawCalls, uiSubmission);
    m_DeferFlag = false;
    updateUserLayerPointer();

    if (m_Window->ShouldClose()) [[unlikely]]
    {
        shutdownImGui();
        m_Window.Destruct();
        m_WindowAlive = false;
        TKIT_PROFILE_MARK_FRAME();
        return false;
    }
    m_DeltaTime = p_Clock.Restart();
    TKIT_PROFILE_MARK_FRAME();
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

template <typename F1, typename F2>
void MultiWindowApplication::processFrame(const u32 p_WindowIndex, F1 &&p_FirstDrawCalls, F2 &&p_LastDrawCalls) noexcept
{
    const auto &window = m_Windows[p_WindowIndex];
    for (const Event &event : window->GetNewEvents())
        onEvent(p_WindowIndex, event);

    window->FlushEvents();
    // Should maybe exit if window is closed at this point? (triggered by event)

    onUpdate(p_WindowIndex);

    window->Render(std::forward<F1>(p_FirstDrawCalls), std::forward<F2>(p_LastDrawCalls));
}

void MultiWindowApplication::Shutdown() noexcept
{
    CloseAllWindows();
    IApplication::Shutdown();
}

void MultiWindowApplication::CloseAllWindows() noexcept
{
    for (u32 i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        CloseWindow(i);
}

void MultiWindowApplication::CloseWindow(const Window *p_Window) noexcept
{
    for (u32 i = 0; i < m_Windows.size(); ++i)
        if (m_Windows[i].Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    TKIT_ERROR("Window was not found");
}

const Window *MultiWindowApplication::GetWindow(const u32 p_Index) const noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.size(), "[ONYX] Index out of bounds");
    return m_Windows[p_Index].Get();
}
Window *MultiWindowApplication::GetWindow(const u32 p_Index) noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.size(), "[ONYX] Index out of bounds");
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
    return m_Windows.size();
}

bool MultiWindowApplication::NextFrame(TKit::Clock &p_Clock) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::MultiWindowApplication::NextFrame");
    if (m_Windows.empty() || m_QuitFlag) [[unlikely]]
    {
        m_QuitFlag = false;
        TKIT_PROFILE_MARK_FRAME();
        return false;
    }

    Input::PollEvents();
    processWindows();

    m_DeltaTime = p_Clock.Restart();
    TKIT_PROFILE_MARK_FRAME();
    return !m_Windows.empty();
}

void MultiWindowApplication::CloseWindow(const u32 p_Index) noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.size(), "[ONYX] Index out of bounds");
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
        shutdownImGui();
        m_Windows.erase(m_Windows.begin() + p_Index);
        if (!m_Windows.empty())
            initializeImGui(*m_Windows[0]);
    }
    else
        m_Windows.erase(m_Windows.begin() + p_Index);
}

void MultiWindowApplication::OpenWindow(const Window::Specs &p_Specs) noexcept
{
    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    if (m_DeferFlag)
    {
        m_WindowsToAdd.push_back(p_Specs);
        return;
    }

    auto window = TKit::Scope<Window>::Create(p_Specs);
    if (m_Windows.empty())
        initializeImGui(*window);

    m_Windows.push_back(std::move(window));
    Event event;
    event.Type = Event::WindowOpened;
    event.Window = m_Windows.back().Get();
    onEvent(m_Windows.size() - 1, event);
}

void MultiWindowApplication::processWindows() noexcept
{
    m_DeferFlag = true;
    const auto drawCalls = [this](const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
        onRender(0, p_CommandBuffer);
        onImGuiRender();
    };
    const auto uiSubmission = [this](const VkCommandBuffer p_CommandBuffer) {
        onLateRender(0, p_CommandBuffer);
        endRenderImGui(p_CommandBuffer);
    };
    processFrame(0, drawCalls, uiSubmission);

    for (u32 i = 1; i < m_Windows.size(); ++i)
        processFrame(
            i, [this, i](const VkCommandBuffer p_CommandBuffer) { onRender(i, p_CommandBuffer); },
            [this, i](const VkCommandBuffer p_CommandBuffer) { onLateRender(i, p_CommandBuffer); });

    m_DeferFlag = false;
    updateUserLayerPointer();

    for (u32 i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
    for (const auto &specs : m_WindowsToAdd)
        OpenWindow(specs);
    m_WindowsToAdd.clear();
}

} // namespace Onyx
