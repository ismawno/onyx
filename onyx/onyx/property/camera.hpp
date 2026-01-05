#pragma once

#include "onyx/state/state.hpp"
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
 * the gpu as a `f32m4`, the renderer applies the extrinsic coordinate system.
 *
 * In 3D, the projection view matrix is the projection matrix multiplied by the view matrix. As the view matrix is
 * already a `f32m4`, the renderer can directly apply the extrinsic coordinate system.
 *
 * @tparam D The dimension (`D2` or `D3`).
 */
template <Dimension D> struct ProjectionViewData;

template <> struct ProjectionViewData<D2>
{
    TKIT_REFLECT_DECLARE(ProjectionViewData)
    TKIT_YAML_SERIALIZE_DECLARE(ProjectionViewData)
    Transform<D2> View{};
    f32m3 ProjectionView = f32m3::Identity();
};
template <> struct ProjectionViewData<D3>
{
    TKIT_REFLECT_DECLARE(ProjectionViewData)
    TKIT_YAML_SERIALIZE_DECLARE(ProjectionViewData)
    Transform<D3> View{};
    f32m4 Projection = f32m4::Identity();
    f32m4 ProjectionView = f32m4::Identity();
};

/**
 * @brief The `ScreenViewport` struct holds screen viewport dimensions.
 *
 * It is represented as an axis-aligned rectangle with the `Min` and `Max` coordinates ranging from -1 to 1. The
 * `DepthBounds` are normalized, ranging from 0 to 1. The default values are set to cover the entire screen.
 */
struct ScreenViewport
{
    TKIT_REFLECT_DECLARE(ScreenViewport)
    TKIT_YAML_SERIALIZE_DECLARE(ScreenViewport)
    f32v2 Min{-1.f};
    f32v2 Max{1.f};
    f32v2 DepthBounds{0.f, 1.f};

    VkViewport AsVulkanViewport(const VkExtent2D &p_Extent) const;
};

/**
 * @brief The `ScreenScissor` struct holds screen scissor dimensions relative to a viewport.
 *
 * It is represented as an axis-aligned rectangle with the `Min` and `Max` coordinates ranging from -1 to 1. The default
 * values are set to cover the entire screen.
 */
struct ScreenScissor
{
    TKIT_REFLECT_DECLARE(ScreenScissor)
    TKIT_YAML_SERIALIZE_DECLARE(ScreenScissor)
    f32v2 Min{-1.f};
    f32v2 Max{1.f};

    VkRect2D AsVulkanScissor(const VkExtent2D &p_Extent, const ScreenViewport &p_Viewport) const;
};

template <Dimension D> struct CameraControls;

template <> struct CameraControls<D2>
{
    f32 TranslationStep = 1.f / 60.f;
    f32 RotationStep = 1.f / 60.f;
    Input::Key Up = Input::Key_W;
    Input::Key Down = Input::Key_S;
    Input::Key Left = Input::Key_A;
    Input::Key Right = Input::Key_D;
    Input::Key RotateLeft = Input::Key_Q;
    Input::Key RotateRight = Input::Key_E;
};
template <> struct CameraControls<D3>
{
    f32 TranslationStep = 1.f / 60.f;
    f32 RotationStep = 1.f / 60.f;
    Input::Key Forward = Input::Key_W;
    Input::Key Backward = Input::Key_S;
    Input::Key Left = Input::Key_A;
    Input::Key Right = Input::Key_D;
    Input::Key Up = Input::Key_Space;
    Input::Key Down = Input::Key_LeftControl;
    Input::Key RotateLeft = Input::Key_Q;
    Input::Key RotateRight = Input::Key_E;
    Input::Key ToggleLookAround = Input::Key_LeftShift;
};

} // namespace Onyx

namespace Onyx::Detail
{

template <Dimension D> class ICamera
{
    TKIT_NON_COPYABLE(ICamera);

  public:
    ICamera() = default;

    f32v2 ScreenToViewport(const f32v2 &p_ScreenPos) const;
    f32v<D> ViewportToWorld(f32v<D> p_ViewportPos) const;
    f32v2 WorldToViewport(const f32v<D> &p_WorldPos) const;

    f32v2 ViewportToScreen(const f32v2 &p_ViewportPos) const;
    f32v<D> ScreenToWorld(const f32v<D> &p_ScreenPos) const;
    f32v2 WorldToScreen(const f32v<D> &p_WorldPos) const;

    f32v2 GetViewportMousePosition() const;

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

    f32v2 m_PrevMousePos{0.f};
    friend class Onyx::Window;
};
} // namespace Onyx::Detail

namespace Onyx
{
template <Dimension D> class Camera;

template <> class Camera<D2> final : public Detail::ICamera<D2>
{
    TKIT_NON_COPYABLE(Camera);

  public:
    using ICamera<D2>::ICamera;

    f32v2 GetWorldMousePosition() const;

    void SetSize(f32 p_Size);
};

template <> class Camera<D3> final : public Detail::ICamera<D3>
{
    TKIT_NON_COPYABLE(Camera);

  public:
    using ICamera<D3>::ICamera;

    f32v3 GetWorldMousePosition(f32 p_Depth = 0.5f) const;
    f32v3 GetViewLookDirection() const;
    f32v3 GetMouseRayCastDirection() const;

    void SetProjection(const f32m4 &p_Projection);
    void SetPerspectiveProjection(f32 p_FieldOfView = Math::Radians(75.f), f32 p_Near = 0.1f, f32 p_Far = 100.f);
    void SetOrthographicProjection();
    void SetOrthographicProjection(f32 p_Size);
};
} // namespace Onyx
