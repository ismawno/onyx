#pragma once

#include "onyx/app/app.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"
#include "kit/core/literals.hpp"

namespace ONYX
{
class SWDemoApplication
{
  public:
    KIT_NON_COPYABLE(SWDemoApplication)

    SWDemoApplication() noexcept = default;
    void Run() noexcept;

  private:
    KIT::Storage<Application> m_Application;
};
} // namespace ONYX