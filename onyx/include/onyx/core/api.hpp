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

#ifndef ONYX_CS_CENTERED_CARTESIAN
#    define ONYX_CS_CENTERED_CARTESIAN 0
#endif

#ifndef ONYX_CS_CENTERED_CARTESIAN_FLIPPED_Y
#    define ONYX_CS_CENTERED_CARTESIAN_FLIPPED_Y 1
#endif

#ifndef ONYX_COORDINATE_SYSTEM
#    define ONYX_COORDINATE_SYSTEM ONYX_CS_CENTERED_CARTESIAN
#endif
