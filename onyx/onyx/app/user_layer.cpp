#include "onyx/core/pch.hpp"
#include "onyx/app/user_layer.hpp"
#include "onyx/property/color.hpp"
#include "onyx/property/transform.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/app/window.hpp"
#include "onyx/app/input.hpp"
#include "onyx/imgui/imgui.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
static void displayTransformHelp()
{
    UserLayer::HelpMarker(
        "The transform is the main component with which a shape or an object in a scene is positioned, "
        "scaled, and rotated. It is composed of a translation vector, a scale vector, and a rotation "
        "quaternion in 3D, or a rotation angle in 2D. Almost all objects in a scene have a transform.");
}

template <Dimension D> bool UserLayer::TransformEditor(Transform<D> &transform, const UserLayerFlags flags)
{
    ImGui::PushID(&transform);
    if (flags & UserLayerFlag_DisplayHelp)
        displayTransformHelp();
    bool changed = false;
    if constexpr (D == D2)
    {
        changed |= ImGui::DragFloat2("Translation", Math::AsPointer(transform.Translation), 0.03f);
        changed |= ImGui::DragFloat2("Scale", Math::AsPointer(transform.Scale), 0.03f);

        f32 degrees = Math::Degrees(transform.Rotation);
        if (ImGui::DragFloat("Rotation", &degrees, 0.3f, 0.f, 0.f, "%.1f deg"))
        {
            transform.Rotation = Math::Radians(degrees);
            changed = true;
        }
    }
    else
    {
        changed |= ImGui::DragFloat3("Translation", Math::AsPointer(transform.Translation), 0.03f);
        changed |= ImGui::DragFloat3("Scale", Math::AsPointer(transform.Scale), 0.03f);

        ImGui::Spacing();

        f32v3 degrees = Math::Degrees(Math::ToEulerAngles(transform.Rotation));
        if (ImGui::InputFloat3("Rotation", Math::AsPointer(degrees), "%.0f deg"))
        {
            transform.Rotation = f32q{Math::Radians(degrees)};
            changed = true;
        }

        f32v3 angles{0.f};
        if (ImGui::DragFloat3("Rotate (global)", Math::AsPointer(angles), 0.3f, 0.f, 0.f, "Slide!"))
        {
            transform.Rotation = Math::Normalize(f32q(Math::Radians(angles)) * transform.Rotation);
            changed = true;
        }

        if (ImGui::DragFloat3("Rotate (Local)", Math::AsPointer(angles), 0.3f, 0.f, 0.f, "Slide!"))
        {
            transform.Rotation = Math::Normalize(transform.Rotation * f32q(Math::Radians(angles)));
            changed = true;
        }
        if (ImGui::Button("Reset transform"))
        {
            transform = Transform<D>{};
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset rotation"))
        {
            transform.Rotation = f32q{1.f, 0.f, 0.f, 0.f};
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

template bool UserLayer::TransformEditor<D2>(Transform<D2> &transform, UserLayerFlags flags);
template bool UserLayer::TransformEditor<D3>(Transform<D3> &transform, UserLayerFlags flags);

template <Dimension D> void UserLayer::DisplayTransform(const Transform<D> &transform, const UserLayerFlags flags)
{
    const f32v<D> &translation = transform.Translation;
    const f32v<D> &scale = transform.Scale;

    if (flags & UserLayerFlag_DisplayHelp)
        displayTransformHelp();
    if constexpr (D == D2)
    {
        ImGui::Text("Translation: (%.2f, %.2f)", translation[0], translation[1]);
        ImGui::Text("Scale: (%.2f, %.2f)", scale[0], scale[1]);
        ImGui::Text("Rotation: %.2f deg", Math::Degrees(transform.Rotation));
    }
    else
    {
        ImGui::Text("Translation: (%.2f, %.2f, %.2f)", translation[0], translation[1], translation[2]);
        ImGui::Text("Scale: (%.2f, %.2f, %.2f)", scale[0], scale[1], scale[2]);

        const f32v3 angles = Math::Degrees(Math::ToEulerAngles(transform.Rotation));
        ImGui::Text("Rotation: (%.2f, %.2f, %.2f) deg", angles[0], angles[1], angles[2]);
    }
}

template void UserLayer::DisplayTransform<D2>(const Transform<D2> &transform, UserLayerFlags flags);
template void UserLayer::DisplayTransform<D3>(const Transform<D3> &transform, UserLayerFlags flags);

template <Dimension D> void UserLayer::DisplayCameraControls(const CameraControls<D> &controls)
{
    if constexpr (D == D2)
    {
        ImGui::BulletText("%s: Up", Input::GetKeyName(controls.Up));
        ImGui::BulletText("%s: Left", Input::GetKeyName(controls.Left));
        ImGui::BulletText("%s: Down", Input::GetKeyName(controls.Down));
        ImGui::BulletText("%s: Right", Input::GetKeyName(controls.Right));
        ImGui::BulletText("%s: Rotate left", Input::GetKeyName(controls.RotateLeft));
        ImGui::BulletText("%s: Rotate right", Input::GetKeyName(controls.RotateRight));
    }
    else
    {
        ImGui::BulletText("%s: Forward", Input::GetKeyName(controls.Forward));
        ImGui::BulletText("%s: Left", Input::GetKeyName(controls.Left));
        ImGui::BulletText("%s: Backward", Input::GetKeyName(controls.Backward));
        ImGui::BulletText("%s: Right", Input::GetKeyName(controls.Right));
        ImGui::BulletText("%s: Up", Input::GetKeyName(controls.Up));
        ImGui::BulletText("%s: Down", Input::GetKeyName(controls.Down));
        ImGui::BulletText("%s: Look around", Input::GetKeyName(controls.ToggleLookAround));
        ImGui::BulletText("%s: Rotate left", Input::GetKeyName(controls.RotateLeft));
        ImGui::BulletText("%s: Rotate right", Input::GetKeyName(controls.RotateRight));
    }
}

template void UserLayer::DisplayCameraControls<D2>(const CameraControls<D2> &controls);
template void UserLayer::DisplayCameraControls<D3>(const CameraControls<D3> &controls);

void UserLayer::HelpMarker(const char *description, const char *icon)
{
    ImGui::TextDisabled("%s", icon);
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
void UserLayer::HelpMarkerSameLine(const char *description, const char *icon)
{
    ImGui::SameLine();
    HelpMarker(description, icon);
}

bool UserLayer::DirectionalLightEditor(DirectionalLight &light, const UserLayerFlags flags)
{
    bool changed = false;
    if (flags & UserLayerFlag_DisplayHelp)
        HelpMarker(
            "Directional lights are lights that have no position, only a direction. They are used to simulate "
            "infinite light sources, such as the sun. They have a direction, an intensity, and a color. The "
            "direction is a Math::Normalized vector that points in the direction of the light, the intensity is the "
            "brightness of the light, and the color is the color of the light.");
    ImGui::PushID(&light);
    changed |= ImGui::SliderFloat("Intensity", &light.Intensity, 0.f, 1.f);
    changed |= ImGui::SliderFloat3("Direction", Math::AsPointer(light.Direction), 0.f, 1.f);

    Color color = Color::Unpack(light.Color);
    if (ImGui::ColorEdit3("Color", color.GetData()))
    {
        light.Color = color.Pack();
        changed = true;
    }
    ImGui::PopID();

    return changed;
}

bool UserLayer::PointLightEditor(PointLight &light, const UserLayerFlags flags)
{
    bool changed = false;
    if (flags & UserLayerFlag_DisplayHelp)
        HelpMarker(
            "Point lights are lights that have a position and a radius. They are used to simulate light sources "
            "that emit light in all directions, such as light bulbs. They have a position, an intensity, a "
            "radius, and a color. The position is the position of the light, the intensity is the brightness of "
            "the light, the radius is the distance at which the light is still visible, and the color is the color "
            "of the light.");
    ImGui::PushID(&light);

    changed |= ImGui::SliderFloat("Intensity", &light.Intensity, 0.f, 1.f);
    changed |= ImGui::DragFloat3("Position", Math::AsPointer(light.Position), 0.01f);
    changed |= ImGui::SliderFloat("Radius", &light.Radius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);

    Color color = Color::Unpack(light.Color);
    if (ImGui::ColorEdit3("Color", color.GetData()))
    {
        light.Color = color.Pack();
        changed = true;
    }
    ImGui::PopID();
    return changed;
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

bool UserLayer::PresentModeEditor(Window *window, const UserLayerFlags flags)
{
    const VkPresentModeKHR current = window->GetPresentMode();
    const TKit::TierArray<VkPresentModeKHR> &available = window->GetAvailablePresentModes();

    int index = -1;
    TKit::StackArray<const char *> presentModes;
    presentModes.Reserve(available.GetSize());
    for (u32 i = 0; i < available.GetSize(); ++i)
    {
        presentModes.Append(presentModeToString(available[i]));
        if (available[i] == current)
            index = static_cast<int>(i);
    }

    const bool changed =
        ImGui::Combo("Present mode", &index, presentModes.GetData(), static_cast<int>(available.GetSize()));
    if (changed)
        window->SetPresentMode(available[index]);

    if (flags & UserLayerFlag_DisplayHelp)
        HelpMarkerSameLine("Controls the frequency with which rendered images are sent to the screen. This setting "
                           "can be used to limit the frame rate of the application. The most common present mode is "
                           "Fifo, and uses V-Sync to synchronize the frame rate with the "
                           "refresh rate of the monitor.");
    return changed;
}

bool UserLayer::ViewportEditor(ScreenViewport &viewport, const UserLayerFlags flags)
{
    bool changed = false;
    ImGui::PushID(&viewport);
    if (flags & UserLayerFlag_DisplayHelp)
    {
        HelpMarker("The viewport is the area of the screen where the camera is rendered. It is defined as a "
                   "rectangle that is specified in Math::Normalized coordinates (0, 0) to (1, 1).");
        HelpMarkerSameLine("Vulkan is pretty strict about the validity of viewports. The area of the viewport must "
                           "always be greater than zero, and the minimum and maximum depth bounds must be between 0 "
                           "and 1. Otherwise, the application will crash.",
                           "(!)");
    }

    if (ImGui::Button("Fullscreen", ImVec2{166.f, 0.f}))
    {
        viewport.Min = f32v2{-1.f, -1.f};
        viewport.Max = f32v2{1.f, 1.f};
        changed = true;
    }

    if (ImGui::Button("Top-left", ImVec2{80.f, 0.f}))
    {
        viewport.Min = f32v2{-1.f, 0.f};
        viewport.Max = f32v2{0.f, 1.f};
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Top-right", ImVec2{80.f, 0.f}))
    {
        viewport.Min = f32v2{0.f, 0.f};
        viewport.Max = f32v2{1.f, 1.f};
        changed = true;
    }

    if (ImGui::Button("Bottom-left", ImVec2{80.f, 0.f}))
    {
        viewport.Min = f32v2{-1.f, -1.f};
        viewport.Max = f32v2{0.f, 0.f};
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Bottom-right", ImVec2{80.f, 0.f}))
    {
        viewport.Min = f32v2{0.f, -1.f};
        viewport.Max = f32v2{1.f, 0.f};
        changed = true;
    }

    changed |= ImGui::SliderFloat2("Min", Math::AsPointer(viewport.Min), -1.f, 1.f);
    changed |= ImGui::SliderFloat2("Max", Math::AsPointer(viewport.Max), -1.f, 1.f);
    changed |= ImGui::SliderFloat2("Depth bounds", Math::AsPointer(viewport.DepthBounds), 0.f, 1.f);
    ImGui::PopID();
    return changed;
}

bool UserLayer::ScissorEditor(ScreenScissor &scissor, const UserLayerFlags flags)
{
    bool changed = false;
    ImGui::PushID(&scissor);
    if (flags & UserLayerFlag_DisplayHelp)
    {
        HelpMarker("The scissor limits the area of the screen the camera is rendered to. It is defined as a "
                   "rectangle that is specified in Math::Normalized coordinates (0, 0) to (1, 1).");
        HelpMarkerSameLine("Vulkan is pretty strict about the validity of scissors. The area of the scissor must "
                           "always be greater than zero, and the minimum and maximum depth bounds must be between 0 "
                           "and 1. Otherwise, the application will crash.",
                           "(!)");
    }

    changed |= ImGui::SliderFloat2("Min", Math::AsPointer(scissor.Min), -1.f, 1.f);
    changed |= ImGui::SliderFloat2("Max", Math::AsPointer(scissor.Max), -1.f, 1.f);
    ImGui::PopID();
    return changed;
}

} // namespace Onyx
