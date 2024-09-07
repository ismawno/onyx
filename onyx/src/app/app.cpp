#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"

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
    return m_Windows.back().Get();
}

ONYX_DIMENSION_TEMPLATE Window<N> *Application<N>::OpenWindow() noexcept
{
    m_Windows.push_back(KIT::Scope<Window<N>>::Create());
    return m_Windows.back().Get();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::CloseWindow(usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    m_Windows.erase(m_Windows.begin() + p_Index);
}

ONYX_DIMENSION_TEMPLATE void Application<N>::CloseWindow(const Window<N> *p_Window) noexcept
{
    for (usize i = 0; i < m_Windows.size(); ++i)
        if (m_Windows[i].Get() == p_Window)
        {
            m_Windows.erase(m_Windows.begin() + i);
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

ONYX_DIMENSION_TEMPLATE bool Application<N>::NextFrameSerial() noexcept
{
    Input::PollEvents();
    for (const auto &window : m_Windows)
    {
        window->MakeContextCurrent();
        KIT_ASSERT_RETURNS(
            window->Display(), true,
            "Failed to display the window. Failed to acquire a command buffer when beginning a new frame");
    }
    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
    return !m_Windows.empty();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::RunSerial() noexcept
{
    Start();
    while (NextFrameSerial())
        ;
    Shutdown();
}

template class Application<2>;
template class Application<3>;
} // namespace ONYX