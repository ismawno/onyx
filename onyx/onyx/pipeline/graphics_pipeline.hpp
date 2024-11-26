#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/device.hpp"
#include "onyx/pipeline/shader.hpp"
#include "tkit/core/non_copyable.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/container/storage.hpp"

#include <vulkan/vulkan.hpp>

namespace Onyx
{
/**
 * @brief The GraphicsPipeline class encapsulates Vulkan pipeline creation and management.
 *
 * Responsible for creating graphics pipelines based on provided specifications,
 * and provides methods to bind the pipeline for rendering.
 */
class ONYX_API GraphicsPipeline
{
    TKIT_NON_COPYABLE(GraphicsPipeline)

  public:
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

    explicit GraphicsPipeline(Specs p_Specs) noexcept;
    ~GraphicsPipeline();

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
    void createShaders(const char *p_VertexPath, const char *p_FragmentPath) noexcept;

    TKit::Ref<Device> m_Device;
    VkPipeline m_Pipeline;
    VkPipelineLayout m_Layout;
    TKit::Storage<Shader> m_VertexShader;
    TKit::Storage<Shader> m_FragmentShader;
};
} // namespace Onyx
