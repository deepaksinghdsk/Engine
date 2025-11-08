#include "Pipeline.h"
#include <iostream>
#include <fstream>
#include <vector>

Pipeline::Pipeline(VkDevice device) : device(device)
{
    init();
}

Pipeline::~Pipeline()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

void Pipeline::init()
{
    //Fragment and vertex shader loading
    auto vertexShadercode = readfile("././res/shaders/vert.spv");
    auto fragShadercode = readfile("././res/shaders/frag.spv");
    VkShaderModule vertShaderModule = createShaderModule(vertexShadercode);
    VkShaderModule fragShaderModule = createShaderModule(fragShadercode);
    VkPipelineShaderStageCreateInfo vertShaderCI{};
    vertShaderCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderCI.module = vertShaderModule;
    vertShaderCI.pName = "main";
    VkPipelineShaderStageCreateInfo fragShaderCI{};
    vertShaderCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    vertShaderCI.module = fragShaderModule;
    vertShaderCI.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStage[] = {vertShaderCI, fragShaderCI};

    //VertexInput
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    //InputAssembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    //DynamicStates which doesn't have to be baked in pipeline, 
    //ToDo: their data have to be provided at draw time, its little complex see tut
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicstate{};
    dynamicstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicstate.dynamicStateCount = dynamicStates.size();
    dynamicstate.pDynamicStates = dynamicStates.data();
    
    //Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    //used in shadow mapping (requires enabling a GPU feature)
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    //Multisampling: One of the ways to perform antialiasing
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    //ToDo: Depth and stencil testing


    //Color blending: combining the color of current frag shad with old in framebuffer
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    //Pipeline layout: to pass on Uniform buffer and push Constant data to shaders
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
    if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }



}



std::vector<char> Pipeline::readfile(char* fileloc)
{
    std::ifstream shaderData{fileloc, std::ios::ate | std::ios::binary};

    if(!shaderData.is_open())
        throw std::runtime_error("unable to open shader file");

    size_t file_size = (size_t)shaderData.tellg();
    std::vector<char> buffer(file_size);

    shaderData.seekg(0);
    shaderData.read(buffer.data(), file_size);
    
    shaderData.close();
    return buffer;
}

VkShaderModule Pipeline::createShaderModule(std::vector<char> code)
{
    VkShaderModuleCreateInfo shaderModuleCI{};
    shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    //shaderModuleCI.pNext = nullptr;
    //shaderModuleCI.flags;
    shaderModuleCI.codeSize = code.size();
    shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule module;
    if(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("unable to create shader Module");
    
    return module;
}