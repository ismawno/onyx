#pragma once

#include "onyx/data/state.hpp"
#include "onyx/app/input.hpp"
#include "onyx/property/transform.hpp"
#include "tkit/profiling/timespan.hpp"
#include "tkit/utils/non_copyable.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/serialization/yaml/serialize.hpp"

namespace Onyx
{
class Window;
/**
 * @brief The `ProjectionViewData` struct is a simple struct that holds the view and projection matrices.
 *
 * 2D shapes only need a view matrix, as the projection matrix is always an orthographic projection matrix, and so
 * `ProjectionView` is just the view in the 2D case, but is kept with that name to keep both cases as similar as
 * possible. The view can also include scaling.
 *
 * In 2D, the projection view matrix is the "raw" inverse of the view's transform. Then, just before sending the data to
 * the gpu as a `fmat4`, the renderer applies the extrinsic coordinate system.
 *
 * In 3D, the projection view matrix is the projection matrix multiplied by the view matrix. As the view matrix is
 * already a `fmat4`, the renderer can directly apply the extrinsic coordinate system.
 *
 * @tparam D The dimension (`D2` or `D3`).
 */
template <Dimension D> struct ProjectionViewData;

template <> struct ONYX_API ProjectionViewData<D2>
{
    TKIT_REFLECT_DECLARE(ProjectionViewData)
    TKIT_YAML_SERIALIZE_DECLARE(ProjectionViewData)
    Transform<D2> View{};
    fmat3 ProjectionView{1.f};
};
template <> struct ONYX_API ProjectionViewData<D3>
{
    TKIT_REFLECT_DECLARE(ProjectionViewData)
    TKIT_YAML_SERIALIZE_DECLARE(ProjectionViewData)
    Transform<D3> View{};
    fmat4 Projection{1.f};
    fmat4 ProjectionView{1.f};
};

/**
 * @brief The `ScreenViewport` struct holds screen viewport dimensions.
 *
 * It is represented as an axis-aligned rectangle with the `Min` and `Max` coordinates ranging from -1 to 1. The
 * `DepthBounds` are normalized, ranging from 0 to 1. The default values are set to cover the entire screen.
 *
 */
struct ONYX_API ScreenViewport
{
    TKIT_REFLECT_DECLARE(ScreenViewport)
    TKIT_YAML_SERIALIZE_DECLARE(ScreenViewport)
    fvec2 Min{-1.f};
    fvec2 Max{1.f};
    fvec2 DepthBounds{0.f, 1.f};

    /**
     * @brief Convert the viewport to a Vulkan viewport given a Vulkan extent.
     *
     * @param p_Extent The Vulkan extent to use for the conversion.
     * @return The Vulkan viewport.
     */
    VkViewport AsVulkanViewport(const VkExtent2D &p_Extent) const;
};

/**
 * @brief The `ScreenScissor` struct holds screen scissor dimensions relative to a viewport.
 *
 * It is represented as an axis-aligned rectangle with the `Min` and `Max` coordinates ranging from -1 to 1. The default
 * values are set to cover the entire screen.
 *
 */
struct ONYX_API ScreenScissor
{
    TKIT_REFLECT_DECLARE(ScreenScissor)
    TKIT_YAML_SERIALIZE_DECLARE(ScreenScissor)
    fvec2 Min{-1.f};
    fvec2 Max{1.f};

    /**
     * @brief Convert the scissor to a Vulkan scissor given a Vulkan extent and a viewport.
     *
     * The scissor will be adapted so its coordinates are relative to the viewport.
     *
     * @param p_Extent The Vulkan extent to use for the conversion.
     * @return The Vulkan scissor.
     */
    VkRect2D AsVulkanScissor(const VkExtent2D &p_Extent, const ScreenViewport &p_Viewport) const;
};

template <Dimension D> struct CameraControls;

template <> struct ONYX_API CameraControls<D2>
{
    f32 TranslationStep = 1.f / 60.f;
    f32 RotationStep = 1.f / 60.f;
    Input::Key Up = Input::Key::W;
    Input::Key Down = Input::Key::S;
    Input::Key Left = Input::Key::A;
    Input::Key Right = Input::Key::D;
    Input::Key RotateLeft = Input::Key::Q;
    Input::Key RotateRight = Input::Key::E;
};
template <> struct ONYX_API CameraControls<D3>
{
    f32 TranslationStep = 1.f / 60.f;
    f32 RotationStep = 1.f / 60.f;
    Input::Key Forward = Input::Key::W;
    Input::Key Backward = Input::Key::S;
    Input::Key Left = Input::Key::A;
    Input::Key Right = Input::Key::D;
    Input::Key Up = Input::Key::Space;
    Input::Key Down = Input::Key::LeftControl;
    Input::Key RotateLeft = Input::Key::Q;
    Input::Key RotateRight = Input::Key::E;
    Input::Key ToggleLookAround = Input::Key::LeftShift;
};

} // namespace Onyx

namespace Onyx::Detail
{

template <Dimension D> class ICamera
{
    TKIT_NON_COPYABLE(ICamera);

  public:
    ICamera() = default;

    /**
     * @brief Compute the position of a point in the camera's rendering context from screen to viewport coordinates.
     *
     * @param p_ScreenPos The position to convert. Should be in the range [-1, 1] for points contained in the screen,
     * with the y axis pointing upwards.
     * @return The position in viewport coordinates.
     */
    fvec2 ScreenToViewport(const fvec2 &p_ScreenPos) const;

    /**
     * @brief Compute the position of a point in the camera's rendering context from viewport to world coordinates.
     *
     * @param p_ViewportPos The position to convert. Should be in the range [-1, 1] for points contained in the
     * viewport, with the y axis pointing upwards. If in 3D, the z axis must be between [0, 1], mapping the near and far
     * planes or the orthographic size.
     *
     * @param p_Axes an optional axes parameter to compatibilize with render contexts.
     *
     * @return The position in world coordinates.
     */
    fvec<D> ViewportToWorld(fvec<D> p_ViewportPos, const fmat<D> *p_Axes = nullptr) const;

    /**
     * @brief Compute the position of a point in the camera's rendering context from world to viewport coordinates.
     *
     * @param p_WorldPos The position to convert.
     * @param p_Axes an optional axes parameter to compatibilize with render contexts.
     *
     * @return The position in viewport coordinates. Should be in the range [-1, 1] only if the provided point was
     * contained in the viewport, with the y axis pointing upwards.
     */
    fvec2 WorldToViewport(const fvec<D> &p_WorldPos, const fmat<D> *p_Axes = nullptr) const;

    /**
     * @brief Compute the position of a point in the camera's rendering context from viewport to screen coordinates.
     *
     * @param p_ViewportPos The position to convert. Should be in the range [-1, 1] for points contained in the
     * viewport, with the y axis pointing upwards.
     * @return The position in screen coordinates. Should be in the range [-1, 1] only if the provided point was
     * contained in the screen, with the y axis pointing upwards.
     */
    fvec2 ViewportToScreen(const fvec2 &p_ViewportPos) const;

    /**
     * @brief Compute the position of a point in the camera's rendering context from screen to world coordinates.
     *
     * @param p_ScreenPos The position to convert. Should be in the range [-1, 1] for points contained in the screen,
     * with the y axis pointing upwards. If in 3D, the z axis must be between [0, 1], mapping the near and far planes or
     * the orthographic size.
     * @param p_Axes an optional axes parameter to compatibilize with render contexts.
     *
     * @return The position in world coordinates.
     */
    fvec<D> ScreenToWorld(const fvec<D> &p_ScreenPos, const fmat<D> *p_Axes = nullptr) const;

    /**
     * @brief Compute the position of a point in the camera's rendering context from world to screen coordinates.
     *
     * @param p_WorldPos The position to convert.
     * @return The position in screen coordinates. Should be in the range [-1, 1] only if the provided point was
     * contained in the screen, with the y axis pointing upwards.
     */
    fvec2 WorldToScreen(const fvec<D> &p_WorldPos, const fmat<D> *p_Axes = nullptr) const;

    /**
     * @brief Compute the position of the mouse in the camera's rendering context from screen to viewport coordinates.
     *
     * @return The mouse position in the camera's viewport coordinates.
     */
    fvec2 GetViewportMousePosition() const;

    void ControlMovementWithUserInput(const CameraControls<D> &p_Controls);
    void ControlMovementWithUserInput(TKit::Timespan p_DeltaTime);

    /**
     * @brief Control the view's scale of the camera with user input.
     *
     * Typically used in scroll events. Not recommended to use in 3D, specially with a perspective projection.
     *
     * @param p_ScaleStep The step size for scaling.
     */
    void ControlScrollWithUserInput(f32 p_ScaleStep);

    const ProjectionViewData<D> &GetProjectionViewData() const
    {
        return m_ProjectionView;
    }
    const ScreenViewport &GetViewport() const
    {
        return m_Viewport;
    }
    const ScreenScissor &GetScissor() const
    {
        return m_Scissor;
    }

    /**
     * @brief Get the view transform of the camera.
     *
     * @param p_Axes The axes coordinates of the system.
     * @return The view transform of the camera.
     */
    Onyx::Transform<D> GetViewTransform(const fmat<D> &p_Axes) const;

    void SetView(const Onyx::Transform<D> &p_View);
    void SetViewport(const ScreenViewport &p_Viewport);
    void SetScissor(const ScreenScissor &p_Scissor);

    CameraInfo CreateCameraInfo() const;

    Color BackgroundColor{Color::BLACK};
    bool Transparent = false;

  protected:
    void updateProjectionView();

    Window *m_Window;
    ProjectionViewData<D> m_ProjectionView{};
    ScreenViewport m_Viewport{};
    ScreenScissor m_Scissor{};

  private:
    void adaptViewToViewportAspect();

    fvec2 m_PrevMousePos{0.f};
    friend class Onyx::Window;
};
} // namespace Onyx::Detail

namespace Onyx
{
template <Dimension D> class Camera;

template <> class ONYX_API Camera<D2> final : public Detail::ICamera<D2>
{
    TKIT_NON_COPYABLE(Camera);

  public:
    using ICamera<D2>::ICamera;

    /**
     * @brief Compute the position of the mouse in the camera's rendering context from screen to world coordinates.
     *
     * @param p_Axes an optional axes parameter to compatibilize with render contexts.
     * @return The mouse position in the camera's rendering context coordinates.
     */
    fvec2 GetWorldMousePosition(const fmat3 *p_Axes = nullptr) const;

    void SetSize(f32 p_Size);
};

template <> class ONYX_API Camera<D3> final : public Detail::ICamera<D3>
{
    TKIT_NON_COPYABLE(Camera);

  public:
    using ICamera<D3>::ICamera;

    /**
     * @brief Compute the position of the mouse in the camera's rendering context from screen to world coordinates.
     *
     * @param p_Axes an optional axes parameter to compatibilize with render contexts.
     * @param p_Depth The depth at which to get the mouse coordinates.
     *
     * @return The mouse position in the camera's rendering context coordinates.
     */
    fvec3 GetWorldMousePosition(const fmat4 *p_Axes = nullptr, f32 p_Depth = 0.5f) const;

    /**
     * @brief Compute the position of the mouse in the camera's rendering context from screen to world coordinates.
     *
     * @param p_Depth The depth at which to get the mouse coordinates.
     *
     * @return The mouse position in the camera's rendering context coordinates.
     */
    fvec3 GetWorldMousePosition(f32 p_Depth) const;

    /**
     * @brief Get the direction of the view.
     *
     * @param p_Axes an optional axes parameter to compatibilize with render contexts.
     *
     */
    fvec3 GetViewLookDirection(const fmat4 *p_Axes = nullptr) const;

    /**
     * @brief Get the direction of an imaginary ray cast from the mouse.
     *
     * @param p_Axes an optional axes parameter to compatibilize with render contexts.
     *
     */
    fvec3 GetMouseRayCastDirection(const fmat4 *p_Axes = nullptr) const;

    void SetProjection(const fmat4 &p_Projection);

    /**
     * @brief Set a perspective projection with the given field of view and near/far planes.
     *
     * @param p_FieldOfView The field of view in radians.
     * @param p_Near The near clipping plane.
     * @param p_Far The far clipping plane.
     */
    void SetPerspectiveProjection(f32 p_FieldOfView = glm::radians(75.f), f32 p_Near = 0.1f, f32 p_Far = 100.f);

    /**
     * @brief Set a basic orthographic projection.
     */
    void SetOrthographicProjection();

    /**
     * @brief Set a basic orthographic projection.
     *
     * @param p_Size The size of the camera, respecting the current aspect ratio.
     */
    void SetOrthographicProjection(f32 p_Size);
};
} // namespace Onyx
