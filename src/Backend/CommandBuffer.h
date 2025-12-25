#pragma once
#include "Context.h"
#include "RenderPass.h"

class CommandBuffer
{
public:
    std::vector<VkCommandBuffer> commandBuffers;

    CommandBuffer(Context &context, const int framesInFlight);
    ~CommandBuffer();

    void createCommandPool();
    void recordCommandBuffer(VkCommandBuffer commandBuffer,
                             VkPipeline &pipeline,
                             VkRenderPass &renderPass,
                             VkFramebuffer &swapchainFramebuffer,
                             VkExtent2D swapchainExtent,
                             VkBuffer vertexBuffers[], VkDeviceSize offsets[], VkBuffer indexBuffer, VkDeviceSize ibSize,
                             VkDescriptorSet descSet, VkPipelineLayout pipelineLayout);
    VkCommandPool getCmdPool() const { return commandPool; }

private:
    VkCommandPool commandPool;

    Context &context; /*
     RenderPass &renderPass;
     VulkanSwapChain &swapchain; */
};