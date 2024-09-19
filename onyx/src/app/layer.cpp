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

const IApplication *Layer::GetApplication() const noexcept
{
    return m_Application;
}
IApplication *Layer::GetApplication() noexcept
{
    return m_Application;
}

LayerSystem::LayerSystem(IApplication *p_Application) noexcept : m_Application(p_Application)
{
}

void LayerSystem::OnStart() noexcept
{
    for (auto &layer : m_Layers)
        if (layer->Enabled)
            layer->OnStart();
}
void LayerSystem::OnShutdown() noexcept
{
    for (auto &layer : m_Layers)
        if (layer->Enabled)
            layer->OnShutdown();
}

void LayerSystem::OnUpdate(const usize p_WindowIndex) noexcept
{
    for (auto &layer : m_Layers)
        if (layer->Enabled)
            layer->OnUpdate(p_WindowIndex);
}
void LayerSystem::OnRender(const usize p_WindowIndex) noexcept
{
    for (auto &layer : m_Layers)
        if (layer->Enabled)
            layer->OnRender(p_WindowIndex);
}
void LayerSystem::OnImGuiRender() noexcept
{
    for (auto &layer : m_Layers)
        if (layer->Enabled)
            layer->OnImGuiRender();
}

void LayerSystem::OnEvent(const usize p_WindowIndex, const Event &p_Event) noexcept
{
    for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it)
        if ((*it)->Enabled && (*it)->OnEvent(p_WindowIndex, p_Event))
            return;
}

} // namespace ONYX