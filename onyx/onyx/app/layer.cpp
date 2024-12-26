#include "onyx/core/pch.hpp"
#include "onyx/app/layer.hpp"

namespace Onyx
{
Layer::Layer(const char *p_Name) noexcept : m_Name(p_Name)
{
}

const char *Layer::GetName() const noexcept
{
    return m_Name;
}

template <std::ranges::range R> static auto enabledLayers(R &&p_Layers) noexcept
{
    return p_Layers | std::views::all | std::views::filter([](const auto &layer) { return layer->Enabled; });
}

void LayerSystem::OnStart() noexcept
{
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnStart();
}
void LayerSystem::OnShutdown() noexcept
{
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnShutdown();
}

void LayerSystem::OnUpdate() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnUpdate");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnUpdate();
}
void LayerSystem::OnRender(const VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnRender(p_CommandBuffer);
}
void LayerSystem::OnLateRender(const VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnLateRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnLateRender(p_CommandBuffer);
}

void LayerSystem::OnUpdate(const usize p_WindowIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnUpdate");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnUpdate(p_WindowIndex);
}
void LayerSystem::OnRender(const usize p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnRender(p_WindowIndex, p_CommandBuffer);
}
void LayerSystem::OnLateRender(const usize p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnLateRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnLateRender(p_WindowIndex, p_CommandBuffer);
}

void LayerSystem::OnImGuiRender() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnImGuiRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnImGuiRender();
}

void LayerSystem::OnEvent(const Event &p_Event) noexcept
{
    for (auto &layer : enabledLayers(m_Layers) | std::views::reverse)
        if (layer->OnEvent(p_Event))
            return;
}

void LayerSystem::OnEvent(const usize p_WindowIndex, const Event &p_Event) noexcept
{
    for (auto &layer : enabledLayers(m_Layers) | std::views::reverse)
        if (layer->OnEvent(p_WindowIndex, p_Event))
            return;
}

} // namespace Onyx