#pragma once

#include "onyx/app/app.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"
#include "kit/core/literals.hpp"

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