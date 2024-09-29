#include "core/pch.hpp"
#include "onyx/app/mwapp.hpp"
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
    // Should maybe exit if window is closed at this point (triggered by event)

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

void IMultiWindowApplication::Draw(IDrawable &p_Drawable) noexcept
{
    Draw(p_Drawable, 0);
}

void IMultiWindowApplication::Draw(IDrawable &p_Drawable, const usize p_WindowIndex) noexcept
{
    KIT_ASSERT(p_WindowIndex < m_Windows.size(), "Window index out of bounds");
    m_Windows[p_WindowIndex]->Draw(p_Drawable);
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

void IMultiWindowApplication::Shutdown() noexcept
{
    IApplication::Shutdown();
    CloseAllWindows(); // Should not be that necessary
}

bool IMultiWindowApplication::NextFrame(KIT::Clock &p_Clock) noexcept
{
    m_DeltaTime.store(p_Clock.Restart().AsSeconds(), std::memory_order_relaxed); // In case concurrent mode is used
    if (m_Windows.empty())
        return false;

    Input::PollEvents();
    processWindows();
    return !m_Windows.empty();
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
        if (IsStarted())
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
        if (IsStarted())
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