#pragma once

#include "onyx/window.hpp"
#include "tkit/math/tensor.hpp"

namespace Onyx::Platform
{
Window *CreateWindow(const WindowSpecs &specs = {});
void DestroyWindow(Window *window);
void DestroyWindows();

} // namespace Onyx::Platform
