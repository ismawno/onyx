#pragma once

#ifndef TKIT_ASAN_OPTIONS
#    define TKIT_ASAN_OPTIONS                                                                                          \
        "alloc_dealloc_mismatch=0"                                                                                     \
        ":print_suppressions=0"                                                                                        \
        ":detect_leaks=1"                                                                                              \
        ":strict_string_checks=1"                                                                                      \
        ":detect_stack_use_after_return=1"                                                                             \
        ":check_initialization_order=1"                                                                                \
        ":poison_history_size=16"                                                                                      \
        ":strict_init_order=1"
#endif

#ifndef TKIT_LSAN_OPTIONS
#    define TKIT_LSAN_OPTIONS                                                                                          \
        "print_suppressions=0"                                                                                         \
        ":exitcode=0"                                                                                                  \
        ":use_unaligned=1"
#endif

#define TKIT_LSAN_SUPPRESSIONS                                                                                         \
    "leak:libfontconfig\n"                                                                                             \
    "leak:libexpat\n"                                                                                                  \
    "leak:libnvidia\n"                                                                                                 \
    "leak:libGLX_nvidia\n"                                                                                             \
    "leak:libGL_nvidia\n"                                                                                              \
    "leak:libVkLayer_khronos_validation\n"                                                                             \
    "leak:libvulkan\n"                                                                                                 \
    "leak:SetDebugUtilsObjectNameEXT\n"                                                                                \
    "leak:FcConfigInit\n"                                                                                              \
    "leak:FcInitLoadConfigAndFonts\n"

#ifndef TKIT_UBSAN_OPTIONS
#    define TKIT_UBSAN_OPTIONS                                                                                         \
        "print_stacktrace=1"                                                                                           \
        ":halt_on_error=0"                                                                                             \
        ":report_error_type=1"
#endif

#ifndef TKIT_TSAN_OPTIONS
#    define TKIT_TSAN_OPTIONS                                                                                          \
        "halt_on_error=0"                                                                                              \
        ":second_deadlock_stack=1"                                                                                     \
        ":history_size=4"
#endif

#ifndef TKIT_TSAN_SUPPRESSIONS
#    define TKIT_TSAN_SUPPRESSIONS                                                                                     \
        "race:libnvidia\n"                                                                                             \
        "race:libVkLayer_khronos_validation\n"
#endif

#ifndef TKIT_MSAN_OPTIONS
#    define TKIT_MSAN_OPTIONS                                                                                          \
        "print_stats=0"                                                                                                \
        ":halt_on_error=0"
#endif

#include "tkit/core/sanitizer_options.hpp"
