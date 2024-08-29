#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/core/non_copyable.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
class Pipeline
{
    KIT_NON_COPYABLE(Pipeline)
  public:
    // TODO: Reconsider the use of DynamicArray
    struct Specs
    {
        KIT_NON_COPYABLE(Specs)
        Specs() noexcept = default;

        VkPipelineViewportStateCreateInfo ViewportInfo;
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo RasterizationInfo;
        VkPipelineMultisampleStateCreateInfo MultisampleInfo;
        VkPipelineColorBlendAttachmentState ColorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo ColorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo DepthStencilInfo;

        DynamicArray<VkDynamicState> DynamicStateEnables;
        VkPipelineDynamicStateCreateInfo DynamicStateInfo;

        VkPipelineLayout PipelineLayout = nullptr;
        VkRenderPass RenderPass = nullptr;
        u32 Subpass = 0;

        const char *VertexShaderPath = nullptr;
        const char *FragmentShaderPath = nullptr;

        DynamicArray<VkVertexInputBindingDescription> BindingDescriptions;
        DynamicArray<VkVertexInputAttributeDescription> AttributeDescriptions;
        u32 ConstantRangeSize = 0;

        static void PopulateWithDefault(Specs &p_Specs) noexcept;
    };

    explicit Pipeline(const Specs &p_Specs) noexcept;
    ~Pipeline();

  private:
    void initialize(const Specs &p_Specs) noexcept;
    VkShaderModule createShaderModule(const char *p_Path) noexcept;

    KIT::Ref<Device> m_Device;
    VkPipeline m_Pipeline;
    VkShaderModule m_VertexShader;
    VkShaderModule m_FragmentShader;
};
} // namespace ONYX