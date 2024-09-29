#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "kit/multiprocessing/task_manager.hpp"

namespace ONYX
{
// this function smells like shit
template <typename F>
static void processFrame(const usize p_WindowIndex, Window &p_Window, LayerSystem &p_Layers, F &&p_Submission) noexcept
{
    p_Window.MakeContextCurrent();
    for (const Event &event : p_Window.GetNewEvents())
        p_Layers.OnEvent(p_WindowIndex, event);
    p_Window.FlushEvents();
    if (p_Window.ShouldClose())
        return;
    // These are called in exactly the same context, but it is nice to have update/render separated
    p_Layers.OnUpdate(p_WindowIndex);
    p_Layers.OnRender(p_WindowIndex);
    KIT_ASSERT_RETURNS(p_Window.Display(std::forward<F>(p_Submission)), true,
                       "Failed to display the window. Failed to acquire a command buffer when beginning a new frame");
}
static void processFrame(const usize p_WindowIndex, Window &p_Window, LayerSystem &p_Layers) noexcept
{
    processFrame(p_WindowIndex, p_Window, p_Layers, [](const VkCommandBuffer) {});
}

IMultiWindowApplication::~IMultiWindowApplication() noexcept
{
    if (!m_Terminated && m_Started)
        Shutdown();
}

void IMultiWindowApplication::Draw(IDrawable &p_Drawable, usize p_WindowIndex) noexcept
{
    KIT_ASSERT(p_WindowIndex < m_Windows.size(), "Window index out of bounds");
    m_Windows[p_WindowIndex]->Draw(p_Drawable);
}

bool IMultiWindowApplication::Started() const noexcept
{
    return m_Started;
}
bool IMultiWindowApplication::Terminated() const noexcept
{
    return m_Terminated;
}

void IMultiWindowApplication::CloseAllWindows() noexcept
{
    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        CloseWindow(i);
}

void IMultiWindowApplication::CloseWindow(const Window *p_Window) noexcept
{
    for (usize i = 0; i < m_Windows.size(); ++i)
        if (m_Windows[i].Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    KIT_ERROR("Window was not found");
}

const Window *IMultiWindowApplication::GetWindow(const usize p_Index) const noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    return m_Windows[p_Index].Get();
}
Window *IMultiWindowApplication::GetWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    return m_Windows[p_Index].Get();
}
usize IMultiWindowApplication::GetWindowCount() const noexcept
{
    return m_Windows.size();
}

f32 IMultiWindowApplication::GetDeltaTime() const noexcept
{
    return m_DeltaTime.load(std::memory_order_relaxed);
}

void IMultiWindowApplication::Startup() noexcept
{
    KIT_ASSERT(!m_Terminated && !m_Started, "MultiWindowApplication already started");
    m_Started = true;
    Layers.OnStart();
}

void IMultiWindowApplication::Shutdown() noexcept
{
    KIT_ASSERT(!m_Terminated && m_Started, "MultiWindowApplication not started");
    Layers.OnShutdown();
    CloseAllWindows(); // Should not be that necessary
    if (m_Device)
        vkDestroyDescriptorPool(m_Device->GetDevice(), m_ImGuiPool, nullptr);
    m_Terminated = true;
}

bool IMultiWindowApplication::NextFrame(KIT::Clock &p_Clock) noexcept
{
    m_DeltaTime.store(p_Clock.Restart().AsSeconds(), std::memory_order_relaxed);
    if (m_Windows.empty())
        return false;

    Input::PollEvents();
    processWindows();
    return !m_Windows.empty();
}

void IMultiWindowApplication::Run() noexcept
{
    Startup();
    KIT::Clock clock;
    while (NextFrame(clock))
        ;
    Shutdown();
}

void IMultiWindowApplication::beginRenderImGui() noexcept
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void IMultiWindowApplication::endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    // Lock the queues, as imgui requires them
    m_Device->LockQueues();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault(nullptr, p_CommandBuffer);
    m_Device->UnlockQueues();
}

void IMultiWindowApplication::createImGuiPool() noexcept
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

void IMultiWindowApplication::initializeImGui(Window &p_Window) noexcept
{
    if (!m_ImGuiPool)
        createImGuiPool();
    ImGui::CreateContext();
#ifdef ONYX_ENABLE_IMPLOT
    ImPlot::CreateContext();
#endif

    IMGUI_CHECKVERSION();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

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

void IMultiWindowApplication::shutdownImGui() noexcept
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

void MultiWindowApplication<WindowFlow::SERIAL>::Draw(Window &p_Window, usize p_WindowIndex) noexcept
{
    KIT_ASSERT(p_WindowIndex < m_Windows.size(), "Window index out of bounds");
    m_Windows[p_WindowIndex]->Draw(p_Window);
}

void MultiWindowApplication<WindowFlow::SERIAL>::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    if (m_MainThreadProcessing)
    {
        m_Windows[p_Index]->FlagShouldClose();
        return;
    }
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

WindowFlow MultiWindowApplication<WindowFlow::SERIAL>::GetWindowFlow() const noexcept
{
    return WindowFlow::SERIAL;
}

Window *MultiWindowApplication<WindowFlow::SERIAL>::handleWindowAddition(KIT::Scope<Window> &&p_Window) noexcept
{
    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    Event event;
    event.Type = Event::WINDOW_OPENED;
    p_Window->PushEvent(event);
    if (m_Windows.empty())
    {
        m_Device = Core::GetDevice();
        initializeImGui(*p_Window);
    }
    m_Windows.push_back(std::move(p_Window));
    return m_Windows.back().Get();
}

void MultiWindowApplication<WindowFlow::SERIAL>::processWindows() noexcept
{
    m_MainThreadProcessing = true;
    processFrame(0, *m_Windows[0], Layers, [this](const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
        Layers.OnImGuiRender();
        endRenderImGui(p_CommandBuffer);
    });

    for (usize i = 1; i < m_Windows.size(); ++i)
        processFrame(i, *m_Windows[i], Layers);

    m_MainThreadProcessing = false;
    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
}

void MultiWindowApplication<WindowFlow::CONCURRENT>::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    if (m_MainThreadID != std::this_thread::get_id() || (p_Index == 0 && m_MainThreadProcessing))
    {
        m_Windows[p_Index]->FlagShouldClose();
        return;
    }
    if (Started())
        for (auto &task : m_Tasks)
            task->WaitUntilFinished();
    // It is now safe to resubmit all tasks

    m_Windows.erase(m_Windows.begin() + p_Index);
    // Check if the main window got removed. If so, imgui needs to be reinitialized with the new main window
    if (p_Index == 0)
    {
        shutdownImGui();
        if (m_Windows.empty())
            return;
        initializeImGui(*m_Windows[0]);
        m_Tasks.erase(m_Tasks.begin());
    }
    else
        m_Tasks.erase(m_Tasks.begin() + p_Index - 1);

    KIT::ITaskManager *taskManager = Core::GetTaskManager();
    for (usize i = 0; i < m_Tasks.size(); ++i)
    {
        m_Tasks[i] = createWindowTask(i + 1);
        if (Started())
            taskManager->SubmitTask(m_Tasks[i]);
    }
}

WindowFlow MultiWindowApplication<WindowFlow::CONCURRENT>::GetWindowFlow() const noexcept
{
    return WindowFlow::CONCURRENT;
}

void MultiWindowApplication<WindowFlow::CONCURRENT>::Startup() noexcept
{
    IMultiWindowApplication::Startup();

    KIT::ITaskManager *taskManager = Core::GetTaskManager();
    for (auto &task : m_Tasks)
        taskManager->SubmitTask(task);
}

KIT::Ref<KIT::Task<void>> MultiWindowApplication<WindowFlow::CONCURRENT>::createWindowTask(
    const usize p_WindowIndex) noexcept
{
    const KIT::ITaskManager *taskManager = Core::GetTaskManager();
    return taskManager->CreateTask(
        [this, p_WindowIndex](usize) { processFrame(p_WindowIndex, *m_Windows[p_WindowIndex], Layers); });
}

Window *MultiWindowApplication<WindowFlow::CONCURRENT>::handleWindowAddition(KIT::Scope<Window> &&p_Window) noexcept
{
    Window *windowPtr = p_Window.Get();
    m_Windows.push_back(std::move(p_Window));

    Event event;
    event.Type = Event::WINDOW_OPENED;

    // Dispatch immediately. In concurrent scenarios, layers may depend on the window index. If two windows are
    // opened before the start of te application, the window index 1 may OnEvent before window index 0, causing a
    // mismatch or a crash
    Layers.OnEvent(m_Windows.size() - 1, event);

    if (m_Windows.size() > 1)
    {
        KIT::ITaskManager *taskManager = Core::GetTaskManager();
        auto &task = m_Tasks.emplace_back(createWindowTask(m_Windows.size() - 1));
        if (Started())
            taskManager->SubmitTask(task);
    }
    else
    {
        // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due
        // to the GLFW ImGui backend limitations
        m_Device = Core::GetDevice();
        initializeImGui(*windowPtr);
    }

    return windowPtr;
}

void MultiWindowApplication<WindowFlow::CONCURRENT>::processWindows() noexcept
{
    KIT::ITaskManager *taskManager = Core::GetTaskManager();
    for (auto &task : m_Tasks)
    {
        task->WaitUntilFinished();
        task->Reset();
        taskManager->SubmitTask(task);
    }

    m_MainThreadProcessing = true;
    // Main thread always handles the first window. First element of tasks is always nullptr
    processFrame(0, *m_Windows[0], Layers, [this](const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
        Layers.OnImGuiRender();
        endRenderImGui(p_CommandBuffer);
    });
    m_MainThreadProcessing = false;

    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
}

} // namespace ONYX