#include "core/pch.hpp"
#include "onyx/camera/orthographic.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Orthographic<N>::Orthographic(const f32 p_Size, const f32 p_Aspect) noexcept
{
    if constexpr (N == 2)
        this->Transform.Scale = {p_Aspect * p_Size, p_Size};
    else
        this->Transform.Scale = {p_Aspect * p_Size, p_Size, p_Size};
}

ONYX_DIMENSION_TEMPLATE void Orthographic<N>::UpdateMatrices() noexcept
{
    if (this->m_YFlipped)
        this->Transform.Scale.y = -this->Transform.Scale.y;

    this->m_Projection = this->Transform.CameraTransform();
    this->m_InverseProjection = this->Transform.InverseCameraTransform();

    if (this->m_YFlipped)
        this->Transform.Scale.y = -this->Transform.Scale.y;
}

ONYX_DIMENSION_TEMPLATE f32 Orthographic<N>::GetSize() const noexcept
{
    return this->Transform.Scale.y;
}

ONYX_DIMENSION_TEMPLATE void Orthographic<N>::SetSize(const f32 p_Size) noexcept
{
    const f32 aspect = this->Transform.Scale.x / this->Transform.Scale.y;
    if constexpr (N == 2)
        this->Transform.Scale = {aspect * p_Size, p_Size};
    else
        this->Transform.Scale = {aspect * p_Size, p_Size, p_Size};
}

template class Orthographic<2>;
template class Orthographic<3>;

} // namespace ONYX