#pragma once

#include "onyx/window.hpp"

namespace Onyx::Platform
{
Window *CreateWindow(const WindowSpecs &specs = {});
void DestroyWindow(Window *window);
void DestroyWindows();

void SetClipboard(const char *data);
const char *GetClipboard();

} // namespace Onyx::Platform
