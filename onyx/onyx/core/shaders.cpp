#include "onyx/core/pch.hpp"
#include "onyx/core/shaders.hpp"
#include "onyx/core/core.hpp"

namespace Onyx
{
VKit::Shader CreateShader(const std::string_view p_SourcePath) noexcept
{
    namespace fs = std::filesystem;
    const auto createBinaryPath = [](const std::string_view p_Path) {
        fs::path binaryPath = p_Path;
        binaryPath = binaryPath.parent_path() / "bin" / binaryPath.filename();
        binaryPath += ".spv";
        return binaryPath;
    };

    const fs::path binaryPath = createBinaryPath(p_SourcePath);
    const i32 shresult = VKit::Shader::CompileIfModified(p_SourcePath, binaryPath.c_str());

    TKIT_ASSERT(shresult == 0 || shresult == INT32_MAX, "Failed to compile shader at {}", p_SourcePath);
    TKIT_LOG_INFO_IF(shresult == 0, "Compiled shader: {}", p_SourcePath);
    (void)shresult;

    const auto result = VKit::Shader::Create(Core::GetDevice(), binaryPath.c_str());
    VKIT_ASSERT_RESULT(result);
    return result.GetValue();
}
template <Dimension D, DrawMode DMode> struct SneakyShaders
{
    static void Initialize() noexcept
    {
        if constexpr (D == D2)
        {
            MeshVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh2D.vert");
            MeshFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh2D.frag");

            CircleVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle2D.vert");
            CircleFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle2D.frag");
        }
        else if constexpr (DMode == DrawMode::Fill)
        {
            MeshVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh3D.vert");
            MeshFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh3D.frag");

            CircleVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle3D.vert");
            CircleFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle3D.frag");
        }
        else
        {
            MeshVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh-outline3D.vert");
            MeshFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/mesh2D.frag");

            CircleVertexShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle2D.vert");
            CircleFragmentShader = CreateShader(ONYX_ROOT_PATH "/onyx/shaders/circle2D.frag");
        }
    }

    static void Terminate() noexcept
    {
        MeshVertexShader.Destroy();
        MeshFragmentShader.Destroy();
        CircleVertexShader.Destroy();
        CircleFragmentShader.Destroy();
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
template <Dimension D, DrawMode DMode> void Shaders<D, DMode>::Terminate() noexcept
{
    SneakyShaders<D, DMode>::Terminate();
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

template struct Shaders<D2, DrawMode::Fill>;
template struct Shaders<D2, DrawMode::Stencil>;
template struct Shaders<D3, DrawMode::Fill>;
template struct Shaders<D3, DrawMode::Stencil>;
} // namespace Onyx