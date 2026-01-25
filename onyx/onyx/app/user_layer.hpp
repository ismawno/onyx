#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/dimension.hpp"
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

struct FrameInfo;
struct DeltaTime;

class Application;
class Window;

using UserLayerFlags = u8;
enum UserLayerFlagBit : UserLayerFlags
{
    UserLayerFlag_DisplayHelp = 1 << 0,
};

/**
 * @brief A base class that allows users to inject their own code into the application's lifecycle with different
 * callbacks.
 *
 * Every user layer is assigned to a different window in an application, and there is only one `UserLayer` allowed per
 * window.
 *
 * The type of operations allowed in each callback is different based on where in the rendering pipeline they get
 * called. In addition, each of them can be called with different frequencies, depending on swap chain image
 * availability and performance.
 *
 */
class UserLayer
{
  public:
    UserLayer(Application *application, Window *window) : m_Application(application), m_Window(window)
    {
    }

    virtual ~UserLayer() = default;

    /**
     * @brief Called periodically outside of the rendering loop.
     *
     * Its purpose is to update the user's rendering-unrelated state. Its frequency is given by the target date time.
     *
     */
    virtual void OnUpdate(const DeltaTime &)
    {
    }

    /**
     * @brief Called everytime a frame begins.
     *
     * It is the only callback where render context calls are allowed. `ImGui` and `ImPlot` calls are also allowed until
     * `OnRenderEnd()` (exclusive).
     *
     * Take into account this method is not executed inside a `vkBeginRendering()`/`vkEndRendering()` pair call.
     *
     */
    virtual void OnFrameBegin(const DeltaTime &, const FrameInfo &)
    {
    }

    /**
     * @brief Called everytime rendering begins.
     *
     * It is designed to submit direct vulkan commands before the main scene rendering. It is called in between a
     * `vkBeginRendering()`/`vkEndRendering()` pair call. It may also be used to issue `ImGui` or `ImPlot` calls.
     *
     */
    virtual void OnRenderBegin(const DeltaTime &, const FrameInfo &)
    {
    }

    /**
     * @brief Called everytime rendering ends.
     *
     * It is designed to submit direct vulkan commands after the main scene rendering. It is called in between a
     * `vkBeginRendering()`/`vkEndRendering()` pair call. It may also be used to issue `ImGui` or `ImPlot` calls.
     *
     */
    virtual void OnRenderEnd(const DeltaTime &, const FrameInfo &)
    {
    }

    /**
     * @brief Called everytime a frame ends.
     *
     * Its purpose is to contain direct vulkan draw calls that execute after the main scene rendering. The draw calls
     * must come from the Vulkan's API itself. It cannot be used to issue `ImGui` or `ImPlot` calls.
     *
     * Take into account this method is not executed inside a `vkBeginRendering()`/`vkEndRendering()` pair call.
     *
     */
    virtual void OnFrameEnd(const DeltaTime &, const FrameInfo &)
    {
    }

    /**
     * @brief Called for every event that is processed by the application.
     *
     */
    virtual void OnEvent(const Event &)
    {
    }

#ifdef ONYX_ENABLE_IMGUI
    template <Dimension D> static bool TransformEditor(Transform<D> &transform, UserLayerFlags flags = 0);

    template <Dimension D> static void DisplayTransform(const Transform<D> &transform, UserLayerFlags flags = 0);
    template <Dimension D> static void DisplayCameraControls(const CameraControls<D> &controls = {});

    static bool DirectionalLightEditor(DirectionalLight &light, UserLayerFlags flags = 0);
    static bool PointLightEditor(PointLight &light, UserLayerFlags flags = 0);

    static bool PresentModeEditor(Window *window, UserLayerFlags flags = 0);

    static bool ViewportEditor(ScreenViewport &viewport, UserLayerFlags flags = 0);
    static bool ScissorEditor(ScreenScissor &scissor, UserLayerFlags flags = 0);

    static void HelpMarker(const char *description, const char *icon = "(?)");
    static void HelpMarkerSameLine(const char *description, const char *icon = "(?)");
#endif
  protected:
    Application *m_Application;
    Window *m_Window;
};

} // namespace Onyx
