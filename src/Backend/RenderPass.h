#pragma once
#include "Context.h"

class RenderPass
{
public:
    VkRenderPass renderPass = nullptr;
    VulkanSwapChain vulkanSwapchain;
    
    RenderPass(Context &context, VulkanSwapChain &swapchain);
    ~RenderPass();

private:
    Context &context;
    
    void init();
};