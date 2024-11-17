#include "onyx/core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"

#include "kit/profiling/macros.hpp"
#include "kit/profiling/vulkan.hpp"

namespace ONYX
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

void IApplication::Startup() noexcept
{
    KIT_ASSERT(!m_Terminated && !m_Started, "Application cannot be started more than once");
    m_Started = true;
    Layers.OnStart();
}

void IApplication::Shutdown() noexcept
{
    KIT_ASSERT(!m_Terminated && m_Started, "Application cannot be terminated before it is started");
    Layers.OnShutdown();
    if (m_Device)
        vkDestroyDescriptorPool(m_Device->GetDevice(), m_ImGuiPool, nullptr);
    m_Terminated = true;
}

void IApplication::ApplyTheme() noexcept
{
    KIT_ASSERT(m_Theme, "No theme has been set. Set one with SetTheme");
    m_Theme->Apply();
}

void IApplication::Run() noexcept
{
    Startup();
    KIT::Clock clock;
    while (NextFrame(clock))
        ;
    Shutdown();
}

void IApplication::beginRenderImGui() noexcept
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void IApplication::endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    // Lock the queues, as imgui requires them
    m_Device->LockQueues();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault(nullptr, p_CommandBuffer);
    m_Device->UnlockQueues();
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

    KIT_ASSERT_RETURNS(vkCreateDescriptorPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_ImGuiPool), VK_SUCCESS,
                       "Failed to create descriptor pool");
}

void IApplication::initializeImGui(Window &p_Window) noexcept
{
    if (!m_ImGuiPool)
        createImGuiPool();
    if (!m_Theme)
        m_Theme = KIT::Scope<DefaultTheme>::Create();

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

    const auto &instance = Core::GetInstance();
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance->GetInstance();
    initInfo.PhysicalDevice = m_Device->GetPhysicalDevice();
    initInfo.Device = m_Device->GetDevice();
    initInfo.Queue = m_Device->GetGraphicsQueue();
    initInfo.DescriptorPool = m_ImGuiPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = p_Window.GetFrameScheduler().GetSwapChain().GetRenderPass();

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void IApplication::shutdownImGui() noexcept
{
    m_Device->WaitIdle();
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
    m_Window.Create(p_WindowSpecs);
    m_Device = Core::GetDevice();
    initializeImGui(*m_Window);
}

bool Application::NextFrame(KIT::Clock &p_Clock) noexcept
{
    KIT_PROFILE_NSCOPE("ONYX::Application::NextFrame");
    m_DeltaTime = p_Clock.Restart();
    Input::PollEvents();
    for (const Event &event : m_Window->GetNewEvents())
        Layers.OnEvent(event);

    m_Window->FlushEvents();
    // Should maybe exit if window is closed at this point (triggered by event)

    Layers.OnUpdate();

    const auto drawCalls = [this](const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
        Layers.OnRender(p_CommandBuffer);
    };
    const auto uiSubmission = [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); };

    KIT_ASSERT_RETURNS(m_Window->Render(drawCalls, uiSubmission), true,
                       "Failed to render to the window. Failed to acquire a command buffer when beginning a new frame");
    if (m_Window->ShouldClose())
    {
        m_Window.Destroy();
        shutdownImGui();
        KIT_PROFILE_MARK_FRAME;
        return false;
    }
    KIT_PROFILE_MARK_FRAME;
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

KIT::Timespan Application::GetDeltaTime() const noexcept
{
    return m_DeltaTime;
}

} // namespace ONYX
