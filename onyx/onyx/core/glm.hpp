#pragma once

#include "tkit/utils/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/core/alias.hpp"
#include "onyx/core/api.hpp"

namespace Onyx
{
using fvec2 = glm::vec<2, f32>;
using fvec3 = glm::vec<3, f32>;
using fvec4 = glm::vec<4, f32>;

template <Dimension D> using fvec = glm::vec<D, f32>;

using fmat2 = glm::mat<2, 2, f32>;
using fmat3 = glm::mat<3, 3, f32>;
using fmat4 = glm::mat<4, 4, f32>;

template <Dimension D> using fmat = glm::mat<D + 1, D + 1, f32>;

using quat = glm::qua<f32>;

template <Dimension D> struct RotType;

template <> struct RotType<D2>
{
    using Type = f32;
    static constexpr f32 Identity = 0.f;
};
template <> struct RotType<D3>
{
    using Type = quat;
    static inline const quat Identity = quat{1.f, 0.f, 0.f, 0.f};
};

template <Dimension D> using rot = typename RotType<D>::Type;
} // namespace Onyx