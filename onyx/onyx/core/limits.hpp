#pragma once

#include "onyx/core/alias.hpp"
#include "tkit/utils/limits.hpp"

#ifndef ONYX_APP_MAX_WINDOWS
#    define ONYX_APP_MAX_WINDOWS 8
#endif

#if ONYX_APP_MAX_WINDOWS > 1
#    define __ONYX_MULTI_WINDOW
#endif

#ifndef ONYX_MAX_DIALOGS
#    define ONYX_MAX_DIALOGS 64
#endif

#ifndef ONYX_ASSETS_MAX_STATIC_MESHES
#    define ONYX_ASSETS_MAX_STATIC_MESHES 64
#endif

#ifndef ONYX_MAX_DESCRIPTOR_SETS
#    define ONYX_MAX_DESCRIPTOR_SETS 1024
#endif

#ifndef ONYX_MAX_DESCRIPTORS
#    define ONYX_MAX_DESCRIPTORS 1024
#endif

#ifndef ONYX_MAX_FRAMES_IN_FLIGHT
#    define ONYX_MAX_FRAMES_IN_FLIGHT 2
#endif

#if ONYX_MAX_FRAMES_IN_FLIGHT < 2
#    error "[ONYX] Maximum frames in flight must be greater than 2"
#endif

// The amount of active threads (accounting for the main thread as well) should not surpass this number. This means
// thread pools should be created with LESS threads than this limit.
#ifndef ONYX_MAX_THREADS
#    define ONYX_MAX_THREADS TKIT_MAX_THREADS
#endif

#ifndef ONYX_MAX_TASKS
#    define ONYX_MAX_TASKS 256
#endif

#ifndef ONYX_MAX_contextS
#    define ONYX_MAX_contextS 16
#endif

#ifndef ONYX_MAX_CAMERAS
#    define ONYX_MAX_CAMERAS 16
#endif

#ifndef ONYX_MAX_EVENTS
#    define ONYX_MAX_EVENTS 32
#endif

#ifndef ONYX_MAX_RENDER_STATE_DEPTH
#    define ONYX_MAX_RENDER_STATE_DEPTH 8
#endif

#ifndef ONYX_MAX_BATCHES
#    define ONYX_MAX_BATCHES 32
#endif

#ifndef ONYX_MAX_RENDERER_RANGES
#    define ONYX_MAX_RENDERER_RANGES 32
#endif

namespace Onyx
{
constexpr u32 MaxWindows = ONYX_APP_MAX_WINDOWS;
constexpr u32 MaxDialogs = ONYX_MAX_DIALOGS;
constexpr u32 MaxStatMeshes = ONYX_ASSETS_MAX_STATIC_MESHES;
constexpr u32 MaxDescriptorSets = ONYX_MAX_DESCRIPTOR_SETS;
constexpr u32 MaxDescriptors = ONYX_MAX_DESCRIPTORS;
constexpr u32 MaxFramesInFlight = ONYX_MAX_FRAMES_IN_FLIGHT; // hard limit
constexpr u32 MaxThreads = ONYX_MAX_THREADS;                 // hard limit
constexpr u32 MaxTasks = ONYX_MAX_TASKS;
constexpr u32 MaxRenderContexts = ONYX_MAX_contextS; // hard limit
constexpr u32 MaxCameras = ONYX_MAX_CAMERAS;                // hard limit
constexpr u32 MaxEvents = ONYX_MAX_EVENTS;
constexpr u32 MaxRenderStateDepth = ONYX_MAX_RENDER_STATE_DEPTH;
constexpr u32 MaxBatches = ONYX_MAX_BATCHES; // hard limit
constexpr u32 MaxRendererRanges = ONYX_MAX_RENDERER_RANGES;
} // namespace Onyx
