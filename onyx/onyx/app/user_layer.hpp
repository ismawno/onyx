#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/api.hpp"
#include "onyx/core/dimension.hpp"
#include "tkit/profiling/timespan.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
struct Event;

template <Dimension D> struct Transform;
template <Dimension D> struct MaterialData;
template <Dimension D> struct CameraControls;

struct DirectionalLight;
struct PointLight;

struct ScreenViewport;
struct ScreenScissor;

enum class Resolution : u32;

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
class ONYX_API UserLayer
{
  public:
    using Flags = u8;
    enum FlagBit : Flags
    {
        Flag_None = 0,
        Flag_DisplayHelp = 1 << 0,
    };

    virtual ~UserLayer() = default;

    /**
     * @brief Called every frame before the `OnFrameBegin()` method.
     *
     * This method is called outside the the frame loop, so you cannot issue any vulkan draw calls here. You can (and is
     * advised) to submit `RenderContext` draw calls from this callback, as it is cpu-side work only. You may also
     * submit `ImGui` or `ImPlot` calls.
     *
     * Its purpose is to update the user's state as they see fit. Doing so in other render callbacks is not recommended,
     * as some gpu operations can be performed at the same time `OnUpdate()` runs, but not at the same time the other
     * callbacks run.
     *
     * @note This variant of the method is not called in multi-window applications. Use the `OnUpdate(u32)` method
     * instead.
     *
     */
    virtual void OnUpdate()
    {
    }

    /**
     * @brief Called every frame before the `OnRenderBegin()` method.
     *
     * It may be used to issue `ImGui` or `ImPlot` calls or to record vulkan commands directly before the main scene
     * rendering.
     *
     * Take into account this method is not executed inside a `vkBeginRendering()`/`vkEndRendering()` pair call.
     *
     * @note This variant of the method is not called in multi-window applications. Use the `OnFrameBegin(u32, u32,
     * VkCommandBuffer)` method instead.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnFrameBegin(u32, VkCommandBuffer)
    {
    }

    /**
     * @brief Called every frame at the bottom of the frame loop.
     *
     * Its purpose is to contain direct vulkan draw calls that execute after the main scene rendering. The draw calls
     * must come from the Vulkan's API itself. It cannot be used to issue `ImGui` or `ImPlot` calls.
     *
     * Take into account this method is not executed inside a `vkBeginRendering()`/`vkEndRendering()` pair call.
     *
     * @note This variant of the method is not called in multi-window applications. Use the `OnFrameEnd(u32, u32,
     * VkCommandBuffer)` method instead.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnFrameEnd(u32, VkCommandBuffer)
    {
    }

    /**
     * @brief Called every frame before the `OnRenderEnd()` method.
     *
     * It is designed to submit direct vulkan commands before the main scene rendering. It is called in between a
     * `vkBeginRendering()`/`vkEndRendering()` pair call. It may also be used to issue `ImGui` or `ImPlot` calls.
     *
     * @note This variant of the method is not called in multi-window applications. Use the `OnRenderBegin(u32, u32,
     * VkCommandBuffer)` method instead.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnRenderBegin(u32, VkCommandBuffer)
    {
    }

    /**
     * @brief Called every frame before the `OnFrameEnd()` method.
     *
     * It is designed to submit direct vulkan commands after the main scene rendering. It is called in between a
     * `vkBeginRendering()`/`vkEndRendering()` pair call. It may also be used to issue `ImGui` or `ImPlot` calls.
     *
     * @note This variant of the method is not called in multi-window applications. Use the `OnFrameBegin(u32, u32,
     * VkCommandBuffer)` method instead.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnRenderEnd(u32, VkCommandBuffer)
    {
    }

    /**
     * @brief Called every frame before the `OnFrameBegin()` method.
     *
     * This method is called outside the the frame loop, so you cannot issue any vulkan draw calls here. You can (and is
     * advised) to submit onyx draw calls from this callback, as it is cpu-side work only. You may also submit `ImGui`
     * or `ImPlot` calls.
     *
     * Its purpose is to update the user's state as they see fit. Doing so in other render callbacks is not recommended,
     * as some gpu operations can be performed at the same time `OnUpdate()` runs, but not at the same time the other
     * callbacks run.
     *
     * @note This variant of the method is not called in single-window applications. Use the `OnUpdate()` method
     * instead.
     *
     * @param p_WindowIndex The index of the window this method will be called for.
     *
     */
    virtual void OnUpdate(u32)
    {
    }

    /**
     * @brief Called every frame before the `OnRenderBegin()` method.
     *
     * It may be used to record vulkan commands directly before the main scene rendering. It cannot be used to issue
     * `ImGui` or `ImPlot` calls.
     *
     * Take into account this method is not executed inside a `vkBeginRendering()`/`vkEndRendering()` pair call.
     *
     * @note This variant of the method is not called in single-window applications. Use the `OnFrameBegin(u32,
     * VkCommandBuffer)` method instead.
     *
     * @param p_WindowIndex The index of the window this method will be called for.
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnFrameBegin(u32, u32, VkCommandBuffer)
    {
    }

    /**
     * @brief Called every frame at the bottom of the frame loop.
     *
     * Its purpose is to contain direct vulkan draw calls that execute after the main scene rendering. The draw calls
     * must come from the Vulkan's API itself. It may also be used to issue `ImGui` or `ImPlot` calls.
     *
     * Take into account this method is not executed inside a `vkBeginRendering()`/`vkEndRendering()` pair call.
     *
     * @note This variant of the method is not called in single-window applications. Use the `OnFrameEnd(u32,
     * VkCommandBuffer)` method instead.
     *
     * @param p_WindowIndex The index of the window this method will be called for.
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnFrameEnd(u32, u32, VkCommandBuffer)
    {
    }

    /**
     * @brief Called every frame before the `OnRenderEnd()` method.
     *
     * It is designed to submit direct vulkan commands before the main scene rendering. It is called in between a
     * `vkBeginRendering()`/`vkEndRendering()` pair call. It cannot be used to issue `ImGui` or `ImPlot` calls.
     *
     * @note This variant of the method is not called in single-window applications. Use the `OnRenderBegin(u32,
     * VkCommandBuffer)` method instead.
     *
     * @param p_WindowIndex The index of the window this method will be called for.
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnRenderBegin(u32, u32, VkCommandBuffer)
    {
    }

    /**
     * @brief Called every frame before the `OnFrameEnd()` method.
     *
     * It is designed to submit direct vulkan commands after the main scene rendering. It is called in between a
     * `vkBeginRendering()`/`vkEndRendering()` pair call. It cannot be used to issue `ImGui` or `ImPlot` calls.
     *
     * @note This variant of the method is not called in single-window applications. Use the `OnFrameBegin(u32,
     * VkCommandBuffer)` method instead.
     *
     * @param p_WindowIndex The index of the window this method will be called for.
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The command buffer to issue draw calls to, if needed.
     *
     */
    virtual void OnRenderEnd(u32, u32, VkCommandBuffer)
    {
    }

#ifdef ONYX_ENABLE_IMGUI
    /**
     * @brief A specialized method used to issue `ImGui` draw calls in multi-window applications.
     *
     * As there is only a unique ImGui context per application (not per window), `ImGui` callbacks are always called
     * once per frame in the main thread, no matter how many active windows there are. The `ImGui` calls are linked to
     * the main window.
     *
     * @note This method is not called in single window applications. Use the any of the other render methods instead.
     *
     */
    virtual void OnImGuiRender()
    {
    }
#endif

    /**
     * @brief Called for every event that is processed by the application.
     *
     * @note This method is not called in multi-window applications. Use the `OnEvent(u32)` method instead.
     *
     */
    virtual void OnEvent(const Event &)
    {
    }

    /**
     * @brief Called for every event that is processed by the application, for every window.
     *
     * @note This method is not called in single window applications. Use the `OnEvent()` method instead.
     *
     * @param p_WindowIndex The index of the window that is currently being processed.
     *
     */
    virtual void OnEvent(u32, const Event &)
    {
    }

#ifdef ONYX_ENABLE_IMGUI
    template <Dimension D> static bool TransformEditor(Transform<D> &p_Transform, Flags p_Flags = 0);
    template <Dimension D> static bool MaterialEditor(MaterialData<D> &p_Material, Flags p_Flags = 0);

    template <Dimension D> static void DisplayTransform(const Transform<D> &p_Transform, Flags p_Flags = 0);
    template <Dimension D> static void DisplayCameraControls(const CameraControls<D> &p_Controls = {});

    static void DisplayFrameTime(TKit::Timespan p_DeltaTime, Flags p_Flags = 0);

    static bool DirectionalLightEditor(DirectionalLight &p_Light, Flags p_Flags = 0);
    static bool PointLightEditor(PointLight &p_Light, Flags p_Flags = 0);

    static bool ResolutionEditor(const char *p_Name, Resolution &p_Res, Flags p_Flags = 0);
    static bool PresentModeEditor(Window *p_Window, Flags p_Flags = 0);

    static bool ViewportEditor(ScreenViewport &p_Viewport, Flags p_Flags = 0);
    static bool ScissorEditor(ScreenScissor &p_Scissor, Flags p_Flags = 0);

    static void HelpMarker(const char *p_Description, const char *p_Icon = "(?)");
    static void HelpMarkerSameLine(const char *p_Description, const char *p_Icon = "(?)");
#endif
};

} // namespace Onyx
