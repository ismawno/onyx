#pragma once

#include "onyx/app/window.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class Application
{
    KIT_NON_COPYABLE(Application)
  public:
    Application() noexcept;
    explicit Application(const Window<N>::Specs &specs) noexcept;

    void Run() noexcept;

  private:
    Window<N> m_Window;
};

using Application2D = Application<2>;
using Application3D = Application<3>;
} // namespace ONYX