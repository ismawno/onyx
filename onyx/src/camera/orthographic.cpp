#include "core/pch.hpp"
#include "onyx/camera/orthographic.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Orthographic<N>::Orthographic(const f32 p_Size, const f32 p_Aspect) noexcept
{
    if constexpr (N == 2)
        this->Transform.SetScale({p_Aspect * p_Size, p_Size});
    else
        this->Transform.SetScale({p_Aspect * p_Size, p_Size, p_Size});
}

ONYX_DIMENSION_TEMPLATE void Orthographic<N>::UpdateMatrices() noexcept
{
    if (this->Transform.NeedsMatrixUpdate())
        this->Transform.UpdateMatricesAsCamera();

    this->m_Projection = this->Transform.GetGlobalTransform();
    this->m_InverseProjection = this->Transform.ComputeInverseGlobalCameraTransform();
}

ONYX_DIMENSION_TEMPLATE f32 Orthographic<N>::GetSize() const noexcept
{
    return this->Transform.GetScale().y;
}

ONYX_DIMENSION_TEMPLATE void Orthographic<N>::SetSize(const f32 p_Size) noexcept
{
    const f32 aspect = this->Transform.GetScale().x / this->Transform.GetScale().y;
    if constexpr (N == 2)
        this->Transform.SetScale({aspect * p_Size, p_Size});
    else
        this->Transform.SetScale({aspect * p_Size, p_Size, p_Size});
}

template class Orthographic<2>;
template class Orthographic<3>;

} // namespace ONYX