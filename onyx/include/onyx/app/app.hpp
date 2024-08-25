#pragma once

#include "onyx/app/window.hpp"

ONYX_NAMESPACE_BEGIN

ONYX_DIMENSION_TEMPLATE class Application
{
    KIT_NON_COPYABLE(Application)
  public:
    Application() KIT_NOEXCEPT;
    explicit Application(const Window<N>::Specs &specs) KIT_NOEXCEPT;

    void Run() KIT_NOEXCEPT;

  private:
    Window<N> m_Window;
};

using Application2D = Application<2>;
using Application3D = Application<3>;

ONYX_NAMESPACE_END