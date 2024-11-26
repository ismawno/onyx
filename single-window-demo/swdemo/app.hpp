#pragma once

#include "onyx/app/app.hpp"
#include "tkit/memory/stack_allocator.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/core/literals.hpp"

namespace Onyx
{
class SWDemoApplication
{
  public:
    TKIT_NON_COPYABLE(SWDemoApplication)

    SWDemoApplication() noexcept = default;
    void Run() noexcept;

  private:
    TKit::Storage<Application> m_Application;
};
} // namespace Onyx