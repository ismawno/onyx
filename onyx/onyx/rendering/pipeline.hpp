#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "kit/core/non_copyable.hpp"
#include "kit/container/static_array.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
/**
 * @brief The Pipeline class encapsulates Vulkan pipeline creation and management.
 *
 * Responsible for creating graphics pipelines based on provided specifications,
 * and provides methods to bind the pipeline for rendering.
 */
class ONYX_API Pipeline
{
    KIT_NON_COPYABLE(Pipeline)

  public:
    // TODO: Reconsider the use of DynamicArray

    /**
     * @brief Struct containing specifications for creating a Vulkan pipeline.
     */
    struct Specs
    {
        /**
         * @brief Constructs a Specs object with default initialization.
         */
        Specs() noexcept;

        /**
         * @brief Populates the pipeline specifications internally.
         *
         * Involves some of the members of the struct to
         * point to other members. Every time the specs are copied, this method should be called.
         *
         */
        void Populate() noexcept;

        VkPipelineViewportStateCreateInfo ViewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo RasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo MultisampleInfo{};
        VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo ColorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo DepthStencilInfo{};

        VkPipelineLayout Layout = VK_NULL_HANDLE;
        std::array<VkDynamicState, 2> DynamicStateEnables;
        VkPipelineDynamicStateCreateInfo DynamicStateInfo{};
        VkRenderPass RenderPass = VK_NULL_HANDLE;

        u32 Subpass = 0;
        const char *VertexShaderPath = nullptr;
        const char *FragmentShaderPath = nullptr;

        std::span<const VkVertexInputBindingDescription> BindingDescriptions;
        std::span<const VkVertexInputAttributeDescription> AttributeDescriptions;
    };

    explicit Pipeline(Specs p_Specs) noexcept;
    ~Pipeline();

    /**
     * @brief Binds the pipeline to the specified command buffer.
     *
     * @param p_CommandBuffer The Vulkan command buffer to bind the pipeline to.
     */
    void Bind(VkCommandBuffer p_CommandBuffer) const noexcept;

    /**
     * @brief Retrieves the pipeline layout.
     *
     * @return The Vulkan pipeline layout.
     */
    VkPipelineLayout GetLayout() const noexcept;

  private:
    void createPipeline(const Specs &p_Specs) noexcept;
    VkShaderModule createShaderModule(const char *p_Path) noexcept;

    KIT::Ref<Device> m_Device;
    VkPipeline m_Pipeline;
    VkPipelineLayout m_Layout;
    VkShaderModule m_VertexShader;
    VkShaderModule m_FragmentShader;
};
} // namespace ONYX
