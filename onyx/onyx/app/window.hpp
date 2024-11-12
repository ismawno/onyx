#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/device.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/rendering/frame_scheduler.hpp"

#include "kit/container/storage.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
class ONYX_API Window
{
    KIT_NON_COPYABLE(Window)
  public:
    // Could use a bitset, but I cant directly initialize it with the flags as I do with the u8
    enum Flags : u8
    {
        RESIZABLE = 1 << 0,
        VISIBLE = 1 << 1,
        DECORATED = 1 << 2,
        FOCUSED = 1 << 3,
        FLOATING = 1 << 4
    };
    struct Specs
    {
        const char *Name = "Onyx window";
        u32 Width = 800;
        u32 Height = 600;
        u8 Flags = RESIZABLE | VISIBLE | DECORATED | FOCUSED;
    };

    Window() noexcept;
    explicit Window(const Specs &p_Specs) noexcept;
    ~Window() noexcept;

    template <typename F1, typename F2> bool Render(F1 &&p_DrawCalls, F2 &&p_UICalls) noexcept
    {
        if (const VkCommandBuffer cmd = m_FrameScheduler->BeginFrame(*this))
        {
            m_FrameScheduler->BeginRenderPass(BackgroundColor);
            std::forward<F1>(p_DrawCalls)(cmd);
            m_RenderContext2D->Render(cmd);
            m_RenderContext3D->Render(cmd);
            std::forward<F2>(p_UICalls)(cmd);
            m_FrameScheduler->EndRenderPass();
            m_FrameScheduler->EndFrame(*this);
            return true;
        }
        return false;
    }
    bool Render() noexcept;
    bool ShouldClose() const noexcept;

    const GLFWwindow *GetWindowHandle() const noexcept;
    GLFWwindow *GetWindowHandle() noexcept;

    const char *GetName() const noexcept;
    u32 GetScreenWidth() const noexcept;
    u32 GetScreenHeight() const noexcept;

    u32 GetPixelWidth() const noexcept;
    u32 GetPixelHeight() const noexcept;

    f32 GetScreenAspect() const noexcept;
    f32 GetPixelAspect() const noexcept;

    std::pair<u32, u32> GetPosition() const noexcept;

    bool WasResized() const noexcept;
    void FlagResize(u32 p_Width, u32 p_Height) noexcept;
    void FlagResizeDone() noexcept;
    void FlagShouldClose() noexcept;

    VkSurfaceKHR GetSurface() const noexcept;

    void PushEvent(const Event &p_Event) noexcept;
    const DynamicArray<Event> &GetNewEvents() const noexcept;
    void FlushEvents() noexcept;

    template <Dimension D> const RenderContext<D> *GetRenderContext() const noexcept
    {
        if constexpr (D == D2)
            return m_RenderContext2D.Get();
        else
            return m_RenderContext3D.Get();
    }
    template <Dimension D> RenderContext<D> *GetRenderContext() noexcept
    {
        if constexpr (D == D2)
            return m_RenderContext2D.Get();
        else
            return m_RenderContext3D.Get();
    }

    const FrameScheduler &GetFrameScheduler() const noexcept;
    FrameScheduler &GetFrameScheduler() noexcept;

    Color BackgroundColor = Color::BLACK;

  private:
    void createWindow(const Specs &p_Specs) noexcept;

    GLFWwindow *m_Window;

    KIT::Ref<Instance> m_Instance;
    KIT::Ref<Device> m_Device;

    KIT::Storage<FrameScheduler> m_FrameScheduler;
    KIT::Storage<RenderContext<D2>> m_RenderContext2D;
    KIT::Storage<RenderContext<D3>> m_RenderContext3D;

    DynamicArray<Event> m_Events;
    VkSurfaceKHR m_Surface;

    const char *m_Name;
    u32 m_Width;
    u32 m_Height;

    bool m_Resized = false;
};
} // namespace ONYX