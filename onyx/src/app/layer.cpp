#include "core/pch.hpp"
#include "onyx/app/layer.hpp"

namespace ONYX
{
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

void LayerSystem::OnUpdate(const f32 p_TS, Window *p_Window) noexcept
{
    for (auto &layer : m_Layers)
        if (layer->Enabled)
            layer->OnUpdate(p_TS, p_Window);
}
void LayerSystem::OnRender(const f32 p_TS, Window *p_Window) noexcept
{
    for (auto &layer : m_Layers)
        if (layer->Enabled)
            layer->OnRender(p_TS, p_Window);
}

void LayerSystem::OnEvent(const Event &p_Event) noexcept
{
    for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it)
        if ((*it)->Enabled && (*it)->OnEvent(p_Event))
            break;
}

} // namespace ONYX