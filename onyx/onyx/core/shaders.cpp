#include "onyx/core/pch.hpp"
#include "onyx/core/shaders.hpp"
#include "onyx/core/core.hpp"

namespace Onyx
{
std::string CreateShaderDefaultBinaryPath(const std::string_view p_SourcePath) noexcept
{
    namespace fs = std::filesystem;
    fs::path binaryPath = p_SourcePath;
    binaryPath = binaryPath.parent_path() / "bin" / binaryPath.filename();
    binaryPath += ".spv";
    return binaryPath.string();
}

VKit::Shader CreateShader(const std::string_view p_SourcePath) noexcept
{
    const std::string binaryPath = CreateShaderDefaultBinaryPath(p_SourcePath);
    return CreateShader(p_SourcePath, binaryPath);
}

VKit::Shader CreateShader(const std::string_view p_SourcePath, const std::string_view p_BinaryPath,
                          const std::string_view p_Arguments) noexcept
{
    if (VKit::Shader::MustCompile(p_SourcePath, p_BinaryPath))
    {
        const i32 shresult = VKit::Shader::Compile(p_SourcePath, p_BinaryPath, p_Arguments);

        TKIT_ASSERT(shresult == 0 || shresult == INT32_MAX, "[ONYX] Failed to compile shader at {}", p_SourcePath);
        TKIT_LOG_INFO_IF(shresult == 0, "[ONYX] Compiled shader: {}", p_SourcePath);
        (void)shresult;
    }

    const auto result = VKit::Shader::Create(Core::GetDevice(), p_BinaryPath);
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}

const VKit::Shader &GetFullPassVertexShader() noexcept
{
    static VKit::Shader shader{};
    if (shader)
        return shader;
    shader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/pp-full-pass.vert");
    Core::GetDeletionQueue().SubmitForDeletion(shader);
    return shader;
}
} // namespace Onyx

namespace Onyx::Detail
{
template <Dimension D, DrawMode DMode> struct SneakyShaders
{
    static void Initialize() noexcept
    {
        if constexpr (D == D2)
        {
            MeshVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-2D.vert");
            MeshFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-drawlevel-simple.frag");

            CircleVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle-drawlevel-simple.vert");
            CircleFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle-drawlevel-simple.frag");
        }
        else if constexpr (DMode == DrawMode::Fill)
        {
            MeshVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-drawmode-fill-3D.vert");
            MeshFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-drawmode-fill-3D.frag");

            CircleVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle-drawmode-fill-3D.vert");
            CircleFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle-drawmode-fill-3D.frag");
        }
        else
        {
            MeshVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-drawmode-stencil-3D.vert");
            MeshFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-drawlevel-simple.frag");

            CircleVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle-drawlevel-simple.vert");
            CircleFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle-drawlevel-simple.frag");
        }

        Core::GetDeletionQueue().SubmitForDeletion(MeshVertexShader);
        Core::GetDeletionQueue().SubmitForDeletion(MeshFragmentShader);
        Core::GetDeletionQueue().SubmitForDeletion(CircleVertexShader);
        Core::GetDeletionQueue().SubmitForDeletion(CircleFragmentShader);
    }

    static inline VKit::Shader MeshVertexShader{};
    static inline VKit::Shader MeshFragmentShader{};
    static inline VKit::Shader CircleVertexShader{};
    static inline VKit::Shader CircleFragmentShader{};
};

template <Dimension D, DrawMode DMode> void Shaders<D, DMode>::Initialize() noexcept
{
    SneakyShaders<D, DMode>::Initialize();
}

template <Dimension D, DrawMode DMode> const VKit::Shader &Shaders<D, DMode>::GetMeshVertexShader() noexcept
{
    return SneakyShaders<D, DMode>::MeshVertexShader;
}
template <Dimension D, DrawMode DMode> const VKit::Shader &Shaders<D, DMode>::GetMeshFragmentShader() noexcept
{
    return SneakyShaders<D, DMode>::MeshFragmentShader;
}
template <Dimension D, DrawMode DMode> const VKit::Shader &Shaders<D, DMode>::GetCircleVertexShader() noexcept
{
    return SneakyShaders<D, DMode>::CircleVertexShader;
}
template <Dimension D, DrawMode DMode> const VKit::Shader &Shaders<D, DMode>::GetCircleFragmentShader() noexcept
{
    return SneakyShaders<D, DMode>::CircleFragmentShader;
}

template struct ONYX_API Shaders<D2, DrawMode::Fill>;
template struct ONYX_API Shaders<D2, DrawMode::Stencil>;
template struct ONYX_API Shaders<D3, DrawMode::Fill>;
template struct ONYX_API Shaders<D3, DrawMode::Stencil>;
} // namespace Onyx::Detail