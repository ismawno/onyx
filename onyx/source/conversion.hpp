#pragma once

#include "onyx/instance.hpp"
#include "onyx/image.hpp"
#include "state.hpp"
#include "graph/core.hpp"

namespace Onyx
{
const char *ToString(Geometry geo);
const char *ToString(LightType light);
const char *ToString(RenderPass pass);

Graph::Format GetFormat(const u32 components, ImageComponentType type, bool rgb);

} // namespace Onyx
