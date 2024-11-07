#include "onyx/core/pch.hpp"
#include "onyx/app/mwapp.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "kit/multiprocessing/task_manager.hpp"

namespace ONYX
{
// this function smells like shit
template <typename F1, typename F2>
static void processFrame(const usize p_WindowIndex, Window &p_Window, LayerSystem &p_Layers, F1 &&p_DrawCalls,
                         F2 &&p_UICalls) noexcept
{
    for (const Event &event : p_Window.GetNewEvents())
        p_Layers.OnEvent(p_WindowIndex, event);
    p_Window.FlushEvents();
    // Should maybe exit if window is closed at this point (triggered by event)

    p_Layers.OnUpdate(p_WindowIndex);
    KIT_ASSERT_RETURNS(p_Window.Render(std::forward<F1>(p_DrawCalls), std::forward<F2>(p_UICalls)), true,
                       "Failed to render to the window. Failed to acquire a command buffer when beginning a new frame");
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

const Window *IMultiWindowApplication::GetMainWindow() const noexcept
{
    return GetWindow(0);
}
Window *IMultiWindowApplication::GetMainWindow() noexcept
{
    return GetWindow(0);
}

usize IMultiWindowApplication::GetWindowCount() const noexcept
{
    return m_Windows.size();
}

KIT::Timespan IMultiWindowApplication::GetDeltaTime() const noexcept
{
    return m_DeltaTime.load(std::memory_order_relaxed);
}

bool IMultiWindowApplication::NextFrame(KIT::Clock &p_Clock) noexcept
{
    m_DeltaTime.store(p_Clock.Restart(), std::memory_order_relaxed); // In case concurrent mode is used
    if (m_Windows.empty())
        return false;

    Input::PollEvents();
    processWindows();
    return !m_Windows.empty();
}

void MultiWindowApplication<WindowFlow::Serial>::CloseWindow(const usize p_Index) noexcept
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

WindowFlow MultiWindowApplication<WindowFlow::Serial>::GetWindowFlow() const noexcept
{
    return WindowFlow::Serial;
}

Window *MultiWindowApplication<WindowFlow::Serial>::OpenWindow(const Window::Specs &p_Specs) noexcept
{
    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    Event event;
    event.Type = Event::WindowOpened;

    auto window = KIT::Scope<Window>::Create(p_Specs);
    window->PushEvent(event);
    if (m_Windows.empty())
    {
        m_Device = Core::GetDevice();
        initializeImGui(*window);
    }
    m_Windows.push_back(std::move(window));
    return m_Windows.back().Get();
}

void MultiWindowApplication<WindowFlow::Serial>::processWindows() noexcept
{
    m_MainThreadProcessing = true;
    const auto drawCalls = [this](const VkCommandBuffer) {
        beginRenderImGui();
        Layers.OnRender(0);
        Layers.OnImGuiRender();
    };
    const auto uiSubmission = [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); };
    processFrame(0, *m_Windows[0], Layers, drawCalls, uiSubmission);

    for (usize i = 1; i < m_Windows.size(); ++i)
        processFrame(
            i, *m_Windows[i], Layers, [this, i](const VkCommandBuffer) { Layers.OnRender(i); },
            [](const VkCommandBuffer) {});

    m_MainThreadProcessing = false;
    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
}

void MultiWindowApplication<WindowFlow::Concurrent>::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    if (m_MainThreadID != std::this_thread::get_id() || (p_Index == 0 && m_MainThreadProcessing))
    {
        m_Windows[p_Index]->FlagShouldClose();
        return;
    }
    if (IsStarted())
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
        if (IsRunning())
            taskManager->SubmitTask(m_Tasks[i]);
    }
}

WindowFlow MultiWindowApplication<WindowFlow::Concurrent>::GetWindowFlow() const noexcept
{
    return WindowFlow::Concurrent;
}

void MultiWindowApplication<WindowFlow::Concurrent>::Startup() noexcept
{
    IMultiWindowApplication::Startup();

    KIT::ITaskManager *taskManager = Core::GetTaskManager();
    for (auto &task : m_Tasks)
        taskManager->SubmitTask(task);
}

KIT::Ref<KIT::Task<void>> MultiWindowApplication<WindowFlow::Concurrent>::createWindowTask(
    const usize p_WindowIndex) noexcept
{
    const KIT::ITaskManager *taskManager = Core::GetTaskManager();
    return taskManager->CreateTask([this, p_WindowIndex](usize) {
        processFrame(
            p_WindowIndex, *m_Windows[p_WindowIndex], Layers,
            [this, p_WindowIndex](const VkCommandBuffer) { Layers.OnRender(p_WindowIndex); },
            [](const VkCommandBuffer) {});
    });
}

Window *MultiWindowApplication<WindowFlow::Concurrent>::OpenWindow(const Window::Specs &p_Specs) noexcept
{
    auto window = KIT::Scope<Window>::Create(p_Specs);
    Window *windowPtr = window.Get();
    m_Windows.push_back(std::move(window));

    Event event;
    event.Type = Event::WindowOpened;

    // Dispatch immediately. In concurrent scenarios, layers may depend on the window index. If two windows are
    // opened before the start of te application, the window index 1 may OnEvent before window index 0, causing a
    // mismatch or a crash
    Layers.OnEvent(m_Windows.size() - 1, event);

    if (m_Windows.size() > 1)
    {
        KIT::ITaskManager *taskManager = Core::GetTaskManager();
        auto &task = m_Tasks.emplace_back(createWindowTask(m_Windows.size() - 1));
        if (IsRunning())
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

void MultiWindowApplication<WindowFlow::Concurrent>::processWindows() noexcept
{
    KIT::ITaskManager *taskManager = Core::GetTaskManager();
    for (auto &task : m_Tasks)
    {
        task->WaitUntilFinished();
        task->Reset();
        taskManager->SubmitTask(task);
    }

    const auto drawCalls = [this](const VkCommandBuffer) {
        beginRenderImGui();
        Layers.OnRender(0);
        Layers.OnImGuiRender();
    };
    const auto uiSubmission = [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); };

    m_MainThreadProcessing = true;
    // Main thread always handles the first window. First element of tasks is always nullptr
    processFrame(0, *m_Windows[0], Layers, drawCalls, uiSubmission);
    m_MainThreadProcessing = false;

    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
}

} // namespace ONYX