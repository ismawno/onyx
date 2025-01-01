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
/**
 * @brief This is a high level function to create a compiled shader binary from a source glsl shader file.
 *
 * First, the function checks if there exists an up-to-date binary file for the given source file at
 * <source-parent>/bin/<shader-filename>.spv. If it does, the function returns the shader module created from that
 * existing binary file. If it doesn't, the function compiles the source file into a binary file using the glslc
 * executable (its path stored in the VKIT_GLSL_BINARY macro), located at <source-parent>/bin/<shader-filename>.spv
 * and returns the shader module created from the binary file.
 *
 * @param p_SourcePath The path to the source file.
 * @return VKit::Shader The created shader.
 */
ONYX_API VKit::Shader CreateShader(std::string_view p_SourcePath) noexcept;

/**
 * @brief Get a full pass vertex shader.
 *
 * This function returns an already available full pass vertex shader that outputs the UV coordinates of the whole
 * screen to the fragment shader.
 *
 * @return const VKit::Shader& The full pass vertex shader.
 */
ONYX_API const VKit::Shader &GetFullPassVertexShader() noexcept;
} // namespace Onyx