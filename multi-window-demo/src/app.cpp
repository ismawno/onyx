#include "mwdemo/app.hpp"
#include "mwdemo/layer.hpp"
#include "onyx/camera/orthographic.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"

namespace ONYX
{
void MWDemoApplication::RunSerial() noexcept
{
    static KIT::ThreadPool<std::mutex> threadPool{7};
    static KIT::StackAllocator allocator{10_kb};
    ONYX::Core::Initialize(&allocator, &threadPool);

    m_SerialApplication.Layers.Push<MWExampleLayer>(&m_SerialApplication);
    m_SerialApplication.OpenWindow<ONYX::Orthographic2D>(Window::Specs{}, 5.f);
    m_SerialApplication.Run();
    ONYX::Core::Terminate();
}

void MWDemoApplication::RunConcurrent() noexcept
{
    static KIT::ThreadPool<std::mutex> threadPool{7};
    static KIT::StackAllocator allocator{10_kb};
    ONYX::Core::Initialize(&allocator, &threadPool);

    m_ConcurrentApplication.Layers.Push<MWExampleLayer>(&m_ConcurrentApplication);
    m_ConcurrentApplication.OpenWindow<ONYX::Orthographic2D>(Window::Specs{}, 5.f);
    m_ConcurrentApplication.Run();
    ONYX::Core::Terminate();
}

} // namespace ONYX