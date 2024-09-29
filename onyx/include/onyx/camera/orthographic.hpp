#pragma once

#include "onyx/camera/camera.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE class Orthographic final : public Camera<N>
{
  public:
    using Camera<N>::Camera;
    Orthographic(f32 p_Size, f32 p_Aspect = ONYX_DEFAULT_ASPECT) noexcept;

    mat4 ComputeProjectionView() const noexcept override;
    mat4 ComputeInverseProjectionView() const noexcept override;

    f32 GetSize() const noexcept;
    void SetSize(f32 p_Size) noexcept;

    bool IsOrthographic() const noexcept override;
};

using Orthographic2D = Orthographic<2>;
using Orthographic3D = Orthographic<3>;

} // namespace ONYX