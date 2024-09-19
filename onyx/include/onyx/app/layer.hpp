#pragma once

#include "onyx/core/alias.hpp"
#include "kit/core/non_copyable.hpp"
#include "kit/memory/ptr.hpp"
#include <concepts>

namespace ONYX
{
class IApplication;
class Window;
struct Event;
class Layer
{
  public:
    Layer(const char *p_Name) noexcept;
    virtual ~Layer() noexcept = default;

    virtual void OnStart() noexcept
    {
    }
    virtual void OnShutdown() noexcept
    {
    }

    virtual void OnUpdate(usize) noexcept
    {
    }
    virtual void OnRender(usize) noexcept
    {
    }
    virtual void OnImGuiRender() noexcept
    {
    }

    virtual bool OnEvent(usize, const Event &) noexcept
    {
        return false;
    }

    const char *GetName() const noexcept;

    const IApplication *GetApplication() const noexcept;
    IApplication *GetApplication() noexcept;

    bool Enabled = true;

  private:
    const char *m_Name = nullptr;
    IApplication *m_Application = nullptr;

    friend class LayerSystem;
};

class LayerSystem
{
    KIT_NON_COPYABLE(LayerSystem)
  public:
    LayerSystem(IApplication *p_Application) noexcept;

    void OnStart() noexcept;
    void OnShutdown() noexcept;

    // Window is also passed in update because it also acts as an identifier for the current window thread
    void OnUpdate(usize p_WindowIndex) noexcept;
    void OnRender(usize p_WindowIndex) noexcept;
    void OnImGuiRender() noexcept;

    void OnEvent(usize p_WindowIndex, const Event &p_Event) noexcept;

    template <std::derived_from<Layer> T, typename... LayerArgs> T *Push(LayerArgs &&...p_Args) noexcept
    {
        auto layer = KIT::Scope<T>::Create(std::forward<LayerArgs>(p_Args)...);
        layer->m_Application = m_Application;
        T *ptr = layer.Get();
        m_Layers.push_back(std::move(layer));
        return ptr;
    }

    template <std::derived_from<Layer> T = Layer> const T *GetLayer(const usize p_Index)
    {
        return static_cast<const T *>(m_Layers[p_Index].Get());
    }
    template <std::derived_from<Layer> T = Layer> const T *GetLayer(const std::string_view p_Name)
    {
        for (auto &layer : m_Layers)
            if (layer->GetName() == p_Name)
                return static_cast<const T *>(layer.Get());
        return nullptr;
    }

    template <std::derived_from<Layer> T = Layer> T *GetLayer(const usize p_Index)
    {
        return static_cast<T *>(m_Layers[p_Index].Get());
    }
    template <std::derived_from<Layer> T = Layer> T *GetLayer(const std::string_view p_Name)
    {
        for (auto &layer : m_Layers)
            if (layer->GetName() == p_Name)
                return static_cast<T *>(layer.Get());
        return nullptr;
    }

  private:
    IApplication *m_Application;
    DynamicArray<KIT::Scope<Layer>> m_Layers;
    friend class IApplication;
};

} // namespace ONYX