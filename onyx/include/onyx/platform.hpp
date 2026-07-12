#pragma once

#include "onyx/window.hpp"

namespace Onyx::Platform
{
struct VideoMode
{
    u32v2 Dimensions;
    u32v3 RgbBits;
    u32 RefreshRate;
};

Window *CreateWindow(const WindowSpecs &specs = {});
void DestroyWindow(Window *window);
void DestroyWindows();

void SetClipboard(const char *data);
const char *GetClipboard();

Onyx_MonitorHandle *GetMonitor(Onyx_WindowHandle *win);
Onyx_MonitorHandle *GetMainMonitor();

VideoMode GetVideoMode(Onyx_MonitorHandle *monitor);

} // namespace Onyx::Platform
