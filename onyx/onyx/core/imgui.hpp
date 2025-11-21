#include "tkit/preprocessor/system.hpp"
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_CLANG_WARNING_IGNORE("-Wnontrivial-memcall")
TKIT_GCC_WARNING_IGNORE("-Wnontrivial-memcall")
#ifdef ONYX_ENABLE_IMGUI
#    include <imgui.h>
#    include <backends/imgui_impl_glfw.h>
#    include <backends/imgui_impl_vulkan.h>
#endif
TKIT_COMPILER_WARNING_IGNORE_POP()
