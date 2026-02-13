#pragma once

#include "onyx/rendering/renderer.hpp"
#include "onyx/platform/window.hpp"
#ifdef ONYX_ENABLE_IMGUI
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
    DeltaTime DeltaTime;
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

struct WindowLayerSpecs
{
    WindowLayerFlags Flags = 0;
#ifdef ONYX_ENABLE_IMGUI
    i32 ImGuiConfigFlags = 0;
#endif
};

class ApplicationLayer;
class WindowLayer
{
    TKIT_NON_COPYABLE(WindowLayer)
  public:
    WindowLayer(ApplicationLayer *appLayer, Window *window, TKit::Timespan targetDeltaTime,
                const WindowLayerSpecs &specs = {});
    WindowLayer(ApplicationLayer *appLayer, Window *window, const WindowLayerSpecs &specs = {});

    virtual ~WindowLayer()
    {
#ifdef ONYX_ENABLE_IMGUI
        if (checkFlags(WindowLayerFlag_ImGuiEnabled))
            shutdownImGui();
#endif
    }

    virtual void OnRender(const DeltaTime &)
    {
    }
    virtual Result<Renderer::RenderSubmitInfo> OnRender(const ExecutionInfo &info);

    virtual void OnEvent(const Event &)
    {
    }

    template <std::derived_from<WindowLayer> T = WindowLayer, typename... LayerArgs>
    void RequestReplaceLayer(LayerArgs... args)
    {
        m_Replacement = [=](ApplicationLayer *appLayer, Window *window) {
            return Detail::CreateLayer<WindowLayer, T>(appLayer, window, args...);
        };
    }
    template <std::derived_from<WindowLayer> T = WindowLayer, std::invocable<T *, Window *> F, typename... LayerArgs>
    void RequestReplaceLayer(F fun, LayerArgs... args)
    {
        m_Replacement = [=](ApplicationLayer *appLayer, Window *window) {
            T *layer = Detail::CreateLayer<WindowLayer, T>(appLayer, window, args...);
            fun(layer, window);
            return layer;
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
    i32 GetImGuiConfigFlags() const
    {
        return m_ImGuiConfigFlags;
    }
#endif

    ApplicationLayer *GetApplicationLayer() const
    {
        return m_AppLayer;
    }
    Window *GetWindow() const
    {
        return m_Window;
    }

  protected:
    ONYX_NO_DISCARD Result<Renderer::RenderSubmitInfo> Render(const ExecutionInfo &info);

  private:
#ifdef ONYX_ENABLE_IMGUI
    ONYX_NO_DISCARD Result<> initializeImGui();
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

    ApplicationLayer *m_AppLayer;
    Window *m_Window;

    TKit::Clock m_Clock{};
    DeltaTime m_Delta{};
#ifdef ONYX_ENABLE_IMGUI
    ImGuiContext *m_ImGuiContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlotContext *m_ImPlotContext = nullptr;
#    endif
    i32 m_ImGuiConfigFlags = 0;
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
    WindowSpecs Specs{};
    std::function<WindowLayer *(ApplicationLayer *, Window *)> LayerCreation = nullptr;
};

using ApplicationLayerFlags = u8;
enum ApplicationLayerFlagBit : ApplicationLayerFlags
{
    ApplicationLayerFlag_RequestQuitApplication = 1 << 0,
};

using WindowLayers = TKit::TierArray<WindowLayer *>;
struct ApplicationLayerSpecs
{
    TKit::Timespan TargetUpdateDeltaTime = ToDeltaTime(60);
    TKit::Timespan TargetTransferDeltaTime = ToDeltaTime(60);
};

class ApplicationLayer
{
    TKIT_NON_COPYABLE(ApplicationLayer)
  public:
    ApplicationLayer(const WindowLayers *layers, const ApplicationLayerSpecs &specs = {}) : m_WindowLayers(layers)
    {
        m_UpdateDelta.Target = specs.TargetUpdateDeltaTime;
        m_TransferDelta.Target = specs.TargetTransferDeltaTime;
    }

    virtual ~ApplicationLayer() = default;

    virtual void OnUpdate(const DeltaTime &)
    {
    }

    virtual void OnTransfer(const DeltaTime &)
    {
    }
    virtual Result<Renderer::TransferSubmitInfo> OnTransfer(const ExecutionInfo &info);

    template <std::derived_from<ApplicationLayer> T = ApplicationLayer, typename... LayerArgs>
    void RequestReplaceLayer(LayerArgs... args)
    {
        m_Replacement = [=](const WindowLayers *layers) {
            return Detail::CreateLayer<ApplicationLayer, T>(layers, args...);
        };
    }
    template <std::derived_from<ApplicationLayer> T = ApplicationLayer, std::invocable<T *> F, typename... LayerArgs>
    void RequestReplaceLayer(F fun, LayerArgs... args)
    {
        m_Replacement = [=](const WindowLayers *layers) {
            T *layer = Detail::CreateLayer<ApplicationLayer, T>(layers, args...);
            fun(layer);
            return layer;
        };
    }

    void RequestQuitApplication()
    {
        m_Flags |= ApplicationLayerFlag_RequestQuitApplication;
    }

    template <std::derived_from<WindowLayer> T = WindowLayer, typename... LayerArgs>
    void RequestOpenWindow(const WindowSpecs &specs, LayerArgs... args)
    {
        m_WindowRequests.Append(specs, [=](ApplicationLayer *appLayer, Window *window) {
            return Detail::CreateLayer<WindowLayer, T>(appLayer, window, args...);
        });
    }
    template <std::derived_from<WindowLayer> T = WindowLayer, std::invocable<T *, Window *> F, typename... LayerArgs>
    void RequestOpenWindow(F fun, const WindowSpecs &specs, LayerArgs... args)
    {
        m_WindowRequests.Append(specs, [=](ApplicationLayer *appLayer, Window *window) {
            T *layer = Detail::CreateLayer<WindowLayer, T>(appLayer, window, args...);
            fun(layer, window);
            return layer;
        });
    }

    template <std::derived_from<WindowLayer> T = WindowLayer, typename... LayerArgs>
    void RequestOpenWindow(LayerArgs... args)
    {
        const WindowSpecs specs{};
        RequestOpenWindow<T>(specs, args...);
    }
    template <std::derived_from<WindowLayer> T = WindowLayer, std::invocable<T *, Window *> F, typename... LayerArgs>
    void RequestOpenWindow(F fun, LayerArgs... args)
    {
        const WindowSpecs specs{};
        RequestOpenWindow<T>(fun, specs, args...);
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

    const WindowLayers *GetWindowLayers() const
    {
        return m_WindowLayers;
    }
    TKit::Timespan GetApplicationDeltaTime() const
    {
        return m_ApplicationDeltaTime;
    }

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

    ApplicationLayer *m_AppLayer;
    Window *m_Window;

    TKit::TierArray<OpenWindowRequest> m_WindowRequests{};

    TKit::Clock m_UpdateClock{};
    TKit::Clock m_TransferClock{};

    DeltaTime m_UpdateDelta{};
    DeltaTime m_TransferDelta{};

#ifdef ONYX_ENABLE_IMGUI
    DeltaInfo m_UpdateDeltaInfo{};
    DeltaInfo m_TransferDeltaInfo{};
#endif

    const WindowLayers *m_WindowLayers;
    TKit::Timespan m_ApplicationDeltaTime{};

    std::function<ApplicationLayer *(const WindowLayers *)> m_Replacement = nullptr;
    u32 m_Size = 0;

    ApplicationLayerFlags m_Flags = 0;

    friend class Application;
    template <typename Base, std::derived_from<Base> Derived, typename... LayerArgs>
    friend Derived *Detail::CreateLayer(LayerArgs &&...args);
};

} // namespace Onyx
