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
class ONYX_API IWindow
{
    KIT_NON_COPYABLE(IWindow)
  public:
    struct Specs
    {
        const char *Name = "Onyx window";
        u32 Width = 800;
        u32 Height = 600;
    };

    IWindow() noexcept;
    explicit IWindow(const Specs &p_Specs) noexcept;

    virtual ~IWindow() noexcept;

    template <typename F> bool Display(F &&p_Submission) noexcept
    {
        if (const VkCommandBuffer cmd = m_Renderer->BeginFrame(*this))
        {
            m_Renderer->BeginRenderPass(BackgroundColor);
            std::forward<F>(p_Submission)(cmd);
            runRenderSystems();
            m_Renderer->EndRenderPass();
            m_Renderer->EndFrame(*this);
            return true;
        }
        return false;
    }
    bool Display() noexcept;

    void MakeContextCurrent() const noexcept;
    bool ShouldClose() const noexcept;

    const GLFWwindow *GLFWWindow() const noexcept;
    GLFWwindow *GLFWWindow() noexcept;

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

    const Renderer &GetRenderer() const noexcept;

    Color BackgroundColor = Color::BLACK;

  protected:
    virtual void runRenderSystems() noexcept = 0;

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

ONYX_DIMENSION_TEMPLATE class ONYX_API Window final : public IWindow
{
    KIT_NON_COPYABLE(Window)
  public:
    KIT_BLOCK_ALLOCATED_SERIAL(IWindow, 8)
    using Specs = typename IWindow::Specs;
    using IWindow::IWindow;

  private:
    void runRenderSystems() noexcept override;
};

using Window2D = Window<2>;
using Window3D = Window<3>;
} // namespace ONYX