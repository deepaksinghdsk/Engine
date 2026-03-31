#pragma once
#include "Context.h"
#include "string"

struct PipelineDesc
{
    std::string vertShaderLoc;
    std::string fragShaderLoc;

    VkVertexInputBindingDescription vertexBinding;
    std::vector<VkVertexInputAttributeDescription> vertexAttrib;
    
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT; //VK_CULL_MODE_NONE
    
    VkCompareOp depthCompare = VK_COMPARE_OP_LESS;
    bool depthTest = true;
    bool depthWrite = true;
    
    VkRenderPass renderPass;
    VkPipelineLayout layout;
};

class Pipeline
{
public:
    VkPipeline pipeline;

    Pipeline(const VkDevice &device);
    ~Pipeline();

    void build(const PipelineDesc desc);
    void initPipeline(const VkDescriptorSetLayout& descSetLayout, VkRenderPass *renderPass, VkExtent2D swapchainExtent);

    VkPipelineLayout getPipelineLayout() const {return pipelineLayout;}

private:
    const VkDevice &device;
    VkPipelineLayout pipelineLayout;

    std::vector<char> readfile(const char* fileloc);
    VkShaderModule createShaderModule(std::vector<char> code);
};