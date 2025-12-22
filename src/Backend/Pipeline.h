#pragma once
#include "Context.h"

class Pipeline
{
public:
    VkPipeline pipeline;

    Pipeline(VkDevice &device, VkRenderPass renderpass, VkExtent2D extent);
    ~Pipeline();

private:
    VkDevice &device;
    VkPipelineLayout pipelineLayout;

    void init(VkRenderPass *renderPass, VkExtent2D extent);
    std::vector<char> readfile(char* fileloc);
    VkShaderModule createShaderModule(std::vector<char> code);

};