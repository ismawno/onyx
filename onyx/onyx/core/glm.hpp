#pragma once

#include "tkit/utilities/glm.hpp"
#include "onyx/core/dimension.hpp"
#include "onyx/core/alias.hpp"

namespace Onyx
{
using vec2 = glm::vec<2, f32>;
using vec3 = glm::vec<3, f32>;
using vec4 = glm::vec<4, f32>;

template <Dimension D> using vec = glm::vec<D, f32>;

using mat2 = glm::mat<2, 2, f32>;
using mat3 = glm::mat<3, 3, f32>;
using mat4 = glm::mat<4, 4, f32>;

template <Dimension D> using mat = glm::mat<D + 1, D + 1, f32>;

using quat = glm::quat;

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