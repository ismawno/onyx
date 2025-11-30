#pragma once

#include "sbox/window_data.hpp"
#include "onyx/app/user_layer.hpp"

namespace Onyx::Demo
{
class SandboxLayer final : public UserLayer
{
  public:
    SandboxLayer(SingleWindowApp *p_Application, Dimension p_Dim);
    SandboxLayer(MultiWindowApp *p_Application, Dimension p_Dim);

    void OnUpdate() override;
    void OnRenderBegin(u32, VkCommandBuffer p_CommandBuffer) override;
    void OnEvent(const Event &p_Event) override;

    void OnUpdate(u32 p_WindowIndex) override;
    void OnRenderBegin(u32 p_WindowIndex, u32, VkCommandBuffer p_CommandBuffer) override;
    void OnImGuiRender() override;
    void OnEvent(u32 p_WindowIndex, const Event &p_Event) override;

    QuitResult QuitResult;

  private:
    SingleWindowApp *m_SingleApp = nullptr;
    MultiWindowApp *m_MultiApp = nullptr;
    WindowData m_SingleData;
    TKit::StaticArray<WindowData, ONYX_MAX_WINDOWS> m_MultiData;
    Dimension m_Dim;
};
} // namespace Onyx::Demo
