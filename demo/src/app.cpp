#include "demo/app.hpp"
#include "demo/layer.hpp"
#include "onyx/camera/orthographic.hpp"

namespace ONYX
{
DemoApplication::DemoApplication() noexcept
{
    ONYX::Core::Initialize(&m_Allocator, &m_ThreadPool);
}

DemoApplication::~DemoApplication() noexcept
{
    ONYX::Core::Terminate();
}

void DemoApplication::RunSerial() noexcept
{
    m_SerialApplication.Layers.Push<ExampleLayer>(&m_SerialApplication);
    m_SerialApplication.OpenWindow<ONYX::Orthographic2D>(Window::Specs{}, 5.f);
    m_SerialApplication.Run();
}

void DemoApplication::RunConcurrent() noexcept
{
    m_ConcurrentApplication.Layers.Push<ExampleLayer>(&m_ConcurrentApplication);
    m_ConcurrentApplication.OpenWindow<ONYX::Orthographic2D>(Window::Specs{}, 5.f);
    m_ConcurrentApplication.Run();
}

} // namespace ONYX