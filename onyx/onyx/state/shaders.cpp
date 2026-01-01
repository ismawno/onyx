#include "onyx/core/pch.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/core/core.hpp"
#include "tkit/preprocessor/utils.hpp"

namespace Onyx::Shaders
{
std::string CreateShaderDefaultBinaryPath(const std::string_view p_SourcePath)
{
    namespace fs = std::filesystem;
    fs::path binaryPath = p_SourcePath;
    binaryPath = binaryPath.parent_path() / "bin" / binaryPath.filename();
    binaryPath += ".spv";
    return binaryPath.string();
}

VKit::Shader Create(const std::string_view p_SourcePath)
{
    const std::string binaryPath = CreateShaderDefaultBinaryPath(p_SourcePath);
    return Create(p_SourcePath, binaryPath);
}

VKit::Shader Create(const std::string_view p_SourcePath, const std::string_view p_BinaryPath,
                    const std::string_view p_Arguments)
{
    if (VKit::Shader::MustCompile(p_SourcePath, p_BinaryPath))
        Compile(p_SourcePath, p_BinaryPath, p_Arguments);

    const auto result = VKit::Shader::Create(Core::GetDevice(), p_BinaryPath);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

void Compile(const std::string_view p_SourcePath)
{
    const std::string binaryPath = CreateShaderDefaultBinaryPath(p_SourcePath);
    Compile(p_SourcePath, binaryPath);
}

void Compile(const std::string_view p_SourcePath, const std::string_view p_BinaryPath,
             const std::string_view p_Arguments)
{
    const auto shresult = VKit::Shader::CompileFromFile(p_SourcePath, p_BinaryPath, p_Arguments);
    TKIT_ASSERT(shresult, "[ONYX] Failed to compile shader at {}. Error code is: {}", p_SourcePath,
                shresult.GetError());

    TKIT_LOG_INFO_IF(shresult, "[ONYX] Compiled shader: {}", p_SourcePath);
    TKIT_UNUSED(shresult);
}

const VKit::Shader &GetFullPassVertexShader()
{
    static VKit::Shader shader{};
    if (shader)
        return shader;
    shader = Create(ONYX_ROOT_PATH "/onyx/shaders/pp-full-pass.vert");
    Core::GetDeletionQueue().SubmitForDeletion(shader);
    return shader;
}

} // namespace Onyx::Shaders
