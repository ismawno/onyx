#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/core/non_copyable.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
class ONYX_API Pipeline
{
    KIT_NON_COPYABLE(Pipeline)
  public:
    // TODO: Reconsider the use of DynamicArray
    struct Specs
    {
        Specs() noexcept;
        void Populate() noexcept;

        VkPipelineViewportStateCreateInfo ViewportInfo;
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo RasterizationInfo;
        VkPipelineMultisampleStateCreateInfo MultisampleInfo;
        VkPipelineColorBlendAttachmentState ColorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo ColorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo DepthStencilInfo;

        DynamicArray<VkDynamicState> DynamicStateEnables;
        VkPipelineDynamicStateCreateInfo DynamicStateInfo;

        VkRenderPass RenderPass = VK_NULL_HANDLE;
        u32 Subpass = 0;

        const char *VertexShaderPath = nullptr;
        const char *FragmentShaderPath = nullptr;

        DynamicArray<VkVertexInputBindingDescription> BindingDescriptions;
        DynamicArray<VkVertexInputAttributeDescription> AttributeDescriptions;
        u32 ConstantRangeSize = 0;
    };

    explicit Pipeline(Specs p_Specs) noexcept;
    ~Pipeline();

    void Bind(VkCommandBuffer p_CommandBuffer) const noexcept;

  private:
    void initialize(const Specs &p_Specs) noexcept;
    void createPipelineLayout() noexcept;

    VkShaderModule createShaderModule(const char *p_Path) noexcept;

    KIT::Ref<Device> m_Device;
    VkPipeline m_Pipeline;
    VkPipelineLayout m_PipelineLayout;

    VkShaderModule m_VertexShader;
    VkShaderModule m_FragmentShader;
};
} // namespace ONYX