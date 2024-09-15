#include "core/pch.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/core/core.hpp"
#include "kit/core/logging.hpp"
#include "kit/memory/stack_allocator.hpp"
#include <fstream>

namespace ONYX
{
Pipeline::Pipeline(Specs p_Specs) noexcept
{
    p_Specs.Populate();
    createPipeline(p_Specs);
}

Pipeline::~Pipeline()
{
    vkDestroyShaderModule(m_Device->GetDevice(), m_VertexShader, nullptr);
    vkDestroyShaderModule(m_Device->GetDevice(), m_FragmentShader, nullptr);
    vkDestroyPipelineLayout(m_Device->GetDevice(), m_PipelineLayout, nullptr);
    vkDestroyPipeline(m_Device->GetDevice(), m_Pipeline, nullptr);
}

void Pipeline::Bind(VkCommandBuffer p_CommandBuffer) const noexcept
{
    vkCmdBindPipeline(p_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
}

VkPipelineLayout Pipeline::GetLayout() const noexcept
{
    return m_PipelineLayout;
}

void Pipeline::createPipeline(const Specs &p_Specs) noexcept
{
    // KIT_LOG_INFO("Creating new pipeline...");
    KIT_ASSERT(p_Specs.RenderPass, "Render pass must be provided to create graphics pipeline");

    m_Device = Core::GetDevice();
    m_VertexShader = createShaderModule(p_Specs.VertexShaderPath);
    m_FragmentShader = createShaderModule(p_Specs.FragmentShaderPath);

    KIT_ASSERT_RETURNS(
        vkCreatePipelineLayout(m_Device->GetDevice(), &p_Specs.PipelineLayoutInfo, nullptr, &m_PipelineLayout),
        VK_SUCCESS, "Failed to create pipeline layout");

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
        vkCreateGraphicsPipelines(m_Device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline),
        VK_SUCCESS, "Failed to create graphics pipeline");
}

VkShaderModule Pipeline::createShaderModule(const char *p_Path) noexcept
{
    std::ifstream file{p_Path, std::ios::ate | std::ios::binary};
    KIT_ASSERT(file.is_open(), "File at path {} not found", p_Path);
    const auto fileSize = file.tellg();

    KIT::StackAllocator *allocator = Core::GetStackAllocator();

    char *code = static_cast<char *>(allocator->Push(fileSize * sizeof(char), alignof(u32)));
    file.seekg(0);
    file.read(code, fileSize);

    // KIT_LOG_INFO("Creating shader module from file: {} with size: {}", p_Path, static_cast<usize>(fileSize));

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<const u32 *>(code);

    VkShaderModule shaderModule;
    KIT_ASSERT_RETURNS(vkCreateShaderModule(m_Device->GetDevice(), &createInfo, nullptr, &shaderModule), VK_SUCCESS,
                       "Failed to create shader module");

    allocator->Pop();
    return shaderModule;
}

Pipeline::Specs::Specs() noexcept
{
    InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportInfo.viewportCount = 1;
    ViewportInfo.pViewports = nullptr;
    ViewportInfo.scissorCount = 1;
    ViewportInfo.pScissors = nullptr;

    RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizationInfo.depthClampEnable = VK_FALSE;
    RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    RasterizationInfo.lineWidth = 1.0f;
    RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    RasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    RasterizationInfo.depthBiasEnable = VK_FALSE;
    RasterizationInfo.depthBiasConstantFactor = 0.0f; // Optional
    RasterizationInfo.depthBiasClamp = 0.0f;          // Optional
    RasterizationInfo.depthBiasSlopeFactor = 0.0f;    // Optional

    MultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisampleInfo.sampleShadingEnable = VK_FALSE;
    MultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    MultisampleInfo.minSampleShading = 1.0f;          // Optional
    MultisampleInfo.pSampleMask = nullptr;            // Optional
    MultisampleInfo.alphaToCoverageEnable = VK_FALSE; // Optional
    MultisampleInfo.alphaToOneEnable = VK_FALSE;      // Optional

    ColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable = VK_TRUE;
    ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;           // Optional
    ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
    ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;                            // Optional
    ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;                 // Optional
    ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
    ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;                            // Optional

    ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendInfo.logicOpEnable = VK_FALSE;
    ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
    ColorBlendInfo.attachmentCount = 1;
    ColorBlendInfo.blendConstants[0] = 0.0f; // Optional
    ColorBlendInfo.blendConstants[1] = 0.0f; // Optional
    ColorBlendInfo.blendConstants[2] = 0.0f; // Optional
    ColorBlendInfo.blendConstants[3] = 0.0f; // Optional

    DepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthStencilInfo.depthTestEnable = VK_TRUE;
    DepthStencilInfo.depthWriteEnable = VK_TRUE;
    DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    DepthStencilInfo.minDepthBounds = 0.0f; // Optional
    DepthStencilInfo.maxDepthBounds = 1.0f; // Optional
    DepthStencilInfo.stencilTestEnable = VK_FALSE;
    DepthStencilInfo.front = {}; // Optional
    DepthStencilInfo.back = {};  // Optional

    PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    PushConstantRange.offset = 0;
    PushConstantRange.size = 0;

    PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutInfo.setLayoutCount = 0;
    PipelineLayoutInfo.pSetLayouts = nullptr;
    PipelineLayoutInfo.pushConstantRangeCount = 1;

    DynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
}

void Pipeline::Specs::Populate() noexcept
{
    ColorBlendInfo.pAttachments = &ColorBlendAttachment;
    DynamicStateInfo.pDynamicStates = DynamicStateEnables.data();
    DynamicStateInfo.dynamicStateCount = static_cast<u32>(DynamicStateEnables.size());
    PipelineLayoutInfo.pPushConstantRanges = &PushConstantRange;
}
} // namespace ONYX