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
    Layer(Application *p_Application, TKit::Span<const Lattice<D>> p_Lattices) noexcept;

    void OnStart() noexcept override;
    void OnRender(u32, VkCommandBuffer) noexcept override;
    void OnEvent(const Event &p_Event) noexcept override;

  private:
    Application *m_Application;
    Window *m_Window;
    RenderContext<D> *m_Context;
    Camera<D> *m_Camera;
    TKit::StaticArray8<Lattice<D>> m_Lattices{};
};
} // namespace Onyx::Perf
