#pragma once

#include "kit/core/api.hpp"

#ifdef KIT_OS_WINDOWS
#    ifdef ONYX_SHARED_LIBRARY
#        ifdef ONYX_EXPORT
#            define ONYX_API __declspec(dllexport)
#        else
#            define ONYX_API __declspec(dllimport)
#        endif
#    else
#        define ONYX_API
#    endif
#elif defined(KIT_OS_LINUX) || defined(KIT_OS_APPLE)
#    if defined(ONYX_SHARED_LIBRARY) && defined(ONYX_EXPORT)
#        define ONYX_API __attribute__((visibility("default")))
#    else
#        define ONYX_API
#    endif
#endif
