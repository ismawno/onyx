#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "kit/multiprocessing/task_manager.hpp"

namespace ONYX
{
template <typename F> static void runFrame(Window &p_Window, F &&p_Submission) noexcept
{
    p_Window.MakeContextCurrent();
    KIT_ASSERT_RETURNS(p_Window.Display(std::forward<F>(p_Submission)), true,
                       "Failed to display the window. Failed to acquire a command buffer when beginning a new frame");
}
static void runFrame(Window &p_Window) noexcept
{
    runFrame(p_Window, [](const VkCommandBuffer) {});
}

template <MultiWindowFlow Flow> Application<Flow>::~Application() noexcept
{
    if (!m_Terminated && m_Started)
        Shutdown();
}
template <MultiWindowFlow Flow> Window *Application<Flow>::openWindow(const Window::Specs &p_Specs) noexcept
{
    auto window = KIT::Scope<Window>::Create(p_Specs);

    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    if (m_WindowData.empty())
    {
        m_Device = Core::GetDevice();
        initializeImGui(*window);
    }
    Window *windowPtr = window.Get();

    WindowData<Flow> windowData;
    windowData.Window = std::move(window);
    if constexpr (Flow == MultiWindowFlow::CONCURRENT)
        windowData.Task = nullptr;
    m_WindowData.push_back(std::move(windowData));

    return windowPtr;
}

template <MultiWindowFlow Flow> void Application<Flow>::Draw(Drawable &p_Drawable, usize p_WindowIndex) noexcept
{
    KIT_ASSERT(p_WindowIndex < m_WindowData.size(), "Index out of bounds");
    m_WindowData[p_WindowIndex].Window->Draw(p_Drawable);
}

template <MultiWindowFlow Flow> void Application<Flow>::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_WindowData.size(), "Index out of bounds");
    if constexpr (Flow == MultiWindowFlow::CONCURRENT)
    {
        if (m_WindowData[p_Index].Task)
            m_WindowData[p_Index].Task->WaitUntilFinished();
    }

    m_WindowData.erase(m_WindowData.begin() + p_Index);
    // Check if the main window got removed. If so, the main thread will handle the next window
    if (p_Index == 0)
    {
        shutdownImGui();
        if (m_WindowData.empty())
            return;
        if constexpr (Flow == MultiWindowFlow::CONCURRENT)
        {
            m_WindowData[0].Task->WaitUntilFinished();
            m_WindowData[0].Task = nullptr;
        }
        initializeImGui(*m_WindowData[0].Window);
    }
}

template <MultiWindowFlow Flow> void Application<Flow>::CloseWindow(const Window *p_Window) noexcept
{
    for (usize i = 0; i < m_WindowData.size(); ++i)
        if (m_WindowData[i].Window.Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    KIT_ERROR("Window was not found");
}

template <MultiWindowFlow Flow> void Application<Flow>::Start() noexcept
{
    KIT_ASSERT(!m_Terminated && !m_Started, "Application already started");
    m_Started = true;
}

template <MultiWindowFlow Flow> void Application<Flow>::Shutdown() noexcept
{
    KIT_ASSERT(!m_Terminated && m_Started, "Application not started");
    for (usize i = m_WindowData.size() - 1; i < m_WindowData.size(); --i)
        if (m_WindowData[i].Window->ShouldClose())
            CloseWindow(i);
    if (m_Device)
        vkDestroyDescriptorPool(m_Device->GetDevice(), m_ImGuiPool, nullptr);
    m_Terminated = true;
}

template <MultiWindowFlow Flow> bool Application<Flow>::NextFrame() noexcept
{
    if (m_WindowData.empty())
        return false;

    Input::PollEvents();
    runAndManageWindows();
    return !m_WindowData.empty();
}

template <MultiWindowFlow Flow> void Application<Flow>::Run() noexcept
{
    Start();
    while (NextFrame())
        ;
    Shutdown();
}

template <MultiWindowFlow Flow> void Application<Flow>::runAndManageWindows() noexcept
{
    if constexpr (Flow == MultiWindowFlow::CONCURRENT)
    {
        for (usize i = 1; i < m_WindowData.size(); ++i)
        {
            WindowData<Flow> &windowData = m_WindowData[i];
            KIT::TaskManager *taskManager = Core::GetTaskManager();
            if (!windowData.Task)
            {
                Window *window = windowData.Window.Get();
                windowData.Task = taskManager->CreateAndSubmit([window](usize) { runFrame(*window); });
            }
            else
            {
                windowData.Task->WaitUntilFinished();
                windowData.Task->Reset();
                taskManager->SubmitTask(windowData.Task);
            }
        }

        beginRenderImGui();
        // Main thread always handles the first window. First element of tasks is always nullptr
        runFrame(*m_WindowData[0].Window,
                 [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); });
    }
    else
    {
        beginRenderImGui();
        runFrame(*m_WindowData[0].Window,
                 [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); });
        for (usize i = 1; i < m_WindowData.size(); ++i)
            runFrame(*m_WindowData[i].Window);
    }

    for (usize i = m_WindowData.size() - 1; i < m_WindowData.size(); --i)
        if (m_WindowData[i].Window->ShouldClose())
            CloseWindow(i);
}

template <MultiWindowFlow Flow> void Application<Flow>::beginRenderImGui() noexcept
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

template <MultiWindowFlow Flow> void Application<Flow>::endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    // Lock the queues, as imgui requires them
    m_Device->LockQueues();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault(nullptr, p_CommandBuffer);
    m_Device->UnlockQueues();
}

template <MultiWindowFlow Flow> void Application<Flow>::createImGuiPool() noexcept
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

template <MultiWindowFlow Flow> void Application<Flow>::initializeImGui(Window &p_Window) noexcept
{
    if (!m_ImGuiPool)
        createImGuiPool();
    ImGui::CreateContext();

    IMGUI_CHECKVERSION();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(p_Window.GetWindow(), true);

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
    initInfo.RenderPass = p_Window.GetRenderer().GetSwapChain().GetRenderPass();

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

template <MultiWindowFlow Flow> void Application<Flow>::shutdownImGui() noexcept
{
    m_Device->WaitIdle();
    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyPlatformWindows();
    ImGui::DestroyContext();
}

template class Application<MultiWindowFlow::SERIAL>;
template class Application<MultiWindowFlow::CONCURRENT>;
} // namespace ONYX