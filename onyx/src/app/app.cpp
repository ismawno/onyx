#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"

ONYX_NAMESPACE_BEGIN

ONYX_DIMENSION_TEMPLATE Application<N>::Application() KIT_NOEXCEPT
{
}

ONYX_DIMENSION_TEMPLATE Application<N>::Application(const Window<N>::Specs &specs) KIT_NOEXCEPT : m_Window(specs)
{
}

ONYX_DIMENSION_TEMPLATE void Application<N>::Run() KIT_NOEXCEPT
{
    while (!m_Window.ShouldClose())
        Input<N>::PollEvents();
}

template class Application<2>;
template class Application<3>;

ONYX_NAMESPACE_END