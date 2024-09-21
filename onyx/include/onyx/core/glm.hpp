#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/dimension.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

// This file includes all necessary glm headers and defines some useful aliases

namespace ONYX
{
namespace Aliases
{
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

template <u32 N> using vec = glm::vec<N, f32>;

using mat2 = glm::mat2;
using mat3 = glm::mat3;
using mat4 = glm::mat4;

template <u32 N> using mat = glm::mat<N, N, f32>;

using quat = glm::quat;

ONYX_DIMENSION_TEMPLATE struct RotType;

template <> struct RotType<2>
{
    using Type = f32;
};
template <> struct RotType<3>
{
    using Type = quat;
};

ONYX_DIMENSION_TEMPLATE using rot = typename RotType<N>::Type;

}; // namespace Aliases

// Directly use the Aliases namespace when in the ONYX namespace
using namespace Aliases;
} // namespace ONYX