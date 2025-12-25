#pragma once
#include "Context.h"

class Pipeline
{
public:
    VkPipeline pipeline;

    Pipeline(VkDevice &device);
    ~Pipeline();

    void initPipeline(const VkDescriptorSetLayout& descSetLayout, VkRenderPass *renderPass, VkExtent2D swapchainExtent);

    VkPipelineLayout getPipelineLayout() const {return pipelineLayout;}

private:
    VkDevice &device;
    VkPipelineLayout pipelineLayout;

    std::vector<char> readfile(char* fileloc);
    VkShaderModule createShaderModule(std::vector<char> code);

};