#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/core/pch.hpp"
#    include "onyx/app/user_layer.hpp"
#    include "onyx/app/window.hpp"
#    include "onyx/rendering/render_context.hpp"
#    include "onyx/property/transform.hpp"
#    include <imgui.h>

namespace Onyx
{
static void displayTransformHelp()
{
    UserLayer::HelpMarker(
        "The transform is the main component with which a shape or an object in a scene is positioned, "
        "scaled, and rotated. It is composed of a translation vector, a scale vector, and a rotation "
        "quaternion in 3D, or a rotation angle in 2D. Almost all objects in a scene have a transform.");
}

template <Dimension D> bool UserLayer::TransformEditor(Transform<D> &p_Transform, const Flags p_Flags)
{
    ImGui::PushID(&p_Transform);
    if (p_Flags & Flag_DisplayHelp)
        displayTransformHelp();
    bool changed = false;
    if constexpr (D == D2)
    {
        changed |= ImGui::DragFloat2("Translation", glm::value_ptr(p_Transform.Translation), 0.03f);
        changed |= ImGui::DragFloat2("Scale", glm::value_ptr(p_Transform.Scale), 0.03f);

        f32 degrees = glm::degrees(p_Transform.Rotation);
        if (ImGui::DragFloat("Rotation", &degrees, 0.3f, 0.f, 0.f, "%.1f deg"))
        {
            p_Transform.Rotation = glm::radians(degrees);
            changed = true;
        }
    }
    else
    {
        changed |= ImGui::DragFloat3("Translation", glm::value_ptr(p_Transform.Translation), 0.03f);
        changed |= ImGui::DragFloat3("Scale", glm::value_ptr(p_Transform.Scale), 0.03f);

        ImGui::Spacing();

        fvec3 degrees = glm::degrees(glm::eulerAngles(p_Transform.Rotation));
        if (ImGui::InputFloat3("Rotation", glm::value_ptr(degrees), "%.0f deg"))
        {
            p_Transform.Rotation = glm::quat{glm::radians(degrees)};
            changed = true;
        }

        fvec3 angles{0.f};
        if (ImGui::DragFloat3("Rotate (global)", glm::value_ptr(angles), 0.3f, 0.f, 0.f, "Slide!"))
        {
            p_Transform.Rotation = glm::normalize(glm::quat(glm::radians(angles)) * p_Transform.Rotation);
            changed = true;
        }

        if (ImGui::DragFloat3("Rotate (Local)", glm::value_ptr(angles), 0.3f, 0.f, 0.f, "Slide!"))
        {
            p_Transform.Rotation = glm::normalize(p_Transform.Rotation * glm::quat(glm::radians(angles)));
            changed = true;
        }
        if (ImGui::Button("Reset transform"))
        {
            p_Transform = Transform<D>{};
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset rotation"))
        {
            p_Transform.Rotation = quat{1.f, 0.f, 0.f, 0.f};
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

template bool UserLayer::TransformEditor<D2>(Transform<D2> &p_Transform, Flags p_Flags);
template bool UserLayer::TransformEditor<D3>(Transform<D3> &p_Transform, Flags p_Flags);

template <Dimension D> void UserLayer::DisplayTransform(const Transform<D> &p_Transform, const Flags p_Flags)
{
    const fvec<D> &translation = p_Transform.Translation;
    const fvec<D> &scale = p_Transform.Scale;

    if (p_Flags & Flag_DisplayHelp)
        displayTransformHelp();
    if constexpr (D == D2)
    {
        ImGui::Text("Translation: (%.2f, %.2f)", translation.x, translation.y);
        ImGui::Text("Scale: (%.2f, %.2f)", scale.x, scale.y);
        ImGui::Text("Rotation: %.2f deg", glm::degrees(p_Transform.Rotation));
    }
    else
    {
        ImGui::Text("Translation: (%.2f, %.2f, %.2f)", translation.x, translation.y, translation.z);
        ImGui::Text("Scale: (%.2f, %.2f, %.2f)", scale.x, scale.y, scale.z);

        const fvec3 angles = glm::degrees(glm::eulerAngles(p_Transform.Rotation));
        ImGui::Text("Rotation: (%.2f, %.2f, %.2f) deg", angles.x, angles.y, angles.z);
    }
}

template void UserLayer::DisplayTransform<D2>(const Transform<D2> &p_Transform, Flags p_Flags);
template void UserLayer::DisplayTransform<D3>(const Transform<D3> &p_Transform, Flags p_Flags);

template <Dimension D> void UserLayer::DisplayCameraControls(const CameraControls<D> &p_Controls)
{
    if constexpr (D == D2)
    {
        ImGui::BulletText("%s: Up", Input::GetKeyName(p_Controls.Up));
        ImGui::BulletText("%s: Left", Input::GetKeyName(p_Controls.Left));
        ImGui::BulletText("%s: Down", Input::GetKeyName(p_Controls.Down));
        ImGui::BulletText("%s: Right", Input::GetKeyName(p_Controls.Right));
        ImGui::BulletText("%s: Rotate left", Input::GetKeyName(p_Controls.RotateLeft));
        ImGui::BulletText("%s: Rotate right", Input::GetKeyName(p_Controls.RotateRight));
    }
    else
    {
        ImGui::BulletText("%s: Forward", Input::GetKeyName(p_Controls.Forward));
        ImGui::BulletText("%s: Left", Input::GetKeyName(p_Controls.Left));
        ImGui::BulletText("%s: Backward", Input::GetKeyName(p_Controls.Backward));
        ImGui::BulletText("%s: Right", Input::GetKeyName(p_Controls.Right));
        ImGui::BulletText("%s: Up", Input::GetKeyName(p_Controls.Up));
        ImGui::BulletText("%s: Down", Input::GetKeyName(p_Controls.Down));
        ImGui::BulletText("%s: Look around", Input::GetKeyName(p_Controls.ToggleLookAround));
        ImGui::BulletText("%s: Rotate left", Input::GetKeyName(p_Controls.RotateLeft));
        ImGui::BulletText("%s: Rotate right", Input::GetKeyName(p_Controls.RotateRight));
    }
}

template void UserLayer::DisplayCameraControls<D2>(const CameraControls<D2> &p_Controls);
template void UserLayer::DisplayCameraControls<D3>(const CameraControls<D3> &p_Controls);

void UserLayer::DisplayFrameTime(const TKit::Timespan p_DeltaTime, const Flags p_Flags)
{
    // These statics may cause trouble with multiple applications
    static TKit::Timespan maxDeltaTime{};
    static TKit::Timespan smoothDeltaTime{};
    static f32 smoothFactor = 0.f;
    static i32 unit = 1;

    if (p_DeltaTime > maxDeltaTime)
        maxDeltaTime = p_DeltaTime;
    smoothDeltaTime = smoothFactor * smoothDeltaTime + (1.f - smoothFactor) * p_DeltaTime;

    ImGui::SliderFloat("Smoothing factor", &smoothFactor, 0.f, 0.999f);
    if (p_Flags & Flag_DisplayHelp)
        HelpMarkerSameLine(
            "Because frames get dispatched so quickly, the frame time can vary a lot, be inconsistent, and hard to "
            "see. This slider allows you to smooth out the frame time across frames, making it easier to see the "
            "trend.");

    ImGui::Combo("Unit", &unit, "s\0ms\0us\0ns\0");
    if (unit == 0)
        ImGui::Text("Frame time: %.4f s (max: %.4f s)", smoothDeltaTime.AsSeconds(), maxDeltaTime.AsSeconds());
    else if (unit == 1)
        ImGui::Text("Frame time: %.2f ms (max: %.2f ms)", smoothDeltaTime.AsMilliseconds(),
                    maxDeltaTime.AsMilliseconds());
    else if (unit == 2)
        ImGui::Text("Frame time: %u us (max: %u us)", static_cast<u32>(smoothDeltaTime.AsMicroseconds()),
                    static_cast<u32>(maxDeltaTime.AsMicroseconds()));
    else
#    ifndef TKIT_OS_LINUX
        ImGui::Text("Frame time: %llu ns (max: %llu ns)", smoothDeltaTime.AsNanoseconds(),
                    maxDeltaTime.AsNanoseconds());
#    else
        ImGui::Text("Frame time: %lu ns (max: %lu ns)", smoothDeltaTime.AsNanoseconds(), maxDeltaTime.AsNanoseconds());
#    endif

    if (p_Flags & Flag_DisplayHelp)
        HelpMarkerSameLine(
            "The frame time is a measure of the time it takes to process and render a frame, and it is one of the main "
            "indicators of an application smoothness. It is also used to calculate the frames per second (FPS) of the "
            "application. A good frame time is usually no larger than 16.67 ms (that is, 60 fps). It is also bound to "
            "the present mode of the window.");

    const u32 fps = static_cast<u32>(1.f / smoothDeltaTime.AsSeconds());
    ImGui::Text("FPS: %u", fps);

    if (ImGui::Button("Reset maximum"))
        maxDeltaTime = TKit::Timespan{};
}

void UserLayer::HelpMarker(const char *p_Description, const char *p_Icon)
{
    ImGui::TextDisabled("%s", p_Icon);
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(p_Description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
void UserLayer::HelpMarkerSameLine(const char *p_Description, const char *p_Icon)
{
    ImGui::SameLine();
    HelpMarker(p_Description, p_Icon);
}

template <Dimension D> bool UserLayer::MaterialEditor(MaterialData<D> &p_Material, const Flags p_Flags)
{
    if (p_Flags & Flag_DisplayHelp)
        HelpMarker(
            "The material of an object defines its basic properties, such as its color, its diffuse and specular "
            "contributions, and its specular sharpness. The material is used to calculate the final color of the "
            "object, which is then used to render it. Onyx does not support 2D lights, so 2D materials are very "
            "simple: a lone color.");
    bool changed = false;
    if constexpr (D == D3)
    {
        if (ImGui::SliderFloat("Diffuse contribution", &p_Material.DiffuseContribution, 0.f, 1.f))
        {
            p_Material.SpecularContribution = 1.f - p_Material.DiffuseContribution;
            changed = true;
        }
        if (ImGui::SliderFloat("Specular contribution", &p_Material.SpecularContribution, 0.f, 1.f))
        {
            p_Material.DiffuseContribution = 1.f - p_Material.SpecularContribution;
            changed = true;
        }
        changed |= ImGui::SliderFloat("Specular sharpness", &p_Material.SpecularSharpness, 0.f, 512.f, "%.2f",
                                      ImGuiSliderFlags_Logarithmic);
    }
    changed |= ImGui::ColorEdit4("Color", p_Material.Color.GetData());
    return changed;
}

template bool UserLayer::MaterialEditor<D2>(MaterialData<D2> &p_Material, Flags p_Flags);
template bool UserLayer::MaterialEditor<D3>(MaterialData<D3> &p_Material, Flags p_Flags);

bool UserLayer::DirectionalLightEditor(DirectionalLight &p_Light, const Flags p_Flags)
{
    bool changed = false;
    if (p_Flags & Flag_DisplayHelp)
        HelpMarker("Directional lights are lights that have no position, only a direction. They are used to simulate "
                   "infinite light sources, such as the sun. They have a direction, an intensity, and a color. The "
                   "direction is a normalized vector that points in the direction of the light, the intensity is the "
                   "brightness of the light, and the color is the color of the light.");
    ImGui::PushID(&p_Light);
    changed |= ImGui::SliderFloat("Intensity", &p_Light.Intensity, 0.f, 1.f);
    changed |= ImGui::SliderFloat3("Direction", glm::value_ptr(p_Light.Direction), 0.f, 1.f);

    Color color = Color::Unpack(p_Light.Color);
    if (ImGui::ColorEdit3("Color", color.GetData()))
    {
        p_Light.Color = color.Pack();
        changed = true;
    }
    ImGui::PopID();

    return changed;
}

bool UserLayer::PointLightEditor(PointLight &p_Light, const Flags p_Flags)
{
    bool changed = false;
    if (p_Flags & Flag_DisplayHelp)
        HelpMarker(
            "Point lights are lights that have a position and a radius. They are used to simulate light sources "
            "that emit light in all directions, such as light bulbs. They have a position, an intensity, a "
            "radius, and a color. The position is the position of the light, the intensity is the brightness of "
            "the light, the radius is the distance at which the light is still visible, and the color is the color "
            "of the light.");
    ImGui::PushID(&p_Light);

    changed |= ImGui::SliderFloat("Intensity", &p_Light.Intensity, 0.f, 1.f);
    changed |= ImGui::DragFloat3("Position", glm::value_ptr(p_Light.Position), 0.01f);
    changed |= ImGui::SliderFloat("Radius", &p_Light.Radius, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);

    Color color = Color::Unpack(p_Light.Color);
    if (ImGui::ColorEdit3("Color", color.GetData()))
    {
        p_Light.Color = color.Pack();
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool UserLayer::ResolutionEditor(const char *p_Name, Resolution &p_Res, const Flags p_Flags)
{
    ImGui::PushID(&p_Res);
    i32 res = static_cast<i32>(p_Res);

    const bool changed = ImGui::Combo(p_Name, &res, "Very Low\0Low\0Medium\0High\0Very High\0\0");
    if (changed)
        p_Res = static_cast<Resolution>(res);

    if (p_Flags & Flag_DisplayHelp)
        HelpMarkerSameLine(
            "This setting allows you to control the resolution of certain 3D shapes that have smooth curved surfaces, "
            "such as spheres and cylinders. The resolution is the number of segments that the shape is divided into. A "
            "higher resolution will make the shape smoother, but it will also increase the number of vertices and the "
            "computational cost of rendering the shape.");
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

bool UserLayer::PresentModeEditor(Window *p_Window, const Flags p_Flags)
{
    const FrameScheduler *fs = p_Window->GetFrameScheduler();
    const VkPresentModeKHR current = fs->GetPresentMode();
    const TKit::StaticArray8<VkPresentModeKHR> &available = fs->GetAvailablePresentModes();

    int index = -1;
    TKit::StaticArray8<const char *> presentModes;
    for (u32 i = 0; i < available.GetSize(); ++i)
    {
        presentModes.Append(presentModeToString(available[i]));
        if (available[i] == current)
            index = static_cast<int>(i);
    }

    const bool changed =
        ImGui::Combo("Present mode", &index, presentModes.GetData(), static_cast<int>(available.GetSize()));
    if (changed)
        p_Window->GetFrameScheduler()->SetPresentMode(available[index]);

    if (p_Flags & Flag_DisplayHelp)
        HelpMarkerSameLine("Controls the frequency with which rendered images are sent to the screen. This setting "
                           "can be used to limit the frame rate of the application. The most common present mode is "
                           "Fifo, and uses V-Sync to synchronize the frame rate with the "
                           "refresh rate of the monitor.");
    return changed;
}

bool UserLayer::ViewportEditor(ScreenViewport &p_Viewport, const Flags p_Flags)
{
    bool changed = false;
    ImGui::PushID(&p_Viewport);
    if (p_Flags & Flag_DisplayHelp)
    {
        HelpMarker("The viewport is the area of the screen where the camera is rendered. It is defined as a "
                   "rectangle that is specified in normalized coordinates (0, 0) to (1, 1).");
        HelpMarkerSameLine("Vulkan is pretty strict about the validity of viewports. The area of the viewport must "
                           "always be greater than zero, and the minimum and maximum depth bounds must be between 0 "
                           "and 1. Otherwise, the application will crash.",
                           "(!)");
    }

    if (ImGui::Button("Fullscreen", ImVec2{166.f, 0.f}))
    {
        p_Viewport.Min = {-1.f, -1.f};
        p_Viewport.Max = {1.f, 1.f};
        changed = true;
    }

    if (ImGui::Button("Top-left", ImVec2{80.f, 0.f}))
    {
        p_Viewport.Min = {-1.f, 0.f};
        p_Viewport.Max = {0.f, 1.f};
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Top-right", ImVec2{80.f, 0.f}))
    {
        p_Viewport.Min = {0.f, 0.f};
        p_Viewport.Max = {1.f, 1.f};
        changed = true;
    }

    if (ImGui::Button("Bottom-left", ImVec2{80.f, 0.f}))
    {
        p_Viewport.Min = {-1.f, -1.f};
        p_Viewport.Max = {0.f, 0.f};
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Bottom-right", ImVec2{80.f, 0.f}))
    {
        p_Viewport.Min = {0.f, -1.f};
        p_Viewport.Max = {1.f, 0.f};
        changed = true;
    }

    changed |= ImGui::SliderFloat2("Min", glm::value_ptr(p_Viewport.Min), -1.f, 1.f);
    changed |= ImGui::SliderFloat2("Max", glm::value_ptr(p_Viewport.Max), -1.f, 1.f);
    changed |= ImGui::SliderFloat2("Depth bounds", glm::value_ptr(p_Viewport.DepthBounds), 0.f, 1.f);
    ImGui::PopID();
    return changed;
}

bool UserLayer::ScissorEditor(ScreenScissor &p_Scissor, const Flags p_Flags)
{
    bool changed = false;
    ImGui::PushID(&p_Scissor);
    if (p_Flags & Flag_DisplayHelp)
    {
        HelpMarker("The scissor limits the area of the screen the camera is rendered to. It is defined as a "
                   "rectangle that is specified in normalized coordinates (0, 0) to (1, 1).");
        HelpMarkerSameLine("Vulkan is pretty strict about the validity of scissors. The area of the scissor must "
                           "always be greater than zero, and the minimum and maximum depth bounds must be between 0 "
                           "and 1. Otherwise, the application will crash.",
                           "(!)");
    }

    changed |= ImGui::SliderFloat2("Min", glm::value_ptr(p_Scissor.Min), -1.f, 1.f);
    changed |= ImGui::SliderFloat2("Max", glm::value_ptr(p_Scissor.Max), -1.f, 1.f);
    ImGui::PopID();
    return changed;
}

} // namespace Onyx
#endif
