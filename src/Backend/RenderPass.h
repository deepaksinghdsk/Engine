#pragma once
#include "Context.h"

class VulkanSwapChain;

class RenderPass
{
public:
    RenderPass(Context &context, VulkanSwapChain &swapchain);
    ~RenderPass();
    
    VkRenderPass renderPass = nullptr;
    VulkanSwapChain *vulkanSwapchain;

    void createRenderPass(VkFormat depthFormat);
private:
    Context &context;
};