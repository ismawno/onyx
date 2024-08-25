#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/dimension.hpp"
#include "kit/core/non_copyable.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

ONYX_NAMESPACE_BEGIN

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

    Window() KIT_NOEXCEPT;
    explicit Window(const Specs &specs) KIT_NOEXCEPT;

    ~Window() KIT_NOEXCEPT;

    bool ShouldClose() const KIT_NOEXCEPT;

  private:
    void InitializeWindow() KIT_NOEXCEPT;

    GLFWwindow *m_Window;
    Specs m_Specs;
};

using Window2D = Window<2>;
using Window3D = Window<3>;

ONYX_NAMESPACE_END