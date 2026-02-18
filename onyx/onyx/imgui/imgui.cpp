#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/core/pch.hpp"
#    include "onyx/imgui/imgui.hpp"
#    include "onyx/property/color.hpp"
#    include "onyx/property/transform.hpp"
#    include "onyx/rendering/light.hpp"
#    include "onyx/application/layer.hpp"
#    include "onyx/platform/input.hpp"
#    include "onyx/imgui/imgui.hpp"
#    include "tkit/container/stack_array.hpp"

namespace Onyx
{
static void displayTransformHelp()
{
    HelpMarker("The transform is the main component with which a shape or an object in a scene is positioned, "
               "scaled, and rotated. It is composed of a translation vector, a scale vector, and a rotation "
               "quaternion in 3D, or a rotation angle in 2D. Almost all objects in a scene have a transform.");
}

template <Dimension D> bool TransformEditor(Transform<D> &transform, const EditorFlags flags)
{
    ImGui::PushID(&transform);
    if (flags & EditorFlag_DisplayHelp)
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

template bool TransformEditor<D2>(Transform<D2> &transform, EditorFlags flags);
template bool TransformEditor<D3>(Transform<D3> &transform, EditorFlags flags);

template <Dimension D> void DisplayTransform(const Transform<D> &transform, const EditorFlags flags)
{
    const f32v<D> &translation = transform.Translation;
    const f32v<D> &scale = transform.Scale;

    if (flags & EditorFlag_DisplayHelp)
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

template void DisplayTransform<D2>(const Transform<D2> &transform, EditorFlags flags);
template void DisplayTransform<D3>(const Transform<D3> &transform, EditorFlags flags);

template <Dimension D> void DisplayCameraControls(const CameraControls<D> &controls)
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

template void DisplayCameraControls<D2>(const CameraControls<D2> &controls);
template void DisplayCameraControls<D3>(const CameraControls<D3> &controls);

bool DeltaTimeEditor(DeltaTime &dt, DeltaInfo &di, const Window *window, const EditorFlags flags)
{
    if (dt.Measured > di.Max)
        di.Max = dt.Measured;
    di.Smoothed = di.Smoothness * di.Smoothed + (1.f - di.Smoothness) * dt.Measured;

    ImGui::SliderFloat("Smoothing factor", &di.Smoothness, 0.f, 0.999f);
    if (flags & EditorFlag_DisplayHelp)
        HelpMarkerSameLine(
            "Because frames get dispatched so quickly, the frame time can vary a lot, be inconsistent, and hard to "
            "see. This slider allows you to smooth out the frame time across frames, making it easier to see the "
            "trend.");

    ImGui::Combo("Unit", &di.Unit, "s\0ms\0us\0ns\0");
    const u32 mfreq = ToFrequency(di.Smoothed);
    u32 tfreq = ToFrequency(dt.Target);

    bool changed = false;
    if (window && window->IsVSync())
        ImGui::Text("Target hertz: %u", tfreq);
    else
    {
        changed = ImGui::Checkbox("Limit hertz", &di.LimitHertz);
        if (changed)
        {
            if (di.LimitHertz)
                dt.Target = window ? window->GetMonitorDeltaTime() : ToDeltaTime(60);
            else
                dt.Target = TKit::Timespan{};
        }

        if (di.LimitHertz)
        {
            const u32 mn = 30;
            const u32 mx = 240;
            if (ImGui::SliderScalarN("Target hertz", ImGuiDataType_U32, &tfreq, 1, &mn, &mx))
            {
                dt.Target = ToDeltaTime(tfreq);
                changed = true;
            }
        }
    }
    ImGui::Text("Measured hertz: %u", mfreq);

    if (di.Unit == 0)
        ImGui::Text("Measured delta time: %.4f s (max: %.4f s)", di.Smoothed.AsSeconds(), di.Max.AsSeconds());
    else if (di.Unit == 1)
        ImGui::Text("Measured delta time: %.2f ms (max: %.2f ms)", di.Smoothed.AsMilliseconds(),
                    di.Max.AsMilliseconds());
    else if (di.Unit == 2)
        ImGui::Text("Measured delta time: %u us (max: %u us)", static_cast<u32>(di.Smoothed.AsMicroseconds()),
                    static_cast<u32>(di.Max.AsMicroseconds()));
    else
#    ifndef TKIT_OS_LINUX
        ImGui::Text("Measured delta time: %llu ns (max: %llu ns)", di.Smoothed.AsNanoseconds(), di.Max.AsNanoseconds());
#    else
        ImGui::Text("Measured delta time: %lu ns (max: %lu ns)", di.Smoothed.AsNanoseconds(), di.Max.AsNanoseconds());
#    endif

    if (flags & EditorFlag_DisplayHelp)
        HelpMarkerSameLine(
            "The delta time is a measure of the time it takes to complete a frame loop around a particular callback "
            "(which can be an update or render callback), and it is one of the main indicators of an application "
            "smoothness. It is also used to calculate the frames per second (FPS) of the application. A good frame "
            "time is usually no larger than 16.67 ms (that is, 60 fps). It is also bound to the present mode of the "
            "window.");

    if (ImGui::Button("Reset maximum"))
        di.Max = TKit::Timespan{};
    return changed;
}

void HelpMarker(const char *description, const char *icon)
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
void HelpMarkerSameLine(const char *description, const char *icon)
{
    ImGui::SameLine();
    HelpMarker(description, icon);
}

bool DirectionalLightEditor(DirectionalLight &light, const EditorFlags flags)
{
    bool changed = false;
    if (flags & EditorFlag_DisplayHelp)
        HelpMarker(
            "Directional lights are lights that have no position, only a direction. They are used to simulate "
            "infinite light sources, such as the sun. They have a direction, an intensity, and a color. The "
            "direction is a Math::Normalized vector that points in the direction of the light, the intensity is the "
            "brightness of the light, and the color is the color of the light.");
    ImGui::PushID(&light);

    f32 intensity = light.GetIntensity();
    if (ImGui::SliderFloat("Intensity", &intensity, 0.f, 1.f))
    {
        light.SetIntensity(intensity);
        changed = true;
    }
    f32v3 direction = light.GetDirection();
    if (ImGui::SliderFloat3("Direction", Math::AsPointer(direction), 0.f, 1.f))
    {
        light.SetDirection(direction);
        changed = true;
    }

    Color color = light.GetColor();
    if (ImGui::ColorEdit3("Color", color.GetData()))
    {
        light.SetColor(color);
        changed = true;
    }
    ImGui::PopID();

    return changed;
}

template <Dimension D> bool PointLightEditor(PointLight<D> &light, const EditorFlags flags)
{
    bool changed = false;
    if (flags & EditorFlag_DisplayHelp)
        HelpMarker(
            "Point lights are lights that have a position and a radius. They are used to simulate light sources "
            "that emit light in all directions, such as light bulbs. They have a position, an intensity, a "
            "radius, and a color. The position is the position of the light, the intensity is the brightness of "
            "the light, the radius is the distance at which the light is still visible, and the color is the color "
            "of the light.");
    ImGui::PushID(&light);

    f32 intensity = light.GetIntensity();
    if (ImGui::SliderFloat("Intensity", &intensity, 0.f, 1.f))
    {
        light.SetIntensity(intensity);
        changed = true;
    }
    if constexpr (D == D2)
    {
        f32v2 position = light.GetPosition();
        if (ImGui::DragFloat2("Direction", Math::AsPointer(position), 0.f, 1.f))
        {
            light.SetPosition(position);
            changed = true;
        }
    }
    else
    {
        f32v3 position = light.GetPosition();
        if (ImGui::DragFloat3("Direction", Math::AsPointer(position), 0.f, 1.f))
        {
            light.SetPosition(position);
            changed = true;
        }
    }
    f32 radius = light.GetRadius();
    if (ImGui::SliderFloat("Radius", &radius, 0.f, 1.f))
    {
        light.SetIntensity(radius);
        changed = true;
    }

    Color color = light.GetColor();
    if (ImGui::ColorEdit3("Color", color.GetData()))
    {
        light.SetColor(color);
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

template bool PointLightEditor<D2>(PointLight<D2> &light, EditorFlags flags);
template bool PointLightEditor<D3>(PointLight<D3> &light, EditorFlags flags);

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

bool PresentModeEditor(Window *window, const EditorFlags flags)
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

    if (flags & EditorFlag_DisplayHelp)
        HelpMarkerSameLine("Controls the frequency with which rendered images are sent to the screen. This setting "
                           "can be used to limit the frame rate of the application. The most common present mode is "
                           "Fifo, and uses V-Sync to synchronize the frame rate with the "
                           "refresh rate of the monitor.");
    return changed;
}

bool ViewportEditor(ScreenViewport &viewport, const EditorFlags flags)
{
    bool changed = false;
    ImGui::PushID(&viewport);
    if (flags & EditorFlag_DisplayHelp)
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

bool ScissorEditor(ScreenScissor &scissor, const EditorFlags flags)
{
    bool changed = false;
    ImGui::PushID(&scissor);
    if (flags & EditorFlag_DisplayHelp)
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

void ConfigurationEditor()
{
    if (ImGui::CollapsingHeader("Configuration"))
    {
        ImGuiIO &io = ImGui::GetIO();

        if (ImGui::TreeNode("Configuration##2"))
        {
            ImGui::SeparatorText("General");
            ImGui::CheckboxFlags("io.ConfigFlags: NavEnableKeyboard", &io.ConfigFlags,
                                 ImGuiConfigFlags_NavEnableKeyboard);
            ImGui::SameLine();
            HelpMarker("Enable keyboard controls.");
            ImGui::CheckboxFlags("io.ConfigFlags: NavEnableGamepad", &io.ConfigFlags,
                                 ImGuiConfigFlags_NavEnableGamepad);
            ImGui::SameLine();
            HelpMarker("Enable gamepad controls. Require backend to set io.BackendFlags |= "
                       "ImGuiBackendFlags_HasGamepad.\n\nRead instructions in imgui.cpp for details.");
            ImGui::CheckboxFlags("io.ConfigFlags: NoMouse", &io.ConfigFlags, ImGuiConfigFlags_NoMouse);
            ImGui::SameLine();
            HelpMarker("Instruct dear imgui to disable mouse inputs and interactions.");

            // The "NoMouse" option can get us stuck with a disabled mouse! Let's provide an alternative way to fix it:
            if (io.ConfigFlags & ImGuiConfigFlags_NoMouse)
            {
                if (fmodf((float)ImGui::GetTime(), 0.40f) < 0.20f)
                {
                    ImGui::SameLine();
                    ImGui::Text("<<PRESS SPACE TO DISABLE>>");
                }
                // Prevent both being checked
                if (ImGui::IsKeyPressed(ImGuiKey_Space) || (io.ConfigFlags & ImGuiConfigFlags_NoKeyboard))
                    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            }

            ImGui::CheckboxFlags("io.ConfigFlags: NoMouseCursorChange", &io.ConfigFlags,
                                 ImGuiConfigFlags_NoMouseCursorChange);
            ImGui::SameLine();
            HelpMarker("Instruct backend to not alter mouse cursor shape and visibility.");
            ImGui::CheckboxFlags("io.ConfigFlags: NoKeyboard", &io.ConfigFlags, ImGuiConfigFlags_NoKeyboard);
            ImGui::SameLine();
            HelpMarker("Instruct dear imgui to disable keyboard inputs and interactions.");

            ImGui::Checkbox("io.ConfigInputTrickleEventQueue", &io.ConfigInputTrickleEventQueue);
            ImGui::SameLine();
            HelpMarker(
                "Enable input queue trickling: some types of events submitted during the same frame (e.g. button down "
                "+ up) will be spread over multiple frames, improving interactions with low framerates.");
            ImGui::Checkbox("io.MouseDrawCursor", &io.MouseDrawCursor);
            ImGui::SameLine();
            HelpMarker("Instruct Dear ImGui to render a mouse cursor itself. Note that a mouse cursor rendered via "
                       "your application GPU rendering path will feel more laggy than hardware cursor, but will be "
                       "more in sync with your other visuals.\n\nSome desktop applications may use both kinds of "
                       "cursors (e.g. enable software cursor only when resizing/dragging something).");

            ImGui::SeparatorText("Keyboard/Gamepad Navigation");
            ImGui::Checkbox("io.ConfigNavSwapGamepadButtons", &io.ConfigNavSwapGamepadButtons);
            ImGui::Checkbox("io.ConfigNavMoveSetMousePos", &io.ConfigNavMoveSetMousePos);
            ImGui::SameLine();
            HelpMarker("Directional/tabbing navigation teleports the mouse cursor. May be useful on TV/console systems "
                       "where moving a virtual mouse is difficult");
            ImGui::Checkbox("io.ConfigNavCaptureKeyboard", &io.ConfigNavCaptureKeyboard);
            ImGui::Checkbox("io.ConfigNavEscapeClearFocusItem", &io.ConfigNavEscapeClearFocusItem);
            ImGui::SameLine();
            HelpMarker("Pressing Escape clears focused item.");
            ImGui::Checkbox("io.ConfigNavEscapeClearFocusWindow", &io.ConfigNavEscapeClearFocusWindow);
            ImGui::SameLine();
            HelpMarker("Pressing Escape clears focused window.");
            ImGui::Checkbox("io.ConfigNavCursorVisibleAuto", &io.ConfigNavCursorVisibleAuto);
            ImGui::SameLine();
            HelpMarker("Using directional navigation key makes the cursor visible. Mouse click hides the cursor.");
            ImGui::Checkbox("io.ConfigNavCursorVisibleAlways", &io.ConfigNavCursorVisibleAlways);
            ImGui::SameLine();
            HelpMarker("Navigation cursor is always visible.");

            ImGui::SeparatorText("Docking");
            ImGui::CheckboxFlags("io.ConfigFlags: DockingEnable", &io.ConfigFlags, ImGuiConfigFlags_DockingEnable);
            ImGui::SameLine();
            if (io.ConfigDockingWithShift)
                HelpMarker(
                    "Drag from window title bar or their tab to dock/undock. Hold SHIFT to enable docking.\n\nDrag "
                    "from window menu button (upper-left button) to undock an entire node (all windows).");
            else
                HelpMarker(
                    "Drag from window title bar or their tab to dock/undock. Hold SHIFT to disable docking.\n\nDrag "
                    "from window menu button (upper-left button) to undock an entire node (all windows).");
            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                ImGui::Indent();
                ImGui::Checkbox("io.ConfigDockingNoSplit", &io.ConfigDockingNoSplit);
                ImGui::SameLine();
                HelpMarker("Simplified docking mode: disable window splitting, so docking is limited to merging "
                           "multiple windows together into tab-bars.");
                ImGui::Checkbox("io.ConfigDockingNoDockingOver", &io.ConfigDockingNoDockingOver);
                ImGui::SameLine();
                HelpMarker("Simplified docking mode: disable window merging into a same tab-bar, so docking is limited "
                           "to splitting windows.");
                ImGui::Checkbox("io.ConfigDockingWithShift", &io.ConfigDockingWithShift);
                ImGui::SameLine();
                HelpMarker(
                    "Enable docking when holding Shift only (allow to drop in wider space, reduce visual noise)");
                ImGui::Checkbox("io.ConfigDockingAlwaysTabBar", &io.ConfigDockingAlwaysTabBar);
                ImGui::SameLine();
                HelpMarker("Create a docking node and tab-bar on single floating windows.");
                ImGui::Checkbox("io.ConfigDockingTransparentPayload", &io.ConfigDockingTransparentPayload);
                ImGui::SameLine();
                HelpMarker("Make window or viewport transparent when docking and only display docking boxes on the "
                           "target viewport. Useful if rendering of multiple viewport cannot be synced. Best used with "
                           "ConfigViewportsNoAutoMerge.");
                ImGui::Unindent();
            }

            ImGui::SeparatorText("Multi-viewports");
            ImGui::CheckboxFlags("io.ConfigFlags: ViewportsEnable", &io.ConfigFlags, ImGuiConfigFlags_ViewportsEnable);
            ImGui::SameLine();
            HelpMarker("[beta] Enable beta multi-viewports support. See ImGuiPlatformIO for details.");
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                ImGui::Indent();
                ImGui::Checkbox("io.ConfigViewportsNoAutoMerge", &io.ConfigViewportsNoAutoMerge);
                ImGui::SameLine();
                HelpMarker("Set to make all floating imgui windows always create their own viewport. Otherwise, they "
                           "are merged into the main host viewports when overlapping it.");
                ImGui::Checkbox("io.ConfigViewportsNoTaskBarIcon", &io.ConfigViewportsNoTaskBarIcon);
                ImGui::SameLine();
                HelpMarker("(note: some platform backends may not reflect a change of this value for existing "
                           "viewports, and may need the viewport to be recreated)");
                ImGui::Checkbox("io.ConfigViewportsNoDecoration", &io.ConfigViewportsNoDecoration);
                ImGui::SameLine();
                HelpMarker("(note: some platform backends may not reflect a change of this value for existing "
                           "viewports, and may need the viewport to be recreated)");
                ImGui::Checkbox("io.ConfigViewportsNoDefaultParent", &io.ConfigViewportsNoDefaultParent);
                ImGui::SameLine();
                HelpMarker("(note: some platform backends may not reflect a change of this value for existing "
                           "viewports, and may need the viewport to be recreated)");
                ImGui::Checkbox("io.ConfigViewportsPlatformFocusSetsImGuiFocus",
                                &io.ConfigViewportsPlatformFocusSetsImGuiFocus);
                ImGui::SameLine();
                HelpMarker("When a platform window is focused (e.g. using Alt+Tab, clicking Platform Title Bar), apply "
                           "corresponding focus on imgui windows (may clear focus/active id from imgui windows "
                           "location in other platform windows). In principle this is better enabled but we provide an "
                           "opt-out, because some Linux window managers tend to eagerly focus windows (e.g. on mouse "
                           "hover, or even a simple window pos/size change).");
                ImGui::Unindent();
            }

            // ImGui::SeparatorText("DPI/Scaling");
            // ImGui::Checkbox("io.ConfigDpiScaleFonts", &io.ConfigDpiScaleFonts);
            // ImGui::SameLine(); HelpMarker("Experimental: Automatically update style.FontScaleDpi when Monitor DPI
            // changes. This will scale fonts but NOT style sizes/padding for now.");
            // ImGui::Checkbox("io.ConfigDpiScaleViewports", &io.ConfigDpiScaleViewports);
            // ImGui::SameLine(); HelpMarker("Experimental: Scale Dear ImGui and Platform Windows when Monitor DPI
            // changes.");

            ImGui::SeparatorText("Windows");
            ImGui::Checkbox("io.ConfigWindowsResizeFromEdges", &io.ConfigWindowsResizeFromEdges);
            ImGui::SameLine();
            HelpMarker("Enable resizing of windows from their edges and from the lower-left corner.\nThis requires "
                       "ImGuiBackendFlags_HasMouseCursors for better mouse cursor feedback.");
            ImGui::Checkbox("io.ConfigWindowsMoveFromTitleBarOnly", &io.ConfigWindowsMoveFromTitleBarOnly);
            ImGui::Checkbox("io.ConfigWindowsCopyContentsWithCtrlC",
                            &io.ConfigWindowsCopyContentsWithCtrlC); // [EXPERIMENTAL]
            ImGui::SameLine();
            HelpMarker("*EXPERIMENTAL* Ctrl+C copy the contents of focused window into the clipboard.\n\nExperimental "
                       "because:\n- (1) has known issues with nested Begin/End pairs.\n- (2) text output quality "
                       "varies.\n- (3) text output is in submission order rather than spatial order.");
            ImGui::Checkbox("io.ConfigScrollbarScrollByPage", &io.ConfigScrollbarScrollByPage);
            ImGui::SameLine();
            HelpMarker("Enable scrolling page by page when clicking outside the scrollbar grab.\nWhen disabled, always "
                       "scroll to clicked location.\nWhen enabled, Shift+Click scrolls to clicked location.");

            ImGui::SeparatorText("Widgets");
            ImGui::Checkbox("io.ConfigInputTextCursorBlink", &io.ConfigInputTextCursorBlink);
            ImGui::SameLine();
            HelpMarker("Enable blinking cursor (optional as some users consider it to be distracting).");
            ImGui::Checkbox("io.ConfigInputTextEnterKeepActive", &io.ConfigInputTextEnterKeepActive);
            ImGui::SameLine();
            HelpMarker("Pressing Enter will keep item active and select contents (single-line only).");
            ImGui::Checkbox("io.ConfigDragClickToInputText", &io.ConfigDragClickToInputText);
            ImGui::SameLine();
            HelpMarker(
                "Enable turning DragXXX widgets into text input with a simple mouse click-release (without moving).");
            ImGui::Checkbox("io.ConfigMacOSXBehaviors", &io.ConfigMacOSXBehaviors);
            ImGui::SameLine();
            HelpMarker("Swap Cmd<>Ctrl keys, enable various MacOS style behaviors.");
            ImGui::Text("Also see Style->Rendering for rendering options.");

            // Also read: https://github.com/ocornut/imgui/wiki/Error-Handling
            ImGui::SeparatorText("Error Handling");

            ImGui::Checkbox("io.ConfigErrorRecovery", &io.ConfigErrorRecovery);
            ImGui::SameLine();
            HelpMarker("Options to configure how we handle recoverable errors.\n"
                       "- Error recovery is not perfect nor guaranteed! It is a feature to ease development.\n"
                       "- You not are not supposed to rely on it in the course of a normal application run.\n"
                       "- Possible usage: facilitate recovery from errors triggered from a scripting language or after "
                       "specific exceptions handlers.\n"
                       "- Always ensure that on programmers seat you have at minimum Asserts or Tooltips enabled when "
                       "making direct imgui API call! "
                       "Otherwise it would severely hinder your ability to catch and correct mistakes!");
            ImGui::Checkbox("io.ConfigErrorRecoveryEnableAssert", &io.ConfigErrorRecoveryEnableAssert);
            ImGui::Checkbox("io.ConfigErrorRecoveryEnableDebugLog", &io.ConfigErrorRecoveryEnableDebugLog);
            ImGui::Checkbox("io.ConfigErrorRecoveryEnableTooltip", &io.ConfigErrorRecoveryEnableTooltip);
            if (!io.ConfigErrorRecoveryEnableAssert && !io.ConfigErrorRecoveryEnableDebugLog &&
                !io.ConfigErrorRecoveryEnableTooltip)
                io.ConfigErrorRecoveryEnableAssert = io.ConfigErrorRecoveryEnableDebugLog =
                    io.ConfigErrorRecoveryEnableTooltip = true;

            // Also read: https://github.com/ocornut/imgui/wiki/Debug-Tools
            ImGui::SeparatorText("Debug");
            ImGui::Checkbox("io.ConfigDebugIsDebuggerPresent", &io.ConfigDebugIsDebuggerPresent);
            ImGui::SameLine();
            HelpMarker("Enable various tools calling IM_DEBUG_BREAK().\n\nRequires a debugger being attached, "
                       "otherwise IM_DEBUG_BREAK() options will appear to crash your application.");
            ImGui::Checkbox("io.ConfigDebugHighlightIdConflicts", &io.ConfigDebugHighlightIdConflicts);
            ImGui::SameLine();
            HelpMarker("Highlight and show an error message when multiple items have conflicting identifiers.");
            ImGui::BeginDisabled();
            ImGui::Checkbox("io.ConfigDebugBeginReturnValueOnce", &io.ConfigDebugBeginReturnValueOnce);
            ImGui::EndDisabled();
            ImGui::SameLine();
            HelpMarker("First calls to Begin()/BeginChild() will return false.\n\nTHIS OPTION IS DISABLED because it "
                       "needs to be set at application boot-time to make sense. Showing the disabled option is a way "
                       "to make this feature easier to discover.");
            ImGui::Checkbox("io.ConfigDebugBeginReturnValueLoop", &io.ConfigDebugBeginReturnValueLoop);
            ImGui::SameLine();
            HelpMarker("Some calls to Begin()/BeginChild() will return false.\n\nWill cycle through window depths then "
                       "repeat. Windows should be flickering while running.");
            ImGui::Checkbox("io.ConfigDebugIgnoreFocusLoss", &io.ConfigDebugIgnoreFocusLoss);
            ImGui::SameLine();
            HelpMarker("Option to deactivate io.AddFocusEvent(false) handling. May facilitate interactions with a "
                       "debugger when focus loss leads to clearing inputs data.");
            ImGui::Checkbox("io.ConfigDebugIniSettings", &io.ConfigDebugIniSettings);
            ImGui::SameLine();
            HelpMarker("Option to save .ini data with extra comments (particularly helpful for Docking, but makes "
                       "saving slower).");

            ImGui::TreePop();
            ImGui::Spacing();
        }

        if (ImGui::TreeNode("Backend Flags"))
        {
            HelpMarker("Those flags are set by the backends (imgui_impl_xxx files) to specify their capabilities.\n"
                       "Here we expose them as read-only fields to avoid breaking interactions with your backend.");

            // Make a local copy to avoid modifying actual backend flags.
            // FIXME: Maybe we need a BeginReadonly() equivalent to keep label bright?
            ImGui::BeginDisabled();
            ImGui::CheckboxFlags("io.BackendFlags: HasGamepad", &io.BackendFlags, ImGuiBackendFlags_HasGamepad);
            ImGui::CheckboxFlags("io.BackendFlags: HasMouseCursors", &io.BackendFlags,
                                 ImGuiBackendFlags_HasMouseCursors);
            ImGui::CheckboxFlags("io.BackendFlags: HasSetMousePos", &io.BackendFlags, ImGuiBackendFlags_HasSetMousePos);
            ImGui::CheckboxFlags("io.BackendFlags: PlatformHasViewports", &io.BackendFlags,
                                 ImGuiBackendFlags_PlatformHasViewports);
            ImGui::CheckboxFlags("io.BackendFlags: HasMouseHoveredViewport", &io.BackendFlags,
                                 ImGuiBackendFlags_HasMouseHoveredViewport);
            ImGui::CheckboxFlags("io.BackendFlags: HasParentViewport", &io.BackendFlags,
                                 ImGuiBackendFlags_HasParentViewport);
            ImGui::CheckboxFlags("io.BackendFlags: RendererHasVtxOffset", &io.BackendFlags,
                                 ImGuiBackendFlags_RendererHasVtxOffset);
            ImGui::CheckboxFlags("io.BackendFlags: RendererHasTextures", &io.BackendFlags,
                                 ImGuiBackendFlags_RendererHasTextures);
            ImGui::CheckboxFlags("io.BackendFlags: RendererHasViewports", &io.BackendFlags,
                                 ImGuiBackendFlags_RendererHasViewports);
            ImGui::EndDisabled();

            ImGui::TreePop();
            ImGui::Spacing();
        }
    }
}

} // namespace Onyx

#endif
