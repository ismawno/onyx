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
 * @brief A base class that allows users to inject their own code into the application's lifecycle with different
 * callbacks.
 *
 * When using a concurrent multi-window application, and using the callback variants `On...(u32)`, you have guaranteed
 * synchronized execution between calls that share the same window index, and multithreaded execution between calls that
 * do not share the same window index, with the exception of `OnEvent()` calls triggered by a `WindowOpened` or
 * `WindowClosed` event. When those happen, the `OnEvent()` method is always executed on the main thread, and all other
 * window tasks are paused. This means the user is free to update its state according to the window addition/removal.
 *
 * @note In multi-window applications, users must always react to a window opening/closing in the `OnEvent()` method
 * even when it was triggered by a direct `OpenWindow()` or `CloseWindow()` call. This is because the window
 * addition/removal mechanism is deferred until it can be performed safely. In that moment, the `OnEvent()` method is
 * finally called.
 *
 */
class UserLayer
{
  public:
    virtual ~UserLayer() noexcept = default;

    /**
     * @brief Called when the `Startup()` method of the application is called.
     *
     * If `Startup()` was already called, this method will never be called. If you wish a callback for when the layer is
     * pushed into the application, you should use your layer's constructor. This method is called serially
     * in all cases.
     *
     */
    virtual void OnStart() noexcept
    {
    }

    /**
     * @brief Called when the `Shutdown()` method of the application is called.
     *
     * If you wish a callback for when the layer is popped from the application, you should use your layer's destructor.
     *
     * It is not possible to reference any window at this point.
     *
     */
    virtual void OnShutdown() noexcept
    {
    }

    /**
     * @brief Called every frame before the `OnRender()` method.
     *
     * This method is called outside the the render loop, so you cannot issue any onyx draw calls here. Its purpose is
     * to update the user's state as they see fit. Doing so in `OnRender()` callbacks is not recommended, as some
     * rendering operations can be performed at the same time `OnUpdate()` runs, but not at the same time `OnRender()`
     * runs.
     *
     * @note This variant of the method is not called in multi-window applications. Use the OnUpdate(u32) method
     * instead.
     *
     */
    virtual void OnUpdate() noexcept
    {
    }

    /**
     * @brief Called every frame after the `OnUpdate()` method.
     *
     * Its purpose is to contain all of the user draw calls, as it is called inside the render loop. Having update
     * code in this method is not recommended. If you need to update some state, you should do so in the `OnUpdate()`
     * method.
     *
     * This method can (and must) be used to issue ImGui draw calls.
     *
     * @note This variant of the method is not called in multi-window applications. Use the OnRender(u32,
     * VkCommandBuffer) method instead.
     *
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnRender(VkCommandBuffer) noexcept
    {
    }

    /**
     * @brief Called every frame after the `OnUpdate()` and `OnRender()` methods.
     *
     * Its purpose is to contain direct user draw calls that execute after the main scene pass. The draw calls must come
     * from the Vulkan's API itself. Having update code in this method is not recommended. If you need to update some
     * state, you should do so in the `OnUpdate()` method.
     *
     * This method can (and must) be used to issue ImGui draw calls.
     *
     * @note This variant of the method is not called in multi-window applications. Use the OnRender(u32,
     * VkCommandBuffer) method instead.
     *
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnLateRender(VkCommandBuffer) noexcept
    {
    }

    /**
     * @brief Called every frame before the `OnRender(u32)` method.
     *
     * Behaves the same as the `OnUpdate()` method, but is called in multi-window applications. In concurrent mode, this
     * method is called simultaneously for all windows.
     *
     * @note This method is not called in single window applications. Use the `OnUpdate()` method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     *
     */
    virtual void OnUpdate(u32) noexcept
    {
    }

    /**
     * @brief Called every frame after the `OnUpdate(u32)` method.
     *
     * Behaves the same as the `OnRender()` method, but is called in multi-window applications. In concurrent mode, this
     * method is called simultaneously for all windows.
     *
     * This method cannot be used to issue ImGui draw calls. Use `OnImGuiRender()` for that.
     *
     * @note This method is not called in single window applications. Use the `OnRender()` method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnRender(u32, VkCommandBuffer) noexcept
    {
    }

    /**
     * @brief Called every frame after the `OnUpdate(u32)` and `OnRender(u32, VkCommandBuffer)` methods.
     *
     * Behaves the same as the `OnLateRender()` method, but is called in multi-window applications. In concurrent mode,
     * this method is called simultaneously for all windows.
     *
     * This method cannot be used to issue ImGui draw calls. Use `OnImGuiRender()` for that.
     *
     * @note This method is not called in single window applications. Use the `OnLateRender()` method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnLateRender(u32, VkCommandBuffer) noexcept
    {
    }

    /**
     * @brief A specialized method used to issue `ImGui` draw calls in multi-window applications.
     *
     * As there is only a unique ImGui context per application (not per window), ImGui callbacks are always called once
     * per frame in the main thread, no matter how many active windows there are. This behaviour can be problematic in
     * the case of concurrent applications, as the user may want to display data from windows that are being processed
     * in other threads. Synchronization between calls to this method and other `On...(u32)` methods is not guaranteed,
     * so the user must provide their own.
     *
     * @note This method is not called in single window applications. Use the `OnRender()` method instead.
     *
     */
    virtual void OnImGuiRender() noexcept
    {
    }

    /**
     * @brief Called for every event that is processed by the application.
     *
     * @note This method is not called in multi-window applications. Use the `OnEvent(u32)` method instead.
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
     * @note This method is not called in single window applications. Use the `OnEvent()` method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     * @return Whether the event was handled by the layer.
     *
     */
    virtual bool OnEvent(u32, const Event &) noexcept
    {
        return false;
    }

    template <Dimension D> static void TransformEditor(Transform<D> &p_Transform, f32 p_DragSpeed = 0.03f) noexcept;
    template <Dimension D> static void MaterialEditor(MaterialData<D> &p_Material) noexcept;

    static void DirectionalLightEditor(DirectionalLight &p_Light) noexcept;
    static void PointLightEditor(PointLight &p_Light) noexcept;

    static void PresentModeEditor(Window *p_Window) noexcept;
};

} // namespace Onyx