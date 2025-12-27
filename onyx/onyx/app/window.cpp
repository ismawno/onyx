#include "onyx/core/pch.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/profiling/vulkan.hpp"
#include "tkit/utils/debug.hpp"
#include "onyx/core/glfw.hpp"

namespace Onyx
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
static u32 s_ColorIndex = 0;
static u32 s_Colors[4] = {0x434E78, 0x607B8F, 0xF7E396, 0xE97F4A};
#endif

// edge cases a bit unrealistic yes
u32 ToFrequency(const TKit::Timespan p_DeltaTime)
{
    const f32 seconds = p_DeltaTime.AsSeconds();
    if (TKit::ApproachesZero(seconds))
        return TKit::Limits<u32>::Max();
    if (seconds == TKit::Limits<f32>::Max())
        return 0;

    return static_cast<u32>(1.f / p_DeltaTime.AsSeconds()) + 1;
}
TKit::Timespan ToDeltaTime(const u32 p_Frequency)
{
    if (p_Frequency == 0)
        return TKit::Timespan::FromSeconds(TKit::Limits<f32>::Max());
    if (p_Frequency == TKit::Limits<u32>::Max())
        return TKit::Timespan{};
    return TKit::Timespan::FromSeconds(1.f / static_cast<f32>(p_Frequency));
}

static TKit::BlockAllocator createAllocator()
{
    const u32 maxSize = static_cast<u32>(
        Math::Max({sizeof(RenderContext<D2>), sizeof(RenderContext<D3>), sizeof(Camera<D2>), sizeof(Camera<D3>)}));
    const u32 alignment = static_cast<u32>(
        Math::Max({alignof(RenderContext<D2>), alignof(RenderContext<D3>), alignof(Camera<D2>), alignof(Camera<D3>)}));
    return TKit::BlockAllocator{maxSize * 2 * (ONYX_MAX_RENDER_CONTEXTS + ONYX_MAX_CAMERAS), maxSize, alignment};
}

Window::Window() : Window(Specs{})
{
}

Window::Window(const Specs &p_Specs)
    : m_Allocator(createAllocator()), m_Name(p_Specs.Name), m_Dimensions(p_Specs.Dimensions), m_Flags(p_Specs.Flags)
{
    TKIT_LOG_DEBUG("[ONYX] Window '{}' has been instantiated", p_Specs.Name);
    createWindow(p_Specs);
    m_FrameScheduler->SetPresentMode(p_Specs.PresentMode);
#ifdef TKIT_ENABLE_INSTRUMENTATION
    m_ColorIndex = s_ColorIndex++ & 3;
#endif
    UpdateMonitorDeltaTime();
}

Window::~Window()
{
    TKIT_LOG_DEBUG("[ONYX] Window '{}' is about to be destroyed", m_Name);
    m_FrameScheduler.Destruct();
    for (RenderContext<D2> *context : m_RenderContexts2)
        m_Allocator.Destroy(context);
    for (RenderContext<D3> *context : m_RenderContexts3)
        m_Allocator.Destroy(context);

    for (Camera<D2> *context : m_Cameras2)
        m_Allocator.Destroy(context);
    for (Camera<D3> *context : m_Cameras3)
        m_Allocator.Destroy(context);

    Core::GetInstanceTable().DestroySurfaceKHR(Core::GetInstance(), m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
}

void Window::createWindow(const Specs &p_Specs)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, p_Specs.Flags & Flag_Resizable);
    glfwWindowHint(GLFW_VISIBLE, p_Specs.Flags & Flag_Visible);
    glfwWindowHint(GLFW_DECORATED, p_Specs.Flags & Flag_Decorated);
    glfwWindowHint(GLFW_FOCUSED, p_Specs.Flags & Flag_Focused);
    glfwWindowHint(GLFW_FLOATING, p_Specs.Flags & Flag_Floating);

    m_Window = glfwCreateWindow(static_cast<i32>(p_Specs.Dimensions[0]), static_cast<i32>(p_Specs.Dimensions[1]),
                                p_Specs.Name, nullptr, nullptr);
    TKIT_ASSERT(m_Window, "[ONYX] Failed to create a GLFW window");

    if (p_Specs.Position != u32v2{TKit::Limits<u32>::Max()})
    {
        glfwSetWindowPos(m_Window, static_cast<i32>(p_Specs.Position[0]), static_cast<i32>(p_Specs.Position[1]));
        m_Position = p_Specs.Position;
    }
    else
    {
        i32 x, y;
        glfwGetWindowPos(m_Window, &x, &y);
        m_Position = u32v2{x, y};
    }

    VKIT_ASSERT_EXPRESSION(glfwCreateWindowSurface(Core::GetInstance(), m_Window, nullptr, &m_Surface));
    glfwSetWindowUserPointer(m_Window, this);

    if (!Core::IsDeviceCreated())
        Core::CreateDevice(m_Surface);
    m_FrameScheduler.Construct(*this);
    Input::InstallCallbacks(*this);
}

FrameInfo Window::BeginFrame(const WaitMode &p_WaitMode)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::BeginFrame");
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);

    FrameInfo info;
    info.GraphicsCommand = m_FrameScheduler->BeginFrame(*this, p_WaitMode);
    info.TransferCommand = info.GraphicsCommand ? m_FrameScheduler->GetTransferCommandBuffer() : VK_NULL_HANDLE;
    info.FrameIndex = m_FrameScheduler->GetFrameIndex();
    return info;
}
VkPipelineStageFlags Window::SubmitContextData(const FrameInfo &p_Info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::SubmitContextData");
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);
    VkPipelineStageFlags transferFlags = 0;

    for (RenderContext<D2> *context : m_RenderContexts2)
    {
        context->GetRenderer().GrowToFit(p_Info.FrameIndex);
        context->GetRenderer().SendToDevice(p_Info.FrameIndex);
        transferFlags |= context->GetRenderer().RecordCopyCommands(p_Info.FrameIndex, p_Info.GraphicsCommand,
                                                                   p_Info.TransferCommand);
    }
    for (RenderContext<D3> *context : m_RenderContexts3)
    {
        context->GetRenderer().GrowToFit(p_Info.FrameIndex);
        context->GetRenderer().SendToDevice(p_Info.FrameIndex);
        transferFlags |= context->GetRenderer().RecordCopyCommands(p_Info.FrameIndex, p_Info.GraphicsCommand,
                                                                   p_Info.TransferCommand);
    }
    if (Core::IsSeparateTransferMode() && transferFlags != 0)
        m_FrameScheduler->SubmitTransferQueue();
    return transferFlags;
}
void Window::BeginRendering()
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::BeginRendering");
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);
    m_FrameScheduler->BeginRendering(BackgroundColor);
}

void Window::Render(const FrameInfo &p_Info)
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::Render");
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);
    TKIT_PROFILE_VULKAN_SCOPE("Onyx::Window::Vulkan::Render",
                              m_FrameScheduler->GetQueueData().Graphics->ProfilingContext, gcmd);
    auto caminfos = getCameraInfos<D2>();
    if (!caminfos.IsEmpty())
        for (RenderContext<D2> *context : m_RenderContexts2)
            context->GetRenderer().Render(p_Info.FrameIndex, p_Info.GraphicsCommand, caminfos);

    caminfos = getCameraInfos<D3>();
    if (!caminfos.IsEmpty())
        for (RenderContext<D3> *context : m_RenderContexts3)
            context->GetRenderer().Render(p_Info.FrameIndex, p_Info.GraphicsCommand, caminfos);
}

void Window::EndRendering()
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::Render");
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);
    m_FrameScheduler->EndRendering();
}

void Window::EndFrame(const VkPipelineStageFlags p_Flags)
{
    TKIT_PROFILE_VULKAN_COLLECT(m_FrameScheduler->GetQueueData().Graphics->ProfilingContext, gcmd);
    m_FrameScheduler->EndFrame(*this, p_Flags);
}

bool Window::Render()
{
    TKIT_PROFILE_NSCOPE("Onyx::Window::Render");
    TKIT_PROFILE_SCOPE_COLOR(s_Colors[m_ColorIndex]);

    const FrameInfo info = BeginFrame();
    if (!info)
        return false;

    const VkPipelineStageFlags flags = SubmitContextData(info);
    BeginRendering();
    Render(info);
    EndRendering();
    EndFrame(flags);
    return true;
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

TKit::Timespan Window::UpdateMonitorDeltaTime(const TKit::Timespan p_Default)
{
    GLFWmonitor *monitor = glfwGetWindowMonitor(m_Window);
    if (!monitor)
        monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        m_MonitorDeltaTime = p_Default;
        return p_Default;
    }
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    m_MonitorDeltaTime = TKit::Timespan::FromSeconds(1.f / static_cast<f32>(mode->refreshRate));
    return m_MonitorDeltaTime;
}

void Window::recreateSurface()
{
    Core::GetInstanceTable().DestroySurfaceKHR(Core::GetInstance(), m_Surface, nullptr);
    VKIT_ASSERT_EXPRESSION(glfwCreateWindowSurface(Core::GetInstance(), m_Window, nullptr, &m_Surface));
}

void Window::FlagShouldClose()
{
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

void Window::PushEvent(const Event &p_Event)
{
    if (!m_Events.IsFull())
        m_Events.Append(p_Event);
}

void Window::FlushEvents()
{
    m_Events.Clear();
}

void Window::adaptCamerasToViewportAspect()
{
    for (Camera<D2> *cam : m_Cameras2)
        cam->adaptViewToViewportAspect();
    for (Camera<D3> *cam : m_Cameras3)
        cam->adaptViewToViewportAspect();
}

} // namespace Onyx
