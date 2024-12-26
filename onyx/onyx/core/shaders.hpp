#pragma once

#include "onyx/rendering/render_specs.hpp"

namespace Onyx
{
template <Dimension D, DrawMode DMode> struct Shaders
{
    static void Initialize() noexcept;

    static const VKit::Shader &GetMeshVertexShader() noexcept;
    static const VKit::Shader &GetMeshFragmentShader() noexcept;
    static const VKit::Shader &GetCircleVertexShader() noexcept;
    static const VKit::Shader &GetCircleFragmentShader() noexcept;
};
// Not meant to be used directly by user
ONYX_API VKit::Shader CreateShader(std::string_view p_SourcePath) noexcept;
ONYX_API const VKit::Shader &GetFullPassVertexShader() noexcept;
} // namespace Onyx