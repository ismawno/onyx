#pragma once

#include "onyx/core/device.hpp"

#include <vulkan/vulkan.hpp>

namespace Onyx
{
class Shader
{
    TKIT_NON_COPYABLE(Shader)
  public:
    Shader(std::string_view p_BinaryPath) noexcept;
    Shader(std::string_view p_SourcePath, std::string_view p_BinaryPath) noexcept;

    ~Shader() noexcept;

    VkShaderModule GetModule() const noexcept;

  private:
    void compileShader(std::string_view p_SourcePath, std::string_view p_BinaryPath) noexcept;
    void createShaderModule(std::string_view p_BinaryPath) noexcept;

    TKit::Ref<Device> m_Device;
    VkShaderModule m_Module;
};
} // namespace Onyx