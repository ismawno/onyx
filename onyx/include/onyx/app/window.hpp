#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/device.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/app/input.hpp"
#include "onyx/drawing/color.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"
#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/rendering/buffer.hpp"
#include "onyx/rendering/render_system.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
// TODO: Align window to the cache line in case a multi window app is used?
class ONYX_API Window
{
    KIT_NON_COPYABLE(Window)
  public:
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
            std::forward<F>(p_Submission)(cmd);
            drawRenderSystems(cmd);
            m_Renderer->EndRenderPass();
            m_Renderer->EndFrame(*this);
            return true;
        }
        return false;
    }
    bool Display() noexcept;

    void MakeContextCurrent() const noexcept;
    bool ShouldClose() const noexcept;

    const GLFWwindow *GetWindow() const noexcept;
    GLFWwindow *GetWindow() noexcept;

    const char *GetName() const noexcept;
    u32 GetScreenWidth() const noexcept;
    u32 GetScreenHeight() const noexcept;

    u32 GetPixelWidth() const noexcept;
    u32 GetPixelHeight() const noexcept;

    f32 GetScreenAspect() const noexcept;
    f32 GetPixelAspect() const noexcept;

    bool WasResized() const noexcept;
    void FlagResize(u32 p_Width, u32 p_Height) noexcept;
    void FlagResizeDone() noexcept;

    VkSurfaceKHR GetSurface() const noexcept;

    void PushEvent(const Event &p_Event) noexcept;
    Event PopEvent() noexcept;

    const Renderer &GetRenderer() const noexcept;

    Color BackgroundColor = Color::BLACK;

  private:
    struct GlobalUniformHelper
    {
        GlobalUniformHelper(const DescriptorPool::Specs &p_PoolSpecs,
                            const std::span<const VkDescriptorSetLayoutBinding> p_Bindings,
                            const Buffer::Specs &p_BufferSpecs) noexcept
            : Pool(p_PoolSpecs), Layouts(p_Bindings), UniformBuffer(p_BufferSpecs)
        {
        }
        DescriptorPool Pool;
        DescriptorSetLayout Layouts;
        Buffer UniformBuffer;
    };
    void createWindow() noexcept;
    void createGlobalUniformHelper() noexcept;

    void drawRenderSystems(VkCommandBuffer p_CommandBuffer) noexcept;

    GLFWwindow *m_Window;

    KIT::Ref<Instance> m_Instance;
    KIT::Ref<Device> m_Device;
    KIT::Scope<Renderer> m_Renderer;

    KIT::Scope<GlobalUniformHelper> m_GlobalUniformHelper;
    std::array<VkDescriptorSet, SwapChain::MAX_FRAMES_IN_FLIGHT> m_GlobalDescriptorSets;

    DynamicArray<RenderSystem> m_RenderSystems;

    Deque<Event> m_Events;
    VkSurfaceKHR m_Surface;
    Specs m_Specs;

    bool m_Resized = false;
};
} // namespace ONYX