#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "kit/multiprocessing/task_manager.hpp"

namespace ONYX
{
template <typename F> static void runFrame(IWindow &p_Window, F &&p_Submission) noexcept
{
    p_Window.MakeContextCurrent();
    KIT_ASSERT_RETURNS(p_Window.Display(std::forward<F>(p_Submission)), true,
                       "Failed to display the window. Failed to acquire a command buffer when beginning a new frame");
}
static void runFrame(IWindow &p_Window) noexcept
{
    runFrame(p_Window, [](const VkCommandBuffer) {});
}

Application::~Application() noexcept
{
    if (!m_Terminated && m_Started)
        Shutdown();
}
ONYX_DIMENSION_TEMPLATE Window<N> *Application::OpenWindow(const typename Window<N>::Specs &p_Specs) noexcept
{
    auto window = KIT::Scope<Window<N>>::Create(p_Specs);

    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    if (m_WindowData.empty())
    {
        m_Device = Core::Device();
        initializeImGui(*window);
    }
    Window<N> *windowPtr = window.Get();

    WindowData windowData;
    windowData.Window = std::move(window);
    windowData.Task = nullptr;
    m_WindowData.push_back(std::move(windowData));

    return windowPtr;
}

ONYX_DIMENSION_TEMPLATE Window<N> *Application::OpenWindow() noexcept
{
    return OpenWindow<N>(typename Window<N>::Specs{});
}

Window2D *Application::OpenWindow2D(const Window2D::Specs &p_Specs) noexcept
{
    return OpenWindow<2>(p_Specs);
}
Window2D *Application::OpenWindow2D() noexcept
{
    return OpenWindow<2>();
}

Window3D *Application::OpenWindow3D(const Window3D::Specs &p_Specs) noexcept
{
    return OpenWindow<3>(p_Specs);
}
Window3D *Application::OpenWindow3D() noexcept
{
    return OpenWindow<3>();
}

void Application::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_WindowData.size(), "Index out of bounds");
    if (m_WindowData[p_Index].Task)
        m_WindowData[p_Index].Task->WaitUntilFinished();

    m_WindowData.erase(m_WindowData.begin() + p_Index);
    // Check if the main window got removed. If so, the main thread will handle the next window
    if (p_Index == 0)
    {
        shutdownImGui();
        if (m_WindowData.empty())
            return;
        m_WindowData[0].Task->WaitUntilFinished();
        m_WindowData[0].Task = nullptr;
        initializeImGui(*m_WindowData[0].Window);
    }
}

void Application::CloseWindow(const IWindow *p_Window) noexcept
{
    for (usize i = 0; i < m_WindowData.size(); ++i)
        if (m_WindowData[i].Window.Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    KIT_ERROR("Window was not found");
}

void Application::Start() noexcept
{
    KIT_ASSERT(!m_Terminated && !m_Started, "Application already started");
    m_Started = true;
}

void Application::Shutdown() noexcept
{
    KIT_ASSERT(!m_Terminated && m_Started, "Application not started");
    for (usize i = m_WindowData.size() - 1; i < m_WindowData.size(); --i)
        if (m_WindowData[i].Window->ShouldClose())
            CloseWindow(i);
    if (m_Device)
        vkDestroyDescriptorPool(m_Device->VulkanDevice(), m_ImGuiPool, nullptr);
    m_Terminated = true;
}

bool Application::NextFrame() noexcept
{
    if (m_WindowData.empty())
        return false;

    Input::PollEvents();
    runAndManageWindows();
    return !m_WindowData.empty();
}

void Application::Run() noexcept
{
    Start();
    while (NextFrame())
        ;
    Shutdown();
}

void Application::runAndManageWindows() noexcept
{
    for (usize i = 1; i < m_WindowData.size(); ++i)
    {
        KIT::TaskManager *taskManager = Core::TaskManager();
        WindowData &windowData = m_WindowData[i];
        if (!windowData.Task)
        {
            IWindow *window = windowData.Window.Get();
            windowData.Task = taskManager->CreateAndSubmit([window](usize) { runFrame(*window); });
        }
        else
        {
            windowData.Task->WaitUntilFinished();
            windowData.Task->Reset();
            taskManager->SubmitTask(windowData.Task);
        }
    }
    static bool openWindow = false;
    if (openWindow)
    {
        OpenWindow2D();
        openWindow = false;
    }

    beginRenderImGui();
    ImGui::Begin("Windows");
    openWindow = ImGui::Button("Open window");
    ImGui::Text("Number of windows: %zu", m_WindowData.size());
    ImGui::End();
    // Main thread always handles the first window. First element of tasks is always nullptr
    runFrame(*m_WindowData[0].Window,
             [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); });

    for (usize i = m_WindowData.size() - 1; i < m_WindowData.size(); --i)
        if (m_WindowData[i].Window->ShouldClose())
            CloseWindow(i);
}

void Application::beginRenderImGui() noexcept
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Application::endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    // Lock the queues, as imgui requires them
    m_Device->LockQueues();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault(nullptr, p_CommandBuffer);
    m_Device->UnlockQueues();
}

void Application::createImGuiPool() noexcept
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

    KIT_ASSERT_RETURNS(vkCreateDescriptorPool(m_Device->VulkanDevice(), &poolInfo, nullptr, &m_ImGuiPool), VK_SUCCESS,
                       "Failed to create descriptor pool");
}

void Application::initializeImGui(IWindow &p_Window) noexcept
{
    if (!m_ImGuiPool)
        createImGuiPool();
    ImGui::CreateContext();

    IMGUI_CHECKVERSION();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(p_Window.GLFWWindow(), true);

    const auto &instance = Core::Instance();
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance->VulkanInstance();
    initInfo.PhysicalDevice = m_Device->PhysicalDevice();
    initInfo.Device = m_Device->VulkanDevice();
    initInfo.Queue = m_Device->GraphicsQueue();
    initInfo.DescriptorPool = m_ImGuiPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = p_Window.GetRenderer().GetSwapChain().RenderPass();

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void Application::shutdownImGui() noexcept
{
    m_Device->WaitIdle();
    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyPlatformWindows();
    ImGui::DestroyContext();
}
} // namespace ONYX