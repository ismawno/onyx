#include "onyx/core/pch.hpp"
#if defined(ONYX_ENABLE_IMAGE_LOAD) && !defined(ONYX_ENABLE_GLTF_LOAD)
#    define STB_IMAGE_IMPLEMENTATION
#    include <stb_image.h>
#elif defined(ONYX_ENABLE_GLTF_LOAD)
#    define TINYGLTF_IMPLEMENTATION
#    define STB_IMAGE_IMPLEMENTATION
#    define STB_IMAGE_WRITE_IMPLEMENTATION
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_CLANG_WARNING_IGNORE("-Wdeprecated-literal-operator")
TKIT_CLANG_WARNING_IGNORE("-Wmissing-field-initializers")
TKIT_GCC_WARNING_IGNORE("-Wdeprecated-literal-operator")
TKIT_GCC_WARNING_IGNORE("-Wmissing-field-initializers")
#    include <tiny_gltf.h>
TKIT_COMPILER_WARNING_IGNORE_POP()
#endif
