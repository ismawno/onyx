#include "onyx/core/pch.hpp"
#include "onyx/app/layer.hpp"
#include "onyx/app/window.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/transform.hpp"

namespace Onyx
{
Layer::Layer(const char *p_Name) noexcept : m_Name(p_Name)
{
}

const char *Layer::GetName() const noexcept
{
    return m_Name;
}

template <Dimension D> void Layer::EditTransform(Transform<D> &p_Transform, f32 p_DragSpeed) noexcept
{
    ImGui::PushID(&p_Transform);
    if constexpr (D == D2)
    {
        ImGui::DragFloat2("Translation", glm::value_ptr(p_Transform.Translation), p_DragSpeed);
        ImGui::DragFloat2("Scale", glm::value_ptr(p_Transform.Scale), p_DragSpeed);
        ImGui::DragFloat("Rotation", &p_Transform.Rotation, p_DragSpeed);
    }
    else
    {
        ImGui::DragFloat3("Translation", glm::value_ptr(p_Transform.Translation), p_DragSpeed);
        ImGui::DragFloat3("Scale", glm::value_ptr(p_Transform.Scale), p_DragSpeed);

        fvec3 angles{0.f};
        if (ImGui::DragFloat3("Rotate (global)", glm::value_ptr(angles), p_DragSpeed, 0.f, 0.f, "Slide!"))
            p_Transform.Rotation = glm::normalize(glm::quat(angles) * p_Transform.Rotation);

        ImGui::Spacing();

        if (ImGui::DragFloat3("Rotate (Local)", glm::value_ptr(angles), p_DragSpeed, 0.f, 0.f, "Slide!"))
            p_Transform.Rotation = glm::normalize(p_Transform.Rotation * glm::quat(angles));
        if (ImGui::Button("Reset rotation"))
            p_Transform.Rotation = quat{1.f, 0.f, 0.f, 0.f};
    }
    ImGui::PopID();
}

template void Layer::EditTransform<D2>(Transform<D2> &p_Transform, f32 p_DragSpeed) noexcept;
template void Layer::EditTransform<D3>(Transform<D3> &p_Transform, f32 p_DragSpeed) noexcept;

template <Dimension D> void Layer::EditMaterial(MaterialData<D> &p_Material) noexcept
{
    if constexpr (D == D2)
        ImGui::ColorEdit4("Color", p_Material.Color.AsPointer());
    else
    {
        if (ImGui::SliderFloat("Diffuse contribution", &p_Material.DiffuseContribution, 0.f, 1.f))
            p_Material.SpecularContribution = 1.f - p_Material.DiffuseContribution;
        if (ImGui::SliderFloat("Specular contribution", &p_Material.SpecularContribution, 0.f, 1.f))
            p_Material.DiffuseContribution = 1.f - p_Material.SpecularContribution;
        ImGui::SliderFloat("Specular sharpness", &p_Material.SpecularSharpness, 0.f, 512.f, "%.2f",
                           ImGuiSliderFlags_Logarithmic);

        ImGui::ColorEdit3("Color", p_Material.Color.AsPointer());
    }
}

template void Layer::EditMaterial<D2>(MaterialData<D2> &p_Material) noexcept;
template void Layer::EditMaterial<D3>(MaterialData<D3> &p_Material) noexcept;

void Layer::EditDirectionalLight(DirectionalLight &p_Light) noexcept
{
    ImGui::PushID(&p_Light);
    ImGui::SliderFloat("Intensity", &p_Light.DirectionAndIntensity.w, 0.f, 1.f);
    ImGui::SliderFloat3("Direction", glm::value_ptr(p_Light.DirectionAndIntensity), 0.f, 1.f);
    ImGui::ColorEdit3("Color", p_Light.Color.AsPointer());
    ImGui::PopID();
}

void Layer::EditPointLight(PointLight &p_Light) noexcept
{
    ImGui::PushID(&p_Light);
    ImGui::SliderFloat("Intensity", &p_Light.PositionAndIntensity.w, 0.f, 1.f);
    ImGui::DragFloat3("Position", glm::value_ptr(p_Light.PositionAndIntensity), 0.01f);
    ImGui::SliderFloat("Radius", &p_Light.Radius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::ColorEdit3("Color", p_Light.Color.AsPointer());
    ImGui::PopID();
}

static const char *presentModeToString(const VkPresentModeKHR mode)
{
    switch (mode)
    {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "Immediate";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "Mailbox";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "Fifo (V-Sync)";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return "Fifo relaxed (V-Sync)";
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
        return "Shared demand refresh";
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
        return "Shared continuous refresh";
    case VK_PRESENT_MODE_MAX_ENUM_KHR:
        return "MaxEnum";
    default:
        return "Unknown present mode";
    }
}

void Layer::EditPresentMode(Window *p_Window) noexcept
{
    const VkPresentModeKHR current = p_Window->GetPresentMode();
    const TKit::StaticArray8<VkPresentModeKHR> &available = p_Window->GetAvailablePresentModes();

    int index = -1;
    TKit::StaticArray8<const char *> presentModes;
    for (usize i = 0; i < available.size(); ++i)
    {
        presentModes.push_back(presentModeToString(available[i]));
        if (available[i] == current)
            index = static_cast<int>(i);
    }

    if (ImGui::Combo("Present mode", &index, presentModes.data(), static_cast<int>(available.size())))
        p_Window->SetPresentMode(available[index]);
}

template <std::ranges::range R> static auto enabledLayers(R &&p_Layers) noexcept
{
    return p_Layers | std::views::all | std::views::filter([](const auto &layer) { return layer->Enabled; });
}

void LayerSystem::onStart() noexcept
{
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnStart();
}
void LayerSystem::onShutdown() noexcept
{
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnShutdown();
}

void LayerSystem::onUpdate() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnUpdate");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnUpdate();
}
void LayerSystem::onRender(const VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnRender(p_CommandBuffer);
}
void LayerSystem::onLateRender(const VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnLateRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnLateRender(p_CommandBuffer);
}

void LayerSystem::onUpdate(const usize p_WindowIndex) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnUpdate");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnUpdate(p_WindowIndex);
}
void LayerSystem::onRender(const usize p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnRender(p_WindowIndex, p_CommandBuffer);
}
void LayerSystem::onLateRender(const usize p_WindowIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnLateRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnLateRender(p_WindowIndex, p_CommandBuffer);
}

void LayerSystem::onImGuiRender() noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::LayerSystem::OnImGuiRender");
    for (auto &layer : enabledLayers(m_Layers))
        layer->OnImGuiRender();
}

void LayerSystem::onEvent(const Event &p_Event) noexcept
{
    for (auto &layer : enabledLayers(m_Layers) | std::views::reverse)
        if (layer->OnEvent(p_Event))
            return;
}

void LayerSystem::onEvent(const usize p_WindowIndex, const Event &p_Event) noexcept
{
    for (auto &layer : enabledLayers(m_Layers) | std::views::reverse)
        if (layer->OnEvent(p_WindowIndex, p_Event))
            return;
}

void LayerSystem::removeFlaggedLayers() noexcept
{
    if (!m_MustRemoveLayers)
        return;

    Core::DeviceWaitIdle();
    for (usize i = 0; i < m_Layers.size();)
        if (m_Layers[i]->m_RemoveFlag)
        {
            m_Layers[i]->OnRemoval();
            m_Layers.erase(m_Layers.begin() + i);
        }
        else
            ++i;

    m_MustRemoveLayers = false;
}

void LayerSystem::Remove(const usize p_Index) noexcept
{
    m_Layers[p_Index]->OnRemoval();
    m_Layers.erase(m_Layers.begin() + p_Index);
}
void LayerSystem::Remove(std::string_view p_Name) noexcept
{
    for (usize i = 0; i < m_Layers.size(); ++i)
        if (m_Layers[i]->GetName() == p_Name)
        {
            m_Layers[i]->OnRemoval();
            m_Layers.erase(m_Layers.begin() + i);
            return;
        }
}
void LayerSystem::Remove(const Layer *p_Layer) noexcept
{
    for (usize i = 0; i < m_Layers.size(); ++i)
        if (m_Layers[i].Get() == p_Layer)
        {
            m_Layers[i]->OnRemoval();
            m_Layers.erase(m_Layers.begin() + i);
            return;
        }
}

void LayerSystem::FlagRemove(const usize p_Index) noexcept
{
    m_Layers[p_Index]->m_RemoveFlag = true;
    m_MustRemoveLayers = true;
}
void LayerSystem::FlagRemove(const std::string_view p_Name) noexcept
{
    for (usize i = 0; i < m_Layers.size(); ++i)
        if (m_Layers[i]->GetName() == p_Name)
        {
            m_Layers[i]->m_RemoveFlag = true;
            m_MustRemoveLayers = true;
            return;
        }
}
void LayerSystem::FlagRemove(const Layer *p_Layer) noexcept
{
    for (usize i = 0; i < m_Layers.size(); ++i)
        if (m_Layers[i].Get() == p_Layer)
        {
            m_Layers[i]->m_RemoveFlag = true;
            m_MustRemoveLayers = true;
            return;
        }
}

} // namespace Onyx