#pragma once

#include "onyx/app/mwapp.hpp"

namespace Onyx
{
class MWDemoApplication
{
  public:
    TKIT_NON_COPYABLE(MWDemoApplication)

    MWDemoApplication() noexcept = default;

    void RunSerial() noexcept;
    void RunConcurrent() noexcept;

  private:
    MultiWindowApplication<Serial> m_SerialApplication;
    MultiWindowApplication<Concurrent> m_ConcurrentApplication;
};
} // namespace Onyx