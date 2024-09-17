#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "kit/multiprocessing/task_manager.hpp"

namespace ONYX
{
template <typename F> static void processFrame(Window &p_Window, F &&p_Submission) noexcept
{
    p_Window.MakeContextCurrent();
    KIT_ASSERT_RETURNS(p_Window.Display(std::forward<F>(p_Submission)), true,
                       "Failed to display the window. Failed to acquire a command buffer when beginning a new frame");
}
static void processFrame(Window &p_Window) noexcept
{
    processFrame(p_Window, [](const VkCommandBuffer) {});
}

IApplication::~IApplication() noexcept
{
    if (!m_Terminated && m_Started)
        Shutdown();
}

void IApplication::Draw(Drawable &p_Drawable, usize p_WindowIndex) noexcept
{
    KIT_ASSERT(p_WindowIndex < m_Windows.size(), "Window index out of bounds");
    m_Windows[p_WindowIndex]->Draw(p_Drawable);
}

void IApplication::CloseAllWindows() noexcept
{
    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        CloseWindow(i);
}

void IApplication::CloseWindow(const Window *p_Window) noexcept
{
    for (usize i = 0; i < m_Windows.size(); ++i)
        if (m_Windows[i].Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    KIT_ERROR("Window was not found");
}

const Window *IApplication::GetWindow(const usize p_Index) const noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    return m_Windows[p_Index].Get();
}
Window *IApplication::GetWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    return m_Windows[p_Index].Get();
}

void IApplication::Start() noexcept
{
    KIT_ASSERT(!m_Terminated && !m_Started, "Application already started");
    m_Started = true;
    Layers.OnStart();
}

void IApplication::Shutdown() noexcept
{
    KIT_ASSERT(!m_Terminated && m_Started, "Application not started");
    Layers.OnShutdown();
    CloseAllWindows(); // Should not be that necessary
    if (m_Device)
        vkDestroyDescriptorPool(m_Device->GetDevice(), m_ImGuiPool, nullptr);
    m_Terminated = true;
}

bool IApplication::NextFrame() noexcept
{
    if (m_Windows.empty())
        return false;

    Input::PollEvents();
    processWindows();
    return !m_Windows.empty();
}

void IApplication::Run() noexcept
{
    Start();
    while (NextFrame())
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

void IApplication::shutdownImGui() noexcept
{
    m_Device->WaitIdle();
    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyPlatformWindows();
    ImGui::DestroyContext();
}

void Application<MultiWindowFlow::SERIAL>::Draw(Window &p_Window, usize p_WindowIndex) noexcept
{
    KIT_ASSERT(p_WindowIndex < m_Windows.size(), "Window index out of bounds");
    m_Windows[p_WindowIndex]->Draw(p_Window);
}

void Application<MultiWindowFlow::SERIAL>::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    m_Windows.erase(m_Windows.begin() + p_Index);
    // Check if the main window got removed. If so, imgui needs to be reinitialized with the new main window
    if (p_Index == 0)
    {
        shutdownImGui();
        if (m_Windows.empty())
            return;
        initializeImGui(*m_Windows[0]);
    }
}

Window *Application<MultiWindowFlow::SERIAL>::openWindow(const Window::Specs &p_Specs) noexcept
{
    auto window = KIT::Scope<Window>::Create(p_Specs);

    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    if (m_Windows.empty())
    {
        m_Device = Core::GetDevice();
        initializeImGui(*window);
    }
    m_Windows.push_back(std::move(window));
    return m_Windows.back().Get();
}

void Application<MultiWindowFlow::SERIAL>::processWindows() noexcept
{
    beginRenderImGui();
    processFrame(*m_Windows[0], [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); });
    for (usize i = 1; i < m_Windows.size(); ++i)
        processFrame(*m_Windows[i]);

    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
}

void Application<MultiWindowFlow::CONCURRENT>::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    m_Windows.erase(m_Windows.begin() + p_Index);
    m_Tasks.erase(m_Tasks.begin() + p_Index);
    // Check if the main window got removed. If so, imgui needs to be reinitialized with the new main window
    if (p_Index == 0)
    {
        shutdownImGui();
        if (m_Windows.empty())
            return;

        m_Tasks[0]->WaitUntilFinished();
        m_Tasks[0] = nullptr;
        initializeImGui(*m_Windows[0]);
    }
}

Window *Application<MultiWindowFlow::CONCURRENT>::openWindow(const Window::Specs &p_Specs) noexcept
{
    auto window = KIT::Scope<Window>::Create(p_Specs);
    Window *windowPtr = window.Get();

    if (!m_Windows.empty())
    {
        KIT::TaskManager *taskManager = Core::GetTaskManager();
        auto task = taskManager->CreateTask([windowPtr](usize) { processFrame(*windowPtr); });
        m_Tasks.push_back(std::move(task));
    }
    else
    {
        // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due
        // to the GLFW ImGui backend limitations

        m_Device = Core::GetDevice();
        initializeImGui(*window);
        m_Tasks.push_back(nullptr);
    }

    m_Windows.push_back(std::move(window));
    return windowPtr;
}

void Application<MultiWindowFlow::CONCURRENT>::processWindows() noexcept
{
    for (usize i = 1; i < m_Windows.size(); ++i)
    {
        auto &task = m_Tasks[i];
        KIT::TaskManager *taskManager = Core::GetTaskManager();
        if (!task)
        {
            Window *windowPtr = m_Windows[i].Get();
            task = taskManager->CreateAndSubmit([windowPtr](usize) { processFrame(*windowPtr); });
        }
        else
        {
            task->WaitUntilFinished();
            task->Reset();
            taskManager->SubmitTask(task);
        }
    }

    beginRenderImGui();
    // Main thread always handles the first window. First element of tasks is always nullptr
    processFrame(*m_Windows[0], [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); });

    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
}
} // namespace ONYX