#include "onyx/core/pch.hpp"
#include "onyx/app/user_layer.hpp"
#include "onyx/app/window.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/transform.hpp"

namespace Onyx
{
template <Dimension D> void UserLayer::TransformEditor(Transform<D> &p_Transform, f32 p_DragSpeed) noexcept
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

template void UserLayer::TransformEditor<D2>(Transform<D2> &p_Transform, f32 p_DragSpeed) noexcept;
template void UserLayer::TransformEditor<D3>(Transform<D3> &p_Transform, f32 p_DragSpeed) noexcept;

template <Dimension D> void UserLayer::MaterialEditor(MaterialData<D> &p_Material) noexcept
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

template void UserLayer::MaterialEditor<D2>(MaterialData<D2> &p_Material) noexcept;
template void UserLayer::MaterialEditor<D3>(MaterialData<D3> &p_Material) noexcept;

void UserLayer::DirectionalLightEditor(DirectionalLight &p_Light) noexcept
{
    ImGui::PushID(&p_Light);
    ImGui::SliderFloat("Intensity", &p_Light.DirectionAndIntensity.w, 0.f, 1.f);
    ImGui::SliderFloat3("Direction", glm::value_ptr(p_Light.DirectionAndIntensity), 0.f, 1.f);
    ImGui::ColorEdit3("Color", p_Light.Color.AsPointer());
    ImGui::PopID();
}

void UserLayer::PointLightEditor(PointLight &p_Light) noexcept
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

void UserLayer::PresentModeEditor(Window *p_Window) noexcept
{
    const VkPresentModeKHR current = p_Window->GetPresentMode();
    const TKit::StaticArray8<VkPresentModeKHR> &available = p_Window->GetAvailablePresentModes();

    int index = -1;
    TKit::StaticArray8<const char *> presentModes;
    for (u32 i = 0; i < available.size(); ++i)
    {
        presentModes.push_back(presentModeToString(available[i]));
        if (available[i] == current)
            index = static_cast<int>(i);
    }

    if (ImGui::Combo("Present mode", &index, presentModes.data(), static_cast<int>(available.size())))
        p_Window->SetPresentMode(available[index]);
}

} // namespace Onyx