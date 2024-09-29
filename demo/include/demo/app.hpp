#pragma once

#include "onyx/app/app.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"
#include "kit/core/literals.hpp"

namespace ONYX
{
using namespace KIT::Literals;

class DemoApplication
{
  public:
    KIT_NON_COPYABLE(DemoApplication)

    DemoApplication() noexcept;
    ~DemoApplication() noexcept;

    void RunSerial() noexcept;
    void RunConcurrent() noexcept;

  private:
    ONYX::MultiWindowApplication<ONYX::WindowFlow::SERIAL> m_SerialApplication;
    ONYX::MultiWindowApplication<ONYX::WindowFlow::CONCURRENT> m_ConcurrentApplication;

    KIT::ThreadPool<std::mutex> m_ThreadPool{7};
    KIT::StackAllocator m_Allocator{10_kb};
};
} // namespace ONYX