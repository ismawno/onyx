#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/device.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/app/input.hpp"
#include "onyx/model/color.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
// TODO: Align window to the cache line in case a multi window app is used?
ONYX_DIMENSION_TEMPLATE class ONYX_API Window
{
    KIT_NON_COPYABLE(Window)
  public:
    KIT_BLOCK_ALLOCATED_SERIAL(Window<N>, 8)
    struct Specs
    {
        const char *Name = "Onyx window";
        u32 Width = 800;
        u32 Height = 600;
    };

    Window() noexcept;
    explicit Window(const Specs &p_Specs) noexcept;

    ~Window() noexcept;

    template <typename F> bool Display(F &&p_Submission) noexcept
    {
        if (const VkCommandBuffer cmd = m_Renderer->BeginFrame(*this))
        {
            m_Renderer->BeginRenderPass(BackgroundColor);
            p_Submission(cmd);
            m_Renderer->EndRenderPass();
            m_Renderer->EndFrame(*this);
            return true;
        }
        return false;
    }
    bool Display() noexcept;

    void MakeContextCurrent() const noexcept;
    bool ShouldClose() const noexcept;

    const GLFWwindow *GetGLFWWindow() const noexcept;
    GLFWwindow *GetGLFWWindow() noexcept;

    const char *Name() const noexcept;
    u32 ScreenWidth() const noexcept;
    u32 ScreenHeight() const noexcept;

    u32 PixelWidth() const noexcept;
    u32 PixelHeight() const noexcept;

    f32 ScreenAspect() const noexcept;
    f32 PixelAspect() const noexcept;

    bool WasResized() const noexcept;
    void FlagResize(u32 p_Width, u32 p_Height) noexcept;
    void FlagResizeDone() noexcept;

    VkSurfaceKHR Surface() const noexcept;

    void PushEvent(const Event &p_Event) noexcept;
    Event PopEvent() noexcept;

    Color BackgroundColor = Color::BLACK;

  private:
    void initialize() noexcept;

    KIT::Ref<Instance> m_Instance;
    KIT::Ref<Device> m_Device;
    KIT::Scope<Renderer> m_Renderer;
    GLFWwindow *m_Window;

    Deque<Event> m_Events;
    VkSurfaceKHR m_Surface;
    Specs m_Specs;

    bool m_Resized = false;
};

using Window2D = Window<2>;
using Window3D = Window<3>;
} // namespace ONYX