#pragma once

#include "onyx/app/mwapp.hpp"
#include "kit/core/literals.hpp"

namespace ONYX
{
using namespace KIT::Literals;

class MWDemoApplication
{
  public:
    KIT_NON_COPYABLE(MWDemoApplication)

    MWDemoApplication() noexcept = default;

    void RunSerial() noexcept;
    void RunConcurrent() noexcept;

  private:
    MultiWindowApplication<WindowFlow::Serial> m_SerialApplication;
    MultiWindowApplication<WindowFlow::Concurrent> m_ConcurrentApplication;
};
} // namespace ONYX