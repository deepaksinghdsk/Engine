#pragma once
#include "Context.h"
#include "RenderPass.h"
#include "VulkanSwapChain.h"

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
            VkBuffer vertexBuffers[], VkDeviceSize offsets[], VkDeviceSize size);

    private:
        VkCommandPool commandPool;

        Context &context;/* 
        RenderPass &renderPass; 
        VulkanSwapChain &swapchain; */
};