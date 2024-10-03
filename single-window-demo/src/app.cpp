#include "swdemo/app.hpp"
#include "swdemo/layer.hpp"
#include "onyx/camera/orthographic.hpp"

namespace ONYX
{
void SWDemoApplication::Run() noexcept
{
    static KIT::ThreadPool<std::mutex> threadPool{7};
    static KIT::StackAllocator allocator{10_kb};
    ONYX::Core::Initialize(&allocator, &threadPool);
    m_Application.Create();

    m_Application->Layers.Push<SWExampleLayer>(m_Application.Get());
    m_Application->Run();
    m_Application.Destroy();

    ONYX::Core::Terminate();
}

} // namespace ONYX