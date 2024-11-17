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
    // Should maybe exit if window is closed at this point? (triggered by event)

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

bool IMultiWindowApplication::NextFrame(KIT::Clock &p_Clock) noexcept
{
    setDeltaTime(p_Clock.Restart());
    if (m_Windows.empty())
        return false;

    Input::PollEvents();
    processWindows();
    return !m_Windows.empty();
}

void MultiWindowApplication<Serial>::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    if (m_MustDeferWindowManagement)
    {
        m_Windows[p_Index]->FlagShouldClose();
        return;
    }
    Event event;
    event.Type = Event::WindowClosed;
    event.Window = m_Windows[p_Index].Get();
    Layers.OnEvent(p_Index, event);

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

WindowThreading MultiWindowApplication<Serial>::GetWindowThreading() const noexcept
{
    return Serial;
}

void MultiWindowApplication<Serial>::OpenWindow(const Window::Specs &p_Specs) noexcept
{
    // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due to
    // the GLFW ImGui backend limitations
    if (m_MustDeferWindowManagement)
    {
        m_WindowsToAdd.push_back(p_Specs);
        return;
    }

    auto window = KIT::Scope<Window>::Create(p_Specs);
    if (m_Windows.empty())
    {
        m_Device = Core::GetDevice();
        initializeImGui(*window);
    }
    m_Windows.push_back(std::move(window));
    Event event;
    event.Type = Event::WindowOpened;
    event.Window = m_Windows.back().Get();
    Layers.OnEvent(m_Windows.size() - 1, event);
}

void MultiWindowApplication<Serial>::processWindows() noexcept
{
    m_MustDeferWindowManagement = true;
    const auto drawCalls = [this](const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
        Layers.OnRender(0, p_CommandBuffer);
        Layers.OnImGuiRender();
    };
    const auto uiSubmission = [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); };
    processFrame(0, *m_Windows[0], Layers, drawCalls, uiSubmission);

    for (usize i = 1; i < m_Windows.size(); ++i)
        processFrame(
            i, *m_Windows[i], Layers,
            [this, i](const VkCommandBuffer p_CommandBuffer) { Layers.OnRender(i, p_CommandBuffer); },
            [](const VkCommandBuffer) {});

    m_MustDeferWindowManagement = false;
    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
    for (const auto &specs : m_WindowsToAdd)
        OpenWindow(specs);
    m_WindowsToAdd.clear();
}

KIT::Timespan MultiWindowApplication<Serial>::GetDeltaTime() const noexcept
{
    return m_DeltaTime;
}

void MultiWindowApplication<Serial>::setDeltaTime(const KIT::Timespan p_DeltaTime) noexcept
{
    m_DeltaTime = p_DeltaTime;
}

void MultiWindowApplication<Concurrent>::CloseWindow(const usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");

    // m_MustDeferWindowManagement does not need to be atomic because only way to get to the second part is by being the
    // main thread

    if (m_MainThreadID != std::this_thread::get_id() || m_MustDeferWindowManagement)
    {
        m_Windows[p_Index]->FlagShouldClose();
        return;
    }

    // No tasks are running at this point, so we can safely remove the window

    Event event;
    event.Type = Event::WindowClosed;
    event.Window = m_Windows[p_Index].Get();
    Layers.OnEvent(p_Index, event);

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

    for (usize i = 0; i < m_Tasks.size(); ++i)
        m_Tasks[i] = createWindowTask(i + 1);
}

WindowThreading MultiWindowApplication<Concurrent>::GetWindowThreading() const noexcept
{
    return Concurrent;
}

void MultiWindowApplication<Concurrent>::Startup() noexcept
{
    IMultiWindowApplication::Startup();
    KIT::ITaskManager *taskManager = Core::GetTaskManager();
    for (auto &task : m_Tasks)
        taskManager->SubmitTask(task);
}

KIT::Ref<KIT::Task<void>> MultiWindowApplication<Concurrent>::createWindowTask(const usize p_WindowIndex) noexcept
{
    const KIT::ITaskManager *taskManager = Core::GetTaskManager();
    return taskManager->CreateTask([this, p_WindowIndex](usize) {
        processFrame(
            p_WindowIndex, *m_Windows[p_WindowIndex], Layers,
            [this, p_WindowIndex](const VkCommandBuffer p_CommandBuffer) {
                Layers.OnRender(p_WindowIndex, p_CommandBuffer);
            },
            [](const VkCommandBuffer) {});
    });
}

void MultiWindowApplication<Concurrent>::OpenWindow(const Window::Specs &p_Specs) noexcept
{
    if (m_MainThreadID != std::this_thread::get_id() || m_MustDeferWindowManagement)
    {
        std::scoped_lock lock(m_WindowsToAddMutex);
        m_WindowsToAdd.push_back(p_Specs);
        return;
    }
    auto window = KIT::Scope<Window>::Create(p_Specs);
    Window *windowPtr = window.Get();
    m_Windows.push_back(std::move(window));

    if (m_Windows.size() > 1)
        m_Tasks.push_back(createWindowTask(m_Windows.size() - 1));
    else
    {
        // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due
        // to the GLFW ImGui backend limitations
        m_Device = Core::GetDevice();
        initializeImGui(*windowPtr);
    }

    Event event;
    event.Type = Event::WindowOpened;
    event.Window = windowPtr;
    Layers.OnEvent(m_Windows.size() - 1, event);
}

void MultiWindowApplication<Concurrent>::processWindows() noexcept
{
    for (auto &task : m_Tasks)
    {
        task->WaitUntilFinished();
        task->Reset();
    }

    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
    for (const auto &specs : m_WindowsToAdd)
        OpenWindow(specs);
    m_WindowsToAdd.clear();

    if (m_Windows.empty())
        return;

    m_MustDeferWindowManagement = true;
    KIT::ITaskManager *taskManager = Core::GetTaskManager();
    for (auto &task : m_Tasks)
        taskManager->SubmitTask(task);

    const auto drawCalls = [this](const VkCommandBuffer) {
        beginRenderImGui();
        Layers.OnRender(0);
        Layers.OnImGuiRender();
    };
    const auto uiSubmission = [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); };

    // Main thread always handles the first window. First element of tasks is always nullptr
    processFrame(0, *m_Windows[0], Layers, drawCalls, uiSubmission);
    m_MustDeferWindowManagement = false;
}

KIT::Timespan MultiWindowApplication<Concurrent>::GetDeltaTime() const noexcept
{
    return m_DeltaTime;
}

void MultiWindowApplication<Concurrent>::setDeltaTime(const KIT::Timespan p_DeltaTime) noexcept
{
    m_DeltaTime = p_DeltaTime;
}

} // namespace ONYX