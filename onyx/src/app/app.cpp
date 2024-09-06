#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Application<N>::Application(const Window<N>::Specs &p_Specs) noexcept : m_Window(p_Specs)
{
}

ONYX_DIMENSION_TEMPLATE Application<N>::~Application() noexcept
{
    if (!m_Terminated && m_Started)
        Shutdown();
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

ONYX_DIMENSION_TEMPLATE void Application<N>::Run() noexcept
{
    while (!m_Window.ShouldClose())
    {
        // Input::PollEvents();
        m_Window.MakeContextCurrent();
        KIT_ASSERT_RETURNS(
            m_Window.Display(), true,
            "Failed to display the window. Failed to acquire a command buffer when beginning a new frame");
    }
}

template class Application<2>;
template class Application<3>;
} // namespace ONYX