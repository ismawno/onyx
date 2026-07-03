#pragma once

#include "onyx/window.hpp"
#include "onyx/render_texture.hpp"
#include "onyx/context.hpp"
#include "onyx/overlay.hpp"

namespace Onyx
{
using OpenWindowFlags = u8;
enum OpenWindowFlagBit : OpenWindowFlags
{
#ifdef ONYX_ENABLE_IMGUI
    OpenWindowFlag_EnableImGui = 1U << 0,
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

TKit::Timespan GetDeltaTime(const Window *win);
TKit::Timespan GetTargetDeltaTime(const Window *win);

void SetTargetDeltaTime(Window *win, TKit::Timespan target);

#ifdef ONYX_ENABLE_IMGUI
bool CanRenderImGui(Window *window);
#endif

template <Dimension D> RenderContext<D> *CreateRenderContext(u32 immediateDynamicMeshCapacity = 0);
template <Dimension D> void DestroyRenderContex(RenderContext<D> *ctx);

RenderTexture *CreateRenderTexture(const u32v2 &dimensions);
void DestroyRenderTexture(RenderTexture *rtex);

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
