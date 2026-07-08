#include "onyx/alias.hpp"

namespace Onyx
{
class Overlay;
}

namespace Onyx::Renderer
{
template <Dimension D> void DisplayMemoryLayout(Overlay *ov);

// // TODO(Isma): This right now is unused and inaccessible. Do something about this
// #ifdef ONYX_ENABLE_IMGUI
// template <Dimension D> void DisplayMemoryLayout();
// #endif
} // namespace Onyx::Renderer
