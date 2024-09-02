#pragma once

#include "onyx/core/alias.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

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
}; // namespace Aliases

// Directly use the Aliases namespace when in the ONYX namespace
using namespace Aliases;
} // namespace ONYX