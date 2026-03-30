#include "onyx/core/pch.hpp"
#ifdef ONYX_ENABLE_GLTF_LOAD
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
#elif defined(ONYX_ENABLE_IMAGE_LOAD)
#    define STB_IMAGE_IMPLEMENTATION
#    include "onyx/vendor/stb_image.hpp"
#endif
