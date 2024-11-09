#include "mwdemo/app.hpp"
#include "mwdemo/layer.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/thread_pool.hpp"
#include "kit/core/literals.hpp"

namespace ONYX
{
using namespace KIT::Literals;

void MWDemoApplication::RunSerial() noexcept
{
    static KIT::ThreadPool<std::mutex> threadPool{7};
    static KIT::StackAllocator allocator{10_kb};
    ONYX::Core::Initialize(&allocator, &threadPool);

    m_SerialApplication.Layers.Push<MWExampleLayer>(&m_SerialApplication);

    Window::Specs spc{};
    spc.Name = "Main window";
    m_SerialApplication.OpenWindow(spc);
    m_SerialApplication.Run();
    ONYX::Core::Terminate();
}

void MWDemoApplication::RunConcurrent() noexcept
{
    static KIT::ThreadPool<std::mutex> threadPool{7};
    static KIT::StackAllocator allocator{10_kb};
    ONYX::Core::Initialize(&allocator, &threadPool);

    m_ConcurrentApplication.Layers.Push<MWExampleLayer>(&m_ConcurrentApplication);

    Window::Specs spc{};
    spc.Name = "Main window";
    m_ConcurrentApplication.OpenWindow(spc);
    m_ConcurrentApplication.Run();
    ONYX::Core::Terminate();
}

} // namespace ONYX