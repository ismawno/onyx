#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/api.hpp"
#include "onyx/resource/state.hpp"
#include "vkit/state/graphics_pipeline.hpp"
#include "vkit/state/shader.hpp"

namespace Onyx::Pipelines
{
ONYX_API void Initialize();
ONYX_API void Terminate();

ONYX_API VkPipelineLayout GetGraphicsPipelineLayout(Shading p_Shading);

// consider exposing shader data

template <Dimension D>
VKit::GraphicsPipeline CreateStaticMeshPipeline(PipelineMode p_Mode,
                                                const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

template <Dimension D>
VKit::GraphicsPipeline CreateCirclePipeline(PipelineMode p_Mode, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

/**
 * @brief Create a default shader binary path from a source path.
 *
 * The default binary path is `<source-parent>/bin/<shader-filename>.spv`.
 *
 * @param p_SourcePath The source path.
 * @return The binary path.
 */
ONYX_API std::string CreateShaderDefaultBinaryPath(std::string_view p_SourcePath);

/**
 * @brief This is a high level function to create a compiled shader binary from a source glsl shader file.
 *
 * First, the function checks if an up-to-date binary file exists for the given source file at
 * `<source-parent>/bin/<shader-filename>.spv`. If it does, the function returns the shader module created from that
 * existing binary file. If it doesn't, the function compiles the source file into a binary file using the glslc
 * executable (its path stored in the `VKIT_GLSL_BINARY` macro), located at `<source-parent>/bin/<shader-filename>.spv`
 * and returns the shader module created from the binary file.
 *
 * @param p_SourcePath The path to the source file.
 * @return The created shader.
 */
ONYX_API VKit::Shader CreateShader(std::string_view p_SourcePath);

/**
 * @brief This is a high level function to create a compiled shader binary from a source glsl shader file.
 *
 * This function is similar to the other `CreateShader()` overload, but it allows the user to specify the binary path
 * and the arguments to pass to the `glslc` compiler.
 *
 * @param p_SourcePath The path to the source file.
 * @param p_BinaryPath The path to the binary file.
 * @param p_Arguments The arguments to pass to the `glslc` compiler.
 * @return The created shader.
 */
ONYX_API VKit::Shader CreateShader(std::string_view p_SourcePath, std::string_view p_BinaryPath,
                                   std::string_view p_Arguments = "");

/**
 * @brief This is a high level function to compile a shader from a source glsl shader file.
 *
 * The function compiles the source file into a binary file using the glslc executable (its path stored in the
 * `VKIT_GLSL_BINARY` macro), located at `<source-parent>/bin/<shader-filename>.spv`.
 *
 * This function, when built in Debug, will assert that the compilation was successful. If you wish to handle the error
 * yourself, use `VKit::Shader::Compile()`
 *
 * @param p_SourcePath The path to the source file.
 */
ONYX_API void CompileShader(std::string_view p_SourcePath);

/**
 * @brief This is a high level function to compile a shader from a source glsl shader file.
 *
 * This function is similar to the other `CompileShader()` overload, but it allows the user to specify the binary path
 * and the arguments to pass to the `glslc` compiler.
 *
 * This function, when built in Debug, will assert that the compilation was successful. If you wish to handle the error
 * yourself, use `VKit::Shader::Compile()`
 *
 * @param p_SourcePath The path to the source file.
 * @param p_BinaryPath The path to the binary file.
 * @param p_Arguments The arguments to pass to the `glslc` compiler.
 */
ONYX_API void CompileShader(std::string_view p_SourcePath, std::string_view p_BinaryPath,
                            std::string_view p_Arguments = "");

/**
 * @brief Get a full pass vertex shader.
 *
 * This function returns an already available full pass vertex shader that outputs the UV coordinates of the whole
 * screen to the fragment shader.
 *
 * @return The full pass vertex shader.
 */
ONYX_API const VKit::Shader &GetFullPassVertexShader();

} // namespace Onyx::Pipelines
