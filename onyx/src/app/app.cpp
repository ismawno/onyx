#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Application<N>::Application() noexcept
{
}

ONYX_DIMENSION_TEMPLATE Application<N>::Application(const Window<N>::Specs &specs) noexcept : m_Window(specs)
{
}

ONYX_DIMENSION_TEMPLATE void Application<N>::Run() noexcept
{
    while (!m_Window.ShouldClose())
        Input<N>::PollEvents();
}

template class Application<2>;
template class Application<3>;
} // namespace ONYX