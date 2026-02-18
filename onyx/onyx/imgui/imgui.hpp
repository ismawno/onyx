#pragma once

#ifndef ONYX_ENABLE_IMGUI
#    error                                                                                                             \
        "[ONYX][IMGUI] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_IMGUI"
#endif

#include "onyx/core/alias.hpp"
#include "onyx/core/dimension.hpp"
#include "tkit/profiling/timespan.hpp"
#include <imgui.h>

namespace Onyx
{
template <Dimension D> struct Transform;
template <Dimension D> struct CameraControls;

class DirectionalLight;
template <Dimension D> class PointLight;

struct ScreenViewport;
struct ScreenScissor;

struct DeltaTime;
struct DeltaInfo
{
    TKit::Timespan Max{};
    TKit::Timespan Smoothed{};
    f32 Smoothness = 0.f;
    i32 Unit = 1;
    bool LimitHertz = true;
};

class Window;

using EditorFlags = u8;
enum EditorFlagBit : EditorFlags
{
    EditorFlag_DisplayHelp = 1 << 0,
};

template <Dimension D> bool TransformEditor(Transform<D> &transform, EditorFlags flags = 0);

template <Dimension D> void DisplayTransform(const Transform<D> &transform, EditorFlags flags = 0);
template <Dimension D> void DisplayCameraControls(const CameraControls<D> &controls = {});

bool DeltaTimeEditor(DeltaTime &dt, DeltaInfo &di, const Window *window = nullptr, EditorFlags flags = 0);

bool DirectionalLightEditor(DirectionalLight &light, EditorFlags flags = 0);

template <Dimension D> bool PointLightEditor(PointLight<D> &light, EditorFlags flags = 0);

bool PresentModeEditor(Window *window, EditorFlags flags = 0);

bool ViewportEditor(ScreenViewport &viewport, EditorFlags flags = 0);
bool ScissorEditor(ScreenScissor &scissor, EditorFlags flags = 0);

void HelpMarker(const char *description, const char *icon = "(?)");
void HelpMarkerSameLine(const char *description, const char *icon = "(?)");

void ConfigurationEditor();

} // namespace Onyx
