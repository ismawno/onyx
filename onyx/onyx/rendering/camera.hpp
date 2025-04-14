#pragma once

#include "onyx/rendering/render_structs.hpp"
#include "onyx/app/input.hpp"
#include "tkit/profiling/timespan.hpp"
#include "tkit/utils/non_copyable.hpp"

namespace Onyx
{
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
    Transform<D2> View{};
    fmat3 ProjectionView{1.f};
};
template <> struct ONYX_API ProjectionViewData<D3>
{
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
    fvec2 Min{-1.f};
    fvec2 Max{1.f};
    fvec2 DepthBounds{0.f, 1.f};

    /**
     * @brief Convert the viewport to a Vulkan viewport given a Vulkan extent.
     *
     * @param p_Extent The Vulkan extent to use for the conversion.
     * @return The Vulkan viewport.
     */
    VkViewport AsVulkanViewport(const VkExtent2D &p_Extent) const noexcept;
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
    VkRect2D AsVulkanScissor(const VkExtent2D &p_Extent, const ScreenViewport &p_Viewport) const noexcept;
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
     * @brief Retrieve the position of a point in the camera's rendering context from screen to world coordinates.
     *
     * @param p_ScreenPos The position to convert. Should be in the range [-1, 1]. If in 3D, the Z
     * axis must be between [0, 1].
     * @return The coordinates of the point in the camera's rendering context.
     */
    fvec<D> ScreenToWorld(const fvec<D> &p_ScreenPos) const noexcept;

    void ControlMovementWithUserInput(const CameraControls<D> &p_Controls) noexcept;
    void ControlMovementWithUserInput(TKit::Timespan p_DeltaTime) noexcept;

    const ProjectionViewData<D> &GetProjectionViewData() const noexcept;
    const ScreenViewport &GetViewport() const noexcept;
    const ScreenScissor &GetScissor() const noexcept;

    /**
     * @brief Get the view transform of the camera in the current axes.
     *
     * @return The view transform of the camera.
     */
    Onyx::Transform<D> GetViewTransform() const noexcept;

    void SetView(const Onyx::Transform<D> &p_View) noexcept;
    void SetViewport(const ScreenViewport &p_Viewport) noexcept;
    void SetScissor(const ScreenScissor &p_Scissor) noexcept;

    CameraInfo CreateCameraInfo() const noexcept;

    Color BackgroundColor{Color::BLACK};
    bool Transparent = false;

  protected:
    void updateProjectionView() noexcept;

    Window *m_Window;
    ProjectionViewData<D> m_ProjectionView{};
    ScreenViewport m_Viewport{};
    ScreenScissor m_Scissor{};

  private:
    void adaptViewToViewportAspect() noexcept;

    RenderState<D> *m_State;
    fvec2 m_PrevMousePos{0.f};

    template <Dimension Dim> friend class IRenderContext;
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
     * @brief Retrieve the position of the mouse in the camera's rendering context from screen to world coordinates.
     *
     * @return The mouse position in the camera's rendering context coordinates.
     */
    fvec2 GetMousePosition() const noexcept;

    /**
     * @brief Control the view's scale of the camera with user input.
     *
     * Typically used in scroll events.
     *
     * @param p_ScaleStep The step size for scaling.
     */
    void ControlScrollWithUserInput(f32 p_ScaleStep) noexcept;
};

template <> class ONYX_API Camera<D3> final : public Detail::ICamera<D3>
{
    TKIT_NON_COPYABLE(Camera);

  public:
    using ICamera<D3>::ICamera;

    /**
     * @brief Retrieve the position of the mouse in the camera's rendering context from screen to world coordinates.
     *
     * @param p_Depth The depth at which to get the mouse coordinates.
     * @return The mouse position in the camera's rendering context coordinates.
     */
    fvec3 GetMousePosition(f32 p_Depth = 0.5f) const noexcept;

    /**
     * @brief Get the direction of the view in the current axes.
     *
     */
    fvec3 GetViewLookDirection() const noexcept;

    /**
     * @brief Get the direction of an imaginary ray cast from the mouse in the current axes.
     *
     */
    fvec3 GetMouseRayCastDirection() const noexcept;

    void SetProjection(const fmat4 &p_Projection) noexcept;

    /**
     * @brief Set a perspective projection with the given field of view and near/far planes.
     *
     * @param p_FieldOfView The field of view in radians.
     * @param p_Near The near clipping plane.
     * @param p_Far The far clipping plane.
     */
    void SetPerspectiveProjection(f32 p_FieldOfView = glm::radians(75.f), f32 p_Near = 0.1f,
                                  f32 p_Far = 100.f) noexcept;

    void SetOrthographicProjection() noexcept;
};
} // namespace Onyx
