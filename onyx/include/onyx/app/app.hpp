#pragma once

#include "onyx/app/layer.hpp"
#include "onyx/app/window.hpp"
#include "kit/profiling/clock.hpp"

namespace ONYX
{
// provides a simple interface for a single or multi-window application
class IApplication
{
  public:
    IApplication() = default;
    virtual ~IApplication() noexcept;

    virtual void Startup() noexcept;
    void Shutdown() noexcept;

    virtual bool NextFrame(KIT::Clock &p_Clock) noexcept = 0;
    virtual void Draw(IDrawable &p_Drawable) noexcept = 0;

    virtual f32 GetDeltaTime() const noexcept = 0;

    virtual const Window *GetMainWindow() const noexcept = 0;
    virtual Window *GetMainWindow() noexcept = 0;

    void Run() noexcept;
    bool IsStarted() const noexcept;
    bool IsTerminated() const noexcept;
    bool IsRunning() const noexcept;

    LayerSystem Layers;

  protected:
    void initializeImGui(Window &p_Window) noexcept;
    void shutdownImGui() noexcept;

    static void beginRenderImGui() noexcept;
    void endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept;

    // Up to the user to get the device once a window is created
    KIT::Ref<Device> m_Device;

  private:
    void createImGuiPool() noexcept;

    bool m_Started = false;
    bool m_Terminated = false;

    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
};

class Application final : public IApplication
{
  public:
    Application(const Window::Specs &p_WindowSpecs = {}) noexcept;

    void Draw(IDrawable &p_Drawable) noexcept override;
    bool NextFrame(KIT::Clock &p_Clock) noexcept override;

    const Window *GetMainWindow() const noexcept override;
    Window *GetMainWindow() noexcept override;

    f32 GetDeltaTime() const noexcept override;

  private:
    KIT::Storage<Window> m_Window;
    f32 m_DeltaTime = 0.0f;
};

} // namespace ONYX