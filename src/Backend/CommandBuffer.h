#pragma once
#include "Context.h"
#include "RenderPass.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

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
                             VkClearColorValue clearColor,
                             VkBuffer vertexBuffers[], VkDeviceSize offsets[], VkBuffer indexBuffer, VkDeviceSize ibSize,
                             VkDescriptorSet descSet, VkPipelineLayout pipelineLayout,
                             ImDrawData *drawData);
    VkCommandPool getCmdPool() const { return commandPool; }

private:
    VkCommandPool commandPool;

    Context &context; /*
     RenderPass &renderPass;
     VulkanSwapChain &swapchain; */
};