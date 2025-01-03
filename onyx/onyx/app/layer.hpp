#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/dimension.hpp"
#include "tkit/core/non_copyable.hpp"
#include "tkit/memory/ptr.hpp"
#include "tkit/container/static_array.hpp"
#include <concepts>
#include <vulkan/vulkan.h>

namespace Onyx
{
struct Event;

template <Dimension D> struct Transform;
template <Dimension D> struct MaterialData;

struct DirectionalLight;
struct PointLight;

class Window;

/**
 * @brief A base class for all layers in the application, containing callback methods for the application's lifecycle.
 *
 * A layer is a way to separate the application into different parts, each with its own update and render methods.
 * Layers can be pushed into the LayerSystem, which will call their methods in the correct order. OnUpdate and OnRender
 * methods are processed in the order they were pushed into the LayerSystem, while OnEvent methods are processed in the
 * reverse order.
 *
 * Layers can be pushed into the LayerSystem after and before the first NextFrame() call only if the application you are
 * using is not a concurrent multi window application. In the concurrent case, layer callbacks are constantly executing
 * outside of the NextFrame() call, and so it is not possible to modify the container that holds all of the layers. Some
 * synchronization could be implemented, but that overhead would defeat the purpose of the concurrency.
 *
 * When using a concurrent multi window application, and using the callback variants On...(usize), you are guaranteed
 * synchronized execution between calls that share the same window index, and simultaneous execution between calls that
 * do not share the same window index, with the exception of OnEvent() calls regarding a WindowOpened or WindowClosed
 * event. When those happen, OnEvent is always executed on the main thread, and all other window tasks are stopped. This
 * means the user is free to update its state according to the window addition/removal.
 *
 */
class Layer
{
  public:
    Layer(const char *p_Name) noexcept;
    virtual ~Layer() noexcept = default;

    /**
     * @brief Called when the Startup() method of the application is called.
     *
     * If Startup() was already called, this method will never be called. If you wish a callback for when the layer is
     * pushed into the LayerSystem, you should use the OnPush() layer callback. This method is called serially
     * in all cases.
     *
     */
    virtual void OnStart() noexcept
    {
    }

    /**
     * @brief Called when the layer is pushed into the LayerSystem.
     *
     *
     */
    virtual void OnPush() noexcept
    {
    }

    /**
     * @brief Called when the Shutdown() method of the application is called.
     *
     * It is not possible to reference any window at this point.
     *
     */
    virtual void OnShutdown() noexcept
    {
    }

    /**
     * @brief Called when the layer is about to be removed from the LayerSystem.
     *
     */
    virtual void OnRemoval() noexcept
    {
    }

    /**
     * @brief Called every frame before the OnRender() method.
     *
     * This method is called outside the the render loop, so you cannot issue any onyx draw calls here. Its purpose is
     * to update the user's state as they see fit. Doing so in OnRender() callbacks is not recommended, as some
     * rendering operations can be performed at the same time OnUpdate() runs, but not at the same time OnRender() runs.
     *
     * @note This variant of the method is not called in multi window applications. Use the OnUpdate(usize) method
     * instead.
     *
     */
    virtual void OnUpdate() noexcept
    {
    }

    /**
     * @brief Called every frame after the OnUpdate() method.
     *
     * Its purpose is to contain all of the user draw calls, as it is called inside the render loop. Having update
     * code in this method is not recommended. If you need to update some state, you should do so in the OnUpdate()
     * method.
     *
     * This method can (and must) be used to issue ImGui draw calls.
     *
     * @note This variant of the method is not called in multi window applications. Use the OnRender(usize,
     * VkCommandBuffer) method instead.
     *
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnRender(VkCommandBuffer) noexcept
    {
    }

    /**
     * @brief Called every frame after the OnUpdate() and OnRender() methods.
     *
     * Its purpose is to contain direct user draw calls that execute after the main scene pass. The draw calls must come
     * from the Vulkan's API itself. Having update code in this method is not recommended. If you need to update some
     * state, you should do so in the OnUpdate() method.
     *
     * This method can (and must) be used to issue ImGui draw calls.
     *
     * @note This variant of the method is not called in multi window applications. Use the OnRender(usize,
     * VkCommandBuffer) method instead.
     *
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnLateRender(VkCommandBuffer) noexcept
    {
    }

    /**
     * @brief Called every frame before the OnRender(usize) method.
     *
     * Behaves the same as the OnUpdate() method, but is called in multi window applications. In concurrent mode, this
     * method is called simultaneously for all windows.
     *
     * @note This method is not called in single window applications. Use the OnUpdate() method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     *
     */
    virtual void OnUpdate(usize) noexcept
    {
    }

    /**
     * @brief Called every frame after the OnUpdate(usize) method.
     *
     * Behaves the same as the OnRender() method, but is called in multi window applications. In concurrent mode, this
     * method is called simultaneously for all windows.
     *
     * This method cannot be used to issue ImGui draw calls. Use OnImGuiRender() for that.
     *
     * @note This method is not called in single window applications. Use the OnRender() method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnRender(usize, VkCommandBuffer) noexcept
    {
    }

    /**
     * @brief Called every frame after the OnUpdate(usize) and OnRender(usize, VkCommandBuffer) methods.
     *
     * Behaves the same as the OnLateRender() method, but is called in multi window applications. In concurrent mode,
     * this method is called simultaneously for all windows.
     *
     * This method cannot be used to issue ImGui draw calls. Use OnImGuiRender() for that.
     *
     * @note This method is not called in single window applications. Use the OnLateRender() method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnLateRender(usize, VkCommandBuffer) noexcept
    {
    }

    /**
     * @brief A specialized method used to issue ImGui draw calls in multi window applications.
     *
     * As there is only a unique ImGui context per application (not per window), ImGui callbacks are always called once
     * per frame in the main thread, no matter how many active windows there are. This behaviour can be problematic in
     * the case of concurrent applications, as the user may want to display data from windows that are being processed
     * in other threads. Synchronization between calls to this method and other On..(usize) methods is not guaranteed,
     * so the user must provide their own.
     *
     * @note This method is not called in single window applications. Use the OnRender() method instead.
     *
     */
    virtual void OnImGuiRender() noexcept
    {
    }

    /**
     * @brief Called for every event that is processed by the application.
     *
     * This method is called in reverse order of the layers, meaning that the last layer pushed into the
     * LayerSystem will be the first to receive the event.
     *
     * @note This method is not called in multi window applications. Use the OnEvent(usize) method instead.
     *
     * @return Whether the event was handled by the layer.
     *
     */
    virtual bool OnEvent(const Event &) noexcept
    {
        return false;
    }

    /**
     * @brief Called for every event that is processed by the application, for every window.
     *
     * This method is called in reverse order of the layers, meaning that the last layer pushed into the
     * LayerSystem will be the first to receive the event.
     *
     * @note This method is not called in single window applications. Use the OnEvent() method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     * @return Whether the event was handled by the layer.
     *
     */
    virtual bool OnEvent(usize, const Event &) noexcept
    {
        return false;
    }

    const char *GetName() const noexcept;

    template <Dimension D> static void EditTransform(Transform<D> &p_Transform, f32 p_DragSpeed = 0.03f) noexcept;
    template <Dimension D> static void EditMaterial(MaterialData<D> &p_Material) noexcept;

    static void EditDirectionalLight(DirectionalLight &p_Light) noexcept;
    static void EditPointLight(PointLight &p_Light) noexcept;

    static void EditPresentMode(Window *p_Window) noexcept;

    bool Enabled = true;

  private:
    const char *m_Name = nullptr;
    bool m_RemoveFlag = false; // If true, the layer will be removed from the LayerSystem in the next frame

    friend class LayerSystem;
};

class LayerSystem
{
    TKIT_NON_COPYABLE(LayerSystem)
  public:
    LayerSystem() noexcept = default;

    /**
     * @brief Push a new layer into the LayerSystem.
     *
     * The layer object is constructed with the given arguments and pushed directly.
     *
     * @tparam T The type of the layer.
     * @tparam LayerArgs The types of the arguments to pass to the layer constructor.
     * @param p_Args The arguments to pass to the layer constructor.
     * @return Pointer to the layer object.
     */
    template <std::derived_from<Layer> T, typename... LayerArgs> T *Push(LayerArgs &&...p_Args) noexcept
    {
        auto layer = TKit::Scope<T>::Create(std::forward<LayerArgs>(p_Args)...);
        T *ptr = layer.Get();
        m_Layers.push_back(std::move(layer));
        m_Layers.back()->OnPush();
        return ptr;
    }

    /**
     * @brief Get a layer by index.
     *
     * @tparam T The type to cast the layer to.
     * @param p_Index The index of the layer.
     */
    template <std::derived_from<Layer> T = Layer> const T *Get(const usize p_Index)
    {
        return static_cast<const T *>(m_Layers[p_Index].Get());
    }
    /**
     * @brief Get a layer by index.
     *
     * @tparam T The type to cast the layer to.
     * @param p_Index The index of the layer.
     */
    template <std::derived_from<Layer> T = Layer> T *Get(const usize p_Index)
    {
        return static_cast<T *>(m_Layers[p_Index].Get());
    }

    /**
     * @brief Get a layer by name.
     *
     * @tparam T The type to cast the layer to.
     * @param p_Name The name of the layer.
     */
    template <std::derived_from<Layer> T = Layer> const T *Get(const std::string_view p_Name)
    {
        for (auto &layer : m_Layers)
            if (layer->GetName() == p_Name)
                return static_cast<const T *>(layer.Get());
        return nullptr;
    }

    /**
     * @brief Get a layer by name.
     *
     * @tparam T The type to cast the layer to.
     * @param p_Name The name of the layer.
     */
    template <std::derived_from<Layer> T = Layer> T *Get(const std::string_view p_Name)
    {
        for (auto &layer : m_Layers)
            if (layer->GetName() == p_Name)
                return static_cast<T *>(layer.Get());
        return nullptr;
    }

    /**
     * @brief Immediately remove a layer by index.
     *
     * Calling this method will immediately remove the layer from the LayerSystem. This method is not recommended to be
     * called during the LayerSystem's update/render loop, as it may cause undefined behaviour or unexpected bugs.
     *
     * @param p_Index The index of the layer.
     */
    void Remove(const usize p_Index) noexcept;

    /**
     * @brief Immediately remove a layer by name.
     *
     * Calling this method will immediately remove the layer from the LayerSystem. This method is not recommended to be
     * called during the LayerSystem's update/render loop, as it may cause undefined behaviour or unexpected bugs.
     *
     * @param p_Name The name of the layer.
     */
    void Remove(std::string_view p_Name) noexcept;

    /**
     * @brief Immediately remove a layer by pointer.
     *
     * Calling this method will immediately remove the layer from the LayerSystem. This method is not recommended to be
     * called during the LayerSystem's update/render loop, as it may cause undefined behaviour or unexpected bugs.
     *
     * @param p_Layer The pointer to the layer.
     */
    void Remove(const Layer *p_Layer) noexcept;

    /**
     * @brief Flag a layer to be removed in the next frame.
     *
     * The layer will be removed from the LayerSystem in the next frame, after all of the layer callbacks have been
     * processed. This method is safe to be called during the LayerSystem's update/render loop.
     *
     * @param p_Index The index of the layer.
     */
    void FlagRemove(const usize p_Index) noexcept;

    /**
     * @brief Flag a layer to be removed in the next frame.
     *
     * The layer will be removed from the LayerSystem in the next frame, after all of the layer callbacks have been
     * processed. This method is safe to be called during the LayerSystem's update/render loop.
     *
     * @param p_Name The name of the layer.
     */
    void FlagRemove(std::string_view p_Name) noexcept;

    /**
     * @brief Flag a layer to be removed in the next frame.
     *
     * The layer will be removed from the LayerSystem in the next frame, after all of the layer callbacks have been
     * processed. This method is safe to be called during the LayerSystem's update/render loop.
     *
     * @param p_Layer The pointer to the layer.
     */
    void FlagRemove(const Layer *p_Layer) noexcept;

    // Made public bc its a hassle to befriend every app class. But note the camelcase: not supposed to be accessed by
    // user
    void onStart() noexcept;
    void onShutdown() noexcept;

    void onUpdate() noexcept;
    void onRender(VkCommandBuffer p_CommandBuffer) noexcept;
    void onLateRender(VkCommandBuffer p_CommandBuffer) noexcept;

    // Window is also passed in update because it also acts as an identifier for the current window thread
    void onUpdate(usize p_WindowIndex) noexcept;
    void onRender(usize p_WindowIndex, VkCommandBuffer p_CommandBuffer) noexcept;
    void onLateRender(usize p_WindowIndex, VkCommandBuffer p_CommandBuffer) noexcept;

    // To be used only in multi window apps (in single window, OnRender does fine)
    void onImGuiRender() noexcept;

    void onEvent(const Event &p_Event) noexcept;
    void onEvent(usize p_WindowIndex, const Event &p_Event) noexcept;

    void removeFlaggedLayers() noexcept;

  private:
    TKit::StaticArray16<TKit::Scope<Layer>> m_Layers;
    bool m_MustRemoveLayers = false;
};

} // namespace Onyx