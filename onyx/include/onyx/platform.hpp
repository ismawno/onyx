#pragma once

#include "onyx/window.hpp"

namespace Onyx::Platform
{
Window *CreateWindow();
void DestroyWindow(const Window *window);
void DestroyWindows();
} // namespace Onyx::Platform
