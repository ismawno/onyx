#pragma once

#include "onyx/rendering/render_specs.hpp"

namespace Onyx
{
template <Dimension D, DrawMode DMode> struct Shaders
{
    static void Initialize() noexcept;
    static void Terminate() noexcept;

    static const VKit::Shader &GetMeshVertexShader() noexcept;
    static const VKit::Shader &GetMeshFragmentShader() noexcept;
    static const VKit::Shader &GetCircleVertexShader() noexcept;
    static const VKit::Shader &GetCircleFragmentShader() noexcept;
};
} // namespace Onyx