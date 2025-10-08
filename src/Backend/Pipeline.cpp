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

}

void Pipeline::init()
{
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