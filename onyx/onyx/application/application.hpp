#pragma once

#include "onyx/application/layer.hpp"

namespace Onyx
{
class Application
{
    TKIT_NON_COPYABLE(Application)
  public:
    Application()
    {
        SetApplicationLayer();
    }

    ~Application();

    /**
     * @brief This method is in charge of processing and presenting the next tick for all windows.
     *
     * This method should be called in a loop until it returns false, which means that all windows have been closed.
     *
     * @param clock A clock that lets both the API and the user to keep track of the tick time.
     * @return Whether the application must keep running.
     */
    ONYX_NO_DISCARD Result<bool> NextTick(TKit::Clock &clock);
    ONYX_NO_DISCARD Result<> Run();

    template <std::derived_from<ApplicationLayer> T = ApplicationLayer, typename... LayerArgs>
    T *SetApplicationLayer(LayerArgs &&...args)
    {
        if (m_AppLayer)
            destroyAppLayer();
        T *layer = Detail::CreateLayer<ApplicationLayer, T>(&m_WindowLayers, std::forward<LayerArgs>(args)...);
        m_AppLayer = layer;
        updateWindowLayers();
        return layer;
    }
    template <std::derived_from<WindowLayer> T = WindowLayer, typename... LayerArgs>
    T *SetWindowLayer(Window *window, LayerArgs &&...args)
    {
        WindowLayer **wlayer = getLayerFromWindow(window);
        destroyWindowLayer(*wlayer);
        T *layer = Detail::CreateLayer<WindowLayer, T>(m_AppLayer, window, std::forward<LayerArgs>(args)...);
        *wlayer = layer;
        return layer;
    }

    template <std::derived_from<WindowLayer> T = WindowLayer, typename... LayerArgs>
    ONYX_NO_DISCARD Result<T *> OpenWindow(const WindowSpecs &specs, LayerArgs &&...args)
    {
        const auto result = Platform::CreateWindow(specs);
        TKIT_RETURN_ON_ERROR(result);
        Window *window = result.GetValue();
        T *layer = Detail::CreateLayer<WindowLayer, T>(m_AppLayer, window, std::forward<LayerArgs>(args)...);
        m_WindowLayers.Append(layer);
        return layer;
    }
    template <std::derived_from<WindowLayer> T = WindowLayer, typename... LayerArgs>
    ONYX_NO_DISCARD Result<T *> OpenWindow(LayerArgs &&...args)
    {
        const WindowSpecs specs{};
        return OpenWindow<T>(specs, std::forward<LayerArgs>(args)...);
    }

  private:
    void updateWindowLayers();
    WindowLayer **getLayerFromWindow(const Window *window);

    void destroyWindowLayer(WindowLayer *layer);
    void destroyAppLayer();

    ApplicationLayer *m_AppLayer = nullptr;
    TKit::TierArray<WindowLayer *> m_WindowLayers{};
};
} // namespace Onyx
