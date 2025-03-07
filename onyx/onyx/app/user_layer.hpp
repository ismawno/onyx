#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/dimension.hpp"
#include "tkit/utils/non_copyable.hpp"
#include "tkit/memory/ptr.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/profiling/timespan.hpp"
#include <concepts>
#include <vulkan/vulkan.h>

namespace Onyx
{
struct Event;

template <Dimension D> struct Transform;
template <Dimension D> struct MaterialData;
template <Dimension D> struct CameraMovementControls;

struct DirectionalLight;
struct PointLight;

class Window;

/**
 * @brief A base class that allows users to inject their own code into the application's lifecycle with different
 * callbacks.
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
    using Flags = u8;
    enum FlagBit : Flags
    {
        Flag_DisplayHelp = 1 << 0,
    };

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
     * Behaves the same as the `OnUpdate()` method, but is called in multi-window applications.
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
     * Behaves the same as the `OnRender()` method, but is called in multi-window applications.
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
     * Behaves the same as the `OnLateRender()` method, but is called in multi-window applications.
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
     * As there is only a unique ImGui context per application (not per window), `ImGui` callbacks are always called
     * once per frame in the main thread, no matter how many active windows there are. The `ImGui` calls are linked to
     * the main window.
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

    template <Dimension D> static void TransformEditor(Transform<D> &p_Transform, Flags p_Flags = 0) noexcept;
    template <Dimension D> static void MaterialEditor(MaterialData<D> &p_Material, Flags p_Flags = 0) noexcept;

    template <Dimension D> static void DisplayTransform(const Transform<D> &p_Transform, Flags p_Flags = 0) noexcept;
    template <Dimension D>
    static void DisplayCameraMovementControls(const CameraMovementControls<D> &p_Controls = {}) noexcept;

    static void DisplayFrameTime(TKit::Timespan p_DeltaTime, Flags p_Flags = 0) noexcept;

    static void DirectionalLightEditor(DirectionalLight &p_Light, Flags p_Flags = 0) noexcept;
    static void PointLightEditor(PointLight &p_Light, Flags p_Flags = 0) noexcept;

    static void PresentModeEditor(Window *p_Window, Flags p_Flags = 0) noexcept;

    static void HelpMarker(const char *p_Description, const char *p_Icon = "(?)") noexcept;
    static void HelpMarkerSameLine(const char *p_Description, const char *p_Icon = "(?)") noexcept;
};

} // namespace Onyx