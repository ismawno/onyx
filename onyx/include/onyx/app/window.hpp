#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/device.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class ONYX_API Window
{
    KIT_NON_COPYABLE(Window)
  public:
    struct Specs
    {
        const char *name = "Onyx window";
        u32 width = 800;
        u32 height = 600;
    };

    Window() noexcept;
    explicit Window(const Specs &p_Specs) noexcept;

    ~Window() noexcept;

    bool ShouldClose() const noexcept;
    GLFWwindow *GetGLFWWindow() const noexcept;

    const char *Name() const noexcept;
    u32 Width() const noexcept;
    u32 Height() const noexcept;

    VkSurfaceKHR Surface() const noexcept;

  private:
    void initializeWindow() noexcept;

    KIT::Ref<Instance> m_Instance;
    KIT::Ref<Device> m_Device;
    GLFWwindow *m_Window;
    VkSurfaceKHR m_Surface;
    Specs m_Specs;
};

using Window2D = Window<2>;
using Window3D = Window<3>;
} // namespace ONYX