#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/device.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/app/input.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"
#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/rendering/buffer.hpp"
#include "onyx/rendering/render_system.hpp"
#include "onyx/camera/camera.hpp"

#include "kit/container/storage.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifndef ONYX_MAX_RENDER_SYSTEMS
#    define ONYX_MAX_RENDER_SYSTEMS 16
#endif

namespace ONYX
{
class IDrawable;
// TODO: Align window to the cache line in case a multi window app is used?
class ONYX_API Window
{
    KIT_NON_COPYABLE(Window)
  public:
    KIT_BLOCK_ALLOCATED_SERIAL(Window, 4)

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

    template <typename F> bool Display(F &&p_Submission) noexcept
    {
        if (const VkCommandBuffer cmd = m_Renderer->BeginFrame(*this))
        {
            m_Renderer->BeginRenderPass(BackgroundColor);
            drawRenderSystems(cmd);
            std::forward<F>(p_Submission)(cmd);
            m_Renderer->EndRenderPass();
            m_Renderer->EndFrame(*this);
            return true;
        }
        return false;
    }
    bool Display() noexcept;
    void Draw(IDrawable &p_Drawable) noexcept;
    void Draw(Window &p_Window) noexcept;

    bool ShouldClose() const noexcept;

    template <std::derived_from<ICamera> T = ICamera> const T *GetCamera() const noexcept
    {
        return static_cast<T *>(m_Camera.Get());
    }
    template <std::derived_from<ICamera> T = ICamera> T *GetCamera() noexcept
    {
        return static_cast<T *>(m_Camera.Get());
    }

    template <std::derived_from<ICamera> T, typename... CameraArgs> T *SetCamera(CameraArgs &&...p_Args) noexcept
    {
        m_Camera = KIT::Scope<T>::Create(std::forward<CameraArgs>(p_Args)...);
        return static_cast<T *>(m_Camera.Get());
    }

    ONYX_DIMENSION_TEMPLATE RenderSystem &AddRenderSystem(RenderSystem::Specs<N> p_Specs) noexcept;

    RenderSystem &AddRenderSystem2D(const RenderSystem::Specs2D &p_Specs) noexcept;
    RenderSystem &AddRenderSystem3D(const RenderSystem::Specs3D &p_Specs) noexcept;

    const RenderSystem &GetRenderSystem(const usize p_Index) const noexcept;
    RenderSystem &GetRenderSystem(const usize p_Index) noexcept;

    ONYX_DIMENSION_TEMPLATE const RenderSystem *GetRenderSystem(VkPrimitiveTopology p_Topology) const noexcept;
    ONYX_DIMENSION_TEMPLATE RenderSystem *GetRenderSystem(VkPrimitiveTopology p_Topology) noexcept;

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
    void FlagShouldClose() noexcept;

    VkSurfaceKHR GetSurface() const noexcept;

    void PushEvent(const Event &p_Event) noexcept;
    const DynamicArray<Event> &GetNewEvents() const noexcept;
    void FlushEvents() noexcept;

    const Renderer &GetRenderer() const noexcept;

    Color BackgroundColor = Color::BLACK;
    vec3 LightDirection = vec3(0.f, 1.f, 0.f);
    f32 LightIntensity = 0.9f;
    f32 AmbientIntensity = 0.1f;

  private:
    struct GlobalUniformHelper
    {
        GlobalUniformHelper(const DescriptorPool::Specs &p_PoolSpecs,
                            const std::span<const VkDescriptorSetLayoutBinding> p_Bindings,
                            const Buffer::Specs &p_BufferSpecs) noexcept
            : Pool(p_PoolSpecs), Layout(p_Bindings), UniformBuffer(p_BufferSpecs)
        {
        }
        DescriptorPool Pool;
        DescriptorSetLayout Layout;
        Buffer UniformBuffer;
    };
    void createWindow(const Specs &p_Specs) noexcept;
    void createGlobalUniformHelper() noexcept;
    ONYX_DIMENSION_TEMPLATE void addDefaultRenderSystems() noexcept;

    void drawRenderSystems(VkCommandBuffer p_CommandBuffer) noexcept;

    GLFWwindow *m_Window;

    KIT::Ref<Instance> m_Instance;
    KIT::Ref<Device> m_Device;
    KIT::Storage<Renderer> m_Renderer;
    KIT::Scope<ICamera> m_Camera;

    KIT::Storage<GlobalUniformHelper> m_GlobalUniformHelper;
    std::array<VkDescriptorSet, SwapChain::MAX_FRAMES_IN_FLIGHT> m_GlobalDescriptorSets;

    KIT::StaticArray<RenderSystem, ONYX_MAX_RENDER_SYSTEMS> m_RenderSystems;

    DynamicArray<Event> m_Events;
    VkSurfaceKHR m_Surface;

    const char *m_Name;
    u32 m_Width;
    u32 m_Height;

    bool m_Resized = false;
};
} // namespace ONYX