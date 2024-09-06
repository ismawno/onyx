#include "core/pch.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/core/core.hpp"
#include "kit/core/logging.hpp"
#include "kit/memory/stack_allocator.hpp"
#include <fstream>

namespace ONYX
{
Pipeline::Pipeline(const Specs &p_Specs) noexcept
{
    initialize(p_Specs);
}

Pipeline::~Pipeline()
{
    vkDestroyShaderModule(m_Device->VulkanDevice(), m_VertexShader, nullptr);
    vkDestroyShaderModule(m_Device->VulkanDevice(), m_FragmentShader, nullptr);
    vkDestroyPipelineLayout(m_Device->VulkanDevice(), m_PipelineLayout, nullptr);
    vkDestroyPipeline(m_Device->VulkanDevice(), m_Pipeline, nullptr);
}

void Pipeline::Bind(VkCommandBuffer p_CommandBuffer) const noexcept
{
    vkCmdBindPipeline(p_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
}

void Pipeline::initialize(const Specs &p_Specs) noexcept
{
    KIT_LOG_INFO("Creating new pipeline...");
    KIT_ASSERT(p_Specs.RenderPass, "Render pass must be provided to create graphics pipeline");

    m_Device = Core::Device();
    m_VertexShader = createShaderModule(p_Specs.VertexShaderPath);
    m_FragmentShader = createShaderModule(p_Specs.FragmentShaderPath);

    createPipelineLayout();

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    for (auto &shaderStage : shaderStages)
    {
        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.pName = "main";
        shaderStage.flags = 0;
        shaderStage.pNext = nullptr;
        shaderStage.pSpecializationInfo = nullptr;
    }
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = m_VertexShader;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = m_FragmentShader;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<u32>(p_Specs.AttributeDescriptions.size());
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<u32>(p_Specs.BindingDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = p_Specs.AttributeDescriptions.data();
    vertexInputInfo.pVertexBindingDescriptions = p_Specs.BindingDescriptions.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &p_Specs.InputAssemblyInfo;
    pipelineInfo.pViewportState = &p_Specs.ViewportInfo;
    pipelineInfo.pRasterizationState = &p_Specs.RasterizationInfo;
    pipelineInfo.pMultisampleState = &p_Specs.MultisampleInfo;
    pipelineInfo.pColorBlendState = &p_Specs.ColorBlendInfo;
    pipelineInfo.pDepthStencilState = &p_Specs.DepthStencilInfo;
    pipelineInfo.pDynamicState = &p_Specs.DynamicStateInfo;

    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = p_Specs.RenderPass;
    pipelineInfo.subpass = p_Specs.Subpass;

    pipelineInfo.basePipelineIndex = -1;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    KIT_ASSERT_RETURNS(
        vkCreateGraphicsPipelines(m_Device->VulkanDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline),
        VK_SUCCESS, "Failed to create graphics pipeline");
}

void Pipeline::createPipelineLayout() noexcept
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    KIT_ASSERT_RETURNS(
        vkCreatePipelineLayout(m_Device->VulkanDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout), VK_SUCCESS,
        "Failed to create pipeline layout");
}

VkShaderModule Pipeline::createShaderModule(const char *p_Path) noexcept
{
    std::ifstream file{p_Path, std::ios::ate | std::ios::binary};
    KIT_ASSERT(file.is_open(), "File at path {} not found", p_Path);
    const auto fileSize = file.tellg();

    KIT::StackAllocator *allocator = Core::StackAllocator();

    char *code = static_cast<char *>(allocator->Push(fileSize * sizeof(char), alignof(u32)));
    file.seekg(0);
    file.read(code, fileSize);

    KIT_LOG_INFO("Creating shader module from file: {} with size: {}", p_Path, static_cast<usize>(fileSize));

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<const u32 *>(code);

    VkShaderModule shaderModule;
    KIT_ASSERT_RETURNS(vkCreateShaderModule(m_Device->VulkanDevice(), &createInfo, nullptr, &shaderModule), VK_SUCCESS,
                       "Failed to create shader module");

    allocator->Pop();
    return shaderModule;
}

void Pipeline::Specs::PopulateWithDefault(Specs &p_Specs) noexcept
{
    p_Specs.InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    p_Specs.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    p_Specs.InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    p_Specs.ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    p_Specs.ViewportInfo.viewportCount = 1;
    p_Specs.ViewportInfo.pViewports = nullptr;
    p_Specs.ViewportInfo.scissorCount = 1;
    p_Specs.ViewportInfo.pScissors = nullptr;

    p_Specs.RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    p_Specs.RasterizationInfo.depthClampEnable = VK_FALSE;
    p_Specs.RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    p_Specs.RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    p_Specs.RasterizationInfo.lineWidth = 1.0f;
    p_Specs.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    p_Specs.RasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    p_Specs.RasterizationInfo.depthBiasEnable = VK_FALSE;
    p_Specs.RasterizationInfo.depthBiasConstantFactor = 0.0f; // Optional
    p_Specs.RasterizationInfo.depthBiasClamp = 0.0f;          // Optional
    p_Specs.RasterizationInfo.depthBiasSlopeFactor = 0.0f;    // Optional

    p_Specs.MultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    p_Specs.MultisampleInfo.sampleShadingEnable = VK_FALSE;
    p_Specs.MultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    p_Specs.MultisampleInfo.minSampleShading = 1.0f;          // Optional
    p_Specs.MultisampleInfo.pSampleMask = nullptr;            // Optional
    p_Specs.MultisampleInfo.alphaToCoverageEnable = VK_FALSE; // Optional
    p_Specs.MultisampleInfo.alphaToOneEnable = VK_FALSE;      // Optional

    p_Specs.ColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    p_Specs.ColorBlendAttachment.blendEnable = VK_TRUE;
    p_Specs.ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;           // Optional
    p_Specs.ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
    p_Specs.ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;                            // Optional
    p_Specs.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;                 // Optional
    p_Specs.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
    p_Specs.ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;                            // Optional

    p_Specs.ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    p_Specs.ColorBlendInfo.logicOpEnable = VK_FALSE;
    p_Specs.ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
    p_Specs.ColorBlendInfo.attachmentCount = 1;
    p_Specs.ColorBlendInfo.pAttachments = &p_Specs.ColorBlendAttachment;
    p_Specs.ColorBlendInfo.blendConstants[0] = 0.0f; // Optional
    p_Specs.ColorBlendInfo.blendConstants[1] = 0.0f; // Optional
    p_Specs.ColorBlendInfo.blendConstants[2] = 0.0f; // Optional
    p_Specs.ColorBlendInfo.blendConstants[3] = 0.0f; // Optional

    p_Specs.DepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    p_Specs.DepthStencilInfo.depthTestEnable = VK_TRUE;
    p_Specs.DepthStencilInfo.depthWriteEnable = VK_TRUE;
    p_Specs.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    p_Specs.DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    p_Specs.DepthStencilInfo.minDepthBounds = 0.0f; // Optional
    p_Specs.DepthStencilInfo.maxDepthBounds = 1.0f; // Optional
    p_Specs.DepthStencilInfo.stencilTestEnable = VK_FALSE;
    p_Specs.DepthStencilInfo.front = {}; // Optional
    p_Specs.DepthStencilInfo.back = {};  // Optional

    p_Specs.DynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    p_Specs.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    p_Specs.DynamicStateInfo.pDynamicStates = p_Specs.DynamicStateEnables.data();
    p_Specs.DynamicStateInfo.dynamicStateCount = static_cast<u32>(p_Specs.DynamicStateEnables.size());
}
} // namespace ONYX