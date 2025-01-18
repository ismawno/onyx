#include "onyx/core/pch.hpp"
#include "onyx/app/mwapp.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "tkit/multiprocessing/task_manager.hpp"

namespace Onyx
{
// this function smells like shit
template <typename F1, typename F2>
void IMultiWindowApplication::processFrame(const u32 p_WindowIndex, F1 &&p_FirstDrawCalls,
                                           F2 &&p_LastDrawCalls) noexcept
{
    const auto &window = m_Windows[p_WindowIndex];
    for (const Event &event : window->GetNewEvents())
        onEvent(p_WindowIndex, event);

    window->FlushEvents();
    // Should maybe exit if window is closed at this point? (triggered by event)

    onUpdate(p_WindowIndex);

    window->Render(std::forward<F1>(p_FirstDrawCalls), std::forward<F2>(p_LastDrawCalls));
}

void IMultiWindowApplication::CloseAllWindows() noexcept
{
    for (u32 i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        CloseWindow(i);
}

void IMultiWindowApplication::CloseWindow(const Window *p_Window) noexcept
{
    for (u32 i = 0; i < m_Windows.size(); ++i)
        if (m_Windows[i].Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    TKIT_ERROR("Window was not found");
}

const Window *IMultiWindowApplication::GetWindow(const u32 p_Index) const noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.size(), "[ONYX] Index out of bounds");
    return m_Windows[p_Index].Get();
}
Window *IMultiWindowApplication::GetWindow(const u32 p_Index) noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.size(), "[ONYX] Index out of bounds");
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

u32 IMultiWindowApplication::GetWindowCount() const noexcept
{
    return m_Windows.size();
}

bool IMultiWindowApplication::NextFrame(TKit::Clock &p_Clock) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::IMultiWindowApplication::NextFrame");
    setDeltaTime(p_Clock.Restart());
    if (m_Windows.empty())
    {
        TKIT_PROFILE_MARK_FRAME();
        return false;
    }

    Input::PollEvents();
    processWindows();

    TKIT_PROFILE_MARK_FRAME();
    return !m_Windows.empty();
}

void MultiWindowApplication<Serial>::CloseWindow(const u32 p_Index) noexcept
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
        m_Windows[p_Index]->WaitForFrameSubmission();
        shutdownImGui();
        m_Windows.erase(m_Windows.begin() + p_Index);
        if (!m_Windows.empty())
            initializeImGui(*m_Windows[0]);
    }
    else
        m_Windows.erase(m_Windows.begin() + p_Index);
}

WindowThreading MultiWindowApplication<Serial>::GetWindowThreading() const noexcept
{
    return Serial;
}

void MultiWindowApplication<Serial>::OpenWindow(const Window::Specs &p_Specs) noexcept
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

void MultiWindowApplication<Serial>::processWindows() noexcept
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

TKit::Timespan MultiWindowApplication<Serial>::GetDeltaTime() const noexcept
{
    return m_DeltaTime;
}

void MultiWindowApplication<Serial>::setDeltaTime(const TKit::Timespan p_DeltaTime) noexcept
{
    m_DeltaTime = p_DeltaTime;
}

void MultiWindowApplication<Concurrent>::CloseWindow(const u32 p_Index) noexcept
{
    TKIT_ASSERT(p_Index < m_Windows.size(), "[ONYX] Index out of bounds");

    // m_DeferFlag does not need to be atomic because only way to get to the second part is by being the
    // main thread

    if (m_MainThreadID != std::this_thread::get_id() || m_DeferFlag)
    {
        m_Windows[p_Index]->FlagShouldClose();
        return;
    }

    // No tasks are running at this point, so we can safely remove the window

    Event event;
    event.Type = Event::WindowClosed;
    event.Window = m_Windows[p_Index].Get();
    onEvent(p_Index, event);

    // Check if the main window got removed. If so, imgui needs to be reinitialized with the new main window
    if (p_Index == 0)
    {
        m_Windows[p_Index]->WaitForFrameSubmission();
        shutdownImGui();
        m_Windows.erase(m_Windows.begin() + p_Index);
        if (!m_Windows.empty())
        {
            initializeImGui(*m_Windows[0]);
            m_Tasks.erase(m_Tasks.begin());
        }
    }
    else
    {
        m_Windows.erase(m_Windows.begin() + p_Index);
        m_Tasks.erase(m_Tasks.begin() + p_Index - 1);
    }

    for (u32 i = 0; i < m_Tasks.size(); ++i)
        m_Tasks[i] = createWindowTask(i + 1);
}

WindowThreading MultiWindowApplication<Concurrent>::GetWindowThreading() const noexcept
{
    return Concurrent;
}

void MultiWindowApplication<Concurrent>::Startup() noexcept
{
    IMultiWindowApplication::Startup();
    m_DeferFlag = true;
    TKit::ITaskManager *taskManager = Core::GetTaskManager();
    for (auto &task : m_Tasks)
        taskManager->SubmitTask(task);
}

TKit::Ref<TKit::Task<void>> MultiWindowApplication<Concurrent>::createWindowTask(const u32 p_WindowIndex) noexcept
{
    const TKit::ITaskManager *taskManager = Core::GetTaskManager();
    return taskManager->CreateTask([this, p_WindowIndex](u32) {
        processFrame(
            p_WindowIndex,
            [this, p_WindowIndex](const VkCommandBuffer p_CommandBuffer) { onRender(p_WindowIndex, p_CommandBuffer); },
            [this, p_WindowIndex](const VkCommandBuffer p_CommandBuffer) {
                onLateRender(p_WindowIndex, p_CommandBuffer);
            });
    });
}

void MultiWindowApplication<Concurrent>::OpenWindow(const Window::Specs &p_Specs) noexcept
{
    if (m_MainThreadID != std::this_thread::get_id() || m_DeferFlag)
    {
        std::scoped_lock lock(m_WindowsToAddMutex);
        m_WindowsToAdd.push_back(p_Specs);
        return;
    }
    auto window = TKit::Scope<Window>::Create(p_Specs);
    Window *windowPtr = window.Get();
    m_Windows.push_back(std::move(window));

    if (m_Windows.size() > 1)
        m_Tasks.push_back(createWindowTask(m_Windows.size() - 1));
    else
        // This application, although supports multiple GLFW windows, will only operate under a single ImGui context due
        // to the GLFW ImGui backend limitations
        initializeImGui(*windowPtr);

    Event event;
    event.Type = Event::WindowOpened;
    event.Window = windowPtr;
    onEvent(m_Windows.size() - 1, event);
}

void MultiWindowApplication<Concurrent>::processWindows() noexcept
{
    for (auto &task : m_Tasks)
    {
        task->WaitUntilFinished();
        task->Reset();
    }
    m_DeferFlag = false;
    updateUserLayerPointer();

    for (u32 i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
    for (const auto &specs : m_WindowsToAdd)
        OpenWindow(specs);
    m_WindowsToAdd.clear();

    if (m_Windows.empty())
        return;

    m_DeferFlag = true;
    TKit::ITaskManager *taskManager = Core::GetTaskManager();
    for (auto &task : m_Tasks)
        taskManager->SubmitTask(task);

    const auto drawCalls = [this](const VkCommandBuffer p_CommandBuffer) {
        beginRenderImGui();
        onRender(0, p_CommandBuffer);
        onImGuiRender();
    };
    const auto uiSubmission = [this](const VkCommandBuffer p_CommandBuffer) {
        onLateRender(0, p_CommandBuffer);
        endRenderImGui(p_CommandBuffer);
    };

    // Main thread always handles the first window. Array of tasks is always one element smaller than the array of
    // windows
    processFrame(0, drawCalls, uiSubmission);
}

TKit::Timespan MultiWindowApplication<Concurrent>::GetDeltaTime() const noexcept
{
    return m_DeltaTime;
}

void MultiWindowApplication<Concurrent>::setDeltaTime(const TKit::Timespan p_DeltaTime) noexcept
{
    m_DeltaTime = p_DeltaTime;
}

} // namespace Onyx