#include "core/pch.hpp"
#include "onyx/app/layer.hpp"

namespace ONYX
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
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnUpdate();
}
void LayerSystem::OnRender() noexcept
{
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnRender();
}

void LayerSystem::OnUpdate(const usize p_WindowIndex) noexcept
{
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnUpdate(p_WindowIndex);
}
void LayerSystem::OnRender(const usize p_WindowIndex) noexcept
{
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnRender(p_WindowIndex);
}
void LayerSystem::OnImGuiRender() noexcept
{
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

} // namespace ONYX