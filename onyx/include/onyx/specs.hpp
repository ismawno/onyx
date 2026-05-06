#pragma once
#include "onyx/alias.hpp"
#include "onyx/instance.hpp"
#include "onyx/image.hpp"

#define ONYX_PLATFORM_ANY 0x00060000
#define ONYX_PLATFORM_WIN32 0x00060001
#define ONYX_PLATFORM_COCOA 0x00060002
#define ONYX_PLATFORM_WAYLAND 0x00060003
#define ONYX_PLATFORM_X11 0x00060004
#define ONYX_PLATFORM_NULL 0x00060005

#ifdef TKIT_OS_LINUX
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_X11
#elif defined(TKIT_OS_APPLE)
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_COCOA
#elif defined(TKIT_OS_WINDOWS)
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_WIN32
#endif

#ifndef ONYX_PLATFORM_AUTO
#    define ONYX_PLATFORM_AUTO ONYX_PLATFORM_ANY
#endif

namespace Onyx
{
namespace Execution
{
struct Specs
{
    u32 MaxCommandPools = 1024;
};
} // namespace Execution
namespace Resources
{
struct Specs
{
    u32 MaxBuffers = 1024;
    u32 MaxSamplers = 8;
    u32 MaxStandaloneSamplers = 8;
    u32 MaxImages = 512;
    u32 MaxTextures = 1024;
    u32 MaxMaterials = 256;
    u32 MaxBounds = 1024;
};
} // namespace Resources
namespace Descriptors
{
struct Specs
{
    u32 MaxSets = 1 << 10;
    u32 StorageBufferPoolSize = 1 << 14;
    u32 SamplerPoolSize = 1 << 10;
    u32 SampledImagePoolSize = 1 << 17;
    u32 CombinedImageSamplerPoolSize = 1 << 10;
    u32 StorageImagePoolSize = 1 << 9;
};
} // namespace Descriptors

#ifdef ONYX_ENABLE_SHADER_API
namespace Shaders
{
struct Specs
{
    bool EnableGlsl = false;
};
} // namespace Shaders
#endif

namespace Renderer
{
template <Dimension D> struct ShadowSpecs;
template <> struct ShadowSpecs<D2>
{
    Format OcclusionFormat = Format_R8_UNORM;
    Format ShadowFormat = Format_D32_SFLOAT;
    u32 OcclusionResolution = 1024;
    TKit::FixedArray<u32, LightTypeCount<D2>> ShadowResolutions{1024, 1024};
};
template <> struct ShadowSpecs<D3>
{
    Format ShadowFormat = Format_D32_SFLOAT;
    TKit::FixedArray<u32, LightTypeCount<D3>> ShadowResolutions{512, 2048, 1024};
};

struct Specs
{
    ShadowSpecs<D2> Shadows2{};
    ShadowSpecs<D3> Shadows3{};
};

} // namespace Renderer
namespace Platform
{
struct Specs
{
    u32 Platform = ONYX_PLATFORM_AUTO;
};
} // namespace Platform
} // namespace Onyx
