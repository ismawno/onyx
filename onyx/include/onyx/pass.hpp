#pragma once

#include "onyx/alias.hpp"

namespace Onyx
{
using RenderModeFlags = u8;
enum RenderModeFlagBit : RenderModeFlags
{
    RenderModeFlag_None = 0,
    RenderModeFlag_Flat = 1 << 1,
    RenderModeFlag_Shaded = 1 << 0,
    RenderModeFlag_Outlined = 1 << 2,
};
enum RenderMode : u8
{
    RenderMode_Flat,
    RenderMode_Shaded,
    RenderMode_Outlined,
    RenderMode_ShadedOutlined,
    RenderMode_FlatOutlined,
    RenderMode_Count,
    RenderMode_None = RenderMode_Count
};
constexpr RenderMode GetRenderMode(const RenderModeFlags flags)
{
    switch (flags)
    {
    case RenderModeFlag_Shaded:
        return RenderMode_Shaded;
    case RenderModeFlag_Flat:
        return RenderMode_Flat;
    case RenderModeFlag_Outlined:
        return RenderMode_Outlined;
    case RenderModeFlag_Shaded | RenderModeFlag_Outlined:
        return RenderMode_ShadedOutlined;
    case RenderModeFlag_Flat | RenderModeFlag_Outlined:
        return RenderMode_FlatOutlined;
    default:
        TKIT_FATAL("[ONYX][PASS] Unrecognized render mode flags: {}", flags);
        return RenderMode_None;
    }
}
const char *ToString(RenderMode mode);
constexpr RenderModeFlags GetRenderModeFlags(const RenderMode mode)
{
    switch (mode)
    {
    case RenderMode_Shaded:
        return RenderModeFlag_Shaded;
    case RenderMode_Flat:
        return RenderModeFlag_Flat;
    case RenderMode_Outlined:
        return RenderModeFlag_Outlined;
    case RenderMode_ShadedOutlined:
        return RenderModeFlag_Shaded | RenderModeFlag_Outlined;
    case RenderMode_FlatOutlined:
        return RenderModeFlag_Flat | RenderModeFlag_Outlined;
    default:
        TKIT_FATAL("[ONYX][PASS] Unrecognized render mode: {}", ToString(mode));
        return 0;
    }
}
} // namespace Onyx
