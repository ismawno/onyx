#include "mwdemo/app.hpp"
#include "mwdemo/layer.hpp"
#include "tkit/memory/stack_allocator.hpp"
#include "tkit/multiprocessing/thread_pool.hpp"
#include "tkit/core/literals.hpp"

namespace Onyx
{
using namespace TKit::Literals;

void MWDemoApplication::RunSerial() noexcept
{
    static TKit::ThreadPool<std::mutex> threadPool{7};
    static TKit::StackAllocator allocator{10_kb};
    Onyx::Core::Initialize(&allocator, &threadPool);

    m_SerialApplication.Layers.Push<MWExampleLayer>(&m_SerialApplication);

    Window::Specs spc{};
    spc.Name = "Main window";
    m_SerialApplication.OpenWindow(spc);
    m_SerialApplication.Run();
    Onyx::Core::Terminate();
}

void MWDemoApplication::RunConcurrent() noexcept
{
    static TKit::ThreadPool<std::mutex> threadPool{7};
    static TKit::StackAllocator allocator{10_kb};
    Onyx::Core::Initialize(&allocator, &threadPool);

    m_ConcurrentApplication.Layers.Push<MWExampleLayer>(&m_ConcurrentApplication);

    Window::Specs spc{};
    spc.Name = "Main window";
    m_ConcurrentApplication.OpenWindow(spc);
    m_ConcurrentApplication.Run();
    Onyx::Core::Terminate();
}

} // namespace Onyx