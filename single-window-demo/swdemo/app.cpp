#include "swdemo/app.hpp"
#include "swdemo/layer.hpp"
#include "kit/core/literals.hpp"

namespace Onyx
{
using namespace TKit::Literals;

void SWDemoApplication::Run() noexcept
{
    static TKit::ThreadPool<std::mutex> threadPool{7};
    static TKit::StackAllocator allocator{10_kb};
    Onyx::Core::Initialize(&allocator, &threadPool);

    Window::Specs spc;
    spc.Name = "Single window demo app";
    m_Application.Create(spc);

    m_Application->Layers.Push<SWExampleLayer>(m_Application.Get());
    m_Application->Run();
    m_Application.Destroy();

    Onyx::Core::Terminate();
}

} // namespace Onyx