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
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void IApplication::endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    // Lock the queues, as imgui requires them
    Core::LockGraphicsAndPresentQueues();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault(nullptr, p_CommandBuffer);
    Core::UnlockGraphicsAndPresentQueues();
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
    if (m_UserLayer)
        m_UserLayer->OnStart();
}
void IApplication::onShutdown() noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnShutdown();
}
void IApplication::onUpdate() noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnUpdate();
}
void IApplication::onRender(const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnRender(p_CommandBuffer);
}
void IApplication::onLateRender(const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnLateRender(p_CommandBuffer);
}
void IApplication::onEvent(const Event &p_Event) noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnEvent(p_Event);
}
void IApplication::onUpdate(const u32 p_WindowIndex) noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnUpdate(p_WindowIndex);
}
void IApplication::onRender(const u32 p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnRender(p_WindowIndex, p_CommandBuffer);
}
void IApplication::onLateRender(const u32 p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnLateRender(p_WindowIndex, p_CommandBuffer);
}
void IApplication::onEvent(const u32 p_WindowIndex, const Event &p_Event) noexcept
{
    if (m_UserLayer)
        m_UserLayer->OnEvent(p_WindowIndex, p_Event);
}
void IApplication::onImGuiRender() noexcept
{
    if (m_UserLayer)
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

bool Application::NextFrame(TKit::Clock &p_Clock) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::NextFrame");

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

    if (m_Window->ShouldClose())
    {
        m_Window->WaitForFrameSubmission();
        shutdownImGui();
        m_Window.Destruct();
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

TKit::Timespan Application::GetDeltaTime() const noexcept
{
    return m_DeltaTime;
}

} // namespace Onyx
