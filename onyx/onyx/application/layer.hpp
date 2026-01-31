#pragma once

#include "onyx/rendering/renderer.hpp"
#include "onyx/application/window.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/theme.hpp"
#    include "onyx/imgui/imgui.hpp"
#endif
#include "tkit/profiling/timespan.hpp"
#include "tkit/profiling/clock.hpp"
#include <vulkan/vulkan.h>

#ifdef ONYX_ENABLE_IMGUI
struct ImGuiContext;
#    ifdef ONYX_ENABLE_IMPLOT
struct ImPlotContext;
#    endif
#endif

namespace Onyx::Detail
{
class WindowLayer;

template <typename Base, std::derived_from<Base> Derived, typename... LayerArgs>
Derived *CreateLayer(LayerArgs &&...args)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Derived *layer = tier->Allocate<Derived>();
    Base *base = TKit::Construct(layer, std::forward<LayerArgs>(args)...);
    base->m_Size = sizeof(Derived);
    return layer;
}
} // namespace Onyx::Detail

namespace Onyx
{
struct Event;

struct DeltaTime
{
    TKit::Timespan Target{};
    TKit::Timespan Measured{};
};

struct ExecutionInfo
{
    VKit::Queue *Queue;
    VkCommandBuffer CommandBuffer;
};

using WindowLayerFlags = u8;
enum WindowLayerFlagBit : WindowLayerFlags
{
    WindowLayerFlag_RequestCloseWindow = 1 << 1,
#ifdef ONYX_ENABLE_IMGUI
    WindowLayerFlag_ImGuiEnabled = 1 << 2,
    WindowLayerFlag_RequestEnableImGui = 1 << 4,
    WindowLayerFlag_RequestDisableImGui = 1 << 5,
#endif
};

class ApplicationLayer;
class WindowLayer
{
    TKIT_NON_COPYABLE(WindowLayer)
  public:
    WindowLayer(ApplicationLayer *appLayer, Window *window, TKit::Timespan targetDeltaTime);
    WindowLayer(ApplicationLayer *appLayer, Window *window);

    virtual ~WindowLayer()
    {
#ifdef ONYX_ENABLE_IMGUI
        if (checkFlags(WindowLayerFlag_ImGuiEnabled))
            shutdownImGui();
#endif
    }
    virtual Result<Renderer::RenderSubmitInfo> OnRender(const ExecutionInfo &info);

    virtual void OnEvent(const Event &)
    {
    }

    template <std::derived_from<WindowLayer> T = WindowLayer, typename... LayerArgs>
    void RequestReplaceLayer(LayerArgs &&...args)
    {
        m_Replacement = [=](ApplicationLayer *appLayer, Window *window) {
            return Detail::CreateLayer<WindowLayer, T>(appLayer, window, std::forward<LayerArgs>(args)...);
        };
    }

    void RequestCloseWindow()
    {
        m_Flags |= WindowLayerFlag_RequestCloseWindow;
    }

#ifdef ONYX_ENABLE_IMGUI
    void RequestEnableImGui(const i32 configFlags)
    {
        TKIT_ASSERT(!checkFlags(WindowLayerFlag_ImGuiEnabled),
                    "[ONYX][WIN-LAYER] Imgui is already enabled. To reload imgui, use RequestReloadImGui()");
        m_Flags |= WindowLayerFlag_RequestEnableImGui;
        m_ImGuiConfigFlags = configFlags;
    }
    void RequestDisableImGui()
    {
        TKIT_ASSERT(checkFlags(WindowLayerFlag_ImGuiEnabled), "[ONYX][WIN-LAYER] Imgui is already disabled");
        m_Flags |= WindowLayerFlag_RequestDisableImGui;
    }
    void RequestReloadImGui(const i32 configFlags)
    {
        TKIT_ASSERT(checkFlags(WindowLayerFlag_ImGuiEnabled),
                    "[ONYX][WIN-LAYER] ImGui is not enabled. Enable it first with RequestEnableImGui()");
        m_Flags |= WindowLayerFlag_RequestDisableImGui | WindowLayerFlag_RequestEnableImGui;
        m_ImGuiConfigFlags = configFlags;
    }
    bool DeltaTimeEditor(const EditorFlags flags = 0)
    {
        return Onyx::DeltaTimeEditor(m_Delta, m_DeltaInfo, m_Window, flags);
    }
#endif

  protected:
    ONYX_NO_DISCARD Result<Renderer::RenderSubmitInfo> Render(const ExecutionInfo &info);

    ApplicationLayer *m_AppLayer;
    Window *m_Window;

  private:
#ifdef ONYX_ENABLE_IMGUI
    void initializeImGui();
    void shutdownImGui();
#endif

    bool isDue() const
    {
        return m_Clock.GetElapsed() >= m_Delta.Target;
    }
    void markTick()
    {
        m_Delta.Measured = m_Clock.Restart();
    }
    bool checkFlags(const WindowLayerFlags flags) const
    {
        return m_Flags & flags;
    }
    void setFlags(const WindowLayerFlags flags)
    {
        m_Flags |= flags;
    }
    void clearFlags(const WindowLayerFlags flags)
    {
        m_Flags &= ~flags;
    }

    TKit::Clock m_Clock{};
    DeltaTime m_Delta{};
#ifdef ONYX_ENABLE_IMGUI
    ImGuiContext *m_ImGuiContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlotContext *m_ImPlotContext = nullptr;
#    endif
    i32 m_ImGuiConfigFlags = 0;
    Theme *m_Theme = nullptr;
    DeltaInfo m_DeltaInfo{};
#endif

    std::function<WindowLayer *(ApplicationLayer *, Window *)> m_Replacement = nullptr;
    u32 m_Size = 0;

    WindowLayerFlags m_Flags = 0;

    friend class Application;
    friend class ApplicationLayer;
    template <typename Base, std::derived_from<Base> Derived, typename... LayerArgs>
    friend Derived *Detail::CreateLayer(LayerArgs &&...args);
};

struct OpenWindowRequest
{
    Window::Specs Specs{};
    std::function<WindowLayer *(ApplicationLayer *, Window *)> LayerCreation = nullptr;
};

using ApplicationLayerFlags = u8;
enum ApplicationLayerFlagBit : ApplicationLayerFlags
{
    ApplicationLayerFlag_RequestQuitApplication = 1 << 0,
};

class ApplicationLayer
{
    TKIT_NON_COPYABLE(ApplicationLayer)
  public:
    ApplicationLayer(const TKit::Timespan targetDeltaTime = ToDeltaTime(60))
    {
        m_UpdateDelta.Target = targetDeltaTime;
        m_TransferDelta.Target = targetDeltaTime;
    }

    virtual ~ApplicationLayer() = default;

    virtual void OnUpdate(const DeltaTime &)
    {
    }

    virtual Result<Renderer::TransferSubmitInfo> OnTransfer(const ExecutionInfo &info);

    template <std::derived_from<ApplicationLayer> T = ApplicationLayer, typename... LayerArgs>
    void RequestReplaceLayer(LayerArgs &&...args)
    {
        m_Replacement = [=]() { return Detail::CreateLayer<ApplicationLayer, T>(std::forward<LayerArgs>(args)...); };
    }

    void RequestQuitApplication()
    {
        m_Flags |= ApplicationLayerFlag_RequestQuitApplication;
    }

    template <std::derived_from<WindowLayer> T = WindowLayer, typename... LayerArgs>
    void RequestOpenWindow(const Window::Specs &specs, LayerArgs &&...args)
    {
        m_WindowRequests.Append(specs, [=](ApplicationLayer *appLayer, Window *window) {
            return Detail::CreateLayer<WindowLayer, T>(appLayer, window, std::forward<LayerArgs>(args)...);
        });
    }
    template <std::derived_from<WindowLayer> T, typename... LayerArgs> void RequestOpenWindow(LayerArgs &&...args)
    {
        const Window::Specs specs{};
        RequestOpenWindow<T>(specs, std::forward<LayerArgs>(args)...);
    }

#ifdef ONYX_ENABLE_IMGUI
    bool UpdateDeltaTimeEditor(const EditorFlags flags = 0)
    {
        return Onyx::DeltaTimeEditor(m_UpdateDelta, m_UpdateDeltaInfo, nullptr, flags);
    }
    bool TransferDeltaTimeEditor(const EditorFlags flags = 0)
    {
        return Onyx::DeltaTimeEditor(m_TransferDelta, m_TransferDeltaInfo, nullptr, flags);
    }
#endif

  protected:
    ONYX_NO_DISCARD Result<Renderer::TransferSubmitInfo> Transfer(const ExecutionInfo &info);

  private:
    static bool isDue(const TKit::Clock &clock, const DeltaTime &delta)
    {
        return clock.GetElapsed() >= delta.Target;
    }
    static void markTick(TKit::Clock &clock, DeltaTime &delta)
    {
        delta.Measured = clock.Restart();
    }
    bool isUpdateDue() const
    {
        return isDue(m_UpdateClock, m_UpdateDelta);
    }
    bool isTransferDue() const
    {
        return isDue(m_TransferClock, m_TransferDelta);
    }
    void markUpdateTick()
    {
        markTick(m_UpdateClock, m_UpdateDelta);
    }
    void markTransferTick()
    {
        markTick(m_TransferClock, m_TransferDelta);
    }
    bool checkFlags(const WindowLayerFlags flags) const
    {
        return m_Flags & flags;
    }
    void setFlags(const WindowLayerFlags flags)
    {
        m_Flags |= flags;
    }
    void clearFlags(const WindowLayerFlags flags)
    {
        m_Flags &= ~flags;
    }

    TKit::TierArray<OpenWindowRequest> m_WindowRequests{};

    TKit::Clock m_UpdateClock{};
    TKit::Clock m_TransferClock{};

    DeltaTime m_UpdateDelta{};
    DeltaTime m_TransferDelta{};

#ifdef ONYX_ENABLE_IMGUI
    DeltaInfo m_UpdateDeltaInfo{};
    DeltaInfo m_TransferDeltaInfo{};
#endif

    std::function<ApplicationLayer *()> m_Replacement = nullptr;
    u32 m_Size = 0;

    ApplicationLayerFlags m_Flags = 0;

    friend class Application;
    template <typename Base, std::derived_from<Base> Derived, typename... LayerArgs>
    friend Derived *Detail::CreateLayer(LayerArgs &&...args);
};

} // namespace Onyx
