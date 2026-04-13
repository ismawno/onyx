#pragma once

#include "onyx/core/alias.hpp"

namespace Onyx
{
// TODO(Isma): Rename to PipelinePass
// with UI, should be like this (rename for now)
// PipelinePass_NoStencilWriteDoShade,
// PipelinePass_NoStencilWriteDoFlat,
// PipelinePass_DoStencilWriteDoShade,
// PipelinePass_DoStencilWriteDoFlat,
// PipelinePass_DoStencilWriteNoColor,
// PipelinePass_DoStencilTestFlat,
enum StencilPass : u8
{
    StencilPass_NoStencilWriteDoFill,
    StencilPass_DoStencilWriteDoFill,
    StencilPass_DoStencilWriteNoFill,
    StencilPass_DoStencilTestNoFill,
    StencilPass_Count
};
// TODO(Isma): rename fill to shaded, stencil to flat
// RenderPass_Shade,
// RenderPass_Flat,
// RenderPass_Shadow,
// RenderPass_Count
enum RenderPass : u8
{
    RenderPass_Fill,
    RenderPass_Stencil,
    RenderPass_Shadow,
    RenderPass_Count
};

// TODO(Isma): ACTUALLY I CAN HAVE FLAGS AND INDEX WITH FLAGS, IT WILL JUST BE NORMAL INDICES. do that
// RenderFlag_Shade = 1 << 0,
// RenderFlag_Stencil = 1 << 1,
// RenderFlag_UserInterface = 1 << 2,
// RenderMode_Count = 5, // cant have shade + user interface
enum RenderMode : u8
{
    RenderMode_Fill,
    RenderMode_Stencil,
    RenderMode_FillStencil,
    RenderMode_Count,
    RenderMode_None = RenderMode_Count,
};

constexpr RenderPass GetRenderPass(const StencilPass pass)
{
    if (pass == StencilPass_NoStencilWriteDoFill || pass == StencilPass_DoStencilWriteDoFill)
        return RenderPass_Fill;
    return RenderPass_Stencil;
}

const char *ToString(StencilPass pass);
const char *ToString(RenderPass pass);
const char *ToString(RenderMode mode);

} // namespace Onyx
