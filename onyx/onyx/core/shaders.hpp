#pragma once

#include "onyx/rendering/render_specs.hpp"

namespace Onyx::Detail
{
template <Dimension D, DrawMode DMode> struct ONYX_API Shaders
{
    static void Initialize() noexcept;

    static const VKit::Shader &GetMeshVertexShader() noexcept;
    static const VKit::Shader &GetMeshFragmentShader() noexcept;
    static const VKit::Shader &GetCircleVertexShader() noexcept;
    static const VKit::Shader &GetCircleFragmentShader() noexcept;
};
} // namespace Onyx::Detail

namespace Onyx
{
ONYX_API VKit::Shader CreateShader(std::string_view p_SourcePath) noexcept;
ONYX_API const VKit::Shader &GetFullPassVertexShader() noexcept;
} // namespace Onyx