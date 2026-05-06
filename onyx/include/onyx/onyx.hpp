#pragma once

#include "onyx/window.hpp"
#include "onyx/context.hpp"

namespace Onyx
{
using OpenWindowFlags = u8;
enum OpenWindowFlagBit : OpenWindowFlags
{
#ifdef ONYX_ENABLE_IMGUI
    OpenWindowFlag_EnableImGui = 1 << 0,
#endif
};

struct OpenWindowSpecs
{
    WindowSpecs Window = {};
    TKit::Timespan TargetDeltaTime{};
    OpenWindowFlags Flags = 0;
};

Window *OpenWindow(const OpenWindowSpecs &specs = {});
void CloseWindow(Window *window);

#ifdef ONYX_ENABLE_IMGUI
bool CanRenderImGui(Window *window);
#endif

template <Dimension D> RenderContext<D> *CreateRenderContext();
template <Dimension D> void DestroyRenderContex(RenderContext<D> *ctx);

bool Running();
void Quit();

struct TransferInfo
{
    u32 CoalescePeriod = 1;
};

struct RenderInfo
{
    Timeout AcquireImageTimeout = Block;
};

void Transfer(const TransferInfo &info = {});
void Render(const RenderInfo &info = {});
} // namespace Onyx
