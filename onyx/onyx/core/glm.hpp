#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/alias.hpp"
#include "tkit/math/quaternion.hpp"

namespace Onyx
{
namespace Math = TKit::Math;

template <Dimension D> using f32m = mat<f32, D + 1, D + 1>;

namespace Detail
{
template <Dimension D> struct RotType;
template <> struct RotType<D2>
{
    using Type = f32;
    static constexpr f32 Identity = 0.f;
};
template <> struct RotType<D3>
{
    using Type = f32q;
    static inline const f32q Identity = f32q{1.f, 0.f, 0.f, 0.f};
};
} // namespace Detail

template <Dimension D> using rot = typename Detail::RotType<D>::Type;
} // namespace Onyx
