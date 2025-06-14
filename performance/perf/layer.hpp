#pragma once

#include "perf/lattice.hpp"
#include "onyx/app/user_layer.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/rendering/render_context.hpp"
#include "vulkan/vulkan_core.h"

namespace Onyx
{
class Application;
}

namespace Onyx::Perf
{
template <Dimension D> class Layer : public UserLayer
{
  public:
    Layer(Application *p_Application) noexcept;

    void OnStart() noexcept override;
    void OnRender(u32, VkCommandBuffer) noexcept override;
    void OnEvent(const Event &p_Event) noexcept override;

  private:
    Application *m_Application;
    Window *m_Window;
    RenderContext<D> *m_Context;
    Camera<D> *m_Camera;
    Lattice<D> m_Lattice{};
};
} // namespace Onyx::Perf
