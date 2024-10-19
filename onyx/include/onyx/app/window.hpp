#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/device.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/app/input.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/rendering/render_system.hpp"

#include "kit/container/storage.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
// TODO: Align window to the cache line in case a multi window app is used?

// For now, render systems are fixed, and only ONYX systems are used. In the future, user defined render systems will
// be allowed
// NOTE: When supporting user defined render systems, the render pass must be supplied if the user failed to do so:
// if (!p_Specs.RenderPass)
//         p_Specs.RenderPass = m_RenderContext->GetSwapChain().GetRenderPass();
//     return m_RenderSystems.emplace_back(p_Specs);

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

    template <typename F> bool Render(F &&p_Submission) noexcept
    {
        if (const VkCommandBuffer cmd = m_RenderSystem->BeginFrame(*this))
        {
            m_RenderSystem->BeginRenderPass(BackgroundColor);
            m_RenderContext2D->Render(cmd);
            m_RenderContext3D->Render(cmd);
            std::forward<F>(p_Submission)(cmd);
            m_RenderSystem->EndRenderPass();
            m_RenderSystem->EndFrame(*this);
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

    ONYX_DIMENSION_TEMPLATE const RenderContext<N> *GetRenderContext() const noexcept;
    ONYX_DIMENSION_TEMPLATE RenderContext<N> *GetRenderContext() noexcept;

    const RenderContext2D *GetRenderContext2D() const noexcept;
    RenderContext2D *GetRenderContext2D() noexcept;

    const RenderContext3D *GetRenderContext3D() const noexcept;
    RenderContext3D *GetRenderContext3D() noexcept;

    const RenderSystem &GetRenderSystem() const noexcept;

    Color BackgroundColor = Color::BLACK;

  private:
    void createWindow(const Specs &p_Specs) noexcept;

    GLFWwindow *m_Window;

    KIT::Ref<Instance> m_Instance;
    KIT::Ref<Device> m_Device;

    KIT::Storage<RenderSystem> m_RenderSystem;
    KIT::Storage<RenderContext2D> m_RenderContext2D;
    KIT::Storage<RenderContext3D> m_RenderContext3D;

    DynamicArray<Event> m_Events;
    VkSurfaceKHR m_Surface;

    const char *m_Name;
    u32 m_Width;
    u32 m_Height;

    bool m_Resized = false;
};
} // namespace ONYX