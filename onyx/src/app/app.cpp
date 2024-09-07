#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "kit/multiprocessing/task_manager.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Application<N>::~Application() noexcept
{
    if (!m_Terminated && m_Started)
        Shutdown();
}

ONYX_DIMENSION_TEMPLATE Window<N> *Application<N>::OpenWindow(const Window<N>::Specs &p_Specs) noexcept
{
    m_Windows.push_back(KIT::Scope<Window<N>>::Create(p_Specs));
    m_Tasks.emplace_back();
    return m_Windows.back().Get();
}

ONYX_DIMENSION_TEMPLATE Window<N> *Application<N>::OpenWindow() noexcept
{
    return OpenWindow(typename Window<N>::Specs{});
}

ONYX_DIMENSION_TEMPLATE void Application<N>::CloseWindow(usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    if (m_Tasks[p_Index])
        m_Tasks[p_Index]->WaitUntilFinished();

    m_Windows.erase(m_Windows.begin() + p_Index);
    m_Tasks.erase(m_Tasks.begin() + p_Index);

    // Check if the main window got removed. If so, the main thread will handle the next window
    if (p_Index == 0 && !m_Tasks.empty())
    {
        m_Tasks[0]->WaitUntilFinished();
        m_Tasks[0] = nullptr;
    }
}

ONYX_DIMENSION_TEMPLATE void Application<N>::CloseWindow(const Window<N> *p_Window) noexcept
{
    for (usize i = 0; i < m_Windows.size(); ++i)
        if (m_Windows[i].Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    KIT_ERROR("Window was not found");
}

ONYX_DIMENSION_TEMPLATE void Application<N>::Start() noexcept
{
    KIT_ASSERT(!m_Terminated && !m_Started, "Application already started");
    m_Started = true;
}

ONYX_DIMENSION_TEMPLATE void Application<N>::Shutdown() noexcept
{
    KIT_ASSERT(!m_Terminated && m_Started, "Application not started");
    m_Terminated = true;
}

ONYX_DIMENSION_TEMPLATE bool Application<N>::NextFrame() noexcept
{
    if (m_Windows.empty())
        return false;

    Input::PollEvents();
    runAndManageWindows();
    return !m_Windows.empty();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::Run() noexcept
{
    Start();
    while (NextFrame())
        ;
    Shutdown();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::runAndManageWindows() noexcept
{
    KIT::TaskManager *taskManager = Core::TaskManager();
    for (usize i = 1; i < m_Windows.size(); ++i)
    {
        auto &task = m_Tasks[i];
        if (!task)
        {
            Window<N> *window = m_Windows[i].Get();
            task = taskManager->CreateAndSubmit([window](usize) { runFrame(*window); });
        }
        else
        {
            task->WaitUntilFinished();
            task->Reset();
            taskManager->SubmitTask(task);
        }
    }
    // Main thread always handles the first window. First element of tasks is always nullptr
    runFrame(*m_Windows[0]);

    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
}

ONYX_DIMENSION_TEMPLATE void Application<N>::runFrame(Window<N> &p_Window) noexcept
{
    p_Window.MakeContextCurrent();
    KIT_ASSERT_RETURNS(p_Window.Display(), true,
                       "Failed to display the window. Failed to acquire a command buffer when beginning a new frame");
}

template class Application<2>;
template class Application<3>;
} // namespace ONYX