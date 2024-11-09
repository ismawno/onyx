#include "swdemo/app.hpp"
#include "swdemo/layer.hpp"
#include "kit/core/literals.hpp"

namespace ONYX
{
using namespace KIT::Literals;

void SWDemoApplication::Run() noexcept
{
    static KIT::ThreadPool<std::mutex> threadPool{7};
    static KIT::StackAllocator allocator{10_kb};
    ONYX::Core::Initialize(&allocator, &threadPool);

    Window::Specs spc;
    spc.Name = "Single window demo app";
    m_Application.Create(spc);

    m_Application->Layers.Push<SWExampleLayer>(m_Application.Get());
    m_Application->Run();
    m_Application.Destroy();

    ONYX::Core::Terminate();
}

} // namespace ONYX